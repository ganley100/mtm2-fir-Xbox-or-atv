/* The Xbox (UWP) side of the portable host: xbox_main.c. */
#pragma once
#include <stddef.h>

/* Where the game is: LocalState\game if Monster.exe was uploaded there, else
 * the package's own game\ (then *overlay gets LocalState\game, where its
 * writes go; empty otherwise). *fonts gets the folder of the Liberation
 * fonts, in Windows form for SDL_ttf. Also opens the log and sets SDL's
 * hints. Returns 0 if neither folder has Monster.exe (gamedir is then the
 * LocalState one, for the message). */
int xbox_setup(char *gamedir, size_t n, char *overlay, size_t on, char *fonts, size_t fn);
