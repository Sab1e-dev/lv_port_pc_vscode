/**
 * @file main.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#ifndef _DEFAULT_SOURCE
  #define _DEFAULT_SOURCE /* needed for usleep() */
#endif

#include <stdlib.h>
#include <stdio.h>
#ifdef _MSC_VER
  #include <Windows.h>
#else
  #include <unistd.h>
  #include <pthread.h>
#endif
#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"
#include <SDL.h>

#include "hal/hal.h"
#include "widgets/lv_watch_bubble.h"

/*********************
 *      DEFINES
 *********************/

#define SOLID_ICON_COUNT 8

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void bubble_icon_click_event_cb(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

static const uint8_t icon_red_map[] = {0xFF, 0x3B, 0x30, 0xFF};
static const uint8_t icon_orange_map[] = {0xFF, 0x95, 0x00, 0xFF};
static const uint8_t icon_yellow_map[] = {0xFF, 0xE6, 0x20, 0xFF};
static const uint8_t icon_green_map[] = {0x04, 0xDE, 0x71, 0xFF};
static const uint8_t icon_cyan_map[] = {0x00, 0xF5, 0xEA, 0xFF};
static const uint8_t icon_blue_map[] = {0x20, 0x94, 0xFA, 0xFF};
static const uint8_t icon_indigo_map[] = {0x78, 0x7A, 0xFF, 0xFF};
static const uint8_t icon_pink_map[] = {0xFA, 0x11, 0x4F, 0xFF};

static const lv_img_dsc_t icon_red = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 1,
  .header.h = 1,
  .header.cf = LV_COLOR_FORMAT_ARGB8888,
  .data_size = 4,
  .data = icon_red_map,
};

static const lv_img_dsc_t icon_orange = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 1,
  .header.h = 1,
  .header.cf = LV_COLOR_FORMAT_ARGB8888,
  .data_size = 4,
  .data = icon_orange_map,
};

static const lv_img_dsc_t icon_yellow = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 1,
  .header.h = 1,
  .header.cf = LV_COLOR_FORMAT_ARGB8888,
  .data_size = 4,
  .data = icon_yellow_map,
};

static const lv_img_dsc_t icon_green = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 1,
  .header.h = 1,
  .header.cf = LV_COLOR_FORMAT_ARGB8888,
  .data_size = 4,
  .data = icon_green_map,
};

static const lv_img_dsc_t icon_cyan = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 1,
  .header.h = 1,
  .header.cf = LV_COLOR_FORMAT_ARGB8888,
  .data_size = 4,
  .data = icon_cyan_map,
};

static const lv_img_dsc_t icon_blue = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 1,
  .header.h = 1,
  .header.cf = LV_COLOR_FORMAT_ARGB8888,
  .data_size = 4,
  .data = icon_blue_map,
};

static const lv_img_dsc_t icon_indigo = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 1,
  .header.h = 1,
  .header.cf = LV_COLOR_FORMAT_ARGB8888,
  .data_size = 4,
  .data = icon_indigo_map,
};

static const lv_img_dsc_t icon_pink = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 1,
  .header.h = 1,
  .header.cf = LV_COLOR_FORMAT_ARGB8888,
  .data_size = 4,
  .data = icon_pink_map,
};

static const lv_img_dsc_t * solid_icons[SOLID_ICON_COUNT] = {
  &icon_red,
  &icon_orange,
  &icon_yellow,
  &icon_green,
  &icon_cyan,
  &icon_blue,
  &icon_indigo,
  &icon_pink,
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

#if LV_USE_OS != LV_OS_FREERTOS

int main(int argc, char **argv)
{
  (void)argc; /*Unused*/
  (void)argv; /*Unused*/

  /*Initialize LVGL*/
  lv_init();

  /*Initialize the HAL (display, input devices, tick) for LVGL*/
  sdl_hal_init(500, 500);

  lv_obj_t *src = lv_screen_active();
  lv_obj_remove_flag(src, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(src, lv_color_black(), 0);

  lv_obj_t *bubble = lv_watch_bubble_create(lv_screen_active());
  lv_obj_add_event_cb(bubble, bubble_icon_click_event_cb, LV_EVENT_CLICKED, NULL);

  for(uint32_t i = 0; i < 32; i++) {
    const lv_img_dsc_t * icon = solid_icons[i % SOLID_ICON_COUNT];
    lv_watch_bubble_set_icon_src(bubble, i, icon);
    lv_watch_bubble_set_icon_user_data(bubble, i, (void *)(uintptr_t)i);
  }

  while(1) {
    /* Periodically call the lv_task handler.
     * It could be done in a timer interrupt or an OS task too.*/
    uint32_t sleep_time_ms = lv_timer_handler();
    if(sleep_time_ms == LV_NO_TIMER_READY){
	sleep_time_ms =  LV_DEF_REFR_PERIOD;
    }
#ifdef _MSC_VER
    Sleep(sleep_time_ms);
#else
    usleep(sleep_time_ms * 1000);
#endif
  }

  return 0;
}


#endif

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void bubble_icon_click_event_cb(lv_event_t * e)
{
  const lv_watch_bubble_click_event_t *event_data = lv_event_get_param(e);
  if(event_data == NULL) return;

  printf("bubble icon clicked: index=%" LV_PRIu32 ", user_data=%u\n",
         event_data->index,
         (unsigned int)(uintptr_t)event_data->icon_user_data);
}
