#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM

#include "../../cpymo/cpymo_prelude.h"
#include "../../cpymo/cpymo_engine.h"
#include "../../cpymo/cpymo_game_selector.h"
#include "../../cpymo/cpymo_list_ui.h"
#include <3ds.h>
#include <stdio.h>
#include <citro3d.h>
#include <citro2d.h>
#include <stdbool.h>
#include <libavutil/log.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../stb/stb_image_write.h"

#define STB_DS_IMPLEMENTATION
#include "../../stb/stb_ds.h"

#include "../include/cpymo_backend_text.h"
#include "../include/cpymo_backend_save.h"
#include "../../cpymo/cpymo_save_ui.h"
#include "../../cpymo/cpymo_config_ui.h"
#include "../../cpymo/cpymo_rmenu.h"
#include "settings_3ds.h"
#include "ui_3ds.h"

cpymo_engine engine;
C3D_RenderTarget *screen1, *screen2, *screen3 = NULL;
float render_3d_offset;
bool fill_screen;

extern void cpymo_backend_image_init(float, float);

extern error_t cpymo_backend_text_sys_init();
extern void cpymo_backend_text_sys_free();

bool enhanced_3ds_display_mode = false;
bool drawing_bottom_screen;
int display_mode = 1; /* bit 0: dialogue on bottom; bit 1: aspect-fit instead of crop-fill. */

/* Debug mode: the CPyMO console owns the bottom screen (hold L while choosing
 * a game). While it is enabled no bottom screen UI is drawn at all and START
 * is disabled. libctru has no consoleExit(), so it lasts until the process
 * ends. */
bool cpymo_3ds_debug_mode = false;

/* cpymo_engine_free() may only be called while this is true. Some engine init
 * helpers release the engine themselves when they fail, so the exit path needs
 * to know whether the engine is still owned by main(). */
static bool engine_alive = false;

bool enhanced_3ds_display_mode_touch_ui_enabled(void)
{
	if (!enhanced_3ds_display_mode) return false;
	if (engine.ui) return true;
	return false;
}

float enhanced_3ds_bottom_yoffset()
{
	if (enhanced_3ds_display_mode_touch_ui_enabled()) return 0;
	else {
		float msg_h = 0;
		
		if (engine.say.msgbox && engine.say.namebox) {
			float ratio = (float)engine.gameconfig.imagesize_w / (float)engine.say.msgbox_w;
			msg_h = (float)engine.say.msgbox_h * ratio;
		}
		else {
			msg_h = engine.gameconfig.imagesize_h * 0.25f;
		}
		
		float y = (float)engine.gameconfig.imagesize_h - msg_h;
		float fontsize = cpymo_gameconfig_font_size(&engine.gameconfig);
		float namebox_h = fontsize * 1.4f;
		float namebox_y = y - namebox_h;

		return -namebox_y + 16;		
	}
}

static void ensure_save_dir(const char *gamedir)
{
	if (R_FAILED(fsInit())) return;

	char *path = (char *)alloca(strlen(gamedir) + 8);
	strcpy(path, gamedir);
	strcat(path, "/save");

	FS_Archive archive;
	Result error = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC_WRITE_ONLY, fsMakePath(PATH_EMPTY, ""));
	if (R_FAILED(error)) {
		fsExit();
		return;
	}

	FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, path), FS_ATTRIBUTE_DIRECTORY);
	FSUSER_CloseArchive(archive);
	fsExit();
}

static void save_screen_mode()
{
	/* Never touch the game's display mode file in debug mode: the mode is
	 * forced there and the user's choice must survive. */
	if (cpymo_3ds_debug_mode) return;
	if (engine.assetloader.gamedir == NULL) return;
	FILE *sav = cpymo_backend_write_save(engine.assetloader.gamedir, "3ds-display-mode.csav");

	if (sav) {
		uint8_t save = 0x80 | (uint8_t)display_mode;
		fwrite(&save, sizeof(save), 1, sav);
		fclose(sav);
	}
}

