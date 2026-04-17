#include "lvgl.h"
#include "lv_watch_bubble.h"
#include <math.h>

#define ICON_COUNT     64   /* Total icon count; maximum number of bubble slots. */
#define BUBBLE_SIZE    32   /* Base bubble radius used to derive bubble and icon size. */
#define GRID_SPACING   1.2f /* Grid spacing multiplier controlling row/column gap. */
#define ROW_PITCH_X    (BUBBLE_SIZE * 2.0f * GRID_SPACING) /* Horizontal pitch inside a row. */
#define ROW_PITCH_Y    (BUBBLE_SIZE * 1.7f * GRID_SPACING) /* Vertical pitch between rows. */
#define X_DRAG_FACTOR  0.8f /* Horizontal drag sensitivity; larger means more responsive. */
#define X_INERTIA_DAMP 0.72f /* Horizontal inertia damping; smaller stops faster. */
#define MAX_X_OFFSET   80.0f /* Max free horizontal drag range to avoid overlap zones. */
#define MAX_SCALE      1.0f /* Maximum icon scale in the default state. */
#define MIN_SCALE      0.06f /* Minimum icon scale to avoid complete disappearance at edges. */
#define X_RADIUS       120   /* Horizontal radius of the central full-scale region. */
#define Y_RADIUS       90    /* Vertical radius used for edge-based scaling decisions. */
#define CORNER_RADIUS  30    /* Corner fillet radius for smooth edge transitions. */
#define FRINGE_WIDTH   120   /* Transition band width from max to min scale. */
#define SCREEN_W       400   /* Logical view width; matches current canvas width. */
#define SCREEN_H       400   /* Logical view height; matches current canvas height. */
#define Y_OVERSCROLL_MAX 20.0f /* Max compression distance for vertical rubber-band effect. */
#define Y_SPRING_K      0.22f /* Vertical spring strength; larger rebounds faster. */
#define Y_SPRING_DAMP   0.1f /* Vertical spring damping controlling decay during rebound. */
#define X_SPRING_K      0.30f /* Horizontal centering spring strength. */
#define X_SPRING_DAMP   0.72f /* Horizontal centering damping for smoother return. */
#define Y_RATCHET_STEP  (ROW_PITCH_Y * 1.20f) /* Vertical ratchet step; default snaps about one row. */
#define Y_RATCHET_PULL  0.02f /* Mild attraction to nearest slot while dragging. */
#define Y_RATCHET_SNAP  0.04f /* Snap strength to nearest slot after release. */
#define Y_RATCHET_DEAD  (ROW_PITCH_Y * 1.80f) /* Dead zone threshold for click-like ratchet alignment. */
#define TAP_MOVE_TOLERANCE 12 /* Allowed finger jitter for tap detection. */
#define PRESS_SCALE 0.92f /* Scale factor when an icon is pressed. */
#define PRESS_DARKEN_LVL LV_OPA_30 /* Bubble darkening intensity when pressed. */
#define PRESS_IMAGE_DARKEN_LVL LV_OPA_40 /* Image-layer darkening intensity when pressed. */
#define PRESS_NEIGHBOR_PULL 8.0f /* Max pull displacement applied to nearby icons while pressed. */
#define PRESS_NEIGHBOR_RADIUS 140.0f /* Radius of neighboring icons affected by press pull. */
#define PRESS_ANIM_IN_SPEED 0.22f /* Per-frame approach speed for press-in animation. */
#define PRESS_ANIM_OUT_SPEED 0.18f /* Per-frame recovery speed for release/cancel animation. */

typedef struct {
    float q, r;
} icon_t;

static icon_t icons[ICON_COUNT];
static lv_color_t bubble_colors[ICON_COUNT];
static const void * icon_srcs[ICON_COUNT];
static void * icon_user_datas[ICON_COUNT];

static lv_obj_t * bubble_objs[ICON_COUNT];
static lv_obj_t * image_objs[ICON_COUNT];

static float offset_x = 0;
static float offset_y = 0;
static float velocity_x = 0;
static float velocity_y = 0;
static float row_center = 0;
static float default_row_center = 0;
static int pressed_icon_index = -1;
static int press_candidate_index = -1;
static float press_anim_progress = 0.0f;
static float press_anim_target = 0.0f;
static lv_point_t press_start_point;
static bool press_moved = false;
static bool pointer_is_down = false;

