#include <pebble.h>
#define MAX(a,b) ((a)>(b)?(a):(b))

//#define DEBUG_SHOW_EIGHTS

static Window *s_main_window;
static Layer *s_bg_layer;
static TextLayer *s_hours_layer;
static TextLayer *s_minutes_layer;
static TextLayer *s_date_layer;
static TextLayer *s_colon_layer;
static GFont s_digits_font;
static GFont s_hours_font;
static GFont s_date_font;
static char s_hours_buf[3];
static char s_minutes_buf[3];
static char s_date_buf[3];
static int s_current_wday = -1;
static int s_current_hour = -1;
static bool s_bt_connected = false;
static bool dieciocho_mode = false;
static AppTimer *s_countdown_timer = NULL;
static AppTimer *s_blink_timer = NULL;
static bool s_hoy_blink = false;
static int s_countdown_token = 0;
static AppTimer *s_scroll_timer = NULL;
static int s_scroll_pos = 0;
static bool s_scrolling = false;
static char s_scroll_h[3];
static char s_scroll_m[3];

static const char SCROLL_MSG[] = "DANDO LA HORA - HECHO EN CHILE";
#define SCROLL_MSG_LEN ((int)(sizeof(SCROLL_MSG) - 1))
#define SCROLL_INTERVAL_MS 150
static int s_hoy_x = 0;
static int s_hoy_y = 0;
static int s_hoy_w = 0;
static GDrawCommandImage *s_uno_logo;
static BitmapLayer *s_uno_img_layer;
static BitmapLayer *s_eye_layer;
static GBitmap *s_uno_bmp;
static GBitmap *s_eye_bmp;
static GBitmap *s_face_bmp;

#if defined(PBL_PLATFORM_EMERY)
static const char *DAY_NAMES[] = {"LU","MA","MI","JU","VI","SA","DO"};
#else
static const char *DAY_NAMES[] = {"L","M","X","J","V","S","D"};
#endif

static int wday_to_idx(int wday) {
    return (wday == 0) ? 6 : wday - 1;
}

static const GPoint GOLD_BODY_RAW[] = {
    {10,184},{190,184},{194,175},{194,77},{194,64},{132,64},{111,47},{9,47},{6,52},{6,176}
};
static const GPoint WHITE_HEX_RAW[] = {
    {11,115},{24,74},{177,74},{190,115},{177,161},{24,161}
};
static const GPoint HEX_BORDER_RAW[] = {
    {197,176},{175,226},{26,226},{3,176},{3,52},{25,2},{174,2},{197,51}
};

#define N_GOLD   10
#define N_WHITE   6
#define N_BORDER  8
#define STAR_POINTS 10

static GPoint s_gold_pts[N_GOLD];
static GPoint s_white_pts[N_WHITE];
static GPoint s_border_pts[N_BORDER];
static GPoint s_star_pts[STAR_POINTS];


static void scale_pts(GPoint *dst, const GPoint *src, int n, int w, int h) {
    for (int i = 0; i < n; i++) {
        dst[i] = GPoint(src[i].x * w / 200, src[i].y * h / 228);
    }
}

static int sx(int x, int w) { return x * w / 200; }
static int sy(int y, int h) { return y * h / 228; }

static int prv_div_round(int32_t value, int32_t divisor) {
    if (value >= 0) {
        return (int)((value + divisor / 2) / divisor);
    } else {
        return (int)((value - divisor / 2) / divisor);
    }
}

static void build_star_points(GPoint *pts, int cx, int cy, int outer_r, int inner_r) {
    // 0° = arriba con x=cx+sin*r, y=cy-cos*r
    const int32_t angles_deg[STAR_POINTS] = {
        0, 36, 72, 108, 144, 180, 216, 252, 288, 324
    };
    for (int i = 0; i < STAR_POINTS; i++) {
        int r = (i % 2 == 0) ? outer_r : inner_r;
        int32_t angle = angles_deg[i] * TRIG_MAX_ANGLE / 360;
        int32_t sin_v = sin_lookup(angle);
        int32_t cos_v = cos_lookup(angle);
        pts[i].x = cx + prv_div_round(sin_v * r, TRIG_MAX_RATIO);
        pts[i].y = cy - prv_div_round(cos_v * r, TRIG_MAX_RATIO);
    }
}

