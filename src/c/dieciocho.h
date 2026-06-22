#pragma once
#include <pebble.h>

void dieciocho_init(TextLayer *hours, TextLayer *minutes, TextLayer *date,
                    TextLayer *colon, Layer *bg,
                    char *hours_buf, char *minutes_buf, char *date_buf,
                    void (*update_cb)(void));

bool dieciocho_is_active(void);
bool dieciocho_hoy_visible(void);
void dieciocho_set_duration(int ms);
void dieciocho_trigger(void);
void dieciocho_teardown(void);