/* Keep the variable name for minimal external behavior change */
static lv_obj_t * canvas;

static lv_watch_bubble_icon_click_cb_t icon_click_cb = NULL;
static void * icon_click_user_ctx = NULL;

static bool is_component_obj(lv_obj_t * obj)
{
    return obj != NULL && obj == canvas;
}

static float clampf(float v, float min_v, float max_v)
{
    if(v < min_v) return min_v;
    if(v > max_v) return max_v;
    return v;
}

static float round_to_step(float value, float step)
{
    if(step <= 1e-6f) return value;
    return roundf(value / step) * step;
}

static void update_active_row_center(void)
{
    bool has_active = false;
    float min_row = 0.0f;
    float max_row = 0.0f;

    for(uint32_t i = 0; i < ICON_COUNT; i++) {
        if(icon_srcs[i] == NULL) continue;

        float row = icons[i].q;
        if(!has_active) {
            min_row = row;
            max_row = row;
            has_active = true;
        }
        else {
            if(row < min_row) min_row = row;
            if(row > max_row) max_row = row;
        }
    }

    row_center = has_active ? (min_row + max_row) * 0.5f : default_row_center;
}

static void row_layout_to_pixel(float row, float col, float * x, float * y)
{
    int row_i = (int)row;
    int bubble_count = (row_i % 2 == 0) ? 3 : 4;
    float row_half_span = ((float)bubble_count - 1.0f) * 0.5f;

    *x = (col - row_half_span) * ROW_PITCH_X;
    *y = (row - row_center) * ROW_PITCH_Y;
}

static float calc_distance_to_edge(float x, float y)
{
    float dx = fabsf(x);
    float dy = fabsf(y);
    float distance_from_edge = 0.0f;

    if(dx <= X_RADIUS && dy <= Y_RADIUS) {
        distance_from_edge = 0;
    }
    else if(dx <= X_RADIUS + FRINGE_WIDTH && dy <= Y_RADIUS + FRINGE_WIDTH) {
        if(dx > X_RADIUS - CORNER_RADIUS && dy > Y_RADIUS - CORNER_RADIUS) {
            float dx_to_corner = dx - (X_RADIUS - CORNER_RADIUS);
            float dy_to_corner = dy - (Y_RADIUS - CORNER_RADIUS);
            float dist_to_corner = sqrtf(dx_to_corner * dx_to_corner + dy_to_corner * dy_to_corner);
            distance_from_edge = dist_to_corner - CORNER_RADIUS;
        }
        else {
            distance_from_edge = LV_MAX(dx - X_RADIUS, dy - Y_RADIUS);
        }
    }
    else {
        if(dx > X_RADIUS - CORNER_RADIUS && dy > Y_RADIUS - CORNER_RADIUS) {
            float dx_to_corner = dx - (X_RADIUS - CORNER_RADIUS);
            float dy_to_corner = dy - (Y_RADIUS - CORNER_RADIUS);
            float dist_to_corner = sqrtf(dx_to_corner * dx_to_corner + dy_to_corner * dy_to_corner);
            distance_from_edge = dist_to_corner - CORNER_RADIUS;
        }
        else {
            distance_from_edge = LV_MAX(dx - X_RADIUS, dy - Y_RADIUS);
        }
    }

    return distance_from_edge;
}

static float interpolate(float actual_min, float actual_max, float val, float target_min, float target_max)
{
    if(actual_max - actual_min < 1e-6f) return target_min;
    return ((val - actual_min) / (actual_max - actual_min)) * (target_max - target_min) + target_min;
}

static float calc_scale(float distance_from_edge)
{
    if(distance_from_edge <= 0.0f) {
        return MAX_SCALE;
    }
    else if(distance_from_edge <= FRINGE_WIDTH) {
        return interpolate(0.0f, FRINGE_WIDTH, distance_from_edge, MAX_SCALE, MIN_SCALE);
    }
    else {
        return MIN_SCALE;
    }
}

