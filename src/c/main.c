#include <pebble.h>
#include "dieciocho.h"
#include "sonidos.h"
#define MAX(a,b) ((a)>(b)?(a):(b))

//#define DEBUG_SHOW_EIGHTS

// AppMessage keys (coinciden con messageKeys en package.json)
#ifndef MESSAGE_KEY_SHOW_WELCOME
#define MESSAGE_KEY_SHOW_WELCOME         0
#define MESSAGE_KEY_TRANSPARENT_PORTION  1
#define MESSAGE_KEY_BG_COLOR             2
#define MESSAGE_KEY_DIAL_SHAPE           3
#endif

// Persistent storage keys
#define PKEY_SHOW_WELCOME      0
#define PKEY_TRANSPARENT       1
#define PKEY_BG_COLOR          2
#define PKEY_DIAL_SHAPE        3

#define DIAL_SHAPE_HEX         0
#define DIAL_SHAPE_RECT        1

// User settings (defaults)
static int    battery_perc         = 100;
static bool   s_show_welcome       = true;
static bool   s_transparent        = true;
static GColor s_bg_color;
static int    s_dial_shape         = DIAL_SHAPE_HEX;

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
static bool      s_bt_connected   = false;
static GRect     s_eye_touch_rect;
static AppTimer *s_touch_timer    = NULL;
static bool      s_touch_in_eye   = false;
static int s_hoy_x = 0;
static int s_hoy_y = 0;
static int s_hoy_w = 0;
static GDrawCommandImage *s_uno_logo;
static BitmapLayer *s_uno_img_layer;
static BitmapLayer *s_eye_layer;
static BitmapLayer *s_cuarzo_layer;
static Layer       *s_cuarzo_cover_layer;
static int          s_cuarzo_x, s_cuarzo_y, s_cuarzo_w, s_cuarzo_h;
static GBitmap *s_uno_bmp;
static GBitmap *s_eye_bmp;
static GBitmap *s_face_bmp;
static GBitmap *s_cuarzo_bmp;

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
// Bounding rect of WHITE_HEX_RAW: x=[11,190] y=[74,161]
static const GPoint WHITE_RECT_RAW[] = {
    {11,74},{190,74},{190,161},{11,161}
};
static const GPoint HEX_BORDER_RAW[] = {
    {197,176},{175,226},{26,226},{3,176},{3,52},{25,2},{174,2},{197,51}
};

#define N_GOLD   10
#define N_WHITE   6
#define N_RECT    4
#define N_BORDER  8
#define STAR_POINTS 10

static GPoint s_gold_pts[N_GOLD];
static GPoint s_white_pts[N_WHITE];
static GPoint s_rect_pts[N_RECT];
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
    scale_pts(s_rect_pts,   WHITE_RECT_RAW, N_RECT,   w, h);
    scale_pts(s_border_pts, HEX_BORDER_RAW, N_BORDER, w, h);

    // 1. Black background
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, b, 0, GCornerNone);

    // 2. color_face — gray body (color configurable, transparente en emery si aplica)
    GPathInfo gold_info = {N_GOLD, s_gold_pts};
    GPath *gold = gpath_create(&gold_info);
#if defined(PBL_PLATFORM_EMERY)
    GColor body_color = s_transparent ? GColorDarkGray : s_bg_color;
#elif defined(PBL_PLATFORM_BASALT)
    GColor body_color = s_bg_color;
#else
    GColor body_color = GColorDarkGray;
#endif
    graphics_context_set_fill_color(ctx, body_color);
    gpath_draw_filled(ctx, gold);
    gpath_destroy(gold);

    // 3. dial face — hexagonal or rectangular
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 2);
    bool use_rect = (s_dial_shape == DIAL_SHAPE_RECT);
#if defined(PBL_PLATFORM_EMERY)
    if (s_transparent) use_rect = false; // transparent solo disponible en hex
#endif
    if (use_rect) {
        GPathInfo rect_info = {N_RECT, s_rect_pts};
        GPath *rect_path = gpath_create(&rect_info);
        gpath_draw_filled(ctx, rect_path);
        gpath_draw_outline(ctx, rect_path);
        gpath_destroy(rect_path);
    } else {
        GPathInfo white_info = {N_WHITE, s_white_pts};
        GPath *white_hex = gpath_create(&white_info);
        gpath_draw_filled(ctx, white_hex);
        gpath_draw_outline(ctx, white_hex);
        gpath_destroy(white_hex);
    }

    // 4. face_interior (emery + basalt, solo en modo transparente)
