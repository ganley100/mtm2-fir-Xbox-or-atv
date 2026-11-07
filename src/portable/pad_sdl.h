/* Controllers and the TV remote on the portable host: pad_sdl.c. */
#pragma once

/* The mapping (ini, or the built-in one when NULL or missing) and SDL's game
 * controllers. 0 if SDL has no controller support. */
int  pad_sdl_start(const char* ini);
/* Read the pad and send the game what changed. Call on the game's thread. */
void pad_sdl_poll(void);
/* screen.c's key hook: the remote's keys the game has no use for as they
 * are (SELECT, AC_BACK). Nonzero: taken. */
int  pad_sdl_key(int sdl_scancode, int down);
