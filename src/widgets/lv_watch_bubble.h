/**
 * @file lv_watch_bubble.h
 * @brief LVGL watch bubble component public API
 */

#ifndef LV_WATCH_BUBBLE_H
#define LV_WATCH_BUBBLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef void (*lv_watch_bubble_icon_click_cb_t)(uint32_t index, void * icon_user_data, void * user_ctx);

/**
 * Create a watch bubble component.
 * Returns the component root object.
 */
lv_obj_t * lv_watch_bubble_create(lv_obj_t * parent);

/**
 * Set icon source for one bubble index.
 */
void lv_watch_bubble_set_icon_src(lv_obj_t * obj, uint32_t index, const void * src);

/**
 * Set user data for one bubble index.
 */
void lv_watch_bubble_set_icon_user_data(lv_obj_t * obj, uint32_t index, void * user_data);

/**
 * Set click callback for bubble icons.
 */
void lv_watch_bubble_set_icon_click_cb(lv_obj_t * obj, lv_watch_bubble_icon_click_cb_t cb, void * user_ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_WATCH_BUBBLE_H */
