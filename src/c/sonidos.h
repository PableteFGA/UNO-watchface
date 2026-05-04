#pragma once
#include <pebble.h>

// Inicializar el módulo con las capas, fuentes y callback post-stop.
// on_stop se llama al terminar el scroll para refrescar la hora.
void sonidos_init(TextLayer *hours, TextLayer *minutes, TextLayer *colon,
                  TextLayer *date, Layer *bg,
                  GFont hours_font, GFont digits_font,
                  void (*on_stop)(void));

bool sonidos_is_scrolling(void);

// Número de canción activa (se muestra en el lugar del día durante el scroll).
void sonidos_set_song_number(int n);
void scroll_start(void);

// Detener el scroll y disparar on_stop.
void scroll_stop(void);

// Cancelar el timer sin disparar on_stop (llamar en window_unload).
void sonidos_teardown(void);

// --- Himno ---
// Cargar notas desde el recurso (llamar en window_load).
void sonidos_song_load(void);

// Liberar memoria de las notas (llamar en window_unload).
void sonidos_song_free(void);

// Reproducir el himno una sola vez al iniciar (llamar en window_appear).
void sonidos_song_play_once(void);

// Reproducir el himno incondicionalmente (llamar al activar modo sonidos).
void sonidos_song_play(void);

// Detener reproducción (llamar en deinit).
void sonidos_song_stop(void);
