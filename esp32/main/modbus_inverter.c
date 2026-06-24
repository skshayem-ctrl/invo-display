/*
 * modbus_inverter.c — Megmeet inverter Modbus RTU via USB-RS485 dongle
 *
 * Connection:
 *   USB-RS485 dongle USB end → ESP32 USB-C port  (host mode)
 *   Dongle screw terminal A  → Inverter A+
 *   Dongle screw terminal B  → Inverter B-
 *
 * Works with: CP2102, FTDI FT232, PL2303 (standard CDC-ACM)
 * CH340 note: CH340 is vendor-specific — if your dongle uses CH340,
 *             monitor will print "CH340 not supported via CDC-ACM"
 *             and you will need to swap to a CP2102/FTDI dongle.
 */
#include "modbus_inverter.h"
#include "ui_common.h"
#include "lvgl_port.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <math.h>
#include <string.h>

/* ── Config ─────────────────────────────────────────────────────────── */

#define MB_BAUD       9600
#define MB_SLAVE      1

#define REG_B1_START  4017
#define REG_B1_COUNT  17      /* 4017-4033 */
#define REG_B2_START  4036
#define REG_B2_COUNT  8       /* 4036-4043 */
#define REG_OUT_CTRL  4049

#define BATT_RATED_V  48.0f
#define BATT_AH       100.0f
#define BATT_EMA      0.15f

/* CH340 VID/PID — cannot open via CDC-ACM, used to print a warning */
#define CH340_VID     0x1A86
#define CH340_PID     0x7523

/* ── State ───────────────────────────────────────────────────────────── */

static const char *TAG = "modbus";

static volatile bool         s_valid       = false;
static volatile int          s_pending_cmd = -1;
static cdc_acm_dev_hdl_t     s_cdc_hdl     = NULL;
static QueueHandle_t         s_rx_queue;
static SemaphoreHandle_t     s_device_sem;   /* given when dongle opens */

bool modbus_inverter_valid(void)            { return s_valid; }
void modbus_inverter_request_output(int on) { s_pending_cmd = on; }

/* ── CRC16 ───────────────────────────────────────────────────────────── */

static uint16_t crc16(const uint8_t *data, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

static inline int16_t s16(uint16_t v) { return (int16_t)v; }

/* ── USB callbacks ───────────────────────────────────────────────────── */

/* Every byte arriving from the dongle goes into the queue */
static bool usb_rx_cb(const uint8_t *data, size_t len, void *arg)
{
    for (size_t i = 0; i < len; i++)
        xQueueSend(s_rx_queue, &data[i], 0);
    return true;
}

/* Device-level events (disconnect, error) */
static void usb_event_cb(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED) {
        ESP_LOGW(TAG, "Dongle disconnected — waiting for reconnect");
        s_valid   = false;
        s_cdc_hdl = NULL;
    }
}

/* Called by cdc_acm_host driver when any USB device connects */
static void new_dev_cb(usb_device_handle_t usb_dev)
{
    /* Read VID/PID so we can identify the dongle */
    const usb_device_desc_t *dev_desc;
    usb_host_get_device_descriptor(usb_dev, &dev_desc);
    uint16_t vid = dev_desc->idVendor;
    uint16_t pid = dev_desc->idProduct;
    ESP_LOGI(TAG, "USB device connected: VID=0x%04X PID=0x%04X", vid, pid);

    if (vid == CH340_VID && pid == CH340_PID) {
        ESP_LOGE(TAG, "CH340 dongle detected — NOT supported via CDC-ACM.");
        ESP_LOGE(TAG, "Please use a CP2102 or FTDI-based RS485 dongle instead.");
        return;
    }

    cdc_acm_host_device_config_t dev_cfg = {
        .data_cb         = usb_rx_cb,
        .event_cb        = usb_event_cb,
        .user_arg        = NULL,
        .out_buffer_size = 64,
        .in_buffer_size  = 256,
    };

    esp_err_t err = cdc_acm_host_open(vid, pid, 0, &dev_cfg, &s_cdc_hdl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot open dongle: %s", esp_err_to_name(err));
        return;
    }

    /* Set 9600 8N1 */
    cdc_acm_line_coding_t lc = {
        .dwDTERate  = MB_BAUD,
        .bCharFormat = 0,   /* 1 stop bit */
        .bParityType = 0,   /* no parity */
        .bDataBits   = 8,
    };
    cdc_acm_host_line_coding_set(s_cdc_hdl, &lc);

    ESP_LOGI(TAG, "RS485 dongle ready @ %d baud 8N1", MB_BAUD);
    xSemaphoreGive(s_device_sem);
}

/* ── USB host event task ─────────────────────────────────────────────── */

static void usb_host_task(void *arg)
{
    while (1) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
            usb_host_device_free_all();
    }
}

/* ── Modbus transport ────────────────────────────────────────────────── */

