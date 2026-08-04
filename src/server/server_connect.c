#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_spiffs.h"
#include "esp_ota_ops.h"

#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/chachapoly.h"
#include "mbedtls/sha256.h"

#include "server_connect.h"

#define TAG   "srv"
#define HOST  "la5.intellicar.in"
#define PORT  10131

/* ---- Protocol constants ---- */
#define BEACON_SECURE    102   /* outer encrypted transport */
#define BEACON_UPLINK    100   /* device → server           */
#define BEACON_DOWNLINK  101   /* server → device           */

#define MOD_PUBKEY       1
#define MOD_CHACHA       2
#define MOD_CHUNK_OUT    3    /* Beacon-101: server sends chunk data   */
#define MOD_CHUNK_INIT   4    /* Beacon-101: server notifies of file   */
#define MOD_ACK          6
#define MOD_CHUNK_REQ    9    /* Beacon-100: device requests chunks    */
#define MOD_CHUNK_IREPLY 10   /* Beacon-100: device acks file init     */
#define MOD_BEACON_ACK   999  /* Beacon-100: device ACKs server beacon */

#define CHUNK_SIZE       256
#define CHUNKS_AT_ONCE   4

#define STATUS_STARTED   0
#define STATUS_SUCCESS   2
#define STATUS_FAILURE   3
#define STATUS_INPROG    4

/* Server static public key (from lafm_protocol.py) — little-endian X25519 */
static const uint8_t SERVER_PUB_STATIC[32] = {
    0x5C,0x40,0x5A,0xCF, 0x27,0x65,0x28,0xF6,
    0x33,0x06,0x33,0x68, 0x43,0xD2,0x20,0x6B,
    0x9A,0xCC,0xE9,0xC0, 0xB9,0x61,0xF8,0x17,
    0x6E,0xDA,0xFF,0x5B, 0x2A,0xBA,0xE0,0x34
};

/* ===========================================================================
 * Session & download state
 * ========================================================================= */
typedef struct {
    int     sock;
    uint8_t priv[32];         /* our X25519 private key (big-endian MPI)  */
    uint8_t shared[32];       /* DH shared secret                          */
    uint16_t seq;             /* outgoing sequence counter                  */
    uint16_t last_rx_seq;     /* seq_id of last received outer Beacon-102   */
    char    dev_id[17];       /* "0000XXXXXXXXXXXX" device ID               */
    uint8_t rxbuf[4096];
    int     rxlen;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context drbg;
} la5_session_t;

typedef struct {
    uint8_t  file_id[32];
    uint32_t file_size;
    uint8_t  file_hash[32];
    uint32_t total_chunks;
    uint32_t next_chunk;
} la5_dl_t;

/* ===========================================================================
 * Helpers
 * ========================================================================= */
static uint16_t crc16(const uint8_t *d, size_t n)
{
    uint16_t c = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        c ^= (uint16_t)d[i] << 8;
        for (int b = 0; b < 8; b++)
            c = (c & 0x8000) ? ((c << 1) ^ 0x1021) : (c << 1);
    }
    return c;
}

static void bytes_to_hex(const uint8_t *in, size_t len, char *out)
{
    static const char h[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++) {
        out[2*i]   = h[in[i] >> 4];
        out[2*i+1] = h[in[i] & 0xF];
    }
    out[2*len] = '\0';
}

/* ===========================================================================
 * Crypto — X25519 + ChaCha20-Poly1305
 * ========================================================================= */

/* Generate X25519 keypair. priv_out = big-endian MPI, pub_out = LE X-coord. */
static int x25519_gen_pair(uint8_t priv_out[32], uint8_t pub_out[32],
                            mbedtls_ctr_drbg_context *drbg)
{
    mbedtls_ecp_group grp;
    mbedtls_mpi       priv;
    mbedtls_ecp_point pub;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&priv);
    mbedtls_ecp_point_init(&pub);

    int ret = -1;
    size_t olen = 0;
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519)) goto done;
    if (mbedtls_ecp_gen_keypair(&grp, &priv, &pub, mbedtls_ctr_drbg_random, drbg)) goto done;
    if (mbedtls_mpi_write_binary(&priv, priv_out, 32)) goto done;
    if (mbedtls_ecp_point_write_binary(&grp, &pub, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                       &olen, pub_out, 32) || olen != 32) goto done;
    ret = 0;
done:
    mbedtls_ecp_point_free(&pub);
    mbedtls_mpi_free(&priv);
    mbedtls_ecp_group_free(&grp);
    return ret;
}

