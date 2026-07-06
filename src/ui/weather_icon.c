#include "weather_icon.h"

/*
 * Weather icons via LV_EVENT_DRAW_MAIN callback — no canvas, no deadlock.
 * The callback runs inside LVGL's render loop so the layer is already live.
 *
 * WMO code is stored in obj user_data; weather_icon_update() just changes
 * it and calls lv_obj_invalidate() to schedule a repaint.
 */

/* ── colour palette ─────────────────────────────────────────────── */
#define COL_SUN     lv_color_hex(0xFFB300)
#define COL_CLOUD   lv_color_hex(0x8A9BB0)
#define COL_DARK    lv_color_hex(0x374151)
#define COL_RAIN    lv_color_hex(0x4FC3F7)
#define COL_SNOW    lv_color_hex(0xCFE8FF)
#define COL_THUNDER lv_color_hex(0xFFEE58)
#define COL_FOG     lv_color_hex(0xB0BEC5)

/* ── draw helpers (all coords relative to widget top-left ox,oy) ── */

static void dc(lv_layer_t *l, int ox, int oy,
               int cx, int cy, int r,
               lv_color_t col, lv_opa_t opa)
{
    lv_draw_arc_dsc_t d; lv_draw_arc_dsc_init(&d);
    d.color = col; d.opa = opa;
    d.width = r; d.start_angle = 0; d.end_angle = 360;
    d.center.x = ox + cx; d.center.y = oy + cy; d.radius = (uint16_t)r;
    lv_draw_arc(l, &d);
}

static void dl(lv_layer_t *l, int ox, int oy,
               int x1, int y1, int x2, int y2,
               int w, lv_color_t col, lv_opa_t opa)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.color = col; d.opa = opa; d.width = w;
    d.round_start = 1; d.round_end = 1;
    d.p1.x = ox+x1; d.p1.y = oy+y1;
    d.p2.x = ox+x2; d.p2.y = oy+y2;
    lv_draw_line(l, &d);
}

static void dr(lv_layer_t *l, int ox, int oy,
               int x, int y, int w, int h, int r,
               lv_color_t col, lv_opa_t opa)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = col; d.bg_opa = opa; d.radius = r; d.border_width = 0;
    lv_area_t a = { ox+x, oy+y, ox+x+w-1, oy+y+h-1 };
    lv_draw_rect(l, &d, &a);
}

/* ── icon renderers ──────────────────────────────────────────────── */

static void draw_sun(lv_layer_t *l, int ox, int oy, int s)
{
    int cx = s/2, cy = s/2, r = s*32/100;
    int r2 = r + s/10, r3 = s/2 - 1, lw = s/12 + 1;
    static const int8_t cs[8] = { 10, 7, 0,-7,-10,-7, 0, 7 };
    static const int8_t ss[8] = {  0, 7,10, 7,  0,-7,-10,-7 };
    for (int i = 0; i < 8; i++)
        dl(l, ox,oy, cx+r2*cs[i]/10, cy+r2*ss[i]/10,
                     cx+r3*cs[i]/10, cy+r3*ss[i]/10,
                     lw, COL_SUN, LV_OPA_COVER);
    dc(l, ox,oy, cx, cy, r, COL_SUN, LV_OPA_COVER);
}

static void draw_cloud(lv_layer_t *l, int ox, int oy, int s,
                        int yshift, lv_color_t col)
{
    int u = s/8, by = s*55/100 + yshift;
    dc(l, ox,oy, s/2,       by-u,   u*2, col, LV_OPA_COVER);
    dc(l, ox,oy, s/2-u*2,   by,     u*2, col, LV_OPA_COVER);
    dc(l, ox,oy, s/2+u*2,   by,     u*2, col, LV_OPA_COVER);
    dr(l, ox,oy, s/2-u*4, by, u*8, u*2, 0, col, LV_OPA_COVER);
}

static void draw_fog(lv_layer_t *l, int ox, int oy, int s)
{
    int bh = s/9, bw = s*75/100, cx2 = (s-bw)/2;
    dr(l,ox,oy, cx2,          s*25/100, bw,       bh, bh/2, COL_FOG, LV_OPA_COVER);
    dr(l,ox,oy, cx2+bw/8,     s*43/100, bw-bw/4,  bh, bh/2, COL_FOG, LV_OPA_70);
    dr(l,ox,oy, cx2,          s*61/100, bw,       bh, bh/2, COL_FOG, LV_OPA_50);
    dr(l,ox,oy, cx2+bw/8,     s*76/100, bw-bw/4,  bh, bh/2, COL_FOG, LV_OPA_30);
}