static int mb_read_regs(uint16_t start, uint8_t count, uint16_t *out)
{
    if (!s_cdc_hdl) return -10;

    uint8_t req[8];
    req[0] = MB_SLAVE; req[1] = 0x03;
    req[2] = start >> 8; req[3] = start & 0xFF;
    req[4] = 0; req[5] = count;
    uint16_t crc = crc16(req, 6);
    req[6] = crc & 0xFF; req[7] = crc >> 8;

    xQueueReset(s_rx_queue);
    cdc_acm_host_data_tx_blocking(s_cdc_hdl, req, 8, 100);

    int expected = 5 + count * 2;
    uint8_t resp[64];
    for (int i = 0; i < expected; i++) {
        if (xQueueReceive(s_rx_queue, &resp[i], pdMS_TO_TICKS(300)) != pdTRUE) {
            ESP_LOGW(TAG, "RX timeout at byte %d/%d (reg %u)", i, expected, start);
            return -1;
        }
    }

    if (resp[1] & 0x80) { ESP_LOGW(TAG, "Modbus exc 0x%02X", resp[2]); return -2; }
    if (resp[2] != count * 2) return -3;
    uint16_t resp_crc = (uint16_t)resp[expected-2] | ((uint16_t)resp[expected-1] << 8);
    if (resp_crc != crc16(resp, expected - 2)) { ESP_LOGW(TAG, "CRC fail"); return -4; }

    for (int i = 0; i < count; i++)
        out[i] = ((uint16_t)resp[3 + i*2] << 8) | resp[4 + i*2];
    return count;
}

static void mb_write_reg(uint16_t addr, uint16_t value)
{
    if (!s_cdc_hdl) return;
    uint8_t req[8];
    req[0] = MB_SLAVE; req[1] = 0x06;
    req[2] = addr >> 8; req[3] = addr & 0xFF;
    req[4] = value >> 8; req[5] = value & 0xFF;
    uint16_t crc = crc16(req, 6);
    req[6] = crc & 0xFF; req[7] = crc >> 8;
    xQueueReset(s_rx_queue);
    cdc_acm_host_data_tx_blocking(s_cdc_hdl, req, 8, 200);
}

/* ── Main poll task ──────────────────────────────────────────────────── */