/* Compute shared secret = our_priv × their_pub (both Curve25519). */
static int x25519_dh(const uint8_t our_priv[32], const uint8_t their_pub[32],
                      uint8_t shared_out[32], mbedtls_ctr_drbg_context *drbg)
{
    mbedtls_ecp_group grp;
    mbedtls_mpi       priv;
    mbedtls_ecp_point their, shared;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&priv);
    mbedtls_ecp_point_init(&their);
    mbedtls_ecp_point_init(&shared);

    int ret = -1;
    size_t olen = 0;
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519)) goto done;
    if (mbedtls_mpi_read_binary(&priv, our_priv, 32)) goto done;
    /* For Montgomery curves, read_binary takes the raw 32-byte X coordinate (LE) */
    if (mbedtls_ecp_point_read_binary(&grp, &their, their_pub, 32)) goto done;
    if (mbedtls_ecp_mul(&grp, &shared, &priv, &their,
                        mbedtls_ctr_drbg_random, drbg)) goto done;
    if (mbedtls_ecp_point_write_binary(&grp, &shared, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                       &olen, shared_out, 32) || olen != 32) goto done;
    ret = 0;
done:
    mbedtls_ecp_point_free(&shared);
    mbedtls_ecp_point_free(&their);
    mbedtls_mpi_free(&priv);
    mbedtls_ecp_group_free(&grp);
    return ret;
}

/*
 * Module-2 body layout:
 *   aad_len(1B) + aad(4B="LAFM") + ciphertext(plen) + mac(16B) + nonce(12B)
 */
static int mod2_decrypt(const uint8_t *body, size_t blen, const uint8_t key[32],
                         uint8_t *out, size_t *out_len)
{
    if (blen < 33) return -1;                    /* 1+4+0+16+12 minimum */
    uint8_t aad_len = body[0];
    if (blen < (size_t)(1 + aad_len + 16 + 12)) return -1;
    const uint8_t *aad    = body + 1;
    size_t   cipher_len   = blen - 1 - aad_len - 16 - 12;
    const uint8_t *cipher = body + 1 + aad_len;
    const uint8_t *mac    = cipher + cipher_len;
    const uint8_t *nonce  = mac + 16;

    mbedtls_chachapoly_context ctx;
    mbedtls_chachapoly_init(&ctx);
    mbedtls_chachapoly_setkey(&ctx, key);
    int r = mbedtls_chachapoly_auth_decrypt(&ctx, cipher_len, nonce,
                                             aad, aad_len, mac, cipher, out);
    mbedtls_chachapoly_free(&ctx);
    if (r == 0) *out_len = cipher_len;
    return r;
}

static int mod2_encrypt(const uint8_t *plain, size_t plen, const uint8_t key[32],
                         mbedtls_ctr_drbg_context *drbg, uint8_t *out, size_t *out_len)
{
    static const uint8_t aad[] = "LAFM";
    uint8_t nonce[12];
    mbedtls_ctr_drbg_random(drbg, nonce, 12);

    uint8_t *p = out;
    *p++ = 4;                              /* aad_len = 4        */
    memcpy(p, aad, 4); p += 4;            /* aad = "LAFM"       */
    uint8_t *cipher_dst = p; p += plen;   /* ciphertext slot    */
    uint8_t *mac_dst    = p; p += 16;     /* mac slot           */
    memcpy(p, nonce, 12); p += 12;        /* nonce              */

    mbedtls_chachapoly_context ctx;
    mbedtls_chachapoly_init(&ctx);
    mbedtls_chachapoly_setkey(&ctx, key);
    int r = mbedtls_chachapoly_encrypt_and_tag(&ctx, plen, nonce, aad, 4,
                                                plain, cipher_dst, mac_dst);
    mbedtls_chachapoly_free(&ctx);
    if (r == 0) *out_len = 1 + 4 + plen + 16 + 12;
    return r;
}

/* ===========================================================================
 * Frame building
 * ========================================================================= */

/*
 * Build a full LA5 frame into 'out'. Returns total bytes written.
 * Layout: LA5(3) + bt(2LE) + seq(2LE) + plen(2LE) + nmod(1) + modules(mods_len) + CRC(2LE)
 */
static size_t build_la5_frame(uint16_t bt, uint16_t seq,
                               uint8_t nmod, const uint8_t *mods, size_t mods_len,
                               uint8_t *out)
{
    uint16_t plen = 1 + (uint16_t)mods_len;
    uint8_t *p = out;
    p[0]='L'; p[1]='A'; p[2]='5'; p += 3;
    p[0] = bt & 0xFF;   p[1] = bt >> 8;   p += 2;
    p[0] = seq & 0xFF;  p[1] = seq >> 8;  p += 2;
    p[0] = plen & 0xFF; p[1] = plen >> 8; p += 2;
    *p++ = nmod;
    memcpy(p, mods, mods_len); p += mods_len;
    uint16_t crc = crc16(out, p - out);
    p[0] = crc & 0xFF; p[1] = crc >> 8; p += 2;
    return p - out;
}

/*
 * Build a module header (type+len) + body. Returns total bytes written.
 */
