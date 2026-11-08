#include "ui_3ds.h"
#include "settings_3ds.h"
#include "../include/cpymo_backend_image.h"
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

extern C2D_Font cpymo_backend_text_3ds_font(void);
extern void cpymo_3ds_draw_image_raw(cpymo_backend_image image, float x, float y, float w, float h);
extern unsigned char *stbi_load(char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);

static C2D_TextBuf ui_text_buf;
static cpymo_backend_image bottom_background;
static float ui_layer;
extern float render_3d_offset;
extern cpymo_engine engine;

static float ui_x(float x)
{
	return x + ui_layer * render_3d_offset;
}

#define COLOR_WHITE C2D_Color32(238, 244, 250, 255)
#define COLOR_MUTED C2D_Color32(158, 180, 202, 255)
#define COLOR_ACCENT C2D_Color32(91, 211, 232, 255)
#define COLOR_HEADER C2D_Color32(22, 49, 76, 255)
#define COLOR_SELECTED C2D_Color32(38, 91, 128, 255)

/* Settings button in the bottom right corner of the launcher's bottom screen.
 * Text only ("Y 设置") but still tappable. */
#define SETTINGS_BTN_X 248.0f
#define SETTINGS_BTN_Y 214.0f
#define SETTINGS_BTN_W 66.0f
#define SETTINGS_BTN_H 22.0f

static void ui_text(const char *text, float x, float y, float size, u32 color, float max_width)
{
	if (!text || !*text || !ui_text_buf) return;
	C2D_TextBufClear(ui_text_buf);
	C2D_Text parsed;
	if (!C2D_TextFontParse(&parsed, cpymo_backend_text_3ds_font(), ui_text_buf, text)) return;
	C2D_TextOptimize(&parsed);
	float scale = size / 28.0f;
	float width = 0;
	C2D_TextGetDimensions(&parsed, scale, scale, &width, NULL);
	float scale_x = scale;
	if (max_width > 0 && width > max_width) scale_x *= max_width / width;
	C2D_DrawText(&parsed, C2D_WithColor | C2D_AtBaseline, ui_x(x), y, 0, scale_x, scale, color);
}

static void panel_background(int width, int height)
{
	for (int y = 0; y < height; y += 12) {
		float t = (float)y / (float)height;
		u8 r = (u8)(13 + 7 * t), g = (u8)(27 + 11 * t), b = (u8)(45 + 18 * t);
		C2D_DrawRectSolid(ui_x(-8), y, 0, width + 16, 12, C2D_Color32(r, g, b, 255));
	}
}

/* Collects the button labels bound to `action` into out ("A / 下 / L"). */
static void collect_keys(cpymo_3ds_action action, char *out, size_t size)
{
	const cpymo_3ds_settings *s = cpymo_3ds_settings_get();
	out[0] = '\0';

	for (int k = 0; k < cpymo_3ds_key_count; ++k) {
		if (s->keys[k] != action) continue;
		const size_t used = strlen(out);
		if (used && used + 3 < size) strncat(out, " / ", size - used - 1);
		strncat(out, cpymo_3ds_key_label(k), size - strlen(out) - 1);
	}
}

int cpymo_3ds_ui_key_help_lines(char lines[][80], int max_lines)
{
	/* Order in which actions are listed; actions with no bound key are skipped
	 * and "undefined" is never shown. ZL/ZR are intentionally absent. */
	static const cpymo_3ds_action order[] = {
		cpymo_3ds_action_next, cpymo_3ds_action_auto, cpymo_3ds_action_skip,
		cpymo_3ds_action_hide, cpymo_3ds_action_history, cpymo_3ds_action_menu,
		cpymo_3ds_action_save, cpymo_3ds_action_load,
		cpymo_3ds_action_settings, cpymo_3ds_action_restart,
	};

	int count = 0;
	for (size_t i = 0; i < sizeof(order) / sizeof(order[0]) && count < max_lines; ++i) {
		char keys[48];
		collect_keys(order[i], keys, sizeof(keys));
		if (!keys[0]) continue;

		snprintf(lines[count], 80, "%s：%s%s",
			cpymo_3ds_action_label(order[i]), keys,
			order[i] == cpymo_3ds_action_hide ? "（按住）" : "");
		++count;
	}

	return count;
}

static void panel_header(int width)
{
	C2D_DrawRectSolid(ui_x(-8), 0, 0, width + 16, 32, COLOR_HEADER);
	C2D_DrawRectSolid(ui_x(-8), 31, 0, width + 16, 1, C2D_Color32(55, 108, 139, 255));
}