static void draw_bottom_star(GContext *ctx, GRect bounds) {
    int w = bounds.size.w;
    int h = bounds.size.h;

    int outer_r = MAX(4, w * 3 / 100);
    int inner_r = outer_r * 45 / 100;
    int cx = w / 2;
#if defined(PBL_PLATFORM_EMERY)
    int cy = h - outer_r - 25;
#else
    int cy = h - outer_r - 22;
#endif

    build_star_points(s_star_pts, cx, cy, outer_r, inner_r);

    GPathInfo star_info = { .num_points = STAR_POINTS, .points = s_star_pts };
    GPath *star = gpath_create(&star_info);
    graphics_context_set_fill_color(ctx, GColorPastelYellow);
    graphics_context_set_stroke_color(ctx, GColorPastelYellow);
    graphics_context_set_stroke_width(ctx, 1);
    gpath_draw_filled(ctx, star);
    gpath_draw_outline(ctx, star);
    gpath_destroy(star);

    GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_09);
    graphics_context_set_text_color(ctx, GColorPastelYellow);
    int text_y = cy + outer_r + 1;
    int text_w = 40;
    int text_x = cx - text_w / 2;
    graphics_draw_text(ctx, "CHILE", small_font,
        GRect(text_x, text_y, text_w, 10),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "DLH", small_font,
        GRect(text_x, text_y + 7, text_w, 10),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}


static void bg_layer_draw(Layer *layer, GContext *ctx) {
    GRect b = layer_get_bounds(layer);
    int w = b.size.w, h = b.size.h;

    scale_pts(s_gold_pts,   GOLD_BODY_RAW,  N_GOLD,   w, h);
    scale_pts(s_white_pts,  WHITE_HEX_RAW,  N_WHITE,  w, h);
    scale_pts(s_border_pts, HEX_BORDER_RAW, N_BORDER, w, h);

    // 1. Black background
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, b, 0, GCornerNone);

    // 2. color_face — gray body
    GPathInfo gold_info = {N_GOLD, s_gold_pts};
    GPath *gold = gpath_create(&gold_info);
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    gpath_draw_filled(ctx, gold);
    gpath_destroy(gold);

    // 3. hex_face — white hexagon (entre color_face y face_interior)
    GPathInfo white_info = {N_WHITE, s_white_pts};
    GPath *white_hex = gpath_create(&white_info);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 2);
    gpath_draw_filled(ctx, white_hex);
    gpath_draw_outline(ctx, white_hex);
    gpath_destroy(white_hex);

    // 4. face_interior (emery only)
#if defined(PBL_PLATFORM_EMERY)
    if (s_face_bmp) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, s_face_bmp,
            GRect((w - 196) / 2 - 2, sy(49, h) - 7, 196, 144));
    }
#endif

    // alarm_indicator — visible solo si bluetooth conectado y no en countdown
