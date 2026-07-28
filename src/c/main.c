#include <pebble.h>
#include <string.h>
#include "dieciocho.h"
#include "sonidos.h"
#define MAX(a,b) ((a)>(b)?(a):(b))

//#define DEBUG_SHOW_EIGHTS

#ifndef MESSAGE_KEY_WELCOME_ACTION
#define MESSAGE_KEY_WELCOME_ACTION       0
#define MESSAGE_KEY_TRANSPARENT_PORTION  1
#define MESSAGE_KEY_BG_COLOR             2
#define MESSAGE_KEY_DIAL_SHAPE           3
#define MESSAGE_KEY_SHAKE_ACTION         4
#define MESSAGE_KEY_SHOW_SECONDS         5
#define MESSAGE_KEY_DIEC18_DURATION      6
#define MESSAGE_KEY_DIEC18_TARGET_MONTH  7
#define MESSAGE_KEY_DIEC18_TARGET_DAY    8
#endif

#define PKEY_WELCOME_ACTION      0
#define PKEY_TRANSPARENT         1
#define PKEY_BG_COLOR            2
#define PKEY_DIAL_SHAPE          3
#define PKEY_SHAKE_ACTION        4
#define PKEY_SHOW_SECONDS        5
#define PKEY_BATTERY_PERC        6
#define PKEY_DIEC18_DURATION     7
#define PKEY_DIEC18_TARGET_MONTH 8
#define PKEY_DIEC18_TARGET_DAY   9

#define DIAL_SHAPE_HEX         0
#define DIAL_SHAPE_RECT        1

#define SHAKE_NADA    0
#define SHAKE_DIEC18  1
#define SHAKE_SONIDO  2

static int    battery_perc      = 100;
static int    s_welcome_action       = SHAKE_NADA;
static int    s_shake_action         = SHAKE_NADA;
static bool   s_show_seconds         = false;
static int    s_diec18_duration      = 4;
static int    s_diec18_target_month  = 9;   // 1-indexado; 9 = septiembre
static int    s_diec18_target_day    = 18;  // día del mes
static bool   s_transparent  = true;

static GColor s_bg_color;
static int    s_dial_shape   = DIAL_SHAPE_HEX;

static Window    *s_main_window;
static Layer     *s_bg_layer;
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
static int  s_current_wday = -1;
static int  s_current_hour = -1;
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
static GBitmap *s_bg_bmp;
static GBitmap *s_cuarzo_bmp;
#if defined(PBL_PLATFORM_BASALT)

#endif

typedef struct {
    const char prefix[6];
    const char *days[7];  // Do Lu Ma Mi Ju Vi Sa (wday 0=Dom .. 6=Sab)
} LocaleDays;

static const LocaleDays s_locale_days[] = {
    { "es", { "DO", "LU", "MA", "MI", "JU", "VI", "SA" } },  // Español (fallback)
    { "en", { "SU", "MO", "TU", "WE", "TH", "FR", "SA" } },  // English
    { "pt", { "DO", "SE", "TE", "QA", "QI", "SX", "SA" } },  // Português
    { "it", { "DO", "LU", "MA", "ME", "GI", "VE", "SA" } },  // Italiano
    { "de", { "SO", "MO", "DI", "MI", "DO", "FR", "SA" } },  // Deutsch
    { "fr", { "DI", "LU", "MA", "ME", "JE", "VE", "SA" } },  // Français
    { "ca", { "DG", "DL", "DT", "DC", "DJ", "DV", "DS" } },  // Català
};
#define NUM_LOCALES ((int)(sizeof(s_locale_days) / sizeof(s_locale_days[0])))

static const char *s_hoy_labels[] = {
    "HOY",  // es
    "TDY",  // en
    "HOJ",  // pt
    "OGG",  // it
    "HTE",  // de
    "AJD",  // fr
    "AVU",  // ca
};

