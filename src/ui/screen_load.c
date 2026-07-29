#include "ui_common.h"
#include <stdio.h>

/* Helper: one appliance row in the list */
static lv_obj_t *load_row(lv_obj_t *par, const char *icon, lv_color_t icon_col,
                           const char *name, const char *kwval, bool is_live)
{
    lv_obj_t *row = lv_obj_create(par);
    lv_obj_set_size(row, 400, 52);
    lv_obj_set_style_bg_color(row, C_CARD, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(row, C_LINE, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_pad_hor(row, 14, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *ic = lv_label_create(row);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_color(ic, icon_col, 0);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_20, 0);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *nm = lv_label_create(row);
    lv_label_set_text(nm, name);
    lv_obj_set_style_text_color(nm, C_WHITE, 0);
    lv_obj_set_style_text_font(nm, &lv_font_montserrat_14, 0);
    lv_obj_align(nm, LV_ALIGN_LEFT_MID, 32, 0);

    lv_obj_t *kw = lv_label_create(row);
    lv_label_set_text(kw, kwval);
    lv_obj_set_style_text_color(kw, is_live ? C_BLUE : C_GRAY, 0);
    lv_obj_set_style_text_font(kw, &lv_font_montserrat_14, 0);
    lv_obj_align(kw, LV_ALIGN_RIGHT_MID, 0, 0);

    return kw; /* return value label for live updates */
}

lv_obj_t *screen_load_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_ld = add_detail_header(scr, "Load");

    /* ── Total load progress bar (% of 3 kW nominal) ─────────────── */
    {
        lv_obj_t *bg = lv_obj_create(scr);
        lv_obj_set_size(bg, 400, 16);
        lv_obj_align(bg, LV_ALIGN_CENTER, 0, -222);
        lv_obj_set_style_bg_color(bg, C_DGRAY, 0);
        lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bg, 0, 0);
        lv_obj_set_style_radius(bg, 8, 0);
        lv_obj_set_style_pad_all(bg, 0, 0);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        app.w_ld_bar = lv_bar_create(bg);
        lv_obj_set_size(app.w_ld_bar, 400, 16);
        lv_obj_center(app.w_ld_bar);
        lv_bar_set_range(app.w_ld_bar, 0, 100);
        lv_bar_set_value(app.w_ld_bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(app.w_ld_bar, C_DGRAY, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(app.w_ld_bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(app.w_ld_bar, 8, LV_PART_MAIN);
        lv_obj_set_style_bg_color(app.w_ld_bar, C_BLUE, LV_PART_INDICATOR);
        lv_obj_set_style_radius(app.w_ld_bar, 8, LV_PART_INDICATOR);
        lv_obj_clear_flag(app.w_ld_bar, LV_OBJ_FLAG_CLICKABLE);
    }

    /* ── Legend row ──────────────────────────────────────────────── */
    {
        lv_obj_t *leg = mk_row(scr);
        lv_obj_align(leg, LV_ALIGN_CENTER, 0, -196);
        lv_obj_set_style_pad_column(leg, 16, 0);

        lv_obj_t *mon = lv_label_create(leg);
        lv_label_set_text(mon, "Monitored  0.0 kW");
        lv_obj_set_style_text_color(mon, C_GRAY, 0);
        lv_obj_set_style_text_font(mon, &lv_font_montserrat_12, 0);

        lv_obj_t *sep = lv_label_create(leg);
        lv_label_set_text(sep, "|");
        lv_obj_set_style_text_color(sep, C_LINE, 0);
        lv_obj_set_style_text_font(sep, &lv_font_montserrat_12, 0);

        lv_obj_t *unm_lbl = lv_label_create(leg);
        lv_label_set_text(unm_lbl, "Unmonitored");
        lv_obj_set_style_text_color(unm_lbl, C_LTGRAY, 0);
        lv_obj_set_style_text_font(unm_lbl, &lv_font_montserrat_12, 0);

        app.w_ld_kw = lv_label_create(leg);
        lv_label_set_text(app.w_ld_kw, "-- kW");
        lv_obj_set_style_text_color(app.w_ld_kw, C_BLUE, 0);
        lv_obj_set_style_text_font(app.w_ld_kw, &lv_font_montserrat_12, 0);
    }

    /* ── "Active appliances" caption ─────────────────────────────── */
    add_hdiv(scr, 186, 400); /* TOP_MID y=186 → CENTER -174 */
    mk_lbl(scr, "ACTIVE APPLIANCES", &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_CENTER, 0, -162);

    /* ── Scrollable appliance list ───────────────────────────────── */
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, 440, 340);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, +52);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 8, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Real data rows (from inverter output) */
    app.w_ld_out_v  = load_row(list, LV_SYMBOL_POWER, C_GREEN,
                                "AC Output",  "--", true);
    app.w_ld_out_w  = load_row(list, LV_SYMBOL_CHARGE, C_AMBER,
                                "Active Load", "--", true);
    app.w_ld_out_a  = load_row(list, LV_SYMBOL_REFRESH, C_BLUE,
                                "AC Current", "--", true);
    app.w_ld_out_hz = load_row(list, LV_SYMBOL_AUDIO, C_PURPLE,
                                "Frequency",  "--", true);

    /* Dummy appliance rows (sensor data pending) */
    load_row(list, LV_SYMBOL_SETTINGS, C_GRAY, "AC Unit",     "0.9 kW", false);
    load_row(list, LV_SYMBOL_SETTINGS, C_GRAY, "Refrigerator","0.2 kW", false);
    load_row(list, LV_SYMBOL_SETTINGS, C_GRAY, "Lights",      "0.1 kW", false);

    /* null unused handles */
    app.w_ld_kwh   = NULL;
    app.w_ld_chart = NULL;
    app.w_ld_ser   = NULL;

    add_logo(scr, -22);
    return scr;
}
