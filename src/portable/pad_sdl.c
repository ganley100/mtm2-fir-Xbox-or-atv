/*
 * pad_sdl.c - controllers and the TV remote on the portable host.
 *
 * Game controllers come from SDL's GameController API (on Android TV that is
 * any gamepad Android sees; on a desktop, XInput, PlayStation and Switch pads
 * alike). Each poll goes through the shared mapping (../pad/pad_map.c) and
 * comes out as win32hle input: keys with hle_input_key, the pointer and its
 * buttons with hle_input_mouse, in the game's 640x480.
 *
 * An Android TV remote is not a game controller: its D-pad comes as arrow
 * keys, which already reach the game, its centre button as SELECT and its
 * back button as AC_BACK, which pad_sdl_key turns into ENTER and ESC.
 *
 * Everything here runs on the game's thread, from the message pump.
 */
#include <stdio.h>
#include <SDL2/SDL.h>

#include "win32hle.h"
#include "pad_map.h"
#include "pad_sdl.h"

#define WM_MOUSEMOVE   0x0200u
#define WM_LBUTTONDOWN 0x0201u
#define WM_LBUTTONUP   0x0202u
#define WM_RBUTTONDOWN 0x0204u
#define WM_RBUTTONUP   0x0205u

static SDL_GameController* g_pad;
static int g_on, g_cx = 320, g_cy = 240;

/* The PC scan code for a key, for lParam's bits 16-23. */
static uint32_t scan_of_vk(int vk) {
    switch (vk) {
    case 0x26: return 0x48; case 0x28: return 0x50; case 0x25: return 0x4B; case 0x27: return 0x4D;
    case 0x0D: return 0x1C; case 0x1B: return 0x01; case 0x20: return 0x39; case 0x10: return 0x2A;
    case 0x11: return 0x1D; case 0x12: return 0x38; case 0x09: return 0x0F;
    default:   return vk >= 0x70 && vk <= 0x79 ? 0x3B + (uint32_t)(vk - 0x70) : 0;
    }
}

static void out_key(int vk, int down) { hle_input_key(down, (uint32_t)vk, scan_of_vk(vk)); }

static void screen_size(int* w, int* h) {
    int bpp;
    hle_dd_mode(w, h, &bpp);
    if (*w <= 0 || *h <= 0) *w = 640, *h = 480;
}
static void out_mouse_button(int right, int down) {
    hle_input_mouse(right ? (down ? WM_RBUTTONDOWN : WM_RBUTTONUP) : (down ? WM_LBUTTONDOWN : WM_LBUTTONUP), g_cx, g_cy);
}
static void out_cursor_move(int dx, int dy) {
    int w, h;
    screen_size(&w, &h);
    g_cx += dx, g_cy += dy;
    if (g_cx < 0) g_cx = 0; else if (g_cx >= w) g_cx = w - 1;
    if (g_cy < 0) g_cy = 0; else if (g_cy >= h) g_cy = h - 1;
    hle_input_mouse(WM_MOUSEMOVE, g_cx, g_cy);
}
static void out_log(const char* msg) { fprintf(stderr, "%s\n", msg); }

int pad_sdl_start(const char* ini) {
    pad_map_defaults();
    if (ini && pad_map_load(ini)) fprintf(stderr, "[pad] mapping from %s\n", ini);
    else fprintf(stderr, "[pad] the built-in mapping (no %s)\n", ini ? ini : "controller.ini");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "[pad] SDL game controllers: %s\n", SDL_GetError());
        return 0;
    }
    g_on = 1;
    return 1;
}

/* The pad to read: the one controller.ini names, else the first. A pad that
 * goes away is closed, and the next poll looks again. */
static void find_pad(void) {
    if (g_pad && !SDL_GameControllerGetAttached(g_pad)) {
        fprintf(stderr, "[pad] controller disconnected\n");
        SDL_GameControllerClose(g_pad);
        g_pad = NULL;
    }
    if (g_pad) return;
    static uint32_t last;
    uint32_t now = SDL_GetTicks();
    if (now - last < 1000) return;                      /* look once a second */
    last = now;
    int want = pad_map_player(), seen = 0;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!SDL_IsGameController(i)) continue;
        if (want < 0 || seen++ == want) {
            g_pad = SDL_GameControllerOpen(i);
            if (g_pad) fprintf(stderr, "[pad] controller: %s\n", SDL_GameControllerName(g_pad));
            return;
        }
    }
}

void pad_sdl_poll(void) {
    if (!g_on) return;
    static const pad_out_t out = { out_key, out_mouse_button, out_cursor_move, out_log };
    SDL_GameControllerUpdate();
    find_pad();
    pad_snapshot_t s = { 0 };
    if (g_pad) {
        static const struct { SDL_GameControllerButton b; unsigned bit; } map[] = {
            { SDL_CONTROLLER_BUTTON_DPAD_UP, PAD_DPAD_UP }, { SDL_CONTROLLER_BUTTON_DPAD_DOWN, PAD_DPAD_DOWN },
            { SDL_CONTROLLER_BUTTON_DPAD_LEFT, PAD_DPAD_LEFT }, { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, PAD_DPAD_RIGHT },
            { SDL_CONTROLLER_BUTTON_START, PAD_START }, { SDL_CONTROLLER_BUTTON_BACK, PAD_BACK },
            { SDL_CONTROLLER_BUTTON_LEFTSTICK, PAD_LS }, { SDL_CONTROLLER_BUTTON_RIGHTSTICK, PAD_RS },
            { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, PAD_LB }, { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, PAD_RB },
            { SDL_CONTROLLER_BUTTON_A, PAD_A }, { SDL_CONTROLLER_BUTTON_B, PAD_B },
            { SDL_CONTROLLER_BUTTON_X, PAD_X }, { SDL_CONTROLLER_BUTTON_Y, PAD_Y },
        };
        s.connected = 1;
        for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
            if (SDL_GameControllerGetButton(g_pad, map[i].b)) s.buttons |= map[i].bit;
        /* SDL's triggers are 0..32767, its Y axes point down; XInput's are 0..255 and up. */
        s.lt = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7;
        s.rt = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7;
        s.lx = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX);
        s.ly = -1 - SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY);
        s.rx = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTX);
        s.ry = -1 - SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_RIGHTY);
    }
#if defined(__ANDROID__) || defined(__WINRT__)
    int focused = 1;                      /* SDL pauses the app when it is not in front */
#else
    int focused = SDL_GetKeyboardFocus() != NULL;
#endif
    pad_map_step(&s, focused, &out);
}

int pad_sdl_key(int sc, int down) {
    switch (sc) {
    case SDL_SCANCODE_SELECT:  out_key(0x0D, down); return 1;   /* the remote's centre button: ENTER */
#ifdef __WINRT__
    case SDL_SCANCODE_AC_BACK: return 1;     /* Xbox: that is the pad's B, which controller.ini already maps */
#else
    case SDL_SCANCODE_AC_BACK: out_key(0x1B, down); return 1;   /* its back button: ESC */
#endif
    default: return 0;
    }
}