static void draw_rain(lv_layer_t *l, int ox, int oy, int s,
                       lv_color_t cloud_col, int drops)
{
    draw_cloud(l, ox,oy, s, -s/8, cloud_col);
    int lw = s/12+1, step = s/(drops+1), x0 = step;
    for (int i = 0; i < drops; i++, x0 += step)
        dl(l,ox,oy, x0, s*62/100, x0-s/14, s*62/100+s/6,
           lw, COL_RAIN, LV_OPA_COVER);
}

static void draw_snow(lv_layer_t *l, int ox, int oy, int s)
{
    draw_cloud(l, ox,oy, s, -s/8, COL_CLOUD);
    int lw = s/12+1, r = s/10;
    int pts[3][2] = { {s/4,s*72/100}, {s/2,s*78/100}, {s*3/4,s*72/100} };
    for (int i = 0; i < 3; i++) {
        int cx2 = pts[i][0], cy2 = pts[i][1];
        dl(l,ox,oy, cx2-r, cy2, cx2+r, cy2, lw, COL_SNOW, LV_OPA_COVER);
        dl(l,ox,oy, cx2, cy2-r, cx2, cy2+r, lw, COL_SNOW, LV_OPA_COVER);
        dl(l,ox,oy, cx2-r*7/10, cy2-r*7/10, cx2+r*7/10, cy2+r*7/10,
           lw, COL_SNOW, LV_OPA_COVER);
        dl(l,ox,oy, cx2+r*7/10, cy2-r*7/10, cx2-r*7/10, cy2+r*7/10,
           lw, COL_SNOW, LV_OPA_COVER);
    }
}

static void draw_thunder(lv_layer_t *l, int ox, int oy, int s)
{
    draw_cloud(l, ox,oy, s, -s/8, COL_DARK);
    int lw = s/9+1, mx = s/2, ty = s*60/100, by2 = s*88/100, mid = (ty+by2)/2;
    dl(l,ox,oy, mx+s/8, ty,  mx-s/12, mid,  lw, COL_THUNDER, LV_OPA_COVER);
    dl(l,ox,oy, mx-s/12, mid, mx+s/10, mid, lw, COL_THUNDER, LV_OPA_COVER);
    dl(l,ox,oy, mx+s/10, mid, mx-s/8,  by2, lw, COL_THUNDER, LV_OPA_COVER);
}

static void partly_cloudy(lv_layer_t *l, int ox, int oy, int s)
{
    int sr = s*22/100;
    dc(l, ox,oy, s*30/100, s*30/100, sr, COL_SUN, LV_OPA_COVER);
    draw_cloud(l, ox,oy, s, s/8, COL_CLOUD);
}

/* ── special home-screen icons (virtual codes > 900) ─────────────── */

/* thermometer: vertical tube + bulb + tick marks */
static void draw_thermometer(lv_layer_t *l, int ox, int oy, int s)
{
    int cx  = s / 2;
    int tw  = s / 7;          /* tube width          */
    int br  = s / 5;          /* bulb radius         */
    int ty  = s / 10;         /* tube top y          */
    int by  = s - br - s/10;  /* bulb centre y       */
    lv_color_t col = lv_color_hex(0xFF6B35);

    /* bulb (filled circle at bottom) */
    dc(l, ox,oy, cx, by, br, col, LV_OPA_COVER);
    /* tube body */
    dr(l, ox,oy, cx - tw/2, ty, tw, by - ty, tw/2, col, LV_OPA_COVER);
    /* tick marks */
    int lw = LV_MAX(1, s/14);
    for (int i = 0; i < 4; i++) {
        int ty2 = ty + (by - ty) * i / 4 + (by-ty)/8;
        dl(l,ox,oy, cx + tw/2, ty2, cx + tw/2 + s/7, ty2, lw,
           lv_color_hex(0xFFAA80), LV_OPA_70);
    }
}

