#ifndef INVO_UART_H
#define INVO_UART_H
#include "lvgl/lvgl.h"

void invo_uart_register_solar(lv_obj_t *main_val, lv_obj_t *voltage, lv_obj_t *current);
void invo_uart_register_load(lv_obj_t *main_val, lv_obj_t *cur_draw, lv_obj_t *peak);
void invo_uart_register_home(lv_obj_t *solar_val, lv_obj_t *load_val);
void invo_uart_register_battery(lv_obj_t *soc_pct, lv_obj_t *chg_kw, lv_obj_t *temp, lv_obj_t *backup);
void invo_uart_register_grid(lv_obj_t *grid_v, lv_obj_t *batt_v,
                              lv_obj_t *grid_hz, lv_obj_t *grid_w,
                              lv_obj_t *out_v,  lv_obj_t *out_hz);
void invo_uart_register_solar_detail(lv_obj_t *pv_w, lv_obj_t *grid_v_lbl);
void invo_uart_register_load_detail(lv_obj_t *out_v_lbl, lv_obj_t *out_hz_lbl);
void invo_uart_register_home_battery(lv_obj_t *arc, lv_obj_t *pct_lbl, lv_obj_t *backup_lbl);
void invo_uart_register_inverter_screen(
    lv_obj_t *out_v, lv_obj_t *out_hz, lv_obj_t *out_w,
    lv_obj_t *grid_v, lv_obj_t *grid_hz, lv_obj_t *grid_w, lv_obj_t *grid_a,
    lv_obj_t *batt_v, lv_obj_t *batt_a, lv_obj_t *batt_temp,
    lv_obj_t *inv_on, lv_obj_t *bypass, lv_obj_t *ac_chg, lv_obj_t *fault);
void invo_uart_init(void);

#endif