static void night_background(int width, int height)
{
	if (bottom_background) cpymo_3ds_draw_image_raw(bottom_background, ui_x(0), 0, width, height);
	else panel_background(width, height);
}

/* sdmc:/pymogames/bg.png, then bg.jpg, then the built-in night sky. A custom
 * image has to be 4:3 because it is stretched into 320x240 (bottom screen) and
 * 400x240 (top screen info panel). */
static cpymo_backend_image load_bottom_background(void)
{
	static const char *const candidates[] = {
		"sdmc:/pymogames/bg.png",
		"sdmc:/pymogames/bg.jpg",
		"romfs:/night-sky.png",
	};

	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
		const bool builtin = i == (sizeof(candidates) / sizeof(candidates[0])) - 1;
		int w = 0, h = 0;
		unsigned char *pixels = stbi_load(candidates[i], &w, &h, NULL, 3);
		if (pixels == NULL) {
			if (!builtin) printf("[CPyMO] %s: not readable, skipped.\n", candidates[i]);
			continue;
		}

		/* 4:3 (e.g. 320x240, 640x480) or the top screen size 400x240. */
		const bool valid_size = (w * 3 == h * 4) || (w == 400 && h == 240);
		if (!builtin && (w < 160 || h < 120 || !valid_size)) {
			printf("[CPyMO] %s: %dx%d ignored (need 4:3 or 400x240).\n",
				candidates[i], w, h);
			free(pixels);
			continue;
		}

		cpymo_backend_image image = NULL;
		/* cpymo_backend_image_load() takes ownership only when it succeeds. */
		if (cpymo_backend_image_load(&image, pixels, w, h,
				cpymo_backend_image_format_rgb) != CPYMO_ERR_SUCC) {
			free(pixels);
			continue;
		}

		printf("[CPyMO] Bottom screen background: %s (%dx%d).\n", candidates[i], w, h);
		return image;
	}

	return NULL;
}

void cpymo_3ds_ui_init(void)
{
	ui_text_buf = C2D_TextBufNew(4096);
	bottom_background = load_bottom_background();
}

void cpymo_3ds_ui_free(void)
{
	if (bottom_background) cpymo_backend_image_free(bottom_background);
	bottom_background = NULL;
	if (ui_text_buf) C2D_TextBufDelete(ui_text_buf);
	ui_text_buf = NULL;
}

void cpymo_3ds_ui_draw_clock(int width)
{
	time_t now = time(NULL);
	struct tm *local = localtime(&now);
	char clock[6] = "--:--";
	if (local && now >= 946684800) strftime(clock, sizeof(clock), "%H:%M", local);
	float x = width - 49;
	C2D_DrawRectSolid(ui_x(x - 5), 7, 0, 49, 19, C2D_Color32(12, 31, 51, 180));
	ui_text(clock, x, 22, 14, COLOR_WHITE, 42);
}

static void launcher_row(const cpymo_game_selector_item *item, float y, bool selected)
{
	C2D_DrawRectSolid(ui_x(8), y, 0, 304, 37, selected ? COLOR_SELECTED : C2D_Color32(20, 43, 66, 255));
	if (selected) C2D_DrawRectSolid(ui_x(8), y, 0, 3, 37, COLOR_ACCENT);
	if (item->icon) cpymo_3ds_draw_image_raw(item->icon, ui_x(15), y + 4, 29, 29);
	char title[256];
	strncpy(title, item->gametitle_text, sizeof(title) - 1);
	title[sizeof(title) - 1] = '\0';
	char *details = strchr(title, '\n');
	if (details) *details = '\0';
	ui_text(title, 53, y + 25, 15, selected ? COLOR_WHITE : COLOR_MUTED, 247);
}

