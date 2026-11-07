/* The controller mapping, shared by every host: controller.ini read into a
 * table, and each poll of a pad turned into the keys and mouse the game
 * reads. Plain C, no OS: the Windows host feeds it from XInput (pad.c), the
 * portable host from SDL's game controllers (pad_sdl.c). */
#pragma once

/* Buttons, as bits of pad_snapshot_t.buttons (XInput's own layout). */
enum {
    PAD_DPAD_UP = 0x0001, PAD_DPAD_DOWN = 0x0002, PAD_DPAD_LEFT = 0x0004, PAD_DPAD_RIGHT = 0x0008,
    PAD_START = 0x0010, PAD_BACK = 0x0020, PAD_LS = 0x0040, PAD_RS = 0x0080,
    PAD_LB = 0x0100, PAD_RB = 0x0200, PAD_A = 0x1000, PAD_B = 0x2000, PAD_X = 0x4000, PAD_Y = 0x8000,
};

typedef struct {
    int connected;
    unsigned buttons;          /* PAD_* */
    int lt, rt;                /* 0..255 */
    int lx, ly, rx, ry;        /* -32768..32767, up is positive */
} pad_snapshot_t;

/* Where the mapping's output goes. */
typedef struct {
    void (*key)(int vk, int down);            /* a Windows virtual-key code */
    void (*mouse_button)(int right, int down);
    void (*cursor_move)(int dx, int dy);      /* in pixels of the game's 640x480 */
    void (*log)(const char* msg);
} pad_out_t;

/* The built-in mapping (what config/controller.ini ships with), then the
 * file's values over it. Returns 0 if the file cannot be read: the
 * built-in mapping stays. */
void pad_map_defaults(void);
int  pad_map_load(const char* path);
int  pad_map_player(void);             /* 0-3, or -1 for the first pad connected */

/* One poll. focused: the game has the input focus (0 lets everything go). */
void pad_map_step(const pad_snapshot_t* s, int focused, const pad_out_t* out);

/* A key name as --key and controller.ini take it ("UP", "ENTER", "F4", "A",
 * "NUM5", "0x20"): its Windows virtual-key code, 0 for none. */
int  pad_vk(const char* name);
