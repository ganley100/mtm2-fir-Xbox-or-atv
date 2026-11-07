/*
 * xbox_main.c - the Xbox (UWP) side of the portable host.
 *
 * A UWP app starts in WinMain, and SDL's WinRT backend takes it from there:
 * SDL_WinRTRunApp makes the app's CoreWindow and calls SDL_main, which is
 * mtm_hle.c's main (SDL_main.h renames it).
 *
 * xbox_setup() is called from that main before anything else: where the
 * game is, where its writes go, where the log goes, and the hints an Xbox
 * app needs from SDL.
 */
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "xbox_main.h"

int __stdcall WinMain(void *inst, void *prev, char *cmdline, int show) {
    (void)inst; (void)prev; (void)cmdline; (void)show;
    return SDL_WinRTRunApp(SDL_main, NULL);
}

static int stat_ok(const char *p) { struct stat st; return stat(p, &st) == 0; }
static int has_exe(const char *dir) {
    char p[1200];
    struct stat st;
    snprintf(p, sizeof p, "%s/Monster.exe", dir);
    return stat(p, &st) == 0;
}

int xbox_setup(char *gamedir, size_t n, char *overlay, size_t on, char *fonts, size_t fn) {
    const char *local = SDL_WinRTGetFSPathUTF8(SDL_WINRT_PATH_LOCAL_FOLDER);         /* LocalState: ours to write */
    const char *install = SDL_WinRTGetFSPathUTF8(SDL_WINRT_PATH_INSTALLED_LOCATION); /* the package: read-only */
    overlay[0] = fonts[0] = 0;
    if (local) compat_set_start_dir(local);

    /* The log: no console here. The Device Portal's File explorer can fetch
     * it from LocalState. */
    char log[1200];
    snprintf(log, sizeof log, "%s/mtm.log", local ? local : ".");
    freopen(log, "w", stderr);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* B is the game's (ESC, by controller.ini), not "go back": without this
     * the system would close the app on it. */
    SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");

    char local_game[1100], install_game[1100];
    snprintf(local_game, sizeof local_game, "%s/game", local ? local : ".");
    snprintf(install_game, sizeof install_game, "%s/game", install ? install : ".");
    int found = 1;
    if (has_exe(local_game)) {
        /* Uploaded with the Device Portal: read and written in place. */
        snprintf(gamedir, n, "%s", local_game);
    } else if (has_exe(install_game)) {
        /* Built into the package (xbox/package.ps1 -Game): read from there,
         * written to LocalState\game over it. */
        snprintf(gamedir, n, "%s", install_game);
        mkdir(local_game, 0755);
        if (!realpath(local_game, overlay)) overlay[0] = 0;
    } else {
        snprintf(gamedir, n, "%s", local_game);
        found = 0;
    }
    /* Fonts for the front end's text: the game folder's own fonts/, else the
     * Liberation fonts the package carries. Given to SDL_ttf as they are, so
     * in Windows form. */
    char f[1200];
    snprintf(f, sizeof f, "%s/fonts/LiberationSans-Regular.ttf", gamedir);
    snprintf(f, sizeof f, "%s/fonts/", stat_ok(f) ? gamedir : (install ? install : "."));
    compat_native(f, fonts, fn);
    size_t l = strlen(fonts);
    if (l && fonts[l - 1] != '/' && l + 1 < fn) fonts[l] = '/', fonts[l + 1] = 0;
    fprintf(stderr, "[xbox] LocalState %s\n[xbox] package %s\n[xbox] game %s%s%s\n[xbox] fonts %s\n",
            local ? local : "?", install ? install : "?", gamedir,
            overlay[0] ? ", writes to " : "", overlay, fonts);
    return found;
}