void cpymo_3ds_ui_draw_launcher(const cpymo_game_selector_item *selected)
{
	/* This function draws the current physical screen: 400px top, 320px bottom. */
	extern const bool drawing_bottom_screen;
	if (!drawing_bottom_screen) {
		ui_layer = -3.0f;
		panel_background(400, 240);
		ui_layer = 5.0f;
		panel_header(400);
		ui_text("CPyMO for 3DS", 16, 23, 18, COLOR_WHITE, 260);
		ui_text("游戏资料库", 325, 22, 13, COLOR_ACCENT, 58);
		ui_layer = -3.0f;
		if (selected) {
			ui_text("当前选择", 127, 68, 14, COLOR_ACCENT, 150);
			ui_layer = 5.0f;
			if (selected->icon) cpymo_3ds_draw_image_raw(selected->icon, ui_x(20), 51, 88, 88);
			else {
				C2D_DrawRectSolid(ui_x(20), 51, 0, 88, 88, C2D_Color32(34, 68, 94, 255));
				ui_text("PyMO", 39, 104, 20, COLOR_MUTED, 62);
			}
			ui_text(selected->gametitle_text, 127, 108, 23, COLOR_WHITE, 250);
			ui_layer = -3.0f;
			ui_text(selected->gamedir, 20, 166, 13, COLOR_MUTED, 360);
			ui_text("已就绪，可以启动", 20, 201, 16, COLOR_ACCENT, 260);
			ui_text("A", 355, 201, 16, COLOR_WHITE, 25);
		}
		else ui_text("没有找到游戏，请将游戏放入 /pymogames/", 20, 112, 17, COLOR_MUTED, 360);
		ui_text("上下键浏览 · A 键启动", 20, 225, 12, COLOR_MUTED, 300);
	}
	else {
		ui_layer = 0;
		panel_background(320, 240);
		panel_header(320);
		ui_text("选择游戏", 14, 23, 17, COLOR_WHITE, 170);
		cpymo_3ds_ui_draw_clock(320);
		if (selected) {
			const cpymo_game_selector_item *first = selected;
			if (first->prev) first = first->prev;
			const cpymo_game_selector_item *item = first;
			for (int row = 0; row < 4 && item; ++row, item = item->next)
				launcher_row(item, 41 + row * 39, item == selected);
		}
		ui_text("A 启动 · 上下选择", 12, 218, 12, COLOR_ACCENT, 190);
		ui_text("START 退出 · SELECT 切换显示模式", 12, 234, 11, COLOR_MUTED, 232);

		/* Settings entry: text only, still tappable. */
		C2D_DrawRectSolid(ui_x(SETTINGS_BTN_X), SETTINGS_BTN_Y, 0,
			SETTINGS_BTN_W, SETTINGS_BTN_H, C2D_Color32(24, 55, 84, 220));
		ui_text("Y 设置", SETTINGS_BTN_X + 8, SETTINGS_BTN_Y + 16, 13, COLOR_WHITE, 56);
	}
	ui_layer = 0;
}

void cpymo_3ds_ui_draw_help(int width, int mode)
{
	ui_layer = 0;
	night_background(width, 240);
	C2D_DrawRectSolid(0, 0, 0, width, 240, C2D_Color32(7, 20, 39, 105));
	C2D_DrawRectSolid(0, 0, 0, width, 32, COLOR_HEADER);
	ui_text("CPyMO for 3DS", 14, 23, width == 400 ? 18 : 16, COLOR_WHITE, width - 85);
	cpymo_3ds_ui_draw_clock(width);
	const float x = width == 400 ? 22 : 16;
	const float size = width == 400 ? 15 : 12;
	const float step = width == 400 ? 21 : 19;

	/* The key list is generated from the current bindings, so remapping a
	 * button updates this help automatically. ZL/ZR are never listed (they
	 * mirror L/R). */
	char lines[8][80];
	const int count = cpymo_3ds_ui_key_help_lines(lines, 6);
	for (int i = 0; i < count; ++i)
		ui_text(lines[i], x, 54 + step * i, size, COLOR_WHITE, width - 32);

	ui_text("SELECT：切换显示模式", x, 54 + step * count, size, COLOR_ACCENT, width - 32);
	ui_text("START：返回游戏选择", x, 54 + step * (count + 1), size, COLOR_MUTED, width - 32);

	cpymo_3ds_ui_draw_progress();
}

void cpymo_3ds_ui_draw_progress(void)
{
	float progress = -1.0f;
	const char *label = NULL;
	bool voice = false;

	if (cpymo_audio_channel_is_playing(CPYMO_AUDIO_CHANNEL_VO, &engine.audio)) {
		progress = cpymo_audio_channel_progress(CPYMO_AUDIO_CHANNEL_VO, &engine.audio);
		label = "voice";
		voice = true;
	}

	if (progress < 0.0f) {
		progress = cpymo_say_auto_mode_progress(&engine);
		label = "wait";
		voice = false;
	}

	/* Nothing is playing and auto play is not waiting: keep the corner empty. */
	if (progress < 0.0f || label == NULL) return;

	const float w = 76, h = 6, x = 232, y = 194;

	C2D_DrawRectSolid(ui_x(x), y, 0, w, h, C2D_Color32(10, 26, 44, 200));

	if (progress > 0.0f) {
		float filled = (w - 2) * progress;
		if (filled < 1) filled = 1;
		C2D_DrawRectSolid(ui_x(x + 1), y + 1, 0, filled, h - 2,
			voice ? COLOR_ACCENT : COLOR_WHITE);
	}

	/* Small label under the bar, right aligned; same size as the hints. */
	ui_text(label, x + w - (voice ? 32.0f : 26.0f), y + 18, 12,
		voice ? COLOR_ACCENT : COLOR_MUTED, 34);
}