static void apply_boundary_compaction(float * coord, float radius, float fringe)
{
    float sign = (*coord >= 0.0f) ? 1.0f : -1.0f;
    float abs_coord = fabsf(*coord);

    if(abs_coord <= radius) return;

    float outer = radius + fringe;
    float t = (abs_coord - radius) / LV_MAX(1.0f, outer - radius);
    t = clampf(t, 0.0f, 1.0f);

    float keep_ratio = 1.0f - 0.55f * t;
    float compact_abs_coord = radius + (abs_coord - radius) * keep_ratio;

    *coord = sign * compact_abs_coord;
}

static void apply_compact_translation(float * x, float * y, float distance_from_edge)
{
    const float compact_strength = LV_MIN((BUBBLE_SIZE - BUBBLE_SIZE * MIN_SCALE) / 2.0f,
                                          ROW_PITCH_X * 0.08f);
    const float compact_limit = ROW_PITCH_X * 0.08f;
    const float gravitation = 20.0f;

    float tx = 0.0f;
    float ty = 0.0f;

    if(distance_from_edge > 0.0f && distance_from_edge <= FRINGE_WIDTH) {
        float interpolated = interpolate(0.0f, FRINGE_WIDTH, distance_from_edge, 0.0f, compact_strength);
        tx = interpolated;
        ty = interpolated;
    }
    else if(distance_from_edge > FRINGE_WIDTH) {
        float extra = LV_MAX(0.0f, distance_from_edge - FRINGE_WIDTH - BUBBLE_SIZE / 2.0f) * gravitation / 10.0f;
        tx = compact_strength + extra;
        ty = compact_strength + extra;
    }

    if(tx < 1e-6f && ty < 1e-6f) return;

    float dx = *x;
    float dy = *y;
    bool is_corner = (fabsf(dy) > (Y_RADIUS - CORNER_RADIUS)) &&
                     (fabsf(dx) > (X_RADIUS - CORNER_RADIUS));

    if(is_corner) {
        float corner_dx = fabsf(dx) - X_RADIUS + CORNER_RADIUS;
        float corner_dy = fabsf(dy) - Y_RADIUS + CORNER_RADIUS;
        float theta = atan2f(corner_dy, corner_dx);

        tx *= -copysignf(cosf(theta), dx);
        ty *= -copysignf(sinf(theta), dy);
    }
    else if(fabsf(dx) > X_RADIUS || fabsf(dy) > Y_RADIUS) {
        if(fabsf(dx) > X_RADIUS) {
            tx *= -copysignf(1.0f, dx);
            ty = 0.0f;
        }
        else {
            ty *= -copysignf(1.0f, dy);
            tx = 0.0f;
        }
    }

    tx = clampf(tx, -compact_limit, compact_limit);
    ty = clampf(ty, -compact_limit, compact_limit);

    *x += tx;
    *y += ty;
}

static bool calc_icon_visual(uint32_t index, float * out_x, float * out_y, float * out_scale)
{
    if(index >= ICON_COUNT) return false;

    float x, y;
    row_layout_to_pixel(icons[index].q, icons[index].r, &x, &y);

    x += offset_x;
    y += offset_y;

    float distance_from_edge = calc_distance_to_edge(x, y);
    float scale = calc_scale(distance_from_edge);

    apply_boundary_compaction(&x, X_RADIUS, FRINGE_WIDTH);
    apply_boundary_compaction(&y, Y_RADIUS, FRINGE_WIDTH);
    apply_compact_translation(&x, &y, distance_from_edge);

    if(scale < 0.02f) return false;

    x += SCREEN_W / 2.0f;
    y += SCREEN_H / 2.0f;

    int bubble_r = (int)(BUBBLE_SIZE * scale);
    if(x + bubble_r < 0 || x - bubble_r > SCREEN_W ||
       y + bubble_r < 0 || y - bubble_r > SCREEN_H) {
        return false;
    }

    *out_x = x;
    *out_y = y;
    *out_scale = scale;
    return true;
}