#if defined(PBL_PLATFORM_EMERY)
    if (s_face_bmp && s_transparent) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, s_face_bmp,
            GRect((w - 196) / 2 - 2, sy(49, h) - 7, 196, 144));
    }
#elif defined(PBL_PLATFORM_BASALT)
    if (s_face_bmp && s_transparent) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, s_face_bmp,
            GRect((w - 141) / 2, sy(49, h) - 4, 141, 104));
    }
#endif

    // alarm_indicator — visible solo si bluetooth conectado y no en countdown
#ifdef DEBUG_SHOW_EIGHTS
    if (!dieciocho_is_active() && !sonidos_is_scrolling()) {
#else
    if (s_bt_connected && !dieciocho_is_active() && !sonidos_is_scrolling()) {
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
    if (!dieciocho_is_active() && !sonidos_is_scrolling()) {
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
    graphics_context_set_fill_color(ctx, GColorPastelYellow);
    graphics_context_set_stroke_width(ctx, 2);
    gpath_draw_outline(ctx, border);
    gpath_destroy(border);

    // M (mañana) / T (tarde) — solo en modo 12h y fuera de countdown
#ifdef DEBUG_SHOW_EIGHTS
    if (!dieciocho_is_active() && !sonidos_is_scrolling()) {
#else
    if (!clock_is_24h_style() && !dieciocho_is_active() && !sonidos_is_scrolling()) {
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
    if (s_hoy_w > 0 && dieciocho_hoy_visible() && !sonidos_is_scrolling()) {
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

    if (!dieciocho_is_active() && !sonidos_is_scrolling()) {
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

static void tap_handler(AccelAxisType axis, int32_t direction) {
    if (sonidos_is_scrolling()) scroll_stop();
    dieciocho_trigger();
}

static void bt_handler(bool connected) {
    s_bt_connected = connected;
    layer_mark_dirty(s_bg_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();
}


#if defined(PBL_PLATFORM_EMERY)
static void update_cuarzo_cover(void);
#endif

static void battery_handler(BatteryChargeState state) {
    battery_perc = state.charge_percent;
#if defined(PBL_PLATFORM_EMERY)
    if (s_cuarzo_cover_layer) update_cuarzo_cover();
#endif
}

#if defined(PBL_PLATFORM_EMERY)
static void cuarzo_cover_draw(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    if (bounds.size.w <= 1) return; // sin bloque visible, nada que dibujar
    int h = bounds.size.h;

    graphics_context_set_fill_color(ctx, GColorBulgarianRose);

    // Bloque principal (desplazado 1px a la derecha para dejar columna de píxeles)
    graphics_fill_rect(ctx, GRect(1, 0, bounds.size.w - 1, h), 0, GCornerNone);

    // Dos píxeles decorativos en la columna izquierda, separados 14px y centrados
    int margin = (h - 14) / 2;  // (18 - 14) / 2 = 2px desde cada extremo
    graphics_fill_rect(ctx, GRect(0, margin - 1,  1, 1), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(0, margin + 14, 1, 1), 0, GCornerNone);
}

static void update_cuarzo_cover(void) {
    // battery_perc=1  → 10px visibles (mínimo)
    // battery_perc=100 → toda la imagen visible
    int pct = battery_perc < 1 ? 1 : (battery_perc > 100 ? 100 : battery_perc);
    int uncovered = 10 + (pct - 1) * (s_cuarzo_w - 10) / 99;
    int cover_w   = s_cuarzo_w - uncovered;
    // Layer 1px más ancho y 1px más a la izquierda para alojar los píxeles decorativos
    layer_set_frame(s_cuarzo_cover_layer,
        GRect(s_cuarzo_x + uncovered - 1, s_cuarzo_y, cover_w + 1, s_cuarzo_h));
}
#endif

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
    GRect eye_rect = GRect(sx(158, w) - 32, sy(57, h) - 15, 30, 15);
#else
    GRect eye_rect = GRect(sx(158, w) - 32 + 5, sy(57, h) - 15 + 3, 30, 15);
#endif
    s_eye_layer = bitmap_layer_create(eye_rect);
    bitmap_layer_set_bitmap(s_eye_layer, s_eye_bmp);
    bitmap_layer_set_compositing_mode(s_eye_layer, GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_eye_layer));

#if defined(PBL_PLATFORM_EMERY)
    // cuarzo_emery.png: 55x18 px
    // Ajustar: X_CUARZO (posición horizontal), Y_CUARZO (posición vertical)
    s_cuarzo_w = 55; s_cuarzo_h = 18;
    s_cuarzo_x = sx(158, w) - 52;
    s_cuarzo_y = sy(57, h) - 15 - s_cuarzo_h - 8;
    s_cuarzo_bmp   = gbitmap_create_with_resource(RESOURCE_ID_IMG_CUARZO);
    s_cuarzo_layer = bitmap_layer_create(
        GRect(s_cuarzo_x, s_cuarzo_y, s_cuarzo_w, s_cuarzo_h));
    bitmap_layer_set_bitmap(s_cuarzo_layer, s_cuarzo_bmp);
    bitmap_layer_set_compositing_mode(s_cuarzo_layer, GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_cuarzo_layer));

    s_cuarzo_cover_layer = layer_create(
        GRect(s_cuarzo_x, s_cuarzo_y, s_cuarzo_w, s_cuarzo_h));
    layer_set_update_proc(s_cuarzo_cover_layer, cuarzo_cover_draw);
    layer_add_child(window_layer, s_cuarzo_cover_layer);
    update_cuarzo_cover();
#endif
    // Área táctil: 12px de margen alrededor del ojo
    s_eye_touch_rect = GRect(eye_rect.origin.x - 12, eye_rect.origin.y - 12,
                              eye_rect.size.w + 24,  eye_rect.size.h + 24);

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
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_BASALT)
    s_face_bmp = gbitmap_create_with_resource(RESOURCE_ID_IMG_FACE);
#endif

    dieciocho_init(s_hours_layer, s_minutes_layer, s_date_layer, s_colon_layer,
                   s_bg_layer, s_hours_buf, s_minutes_buf, s_date_buf, update_time);
    sonidos_init(s_hours_layer, s_minutes_layer, s_colon_layer, s_date_layer,
                 s_bg_layer, s_hours_font, s_digits_font, update_time);
    sonidos_set_song_number(3);

    sonidos_song_load();
    update_time();
}

static void main_window_unload(Window *window) {
    if (s_touch_timer) { app_timer_cancel(s_touch_timer); s_touch_timer = NULL; }
    sonidos_teardown();
    dieciocho_teardown();
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
#if defined(PBL_PLATFORM_EMERY)
    layer_destroy(s_cuarzo_cover_layer);
    bitmap_layer_destroy(s_cuarzo_layer);
    gbitmap_destroy(s_cuarzo_bmp);
#endif
    gbitmap_destroy(s_uno_bmp);
    gbitmap_destroy(s_eye_bmp);
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_BASALT)
    gbitmap_destroy(s_face_bmp);
#endif
    if (s_uno_logo) {
        gdraw_command_image_destroy(s_uno_logo);
        s_uno_logo = NULL;
    }
    sonidos_song_free();
}

static void inbox_received_cb(DictionaryIterator *iter, void *ctx) {
    Tuple *t;
    bool needs_redraw = false;

    t = dict_find(iter, MESSAGE_KEY_SHOW_WELCOME);
    if (t) {
        s_show_welcome = (bool)t->value->int32;
        persist_write_bool(PKEY_SHOW_WELCOME, s_show_welcome);
    }
    t = dict_find(iter, MESSAGE_KEY_TRANSPARENT_PORTION);
    if (t) {
        s_transparent = (bool)t->value->int32;
        persist_write_bool(PKEY_TRANSPARENT, s_transparent);
        needs_redraw = true;
    }
    t = dict_find(iter, MESSAGE_KEY_BG_COLOR);
    if (t) {
        s_bg_color = GColorFromHEX(t->value->int32);
        persist_write_int(PKEY_BG_COLOR, t->value->int32);
        needs_redraw = true;
    }
    t = dict_find(iter, MESSAGE_KEY_DIAL_SHAPE);
    if (t) {
        s_dial_shape = (int)t->value->int32;
        persist_write_int(PKEY_DIAL_SHAPE, s_dial_shape);
        needs_redraw = true;
    }
    if (needs_redraw) layer_mark_dirty(s_bg_layer);
}

#if defined(PBL_PLATFORM_EMERY)
static void toggle_sonidos_mode(void) {
    vibes_long_pulse();
    if (sonidos_is_scrolling()) {
        scroll_stop();
        sonidos_song_stop();
    } else {
        scroll_start();
        sonidos_song_play();
    }
}

static void long_press_callback(void *context) {
    s_touch_timer = NULL;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "long_press fired, in_eye=%d", (int)s_touch_in_eye);
    if (s_touch_in_eye) toggle_sonidos_mode();
}

static void touch_handler(const TouchEvent *event, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "touch type=%d x=%d y=%d", (int)event->type, (int)event->x, (int)event->y);
    if (event->type == TouchEvent_Touchdown) {
        s_touch_in_eye = (event->x >= s_eye_touch_rect.origin.x &&
                          event->x <  s_eye_touch_rect.origin.x + s_eye_touch_rect.size.w &&
                          event->y >= s_eye_touch_rect.origin.y &&
                          event->y <  s_eye_touch_rect.origin.y + s_eye_touch_rect.size.h);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "touchdown in_eye=%d rect=(%d,%d,%d,%d)",
                (int)s_touch_in_eye,
                s_eye_touch_rect.origin.x, s_eye_touch_rect.origin.y,
                s_eye_touch_rect.size.w,   s_eye_touch_rect.size.h);
        if (s_touch_in_eye) {
            s_touch_timer = app_timer_register(600, long_press_callback, NULL);
        }
    } else if (event->type == TouchEvent_Liftoff) {
        s_touch_in_eye = false;
        if (s_touch_timer) { app_timer_cancel(s_touch_timer); s_touch_timer = NULL; }
    }
}
#endif

