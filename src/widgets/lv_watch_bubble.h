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

typedef struct {
	uint32_t index;
	void * icon_user_data;
} lv_watch_bubble_click_event_t;

typedef struct {
	uint16_t x_drag_factor_permille;  /* Horizontal drag sensitivity. 1000 = 1.0x finger delta. */
	uint16_t x_inertia_damp_permille; /* Horizontal inertia damping per tick. Smaller stops faster. */
	int16_t max_x_offset_px;          /* Maximum horizontal offset in pixels before clamping. */
	uint16_t max_scale_permille;      /* Maximum icon scale in permille. 1000 = 1.0x. */
	uint16_t min_scale_permille;      /* Minimum icon scale in permille for edge regions. */
	int16_t fringe_width_px;          /* Edge transition width in pixels from max to min scale. */
	int16_t y_overscroll_max_px;      /* Vertical rubber-band compression limit in pixels. */
	uint16_t y_spring_k_permille;     /* Vertical spring strength when settling. */
	uint16_t y_spring_damp_permille;  /* Vertical spring damping per tick. */
	uint16_t x_spring_k_permille;     /* Horizontal spring strength back to center. */
	uint16_t x_spring_damp_permille;  /* Horizontal spring damping per tick. */
	int16_t y_ratchet_step_px;        /* Vertical snap step in pixels. */
	uint16_t y_ratchet_pull_permille; /* Snap attraction while dragging within valid range. */
	uint16_t y_ratchet_snap_permille; /* Snap attraction after release near target step. */
	int16_t y_ratchet_dead_px;        /* Dead-zone threshold in pixels for spring-vs-snap mode. */
} lv_watch_bubble_config_t;

/**
 * Fill config with built-in default values.
 */
void lv_watch_bubble_init_config(lv_watch_bubble_config_t * config);

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
 * Dynamically update drag/scale/ratchet physics config.
 */
void lv_watch_bubble_set_config(lv_obj_t * obj, const lv_watch_bubble_config_t * config);

/**
 * Listen with LV_EVENT_CLICKED on the bubble object.
 * Event param type: lv_watch_bubble_click_event_t *
 */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_WATCH_BUBBLE_H */
