#pragma once
#include <pebble.h>

void sonidos_init(TextLayer *hours, TextLayer *minutes, TextLayer *colon,
                  TextLayer *date, Layer *bg,
                  GFont hours_font, GFont digits_font,
                  void (*on_stop)(void));

bool sonidos_is_scrolling(void);
void sonidos_set_song_number(int n);
void scroll_start(void);
void scroll_stop(void);
void sonidos_teardown(void);

void sonidos_song_load(void);
void sonidos_song_free(void);
void sonidos_song_play_once(void);
void sonidos_song_play(void);
void sonidos_song_stop(void);