static int hit_test_icon_index(int32_t px, int32_t py)
{
    update_active_row_center();

    if(canvas == NULL) return -1;

    lv_area_t canvas_coords;
    lv_obj_get_coords(canvas, &canvas_coords);
    float local_px = (float)px - (float)canvas_coords.x1;
    float local_py = (float)py - (float)canvas_coords.y1;

    int hit = -1;
    float best_scale = -1.0f;

    for(uint32_t i = 0; i < ICON_COUNT; i++) {
        if(icon_srcs[i] == NULL) continue;

        float x, y, scale;
        if(!calc_icon_visual(i, &x, &y, &scale)) continue;

        float r = BUBBLE_SIZE * scale;
        float dx = local_px - x;
        float dy = local_py - y;
        if(dx * dx + dy * dy <= r * r) {
            if(scale > best_scale) {
                best_scale = scale;
                hit = (int)i;
            }
        }
    }

    return hit;
}

static bool get_offset_y_settle_limits(float * out_min_allowed, float * out_max_allowed)
{
    update_active_row_center();

    bool has_active = false;
    float min_base_y = 0.0f;
    float max_base_y = 0.0f;

    for(uint32_t i = 0; i < ICON_COUNT; i++) {
        if(icon_srcs[i] == NULL) continue;

        float base_x, base_y;
        row_layout_to_pixel(icons[i].q, icons[i].r, &base_x, &base_y);
        LV_UNUSED(base_x);

        if(!has_active) {
            min_base_y = base_y;
            max_base_y = base_y;
            has_active = true;
        }
        else {
            if(base_y < min_base_y) min_base_y = base_y;
            if(base_y > max_base_y) max_base_y = base_y;
        }
    }

    if(!has_active) return false;

    /* Strict settling limits:
     * - lower limit: bottom-most row center aligns to screen center
     * - upper limit: top-most row center aligns to screen center */
    float min_allowed = -max_base_y;
    float max_allowed = -min_base_y;

    if(min_allowed > max_allowed) {
        float mid = (min_allowed + max_allowed) * 0.5f;
        min_allowed = mid;
        max_allowed = mid;
    }

    *out_min_allowed = min_allowed;
    *out_max_allowed = max_allowed;
    return true;
}

static bool get_offset_y_drag_limits(float * out_min_allowed, float * out_max_allowed)
{
    float settle_min, settle_max;
    if(!get_offset_y_settle_limits(&settle_min, &settle_max)) return false;

    float span = settle_max - settle_min;
    float extra = LV_MAX(40.0f, LV_MIN(180.0f, 48.0f + span * 0.35f));

    *out_min_allowed = settle_min - extra;
    *out_max_allowed = settle_max + extra;
    return true;
}

static float clamp_to_settle_limits(float candidate_offset_y)
{
    float min_allowed, max_allowed;
    if(!get_offset_y_settle_limits(&min_allowed, &max_allowed)) return candidate_offset_y;
    return clampf(candidate_offset_y, min_allowed, max_allowed);
}

static float snap_offset_y_to_ratch(float candidate_offset_y)
{
    float snapped = round_to_step(candidate_offset_y, Y_RATCHET_STEP);
    snapped = clamp_to_settle_limits(snapped);
    return snapped;
}

static float apply_drag_resistance_y(float candidate_offset_y)
{
    float min_allowed, max_allowed;
    if(!get_offset_y_drag_limits(&min_allowed, &max_allowed)) {
        return candidate_offset_y;
    }

    if(candidate_offset_y >= min_allowed && candidate_offset_y <= max_allowed) {
        float snapped = snap_offset_y_to_ratch(candidate_offset_y);
        float delta = snapped - candidate_offset_y;
        if(fabsf(delta) <= Y_RATCHET_DEAD) {
            candidate_offset_y += delta * Y_RATCHET_PULL;
        }
        return candidate_offset_y;
    }

    if(candidate_offset_y < min_allowed) {
        float overshoot = min_allowed - candidate_offset_y;
        float compressed = overshoot / (1.0f + overshoot / Y_OVERSCROLL_MAX);
        return min_allowed - compressed;
    }

    {
        float overshoot = candidate_offset_y - max_allowed;
        float compressed = overshoot / (1.0f + overshoot / Y_OVERSCROLL_MAX);
        return max_allowed + compressed;
    }
}

