/*
 * pad.c - an XInput controller (Xbox 360 / One / Series pads, and anything
 * that presents as one) for a game that only reads the keyboard and mouse.
 *
 * Monster Truck Madness 2 creates DirectInput but never makes a device on
 * it: the race reads keys from window messages and GetKeyState, the front
 * end is MFC buttons under the mouse. So the pad becomes exactly that:
 *
 *   drive mode   each button, trigger and stick direction is a key from
 *                controller.ini, held for as long as it is held, through
 *                host_key() (the path --key takes).
 *   cursor mode  the left stick moves the mouse pointer over the game's
 *                window and A clicks, for the menus and dialogs. The
 *                [settings] cursor_toggle button switches modes.
 *
 * Polled at 125 Hz on its own thread. XInput is loaded at run time
 * (xinput1_4, then 1_3, then 9_1_0), so a machine without it still runs.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pad.h"
#include "ddraw_host.h"

/* XInput's own definitions, so no SDK header or import library is needed. */
typedef struct {
    WORD wButtons; BYTE bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
} pad_gamepad_t;
typedef struct { DWORD dwPacketNumber; pad_gamepad_t Gamepad; } pad_state_t;
typedef DWORD (WINAPI *xinput_get_state_t)(DWORD, pad_state_t*);
static xinput_get_state_t g_get_state;

/* Every input the ini can name, in the order of g_map. */
enum {
    P_DPAD_UP, P_DPAD_DOWN, P_DPAD_LEFT, P_DPAD_RIGHT, P_START, P_BACK,
    P_LS, P_RS, P_LB, P_RB, P_A, P_B, P_X, P_Y,
    P_LT, P_RT,
    P_LSTICK_UP, P_LSTICK_DOWN, P_LSTICK_LEFT, P_LSTICK_RIGHT,
    P_RSTICK_UP, P_RSTICK_DOWN, P_RSTICK_LEFT, P_RSTICK_RIGHT,
    P_COUNT
};
static const char* const g_names[P_COUNT] = {
    "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT", "START", "BACK",
    "LS", "RS", "LB", "RB", "A", "B", "X", "Y",
    "LT", "RT",
    "LSTICK_UP", "LSTICK_DOWN", "LSTICK_LEFT", "LSTICK_RIGHT",
    "RSTICK_UP", "RSTICK_DOWN", "RSTICK_LEFT", "RSTICK_RIGHT",
};
/* XInput's button bits for P_DPAD_UP..P_Y, in that order. */
static const WORD g_bits[P_Y + 1] = {
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020,
    0x0040, 0x0080, 0x0100, 0x0200, 0x1000, 0x2000, 0x4000, 0x8000,
};

/* Up to two keys per input ("UP+SHIFT"): 0 is none. */
static int g_map[P_COUNT][2];
static int g_deadzone = 12000, g_trigger = 40, g_cursor_speed = 14;
static int g_toggle = P_BACK, g_click = P_A, g_rclick = P_B;
static int g_player = -1;                 /* -1: the first pad connected */

static int input_of(const char* s) {
    for (int i = 0; i < P_COUNT; i++)
        if (!_stricmp(s, g_names[i])) return i;
    return -1;
}

static void trim(char* s) {
    char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
    char* b = s;
    while (*b == ' ' || *b == '\t') b++;
    memmove(s, b, strlen(b) + 1);
}