static size_t build_module(uint16_t type, const uint8_t *body, uint16_t blen, uint8_t *out)
{
    out[0] = type & 0xFF; out[1] = type >> 8;
    out[2] = blen & 0xFF; out[3] = blen >> 8;
    memcpy(out + 4, body, blen);
    return 4 + blen;
}

/*
 * Build and send one Beacon-102 frame with a single Module-2 enclosing an
 * inner Beacon-100 frame that contains the given module bytes.
 */
static int send_secure(la5_session_t *s, uint16_t inner_bt,
                        const uint8_t *mods, size_t mods_len)
{
    /* --- Inner LA5 frame (Beacon-100 or 101 depending on inner_bt) --- */
    uint8_t inner[128];
    size_t inner_len = build_la5_frame(inner_bt, 0, 1, mods, mods_len, inner);
    if (inner_len > sizeof(inner)) return -1;

    /* --- Module-2 body: encrypt inner frame --- */
    uint8_t mod2_body[1 + 4 + sizeof(inner) + 16 + 12];
    size_t  mod2_len = 0;
    if (mod2_encrypt(inner, inner_len, s->shared, &s->drbg, mod2_body, &mod2_len))
        return -1;

    /* --- Module-2 wire bytes (header + body) --- */
    uint8_t mod2_wire[4 + sizeof(mod2_body)];
    size_t  mod2_wire_len = build_module(MOD_CHACHA, mod2_body, (uint16_t)mod2_len, mod2_wire);

    /* --- Outer Beacon-102 frame --- */
    uint8_t outer[256];
    size_t  outer_len = build_la5_frame(BEACON_SECURE, s->seq++, 1,
                                        mod2_wire, mod2_wire_len, outer);
    if (outer_len > sizeof(outer)) return -1;

    return (send(s->sock, outer, outer_len, 0) == (ssize_t)outer_len) ? 0 : -1;
}

/* ===========================================================================
 * Receive framing
 * ========================================================================= */

/*
 * Scan rxbuf for a valid LA5 frame at position 0.
 * Discards leading garbage, returns frame length or 0 if incomplete, -1 on error.
 */
static int next_frame(la5_session_t *s)
{
    uint8_t *buf = s->rxbuf;
    int len = s->rxlen;

    for (int i = 0; i + 9 <= len; ) {
        if (buf[i] != 'L' || buf[i+1] != 'A' || buf[i+2] != '5') { i++; continue; }
        uint16_t plen  = buf[i+7] | ((uint16_t)buf[i+8] << 8);
        int      total = 9 + plen + 2;
        if (i + total > len) {
            /* Frame starts at i but incomplete — discard anything before it */
            if (i > 0) { memmove(buf, buf + i, len - i); s->rxlen -= i; }
            return 0;
        }
        uint16_t crc_rx   = buf[i + 9 + plen] | ((uint16_t)buf[i + 9 + plen + 1] << 8);
        uint16_t crc_calc = crc16(buf + i, 9 + plen);
        if (crc_rx != crc_calc) { i++; continue; }
        /* Valid frame at offset i — slide it to front */
        if (i > 0) { memmove(buf, buf + i, len - i); s->rxlen -= i; }
        return total;
    }
    /* No valid start found — discard everything except last 2 bytes (partial magic) */
    if (len > 2) { memmove(buf, buf + len - 2, 2); s->rxlen = 2; }
    return 0;
}

/*
 * Block-receive until a complete valid LA5 frame is available or timeout.
 * Returns frame length (> 0), 0 on timeout, -1 on socket error.
 */