void cpymo_3ds_ui_draw_enhanced_background(void)
{
	ui_layer = 0;
	night_background(320, 240);
}

void cpymo_3ds_ui_draw_enhanced_hints(void)
{
	C2D_DrawRectSolid(0, 222, 0, 320, 18, C2D_Color32(8, 24, 45, 165));

	char keys[48];
	collect_keys(cpymo_3ds_action_next, keys, sizeof(keys));
	char line[96];
	if (keys[0]) snprintf(line, sizeof(line), "%s 继续 · SELECT 显示模式", keys);
	else snprintf(line, sizeof(line), "SELECT 显示模式");
	ui_text(line, 12, 235, 11, COLOR_WHITE, 250);

	cpymo_3ds_ui_draw_progress();

	cpymo_3ds_ui_draw_clock(320);
}

/* ------------------------------------------------------------------------ *
 * Global settings page (sdmc:/pymogames/settings.txt)
 * ------------------------------------------------------------------------ */

enum { settings_page_root = 0, settings_page_keys };

#define SETTINGS_ROOT_ITEMS 5
#define SETTINGS_ROOT_ITEM_KEYS 3
#define SETTINGS_ROOT_ITEM_RESET 4

static struct {
	bool open;
	int page;
	int cursor;
} settings_state;

bool cpymo_3ds_ui_settings_open(void) { return settings_state.open; }

void cpymo_3ds_ui_settings_enter(void)
{
	settings_state.open = true;
	settings_state.page = settings_page_root;
	settings_state.cursor = 0;
}

bool cpymo_3ds_ui_settings_hit(float x, float y)
{
	return x >= SETTINGS_BTN_X && x < SETTINGS_BTN_X + SETTINGS_BTN_W
		&& y >= SETTINGS_BTN_Y && y < SETTINGS_BTN_Y + SETTINGS_BTN_H;
}

static void settings_format_value(int item, char *out, size_t size)
{
	const cpymo_3ds_settings *s = cpymo_3ds_settings_get();

	switch (item) {
	case 0:
		if (s->display_mode < 0) snprintf(out, size, "跟随游戏");
		else snprintf(out, size, "模式 %d", s->display_mode + 1);
		break;
	case 1:
		if (s->fontsize <= 0) snprintf(out, size, "跟随游戏");
		else snprintf(out, size, "%d", s->fontsize);
		break;
	case 2:
		if (s->textspeed < 0) snprintf(out, size, "跟随游戏");
		else snprintf(out, size, "%d / 5", s->textspeed);
		break;
	default:
		out[0] = '\0';
		break;
	}
}

static void settings_change_value(int item, int dir)
{
	cpymo_3ds_settings *s = cpymo_3ds_settings_mutable();

	switch (item) {
	case 0: {
		/* -1 (follow the game) then modes 0..3 */
		const int values = 5;
		int index = s->display_mode + 1;
		if (index < 0) index = 0;
		if (index >= values) index = values - 1;
		index = (index + dir + values) % values;
		s->display_mode = index - 1;
		break;
	}
	case 1:
		if (s->fontsize <= 0) s->fontsize = dir > 0 ? 12 : 32;
		else {
			int value = s->fontsize + dir * 2;
			if (value < 12) value = 0;
			if (value > 32) value = 32;
			s->fontsize = value;
		}
		break;
	case 2:
		if (s->textspeed < 0) s->textspeed = dir > 0 ? 0 : 5;
		else {
			int value = s->textspeed + dir;
			if (value < 0) value = -1;
			if (value > 5) value = 5;
			s->textspeed = value;
		}
		break;
	default:
		break;
	}

	cpymo_3ds_settings_save();
}

static void settings_change_key(int key, int dir)
{
	cpymo_3ds_settings *s = cpymo_3ds_settings_mutable();
	int action = (int)s->keys[key] + dir;
	while (action < 0) action += cpymo_3ds_action_count;
	action %= cpymo_3ds_action_count;
	s->keys[key] = (cpymo_3ds_action)action;
	cpymo_3ds_settings_save();
}

