#include <pebble.h>
#include "sonidos.h"
#include "dieciocho.h"

static const char SCROLL_MSG[] = "DANDO LA HORA - HECHO EN CHILE - ";
#define SCROLL_MSG_LEN ((int)(sizeof(SCROLL_MSG) - 1))
#define SCROLL_FALLBACK_MS 150  // usado si el himno no pudo cargarse

static TextLayer *s_hours_layer;
static TextLayer *s_minutes_layer;
static TextLayer *s_colon_layer;
static TextLayer *s_date_layer;
static Layer     *s_bg_layer;
static GFont      s_hours_font;
static GFont      s_digits_font;
static void     (*s_on_stop)(void);

static AppTimer *s_scroll_timer    = NULL;
static int       s_scroll_pos      = 0;
static bool      s_scrolling       = false;
static int       s_note_idx        = 0;   // nota actual que impulsa el scroll
static char      s_scroll_h[3];
static char      s_scroll_m[3];

// Datos de timing cargados desde himno.txt (disponibles en todas las plataformas)
static uint32_t *s_note_ms         = NULL;
static bool     *s_note_silent     = NULL;
static uint16_t  s_note_count_total = 0;

void sonidos_init(TextLayer *hours, TextLayer *minutes, TextLayer *colon,
                  TextLayer *date, Layer *bg,
                  GFont hours_font, GFont digits_font,
                  void (*on_stop)(void)) {
    s_hours_layer   = hours;
    s_minutes_layer = minutes;
    s_colon_layer   = colon;
    s_date_layer    = date;
    s_bg_layer      = bg;
    s_hours_font    = hours_font;
    s_digits_font   = digits_font;
    s_on_stop       = on_stop;
}

bool sonidos_is_scrolling(void) { return s_scrolling; }

void scroll_stop(void) {
    s_scrolling = false;
    if (s_scroll_timer) { app_timer_cancel(s_scroll_timer); s_scroll_timer = NULL; }
    text_layer_set_font(s_hours_layer,   s_hours_font);
    text_layer_set_font(s_minutes_layer, s_digits_font);
    layer_set_hidden(text_layer_get_layer(s_colon_layer), false);
    layer_set_hidden(text_layer_get_layer(s_date_layer),  false);
    layer_mark_dirty(s_bg_layer);
    if (s_on_stop) s_on_stop();
}

static void show_scroll_pos(void) {
    // Muestra 2 chars en cada TextLayer con wraparound circular
    for (int i = 0; i < 2; i++) {
        s_scroll_h[i] = SCROLL_MSG[(s_scroll_pos + i) % SCROLL_MSG_LEN];
    }
    s_scroll_h[2] = '\0';

    for (int i = 0; i < 2; i++) {
        s_scroll_m[i] = SCROLL_MSG[(s_scroll_pos + 2 + i) % SCROLL_MSG_LEN];
    }
    s_scroll_m[2] = '\0';

    text_layer_set_text(s_hours_layer,   s_scroll_h);
    text_layer_set_text(s_minutes_layer, s_scroll_m);
}

static void scroll_step(void *context) {
    s_scroll_timer = NULL;
    if (!s_scrolling) return;
    if (dieciocho_is_active()) { scroll_stop(); return; }

    // La nota s_note_idx acaba de terminar; avanzar si no es silencio
    bool silent = (s_note_count_total > 0) ? s_note_silent[s_note_idx] : false;
    if (!silent) {
        s_scroll_pos = (s_scroll_pos + 1) % SCROLL_MSG_LEN;
        show_scroll_pos();
    }

    s_note_idx++;

    // Si se acabaron las notas, terminar el scroll
    if (s_note_count_total == 0 || s_note_idx >= s_note_count_total) {
        scroll_stop();
        return;
    }

    s_scroll_timer = app_timer_register(s_note_ms[s_note_idx], scroll_step, NULL);
}

void scroll_start(void) {
    if (dieciocho_is_active()) return;
    if (s_scrolling) scroll_stop();
    s_scrolling  = true;
    s_scroll_pos = 0;
    s_note_idx   = 0;
    text_layer_set_font(s_hours_layer,   s_hours_font);
    text_layer_set_font(s_minutes_layer, s_hours_font);
    layer_set_hidden(text_layer_get_layer(s_colon_layer), true);
    layer_set_hidden(text_layer_get_layer(s_date_layer),  true);
    show_scroll_pos();
    layer_mark_dirty(s_bg_layer);
    uint32_t first_ms = (s_note_count_total > 0) ? s_note_ms[0] : SCROLL_FALLBACK_MS;
    s_scroll_timer = app_timer_register(first_ms, scroll_step, NULL);
}