static int recv_frame(la5_session_t *s, int timeout_ms)
{
    struct timeval tv = { .tv_sec = timeout_ms / 1000,
                          .tv_usec = (timeout_ms % 1000) * 1000 };
    setsockopt(s->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    for (;;) {
        int flen = next_frame(s);
        if (flen > 0) return flen;

        if (s->rxlen >= (int)sizeof(s->rxbuf)) s->rxlen = 0;

        int n = recv(s->sock, s->rxbuf + s->rxlen, sizeof(s->rxbuf) - s->rxlen, 0);
        if (n > 0) { s->rxlen += n; continue; }
        if (n == 0) return -1;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
}

/* Consume the front frame from rxbuf after processing. */
static void consume_frame(la5_session_t *s, int flen)
{
    if (flen > 0 && flen <= s->rxlen) {
        memmove(s->rxbuf, s->rxbuf + flen, s->rxlen - flen);
        s->rxlen -= flen;
    }
}

/* ===========================================================================
 * Login
 * ========================================================================= */

static void build_login_frame(uint8_t buf[65], const char dev_id[17], const uint8_t pub[32])
{
    uint8_t body[49];
    body[0] = 16;
    memcpy(body + 1, dev_id, 16);
    memcpy(body + 17, pub, 32);

    buf[0]='L'; buf[1]='A'; buf[2]='5';
    buf[3] = BEACON_SECURE; buf[4] = 0;
    buf[5] = 0xFF; buf[6] = 0xFF;
    buf[7] = 54; buf[8] = 0;
    buf[9] = 1;
    buf[10] = MOD_PUBKEY; buf[11] = 0;
    buf[12] = 49; buf[13] = 0;
    memcpy(buf + 14, body, 49);
    uint16_t crc = crc16(buf, 63);
    buf[63] = crc & 0xFF; buf[64] = crc >> 8;
}

/*
 * Connect, login, receive server live pubkey, update shared secret.
 * Returns 0 on success.
 */
static int do_login(la5_session_t *s)
{
    uint8_t pub[32];
    if (x25519_gen_pair(s->priv, pub, &s->drbg)) {
        ESP_LOGE(TAG, "X25519 keygen failed"); return -1;
    }
    /* Initial shared secret uses the static server public key */
    if (x25519_dh(s->priv, SERVER_PUB_STATIC, s->shared, &s->drbg)) {
        ESP_LOGE(TAG, "Initial DH failed"); return -1;
    }

    uint8_t frame[65];
    build_login_frame(frame, s->dev_id, pub);
    if (send(s->sock, frame, 65, 0) != 65) {
        ESP_LOGE(TAG, "Login send failed"); return -1;
    }
    ESP_LOGI(TAG, "Login sent — waiting for server response");

    bool got_ack = false;
    for (int attempt = 0; attempt < 20 && !got_ack; attempt++) {
        int flen = recv_frame(s, 2000);
        if (flen <= 0) continue;

        /* Walk outer modules (login response is plaintext Beacon-102) */
        uint16_t plen    = s->rxbuf[7] | ((uint16_t)s->rxbuf[8] << 8);
        uint8_t  nmod    = s->rxbuf[9];
        int      pos = 10, end = 9 + (int)plen;

        for (int m = 0; m < nmod && pos + 4 <= end; m++) {
            uint16_t mc = s->rxbuf[pos] | ((uint16_t)s->rxbuf[pos+1] << 8);
            uint16_t ml = s->rxbuf[pos+2] | ((uint16_t)s->rxbuf[pos+3] << 8);
            pos += 4;
            const uint8_t *mb = s->rxbuf + pos;

            if (mc == MOD_PUBKEY && ml >= 49) {
                /* Server live public key at mb[17..49] — update shared secret */
                if (x25519_dh(s->priv, mb + 17, s->shared, &s->drbg) == 0)
                    ESP_LOGI(TAG, "Shared secret updated from server live pubkey");
                else
                    ESP_LOGW(TAG, "DH update failed");
            }
            if (mc == MOD_ACK && ml >= 1) {
                uint8_t nacks = mb[0];
                for (int k = 0; k < nacks && (size_t)(1 + 2*(k+1)) <= ml; k++) {
                    uint16_t seq = mb[1 + 2*k] | ((uint16_t)mb[2 + 2*k] << 8);
                    if (seq == 0xFFFF) { got_ack = true; break; }
                }
            }
            pos += ml;
        }
        consume_frame(s, flen);
    }

    if (!got_ack) { ESP_LOGE(TAG, "Login ACK not received"); return -1; }
    ESP_LOGI(TAG, "Login ACK — session established");
    return 0;
}

/* ===========================================================================
 * SPIFFS helpers
 * ========================================================================= */
static bool s_spiffs_ready = false;

static void init_spiffs(void)
{
    if (s_spiffs_ready) return;
    esp_vfs_spiffs_conf_t conf = {
        .base_path            = "/spiffs",
        .partition_label      = NULL,
        .max_files            = 5,
        .format_if_mount_failed = true
    };
    esp_err_t e = esp_vfs_spiffs_register(&conf);
    s_spiffs_ready = (e == ESP_OK || e == ESP_ERR_INVALID_STATE);
    if (!s_spiffs_ready) ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(e));
}

static void file_id_to_path(const uint8_t fid[32], char path_out[64])
{
    char hex[17];
    bytes_to_hex(fid, 8, hex);          /* first 8 bytes → 16 hex chars */
    snprintf(path_out, 64, "/spiffs/%.16s.bin", hex);
}

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

/* ===========================================================================
 * Download: build outgoing modules
 * ========================================================================= */

/* Module-10 (CHUNK_TXFR_INIT_REPLY): File_ID(32) + File_Hash(32) + Meta_Size(1) + Status(1) */
static size_t build_mod10(const uint8_t fid[32], const uint8_t fhash[32],
                            uint8_t status, uint8_t *out)
{
    uint8_t body[66];
    memcpy(body, fid, 32);
    memcpy(body + 32, fhash, 32);
    body[64] = 0;       /* Meta_Size = 0 */
    body[65] = status;
    return build_module(MOD_CHUNK_IREPLY, body, 66, out);
}

/* Module-9 (CHUNK_TXFR_INREQ): File_ID(32) + Chunk_Size(2) + nChunks(1) +
 *   Chunk_Ids(2×n) + Meta_Size(1) + Status(1).
 *   Requests n_chunks chunks starting at first_id. */
static size_t build_mod9(const uint8_t fid[32], uint16_t first_id,
                          uint8_t n_chunks, uint8_t status, uint8_t *out)
{
    uint8_t body[32 + 2 + 1 + 2 * CHUNKS_AT_ONCE + 1 + 1];
    uint8_t *p = body;
    memcpy(p, fid, 32); p += 32;
    p[0] = CHUNK_SIZE & 0xFF; p[1] = CHUNK_SIZE >> 8; p += 2;
    *p++ = n_chunks;
    for (uint8_t i = 0; i < n_chunks; i++) {
        uint16_t id = first_id + i;
        p[0] = id & 0xFF; p[1] = id >> 8; p += 2;
    }
    *p++ = 0;       /* Meta_Size = 0 */
    *p++ = status;
    return build_module(MOD_CHUNK_REQ, body, (uint16_t)(p - body), out);
}

/* Send Beacon-100 Module-999: ACK one received server beacon sequence ID. */
static int send_ack(la5_session_t *s, uint16_t ack_seq)
{
    /* wire format: type(2LE) + len(2LE) + nacks(1) + seq(2LE) = 7 bytes */
    uint8_t mod[7];
    mod[0] = MOD_BEACON_ACK & 0xFF; mod[1] = (MOD_BEACON_ACK >> 8) & 0xFF;
    mod[2] = 3; mod[3] = 0;
    mod[4] = 1;
    mod[5] = ack_seq & 0xFF;
    mod[6] = ack_seq >> 8;
    return send_secure(s, BEACON_UPLINK, mod, 7);
}

/* ===========================================================================
 * Download: receive and parse one secure (Beacon-102 + Module-2) frame.
 * Fills *pf with the inner module that was found. Returns false on no data.
 * ========================================================================= */
typedef enum { INNER_NONE, INNER_CHUNK_INIT, INNER_CHUNK_OUT, INNER_ACK } inner_type_t;

typedef struct {
    inner_type_t type;
    union {
        struct {                    /* INNER_CHUNK_INIT (Module-4) */
            uint8_t  file_id[32];
            uint32_t file_size;
            uint16_t file_type;
            uint8_t  file_hash[32];
        } init;
        struct {                    /* INNER_CHUNK_OUT (Module-3) */
            uint8_t  file_id[32];
            uint16_t chunk_size;
            uint8_t  n_chunks;
            uint8_t  data[CHUNKS_AT_ONCE * CHUNK_SIZE];  /* copied chunk bytes */
        } chunks;
    };
} inner_frame_t;

/* Reusable decrypt scratch buffer — kept off the stack due to size */
static uint8_t s_decrypt_buf[2048];

/* Returns: 1 = got frame, 0 = timeout, -1 = socket error / closed */
static int recv_inner(la5_session_t *s, inner_frame_t *pf, int timeout_ms)
{
    pf->type = INNER_NONE;
    int flen = recv_frame(s, timeout_ms);
    if (flen == 0) return 0;
    if (flen  < 0) return -1;

    uint16_t outer_bt = s->rxbuf[3] | ((uint16_t)s->rxbuf[4] << 8);
    uint16_t plen     = s->rxbuf[7] | ((uint16_t)s->rxbuf[8] << 8);
    uint8_t  nmod     = s->rxbuf[9];
    int      pos = 10, end = 9 + (int)plen;

    /* Save the outer sequence ID — caller uses it to ACK this beacon */
    s->last_rx_seq = s->rxbuf[5] | ((uint16_t)s->rxbuf[6] << 8);

    if (outer_bt == BEACON_SECURE) {
        for (int m = 0; m < nmod && pos + 4 <= end; m++) {
            uint16_t mc = s->rxbuf[pos] | ((uint16_t)s->rxbuf[pos+1] << 8);
            uint16_t ml = s->rxbuf[pos+2] | ((uint16_t)s->rxbuf[pos+3] << 8);
            pos += 4;

            if (mc == MOD_CHACHA) {
                size_t plain_len = 0;
                if (mod2_decrypt(s->rxbuf + pos, ml, s->shared,
                                  s_decrypt_buf, &plain_len)) {
                    ESP_LOGW(TAG, "ChaCha decrypt failed");
                    break;
                }
                /* Validate inner LA5 frame */
                if (plain_len < 11) break;
                if (s_decrypt_buf[0]!='L' || s_decrypt_buf[1]!='A' || s_decrypt_buf[2]!='5') break;
                uint16_t iplen   = s_decrypt_buf[7] | ((uint16_t)s_decrypt_buf[8] << 8);
                size_t   itotal  = 9 + iplen + 2;
                if (itotal > plain_len) break;
                uint16_t icrc_rx   = s_decrypt_buf[9+iplen] | ((uint16_t)s_decrypt_buf[9+iplen+1] << 8);
                uint16_t icrc_calc = crc16(s_decrypt_buf, 9 + iplen);
                if (icrc_rx != icrc_calc) { ESP_LOGW(TAG, "Inner CRC fail"); break; }

                /* Walk inner modules */
                uint8_t inmod  = s_decrypt_buf[9];
                int     ipos   = 10, iend = 9 + (int)iplen;
                for (int k = 0; k < inmod && ipos + 4 <= iend; k++) {
                    uint16_t imc = s_decrypt_buf[ipos]   | ((uint16_t)s_decrypt_buf[ipos+1] << 8);
                    uint16_t iml = s_decrypt_buf[ipos+2] | ((uint16_t)s_decrypt_buf[ipos+3] << 8);
                    ipos += 4;
                    uint8_t *ib = s_decrypt_buf + ipos;

                    if (imc == MOD_CHUNK_INIT && iml >= 136) {
                        /* File_ID(32) + File_Size(4) + File_Type(2) + Chunk_Size(2) + File_Hash(32) + Sig(64) */
                        pf->type = INNER_CHUNK_INIT;
                        memcpy(pf->init.file_id, ib, 32);
                        pf->init.file_size  = (uint32_t)ib[32] | ((uint32_t)ib[33]<<8)
                                            | ((uint32_t)ib[34]<<16) | ((uint32_t)ib[35]<<24);
                        pf->init.file_type  = ib[36] | ((uint16_t)ib[37]<<8);
                        memcpy(pf->init.file_hash, ib + 40, 32);
                    }
                    if (imc == MOD_CHUNK_OUT && iml >= 35) {
                        /* File_ID(32) + Chunk_Size(2) + nChnks(1)
                         * + [ChunkID(2) + Data(cs)] × n  ← interleaved per chunk
                         * + Status(1) */
                        uint8_t  nc = ib[34];
                        uint16_t cs = ib[32] | ((uint16_t)ib[33] << 8);
                        size_t   rec  = 2 + (size_t)cs;      /* per-chunk record size */
                        size_t   data_bytes = (size_t)nc * cs;
                        if ((size_t)iml >= 36 + (size_t)nc * rec &&
                            data_bytes <= sizeof(pf->chunks.data)) {
                            pf->type = INNER_CHUNK_OUT;
                            memcpy(pf->chunks.file_id, ib, 32);
                            pf->chunks.chunk_size = cs;
                            pf->chunks.n_chunks   = nc;
                            /* Extract data from each [ChunkID(2)+Data(cs)] record */
                            for (uint8_t i = 0; i < nc; i++)
                                memcpy(pf->chunks.data + i * cs,
                                       ib + 35 + i * rec + 2, cs);
                        } else {
                            ESP_LOGW(TAG, "CHUNK_OUT bad length (iml=%u nc=%u cs=%u)",
                                     iml, nc, cs);
                        }
                    }
                    ipos += iml;
                }
                break; /* one Module-2 per outer frame */
            }
            if (mc == MOD_ACK) pf->type = INNER_ACK;
            pos += ml;
        }
    }

    consume_frame(s, flen);
    return 1;
}

/* ===========================================================================
 * Download state machine — runs forever, returns only when the socket dies
 * ========================================================================= */
static void run_download(la5_session_t *s)
{
    ESP_LOGI(TAG, "Session active — waiting for file notifications");

    while (1) {
        /* ---- Phase 1: Wait for CHUNK_TRANSFER_INIT ---- */
        inner_frame_t pf;
        int got_init = 0;
        while (!got_init) {
            int r = recv_inner(s, &pf, 30000);
            if (r < 0) { ESP_LOGW(TAG, "Connection lost"); return; }
            if (r == 1) send_ack(s, s->last_rx_seq);
            if (pf.type == INNER_CHUNK_INIT) got_init = 1;
        }

        la5_dl_t dl = {0};
        memcpy(dl.file_id,   pf.init.file_id,   32);
        memcpy(dl.file_hash, pf.init.file_hash, 32);
        dl.file_size    = pf.init.file_size;
        dl.total_chunks = (dl.file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;

        char fid_hex[65];  bytes_to_hex(dl.file_id,   32, fid_hex);
        char fhash_hex[65]; bytes_to_hex(dl.file_hash, 32, fhash_hex);
        ESP_LOGI(TAG, "File init: size=%lu chunks=%lu type=%u id=%.16s",
                 (unsigned long)dl.file_size, (unsigned long)dl.total_chunks,
                 pf.init.file_type, fid_hex);
        ESP_LOGI(TAG, "Expected hash: %s", fhash_hex);

        /* ---- Phase 2: Open OTA write handle into the inactive partition ---- */
        const esp_partition_t *ota_part = esp_ota_get_next_update_partition(NULL);
        if (!ota_part) {
            ESP_LOGE(TAG, "No OTA partition available");
            uint8_t mod[4 + 66];
            size_t mlen = build_mod10(dl.file_id, dl.file_hash, STATUS_FAILURE, mod);
            send_secure(s, BEACON_UPLINK, mod, mlen);
            continue;
        }
        ESP_LOGI(TAG, "OTA target: %s @ 0x%08lx (%lu B)",
                 ota_part->label,
                 (unsigned long)ota_part->address,
                 (unsigned long)ota_part->size);

        esp_ota_handle_t ota_handle = 0;
        esp_err_t err = esp_ota_begin(ota_part, dl.file_size, &ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
            uint8_t mod[4 + 66];
            size_t mlen = build_mod10(dl.file_id, dl.file_hash, STATUS_FAILURE, mod);
            send_secure(s, BEACON_UPLINK, mod, mlen);
            continue;
        }

        /* SHA-256 computed incrementally alongside the OTA writes */
        mbedtls_sha256_context sha;
        mbedtls_sha256_init(&sha);
        mbedtls_sha256_starts(&sha, 0);

        /* ---- Phase 3: Send INIT_REPLY STARTED ---- */
        {
            uint8_t mod[4 + 66];
            size_t mlen = build_mod10(dl.file_id, dl.file_hash, STATUS_STARTED, mod);
            if (send_secure(s, BEACON_UPLINK, mod, mlen)) {
                ESP_LOGW(TAG, "Failed to send INIT_REPLY — socket dead");
                esp_ota_abort(ota_handle);
                mbedtls_sha256_free(&sha);
                return;
            }
            ESP_LOGI(TAG, "INIT_REPLY STARTED sent");
        }

        /* ---- Phase 4: Request chunks, write directly to OTA partition ---- */
        dl.next_chunk = 0;
        int consecutive_failures = 0;
        int socket_dead = 0;
        int ota_error   = 0;

        while (dl.next_chunk < dl.total_chunks && !socket_dead && !ota_error) {
            if (consecutive_failures >= 10) {
                ESP_LOGE(TAG, "Too many failures — aborting");
                break;
            }

            uint32_t remaining = dl.total_chunks - dl.next_chunk;
            uint8_t  n          = (remaining > CHUNKS_AT_ONCE) ? CHUNKS_AT_ONCE : (uint8_t)remaining;
            uint8_t  req_status = (dl.next_chunk == 0) ? STATUS_STARTED : STATUS_INPROG;

            uint8_t mod[4 + 32 + 2 + 1 + 2 * CHUNKS_AT_ONCE + 1 + 1];
            size_t  mlen = build_mod9(dl.file_id, (uint16_t)dl.next_chunk, n, req_status, mod);
            if (send_secure(s, BEACON_UPLINK, mod, mlen)) {
                ESP_LOGE(TAG, "Send chunk request failed — socket dead");
                socket_dead = 1; break;
            }
            ESP_LOGI(TAG, "Requested chunks %lu..%lu (of %lu)",
                     (unsigned long)dl.next_chunk,
                     (unsigned long)(dl.next_chunk + n - 1),
                     (unsigned long)(dl.total_chunks - 1));

            int got = 0;
            for (int retry = 0; retry < 5 && !got; retry++) {
                int r = recv_inner(s, &pf, 10000);
                if (r < 0) { socket_dead = 1; break; }
                if (r == 1) send_ack(s, s->last_rx_seq);
                if (pf.type != INNER_CHUNK_OUT) continue;
                if (memcmp(pf.chunks.file_id, dl.file_id, 32) != 0) continue;
                if (pf.chunks.n_chunks != n) {
                    ESP_LOGW(TAG, "Got %u chunks, expected %u", pf.chunks.n_chunks, n);
                    continue;
                }

                for (uint8_t ci = 0; ci < pf.chunks.n_chunks && !ota_error; ci++) {
                    uint32_t chunk_id = dl.next_chunk + ci;
                    uint32_t offset   = chunk_id * CHUNK_SIZE;
                    if (offset >= dl.file_size) continue;
                    uint32_t clen = (offset + pf.chunks.chunk_size > dl.file_size)
                                    ? (dl.file_size - offset)
                                    : pf.chunks.chunk_size;
                    const uint8_t *data = pf.chunks.data + ci * pf.chunks.chunk_size;

                    err = esp_ota_write(ota_handle, data, clen);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "esp_ota_write: %s", esp_err_to_name(err));
                        ota_error = 1; break;
                    }
                    mbedtls_sha256_update(&sha, data, clen);
                }
                if (ota_error) break;

                dl.next_chunk += pf.chunks.n_chunks;
                consecutive_failures = 0;
                got = 1;
                ESP_LOGI(TAG, "Flashed chunks — progress %lu / %lu",
                         (unsigned long)dl.next_chunk, (unsigned long)dl.total_chunks);
            }
            if (!got && !socket_dead) consecutive_failures++;
        }

        /* ---- Phase 5: Verify SHA-256 then commit or abort ---- */
        uint8_t mod[4 + 66];
        size_t  mlen;

        if (socket_dead) {
            esp_ota_abort(ota_handle);
            mbedtls_sha256_free(&sha);
            return;
        }

        if (ota_error || dl.next_chunk < dl.total_chunks) {
            ESP_LOGE(TAG, "Download incomplete or OTA error — aborting");
            esp_ota_abort(ota_handle);
            mbedtls_sha256_free(&sha);
            mlen = build_mod10(dl.file_id, dl.file_hash, STATUS_FAILURE, mod);
            send_secure(s, BEACON_UPLINK, mod, mlen);
            continue;
        }

        uint8_t calc_hash[32];
        mbedtls_sha256_finish(&sha, calc_hash);
        mbedtls_sha256_free(&sha);
        char calc_hex[65]; bytes_to_hex(calc_hash, 32, calc_hex);
        ESP_LOGI(TAG, "Computed hash: %s", calc_hex);

        if (memcmp(calc_hash, dl.file_hash, 32) != 0) {
            ESP_LOGE(TAG, "SHA-256 MISMATCH — OTA aborted");
            esp_ota_abort(ota_handle);
            mlen = build_mod10(dl.file_id, dl.file_hash, STATUS_FAILURE, mod);
            send_secure(s, BEACON_UPLINK, mod, mlen);
            continue;
        }

        ESP_LOGI(TAG, "SHA-256 verified — committing OTA to %s", ota_part->label);
        err = esp_ota_end(ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
            mlen = build_mod10(dl.file_id, dl.file_hash, STATUS_FAILURE, mod);
            send_secure(s, BEACON_UPLINK, mod, mlen);
            continue;
        }

        err = esp_ota_set_boot_partition(ota_part);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
            mlen = build_mod10(dl.file_id, dl.file_hash, STATUS_FAILURE, mod);
            send_secure(s, BEACON_UPLINK, mod, mlen);
            continue;
        }

        ESP_LOGI(TAG, "OTA committed — sending SUCCESS then rebooting");
        mlen = build_mod10(dl.file_id, dl.file_hash, STATUS_SUCCESS, mod);
        send_secure(s, BEACON_UPLINK, mod, mlen);
        vTaskDelay(pdMS_TO_TICKS(500));   /* let the ACK transmit before reset */
        esp_restart();
    }
}