static const char *get_hoy_label(void) {
    const char *locale = i18n_get_system_locale();
    for (int i = 0; i < NUM_LOCALES; i++) {
        const char *p = s_locale_days[i].prefix;
        if (strncmp(locale, p, strlen(p)) == 0) return s_hoy_labels[i];
    }
    return s_hoy_labels[0]; // fallback: español
}

// wday: 0=Sunday..6=Saturday (tm_wday order)
static const char *get_day_abbrev(int wday) {
    const char *locale = i18n_get_system_locale();
    for (int i = 0; i < NUM_LOCALES; i++) {
        const char *p = s_locale_days[i].prefix;
        size_t plen = strlen(p);
        if (strncmp(locale, p, plen) == 0) {
            return s_locale_days[i].days[wday];
        }
    }
    return s_locale_days[0].days[wday]; // fallback: español
}

static int wday_to_idx(int wday) {
    return (wday == 0) ? 6 : wday - 1;
}

static const GPoint GOLD_BODY_RAW[] = {
    {10,184},{190,184},{194,175},{194,77},{194,64},{132,64},{111,47},{9,47},{6,52},{6,176}
};
static const GPoint WHITE_HEX_RAW[] = {
    {11,115},{24,74},{177,74},{190,115},{177,161},{24,161}
};
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
#define N_HEX     6
#define STAR_POINTS 10

static GPoint s_gold_pts[N_GOLD];
static GPoint s_white_pts[N_WHITE];
static GPoint s_rect_pts[N_RECT];
static GPoint s_border_pts[N_BORDER];
static GPoint s_star_pts[STAR_POINTS];
#if defined(PBL_PLATFORM_GABBRO)
static GPoint s_hex_pts[N_HEX];
#endif

static void scale_pts(GPoint *dst, const GPoint *src, int n, int w, int h) {
    for (int i = 0; i < n; i++) {
        dst[i] = GPoint(src[i].x * w / 200, src[i].y * h / 228);
    }
}

static void shift_pts(GPoint *pts, int n, int dx, int dy) {
    for (int i = 0; i < n; i++) { pts[i].x += dx; pts[i].y += dy; }
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

#if defined(PBL_PLATFORM_GABBRO)
static void build_regular_hexagon(GPoint *pts, int cx, int cy, int r) {
    for (int k = 0; k < N_HEX; k++) {
        int32_t angle = TRIG_MAX_ANGLE / 12 + k * (TRIG_MAX_ANGLE / 6);
        pts[k].x = cx + prv_div_round(sin_lookup(angle) * r, TRIG_MAX_RATIO);
        pts[k].y = cy - prv_div_round(cos_lookup(angle) * r, TRIG_MAX_RATIO);
    }
}
#endif

static void build_star_points(GPoint *pts, int cx, int cy, int outer_r, int inner_r) {
    // ángulo 0° apunta arriba: x = cx + sin*r, y = cy - cos*r
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
#if defined(PBL_PLATFORM_GABBRO)
    int cy = h - outer_r - 43;
#elif defined(PBL_PLATFORM_EMERY)
    int cy = h - outer_r - 25;
#elif defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    int cy = h - outer_r - 14;
#endif

    build_star_points(s_star_pts, cx, cy, outer_r, inner_r);

    GPathInfo star_info = { .num_points = STAR_POINTS, .points = s_star_pts };
    GPath *star = gpath_create(&star_info);
#if defined(PBL_PLATFORM_APLITE)
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);
#else
    graphics_context_set_fill_color(ctx, GColorPastelYellow);
    graphics_context_set_stroke_color(ctx, GColorPastelYellow);
#endif
    graphics_context_set_stroke_width(ctx, 1);
    gpath_draw_filled(ctx, star);
    gpath_draw_outline(ctx, star);
    gpath_destroy(star);

    GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_09);
#if defined(PBL_PLATFORM_APLITE)
    graphics_context_set_text_color(ctx, GColorWhite);
#else
    graphics_context_set_text_color(ctx, GColorPastelYellow);