static void refresh_icon_objects(void)
{
    if(canvas == NULL) return;

    update_active_row_center();

    bool has_pressed_center = false;
    float pressed_x = 0.0f;
    float pressed_y = 0.0f;
    if(press_anim_progress > 1e-4f && pressed_icon_index >= 0 && pressed_icon_index < (int)ICON_COUNT && icon_srcs[pressed_icon_index] != NULL) {
        float pressed_scale;
        has_pressed_center = calc_icon_visual((uint32_t)pressed_icon_index, &pressed_x, &pressed_y, &pressed_scale);
    }

    for(uint32_t i = 0; i < ICON_COUNT; i++) {
        lv_obj_t * bubble = bubble_objs[i];
        lv_obj_t * image = image_objs[i];
        if(bubble == NULL || image == NULL) continue;

        if(icon_srcs[i] == NULL) {
            lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        float x, y, scale;
        if(!calc_icon_visual(i, &x, &y, &scale)) {
            lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        if(has_pressed_center && (int)i != pressed_icon_index) {
            float vx = pressed_x - x;
            float vy = pressed_y - y;
            float dist = sqrtf(vx * vx + vy * vy);
            if(dist > 1e-3f && dist < PRESS_NEIGHBOR_RADIUS) {
                float t = 1.0f - dist / PRESS_NEIGHBOR_RADIUS;
                float pull = PRESS_NEIGHBOR_PULL * t * press_anim_progress;
                x += (vx / dist) * pull;
                y += (vy / dist) * pull;
            }
        }

        lv_color_t bubble_color = bubble_colors[i];
        bool is_pressed = ((int)i == pressed_icon_index) && press_anim_progress > 1e-4f;
        if(is_pressed) {
            float press_scale = interpolate(0.0f, 1.0f, press_anim_progress, 1.0f, PRESS_SCALE);
            scale *= press_scale;
            lv_opa_t darken_lvl = (lv_opa_t)(PRESS_DARKEN_LVL * press_anim_progress);
            bubble_color = lv_color_darken(bubble_color, darken_lvl);
        }

        int32_t bubble_r = (int32_t)(BUBBLE_SIZE * scale);
        float y_center_dist = fabsf(y - SCREEN_H * 0.5f);
        if(bubble_r < 1 || (bubble_r <= 1 && y_center_dist > Y_RADIUS)) {
            lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        int32_t diameter = bubble_r * 2;

        lv_obj_clear_flag(bubble, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(bubble, diameter, diameter);
        lv_obj_set_pos(bubble, (int32_t)x - bubble_r, (int32_t)y - bubble_r);
        lv_obj_set_style_bg_color(bubble, bubble_color, 0);

        lv_obj_set_size(image, diameter, diameter);
        lv_obj_center(image);
        lv_obj_set_style_image_recolor(image, lv_color_black(), 0);
        lv_opa_t image_darken_opa = is_pressed ? (lv_opa_t)(PRESS_IMAGE_DARKEN_LVL * press_anim_progress) : LV_OPA_TRANSP;
        lv_obj_set_style_image_recolor_opa(image, image_darken_opa, 0);
    }
}

static void init_icons(void)
{
    int index = 0;
    int row = 0;

    lv_color_t colors[] = {
        lv_color_hex(0xFF3B30),
        lv_color_hex(0xFF9500),
        lv_color_hex(0xFFE620),
        lv_color_hex(0x04DE71),
        lv_color_hex(0x00F5EA),
        lv_color_hex(0x2094FA),
        lv_color_hex(0x787AFF),
        lv_color_hex(0xFA114F),
    };
    int color_count = (int)(sizeof(colors) / sizeof(colors[0]));

    while(index < ICON_COUNT) {
        int bubble_count = (row % 2 == 0) ? 3 : 4;
        for(int col = 0; col < bubble_count && index < ICON_COUNT; col++) {
            icons[index].q = (float)row;
            icons[index].r = (float)col;
            bubble_colors[index] = colors[index % color_count];
            icon_srcs[index] = NULL;
            icon_user_datas[index] = NULL;
            bubble_objs[index] = NULL;
            image_objs[index] = NULL;
            index++;
        }
        row++;
    }

    row_center = ((float)row - 1.0f) * 0.5f;
    default_row_center = row_center;
}

static void create_icon_objects(void)
{
    for(uint32_t i = 0; i < ICON_COUNT; i++) {
        lv_obj_t * bubble = lv_obj_create(canvas);
        bubble_objs[i] = bubble;

        lv_obj_set_size(bubble, BUBBLE_SIZE * 2, BUBBLE_SIZE * 2);
        lv_obj_set_style_radius(bubble, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(bubble, bubble_colors[i], 0);
        lv_obj_set_style_border_width(bubble, 0, 0);
        lv_obj_set_style_pad_all(bubble, 0, 0);
        lv_obj_set_style_clip_corner(bubble, true, 0);
        lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(bubble, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t * img = lv_image_create(bubble);
        image_objs[i] = img;

        lv_obj_set_size(img, BUBBLE_SIZE * 2, BUBBLE_SIZE * 2);
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_COVER);
        lv_obj_center(img);
        lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(img, 0, 0);
        lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void pressed_event(lv_event_t * e)
{
    lv_indev_t * indev = lv_indev_get_act();
    if(!indev) return;

    lv_indev_get_point(indev, &press_start_point);
    pointer_is_down = true;
    press_moved = false;
    velocity_x = 0.0f;
    velocity_y = 0.0f;
    press_candidate_index = hit_test_icon_index(press_start_point.x, press_start_point.y);
    pressed_icon_index = press_candidate_index;
    press_anim_target = (pressed_icon_index >= 0) ? 1.0f : 0.0f;
    refresh_icon_objects();

    LV_UNUSED(e);
}

static void drag_event(lv_event_t * e)
{
    lv_indev_t * indev = lv_indev_get_act();
    if(!indev) return;

    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    if(!press_moved) {
        int32_t moved_x = LV_ABS(point.x - press_start_point.x);
        int32_t moved_y = LV_ABS(point.y - press_start_point.y);
        if(moved_x > TAP_MOVE_TOLERANCE || moved_y > TAP_MOVE_TOLERANCE) {
            press_moved = true;
            /* Cancel pressed state once drag exceeds tap threshold. */
            press_anim_target = 0.0f;
        }
    }

    offset_x += vect.x * X_DRAG_FACTOR;
    {
        float candidate_y = offset_y + vect.y;
        offset_y = apply_drag_resistance_y(candidate_y);
    }
    offset_x = clampf(offset_x, -MAX_X_OFFSET, MAX_X_OFFSET);

    velocity_x = vect.x * X_DRAG_FACTOR;
    velocity_y = vect.y;

    refresh_icon_objects();
    LV_UNUSED(e);
}

static void released_event(lv_event_t * e)
{
    lv_indev_t * indev = lv_indev_get_act();
    pointer_is_down = false;
    if(!indev) {
        pressed_icon_index = -1;
        press_candidate_index = -1;
        refresh_icon_objects();
        return;
    }

    lv_point_t release_point;
    lv_indev_get_point(indev, &release_point);

    int32_t moved_x = LV_ABS(release_point.x - press_start_point.x);
    int32_t moved_y = LV_ABS(release_point.y - press_start_point.y);
    bool is_click = !press_moved && moved_x <= TAP_MOVE_TOLERANCE && moved_y <= TAP_MOVE_TOLERANCE;

    int release_index = hit_test_icon_index(release_point.x, release_point.y);
    if(is_click && release_index >= 0 && release_index == press_candidate_index && icon_click_cb) {
        icon_click_cb((uint32_t)release_index, icon_user_datas[release_index], icon_click_user_ctx);
    }

    press_anim_target = 0.0f;
    press_candidate_index = -1;
    refresh_icon_objects();

    LV_UNUSED(e);
}

static void inertia_timer(lv_timer_t * t)
{
    LV_UNUSED(t);

    float anim_speed = (press_anim_progress < press_anim_target) ? PRESS_ANIM_IN_SPEED : PRESS_ANIM_OUT_SPEED;
    press_anim_progress += (press_anim_target - press_anim_progress) * anim_speed;
    if(fabsf(press_anim_target - press_anim_progress) < 0.001f) {
        press_anim_progress = press_anim_target;
    }

    if(!pointer_is_down && press_anim_progress <= 0.001f && press_anim_target <= 0.001f) {
        press_anim_progress = 0.0f;
        if(pressed_icon_index != -1) {
            pressed_icon_index = -1;
        }
    }

    if(pointer_is_down) {
        refresh_icon_objects();
        return;
    }

    velocity_x *= X_INERTIA_DAMP;
    velocity_y *= 0.92f;

    offset_x += velocity_x;
    offset_y += velocity_y;

    /* Stronger horizontal spring-back to center while still respecting max drag range */
    velocity_x = velocity_x * X_SPRING_DAMP + (-offset_x) * X_SPRING_K;
    offset_x = clampf(offset_x, -MAX_X_OFFSET, MAX_X_OFFSET);

    float min_allowed, max_allowed;
    if(get_offset_y_settle_limits(&min_allowed, &max_allowed)) {
        float snap_target = snap_offset_y_to_ratch(offset_y);
        float correction = 0.0f;
        if(offset_y < min_allowed) {
            correction = min_allowed - offset_y;
        }
        else if(offset_y > max_allowed) {
            correction = max_allowed - offset_y;
        }
        else {
            correction = snap_target - offset_y;
        }

        if(fabsf(correction) > 0.0f) {
            velocity_y = velocity_y * Y_SPRING_DAMP + correction * ((fabsf(correction) > Y_RATCHET_DEAD) ? Y_SPRING_K : Y_RATCHET_SNAP);
            offset_y += correction * ((fabsf(correction) > Y_RATCHET_DEAD) ? (Y_SPRING_K * 0.75f) : Y_RATCHET_SNAP);

            if(fabsf(correction) < 0.6f && fabsf(velocity_y) < 0.35f) {
                offset_y = (offset_y < min_allowed) ? min_allowed : (offset_y > max_allowed ? max_allowed : snap_target);
                velocity_y = 0.0f;
            }
        }
        else if(fabsf(velocity_y) < 0.05f) {
            velocity_y = 0.0f;
        }
    }

    if(fabsf(offset_x) < 0.02f) {
        offset_x = 0.0f;
    }

    if(fabsf(velocity_x) < 0.05f && fabsf(velocity_y) < 0.05f) {
        velocity_x = 0.0f;
        velocity_y = 0.0f;
    }

    refresh_icon_objects();
}

lv_obj_t * lv_watch_bubble_create(lv_obj_t * parent)
{
    canvas = lv_obj_create(parent);
    lv_obj_set_size(canvas, SCREEN_W, SCREEN_H);
    lv_obj_center(canvas);

    lv_obj_set_style_radius(canvas, 0, 0);
    lv_obj_set_style_bg_color(canvas, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(canvas, 0, 0);
    lv_obj_set_style_pad_all(canvas, 0, 0);

    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);

    init_icons();
    create_icon_objects();
    refresh_icon_objects();

    lv_obj_add_event_cb(canvas, pressed_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(canvas, drag_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(canvas, released_event, LV_EVENT_RELEASED, NULL);

    lv_timer_create(inertia_timer, 16, NULL);

    return canvas;
}

void lv_watch_bubble_set_icon_src(lv_obj_t * obj, uint32_t index, const void * src)
{
    if(!is_component_obj(obj)) return;
    if(index >= ICON_COUNT) return;

    icon_srcs[index] = src;
    if(image_objs[index] && src) {
        lv_image_set_src(image_objs[index], src);
    }

    {
        float min_allowed, max_allowed;
        if(get_offset_y_settle_limits(&min_allowed, &max_allowed)) {
            offset_y = clampf(offset_y, min_allowed, max_allowed);
        }
    }

    refresh_icon_objects();
}

void lv_watch_bubble_set_icon_user_data(lv_obj_t * obj, uint32_t index, void * user_data)
{
    if(!is_component_obj(obj)) return;
    if(index >= ICON_COUNT) return;
    icon_user_datas[index] = user_data;
}

void lv_watch_bubble_set_icon_click_cb(lv_obj_t * obj, lv_watch_bubble_icon_click_cb_t cb, void * user_ctx)
{
    if(!is_component_obj(obj)) return;
    icon_click_cb = cb;
    icon_click_user_ctx = user_ctx;
}
