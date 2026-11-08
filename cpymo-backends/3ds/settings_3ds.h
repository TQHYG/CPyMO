#ifndef CPYMO_3DS_SETTINGS_H
#define CPYMO_3DS_SETTINGS_H

#include <stdbool.h>

#include "../../cpymo/cpymo_error.h"

/* Global 3DS frontend settings.
 *
 * They live in a plain text file so that they survive on the SD card and stay
 * completely separate from the engine save data:
 *
 *   sdmc:/pymogames/settings.txt
 *
 * Nothing in here is ever written into a game's save directory, so per-game
 * saves stay bit compatible with the upstream engine. */

/* What a physical button does while a game is running. */
typedef enum {
	cpymo_3ds_action_undefined = 0,
	cpymo_3ds_action_next,      /* advance to the next sentence */
	cpymo_3ds_action_auto,      /* toggle auto play */
	cpymo_3ds_action_save,      /* open the save menu */
	cpymo_3ds_action_load,      /* open the load menu */
	cpymo_3ds_action_skip,      /* hold to skip */
	cpymo_3ds_action_hide,      /* hold to hide the dialogue window */
	cpymo_3ds_action_history,   /* open the backlog */
	cpymo_3ds_action_menu,      /* open the game menu */
	cpymo_3ds_action_settings,  /* open the in-game settings page */
	cpymo_3ds_action_restart,   /* restart the current game */
	cpymo_3ds_action_count
} cpymo_3ds_action;

/* Physical buttons that can be bound. ZL/ZR always mirror L/R and are not
 * listed here on purpose. */
enum {
	cpymo_3ds_key_up = 0,
	cpymo_3ds_key_down,
	cpymo_3ds_key_left,
	cpymo_3ds_key_right,
	cpymo_3ds_key_a,
	cpymo_3ds_key_b,
	cpymo_3ds_key_x,
	cpymo_3ds_key_y,
	cpymo_3ds_key_l,
	cpymo_3ds_key_r,
	cpymo_3ds_key_count
};

typedef struct {
	/* -1: use the built-in default (mode 2 = dialogue on the bottom screen,
	 * aspect-fit) when the game has no display mode of its own. */
	int display_mode;

	/* 0: keep whatever the game / its config.csav defines. */
	int fontsize;
	int textspeed;

	cpymo_3ds_action keys[cpymo_3ds_key_count];
} cpymo_3ds_settings;

#define CPYMO_3DS_SETTINGS_PATH "sdmc:/pymogames/settings.txt"

/* Returns the global (read only) settings. Never NULL. */
const cpymo_3ds_settings *cpymo_3ds_settings_get(void);

/* Mutable access for the settings page. Call cpymo_3ds_settings_save() after
 * changing anything. */
cpymo_3ds_settings *cpymo_3ds_settings_mutable(void);

void cpymo_3ds_settings_set_defaults(cpymo_3ds_settings *s);

/* Read sdmc:/pymogames/settings.txt. Missing file or malformed lines are not
 * fatal: the defaults stay in place for whatever could not be read. */
error_t cpymo_3ds_settings_load(void);

error_t cpymo_3ds_settings_save(void);

/* True when any button bound to `action` is currently held / just pressed.
 * ZL/ZR are treated as L/R. */
bool cpymo_3ds_settings_action_held(cpymo_3ds_action action);
bool cpymo_3ds_settings_action_pressed(cpymo_3ds_action action);

const char *cpymo_3ds_action_name(cpymo_3ds_action action);
const char *cpymo_3ds_action_label(cpymo_3ds_action action);
cpymo_3ds_action cpymo_3ds_action_parse(const char *name);

const char *cpymo_3ds_key_name(int key);
const char *cpymo_3ds_key_label(int key);

#endif
