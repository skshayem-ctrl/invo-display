#include "ui_common.h"

lv_obj_t *screen_sleep_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, wake_cb, LV_EVENT_CLICKED, NULL);

    app.w_sleep_time = mk_lbl(scr, "10:30", &lv_font_montserrat_48, C_WHITE,
                              LV_ALIGN_CENTER, 0, -18);
    app.w_sleep_date = mk_lbl(scr, "Wednesday 21 May", &lv_font_montserrat_16,
                              C_GRAY, LV_ALIGN_CENTER, 0, 44);

    return scr;
}
