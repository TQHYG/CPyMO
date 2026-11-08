#include "../../cpymo/cpymo_prelude.h"
#include "settings_3ds.h"

#include <3ds.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cpymo_3ds_settings settings;
static bool settings_loaded = false;

static const char *const action_names[cpymo_3ds_action_count] = {
	"undefined",  /* cpymo_3ds_action_undefined */
	"next",       /* cpymo_3ds_action_next */
	"auto",       /* cpymo_3ds_action_auto */
	"save",       /* cpymo_3ds_action_save */
	"load",       /* cpymo_3ds_action_load */
	"skip",       /* cpymo_3ds_action_skip */
	"hide",       /* cpymo_3ds_action_hide */
	"history",    /* cpymo_3ds_action_history */
	"menu",       /* cpymo_3ds_action_menu */
	"settings",   /* cpymo_3ds_action_settings */
	"restart",    /* cpymo_3ds_action_restart */
};

static const char *const action_labels[cpymo_3ds_action_count] = {
	"未定义",
	"下一句",
	"自动播放",
	"存档",
	"读档",
	"快进",
	"隐藏对话框",
	"对话历史",
	"菜单",
	"设置",
	"重启游戏",
};

static const char *const key_names[cpymo_3ds_key_count] = {
	"up", "down", "left", "right", "a", "b", "x", "y", "l", "r",
};

static const char *const key_labels[cpymo_3ds_key_count] = {
	"上", "下", "左", "右", "A", "B", "X", "Y", "L", "R",
};

static u32 key_bits(int key)
{
	switch (key) {
	case cpymo_3ds_key_up: return KEY_UP;
	case cpymo_3ds_key_down: return KEY_DOWN;
	case cpymo_3ds_key_left: return KEY_LEFT;
	case cpymo_3ds_key_right: return KEY_RIGHT;
	case cpymo_3ds_key_a: return KEY_A;
	case cpymo_3ds_key_b: return KEY_B;
	case cpymo_3ds_key_x: return KEY_X;
	case cpymo_3ds_key_y: return KEY_Y;
	case cpymo_3ds_key_l: return KEY_L | KEY_ZL;
	case cpymo_3ds_key_r: return KEY_R | KEY_ZR;
	default: return 0;
	}
}

void cpymo_3ds_settings_set_defaults(cpymo_3ds_settings *s)
{
	memset(s, 0, sizeof(*s));

	s->display_mode = -1;
	s->fontsize = 0;
	s->textspeed = -1;

	for (int i = 0; i < cpymo_3ds_key_count; ++i)
		s->keys[i] = cpymo_3ds_action_undefined;

	/* Defaults requested for the 3DS build. */
	s->keys[cpymo_3ds_key_up] = cpymo_3ds_action_history;
	s->keys[cpymo_3ds_key_down] = cpymo_3ds_action_next;
	s->keys[cpymo_3ds_key_a] = cpymo_3ds_action_next;
	s->keys[cpymo_3ds_key_b] = cpymo_3ds_action_auto;
	s->keys[cpymo_3ds_key_x] = cpymo_3ds_action_hide;
	s->keys[cpymo_3ds_key_y] = cpymo_3ds_action_menu;
	s->keys[cpymo_3ds_key_l] = cpymo_3ds_action_next;
	s->keys[cpymo_3ds_key_r] = cpymo_3ds_action_skip;
}

const cpymo_3ds_settings *cpymo_3ds_settings_get(void)
{
	if (!settings_loaded) cpymo_3ds_settings_load();
	return &settings;
}

cpymo_3ds_settings *cpymo_3ds_settings_mutable(void)
{
	if (!settings_loaded) cpymo_3ds_settings_load();
	return &settings;
}

const char *cpymo_3ds_action_name(cpymo_3ds_action action)
{
	if (action < 0 || action >= cpymo_3ds_action_count) return "undefined";
	return action_names[action];
}

const char *cpymo_3ds_action_label(cpymo_3ds_action action)
{
	if (action < 0 || action >= cpymo_3ds_action_count) return action_labels[0];
	return action_labels[action];
}

cpymo_3ds_action cpymo_3ds_action_parse(const char *name)
{
	for (int i = 0; i < cpymo_3ds_action_count; ++i)
		if (strcmp(name, action_names[i]) == 0)
			return (cpymo_3ds_action)i;
	return cpymo_3ds_action_undefined;
}