static void load_screen_mode(const char *gamedir)
{
	/* Fall back to the global default display mode until a per-game mode is
	 * found. */
	display_mode = cpymo_3ds_settings_get()->display_mode;
	if (display_mode < 0 || display_mode > 3) display_mode = 1;

	FILE *sav = cpymo_backend_read_save(gamedir, "3ds-display-mode.csav");

	if (sav) {
		uint8_t save;

		if (fread(&save, sizeof(save), 1, sav) == 1) {
			if (save & 0x80) display_mode = save & 3;
			else {
				/* Old format: bit0 = enhanced (dialogue on the bottom screen),
				 * bit1 = fill screen. New format: bit0 = enhanced,
				 * bit1 = aspect-fit (i.e. NOT fill screen). Translate them
				 * instead of swapping the two bits. */
				const int old_enhanced = save & 1;
				const int old_fill = (save >> 1) & 1;
				display_mode = old_enhanced | (old_fill ? 0 : 2);
			}
			enhanced_3ds_display_mode = (display_mode & 1) != 0;
			fill_screen = (display_mode & 2) == 0;
		}

		fclose(sav);
	}
}

static cpymo_game_selector_item *load_game_list()
{
	if (R_FAILED(fsInit())) return NULL;

	FS_Archive archive;
	Result err = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));

	if (R_FAILED(err)) {
		fsExit();
		return NULL;
	}

	Handle handle;
	err = FSUSER_OpenDirectory(&handle, archive, fsMakePath(PATH_ASCII, "/pymogames/"));
	if (R_FAILED(err)) {
		FSUSER_CloseArchive(archive);
		fsExit();
		return NULL;
	}

	cpymo_game_selector_item *items = NULL, *tail = NULL;

	u32 result = 0;
	do {
		FS_DirectoryEntry item;
		err = FSDIR_Read(handle, &result, 1, &item);

		if (R_FAILED(err) || result != 1 || (item.attributes & FS_ATTRIBUTE_DIRECTORY) == 0) 
			continue;

		size_t name_len = 0;
		for (size_t i = 0; i < sizeof(item.name) / sizeof(item.name[0]); ++i) {
			if (item.name[i] == 0) break;
			++name_len;
		}

		char *path = (char *)malloc(name_len + 16);
		if (path == NULL) continue;

		strcpy(path, "/pymogames/");
		for (size_t i = 0; i < name_len; ++i)
			path[i + 11] = (char)item.name[i];
		path[name_len + 11] = '\0';

		cpymo_game_selector_item *cur = NULL;
		error_t err = cpymo_game_selector_item_create(&cur, &path);
		if (err != CPYMO_ERR_SUCC) {
			free(path);
			continue;
		}

		if (items == NULL) {
			items = cur;
			tail = cur;
		}
		else {
			tail->next = cur;
			tail = cur;
		}
	} while(result);

	FSDIR_Close(handle);
	FSUSER_CloseArchive(archive);
	fsExit();

	return items;
}

static char *get_last_selected_game_dir()
{
	char *str = NULL;
	size_t len;
	error_t err = cpymo_utils_loadfile("/pymogames/last_game.txt", &str, &len);

	if (err != CPYMO_ERR_SUCC) return NULL;

	char *ret = realloc(str, len + 1);
	if (ret == NULL) {
		free(str);
		return NULL;
	}

	ret[len] = '\0';
	return ret;
}

static error_t before_select_game(cpymo_engine *e, const char *gamedir)
{
	size_t len = strlen(gamedir);
	FILE *f = fopen("/pymogames/last_game.txt", "wb");
	if (f) {
		fwrite(gamedir, len, 1, f);
		fclose(f);
	}

	ensure_save_dir(gamedir);

	load_screen_mode(gamedir);

	/* Hold L while choosing a game to start in debug mode: the bottom screen
	 * is handed over to the CPyMO console. */
	if (!cpymo_3ds_debug_mode && (hidKeysHeld() & KEY_L)) {
		if (screen3) {
			C3D_RenderTargetDelete(screen3);
			screen3 = NULL;
		}

		consoleInit(GFX_BOTTOM, NULL);
		gfxSetDoubleBuffering(GFX_BOTTOM, false);
		cpymo_3ds_debug_mode = true;
		printf("[CPyMO] Debug mode: the CPyMO console owns the bottom screen.\n");
	}

	if (cpymo_3ds_debug_mode) {
		display_mode = 0;
		enhanced_3ds_display_mode = false;
		fill_screen = false;
		return CPYMO_ERR_SUCC;
	}

	enhanced_3ds_display_mode = (display_mode & 1) != 0;
	fill_screen = (display_mode & 2) == 0;

	return CPYMO_ERR_SUCC;
}

