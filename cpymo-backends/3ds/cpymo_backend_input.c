#include "../../cpymo/cpymo_prelude.h"
#include "../../cpymo/cpymo_engine.h"
#include "../include/cpymo_backend_input.h"
#include "settings_3ds.h"
#include <3ds.h>
#include <stdlib.h>
#include <string.h>

bool enhanced_3ds_display_mode_touch_ui_enabled(void);
const extern bool enhanced_3ds_display_mode;
extern cpymo_engine engine;

/* Defined in main.c. In debug mode the bottom screen shows the CPyMO console
 * and START must not do anything. */
extern bool cpymo_3ds_debug_mode;

static bool touch_enabled(void) {
    if (!enhanced_3ds_display_mode_touch_ui_enabled()) {
        if (engine.say.active) return true;
        else return false;
    }
    else return true;
}

const extern float game_width, game_height;
const extern float ratio_wb, ratio_h;

/* The selector and every menu keep the classic, fixed button mapping so that
 * navigation, save/load and the launcher behave exactly like upstream. Only a
 * running game (no UI on the stack) honours the player's custom bindings. */
static bool using_default_mapping(void)
{
    /* Custom bindings are only live on the in-game dialogue interface.
     * Everything else keeps the classic mapping so that D-pad navigation,
     * A = confirm and B = cancel always work: the selector, every menu, the
     * backlog, save/load, the settings page and branch choices. */
    if (engine.ui != NULL) return true;
    if (engine.assetloader.gamedir == NULL) return true;
    if (engine.select_img.selections != NULL) return true;
    if (!engine.say.active) return true;
    return false;
}

static void apply_default_mapping(cpymo_input *out, u32 keys)
{
    /* ZL/ZR were already folded into L/R above, so they are not listed here:
     * they always do exactly what L/R do, in menus as well. */
    out->ok = (keys & KEY_A) > 0 || (keys & KEY_Y) > 0;
    out->skip = (keys & KEY_R) > 0;
    out->cancel = (keys & KEY_B) > 0 || (keys & KEY_X) > 0;
    out->hide_window = (keys & KEY_L) > 0;
    out->down = (keys & KEY_DOWN) > 0;
    out->up = (keys & KEY_UP) > 0;
    out->left = (keys & KEY_LEFT) > 0;
    out->right = (keys & KEY_RIGHT) > 0;
}

static void apply_custom_mapping(cpymo_input *out)
{
    out->ok = cpymo_3ds_settings_action_held(cpymo_3ds_action_next);
    out->skip = cpymo_3ds_settings_action_held(cpymo_3ds_action_skip);
    out->hide_window = cpymo_3ds_settings_action_held(cpymo_3ds_action_hide);
    out->cancel = cpymo_3ds_settings_action_held(cpymo_3ds_action_menu);

    /* "history" is the engine's `up` key: it opens the backlog while reading.
     * Actions without a cpymo_input equivalent (auto/save/load/settings/
     * restart) are dispatched by main.c. */
    out->up = cpymo_3ds_settings_action_held(cpymo_3ds_action_history);
    out->down = false;
    out->left = false;
    out->right = false;
}

cpymo_input cpymo_input_snapshot()
{
    cpymo_input out;
    memset(&out, 0, sizeof(out));

    u32 keys = hidKeysHeld();

    /* ZL/ZR mirror L/R and are never shown as separate buttons. */
    if (keys & KEY_ZL) keys |= KEY_L;
    if (keys & KEY_ZR) keys |= KEY_R;

    if ((hidKeysDown() & KEY_START) && !cpymo_3ds_debug_mode)
        cpymo_engine_exit(&engine);

    if (using_default_mapping()) apply_default_mapping(&out, keys);
    else apply_custom_mapping(&out);

    out.mouse_button = (keys & KEY_TOUCH) > 0 && enhanced_3ds_display_mode;
    out.mouse_position_useable = out.mouse_button && touch_enabled();
    out.mouse_wheel_delta = 0;


    circlePosition pos;
    hidCircleRead(&pos);

    circlePosition cpos;
    hidCstickRead(&cpos);

    if(pos.dy > 170 || cpos.dy > 64) out.up = true;
    else if(pos.dy < -170 || cpos.dy < -64) out.down = true;

    if(pos.dx > 170 || cpos.dx > 64) out.right = true;
    else if(pos.dx < -170 || cpos.dx < -64) out.left = true;

    if (out.mouse_position_useable) {
        touchPosition touch_pos;
        hidTouchRead(&touch_pos);

        const float r = ratio_wb > ratio_h ? ratio_wb : ratio_h;
        
        const float
            viewport_w = game_width / r,
            viewport_h = game_height / r;

        const float
            offset_x = (320 - viewport_w) / 2.0f,
            offset_y = (240 - viewport_h) / 2.0f;

        float touch_x = (float)touch_pos.px;
        float touch_y = (float)touch_pos.py;

        touch_x -= offset_x;
        touch_y -= offset_y;
        touch_x /= viewport_w;
        touch_y /= viewport_h;

        out.mouse_x = touch_x * game_width;
        out.mouse_y = touch_y * game_height;

        if (touch_x > 1 || touch_x < 0 || touch_y > 1 || touch_y < 0)
            out.mouse_position_useable = false;
    }

    return out;
}
