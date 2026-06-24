#include "hal.h"
#include "pi5/lib/driver_backends.h"
#include "pi5/invo/invo_uart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>

#define SETTINGS_FILE "/etc/invo/settings.conf"

/* ── Display & Touch ──────────────────────────────────────────────────────── */

void hal_display_init(void)
{
    driver_backends_init_backend(NULL); /* uses default backend (fbdev/drm) */
}

void hal_touch_init(void)
{
#if LV_USE_EVDEV
    driver_backends_init_backend("EVDEV");
#endif
}

/* ── Backlight ────────────────────────────────────────────────────────────── */

static char backlight_path[256] = "";

static const char * get_backlight_path(void)
{
    if(backlight_path[0]) return backlight_path;
    glob_t g;
    if(glob("/sys/class/backlight/*/brightness", 0, NULL, &g) == 0 && g.gl_pathc > 0) {
        strncpy(backlight_path, g.gl_pathv[0], sizeof(backlight_path) - 1);
        globfree(&g);
        return backlight_path;
    }
    globfree(&g);
    return NULL;
}

void hal_brightness_set(int percent)
{
    const char *path = get_backlight_path();
    if(!path) return;

    /* read max_brightness */
    char max_path[264];
    snprintf(max_path, sizeof(max_path), "%.*s/max_brightness",
             (int)(strrchr(path, '/') - path), path);
    int max_val = 255;
    FILE *mf = fopen(max_path, "r");
    if(mf) { fscanf(mf, "%d", &max_val); fclose(mf); }

    int val = (percent * max_val) / 100;
    FILE *f = fopen(path, "w");
    if(f) { fprintf(f, "%d\n", val); fclose(f); }
}

int hal_brightness_get(void)
{
    const char *path = get_backlight_path();
    if(!path) return 100;
    int val = 255, max_val = 255;
    FILE *f = fopen(path, "r");
    if(f) { fscanf(f, "%d", &val); fclose(f); }

    char max_path[264];
    snprintf(max_path, sizeof(max_path), "%.*s/max_brightness",
             (int)(strrchr(path, '/') - path), path);
    FILE *mf = fopen(max_path, "r");
    if(mf) { fscanf(mf, "%d", &max_val); fclose(mf); }

    return max_val ? (val * 100) / max_val : 100;
}

/* ── Live inverter data ───────────────────────────────────────────────────── */

void hal_data_get(invo_data_t *out)
{
    invo_uart_get_live(
        &out->batt_pct, &out->solar_kw, &out->load_kw,
        &out->fault,    &out->bypassing, &out->inv_on,
        &out->batt_temp
    );
}

/* ── FOTA ─────────────────────────────────────────────────────────────────── */

void hal_fota_trigger(void)
{
    system("systemctl start invo-updater &");
}

/* ── Key-value storage ────────────────────────────────────────────────────── */

int hal_kv_set(const char *key, const char *val)
{
    /* Read existing file into a buffer, update or append key, write back */
    FILE *f = fopen(SETTINGS_FILE, "r");
    char lines[64][256];
    int  count = 0;
    int  found = 0;

    if(f) {
        while(count < 64 && fgets(lines[count], sizeof(lines[count]), f))
            count++;
        fclose(f);
    }

    char new_line[256];
    snprintf(new_line, sizeof(new_line), "%s=%s\n", key, val);

    for(int i = 0; i < count; i++) {
        if(strncmp(lines[i], key, strlen(key)) == 0 && lines[i][strlen(key)] == '=') {
            strncpy(lines[i], new_line, sizeof(lines[i]));
            found = 1;
            break;
        }
    }
    if(!found && count < 64)
        strncpy(lines[count++], new_line, sizeof(lines[count]));

    f = fopen(SETTINGS_FILE, "w");
    if(!f) return -1;
    for(int i = 0; i < count; i++) fputs(lines[i], f);
    fclose(f);
    return 0;
}

int hal_kv_get(const char *key, char *out, int out_len)
{
    FILE *f = fopen(SETTINGS_FILE, "r");
    if(!f) return -1;
    char line[256];
    int klen = strlen(key);
    while(fgets(line, sizeof(line), f)) {
        if(strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            int vlen = strlen(v);
            if(vlen > 0 && v[vlen - 1] == '\n') v[vlen - 1] = '\0';
            strncpy(out, v, out_len - 1);
            out[out_len - 1] = '\0';
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return -1;
}
