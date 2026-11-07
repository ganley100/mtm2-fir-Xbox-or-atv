/*
 * Monster Truck Madness 2 - the portable host (Android TV, Linux).
 *
 * The Windows host (../runtime/host.c) runs the lifted game on pcrecomp's
 * native32, which hands every import to the real Windows. This one runs the
 * same lift on pcrecomp's win32hle, which answers the imports itself on libc
 * and SDL2: windows, GDI, DirectDraw and DirectSound are win32hle's, the
 * screen and input are SDL's. What is here is only MTM2's own:
 *
 *   - where the game is (on Android, the app's folder on shared storage),
 *   - what the game asks of DLLs win32hle does not have: Smacker (the intro
 *     videos are skipped), DirectInput (created, never given a device: the
 *     race reads the keyboard), ChangeDisplaySettingsA (fetched with
 *     GetProcAddress; the screen is SDL's, so it is only logged),
 *   - the controller and the TV remote (pad_sdl.c).
 *
 * Anything else the game imports that win32hle does not answer is bound to a
 * stub that names itself if it is ever called, so a gap shows as one line in
 * the log: "[hle] unimplemented: <name>".
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>

#include "win32hle.h"
#include "pe_loader.h"
#include "host.h"
#include "pad_sdl.h"
#ifdef MTM_XBOX
#include "xbox_main.h"
#endif

/* The guest's 32-bit world: these are Win32's values, by number. */
#define DISP_CHANGE_SUCCESSFUL 0
#define E_NOINTERFACE          0x80004002u
#define DIERR_DEVICENOTREG     0x80040154u
#define DIERR_INVALIDPARAM     0x80070057u

#ifndef MTM_GAME_DIR
#define MTM_GAME_DIR "game"                 /* game-demo for the demo (CMakeLists.txt) */
#endif
#ifndef MTM_TITLE
#define MTM_TITLE "Monster Truck Madness 2"
#endif

/* gdidc.c looks here first for a font (CMakeLists.txt adds it to the search
 * on Android, which has no fontconfig and no Liberation fonts): the game
 * folder's fonts/, where LiberationSans-Regular.ttf and friends can go. */
char mtm_font_dir[1024];

static void log_line(const char* s) {
    fprintf(stderr, "%s\n", s);
#ifdef __ANDROID__
    SDL_Log("%s", s);
#endif
}
#define LOG(...) do { char _b[1024]; snprintf(_b, sizeof _b, __VA_ARGS__); log_line(_b); } while (0)

/* ------------------------------------------------------- display mode */

static void mtm_ChangeDisplaySettingsA(void) {          /* (DEVMODEA*, flags) */
    uint32_t dm = A32(0);
    if (dm) LOG("[display] ChangeDisplaySettingsA(%ux%ux%u): not applied", MEM32(dm + 108), MEM32(dm + 112), MEM32(dm + 104));
    RET(DISP_CHANGE_SUCCESSFUL, 2);
}

/* ------------------------------------------------------- DirectInput */

/* IDirectInputA: an object whose only use is to exist. Every method takes
 * `this` first (stdcall), so the counts below include it. */
static void di_QueryInterface(void) { if (A32(2)) MEM32(A32(2)) = 0; RET(E_NOINTERFACE, 3); }
static void di_AddRef(void)         { RET(1, 1); }
static void di_Release(void)        { RET(0, 1); }
static void di_CreateDevice(void)   { if (A32(2)) MEM32(A32(2)) = 0; RET(DIERR_DEVICENOTREG, 4); }
static void di_EnumDevices(void)    { RET(0, 5); }      /* DI_OK, and no device to call back about */
static void di_GetDeviceStatus(void){ RET(DIERR_DEVICENOTREG, 2); }
static void di_RunControlPanel(void){ RET(0, 3); }
static void di_Initialize(void)     { RET(0, 3); }

static const win32hle_shim g_di_methods[] = {
    { "mtm.IDirectInputA.QueryInterface", di_QueryInterface },
    { "mtm.IDirectInputA.AddRef", di_AddRef },
    { "mtm.IDirectInputA.Release", di_Release },
    { "mtm.IDirectInputA.CreateDevice", di_CreateDevice },
    { "mtm.IDirectInputA.EnumDevices", di_EnumDevices },
    { "mtm.IDirectInputA.GetDeviceStatus", di_GetDeviceStatus },
    { "mtm.IDirectInputA.RunControlPanel", di_RunControlPanel },
    { "mtm.IDirectInputA.Initialize", di_Initialize },
    { NULL, NULL },
};