#endif
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    int text_y = cy + outer_r - 2;
#else
    int text_y = cy + outer_r - 1;
#endif
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
#if defined(PBL_PLATFORM_GABBRO)
    int lw = 200, lh = 228, lx = (w - 200) / 2, ly = (h - 228) / 2;
#else
    int lw = w, lh = h, lx = 0, ly = 0;
#endif
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    int by_off = 4;
#else
    int by_off = 0;
#endif

    scale_pts(s_gold_pts,   GOLD_BODY_RAW,  N_GOLD,   lw, lh);
    scale_pts(s_white_pts,  WHITE_HEX_RAW,  N_WHITE,  lw, lh);
    scale_pts(s_rect_pts,   WHITE_RECT_RAW, N_RECT,   lw, lh);
    scale_pts(s_border_pts, HEX_BORDER_RAW, N_BORDER, lw, lh);
    if (lx || ly) {
        shift_pts(s_gold_pts,   N_GOLD,   lx, ly);
        shift_pts(s_white_pts,  N_WHITE,  lx, ly);
        shift_pts(s_rect_pts,   N_RECT,   lx, ly);
        shift_pts(s_border_pts, N_BORDER, lx, ly);
    }

    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, b, 0, GCornerNone);

#if defined(PBL_PLATFORM_GABBRO)
    if (s_bg_bmp) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, s_bg_bmp, GRect(0, 0, w, h));
    }
#else
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

    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 2);
    bool use_rect = (s_dial_shape == DIAL_SHAPE_RECT);
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
#endif

#if defined(PBL_PLATFORM_EMERY)
    if (s_bg_bmp && s_transparent) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, s_bg_bmp, GRect(0, 0, w, h));
    }
#elif defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    if (s_bg_bmp && s_transparent) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, s_bg_bmp, GRect(0, by_off, w, h));
    }
#endif

    // indicador BT — triángulo visible cuando hay conexión
