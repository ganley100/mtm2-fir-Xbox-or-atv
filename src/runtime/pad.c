/*
 * pad.c - an XInput controller (Xbox 360 / One / Series pads, and anything
 * that presents as one) on the Windows host. The mapping, and what drive and
 * cursor mode do, is ../pad/pad_map.c; here is only XInput and where its
 * output goes: keys through host_key() (the path --key takes), the pointer
 * and its buttons through SendInput over the game's window.
 *
 * Polled at 125 Hz on its own thread. XInput is loaded at run time
 * (xinput1_4, then 1_3, then 9_1_0), so a machine without it still runs.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "pad.h"
#include "pad_map.h"
#include "ddraw_host.h"

/* XInput's own definitions, so no SDK header or import library is needed. */
typedef struct {
    WORD wButtons; BYTE bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
} xi_gamepad_t;
typedef struct { DWORD dwPacketNumber; xi_gamepad_t Gamepad; } xi_state_t;
typedef DWORD (WINAPI *xinput_get_state_t)(DWORD, xi_state_t*);
static xinput_get_state_t g_get_state;

static void out_mouse_button(int right, int down) {
    INPUT in = { INPUT_MOUSE };
    in.mi.dwFlags = right ? (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP)
                          : (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
    SendInput(1, &in, sizeof in);
}

/* Kept on the game's window, where the menus are. */
static void out_cursor_move(int dx, int dy) {
    POINT p;
    GetCursorPos(&p);
    p.x += dx;
    p.y += dy;
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

static void out_log(const char* msg) { fprintf(stderr, "%s\n", msg); }

static DWORD WINAPI pad_thread(LPVOID unused) {
    (void)unused;
    static const pad_out_t out = { host_key, out_mouse_button, out_cursor_move, out_log };
    int player = pad_map_player(), seen = 0;
    while (!ddh_window()) Sleep(50);
    for (;;) {
        Sleep(8);
        xi_state_t st;
        int found = -1;
        for (int p = player >= 0 ? player : 0; p < (player >= 0 ? player + 1 : 4) && found < 0; p++)
            if (g_get_state((DWORD)p, &st) == ERROR_SUCCESS) found = p;
        pad_snapshot_t s = { 0 };
        if (found >= 0) {
            if (!seen) fprintf(stderr, "[pad] controller %d connected\n", found + 1);
            seen = 1;
            s.connected = 1;
            s.buttons = st.Gamepad.wButtons;
            s.lt = st.Gamepad.bLeftTrigger;
            s.rt = st.Gamepad.bRightTrigger;
            s.lx = st.Gamepad.sThumbLX;
            s.ly = st.Gamepad.sThumbLY;
            s.rx = st.Gamepad.sThumbRX;
            s.ry = st.Gamepad.sThumbRY;
        } else if (seen) {
            fprintf(stderr, "[pad] controller disconnected\n");
            seen = 0;
        }
        /* Only while the game is the foreground app; otherwise all is let go. */
        HWND fg = GetForegroundWindow();
        DWORD fgpid = 0;
        if (fg) GetWindowThreadProcessId(fg, &fgpid);
        pad_map_step(&s, fgpid == GetCurrentProcessId(), &out);
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
    pad_map_defaults();
    if (pad_map_load(path)) fprintf(stderr, "[pad] mapping from %s\n", path);
    else fprintf(stderr, "[pad] %s not found: the built-in mapping\n", path);
    CloseHandle(CreateThread(NULL, 0, pad_thread, NULL, 0, NULL));
    return 1;
}