static void mtm_DirectInputCreateA(void) {              /* (hinst, version, IDirectInputA**, outer) */
    static uint32_t obj;
    if (!obj) {
        uint32_t* vt = (uint32_t*)hle_heap_alloc(8 * 4, 1);
        uint32_t* o = (uint32_t*)hle_heap_alloc(4, 1);
        for (int i = 0; i < 8; i++) vt[i] = hle_resolve(g_di_methods[i].name);
        o[0] = hle_va(vt);
        obj = hle_va(o);
    }
    LOG("[dinput] DirectInputCreateA(version 0x%x): an object with no devices", A32(1));
    if (!A32(2)) RET(DIERR_INVALIDPARAM, 4);
    MEM32(A32(2)) = obj;
    RET(0, 4);
}

static const win32hle_shim g_mtm_shims[] = {
    { "ChangeDisplaySettingsA", mtm_ChangeDisplaySettingsA },
    { "DirectInputCreateA", mtm_DirectInputCreateA },
    { NULL, NULL },
};

/* ------------------------------------------------------- decorated stubs */

/* smackw32.dll exports stdcall names with their argument bytes in them
 * ("_SmackOpen@12"), so a stub can pop the right count without knowing the
 * call. Each returns 0: SmackOpen's 0 is "no such video", and the game goes
 * on without the intros. One function per argument count. */
#define MTM_STUB(n) static void stub##n(void) { RET(0, n); }
MTM_STUB(0) MTM_STUB(1) MTM_STUB(2) MTM_STUB(3) MTM_STUB(4) MTM_STUB(5) MTM_STUB(6) MTM_STUB(7) MTM_STUB(8)
MTM_STUB(9) MTM_STUB(10) MTM_STUB(11) MTM_STUB(12) MTM_STUB(13) MTM_STUB(14) MTM_STUB(15) MTM_STUB(16)
static const recomp_func_t g_stub[17] = {
    stub0, stub1, stub2, stub3, stub4, stub5, stub6, stub7, stub8,
    stub9, stub10, stub11, stub12, stub13, stub14, stub15, stub16,
};

static uint32_t resolve(const char* name) {
    uint32_t va = hle_resolve(name);
    if (va) return va;
    const char* at = name[0] == '_' ? strrchr(name, '@') : NULL;
    if (at && at[1] >= '0' && at[1] <= '9') {
        int bytes = atoi(at + 1);
        if (bytes % 4 == 0 && bytes / 4 <= 16) {
            win32hle_shim* one = (win32hle_shim*)calloc(2, sizeof *one);
            one[0].name = strdup(name);
            one[0].fn = g_stub[bytes / 4];
            win32hle_register(one);
            LOG("[mtm] %s: a stub returning 0", name);
            return hle_resolve(name);
        }
    }
    return hle_resolve_or_stub(name);
}

/* ------------------------------------------------------- the pump */

static void pump(void) {
    hle_screen_pump();
    pad_sdl_poll();
}

/* MFC's front end uses the Windows arrow: show the cursor the guest set. */
static int show_cursor(void) { return 1; }

/* ------------------------------------------------------- start */

static int exists(const char* p) { struct stat st; return stat(p, &st) == 0; }

static void fail(const char* title, const char* msg) {
    LOG("%s: %s", title, msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, msg, NULL);
}

int main(int argc, char** argv) {
    const char *game = NULL, *pad_ini = NULL;
    int headless = 0, fullscreen = 0, pad = 1;
#ifdef __ANDROID__
    fullscreen = 1;
#endif
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--game") && i + 1 < argc) game = argv[++i];
        else if (!strcmp(argv[i], "--headless")) headless = 1;
        else if (!strcmp(argv[i], "--fullscreen")) fullscreen = 1;
        else if (!strcmp(argv[i], "--windowed")) fullscreen = 0;
        else if (!strcmp(argv[i], "--pad") && i + 1 < argc) pad_ini = argv[++i];
        else if (!strcmp(argv[i], "--no-pad")) pad = 0;
        else {
            printf("usage: mtm [--game DIR] [--fullscreen|--windowed] [--headless]\n"
                   "           [--pad controller.ini] [--no-pad]\n");
            return argv[i][1] == 'h' || argv[i][2] == 'h' ? 0 : 64;
        }
    }

    if (recomp_dispatch_count == 0) {
        fail(MTM_TITLE,
             "This build has no lifted code (MTM_NO_LIFT). Run tools/catalog.py and run_lift.py "
             "on your own Monster.exe, then build again.");
        return 1;
    }

    static char gamedir[1024], overlay[1024];
    overlay[0] = 0;
