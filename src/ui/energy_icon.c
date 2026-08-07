#include "energy_icon.h"

#define COL_SUN   lv_color_hex(0xFFC940)
#define COL_GRID  lv_color_hex(0x3B8BD4)
#define COL_LOAD  lv_color_hex(0x3B8BD4)
#define COL_DISCH lv_color_hex(0xF2A623)
#define COL_DARK  lv_color_hex(0x0A0E14)

static void dl(lv_layer_t *l, int ox, int oy, int x1, int y1, int x2, int y2,
               int w, lv_color_t col, lv_opa_t opa)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.color = col; d.opa = opa; d.width = (uint16_t)w;
    d.round_start = 1; d.round_end = 1;
    d.p1.x = ox + x1; d.p1.y = oy + y1;
    d.p2.x = ox + x2; d.p2.y = oy + y2;
    lv_draw_line(l, &d);
}

static void dr(lv_layer_t *l, int ox, int oy, int x, int y, int w, int h, int r,
               lv_color_t col, lv_opa_t opa)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = col; d.bg_opa = opa; d.radius = r; d.border_width = 0;
    lv_area_t a = { ox+x, oy+y, ox+x+w-1, oy+y+h-1 };
    lv_draw_rect(l, &d, &a);
}

/* ── SOLAR: rectangle panel with 3×2 cell grid ───────────────────── */
static void draw_solar(lv_layer_t *l, int ox, int oy, int s)
{
    lv_color_t col = COL_SUN;
    int lw = LV_MAX(2, s / 12);

    int px = s *  8 / 100, py = s * 18 / 100;
    int pw = s * 84 / 100, ph = s * 64 / 100;
    int br = LV_MAX(2, s / 16);

    /* subtle tint fill */
    dr(l, ox, oy, px, py, pw, ph, br, col, LV_OPA_10);

    /* outer border: four rounded-cap lines */
    dl(l, ox, oy, px + br,    py,      px + pw - br, py,      lw, col, LV_OPA_COVER);
    dl(l, ox, oy, px + br,    py + ph, px + pw - br, py + ph, lw, col, LV_OPA_COVER);
    dl(l, ox, oy, px,         py + br, px,           py+ph-br, lw, col, LV_OPA_COVER);
    dl(l, ox, oy, px + pw,    py + br, px + pw,      py+ph-br, lw, col, LV_OPA_COVER);

    /* cell dividers: 2 vertical + 1 horizontal */
    int gw  = LV_MAX(1, lw - 1);
    int d1x = px + pw     / 3;
    int d2x = px + pw * 2 / 3;
    int dmy = py + ph     / 2;
    dl(l, ox, oy, d1x,     py + lw, d1x,      py + ph - lw, gw, col, LV_OPA_70);
    dl(l, ox, oy, d2x,     py + lw, d2x,      py + ph - lw, gw, col, LV_OPA_70);
    dl(l, ox, oy, px + lw, dmy,     px+pw-lw, dmy,           gw, col, LV_OPA_70);
}

/* ── GRID: smooth AC sine wave (8 segments, 1 full cycle) ────────── */
static void draw_grid(lv_layer_t *l, int ox, int oy, int s)
{
    lv_color_t col = COL_GRID;
    int lw  = LV_MAX(2, s / 10);
    int cy  = s / 2;
    int amp = s * 28 / 100;

    /* x as % of s; sin*100 at 0,45,90,135,180,225,270,315,360 degrees */
    static const uint8_t sx[9] = {  3, 14, 25, 36, 50, 64, 75, 86, 97 };
    static const int8_t  sy[9] = {  0, 71,100, 71,  0,-71,-100,-71,  0 };

    for (int i = 0; i < 8; i++) {
        int x1 = (int)sx[i]   * s / 100;
        int x2 = (int)sx[i+1] * s / 100;
        int y1 = cy - amp * (int)sy[i]   / 100;
        int y2 = cy - amp * (int)sy[i+1] / 100;
        dl(l, ox, oy, x1, y1, x2, y2, lw, col, LV_OPA_COVER);
    }
}