const char *cpymo_3ds_key_name(int key)
{
	if (key < 0 || key >= cpymo_3ds_key_count) return "?";
	return key_names[key];
}

const char *cpymo_3ds_key_label(int key)
{
	if (key < 0 || key >= cpymo_3ds_key_count) return "?";
	return key_labels[key];
}

static char *trim(char *s)
{
	while (*s && isspace((unsigned char)*s)) ++s;

	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1])) --end;
	*end = '\0';

	return s;
}

static void apply_line(char *line)
{
	char *eq = strchr(line, '=');
	if (eq == NULL) return;

	*eq = '\0';
	char *key = trim(line);
	char *value = trim(eq + 1);

	if (*key == '\0' || *value == '\0') return;

	if (strcmp(key, "display_mode") == 0) {
		int mode = atoi(value);
		if (mode >= 0 && mode <= 3) settings.display_mode = mode;
		else if (mode < 0) settings.display_mode = -1;
		return;
	}

	if (strcmp(key, "fontsize") == 0) {
		int size = atoi(value);
		if (size <= 0) settings.fontsize = 0;
		else if (size >= 12 && size <= 32) settings.fontsize = size;
		return;
	}

	if (strcmp(key, "textspeed") == 0) {
		int speed = atoi(value);
		if (speed < 0) settings.textspeed = -1;
		else if (speed <= 5) settings.textspeed = speed;
		return;
	}

	if (strncmp(key, "key_", 4) == 0) {
		for (int i = 0; i < cpymo_3ds_key_count; ++i) {
			if (strcmp(key + 4, key_names[i]) == 0) {
				settings.keys[i] = cpymo_3ds_action_parse(value);
				return;
			}
		}
	}
}

error_t cpymo_3ds_settings_load(void)
{
	cpymo_3ds_settings_set_defaults(&settings);
	settings_loaded = true;

	FILE *f = fopen(CPYMO_3DS_SETTINGS_PATH, "rb");
	if (f == NULL) return CPYMO_ERR_CAN_NOT_OPEN_FILE;

	char line[256];
	while (fgets(line, sizeof(line), f)) {
		char *text = trim(line);
		if (*text == '\0' || *text == '#' || *text == ';') continue;
		apply_line(text);
	}

	fclose(f);
	return CPYMO_ERR_SUCC;
}

error_t cpymo_3ds_settings_save(void)
{
	FILE *f = fopen(CPYMO_3DS_SETTINGS_PATH, "wb");
	if (f == NULL) return CPYMO_ERR_CAN_NOT_OPEN_FILE;

	fprintf(f,
		"# CPyMO for 3DS - global settings\n"
		"# This file is not part of any game save. Delete it to reset.\n"
		"#\n"
		"# display_mode: -1 = game default, 0..3 = forced default display mode\n"
		"# fontsize:     0 = follow the game, 12..32 = default font size\n"
		"# textspeed:    -1 = follow the game, 0..5 = default text speed\n"
		"# key_*:        undefined|next|auto|save|load|skip|hide|history|menu|settings|restart\n"
		"#               (ZL mirrors L, ZR mirrors R)\n"
		"\n"
		"display_mode=%d\n"
		"fontsize=%d\n"
		"textspeed=%d\n"
		"\n",
		settings.display_mode, settings.fontsize, settings.textspeed);

	for (int i = 0; i < cpymo_3ds_key_count; ++i)
		fprintf(f, "key_%s=%s\n", key_names[i], cpymo_3ds_action_name(settings.keys[i]));

	const bool written = ferror(f) == 0;
	const int closed = fclose(f);
	if (!written || closed != 0) return CPYMO_ERR_UNKNOWN;

	return CPYMO_ERR_SUCC;
}

bool cpymo_3ds_settings_action_held(cpymo_3ds_action action)
{
	const cpymo_3ds_settings *s = cpymo_3ds_settings_get();
	const u32 keys = hidKeysHeld();

	for (int i = 0; i < cpymo_3ds_key_count; ++i)
		if (s->keys[i] == action && (keys & key_bits(i)))
			return true;

	return false;
}

bool cpymo_3ds_settings_action_pressed(cpymo_3ds_action action)
{
	const cpymo_3ds_settings *s = cpymo_3ds_settings_get();
	const u32 keys = hidKeysDown();

	for (int i = 0; i < cpymo_3ds_key_count; ++i)
		if (s->keys[i] == action && (keys & key_bits(i)))
			return true;

	return false;
}