#ifdef DEBUG_SHOW_EIGHTS
    if (!dieciocho_mode) {
#else
    if (s_bt_connected && !dieciocho_mode) {
#endif
        int hex_y0_tri = sy(74, h);
        int dz_w       = sx(126, w);
        int dz_x       = (w - dz_w) / 2 - 2;
        int cell_w_t   = dz_w / 7;
        int tri_w      = MAX(4, cell_w_t * 9 / 20);
        int tri_h      = MAX(5, tri_w * 26 / 17);
        int top        = hex_y0_tri + 3;
        int cx         = w / 2 - tri_w / 2 + 20;
        GPoint alarm_tri[3] = {
            GPoint(cx + tri_w,                       top),
            GPoint(cx,                               top + tri_h * 741 / 1000),
            GPoint(cx + tri_w * 893 / 1000,         top + tri_h),
        };
        graphics_context_set_fill_color(ctx, GColorBlack);
        GPathInfo ai = {3, alarm_tri};
        GPath *ap = gpath_create(&ai);
        gpath_draw_filled(ctx, ap);
        gpath_destroy(ap);
    }

    // 5+. Everything else
    draw_bottom_star(ctx, b);

    if (s_uno_logo) {
        GSize logo_sz = gdraw_command_image_get_bounds_size(s_uno_logo);
        int logo_x = (w - logo_sz.w) / 2;
        int logo_y = sy(5, h);
        gdraw_command_image_draw(ctx, s_uno_logo, GPoint(logo_x, logo_y));
    }

    // Day indicator triangle — SVG flecha_dias as-is, tip at bottom-left
    if (!dieciocho_mode) {
        int hex_y1   = sy(151, h);
        int dz_w     = sx(126, w);
        int dz_x     = (w - dz_w) / 2 - 2;
        int cell_w_t = dz_w / 7;
        int tri_w    = MAX(4, cell_w_t * 9 / 20);
        int tri_h    = MAX(5, tri_w * 26 / 17);
        int bot      = hex_y1  + 6;
        graphics_context_set_fill_color(ctx, GColorBlack);
        for (int i = 0; i < 7; i++) {
#ifndef DEBUG_SHOW_EIGHTS
            if (i != s_current_wday) continue;
#endif
            int cx = dz_x + i * cell_w_t + cell_w_t / 2;
            GPoint tri[3] = {
                GPoint(cx,                         bot),
                GPoint(cx + tri_w,                 bot - tri_h * 741 / 1000),
                GPoint(cx + tri_w * 107 / 1000,   bot - tri_h),
            };
            GPathInfo ti = {3, tri};
            GPath *tp = gpath_create(&ti);
            gpath_draw_filled(ctx, tp);
            gpath_destroy(tp);
        }
    }

    GPathInfo border_info = {N_BORDER, s_border_pts};
    GPath *border = gpath_create(&border_info);
    graphics_context_set_stroke_color(ctx, GColorPastelYellow);
    graphics_context_set_stroke_width(ctx, 2);
    gpath_draw_outline(ctx, border);
    gpath_destroy(border);

    // M (mañana) / T (tarde) — solo en modo 12h y fuera de countdown
#ifdef DEBUG_SHOW_EIGHTS
    if (!dieciocho_mode) {
#else
    if (!clock_is_24h_style() && !dieciocho_mode) {
#endif
        GFont small_font2 = fonts_get_system_font(FONT_KEY_GOTHIC_09);
        int hx = sx(35, w) + 2;
        int hy = sy(76, h) + 4;
        graphics_context_set_text_color(ctx, GColorBlack);
#ifdef DEBUG_SHOW_EIGHTS
        graphics_draw_text(ctx, "M", small_font2,
            GRect(hx, hy, 10, 12),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        graphics_draw_text(ctx, "T", small_font2,
            GRect(hx + 8, hy, 10, 12),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
#else
        if (s_current_hour >= 0 && s_current_hour < 12) {
            graphics_draw_text(ctx, "M", small_font2,
                GRect(hx, hy, 10, 12),
                GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        } else {
            graphics_draw_text(ctx, "T", small_font2,
                GRect(hx + 8, hy, 10, 12),
                GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        }
#endif
    }

    // HOY label — above the date display
    GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_09);
    graphics_context_set_text_color(ctx, GColorBlack);
    if (s_hoy_w > 0 && (!dieciocho_mode || s_hoy_blink)) {
        graphics_draw_text(ctx, "HOY",
            small_font,
            GRect(s_hoy_x, s_hoy_y, s_hoy_w, 12),
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }

    // Day names
#if defined(PBL_PLATFORM_EMERY)
    int day_zone_y = sy(163, h) + 3;
#else
    int day_zone_y = sy(163, h) + 2;
#endif
    int day_zone_w = sx(126, w);
    int day_zone_x = (w - day_zone_w) / 2 - 3;
    int cell_w = day_zone_w / 7;
    int label_h = sy(14, h);

    for (int i = 0; i < 7; i++) {
        int lx = day_zone_x + i * cell_w;
        int sq_pad_x = 1;
        GRect sq = GRect(lx + sq_pad_x, day_zone_y, cell_w - sq_pad_x * 2, label_h);
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_rect(ctx, sq, 1, GCornersAll);
        graphics_context_set_text_color(ctx, GColorPastelYellow);
        graphics_draw_text(ctx, DAY_NAMES[i],
            small_font,
            GRect(lx, day_zone_y, cell_w, label_h),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
}


static void update_time(void) {
    time_t temp = time(NULL);
    struct tm *t = localtime(&temp);

#ifdef DEBUG_SHOW_EIGHTS
    strncpy(s_hours_buf,   "88", sizeof(s_hours_buf));
    strncpy(s_minutes_buf, "88", sizeof(s_minutes_buf));
    strncpy(s_date_buf,    "88", sizeof(s_date_buf));
#else
    if (clock_is_24h_style()) {
        strftime(s_hours_buf, sizeof(s_hours_buf), "%H", t);
    } else {
        strftime(s_hours_buf, sizeof(s_hours_buf), "%I", t);
    }
    strftime(s_minutes_buf, sizeof(s_minutes_buf), "%M", t);
    strftime(s_date_buf,    sizeof(s_date_buf),    "%d", t);
#endif

    if (!dieciocho_mode) {
        text_layer_set_text(s_hours_layer,   s_hours_buf);
        text_layer_set_text(s_minutes_layer, s_minutes_buf);
        text_layer_set_text(s_date_layer,    s_date_buf);
    }

    int new_wday = wday_to_idx(t->tm_wday);
    int new_hour = t->tm_hour;
    if (new_wday != s_current_wday || new_hour != s_current_hour) {
        s_current_wday = new_wday;
        s_current_hour = new_hour;
        layer_mark_dirty(s_bg_layer);
    }
}

static int days_to_sept18(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    struct tm target = {0};
    target.tm_year   = t->tm_year;
    target.tm_mon    = 8;   // septiembre (0-indexed)
    target.tm_mday   = 18;
    target.tm_isdst  = -1;
    time_t t_target  = mktime(&target);
    int days = (int)((t_target - now) / 86400);
    if (days < 0) {
        target.tm_year = t->tm_year + 1;
        t_target = mktime(&target);
        days = (int)((t_target - now) / 86400);
    }
    return days;
}

static void blink_timer_callback(void *context) {
    s_hoy_blink = !s_hoy_blink;
    layer_mark_dirty(s_bg_layer);
    if (dieciocho_mode) {
        s_blink_timer = app_timer_register(250, blink_timer_callback, NULL);
    } else {
        s_blink_timer = NULL;
    }
}

static void show_countdown(void) {
    dieciocho_mode = true;
    int days = days_to_sept18();
    int hundreds  = days / 100;
    int remainder = days % 100;
    if (hundreds > 0) {
        snprintf(s_hours_buf,   sizeof(s_hours_buf),   "%d",   hundreds);
    } else {
        s_hours_buf[0] = '\0';
    }
    snprintf(s_minutes_buf, sizeof(s_minutes_buf), "%02d", remainder);
    snprintf(s_date_buf,    sizeof(s_date_buf),    "18");
    layer_set_hidden(text_layer_get_layer(s_colon_layer), true);
    text_layer_set_text(s_hours_layer,   s_hours_buf);
    text_layer_set_text(s_minutes_layer, s_minutes_buf);
    text_layer_set_text(s_date_layer,    s_date_buf);
    // arrancar parpadeo de HOY
    s_hoy_blink = true;
    if (s_blink_timer) app_timer_cancel(s_blink_timer);
    s_blink_timer = app_timer_register(250, blink_timer_callback, NULL);
    layer_mark_dirty(s_bg_layer);
}

static void hide_countdown(void) {
    dieciocho_mode = false;
    s_hoy_blink = false;
    if (s_blink_timer) { app_timer_cancel(s_blink_timer); s_blink_timer = NULL; }
    layer_set_hidden(text_layer_get_layer(s_colon_layer), false);
    update_time();
    layer_mark_dirty(s_bg_layer);
}

static void countdown_timer_callback(void *context) {
    int token = (int)(intptr_t)context;
    if (token != s_countdown_token) return;  // callback obsoleto, ignorar
    s_countdown_timer = NULL;
    hide_countdown();
}

static void scroll_stop(void) {
    s_scrolling = false;
    if (s_scroll_timer) { app_timer_cancel(s_scroll_timer); s_scroll_timer = NULL; }
    text_layer_set_font(s_hours_layer, s_hours_font);
    text_layer_set_font(s_minutes_layer, s_digits_font);
    layer_set_hidden(text_layer_get_layer(s_colon_layer), false);
    update_time();
}

static void scroll_step(void *context) {
    s_scroll_timer = NULL;
    if (!s_scrolling) return;
    if (dieciocho_mode) { scroll_stop(); return; }

    // s_scroll_pos starts at -3: message enters desde la derecha slot a slot
    int j = 0;
    for (int i = 0; i < 2; i++) {
        int p = s_scroll_pos + i;
        if (p >= 0 && p < SCROLL_MSG_LEN) s_scroll_h[j++] = SCROLL_MSG[p];
    }
    s_scroll_h[j] = '\0';

    j = 0;
    for (int i = 2; i < 4; i++) {
        int p = s_scroll_pos + i;
        if (p >= 0 && p < SCROLL_MSG_LEN) s_scroll_m[j++] = SCROLL_MSG[p];
    }
    s_scroll_m[j] = '\0';

    text_layer_set_text(s_hours_layer, s_scroll_h);
    text_layer_set_text(s_minutes_layer, s_scroll_m);

    s_scroll_pos++;
    if (s_scroll_pos >= SCROLL_MSG_LEN) {
        scroll_stop();
        return;
    }
    s_scroll_timer = app_timer_register(SCROLL_INTERVAL_MS, scroll_step, NULL);
}

static void scroll_start(void) {
    if (dieciocho_mode) return;
    if (s_scrolling) scroll_stop();
    s_scrolling = true;
    s_scroll_pos = -3;  // entra 1 char a la vez desde la derecha
    text_layer_set_font(s_hours_layer, s_hours_font);
    text_layer_set_font(s_minutes_layer, s_hours_font);
    layer_set_hidden(text_layer_get_layer(s_colon_layer), true);
    s_scroll_timer = app_timer_register(SCROLL_INTERVAL_MS, scroll_step, NULL);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
    if (s_scrolling) scroll_stop();
    if (s_countdown_timer) {
        app_timer_cancel(s_countdown_timer);
    }
    s_countdown_token++;
    show_countdown();
    s_countdown_timer = app_timer_register(4000, countdown_timer_callback,
                                           (void *)(intptr_t)s_countdown_token);
}

static void bt_handler(bool connected) {
    s_bt_connected = connected;
    layer_mark_dirty(s_bg_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    int w = bounds.size.w, h = bounds.size.h;

    s_uno_logo = gdraw_command_image_create_with_resource(RESOURCE_ID_UNO_LOGO);

    s_bg_layer = layer_create(bounds);
    layer_set_update_proc(s_bg_layer, bg_layer_draw);
    layer_add_child(window_layer, s_bg_layer);

    // UNO image
    s_uno_bmp = gbitmap_create_with_resource(RESOURCE_ID_IMG_UNO);
#if defined(PBL_PLATFORM_EMERY)
    s_uno_img_layer = bitmap_layer_create(GRect((w - 44) / 2 - 43, sy(9, h), 44, 35));
#else
    s_uno_img_layer = bitmap_layer_create(GRect((w - 44) / 2 - 43 + 15, sy(9, h) - 4, 44, 35));
#endif
    bitmap_layer_set_bitmap(s_uno_img_layer, s_uno_bmp);
    bitmap_layer_set_compositing_mode(s_uno_img_layer, GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_uno_img_layer));

    // Eye image 
    s_eye_bmp = gbitmap_create_with_resource(RESOURCE_ID_IMG_EYE);
#if defined(PBL_PLATFORM_EMERY)
    s_eye_layer = bitmap_layer_create(GRect(sx(158, w) - 32, sy(57, h) - 15, 30, 15));
#else
    s_eye_layer = bitmap_layer_create(GRect(sx(158, w) - 32 + 5, sy(57, h) - 15 + 3, 30, 15));
#endif
    bitmap_layer_set_bitmap(s_eye_layer, s_eye_bmp);
    bitmap_layer_set_compositing_mode(s_eye_layer, GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_eye_layer));

#if defined(PBL_PLATFORM_EMERY)
    s_digits_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_50));
    s_hours_font  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_51));
    s_date_font   = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_24));
#else
    s_digits_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_38));
    s_hours_font  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_39));
    s_date_font   = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_18));