#ifdef DEBUG_SHOW_EIGHTS
    if (!dieciocho_is_active() && !sonidos_is_scrolling()) {
#else
    if (s_bt_connected && !dieciocho_is_active() && !sonidos_is_scrolling()) {
#endif
        int hex_y0_tri = ly + sy(74, lh) + by_off;
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
        int tri_w      = 8;
        int tri_h      = 12;
#else
        int dz_w       = sx(126, lw);
        int dz_x       = lx + (lw - dz_w) / 2 - 2;
        int cell_w_t   = dz_w / 7;
        int tri_w      = MAX(4, cell_w_t * 9 / 20);
        int tri_h      = MAX(5, tri_w * 26 / 17);
#endif
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
        int top        = hex_y0_tri + 3 - 3;
        int cx         = lx + lw / 2 - tri_w / 2 + 20 - 4;
#else
        int top        = hex_y0_tri + 3;
        int cx         = lx + lw / 2 - tri_w / 2 + 20;
#endif
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

    draw_bottom_star(ctx, b);

    if (s_uno_logo) {
        GSize logo_sz = gdraw_command_image_get_bounds_size(s_uno_logo);
        int logo_x = lx + (lw - logo_sz.w) / 2;
        int logo_y = ly + sy(5, lh);
        gdraw_command_image_draw(ctx, s_uno_logo, GPoint(logo_x, logo_y));
    }

    // triángulo indicador del día actual
    if (!dieciocho_is_active() && !sonidos_is_scrolling()) {
        int hex_y1   = ly + sy(151, lh) + by_off;
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
        int cell_w_t = 18;
        int dz_x     = 7;
#else
        int dz_w     = sx(126, lw);
        int dz_x     = lx + (lw - dz_w) / 2 - 2;
        int cell_w_t = dz_w / 7;
#endif
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

    // indicador M/T solo en formato 12h
#ifdef DEBUG_SHOW_EIGHTS
    if (!dieciocho_is_active() && !sonidos_is_scrolling()) {
#else
    if (!clock_is_24h_style() && !dieciocho_is_active() && !sonidos_is_scrolling()) {
#endif
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
        GFont small_font2 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
        int hx = lx + sx(35, lw) + 2 - 13;
#else
        GFont small_font2 = fonts_get_system_font(FONT_KEY_GOTHIC_09);
        int hx = lx + sx(35, lw) + 2;
#endif
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
        int hy = ly + sy(76, lh) + 4 + by_off - 8;
#else
        int hy = ly + sy(76, lh) + 4 + by_off;
#endif
        graphics_context_set_text_color(ctx, GColorBlack);
        {
            bool is_morning = (s_current_hour >= 0 && s_current_hour < 12);
            bool is_es = (strncmp(i18n_get_system_locale(), "es", 2) == 0);
#ifdef DEBUG_SHOW_EIGHTS
            is_morning = true; // fuerza la rama de mañana para ver ambas letras
            // muestra M y T simultáneamente en debug
            graphics_draw_text(ctx, is_es ? "M" : "AM", small_font2,
                GRect(hx, hy, 24, 16),
                GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
            graphics_draw_text(ctx, is_es ? "T" : "PM", small_font2,
                GRect(hx + (is_es ? 10 : 0), hy, 24, 16),
                GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
#else
            if (is_es) {
                // Español: M (mañana) o T (tarde), posiciones escalonadas
                graphics_draw_text(ctx, is_morning ? "M" : "T", small_font2,
                    GRect(hx + (is_morning ? 0 : 10), hy, 12, 16),
                    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
            } else {
                // Otros idiomas: AM / PM
                graphics_draw_text(ctx, is_morning ? "AM" : "PM", small_font2,
                    GRect(hx, hy, 24, 16),
                    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
            }
#endif
        }
    }

    GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_09);
    graphics_context_set_text_color(ctx, GColorBlack);
    if (s_hoy_w > 0 && dieciocho_hoy_visible() && !sonidos_is_scrolling() && (!s_show_seconds || dieciocho_is_active())) {
        graphics_draw_text(ctx, get_hoy_label(),
            small_font,
            GRect(s_hoy_x, s_hoy_y, s_hoy_w, 12),
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }

#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    int day_zone_y = sy(163, h) + 9;
    int day_zone_w = 126;
    int day_zone_x = (w - day_zone_w) / 2 - 3;
    int cell_w     = day_zone_w / 7;
    int label_h    = 14;
#else
    int day_zone_y = ly + sy(163, lh) + 3;
    int day_zone_w = sx(126, lw);
    int day_zone_x = lx + (lw - day_zone_w) / 2 - 3;
    int cell_w = day_zone_w / 7;
    int label_h = sy(14, lh);
#endif

    for (int i = 0; i < 7; i++) {
        int cell_x = day_zone_x + i * cell_w;
        int sq_pad_x = 1;
        GRect sq = GRect(cell_x + sq_pad_x, day_zone_y, cell_w - sq_pad_x * 2, label_h);
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_rect(ctx, sq, 1, GCornersAll);
#if defined(PBL_PLATFORM_APLITE)
        graphics_context_set_text_color(ctx, GColorWhite);
#else
        graphics_context_set_text_color(ctx, GColorPastelYellow);
#endif
        graphics_draw_text(ctx, get_day_abbrev((i + 1) % 7),
            small_font,
            GRect(cell_x, day_zone_y, cell_w, label_h),
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
    if (s_show_seconds) {
        strftime(s_date_buf, sizeof(s_date_buf), "%S", t);
    } else {
        strftime(s_date_buf, sizeof(s_date_buf), "%d", t);
    }
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
    if (s_shake_action == SHAKE_DIEC18) {
        if (sonidos_is_scrolling()) { scroll_stop(); sonidos_song_stop(); }
        dieciocho_trigger();
    } else if (s_shake_action == SHAKE_SONIDO) {
        if (dieciocho_is_active()) dieciocho_teardown();
        if (sonidos_is_scrolling()) {
            scroll_stop();
            sonidos_song_stop();
        } else {
            scroll_start();
            sonidos_song_play();
        }
    }
}

static void bt_handler(bool connected) {
    if (!connected && s_bt_connected) {
        vibes_long_pulse();
    }
    s_bt_connected = connected;
    layer_mark_dirty(s_bg_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    if (dieciocho_is_active() || sonidos_is_scrolling()) return;
    update_time();
}

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE) || defined(PBL_PLATFORM_GABBRO)
static void update_cuarzo_cover(void);
#endif

static void battery_handler(BatteryChargeState state) {
    battery_perc = state.charge_percent;
    persist_write_int(PKEY_BATTERY_PERC, battery_perc);
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE) || defined(PBL_PLATFORM_GABBRO)
    if (s_cuarzo_cover_layer) update_cuarzo_cover();
#endif
}

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE) || defined(PBL_PLATFORM_GABBRO)
static void cuarzo_cover_draw(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    if (bounds.size.w <= 1) return;
    int h = bounds.size.h;

#if defined(PBL_PLATFORM_APLITE)
    graphics_context_set_fill_color(ctx, GColorBlack);
#else
    graphics_context_set_fill_color(ctx, GColorBulgarianRose);
#endif
    graphics_fill_rect(ctx, GRect(1, 0, bounds.size.w - 1, h), 0, GCornerNone);

    // dos píxeles decorativos centrados verticalmente simulan esquinas redondeadas
    int pixel_sep = h - 4;
    int margin    = (h - pixel_sep) / 2;
    graphics_fill_rect(ctx, GRect(0, margin - 1,         1, 1), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(0, margin + pixel_sep, 1, 1), 0, GCornerNone);
}

static void update_cuarzo_cover(void) {
    int pct       = battery_perc < 1 ? 1 : (battery_perc > 100 ? 100 : battery_perc);
    int uncovered = 10 + (pct - 1) * (s_cuarzo_w - 10) / 99;
    int cover_w   = s_cuarzo_w - uncovered;
    layer_set_frame(s_cuarzo_cover_layer,
        GRect(s_cuarzo_x + uncovered - 1, s_cuarzo_y, cover_w + 1, s_cuarzo_h));
}
#endif

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    int w = bounds.size.w, h = bounds.size.h;
#if defined(PBL_PLATFORM_GABBRO)
    int lw = 200, lh = 228, lx = (w - 200) / 2, ly = (h - 228) / 2;
#else
    int lw = w, lh = h, lx = 0, ly = 0;
#endif
#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    int by_off = 4;
#else
    int by_off = 0;
#endif

    s_uno_logo = gdraw_command_image_create_with_resource(RESOURCE_ID_UNO_LOGO);

    s_bg_layer = layer_create(bounds);
    layer_set_update_proc(s_bg_layer, bg_layer_draw);
    layer_add_child(window_layer, s_bg_layer);

    s_uno_bmp = gbitmap_create_with_resource(RESOURCE_ID_IMG_UNO);
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    s_uno_img_layer = bitmap_layer_create(GRect(lx + (lw - 44) / 2 - 43 + 5, ly + sy(9, lh), 44, 35));
#elif defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    s_uno_img_layer = bitmap_layer_create(GRect(6, -3, 44, 35));
#endif
    bitmap_layer_set_bitmap(s_uno_img_layer, s_uno_bmp);
    bitmap_layer_set_compositing_mode(s_uno_img_layer, GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_uno_img_layer));

    s_eye_bmp = gbitmap_create_with_resource(RESOURCE_ID_IMG_EYE);
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    GRect eye_rect = GRect(lx + sx(158, lw) - 32, ly + sy(57, lh) - 15, 30, 15);
#elif defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    GRect eye_rect = GRect(sx(158, w) - 32 + 8, sy(57, h) - 15 + by_off, 30, 15);
#endif
    s_eye_layer = bitmap_layer_create(eye_rect);
    bitmap_layer_set_bitmap(s_eye_layer, s_eye_bmp);
    bitmap_layer_set_compositing_mode(s_eye_layer, GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_eye_layer));

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE) || defined(PBL_PLATFORM_GABBRO)
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    s_cuarzo_w = 55; s_cuarzo_h = 18;
    s_cuarzo_x = lx + sx(158, lw) - 52;
    s_cuarzo_y = ly + sy(57, lh) - 15 - s_cuarzo_h - 8;
#elif defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    s_cuarzo_w = 55; s_cuarzo_h = 18;
    s_cuarzo_x = sx(158, w) - 52 + 16;
    s_cuarzo_y = sy(57, h) - 15 - s_cuarzo_h - 8 - 1 + by_off;
#endif
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

    s_eye_touch_rect = GRect(eye_rect.origin.x - 12, eye_rect.origin.y - 12,
                              eye_rect.size.w + 24,  eye_rect.size.h + 24);

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    s_digits_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_50));
    s_hours_font  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_51));
    s_date_font   = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_24));