static void load(const char* ini) {
    char v[64];
    for (int i = 0; i < P_COUNT; i++) {
        GetPrivateProfileStringA("buttons", g_names[i], "", v, sizeof v, ini);
        g_map[i][0] = g_map[i][1] = 0;
        char* plus = strchr(v, '+');
        if (plus) { *plus++ = 0; trim(plus); g_map[i][1] = *plus ? host_vk(plus) & 0xFF : 0; }
        trim(v);
        g_map[i][0] = *v ? host_vk(v) & 0xFF : 0;
    }
    g_deadzone     = GetPrivateProfileIntA("settings", "stick_deadzone", g_deadzone, ini);
    g_trigger      = GetPrivateProfileIntA("settings", "trigger_threshold", g_trigger, ini);
    g_cursor_speed = GetPrivateProfileIntA("settings", "cursor_speed", g_cursor_speed, ini);
    g_player       = GetPrivateProfileIntA("settings", "player", 0, ini) - 1;
    GetPrivateProfileStringA("settings", "cursor_toggle", "BACK", v, sizeof v, ini);
    trim(v);
    g_toggle = input_of(v);
    GetPrivateProfileStringA("settings", "cursor_click", "A", v, sizeof v, ini);
    trim(v);
    if (input_of(v) >= 0) g_click = input_of(v);
    GetPrivateProfileStringA("settings", "cursor_right_click", "B", v, sizeof v, ini);
    trim(v);
    if (input_of(v) >= 0) g_rclick = input_of(v);
}

/* Which inputs are down, as a bit per P_*. */
static unsigned long long pressed(const pad_gamepad_t* g) {
    unsigned long long d = 0;
    for (int i = 0; i <= P_Y; i++)
        if (g->wButtons & g_bits[i]) d |= 1ull << i;
    if (g->bLeftTrigger  > g_trigger) d |= 1ull << P_LT;
    if (g->bRightTrigger > g_trigger) d |= 1ull << P_RT;
    if (g->sThumbLY >  g_deadzone) d |= 1ull << P_LSTICK_UP;
    if (g->sThumbLY < -g_deadzone) d |= 1ull << P_LSTICK_DOWN;
    if (g->sThumbLX < -g_deadzone) d |= 1ull << P_LSTICK_LEFT;
    if (g->sThumbLX >  g_deadzone) d |= 1ull << P_LSTICK_RIGHT;
    if (g->sThumbRY >  g_deadzone) d |= 1ull << P_RSTICK_UP;
    if (g->sThumbRY < -g_deadzone) d |= 1ull << P_RSTICK_DOWN;
    if (g->sThumbRX < -g_deadzone) d |= 1ull << P_RSTICK_LEFT;
    if (g->sThumbRX >  g_deadzone) d |= 1ull << P_RSTICK_RIGHT;
    return d;
}

/* Keys are counted, so two inputs on one key (DPAD_UP and RT both UP) hold
 * it until both let go. */
static int g_keycount[256];
static void key(int vk, int down) {
    if (!vk) return;
    if (down) { if (g_keycount[vk]++ == 0) host_key(vk, 1); }
    else if (g_keycount[vk] > 0 && --g_keycount[vk] == 0) host_key(vk, 0);
}
static void input(int i, int down) {
    key(g_map[i][0], down);
    key(g_map[i][1], down);
}