#endif

    int hex_x0 = sx(6,   w);
    int hex_x1 = sx(194, w);
    int hex_y0 = sy(70,  h);
    int hex_y1 = sy(151, h);
    int hex_w  = hex_x1 - hex_x0;
    int hex_h  = hex_y1 - hex_y0;

#if defined(PBL_PLATFORM_EMERY)
    int font_h = 51;
    int date_font_h = 24;
    int colon_gap = 12;
    int colon_w   = 12;
#else
    int font_h = 39;
    int date_font_h = 18;
    int colon_gap = 6;
    int colon_w   = 13;
#endif

    int t_y0 = hex_y0 + (hex_h - font_h) / 2 - hex_h * 10 / 100 + 9;
    int t_h  = font_h + 4;

    int date_w    = hex_w * 28 / 100;
    int time_w    = hex_w - date_w - 4;
    int group_w   = (time_w - colon_gap) / 2;
    int x_hours   = hex_x0 + hex_w * 10 / 100;
    int x_colon   = x_hours + group_w;
    int x_minutes = x_colon + colon_gap;
    int x_date    = hex_x0 + time_w - hex_w * 5 / 100 + 9;

    int date_y0 = hex_y0 + (hex_h - date_font_h) / 2 + 9;
    int date_h  = date_font_h + 4;

    s_hours_layer = text_layer_create(GRect(x_hours, t_y0, group_w, t_h));
    text_layer_set_background_color(s_hours_layer, GColorClear);
    text_layer_set_text_color(s_hours_layer, GColorBlack);
    text_layer_set_font(s_hours_layer, s_hours_font);
    text_layer_set_text_alignment(s_hours_layer, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(s_hours_layer));

    int cl_x = x_colon + colon_gap / 2 - colon_w / 2;
    s_colon_layer = text_layer_create(GRect(cl_x, t_y0, colon_w, t_h));
    text_layer_set_background_color(s_colon_layer, GColorClear);
    text_layer_set_text_color(s_colon_layer, GColorBlack);
    text_layer_set_font(s_colon_layer, s_hours_font);
    text_layer_set_text_alignment(s_colon_layer, GTextAlignmentCenter);
    text_layer_set_text(s_colon_layer, ":");
    layer_add_child(window_layer, text_layer_get_layer(s_colon_layer));

    s_minutes_layer = text_layer_create(GRect(x_minutes, t_y0, group_w, t_h));
    text_layer_set_background_color(s_minutes_layer, GColorClear);
    text_layer_set_text_color(s_minutes_layer, GColorBlack);
    text_layer_set_font(s_minutes_layer, s_digits_font);
    text_layer_set_text_alignment(s_minutes_layer, GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(s_minutes_layer));

    s_hoy_x = x_date;
    s_hoy_y = date_y0 - 7;
    s_hoy_w = date_w;

    s_date_layer = text_layer_create(GRect(x_date, date_y0, date_w, date_h));
    text_layer_set_background_color(s_date_layer, GColorClear);
    text_layer_set_text_color(s_date_layer, GColorBlack);
    text_layer_set_font(s_date_layer, s_date_font);
    text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

    // face_interior — on top of everything
    s_face_bmp = gbitmap_create_with_resource(RESOURCE_ID_IMG_FACE);

    update_time();
}