static void main_window_appear(Window *window) {
#if defined(PBL_PLATFORM_EMERY)
    bool touch_ok = touch_service_is_enabled();
    APP_LOG(APP_LOG_LEVEL_DEBUG, "touch_service_is_enabled: %d", (int)touch_ok);
    if (touch_ok) {
        touch_service_subscribe(touch_handler, NULL);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "touch subscribed");
    }
#endif
}

static void main_window_disappear(Window *window) {
    if (sonidos_is_scrolling()) scroll_stop();
#if defined(PBL_PLATFORM_EMERY)
    touch_service_unsubscribe();
#endif
}

static void init(void) {
    // Load persisted settings (defaults if first run)
    s_show_welcome = persist_exists(PKEY_SHOW_WELCOME)
        ? persist_read_bool(PKEY_SHOW_WELCOME) : true;
    s_transparent  = persist_exists(PKEY_TRANSPARENT)
        ? persist_read_bool(PKEY_TRANSPARENT)  : true;
    s_bg_color = persist_exists(PKEY_BG_COLOR)
        ? GColorFromHEX(persist_read_int(PKEY_BG_COLOR)) : GColorDarkGray;
    s_dial_shape = persist_exists(PKEY_DIAL_SHAPE)
        ? persist_read_int(PKEY_DIAL_SHAPE) : DIAL_SHAPE_HEX;

    app_message_register_inbox_received(inbox_received_cb);
    app_message_open(256, 64);

    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load      = main_window_load,
        .unload    = main_window_unload,
        .appear    = main_window_appear,
        .disappear = main_window_disappear,
    });
    window_stack_push(s_main_window, true);
    battery_perc = battery_state_service_peek().charge_percent;
    battery_state_service_subscribe(battery_handler);
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    accel_tap_service_subscribe(tap_handler);
    connection_service_subscribe((ConnectionHandlers) {
        .pebble_app_connection_handler = bt_handler
    });
    s_bt_connected = connection_service_peek_pebble_app_connection();
}

static void deinit(void) {
    sonidos_song_stop();
    battery_state_service_unsubscribe();
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