/* Touch support for the settings page. It is only drawn on the bottom screen,
 * so raw touch coordinates map directly to the layout below. */
static void settings_touch(void)
{
	touchPosition touch;
	hidTouchRead(&touch);

	const float x = (float)touch.px;
	const float y = (float)touch.py;

	/* Bottom hint line doubles as a "back" button. */
	if (y >= 216) {
		if (settings_state.page == settings_page_keys) {
			settings_state.page = settings_page_root;
			settings_state.cursor = SETTINGS_ROOT_ITEM_KEYS;
		}
		else settings_state.open = false;
		return;
	}

	if (settings_state.page == settings_page_root) {
		if (y < 41) return;

		const int row = (int)((y - 41) / 33);
		if (row < 0 || row >= SETTINGS_ROOT_ITEMS) return;
		if (y - (41 + row * 33) > 29) return;

		settings_state.cursor = row;

		if (row <= 2) {
			/* Left half decreases, right half increases */
			settings_change_value(row, x < 160 ? -1 : 1);
		}
		else if (row == SETTINGS_ROOT_ITEM_KEYS) {
			settings_state.page = settings_page_keys;
			settings_state.cursor = 0;
		}
		else {
			cpymo_3ds_settings_set_defaults(cpymo_3ds_settings_mutable());
			cpymo_3ds_settings_save();
		}

		return;
	}

	/* Key binding page: two columns of five rows. Tap once to select, tap the
	 * selected cell again to cycle its action. */
	if (y < 40) return;

	const int row = (int)((y - 40) / 34);
	if (row < 0 || row > 4) return;
	if (y - (40 + row * 34) > 30) return;

	const int column = x < 162 ? 0 : 1;
	if (column == 0 && (x < 10 || x > 158)) return;
	if (column == 1 && (x < 166 || x > 314)) return;

	const int key = column * 5 + row;
	if (key >= cpymo_3ds_key_count) return;

	if (settings_state.cursor == key) settings_change_key(key, 1);
	else settings_state.cursor = key;
}

void cpymo_3ds_ui_settings_update(void)
{
	if (!settings_state.open) return;

	const u32 pressed = hidKeysDown();

	if (pressed & KEY_TOUCH) settings_touch();

	if (settings_state.page == settings_page_root) {
		if (pressed & KEY_UP)
			settings_state.cursor =
				(settings_state.cursor + SETTINGS_ROOT_ITEMS - 1) % SETTINGS_ROOT_ITEMS;
		if (pressed & KEY_DOWN)
			settings_state.cursor =
				(settings_state.cursor + 1) % SETTINGS_ROOT_ITEMS;

		if (pressed & KEY_LEFT) settings_change_value(settings_state.cursor, -1);
		if (pressed & KEY_RIGHT) settings_change_value(settings_state.cursor, 1);

		if (pressed & KEY_A) {
			if (settings_state.cursor == SETTINGS_ROOT_ITEM_KEYS) {
				settings_state.page = settings_page_keys;
				settings_state.cursor = 0;
			}
			else if (settings_state.cursor == SETTINGS_ROOT_ITEM_RESET) {
				cpymo_3ds_settings_set_defaults(cpymo_3ds_settings_mutable());
				cpymo_3ds_settings_save();
			}
			else settings_change_value(settings_state.cursor, 1);
		}

		if (pressed & KEY_B) settings_state.open = false;
	}
	else {
		if (pressed & KEY_UP)
			settings_state.cursor =
				(settings_state.cursor + cpymo_3ds_key_count - 1) % cpymo_3ds_key_count;
		if (pressed & KEY_DOWN)
			settings_state.cursor =
				(settings_state.cursor + 1) % cpymo_3ds_key_count;

		if (pressed & KEY_LEFT) settings_change_key(settings_state.cursor, -1);
		if (pressed & KEY_RIGHT) settings_change_key(settings_state.cursor, 1);
		if (pressed & KEY_A) settings_change_key(settings_state.cursor, 1);

		if (pressed & KEY_B) {
			settings_state.page = settings_page_root;
			settings_state.cursor = SETTINGS_ROOT_ITEM_KEYS;
		}
	}
}