/* humidity: water drop + two horizontal moisture-wave lines below */
static void draw_humidity_new(lv_layer_t *l, int ox, int oy, int s)
{
    int cx = s/2, r = s*27/100;
    int by = s*62/100;
    lv_color_t col = lv_color_hex(0x4FC3F7);
    /* body: filled circle */
    dc(l, ox,oy, cx, by, r, col, LV_OPA_COVER);
    /* top spike */
    int lw = r * 2;
    int tip_y = s * 12 / 100;
    dl(l,ox,oy, cx, tip_y, cx - r, by - r/2, lw, col, LV_OPA_COVER);
    dl(l,ox,oy, cx, tip_y, cx + r, by - r/2, lw, col, LV_OPA_COVER);
    dc(l, ox,oy, cx, by, r, col, LV_OPA_COVER);
    /* glint */
    dc(l, ox,oy, cx - r/3, by - r/3, r/5, lv_color_hex(0xFFFFFF), LV_OPA_60);
    /* two moisture wave lines below the drop */
    int wlw  = LV_MAX(1, s/12);
    int half = r * 6/10;
    int wy1  = by + r + s/14;
    int wy2  = wy1 + wlw + s/18;
    if (wy1 + wlw < s)
        dl(l,ox,oy, cx - half,      wy1, cx + half,      wy1, wlw, col, LV_OPA_70);
    if (wy2 + wlw < s)
        dl(l,ox,oy, cx - half*7/10, wy2, cx + half*7/10, wy2, wlw, col, LV_OPA_40);
}

/* AQI / air quality: three horizontal wind-flow lines in teal */
static void draw_aqi_wind(lv_layer_t *l, int ox, int oy, int s)
{
    lv_color_t col = lv_color_hex(0x26C6DA);
    int lw = LV_MAX(2, s/10);
    int cr = lw/2 + 1;
    int x0 = s*15/100;
    /* top line — widest */
    int y1 = s*28/100;
    dl(l,ox,oy, x0, y1, s*82/100, y1, lw, col, LV_OPA_COVER);
    dc(l,ox,oy, x0, y1, cr, col, LV_OPA_COVER);
    /* middle line */
    int y2 = s*50/100;
    dl(l,ox,oy, x0, y2, s*70/100, y2, lw, col, LV_OPA_80);
    dc(l,ox,oy, x0, y2, cr, col, LV_OPA_80);
    /* bottom line — narrowest */
    int y3 = s*72/100;
    dl(l,ox,oy, x0, y3, s*55/100, y3, lw, col, LV_OPA_50);
    dc(l,ox,oy, x0, y3, cr, col, LV_OPA_50);
}

/* ── render dispatch ─────────────────────────────────────────────── */

static void do_render(lv_layer_t *layer, int ox, int oy, int s, int wmo)
{
    if      (wmo == 999) draw_thermometer(layer, ox, oy, s);
    else if (wmo == 998) draw_humidity_new(layer, ox, oy, s);
    else if (wmo == 997) draw_aqi_wind(layer, ox, oy, s);
    else if (wmo == 0)   draw_sun(layer, ox, oy, s);
    else if (wmo <= 3)   partly_cloudy(layer, ox, oy, s);
    else if (wmo <= 48)  draw_fog(layer, ox, oy, s);
    else if (wmo <= 55)  draw_rain(layer, ox, oy, s, COL_CLOUD, 3);
    else if (wmo <= 65)  draw_rain(layer, ox, oy, s, COL_DARK,  4);
    else if (wmo <= 77)  draw_snow(layer, ox, oy, s);
    else if (wmo <= 82)  draw_rain(layer, ox, oy, s, COL_DARK,  4);
    else                 draw_thunder(layer, ox, oy, s);
}

/* ── LVGL draw event ─────────────────────────────────────────────── */

static void icon_draw_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_obj_t   *obj   = lv_event_get_target(e);
    lv_area_t   coords;
    lv_obj_get_coords(obj, &coords);
    int wmo = (int)(intptr_t)lv_obj_get_user_data(obj);
    int sz  = lv_obj_get_width(obj);
    do_render(layer, coords.x1, coords.y1, sz, wmo);
}

/* ── public API ──────────────────────────────────────────────────── */

lv_obj_t *weather_icon_create(lv_obj_t *parent, int sz, int wmo_code)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, sz, sz);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(obj, (void *)(intptr_t)wmo_code);
    lv_obj_add_event_cb(obj, icon_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}

void weather_icon_update(lv_obj_t *icon, int wmo_code)
{
    if (!icon) return;
    lv_obj_set_user_data(icon, (void *)(intptr_t)wmo_code);
    lv_obj_invalidate(icon);
}