/* ===========================================================================
 * FreeRTOS task — connects, stays connected, reconnects if dropped
 * ========================================================================= */
static void server_task(void *arg)
{
    la5_session_t *s = calloc(1, sizeof(la5_session_t));
    if (!s) { ESP_LOGE(TAG, "OOM"); vTaskDelete(NULL); return; }

    /* Device ID and RNG are initialised once for the lifetime of the task */
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    snprintf(s->dev_id, sizeof(s->dev_id), "0000%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device: %s", s->dev_id);

    mbedtls_entropy_init(&s->entropy);
    mbedtls_ctr_drbg_init(&s->drbg);
    if (mbedtls_ctr_drbg_seed(&s->drbg, mbedtls_entropy_func, &s->entropy, NULL, 0)) {
        ESP_LOGE(TAG, "RNG init failed"); goto shutdown;
    }

    while (1) {
        s->sock  = -1;
        s->rxlen = 0;
        s->seq   = 0;

        /* DNS + TCP connect */
        int connected = 0;
        {
            char port_str[8];
            snprintf(port_str, sizeof(port_str), "%d", PORT);
            struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
            struct addrinfo *res  = NULL;
            if (getaddrinfo(HOST, port_str, &hints, &res) == 0 && res) {
                s->sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
                if (s->sock >= 0 && connect(s->sock, res->ai_addr, res->ai_addrlen) == 0) {
                    connected = 1;
                    ESP_LOGI(TAG, "TCP connected to %s:%d", HOST, PORT);
                } else if (s->sock >= 0) {
                    close(s->sock); s->sock = -1;
                }
                freeaddrinfo(res);
            } else {
                ESP_LOGE(TAG, "DNS failed for %s", HOST);
            }
        }

        if (connected && do_login(s) == 0) {
            run_download(s);   /* returns only when socket dies */
        }

        if (s->sock >= 0) { close(s->sock); s->sock = -1; }
        ESP_LOGI(TAG, "Reconnecting in 10s...");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

shutdown:
    mbedtls_ctr_drbg_free(&s->drbg);
    mbedtls_entropy_free(&s->entropy);
    free(s);
    vTaskDelete(NULL);
}

void server_connect_test(void)
{
    xTaskCreate(server_task, "srv", 16384, NULL, 3, NULL);
}