static void settings_draw_top(void)
{
	/* Background sits far behind, the explanatory lines just above it, while
	 * the title and the key values float in front (3D parallax). */
	ui_layer = -5.0f;
	panel_background(400, 240);

	ui_layer = -1.5f;
	ui_text("全局设置保存在 sdmc:/pymogames/settings.txt", 20, 62, 13, COLOR_MUTED, 364);
	ui_text("游戏内设置过的项目（存档中的设置）优先，", 20, 86, 13, COLOR_MUTED, 364);
	ui_text("这里的值只作为未设置时的默认值。", 20, 108, 13, COLOR_MUTED, 364);

	ui_layer = 6.0f;
	C2D_DrawRectSolid(ui_x(-8), 0, 0, 416, 32, COLOR_HEADER);
	C2D_DrawRectSolid(ui_x(-8), 31, 0, 416, 1, C2D_Color32(55, 108, 139, 255));
	ui_text("CPyMO for 3DS · 设置", 16, 23, 18, COLOR_WHITE, 320);

	ui_layer = 3.0f;
	C2D_DrawRectSolid(ui_x(12), 132, 0, 376, 80, C2D_Color32(10, 26, 44, 190));
	C2D_DrawRectSolid(ui_x(12), 132, 0, 3, 80, COLOR_ACCENT);

	char line[80];
	char text[128];

	settings_format_value(0, line, sizeof(line));
	snprintf(text, sizeof(text), "默认显示模式：%s", line);
	ui_text(text, 26, 156, 15, COLOR_ACCENT, 350);

	settings_format_value(1, line, sizeof(line));
	snprintf(text, sizeof(text), "默认字体大小：%s", line);
	ui_text(text, 26, 180, 15, COLOR_ACCENT, 350);

	settings_format_value(2, line, sizeof(line));
	snprintf(text, sizeof(text), "默认播放速度：%s", line);
	ui_text(text, 26, 204, 15, COLOR_ACCENT, 350);

	cpymo_3ds_ui_draw_clock(400);
}

void cpymo_3ds_ui_settings_draw(bool bottom_screen)
{
	ui_layer = 0;

	if (!bottom_screen) {
		settings_draw_top();
		return;
	}

	panel_background(320, 240);
	panel_header(320);
	cpymo_3ds_ui_draw_clock(320);

	const cpymo_3ds_settings *s = cpymo_3ds_settings_get();
	char value[32];

	if (settings_state.page == settings_page_root) {
		static const char *const labels[SETTINGS_ROOT_ITEMS] = {
			"默认显示模式", "默认字体大小", "默认播放速度", "键位设置", "恢复默认设置",
		};

		ui_text("设置", 14, 23, 17, COLOR_WHITE, 170);

		for (int i = 0; i < SETTINGS_ROOT_ITEMS; ++i) {
			const float y = 41 + i * 33;
			const bool selected = i == settings_state.cursor;

			C2D_DrawRectSolid(ui_x(8), y, 0, 304, 29,
				selected ? COLOR_SELECTED : C2D_Color32(20, 43, 66, 255));
			if (selected)
				C2D_DrawRectSolid(ui_x(8), y, 0, 3, 29, COLOR_ACCENT);

			ui_text(labels[i], 20, y + 20, 14,
				selected ? COLOR_WHITE : COLOR_MUTED, 170);

			if (i <= 2) {
				settings_format_value(i, value, sizeof(value));
				ui_text(value, 205, y + 20, 13, COLOR_ACCENT, 100);
			}
		}

		ui_text("上下/触摸选择 · 左右改值 · A 确认 · B 返回", 12, 232, 11, COLOR_MUTED, 296);
	}
	else {
		ui_text("键位设置", 14, 23, 17, COLOR_WHITE, 200);

		for (int i = 0; i < cpymo_3ds_key_count; ++i) {
			const int column = i / 5;
			const float x = 10 + column * 156;
			const float y = 40 + (i % 5) * 34;
			const bool selected = i == settings_state.cursor;

			C2D_DrawRectSolid(ui_x(x), y, 0, 148, 30,
				selected ? COLOR_SELECTED : C2D_Color32(20, 43, 66, 255));
			if (selected)
				C2D_DrawRectSolid(ui_x(x), y, 0, 3, 30, COLOR_ACCENT);

			ui_text(cpymo_3ds_key_label(i), x + 10, y + 21, 14, COLOR_WHITE, 30);
			ui_text(cpymo_3ds_action_label(s->keys[i]), x + 46, y + 21, 12,
				selected ? COLOR_WHITE : COLOR_MUTED, 96);
		}

		ui_text("左右切换功能 · A 下一个 · 触摸选中/切换 · B 返回", 12, 232, 11, COLOR_MUTED, 296);
	}
}