#elif defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    s_digits_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_45));
    s_hours_font  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_46));
    s_date_font   = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DIGITS_22));
#endif

    int hex_x0 = lx + sx(6,   lw);
    int hex_x1 = lx + sx(194, lw);
    int hex_y0 = ly + sy(70,  lh) + by_off;
    int hex_y1 = ly + sy(151, lh) + by_off;
    int hex_w  = hex_x1 - hex_x0;
    int hex_h  = hex_y1 - hex_y0;

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    int font_h = 51;
    int date_font_h = 24;
    int colon_gap = 12;
    int colon_w   = 12;
#elif defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    int font_h = 45;
    int date_font_h = 22;
    int colon_gap = 6;
    int colon_w   = 13;
#endif

    int t_y0 = hex_y0 + (hex_h - font_h) / 2 - hex_h * 10 / 100 + 9;
    int t_h  = font_h + 4;

#if defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    int group_w   = 55;
    int date_w    = 28;
    int time_w    = group_w * 2 + colon_gap;
#else
    int date_w    = hex_w * 28 / 100;
    int time_w    = hex_w - date_w - 4;
    int group_w   = (time_w - colon_gap) / 2;
#endif
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    int time_offset = -3;
    int date_offset =  4;
    int time_y_off  =  0;
#elif defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE)
    int time_offset = -14;
    int date_offset = -12;
    int time_y_off  =  -6;
