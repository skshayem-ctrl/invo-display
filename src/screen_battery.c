#include "ui_common.h"

lv_obj_t *screen_battery_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_bd = add_detail_header(scr, "Battery");

    /* ── Left: SYS STATUS card (centre-relative) ──────────────────── */
    lv_obj_t *scard = lv_obj_create(scr);
    lv_obj_set_size(scard, 185, 200);
    lv_obj_align(scard, LV_ALIGN_CENTER, -195, -20);
    lv_obj_set_style_bg_color(scard, C_CARD, 0);
    lv_obj_set_style_border_color(scard, C_GREEN, 0);
    lv_obj_set_style_border_width(scard, 1, 0);
    lv_obj_set_style_radius(scard, 12, 0);
    lv_obj_set_style_pad_all(scard, 12, 0);
    lv_obj_clear_flag(scard, LV_OBJ_FLAG_SCROLLABLE);

    mk_lbl(scard, LV_SYMBOL_BATTERY_FULL, &lv_font_montserrat_36,
           C_GREEN, LV_ALIGN_TOP_MID, 0, 0);

    app.w_bd_pct = lv_label_create(scard);
    lv_label_set_text_fmt(app.w_bd_pct, "%d%%", gd.batt_pct);
    lv_obj_set_style_text_color(app.w_bd_pct, C_WHITE, 0);
    lv_obj_set_style_text_font(app.w_bd_pct, &lv_font_montserrat_36, 0);
    lv_obj_align(app.w_bd_pct, LV_ALIGN_TOP_MID, 0, 46);

    mk_lbl(scard, "STATE OF CHARGE", &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_TOP_MID, 0, 94);

    lv_obj_t *hc = lv_obj_create(scard);
    lv_obj_set_size(hc, 158, 30);
    lv_obj_align(hc, LV_ALIGN_TOP_MID, 0, 112);
    lv_obj_set_style_bg_color(hc, lv_color_hex(0x0A2010), 0);
    lv_obj_set_style_border_color(hc, C_GREEN, 0);
    lv_obj_set_style_border_width(hc, 1, 0);
    lv_obj_set_style_radius(hc, 6, 0);
    lv_obj_clear_flag(hc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *hr = mk_row(hc);
    lv_obj_center(hr);
    lv_obj_t *hic = lv_label_create(hr);
    lv_label_set_text(hic, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(hic, C_GREEN, 0);
    lv_obj_set_style_text_font(hic, &lv_font_montserrat_14, 0);
    lv_obj_t *htxt = lv_label_create(hr);
    lv_label_set_text(htxt, "GOOD HEALTH");
    lv_obj_set_style_text_color(htxt, C_GREEN, 0);
    lv_obj_set_style_text_font(htxt, &lv_font_montserrat_14, 0);

    mk_lbl(scard, "Battery operating\nnormally.",
           &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── Right: 2×3 stat grid (175×80, centre-relative) ─────────────
       col 0: xoff=-8   col 1: xoff=177
       row 0: yoff=-170 row 1: yoff=-75 row 2: yoff=20
    ──────────────────────────────────────────────────────────────────── */
    static const int xoffs[2] = {-8, 177};
    static const int yoffs[3] = {-170, -75, 20};

    static const char *const icons[6]  = {
        LV_SYMBOL_CHARGE, LV_SYMBOL_REFRESH,
        LV_SYMBOL_POWER,  LV_SYMBOL_DOWNLOAD,
        LV_SYMBOL_OK,     LV_SYMBOL_LIST,
    };
    static const char *const titles[6] = {
        "Charging Power", "Backup Time",
        "Full Charge In", "Temperature",
        "Capacity",       "Usable",
    };
    static const char *const inits[6]  = {
        "2.1 kW",   "3h 45m",
        "1h 20m",   "26\xC2\xB0""C",
        "10.2 kWh", "8.0 kWh",
    };
    static const char *const subs[6]   = {
        "Charging", "At current load",
        "Est. remaining", "Optimal",
        "Total", "78% of total",
    };

    for (int i = 0; i < 6; i++) {
        lv_color_t col;
        switch (i) {
            case 0: col = C_GREEN;              break;
            case 1: col = C_BLUE;               break;
            case 2: col = lv_color_hex(0xBB86FC); break;
            case 3: col = lv_color_hex(0xFF6B6B); break;
            default: col = C_BLUE;              break;
        }

        lv_obj_t *c = lv_obj_create(scr);
        lv_obj_set_size(c, 175, 80);
        lv_obj_align(c, LV_ALIGN_CENTER, xoffs[i % 2], yoffs[i / 2]);
        lv_obj_set_style_bg_color(c, C_CARD, 0);
        lv_obj_set_style_border_color(c, C_LINE, 0);
        lv_obj_set_style_border_width(c, 1, 0);
        lv_obj_set_style_radius(c, 10, 0);
        lv_obj_set_style_pad_all(c, 8, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *ico = lv_label_create(c);
        lv_label_set_text(ico, icons[i]);
        lv_obj_set_style_text_color(ico, col, 0);
        lv_obj_set_style_text_font(ico, &lv_font_montserrat_14, 0);
        lv_obj_align(ico, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *ttl = lv_label_create(c);
        lv_label_set_text(ttl, titles[i]);
        lv_obj_set_style_text_color(ttl, C_GRAY, 0);
        lv_obj_set_style_text_font(ttl, &lv_font_montserrat_12, 0);
        lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 20, 2);

        lv_obj_t *val = lv_label_create(c);
        lv_label_set_text(val, inits[i]);
        lv_obj_set_style_text_color(val, C_WHITE, 0);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
        lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, subs[i][0] ? -16 : 0);

        lv_obj_t *sub = lv_label_create(c);
        lv_label_set_text(sub, subs[i]);
        lv_obj_set_style_text_color(sub, C_GRAY, 0);
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
        lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        /* wire up dynamic handles */
        switch (i) {
            case 0: app.w_bd_chg  = val; break;
            case 1: app.w_bd_bkp  = val; break;
            case 2: app.w_bd_full = val; break;
            case 3: app.w_bd_tmp  = val; break;
            default: break;
        }
    }

    return scr;
}
