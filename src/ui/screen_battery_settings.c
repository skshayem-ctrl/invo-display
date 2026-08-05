#include "ui_common.h"
#include "modbus_inverter.h"

static int s_chg_target = 0;
static lv_obj_t *s_chg_target_lbl = NULL;
static lv_obj_t *s_chg_last_lbl = NULL;

static int s_chgv_target = 512; /* tenths of a volt — default 51.2V */
static lv_obj_t *s_chgv_target_lbl = NULL;
static lv_obj_t *s_chgv_last_lbl = NULL;

static int s_fan_target = 50;
static lv_obj_t *s_fan_target_lbl  = NULL;
static lv_obj_t *s_fan_last_lbl    = NULL;
static lv_obj_t *s_fan_actual_lbl  = NULL;

static void chg_minus_cb(lv_event_t *e)
{
    if (s_chg_target - 50 >= 0)
        s_chg_target -= 50;
    if (s_chg_target_lbl)
        lv_label_set_text_fmt(s_chg_target_lbl, "%d W", s_chg_target);
}

static void chg_plus_cb(lv_event_t *e)
{
    s_chg_target += 50;
    if (s_chg_target_lbl)
        lv_label_set_text_fmt(s_chg_target_lbl, "%d W", s_chg_target);
}

static void chg_set_cb(lv_event_t *e)
{
    modbus_inverter_request_chg_w(s_chg_target);
    if (s_chg_last_lbl)
        lv_label_set_text_fmt(s_chg_last_lbl, "SET : %d W", s_chg_target);
}

static void chgv_minus_cb(lv_event_t *e)
{
    if (s_chgv_target - 2 >= 462)
        s_chgv_target -= 2;
    if (s_chgv_target_lbl)
        lv_label_set_text_fmt(s_chgv_target_lbl, "%d.%d V", s_chgv_target / 10, s_chgv_target % 10);
}

static void chgv_plus_cb(lv_event_t *e)
{
    if (s_chgv_target + 2 <= 584)
        s_chgv_target += 2;
    if (s_chgv_target_lbl)
        lv_label_set_text_fmt(s_chgv_target_lbl, "%d.%d V", s_chgv_target / 10, s_chgv_target % 10);
}

static void chgv_set_cb(lv_event_t *e)
{
    modbus_inverter_request_chg_v(s_chgv_target);
    if (s_chgv_last_lbl)
        lv_label_set_text_fmt(s_chgv_last_lbl, "SET : %d.%d V", s_chgv_target / 10, s_chgv_target % 10);
}

static void fan_minus_cb(lv_event_t *e)
{
    if (s_fan_target - 5 >= 0)
        s_fan_target -= 5;
    if (s_fan_target_lbl)
        lv_label_set_text_fmt(s_fan_target_lbl, "%d%%", s_fan_target);
}

static void fan_plus_cb(lv_event_t *e)
{
    if (s_fan_target + 5 <= 100)
        s_fan_target += 5;
    if (s_fan_target_lbl)
        lv_label_set_text_fmt(s_fan_target_lbl, "%d%%", s_fan_target);
}

static void fan_set_cb(lv_event_t *e)
{
    modbus_inverter_request_fan(s_fan_target);
    if (s_fan_last_lbl)
        lv_label_set_text_fmt(s_fan_last_lbl, "SET : %d%%", s_fan_target);
}

