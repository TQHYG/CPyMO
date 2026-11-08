#ifndef CPYMO_3DS_UI_H
#define CPYMO_3DS_UI_H

#include "../../cpymo/cpymo_engine.h"
#include "../../cpymo/cpymo_game_selector.h"

void cpymo_3ds_ui_init(void);
void cpymo_3ds_ui_free(void);
void cpymo_3ds_ui_draw_launcher(const cpymo_game_selector_item *selected);
void cpymo_3ds_ui_draw_help(int width, int mode);
void cpymo_3ds_ui_draw_enhanced_background(void);
void cpymo_3ds_ui_draw_enhanced_hints(void);
void cpymo_3ds_ui_draw_clock(int width);

/* Voice / auto play progress bar in the bottom right corner of the bottom
 * screen. Draws nothing when no voice is playing and auto play is idle. */
void cpymo_3ds_ui_draw_progress(void);

/* Fills `lines` with "<action>：<keys>" help text based on the current global
 * key bindings and returns how many lines were written. */
int cpymo_3ds_ui_key_help_lines(char lines[][80], int max_lines);

/* ---- global settings page (sdmc:/pymogames/settings.txt) ---- */

bool cpymo_3ds_ui_settings_open(void);
void cpymo_3ds_ui_settings_enter(void);

/* Consumes input while the page is open and closes it on B. */
void cpymo_3ds_ui_settings_update(void);

/* Draws the page; call once per physical screen (false = top, true = bottom). */
void cpymo_3ds_ui_settings_draw(bool bottom_screen);

/* Touch hit test for the launcher's settings entry (bottom screen pixels). */
bool cpymo_3ds_ui_settings_hit(float x, float y);

#endif
