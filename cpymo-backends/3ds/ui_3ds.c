#include "ui_3ds.h"
#include "../include/cpymo_backend_image.h"
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern C2D_Font cpymo_backend_text_3ds_font(void);
extern void cpymo_3ds_draw_image_raw(cpymo_backend_image image, float x, float y, float w, float h);
extern unsigned char *stbi_load(char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);

static C2D_TextBuf ui_text_buf;
static cpymo_backend_image night_sky;
static float ui_layer;
extern float render_3d_offset;

static float ui_x(float x)
{
	return x + ui_layer * render_3d_offset;
}

#define COLOR_WHITE C2D_Color32(238, 244, 250, 255)
#define COLOR_MUTED C2D_Color32(158, 180, 202, 255)
#define COLOR_ACCENT C2D_Color32(91, 211, 232, 255)
#define COLOR_HEADER C2D_Color32(22, 49, 76, 255)
#define COLOR_SELECTED C2D_Color32(38, 91, 128, 255)

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

static void panel_header(int width)
{
	C2D_DrawRectSolid(ui_x(-8), 0, 0, width + 16, 32, COLOR_HEADER);
	C2D_DrawRectSolid(ui_x(-8), 31, 0, width + 16, 1, C2D_Color32(55, 108, 139, 255));
}

static void night_background(int width, int height)
{
	if (night_sky) cpymo_3ds_draw_image_raw(night_sky, ui_x(0), 0, width, height);
	else panel_background(width, height);
}

void cpymo_3ds_ui_init(void)
{
	ui_text_buf = C2D_TextBufNew(4096);
	int w = 0, h = 0;
	unsigned char *pixels = stbi_load("romfs:/night-sky.png", &w, &h, NULL, 3);
	if (pixels) {
		/* cpymo_backend_image_load() takes ownership only when it succeeds. */
		if (cpymo_backend_image_load(&night_sky, pixels, w, h, cpymo_backend_image_format_rgb)
				!= CPYMO_ERR_SUCC) {
			free(pixels);
			night_sky = NULL;
		}
	}
}

void cpymo_3ds_ui_free(void)
{
	if (night_sky) cpymo_backend_image_free(night_sky);
	night_sky = NULL;
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
		ui_text("A 启动 · 上下选择 · START 退出", 12, 211, 12, COLOR_ACCENT, 292);
		ui_text("进入游戏后 SELECT 切换显示模式", 12, 231, 11, COLOR_MUTED, 296);
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
	const float size = width == 400 ? 16 : 13;
	const float step = width == 400 ? 23 : 22;
	ui_text("A / Y / ZL / ZR / ↓：继续", x, 56, size, COLOR_WHITE, width - 32);
	ui_text("↑：历史对话", x, 56 + step, size, COLOR_WHITE, width - 32);
	ui_text("X / B：菜单 · 返回", x, 56 + step * 2, size, COLOR_WHITE, width - 32);
	ui_text("L 按住：隐藏文字", x, 56 + step * 3, size, COLOR_WHITE, width - 32);
	ui_text("R：快进", x, 56 + step * 4, size, COLOR_WHITE, width - 32);
	ui_text("SELECT：切换显示模式", x, 56 + step * 5, size, COLOR_ACCENT, width - 32);
	ui_text("START：返回游戏选择", x, 56 + step * 6, size, COLOR_MUTED, width - 32);
	ui_text("SELECT：切换画面比例与台词位置", x, 226, 12, COLOR_ACCENT, width - 32);
}

void cpymo_3ds_ui_draw_enhanced_background(void)
{
	ui_layer = 0;
	night_background(320, 240);
}

void cpymo_3ds_ui_draw_enhanced_hints(void)
{
	C2D_DrawRectSolid(0, 222, 0, 320, 18, C2D_Color32(8, 24, 45, 165));
	ui_text("A / ↓ 继续 · SELECT 显示模式", 12, 235, 11, COLOR_WHITE, 250);
	cpymo_3ds_ui_draw_clock(320);
}