/* ── LOAD: house — solid walls, bold roof, dark door cutout ──────── */
static void draw_load(lv_layer_t *l, int ox, int oy, int s)
{
    lv_color_t col  = COL_LOAD;
    int lw     = LV_MAX(2, s / 12);
    int cx     = s / 2;
    int peak_y = s *  8 / 100;
    int eave_y = s * 46 / 100;
    int wall_l = s * 12 / 100;
    int wall_r = s * 88 / 100;
    int wall_b = s * 94 / 100;

    /* filled wall body */
    dr(l, ox, oy, wall_l, eave_y, wall_r - wall_l, wall_b - eave_y, 0,
       col, LV_OPA_80);

    /* door: dark rectangle cut from bottom-center of wall */
    int dw = s * 24 / 100, dh = s * 32 / 100;
    int dx = cx - dw / 2,  dy = wall_b - dh;
    dr(l, ox, oy, dx, dy, dw, dh, 1, COL_DARK, LV_OPA_COVER);

    /* roof: two slopes + eave line */
    dl(l, ox, oy, cx,          peak_y, wall_l - lw/2, eave_y, lw, col, LV_OPA_COVER);
    dl(l, ox, oy, cx,          peak_y, wall_r + lw/2, eave_y, lw, col, LV_OPA_COVER);
    dl(l, ox, oy, wall_l - lw, eave_y, wall_r + lw,   eave_y, lw, col, LV_OPA_COVER);
}

/* ── DISCHARGE: horizontal battery with Z lightning bolt ─────────── */
static void draw_discharge(lv_layer_t *l, int ox, int oy, int s)
{
    lv_color_t col = COL_DISCH;
    int lw = LV_MAX(2, s / 14);

    int bx = s *  3 / 100, by = s * 22 / 100;
    int bw = s * 78 / 100, bh = s * 56 / 100;
    int br = s *  8 / 100;

    /* shell */
    dr(l, ox, oy, bx, by, bw, bh, br, col, LV_OPA_COVER);
    /* cavity */
    dr(l, ox, oy, bx+lw, by+lw, bw-lw*2, bh-lw*2, LV_MAX(1, br-lw),
       COL_DARK, LV_OPA_COVER);

    /* right terminal nub */
    int tw = s * 10 / 100, th = bh * 4 / 10;
    dr(l, ox, oy, bx+bw, by+(bh-th)/2, tw, th, s*4/100, col, LV_OPA_COVER);

    /* charge fill ~40% */
    int fw = (bw - lw*2) * 40 / 100;
    if (fw > 0)
        dr(l, ox, oy, bx+lw, by+lw, fw, bh-lw*2, LV_MAX(1,br-lw),
           col, LV_OPA_50);

    /* Z-bolt: upper arm → waist → lower arm */
    int bcx  = bx + bw  / 2;
    int bt   = by + bh  * 14 / 100;
    int bm   = by + bh  / 2;
    int bb   = by + bh  * 86 / 100;
    int hw   = bw * 25  / 100;
    int lw2  = LV_MAX(2, s / 9);
    lv_color_t white = lv_color_hex(0xFFFFFF);

    dl(l, ox, oy, bcx+hw,   bt, bcx-hw/2, bm, lw2, white, LV_OPA_COVER);
    dl(l, ox, oy, bcx-hw/2, bm, bcx+hw/2, bm, lw2, white, LV_OPA_COVER);
    dl(l, ox, oy, bcx+hw/2, bm, bcx-hw,   bb, lw2, white, LV_OPA_COVER);
}

/* ── render dispatch ─────────────────────────────────────────────── */
static void do_render(lv_layer_t *layer, int ox, int oy, int s, int type)
{
    switch (type) {
        case EICON_SOLAR:     draw_solar(layer, ox, oy, s);     break;
        case EICON_GRID:      draw_grid(layer, ox, oy, s);      break;
        case EICON_LOAD:      draw_load(layer, ox, oy, s);      break;
        case EICON_DISCHARGE: draw_discharge(layer, ox, oy, s); break;
        default: break;
    }
}

static void icon_draw_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_obj_t   *obj   = lv_event_get_target(e);
    lv_area_t   coords;
    lv_obj_get_coords(obj, &coords);
    int type = (int)(intptr_t)lv_obj_get_user_data(obj);
    int sz   = lv_obj_get_width(obj);
    do_render(layer, coords.x1, coords.y1, sz, type);
}

lv_obj_t *energy_icon_create(lv_obj_t *parent, int sz, int type)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, sz, sz);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(obj, (void *)(intptr_t)type);
    lv_obj_add_event_cb(obj, icon_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}