static error_t after_select_game(cpymo_engine *e, const char *gamedir)
{
	/* Global settings only fill in what this game does not define itself: as
	 * soon as the player changed the in-game settings (config.csav exists) the
	 * game's own values win. Nothing is written into the game's save dir. */
	const cpymo_3ds_settings *s = cpymo_3ds_settings_get();
	bool has_own_config = false;
	FILE *own = cpymo_backend_read_save(gamedir, "config.csav");
	if (own) {
		has_own_config = true;
		fclose(own);
	}

	if (!has_own_config) {
		if (s->fontsize > 0) e->gameconfig.fontsize = (uint16_t)s->fontsize;
		if (s->textspeed >= 0) e->gameconfig.textspeed = (unsigned)s->textspeed;
	}

	cpymo_backend_image_init(e->gameconfig.imagesize_w, e->gameconfig.imagesize_h);
	return CPYMO_ERR_SUCC;
}

/* Actions that cannot be expressed through cpymo_input (see
 * cpymo_backend_input.c) are turned into engine calls here. Only a running
 * game without an open menu is affected, so menus and the selector keep their
 * upstream behaviour. */
static void cpymo_3ds_dispatch_actions(void)
{
	if (engine.ui) return;
	if (engine.assetloader.gamedir == NULL) return;

	if (cpymo_3ds_settings_action_pressed(cpymo_3ds_action_auto)) {
		if (engine.say.auto_mode) cpymo_say_stop_auto_mode(&engine);
		else {
			engine.say.auto_mode = true;
			engine.say.auto_mode_timer = -1;
			cpymo_engine_request_redraw(&engine);
		}
	}

	if (cpymo_3ds_settings_action_pressed(cpymo_3ds_action_save)) {
		error_t err = cpymo_save_ui_enter(&engine, false);
		if (err != CPYMO_ERR_SUCC)
			printf("[Error] open save menu: %s\n", cpymo_error_message(err));
	}

	if (cpymo_3ds_settings_action_pressed(cpymo_3ds_action_load)) {
		error_t err = cpymo_save_ui_enter(&engine, true);
		if (err != CPYMO_ERR_SUCC)
			printf("[Error] open load menu: %s\n", cpymo_error_message(err));
	}

	if (cpymo_3ds_settings_action_pressed(cpymo_3ds_action_settings)) {
		error_t err = cpymo_config_ui_enter(&engine);
		if (err != CPYMO_ERR_SUCC)
			printf("[Error] open settings: %s\n", cpymo_error_message(err));
	}

	if (cpymo_3ds_settings_action_pressed(cpymo_3ds_action_restart)) {
		error_t err = cpymo_rmenu_restart_game(&engine, NULL, true);
		if (err != CPYMO_ERR_SUCC)
			printf("[Error] restart game: %s\n", cpymo_error_message(err));
		else {
			load_screen_mode(engine.assetloader.gamedir);
			enhanced_3ds_display_mode = (display_mode & 1) != 0;
			fill_screen = (display_mode & 2) == 0;
			cpymo_backend_image_init(400, 240);
		}
	}
}

static error_t return_to_game_selector(cpymo_game_selector_item **selector_items)
{
	char *last_selected = NULL;
	if (engine.assetloader.gamedir)
		last_selected = cpymo_str_copy_malloc(cpymo_str_pure(engine.assetloader.gamedir));

	save_screen_mode();
	cpymo_engine_free(&engine);
	engine_alive = false;
	cpymo_backend_image_init(400, 240);

	cpymo_game_selector_item *items = load_game_list();
	*selector_items = items;
	error_t err = cpymo_engine_init_with_game_selector(
		&engine, 400, 240, 22, 20, 4, &items,
		&before_select_game, &after_select_game, &last_selected);

	if (err == CPYMO_ERR_SUCC) {
		engine_alive = true;
	}
	else {
		/* On failure the init helper already released the engine; the game list
		 * is only taken over on success, so it still belongs to us here. */
		if (items) cpymo_game_selector_item_free_all(items);
		if (last_selected) free(last_selected);
	}

	return err;
}