void sonidos_teardown(void) {
    if (s_scroll_timer) { app_timer_cancel(s_scroll_timer); s_scroll_timer = NULL; }
    s_scrolling = false;
}

// --- Himno ---

static int read_int(char **pp, int *out) {
    char *p = *pp;
    while (*p == ' ' || *p == '\t') p++;
    if (*p < '0' || *p > '9') return 0;
    int val = 0;
    while (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); p++; }
    *pp = p;
    *out = val;
    return 1;
}

#ifdef PBL_SPEAKER
static SpeakerNote *s_song_notes = NULL;
static uint16_t     s_song_count = 0;
static bool         s_first_start = true;
#endif

void sonidos_song_load(void) {
    ResHandle rh = resource_get_handle(RESOURCE_ID_SONG_HIMNO);
    size_t size = resource_size(rh);
    if (size == 0) return;

    char *buf = malloc(size + 1);
    if (!buf) return;
    resource_load(rh, (uint8_t *)buf, size);
    buf[size] = '\0';

    // Primera pasada: contar líneas de datos
    uint16_t count = 0;
    char *p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\r') p++;
        if (*p != '#' && *p != '\n' && *p != '\0') count++;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    if (count == 0) { free(buf); return; }

    // Alocar arrays de timing (todas las plataformas) y de reproducción (solo speaker)
    s_note_ms     = malloc(count * sizeof(uint32_t));
    s_note_silent = malloc(count * sizeof(bool));
#ifdef PBL_SPEAKER
    s_song_notes  = malloc(count * sizeof(SpeakerNote));
    if (!s_note_ms || !s_note_silent || !s_song_notes) {
        free(s_note_ms);     s_note_ms = NULL;
        free(s_note_silent); s_note_silent = NULL;
        free(s_song_notes);  s_song_notes = NULL;
        free(buf); return;
    }
#else
    if (!s_note_ms || !s_note_silent) {
        free(s_note_ms);     s_note_ms = NULL;
        free(s_note_silent); s_note_silent = NULL;
        free(buf); return;
    }
#endif

    // Segunda pasada: parsear
    uint16_t idx = 0;
    p = buf;
    while (*p && idx < count) {
        while (*p == ' ' || *p == '\r') p++;
        if (*p == '#' || *p == '\n') {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }
        int note = 0, ms = 0, vel = 100;
        int parsed = 0;
        char *q = p;
        if (read_int(&q, &note)) {
            parsed++;
            if (read_int(&q, &ms)) {
                parsed++;
                if (read_int(&q, &vel)) parsed++;
            }
        }
        while (*q && *q != '\n') q++;
        if (parsed >= 2) {
            s_note_ms[idx]     = (uint32_t)ms;
            s_note_silent[idx] = (note == 0);
#ifdef PBL_SPEAKER
            s_song_notes[idx] = (SpeakerNote){
                .midi_note   = (uint8_t)note,
                .waveform    = SpeakerWaveformSquare,
                .duration_ms = (uint32_t)ms,
                .velocity    = (uint8_t)(parsed >= 3 ? vel : 100)
            };
#endif
            idx++;
        }
        p = q;
        if (*p == '\n') p++;
    }
    s_note_count_total = idx;
#ifdef PBL_SPEAKER
    s_song_count = idx;
#endif
    free(buf);
}

void sonidos_song_free(void) {
    if (s_note_ms)     { free(s_note_ms);     s_note_ms = NULL; }
    if (s_note_silent) { free(s_note_silent); s_note_silent = NULL; }
    s_note_count_total = 0;
#ifdef PBL_SPEAKER
    if (s_song_notes) { free(s_song_notes); s_song_notes = NULL; }
    s_song_count = 0;
#endif
}

void sonidos_song_play_once(void) {
#ifdef PBL_SPEAKER
    if (s_first_start) {
        s_first_start = false;
        if (s_song_notes && s_song_count > 0) {
            speaker_play_notes(s_song_notes, s_song_count, 80);
        }
    }
#endif
}

void sonidos_song_stop(void) {
#ifdef PBL_SPEAKER
    speaker_stop();
#endif
}