#ifdef MTM_XBOX
    fullscreen = 1;
    static char fonts[1024];
    int xbox_found = xbox_setup(gamedir, sizeof gamedir, overlay, sizeof overlay, fonts, sizeof fonts);
    if (game) snprintf(gamedir, sizeof gamedir, "%s", game);
#endif
    if (game) {
        snprintf(gamedir, sizeof gamedir, "%s", game);
    } else {
#ifdef MTM_XBOX
        /* xbox_setup chose it */
#elif defined(__ANDROID__)
        const char* ext = SDL_AndroidGetExternalStoragePath();   /* /sdcard/Android/data/<package>/files */
        snprintf(gamedir, sizeof gamedir, "%s/game", ext ? ext : ".");
#else
        snprintf(gamedir, sizeof gamedir, "%s", MTM_GAME_DIR);
#endif
    }
    char full[PATH_MAX];
    if (realpath(gamedir, full)) snprintf(gamedir, sizeof gamedir, "%s", full);   /* else as given: the message below names it */

    static char exe[1100];
    const char* names[] = { "Monster.exe", "MONSTER.EXE", "monster.exe" };
    exe[0] = 0;
    for (int i = 0; i < 3 && !exe[0]; i++) {
        snprintf(exe, sizeof exe, "%s/%s", gamedir, names[i]);
        if (!exists(exe)) exe[0] = 0;
    }
    if (!exe[0]) {
        char msg[1600];
#ifdef MTM_XBOX
        (void)xbox_found;
        snprintf(msg, sizeof msg,
                 "Monster.exe is not in the app's folder.\n\n"
                 "Upload the folder made by tools/gamefiles.py to LocalState\\game with the "
                 "Device Portal's File explorer, or build the package with it inside "
                 "(xbox/package.ps1 -Game). docs/xbox.md has the steps.\n\n%s", gamedir);
#else
        snprintf(msg, sizeof msg,
                 "Monster.exe is not in\n%s\n\n"
                 "Copy the folder made by tools/gamefiles.py there, e.g.\n"
                 "adb push %s/. \"%s/\"", gamedir, MTM_GAME_DIR, gamedir);
#endif
        fail(MTM_TITLE ": no game files", msg);
        return 1;
    }
#ifdef MTM_XBOX
    snprintf(mtm_font_dir, sizeof mtm_font_dir, "%s", fonts);
#else
    snprintf(mtm_font_dir, sizeof mtm_font_dir, "%s/fonts/", gamedir);
#endif

    static char ini[1100];
    if (!pad_ini) { snprintf(ini, sizeof ini, "%s/controller.ini", gamedir); pad_ini = ini; }

    LOG(MTM_TITLE " recomp host (win32hle)\n  game: %s\n  lifted functions in dispatch: %u",
        gamedir, recomp_dispatch_count);

    /* The game opens its PODs and monster.ini by bare name, from the
     * current folder, and sees the game folder as C:\. */
    if (chdir(gamedir) != 0) { fail(MTM_TITLE, "cannot enter the game folder"); return 1; }
    hle_set_drive('C', gamedir);
    if (overlay[0]) hle_set_overlay(gamedir, overlay);    /* a read-only game folder: writes go here */

    recomp_host_init();
    win32hle_register(g_mtm_shims);
    win32hle_register(g_di_methods);

    static pe_image img;
    if (recomp_pe_map(exe, &img) != 0) {
        fail(MTM_TITLE, "cannot map Monster.exe at 0x00400000");
        return 1;
    }
    char guest[1100];
    hle_guest_path(exe, guest, sizeof guest);
    hle_module_add(guest, img.base, img.resource_rva, img.resource_size, 1);
    static char cmdline[1200];
    snprintf(cmdline, sizeof cmdline, "\"%s\"", guest);
    hle_set_command_line(cmdline);
    int unresolved = recomp_pe_bind(&img, resolve);
    LOG("  mapped %s at 0x%08X, entry 0x%08X; %d imports stubbed", guest, img.base, img.entry, unresolved);

    if (hle_screen_open(MTM_TITLE, fullscreen, headless) != 0) {
        fail(MTM_TITLE, "cannot open the screen (SDL)");
        return 1;
    }
    hle_screen_cursor_hook = show_cursor;
    if (!headless) {
        hle_screen_key_hook = pad_sdl_key;
        if (pad) pad_sdl_start(pad_ini);
        hle_set_pump_hook(pump);              /* after hle_screen_open, which set its own */
    }

    LOG("  entering 0x%08X", img.entry);
    uint32_t args[1];
    uint32_t rc = hle_call_guest(img.entry, 0, args);
    LOG("entry returned eax=%08X", rc);
    hle_exit((int)rc);
    return (int)rc;
}