static void main_window_unload(Window *window) {
    if (s_scroll_timer) {
        app_timer_cancel(s_scroll_timer);
        s_scroll_timer = NULL;
        s_scrolling = false;
    }
    if (s_countdown_timer) {
        app_timer_cancel(s_countdown_timer);
        s_countdown_timer = NULL;
    }
    if (s_blink_timer) {
        app_timer_cancel(s_blink_timer);
        s_blink_timer = NULL;
    }
    text_layer_destroy(s_hours_layer);
    text_layer_destroy(s_colon_layer);
    text_layer_destroy(s_minutes_layer);
    text_layer_destroy(s_date_layer);
    fonts_unload_custom_font(s_digits_font);
    fonts_unload_custom_font(s_hours_font);
    fonts_unload_custom_font(s_date_font);
    layer_destroy(s_bg_layer);
    bitmap_layer_destroy(s_uno_img_layer);
    bitmap_layer_destroy(s_eye_layer);
    gbitmap_destroy(s_uno_bmp);
    gbitmap_destroy(s_eye_bmp);
    gbitmap_destroy(s_face_bmp);
    if (s_uno_logo) {
        gdraw_command_image_destroy(s_uno_logo);
        s_uno_logo = NULL;
    }
}

static void main_window_appear(Window *window) {
    scroll_start();
}

static void main_window_disappear(Window *window) {
    if (s_scrolling) scroll_stop();
}

static void init(void) {
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load      = main_window_load,
        .unload    = main_window_unload,
        .appear    = main_window_appear,
        .disappear = main_window_disappear,
    });
    window_stack_push(s_main_window, true);
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    accel_tap_service_subscribe(tap_handler);
    connection_service_subscribe((ConnectionHandlers) {
        .pebble_app_connection_handler = bt_handler
    });
    s_bt_connected = connection_service_peek_pebble_app_connection();
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    accel_tap_service_unsubscribe();
    connection_service_unsubscribe();
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}