lv_obj_t *screen_battery_settings_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    mk_lbl(scr, LV_SYMBOL_BATTERY_3, &lv_font_montserrat_20, C_GREEN, LV_ALIGN_TOP_MID, 0, 28);
    mk_lbl(scr, "Battery Settings", &lv_font_montserrat_16, C_GRAY, LV_ALIGN_TOP_MID, 0, 56);

    /* ── Charge Power card ─────────────────────────────────────────── */
    lv_obj_t *ccard = lv_obj_create(scr);
    lv_obj_set_size(ccard, 300, 140);
    lv_obj_align(ccard, LV_ALIGN_CENTER, 0, -170);
    lv_obj_set_style_bg_color(ccard, lv_color_hex(0x0a0f1a), 0);
    lv_obj_set_style_border_color(ccard, C_AMBER, 0);
    lv_obj_set_style_border_width(ccard, 1, 0);
    lv_obj_set_style_radius(ccard, 12, 0);
    lv_obj_set_style_pad_all(ccard, 12, 0);
    lv_obj_clear_flag(ccard, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ctl = lv_label_create(ccard);
    lv_label_set_text(ctl, "CHARGE POWER");
    lv_obj_set_style_text_color(ctl, C_AMBER, 0);
    lv_obj_set_style_text_font(ctl, &lv_font_montserrat_12, 0);
    lv_obj_align(ctl, LV_ALIGN_TOP_LEFT, 0, 0);

    s_chg_last_lbl = lv_label_create(ccard);
    lv_label_set_text(s_chg_last_lbl, "SET : --");
    lv_obj_set_style_text_color(s_chg_last_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(s_chg_last_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(s_chg_last_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);

    s_chg_target_lbl = lv_label_create(ccard);
    lv_label_set_text_fmt(s_chg_target_lbl, "%d W", s_chg_target);
    lv_obj_set_style_text_color(s_chg_target_lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(s_chg_target_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(s_chg_target_lbl, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *cm = lv_btn_create(ccard);
    lv_obj_set_size(cm, 40, 40);
    lv_obj_align(cm, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(cm, C_DGRAY, 0);
    lv_obj_set_style_radius(cm, 6, 0);
    lv_obj_add_event_cb(cm, chg_minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cml = lv_label_create(cm);
    lv_label_set_text(cml, "-");
    lv_obj_set_style_text_font(cml, &lv_font_montserrat_16, 0);
    lv_obj_center(cml);

    lv_obj_t *cs = lv_btn_create(ccard);
    lv_obj_set_size(cs, 60, 40);
    lv_obj_align(cs, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(cs, C_AMBER, 0);
    lv_obj_set_style_radius(cs, 6, 0);
    lv_obj_add_event_cb(cs, chg_set_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *csl = lv_label_create(cs);
    lv_label_set_text(csl, "SET");
    lv_obj_set_style_text_font(csl, &lv_font_montserrat_14, 0);
    lv_obj_center(csl);

    lv_obj_t *cp = lv_btn_create(ccard);
    lv_obj_set_size(cp, 40, 40);
    lv_obj_align(cp, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(cp, C_DGRAY, 0);
    lv_obj_set_style_radius(cp, 6, 0);
    lv_obj_add_event_cb(cp, chg_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cpl = lv_label_create(cp);
    lv_label_set_text(cpl, "+");
    lv_obj_set_style_text_font(cpl, &lv_font_montserrat_16, 0);
    lv_obj_center(cpl);

    /* ── Charge Voltage card ───────────────────────────────────────── */
    lv_obj_t *vcard = lv_obj_create(scr);
    lv_obj_set_size(vcard, 300, 140);
    lv_obj_align(vcard, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(vcard, lv_color_hex(0x0a0f1a), 0);
    lv_obj_set_style_border_color(vcard, C_GREEN, 0);
    lv_obj_set_style_border_width(vcard, 1, 0);
    lv_obj_set_style_radius(vcard, 12, 0);
    lv_obj_set_style_pad_all(vcard, 12, 0);
    lv_obj_clear_flag(vcard, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *vtl = lv_label_create(vcard);
    lv_label_set_text(vtl, "CHARGE VOLTAGE");
    lv_obj_set_style_text_color(vtl, C_GREEN, 0);
    lv_obj_set_style_text_font(vtl, &lv_font_montserrat_12, 0);
    lv_obj_align(vtl, LV_ALIGN_TOP_LEFT, 0, 0);

    s_chgv_last_lbl = lv_label_create(vcard);
    lv_label_set_text(s_chgv_last_lbl, "SET : --");
    lv_obj_set_style_text_color(s_chgv_last_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(s_chgv_last_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(s_chgv_last_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);

    s_chgv_target_lbl = lv_label_create(vcard);
    lv_label_set_text_fmt(s_chgv_target_lbl, "%d.%d V", s_chgv_target / 10, s_chgv_target % 10);
    lv_obj_set_style_text_color(s_chgv_target_lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(s_chgv_target_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(s_chgv_target_lbl, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *vm = lv_btn_create(vcard);
    lv_obj_set_size(vm, 40, 40);
    lv_obj_align(vm, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(vm, C_DGRAY, 0);
    lv_obj_set_style_radius(vm, 6, 0);
    lv_obj_add_event_cb(vm, chgv_minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *vml = lv_label_create(vm);
    lv_label_set_text(vml, "-");
    lv_obj_set_style_text_font(vml, &lv_font_montserrat_16, 0);
    lv_obj_center(vml);

    lv_obj_t *vs = lv_btn_create(vcard);
    lv_obj_set_size(vs, 60, 40);
    lv_obj_align(vs, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(vs, C_GREEN, 0);
    lv_obj_set_style_radius(vs, 6, 0);
    lv_obj_add_event_cb(vs, chgv_set_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *vsl = lv_label_create(vs);
    lv_label_set_text(vsl, "SET");
    lv_obj_set_style_text_font(vsl, &lv_font_montserrat_14, 0);
    lv_obj_center(vsl);

    lv_obj_t *vp = lv_btn_create(vcard);
    lv_obj_set_size(vp, 40, 40);
    lv_obj_align(vp, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(vp, C_DGRAY, 0);
    lv_obj_set_style_radius(vp, 6, 0);
    lv_obj_add_event_cb(vp, chgv_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *vpl = lv_label_create(vp);
    lv_label_set_text(vpl, "+");
    lv_obj_set_style_text_font(vpl, &lv_font_montserrat_16, 0);
    lv_obj_center(vpl);

    /* ── Fan Speed card ────────────────────────────────────────────── */
    lv_obj_t *fcard = lv_obj_create(scr);
    lv_obj_set_size(fcard, 300, 140);
    lv_obj_align(fcard, LV_ALIGN_CENTER, 0, 130);
    lv_obj_set_style_bg_color(fcard, lv_color_hex(0x0a0f1a), 0);
    lv_obj_set_style_border_color(fcard, C_BLUE, 0);
    lv_obj_set_style_border_width(fcard, 1, 0);
    lv_obj_set_style_radius(fcard, 12, 0);
    lv_obj_set_style_pad_all(fcard, 12, 0);
    lv_obj_clear_flag(fcard, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ftl = lv_label_create(fcard);
    lv_label_set_text(ftl, "FAN SPEED");
    lv_obj_set_style_text_color(ftl, C_BLUE, 0);
    lv_obj_set_style_text_font(ftl, &lv_font_montserrat_12, 0);
    lv_obj_align(ftl, LV_ALIGN_TOP_LEFT, 0, 0);

    s_fan_actual_lbl = lv_label_create(fcard);
    lv_label_set_text(s_fan_actual_lbl, "NOW : --");
    lv_obj_set_style_text_color(s_fan_actual_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(s_fan_actual_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(s_fan_actual_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);

    s_fan_last_lbl = lv_label_create(fcard);
    lv_label_set_text(s_fan_last_lbl, "SET : --");
    lv_obj_set_style_text_color(s_fan_last_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(s_fan_last_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(s_fan_last_lbl, LV_ALIGN_TOP_MID, 0, 0);

    s_fan_target_lbl = lv_label_create(fcard);
    lv_label_set_text_fmt(s_fan_target_lbl, "%d%%", s_fan_target);
    lv_obj_set_style_text_color(s_fan_target_lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(s_fan_target_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(s_fan_target_lbl, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *fm = lv_btn_create(fcard);
    lv_obj_set_size(fm, 40, 40);
    lv_obj_align(fm, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(fm, C_DGRAY, 0);
    lv_obj_set_style_radius(fm, 6, 0);
    lv_obj_add_event_cb(fm, fan_minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *fml = lv_label_create(fm);
    lv_label_set_text(fml, "-");
    lv_obj_set_style_text_font(fml, &lv_font_montserrat_16, 0);
    lv_obj_center(fml);

    lv_obj_t *fs = lv_btn_create(fcard);
    lv_obj_set_size(fs, 60, 40);
    lv_obj_align(fs, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(fs, C_BLUE, 0);
    lv_obj_set_style_radius(fs, 6, 0);
    lv_obj_add_event_cb(fs, fan_set_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *fsl = lv_label_create(fs);
    lv_label_set_text(fsl, "SET");
    lv_obj_set_style_text_font(fsl, &lv_font_montserrat_14, 0);
    lv_obj_center(fsl);

    lv_obj_t *fp = lv_btn_create(fcard);
    lv_obj_set_size(fp, 40, 40);
    lv_obj_align(fp, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(fp, C_DGRAY, 0);
    lv_obj_set_style_radius(fp, 6, 0);
    lv_obj_add_event_cb(fp, fan_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *fpl = lv_label_create(fp);
    lv_label_set_text(fpl, "+");
    lv_obj_set_style_text_font(fpl, &lv_font_montserrat_16, 0);
    lv_obj_center(fpl);

    /* ── Back button ────────────────────────────────────────────────── */
    lv_obj_t *back = lv_btn_create(scr);
    lv_obj_set_size(back, 110, 42);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -72);
    lv_obj_set_style_bg_color(back, C_DGRAY, 0);
    lv_obj_set_style_radius(back, 21, 0);
    lv_obj_add_event_cb(back, go_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *brow = mk_row(back);
    lv_obj_center(brow);
    lv_obj_t *ba = lv_label_create(brow);
    lv_label_set_text(ba, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(ba, C_WHITE, 0);
    lv_obj_set_style_text_font(ba, &lv_font_montserrat_16, 0);
    lv_obj_t *bbl = lv_label_create(brow);
    lv_label_set_text(bbl, "Settings");
    lv_obj_set_style_text_color(bbl, C_WHITE, 0);
    lv_obj_set_style_text_font(bbl, &lv_font_montserrat_16, 0);

    return scr;
}

void screen_battery_settings_set_chg_last(int watts)
{
    if (s_chg_last_lbl && watts > 0)
        lv_label_set_text_fmt(s_chg_last_lbl, "SET : %d W", watts);
}

void screen_battery_settings_set_chgv_last(int tenths_v)
{
    if (s_chgv_last_lbl && tenths_v > 0)
        lv_label_set_text_fmt(s_chgv_last_lbl, "SET : %d.%d V", tenths_v / 10, tenths_v % 10);
}

void screen_battery_settings_set_fan_actual(int pct)
{
    if (s_fan_actual_lbl)
        lv_label_set_text_fmt(s_fan_actual_lbl, "NOW : %d%%", pct);
}

void screen_battery_settings_set_fan_last(int pct)
{
    if (s_fan_last_lbl)
        lv_label_set_text_fmt(s_fan_last_lbl, "SET : %d%%", pct);
}
