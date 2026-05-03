#pragma once
#include <pebble.h>

// Inicializar el módulo con las capas y buffers que necesita.
// update_cb se llama al salir del modo 18 para refrescar la hora.
void dieciocho_init(TextLayer *hours, TextLayer *minutes, TextLayer *date,
                    TextLayer *colon, Layer *bg,
                    char *hours_buf, char *minutes_buf, char *date_buf,
                    void (*update_cb)(void));

bool dieciocho_is_active(void);

// Devuelve true si el indicador HOY debe ser visible en este fotograma.
bool dieciocho_hoy_visible(void);

// Activar countdown (llamar en tap_handler).
void dieciocho_trigger(void);

// Cancelar timers sin disparar callbacks (llamar en window_unload).
void dieciocho_teardown(void);