int main(void) {
	srand((unsigned)time(NULL));

	/* Keep the fs service (and the sdmc devoptab) alive for the whole process:
	 * settings.txt and the optional background image are read through fopen(). */
	fsInit();

	/* sdmc:/pymogames/settings.txt - global frontend settings, never part of a
	 * game save. */
	cpymo_3ds_settings_load();

	engine.ui = NULL;
	engine.say.active = false;
	bool is_new_3ds = false;
	APT_CheckNew3DS(&is_new_3ds);

	av_log_set_level(AV_LOG_ERROR);
	
	fill_screen = true;
	enhanced_3ds_display_mode = true;
	
	gfxInitDefault();

	gfxSet3D(true);

	if(is_new_3ds) {
		osSetSpeedupEnable(true);
	}

	engine.audio.enabled = false;

	if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
		gfxExit();
		return 0;
	}

	if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
		C3D_Fini();
		gfxExit();
		return 0;
	}

	C2D_Prepare();

	screen1 = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	if(screen1 == NULL) {
		C2D_Fini();
		C3D_Fini();
		gfxExit();
		return 0;
	}

	screen2 = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
	if(screen2 == NULL) {
		C3D_RenderTargetDelete(screen1);
		C2D_Fini();
		C3D_Fini();
		gfxExit();
		return 0;
	}

	screen3 = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	if(screen3 == NULL) {
		C3D_RenderTargetDelete(screen2);
		C3D_RenderTargetDelete(screen1);
		C2D_Fini();
		C3D_Fini();
		gfxExit();
		return 0;
	}

	if(cpymo_backend_text_sys_init() != CPYMO_ERR_SUCC) {
		C3D_RenderTargetDelete(screen3);
		C3D_RenderTargetDelete(screen1);
		C3D_RenderTargetDelete(screen2);
		C2D_Fini();
		C3D_Fini();
		gfxExit();
		return 0;
	}

	cpymo_3ds_ui_init();
	cpymo_backend_image_init(400, 240);

	extern void cpymo_backend_audio_init();
	cpymo_backend_audio_init();
	
	cpymo_game_selector_item *item = load_game_list();
	cpymo_game_selector_item *selector_items = item;
	char *last_select_game = get_last_selected_game_dir();
	error_t err = cpymo_engine_init_with_game_selector(
		&engine, 400, 240, 22, 20, 4, &item, &before_select_game, &after_select_game, &last_select_game);
	
	if (err != CPYMO_ERR_SUCC) {
		printf("[Error] cpymo_engine_init_with_game_selector: %s.\n", cpymo_error_message(err));
		if (last_select_game) free(last_select_game);
		cpymo_game_selector_item_free_all(item);
		cpymo_backend_text_sys_free();
		if (screen3) C3D_RenderTargetDelete(screen3);
		C3D_RenderTargetDelete(screen1);
		C3D_RenderTargetDelete(screen2);
		C2D_Fini();
		C3D_Fini();
		gfxExit();
		return 0;
	}

	engine_alive = true;

	const u32 clr = C2D_Color32(0, 0, 0, 0);

	float prevSlider = NAN;
	TickCounter tickCounter;
	osTickCounterStart(&tickCounter);

	void cpymo_backend_audio_unlock(void);
	cpymo_backend_audio_unlock();

	while (aptMainLoop()) {
		if(aptShouldClose()) break;

		hidScanInput();

		cpymo_3ds_dispatch_actions();

		bool redraw = false;
		bool settings_open = cpymo_3ds_ui_settings_open();

		/* Settings page: only reachable from the game selector. While it is
		 * open it replaces both screens and pauses the selector. */
		if (settings_open) {
			cpymo_3ds_ui_settings_update();
			redraw = true;
		}
		else if (engine.assetloader.gamedir == NULL && engine.ui) {
			bool open_settings = (hidKeysDown() & KEY_Y) != 0;

			if (hidKeysDown() & KEY_TOUCH) {
				touchPosition touch;
				hidTouchRead(&touch);
				if (cpymo_3ds_ui_settings_hit((float)touch.px, (float)touch.py))
					open_settings = true;
			}

			if (open_settings) {
				cpymo_3ds_ui_settings_enter();
				/* Y is also the selector's "ok" button, so the selector must not
				 * see this frame's input. */
				settings_open = true;
				redraw = true;
			}
		}

		osTickCounterUpdate(&tickCounter);
		double deltaTime = osTickCounterRead(&tickCounter) / 1000.0;

		/* The engine is frozen while the settings page is open so that the
		 * selector does not move behind it. */
		if (!settings_open) {
			err = cpymo_engine_update(&engine, (float)deltaTime, &redraw);
			switch(err) {
			case CPYMO_ERR_NO_MORE_CONTENT:
				if (engine.assetloader.gamedir) {
					err = return_to_game_selector(&selector_items);
					if (err != CPYMO_ERR_SUCC) goto EXIT;
					redraw = true;
					continue;
				}
				goto EXIT;
			case CPYMO_ERR_SUCC: break;
			default: {
				printf("[Error] %s(%d).\n", cpymo_error_message(err), err);
				while(aptMainLoop()) {
					hidScanInput();
					gspWaitForVBlank();

					u32 keys = hidKeysHeld();
					if((keys & KEY_ZL)) goto EXIT;
				}
			}
			}
		}

		float slider = osGet3DSliderState();
		if(slider != prevSlider) {
			redraw = true;
			prevSlider = slider;
		}

		if (hidKeysDown() & KEY_SELECT && engine.assetloader.gamedir && !cpymo_3ds_debug_mode) {
			redraw = true;
			display_mode = (display_mode + 1) & 3;
			enhanced_3ds_display_mode = (display_mode & 1) != 0;
			fill_screen = (display_mode & 2) == 0;
			save_screen_mode();
		}

		if(redraw || true) {
			drawing_bottom_screen = false;
			bool selecting = engine.assetloader.gamedir == NULL;
			const bool settings_page = cpymo_3ds_ui_settings_open();
			const cpymo_game_selector_item *selected = NULL;
			if (selecting && selector_items && selector_items->gametitle)
				selected = cpymo_list_ui_get_current_selected_const(&engine);

			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C2D_TargetClear(screen1, clr);
			C2D_SceneBegin(screen1);
			render_3d_offset = slider;

			if (settings_page) cpymo_3ds_ui_settings_draw(false);
			else if (selecting) {
				cpymo_engine_draw(&engine);
				cpymo_3ds_ui_draw_launcher(selected);
			}
			else cpymo_engine_draw(&engine);

			if(slider > 0){
				C2D_TargetClear(screen2, clr);
				C2D_SceneBegin(screen2);
				render_3d_offset = -slider;
				if (settings_page) cpymo_3ds_ui_settings_draw(false);
				else if (selecting) cpymo_3ds_ui_draw_launcher(selected);
				else cpymo_engine_draw(&engine);
			}

			/* The bottom screen is skipped entirely when the CPyMO console owns it
			 * (debug mode) or when the render target could not be created. */
			if (screen3) {
				C2D_SceneBegin(screen3);
				C2D_TargetClear(screen3, clr);
				render_3d_offset = 0;
				if (settings_page) {
					drawing_bottom_screen = true;
					cpymo_3ds_ui_settings_draw(true);
				}
				else if (selecting) {
					drawing_bottom_screen = true;
					cpymo_3ds_ui_draw_launcher(selected);
				}
				else if ((display_mode & 1) == 0) {
					drawing_bottom_screen = true;
					cpymo_3ds_ui_draw_help(320, display_mode);
				}
				else {
					drawing_bottom_screen = true;
					cpymo_3ds_ui_draw_enhanced_background();
					cpymo_engine_draw(&engine);
					cpymo_3ds_ui_draw_enhanced_hints();
				}
				drawing_bottom_screen = false;
			}

			C3D_FrameEnd(0);
		} else {
			gspWaitForVBlank();
		}
	}

	EXIT:

	save_screen_mode();

	cpymo_3ds_ui_free();
	cpymo_backend_text_sys_free();
	if(screen3) C3D_RenderTargetDelete(screen3);
	C3D_RenderTargetDelete(screen2);
	C3D_RenderTargetDelete(screen1);
	if (engine_alive) cpymo_engine_free(&engine);

	extern void cpymo_backend_audio_free();
	cpymo_backend_audio_free();
	
	C2D_Fini();
	C3D_Fini();

	gfxExit();

	return 0;
}