#endif
    int x_hours   = hex_x0 + hex_w * 10 / 100 + time_offset;
    int x_colon   = x_hours + group_w;
    int x_minutes = x_colon + colon_gap;
    int x_date    = hex_x0 + time_w - hex_w * 5 / 100 + 9 + date_offset;

#if defined(PBL_PLATFORM_BASALT)
    int date_y0 = hex_y0 + (hex_h - date_font_h) / 2 + 9;
#else
    int date_y0 = hex_y0 + (hex_h - date_font_h) / 2 + 9;
#endif
    int date_h  = date_font_h + 4;

    s_hours_layer = text_layer_create(GRect(x_hours, t_y0 + time_y_off, group_w, t_h));
    text_layer_set_background_color(s_hours_layer, GColorClear);
    text_layer_set_text_color(s_hours_layer, GColorBlack);
    text_layer_set_font(s_hours_layer, s_hours_font);
    text_layer_set_text_alignment(s_hours_layer, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(s_hours_layer));

    int cl_x = x_colon + colon_gap / 2 - colon_w / 2;
    s_colon_layer = text_layer_create(GRect(cl_x, t_y0 + time_y_off, colon_w, t_h));
    text_layer_set_background_color(s_colon_layer, GColorClear);
    text_layer_set_text_color(s_colon_layer, GColorBlack);
    text_layer_set_font(s_colon_layer, s_hours_font);
    text_layer_set_text_alignment(s_colon_layer, GTextAlignmentCenter);
    text_layer_set_text(s_colon_layer, ":");
    layer_add_child(window_layer, text_layer_get_layer(s_colon_layer));

    s_minutes_layer = text_layer_create(GRect(x_minutes, t_y0 + time_y_off, group_w, t_h));
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

    s_bg_bmp = gbitmap_create_with_resource(RESOURCE_ID_IMG_BG);
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
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_BASALT) || defined(PBL_PLATFORM_APLITE) || defined(PBL_PLATFORM_GABBRO)
    layer_destroy(s_cuarzo_cover_layer);
    bitmap_layer_destroy(s_cuarzo_layer);
    gbitmap_destroy(s_cuarzo_bmp);
