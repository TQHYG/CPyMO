#ifndef INCLUDE_CPYMO_RMENU
#define INCLUDE_CPYMO_RMENU

#include "cpymo_error.h"
#include <stdbool.h>

struct cpymo_engine;
error_t cpymo_rmenu_enter(struct cpymo_engine *);

/* Frees and re-initialises the engine with the current game directory. Used by
 * the menu and by the 3DS custom key bindings. */
error_t cpymo_rmenu_restart_game(struct cpymo_engine *e, void *data, bool confirm);

#endif
