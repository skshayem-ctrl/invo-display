#pragma once
#include "lvgl.h"

/* Icon type codes */
#define EICON_SOLAR     0   /* sun with 8 rays */
#define EICON_GRID      1   /* AC power plug   */
#define EICON_LOAD      2   /* house silhouette */
#define EICON_DISCHARGE 3   /* battery with bolt */

lv_obj_t *energy_icon_create(lv_obj_t *parent, int sz, int type);