#endif
    gbitmap_destroy(s_uno_bmp);
    gbitmap_destroy(s_eye_bmp);
    gbitmap_destroy(s_bg_bmp);
    if (s_uno_logo) {
        gdraw_command_image_destroy(s_uno_logo);
        s_uno_logo = NULL;
    }
    sonidos_song_free();
}

static void inbox_received_cb(DictionaryIterator *iter, void *ctx) {
    Tuple *t;
    bool needs_redraw = false;

    t = dict_find(iter, MESSAGE_KEY_WELCOME_ACTION);
    if (t) {
        s_welcome_action = (int)t->value->int32;
        persist_write_int(PKEY_WELCOME_ACTION, s_welcome_action);
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
    t = dict_find(iter, MESSAGE_KEY_SHAKE_ACTION);
    if (t) {
        s_shake_action = (int)t->value->int32;
        persist_write_int(PKEY_SHAKE_ACTION, s_shake_action);
        if (s_shake_action != SHAKE_NADA) {
            accel_tap_service_subscribe(tap_handler);
        } else {
            accel_tap_service_unsubscribe();
        }
    }
    t = dict_find(iter, MESSAGE_KEY_SHOW_SECONDS);
    if (t) {
        bool new_val = (bool)t->value->int32;
        persist_write_bool(PKEY_SHOW_SECONDS, new_val);
        if (new_val != s_show_seconds) {
            s_show_seconds = new_val;
            tick_timer_service_unsubscribe();
            tick_timer_service_subscribe(s_show_seconds ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
            needs_redraw = true;
        }
    }
    t = dict_find(iter, MESSAGE_KEY_DIEC18_DURATION);
    if (t) {
        s_diec18_duration = (int)t->value->int32;
        if (s_diec18_duration < 1) s_diec18_duration = 1;
        if (s_diec18_duration > 5) s_diec18_duration = 5;
        persist_write_int(PKEY_DIEC18_DURATION, s_diec18_duration);
        dieciocho_set_duration(s_diec18_duration * 1000);
    }
    t = dict_find(iter, MESSAGE_KEY_DIEC18_TARGET_MONTH);
    if (t) {
        int m = (int)t->value->int32;
        s_diec18_target_month = (m >= 1 && m <= 12) ? m : 9;
        persist_write_int(PKEY_DIEC18_TARGET_MONTH, s_diec18_target_month);
        dieciocho_set_target_date(s_diec18_target_month, s_diec18_target_day);
    }
    t = dict_find(iter, MESSAGE_KEY_DIEC18_TARGET_DAY);
    if (t) {
        int d = (int)t->value->int32;
        s_diec18_target_day = (d >= 1 && d <= 31) ? d : 18;
        persist_write_int(PKEY_DIEC18_TARGET_DAY, s_diec18_target_day);
        dieciocho_set_target_date(s_diec18_target_month, s_diec18_target_day);
    }
    if (needs_redraw) layer_mark_dirty(s_bg_layer);
}

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
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
    if (s_touch_in_eye) toggle_sonidos_mode();
}

static void touch_handler(const TouchEvent *event, void *context) {
    if (event->type == TouchEvent_Touchdown) {
        s_touch_in_eye = (event->x >= s_eye_touch_rect.origin.x &&
                          event->x <  s_eye_touch_rect.origin.x + s_eye_touch_rect.size.w &&
                          event->y >= s_eye_touch_rect.origin.y &&
                          event->y <  s_eye_touch_rect.origin.y + s_eye_touch_rect.size.h);
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
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    if (touch_service_is_enabled()) {
        touch_service_subscribe(touch_handler, NULL);
    }
#endif
    battery_perc   = battery_state_service_peek().charge_percent;
    s_bt_connected = bluetooth_connection_service_peek();
    if (s_cuarzo_cover_layer) update_cuarzo_cover();
    layer_mark_dirty(s_bg_layer);

    if (s_welcome_action == SHAKE_DIEC18) {
        dieciocho_trigger();
    } else if (s_welcome_action == SHAKE_SONIDO) {
        scroll_start();
        sonidos_song_play();
    }
}

static void main_window_disappear(Window *window) {
    if (sonidos_is_scrolling()) scroll_stop();
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    touch_service_unsubscribe();
#endif
}

static void init(void) {
    s_welcome_action = persist_exists(PKEY_WELCOME_ACTION)
        ? persist_read_int(PKEY_WELCOME_ACTION) : SHAKE_NADA;
    s_transparent  = persist_exists(PKEY_TRANSPARENT)
        ? persist_read_bool(PKEY_TRANSPARENT)  : true;
    s_bg_color = persist_exists(PKEY_BG_COLOR)
        ? GColorFromHEX(persist_read_int(PKEY_BG_COLOR)) : GColorDarkGray;
    s_dial_shape = persist_exists(PKEY_DIAL_SHAPE)
        ? persist_read_int(PKEY_DIAL_SHAPE) : DIAL_SHAPE_HEX;
    s_shake_action = persist_exists(PKEY_SHAKE_ACTION)
        ? persist_read_int(PKEY_SHAKE_ACTION) : SHAKE_NADA;
    s_show_seconds = persist_exists(PKEY_SHOW_SECONDS)
        ? persist_read_bool(PKEY_SHOW_SECONDS) : false;
    s_diec18_duration = persist_exists(PKEY_DIEC18_DURATION)
        ? persist_read_int(PKEY_DIEC18_DURATION) : 4;
    dieciocho_set_duration(s_diec18_duration * 1000);
    s_diec18_target_month = persist_exists(PKEY_DIEC18_TARGET_MONTH)
        ? persist_read_int(PKEY_DIEC18_TARGET_MONTH) : 9;
    s_diec18_target_day = persist_exists(PKEY_DIEC18_TARGET_DAY)
        ? persist_read_int(PKEY_DIEC18_TARGET_DAY) : 18;
    dieciocho_set_target_date(s_diec18_target_month, s_diec18_target_day);

    app_message_register_inbox_received(inbox_received_cb);
    app_message_open(256, 64);

    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load      = main_window_load,
        .unload    = main_window_unload,
        .appear    = main_window_appear,
        .disappear = main_window_disappear,
    });
    battery_perc   = persist_exists(PKEY_BATTERY_PERC)
        ? persist_read_int(PKEY_BATTERY_PERC)
        : battery_state_service_peek().charge_percent;
    s_bt_connected = bluetooth_connection_service_peek();
    battery_state_service_subscribe(battery_handler);
    bluetooth_connection_service_subscribe(bt_handler);
    window_stack_push(s_main_window, true);
    tick_timer_service_subscribe(s_show_seconds ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
    if (s_shake_action != SHAKE_NADA) accel_tap_service_subscribe(tap_handler);
}

static void deinit(void) {
    sonidos_song_stop();
    battery_state_service_unsubscribe();
    tick_timer_service_unsubscribe();
    accel_tap_service_unsubscribe();
    bluetooth_connection_service_unsubscribe();
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}