static void modbus_task(void *arg)
{
    /* Block until USB dongle connects and opens */
    ESP_LOGI(TAG, "Waiting for RS485 dongle...");
    xSemaphoreTake(s_device_sem, portMAX_DELAY);
    ESP_LOGI(TAG, "Starting Modbus RTU polls");

    float smooth_pct  = -1.0f;
    int   valid_streak = 0;

    while (1) {
        /* Reconnect after disconnect */
        if (!s_cdc_hdl) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* Pending output command (from OUT ON/OFF buttons) */
        int cmd = s_pending_cmd;
        if (cmd >= 0) {
            s_pending_cmd = -1;
            mb_write_reg(REG_OUT_CTRL, cmd ? 1 : 0);
            ESP_LOGI(TAG, "Output %s", cmd ? "ON" : "OFF");
            vTaskDelay(pdMS_TO_TICKS(4000));
        }

        /* ── Read two register blocks ─────────────────────────── */
        uint16_t r1[REG_B1_COUNT], r2[REG_B2_COUNT];
        int rc1 = mb_read_regs(REG_B1_START, REG_B1_COUNT, r1);
        vTaskDelay(pdMS_TO_TICKS(50));
        int rc2 = mb_read_regs(REG_B2_START, REG_B2_COUNT, r2);

        if (rc1 < 0 || rc2 < 0) {
            ESP_LOGW(TAG, "Poll failed (rc1=%d rc2=%d)", rc1, rc2);
            s_valid = false;
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        /* ── Decode (same math as invo_bridge.py) ─────────────── */
        float pv_v       = r1[0]  * 0.1f;
        float pv_a       = s16(r1[1])  * 0.1f;
        int   pv_w       = s16(r1[2]);
        float batt_v     = r1[7]  * 0.1f;
        float batt_a     = s16(r1[8])  * 0.1f;
        int   batt_w     = s16(r1[9]);
        float raw_grid_v = r1[11] * 0.1f;
        float grid_hz    = r1[15] * 0.01f;
        float inv_out_v  = r2[0]  * 0.1f;
        float out_a      = s16(r2[1])  * 0.1f;
        int   out_w      = s16(r2[2]);
        float out_hz     = r2[3]  * 0.01f;
        uint16_t op_st   = r2[4];
        float inv_t      = s16(r2[7])  * 0.1f;

        int is_bypassing = (op_st >> 10) & 1;
        int inv_on       = (op_st >>  8) & 1;
        int ac_chg       = (op_st >>  9) & 1;
        int fault        = (op_st >> 11) & 1;
        float out_v = is_bypassing ? raw_grid_v : inv_out_v;

        if ((grid_hz > 0.0f && grid_hz < 44.0f) || grid_hz > 56.0f) {
            ESP_LOGW(TAG, "Bad grid_hz %.2f — discard", grid_hz);
            vTaskDelay(pdMS_TO_TICKS(3000)); continue;
        }
        if ((out_hz > 0.0f && out_hz < 44.0f) || out_hz > 56.0f) {
            ESP_LOGW(TAG, "Bad out_hz %.2f — discard", out_hz);
            vTaskDelay(pdMS_TO_TICKS(3000)); continue;
        }

        /* Battery validity */
        int batt_ok = (batt_v >= BATT_RATED_V * 0.60f &&
                       batt_v <= BATT_RATED_V * 1.20f &&
                       fabsf(batt_a) >= 0.5f);

        if (batt_ok) { valid_streak++; }
        else         { valid_streak = 0; smooth_pct = 0.0f; }

        if (batt_ok && valid_streak >= 3) {
            float raw_pct = (batt_v - 44.0f) / (57.6f - 44.0f) * 100.0f;
            if (raw_pct < 0.0f) raw_pct = 0.0f;
            if (raw_pct > 100.0f) raw_pct = 100.0f;
            if (smooth_pct < 0.0f || smooth_pct == 0.0f)
                smooth_pct = raw_pct;
            else if (fabsf(raw_pct - smooth_pct) < 25.0f)
                smooth_pct = BATT_EMA * raw_pct + (1.0f - BATT_EMA) * smooth_pct;
        }
        if (smooth_pct < 0.0f) smooth_pct = 0.0f;
        int pct = (int)smooth_pct;

        int backup_min = 0;
        if (out_w > 10 && pct > 0) {
            float remaining_wh = (pct / 100.0f) * BATT_AH * BATT_RATED_V;
            backup_min = (int)((remaining_wh / (float)out_w) * 60.0f);
            if (backup_min > 9999) backup_min = 9999;
        }

        float grid_v = raw_grid_v >= 80.0f ? raw_grid_v : 0.0f;

        /* ── Write to gd ────────────────────────────────────────── */
        lvgl_acquire();
        gd.solar_kw  = pv_w > 0  ? (float)pv_w / 1000.0f : 0.0f;
        gd.pv_v      = pv_v  > 0.0f ? pv_v  : 0.0f;
        gd.pv_a      = pv_a  > 0.0f ? pv_a  : 0.0f;
        gd.load_kw   = out_w > 0  ? (float)out_w / 1000.0f : 0.0f;
        gd.batt_pct  = pct;
        gd.batt_v    = batt_ok ? batt_v : 0.0f;
        gd.batt_a    = batt_a;
        gd.chg_kw    = (float)batt_w / 1000.0f;
        gd.batt_temp = (-10.0f <= inv_t && inv_t <= 120.0f) ? inv_t : 0.0f;
        gd.backup_h  = backup_min / 60;
        gd.backup_m  = backup_min % 60;
        gd.grid_v    = grid_v;
        gd.grid_hz   = grid_hz;
        gd.out_v     = out_v;
        gd.out_hz    = out_hz;
        gd.out_a     = out_a;
        gd.inv_on    = inv_on;
        gd.ac_chg    = ac_chg;
        gd.bypassing = is_bypassing;
        gd.fault     = fault;
        gd.voltage   = (int)(batt_ok ? batt_v : 0.0f);
        gd.current   = batt_a;
        s_valid = true;
        lvgl_release();

        ESP_LOGI(TAG,
            "pv=%.1fV/%.1fA/%dW batt=%.1fV/%.1fA/%d%% "
            "out=%.1fV/%.2fHz/%dW grid=%.1fV/%.2fHz temp=%.1fC",
            pv_v, pv_a, pv_w, batt_v, batt_a, pct,
            out_v, out_hz, out_w, grid_v, grid_hz, inv_t);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* ── Init ────────────────────────────────────────────────────────────── */

void modbus_inverter_start(void)
{
    s_rx_queue   = xQueueCreate(256, sizeof(uint8_t));
    s_device_sem = xSemaphoreCreateBinary();

    /* USB host stack */
    usb_host_config_t host_cfg = {
        .skip_phy_setup = false,
        .intr_flags     = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_cfg));
    xTaskCreate(usb_host_task, "usb_host", 4096, NULL, 5, NULL);

    /* CDC-ACM class driver */
    cdc_acm_host_driver_config_t cdc_cfg = {
        .driver_task_stack_size = 4096,
        .driver_task_priority   = 5,
        .xCoreID                = 0,
        .new_dev_cb             = new_dev_cb,
    };
    ESP_ERROR_CHECK(cdc_acm_host_install(&cdc_cfg));

    xTaskCreate(modbus_task, "modbus_inv", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "USB Modbus RTU ready — plug in RS485 dongle");
}
