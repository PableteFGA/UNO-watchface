#include <pebble.h>
#include "dieciocho.h"

static TextLayer *s_hours_layer;
static TextLayer *s_minutes_layer;
static TextLayer *s_date_layer;
static TextLayer *s_colon_layer;
static Layer     *s_bg_layer;
static char      *s_hours_buf;
static char      *s_minutes_buf;
static char      *s_date_buf;
static void     (*s_update_cb)(void);

static bool      s_active        = false;
static AppTimer *s_countdown_timer = NULL;
static AppTimer *s_blink_timer   = NULL;
static bool      s_hoy_blink     = false;
static int       s_countdown_token = 0;

void dieciocho_init(TextLayer *hours, TextLayer *minutes, TextLayer *date,
                    TextLayer *colon, Layer *bg,
                    char *hours_buf, char *minutes_buf, char *date_buf,
                    void (*update_cb)(void)) {
    s_hours_layer   = hours;
    s_minutes_layer = minutes;
    s_date_layer    = date;
    s_colon_layer   = colon;
    s_bg_layer      = bg;
    s_hours_buf     = hours_buf;
    s_minutes_buf   = minutes_buf;
    s_date_buf      = date_buf;
    s_update_cb     = update_cb;
}

bool dieciocho_is_active(void)   { return s_active; }
bool dieciocho_hoy_visible(void) { return !s_active || s_hoy_blink; }

static int days_to_sept18(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    struct tm target = {0};
    target.tm_year  = t->tm_year;
    target.tm_mon   = 8;
    target.tm_mday  = 18;
    target.tm_isdst = -1;
    time_t t_target = mktime(&target);
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
    if (s_active) {
        s_blink_timer = app_timer_register(250, blink_timer_callback, NULL);
    } else {
        s_blink_timer = NULL;
    }
}

static void hide_countdown(void);

static void countdown_timer_callback(void *context) {
    int token = (int)(intptr_t)context;
    if (token != s_countdown_token) return;
    s_countdown_timer = NULL;
    hide_countdown();
}

static void show_countdown(void) {
    s_active = true;
    int days      = days_to_sept18();
    int hundreds  = days / 100;
    int remainder = days % 100;
    if (hundreds > 0) {
        snprintf(s_hours_buf, 3, "%d", hundreds);
    } else {
        s_hours_buf[0] = '\0';
    }
    snprintf(s_minutes_buf, 3, "%02d", remainder);
    snprintf(s_date_buf,    3, "18");
    layer_set_hidden(text_layer_get_layer(s_colon_layer), true);
    text_layer_set_text(s_hours_layer,   s_hours_buf);
    text_layer_set_text(s_minutes_layer, s_minutes_buf);
    text_layer_set_text(s_date_layer,    s_date_buf);
    s_hoy_blink = true;
    if (s_blink_timer) app_timer_cancel(s_blink_timer);
    s_blink_timer = app_timer_register(250, blink_timer_callback, NULL);
    layer_mark_dirty(s_bg_layer);
}

static void hide_countdown(void) {
    s_active    = false;
    s_hoy_blink = false;
    if (s_blink_timer) { app_timer_cancel(s_blink_timer); s_blink_timer = NULL; }
    layer_set_hidden(text_layer_get_layer(s_colon_layer), false);
    if (s_update_cb) s_update_cb();
    layer_mark_dirty(s_bg_layer);
}

void dieciocho_trigger(void) {
    if (s_countdown_timer) app_timer_cancel(s_countdown_timer);
    s_countdown_token++;
    show_countdown();
    s_countdown_timer = app_timer_register(4000, countdown_timer_callback,
                                           (void *)(intptr_t)s_countdown_token);
}

void dieciocho_teardown(void) {
    if (s_blink_timer)     { app_timer_cancel(s_blink_timer);     s_blink_timer = NULL; }
    if (s_countdown_timer) { app_timer_cancel(s_countdown_timer); s_countdown_timer = NULL; }
    s_active    = false;
    s_hoy_blink = false;
}
