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

#endif