static void mouse_button(int right, int down) {
    INPUT in = { INPUT_MOUSE };
    in.mi.dwFlags = right ? (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP)
                          : (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
    SendInput(1, &in, sizeof in);
}

/* Stick to pointer speed: nothing in the dead zone, then a curve so small
 * pushes are fine and a full push crosses the window in about a second. */
static int axis_speed(SHORT v) {
    int a = v < 0 ? -v : v;
    if (a <= g_deadzone) return 0;
    double t = (double)(a - g_deadzone) / (32767 - g_deadzone);
    int s = (int)(t * t * g_cursor_speed + 0.5);
    if (!s) s = 1;
    return v < 0 ? -s : s;
}

static void move_cursor(const pad_gamepad_t* g) {
    int dx = axis_speed(g->sThumbLX), dy = -axis_speed(g->sThumbLY);
    dx += axis_speed(g->sThumbRX);
    dy -= axis_speed(g->sThumbRY);
    if (!dx && !dy) return;
    POINT p;
    GetCursorPos(&p);
    p.x += dx;
    p.y += dy;
    /* Kept on the game's window, where the menus are. */
    HWND w = ddh_window();
    RECT r;
    if (w && GetClientRect(w, &r)) {
        POINT tl = { r.left, r.top }, br = { r.right - 1, r.bottom - 1 };
        ClientToScreen(w, &tl);
        ClientToScreen(w, &br);
        if (p.x < tl.x) p.x = tl.x; else if (p.x > br.x) p.x = br.x;
        if (p.y < tl.y) p.y = tl.y; else if (p.y > br.y) p.y = br.y;
    }
    SetCursorPos(p.x, p.y);
}

static DWORD WINAPI pad_thread(LPVOID unused) {
    (void)unused;
    unsigned long long was = 0;
    int cursor = 0, player = g_player, seen = 0;
    while (!ddh_window()) Sleep(50);
    for (;;) {
        Sleep(8);
        pad_state_t st;
        int found = -1;
        if (player >= 0) {
            if (g_get_state((DWORD)player, &st) == ERROR_SUCCESS) found = player;
        } else {
            for (int p = 0; p < 4 && found < 0; p++)
                if (g_get_state((DWORD)p, &st) == ERROR_SUCCESS) found = p;
        }
        unsigned long long now = 0;
        if (found >= 0) {
            if (!seen) fprintf(stderr, "[pad] controller %d connected\n", found + 1);
            seen = 1;
            now = pressed(&st.Gamepad);
        } else if (seen) {
            fprintf(stderr, "[pad] controller disconnected\n");
            seen = 0;
        }
        /* Only send input while the game is the foreground app. */
        HWND fg = GetForegroundWindow();
        DWORD fgpid = 0;
        if (fg) GetWindowThreadProcessId(fg, &fgpid);
        if (fgpid != GetCurrentProcessId()) now = 0;

        unsigned long long went = now & ~was, left = was & ~now;
        if (g_toggle >= 0 && (went >> g_toggle & 1)) {
            /* Let go of everything held in the old mode first. */
            for (int i = 0; i < P_COUNT; i++)
                if (i != g_toggle && (was >> i & 1)) {
                    if (cursor && (i == g_click || i == g_rclick)) mouse_button(i == g_rclick, 0);
                    else if (!cursor) input(i, 0);
                }
            was = 1ull << g_toggle;
            now &= 1ull << g_toggle;
            cursor = !cursor;
            fprintf(stderr, "[pad] %s mode\n", cursor ? "cursor" : "drive");
            continue;
        }
        for (int i = 0; i < P_COUNT; i++) {
            if (i == g_toggle) continue;
            int d = (int)(went >> i & 1), u = (int)(left >> i & 1);
            if (!d && !u) continue;
            if (cursor) {
                if (i == g_click || i == g_rclick) mouse_button(i == g_rclick, d);
                else if (i < P_LSTICK_UP) input(i, d);     /* the sticks steer the pointer */
            } else {
                input(i, d);
            }
        }
        if (cursor && found >= 0 && fgpid == GetCurrentProcessId()) move_cursor(&st.Gamepad);
        was = now;
    }
    return 0;                             /* not reached; MSVC wants it (C4716) */
}

int pad_start(const char* ini) {
    static const char* const dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for (int i = 0; i < 3 && !g_get_state; i++) {
        HMODULE h = LoadLibraryA(dlls[i]);
        if (h) g_get_state = (xinput_get_state_t)(void*)GetProcAddress(h, "XInputGetState");
    }
    if (!g_get_state) {
        fprintf(stderr, "[pad] no XInput on this machine: no controller\n");
        return 0;
    }
    char path[MAX_PATH];
    if (ini) {
        GetFullPathNameA(ini, MAX_PATH, path, NULL);
    } else {
        GetModuleFileNameA(NULL, path, MAX_PATH);
        char* slash = strrchr(path, '\\');
        strcpy(slash ? slash + 1 : path, "controller.ini");
    }
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "[pad] %s not found: no controller\n", path);
        return 0;
    }
    load(path);
    fprintf(stderr, "[pad] mapping from %s; %s toggles the cursor\n", path,
            g_toggle >= 0 ? g_names[g_toggle] : "nothing");
    CloseHandle(CreateThread(NULL, 0, pad_thread, NULL, 0, NULL));
    return 1;
}
