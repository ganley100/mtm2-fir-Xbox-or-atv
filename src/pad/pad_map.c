/*
 * pad_map.c - controller.ini, and a pad's state turned into the game's input.
 *
 * Monster Truck Madness 2 reads only the keyboard and mouse: the race takes
 * keys from window messages and GetKeyState, the front end is buttons under
 * the mouse. So the pad becomes exactly those:
 *
 *   drive mode   each button, trigger and stick direction is a key from
 *                controller.ini, held for as long as it is held.
 *   cursor mode  the sticks move the mouse pointer and cursor_click /
 *                cursor_right_click are its buttons, for the menus and
 *                dialogs. The cursor_toggle button switches modes.
 *
 * No OS calls: the host polls the pad and hands each poll to pad_map_step.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "pad_map.h"

/* Every input the ini can name. */
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
static const unsigned g_bits[P_Y + 1] = {
    PAD_DPAD_UP, PAD_DPAD_DOWN, PAD_DPAD_LEFT, PAD_DPAD_RIGHT, PAD_START, PAD_BACK,
    PAD_LS, PAD_RS, PAD_LB, PAD_RB, PAD_A, PAD_B, PAD_X, PAD_Y,
};

/* ------------------------------------------------------------ key names */

/* Windows virtual-key codes, by number: this file includes no Windows header. */
int pad_vk(const char* s) {
    static const struct { const char* n; int vk; } names[] = {
        {"UP", 0x26}, {"DOWN", 0x28}, {"LEFT", 0x25}, {"RIGHT", 0x27},
        {"SPACE", 0x20}, {"ENTER", 0x0D}, {"ESC", 0x1B}, {"SHIFT", 0x10},
        {"CTRL", 0x11}, {"ALT", 0x12}, {"TAB", 0x09}, {"LBUTTON", 0x01},
        {"BACKSPACE", 0x08}, {"INSERT", 0x2D}, {"DELETE", 0x2E},
        {"HOME", 0x24}, {"END", 0x23}, {"PGUP", 0x21}, {"PGDN", 0x22},
        {"PAUSE", 0x13}, {"NONE", 0}};
    if (!s || !*s) return 0;
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        const char *a = s, *b = names[i].n;
        while (*a && toupper((unsigned char)*a) == *b) a++, b++;
        if (!*a && !*b) return names[i].vk;
    }
    if (toupper((unsigned char)s[0]) == 'F' && atoi(s + 1) >= 1 && atoi(s + 1) <= 12 && isdigit((unsigned char)s[1]))
        return 0x70 + atoi(s + 1) - 1;                                        /* VK_F1.. */
    if (toupper((unsigned char)s[0]) == 'N' && toupper((unsigned char)s[1]) == 'U' &&
        toupper((unsigned char)s[2]) == 'M' && isdigit((unsigned char)s[3]) && !s[4])
        return 0x60 + s[3] - '0';                                             /* VK_NUMPAD0.. */
    if (!s[1]) return toupper((unsigned char)s[0]);                           /* A-Z, 0-9 */
    return (int)strtol(s, NULL, 0) & 0xFF;
}

/* ------------------------------------------------------------ the mapping */

static int g_map[P_COUNT][2];          /* up to two keys per input ("UP+SHIFT") */
static int g_deadzone, g_trigger, g_cursor_speed, g_player;
static int g_toggle, g_click, g_rclick;

static int input_of(const char* s) {
    for (int i = 0; i < P_COUNT; i++) {
        const char *a = s, *b = g_names[i];
        while (*a && toupper((unsigned char)*a) == *b) a++, b++;
        if (!*a && !*b) return i;
    }
    return -1;
}

static void set_keys(int i, char* v) {
    g_map[i][0] = g_map[i][1] = 0;
    char* plus = strchr(v, '+');
    if (plus) { *plus++ = 0; g_map[i][1] = pad_vk(plus); }
    g_map[i][0] = pad_vk(v);
}

void pad_map_defaults(void) {
    static const char* const def[][2] = {
        {"RT", "UP"}, {"LT", "DOWN"}, {"LSTICK_LEFT", "LEFT"}, {"LSTICK_RIGHT", "RIGHT"},
        {"DPAD_UP", "UP"}, {"DPAD_DOWN", "DOWN"}, {"DPAD_LEFT", "LEFT"}, {"DPAD_RIGHT", "RIGHT"},
        {"A", "ENTER"}, {"B", "ESC"}, {"START", "ESC"},
    };
    memset(g_map, 0, sizeof g_map);
    for (size_t i = 0; i < sizeof def / sizeof def[0]; i++) {
        char v[16];
        snprintf(v, sizeof v, "%s", def[i][1]);
        set_keys(input_of(def[i][0]), v);
    }
    g_deadzone = 12000, g_trigger = 40, g_cursor_speed = 14, g_player = -1;
    g_toggle = P_BACK, g_click = P_A, g_rclick = P_B;
}

static char* trim(char* s) {
    while (*s == ' ' || *s == '\t') s++;
    char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

int pad_map_load(const char* path) {
    FILE* f = path ? fopen(path, "r") : NULL;
    if (!f) return 0;
    char line[256], section[32] = "";
    while (fgets(line, sizeof line, f)) {
        char* s = trim(line);
        if (!*s || *s == ';' || *s == '#') continue;
        if (*s == '[') {
            char* e = strchr(s, ']');
            if (e) *e = 0;
            snprintf(section, sizeof section, "%s", s + 1);
            for (char* c = section; *c; c++) *c = (char)tolower((unsigned char)*c);
            continue;
        }
        char* eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        char* c = strchr(v, ';');                /* a comment after the value */
        if (c) *c = 0, v = trim(v);
        if (!strcmp(section, "buttons")) {
            int i = input_of(k);
            if (i >= 0) set_keys(i, v);
        } else if (!strcmp(section, "settings")) {
            for (char* p = k; *p; p++) *p = (char)tolower((unsigned char)*p);
            if (!strcmp(k, "stick_deadzone")) g_deadzone = atoi(v);
            else if (!strcmp(k, "trigger_threshold")) g_trigger = atoi(v);
            else if (!strcmp(k, "cursor_speed")) g_cursor_speed = atoi(v);
            else if (!strcmp(k, "player")) g_player = atoi(v) - 1;
            else if (!strcmp(k, "cursor_toggle")) g_toggle = input_of(v);    /* -1: no cursor mode */
            else if (!strcmp(k, "cursor_click") && input_of(v) >= 0) g_click = input_of(v);
            else if (!strcmp(k, "cursor_right_click") && input_of(v) >= 0) g_rclick = input_of(v);
        }
    }
    fclose(f);
    if (g_deadzone < 0) g_deadzone = 0;
    if (g_deadzone > 32000) g_deadzone = 32000;
    if (g_player < -1 || g_player > 3) g_player = -1;
    return 1;
}

int pad_map_player(void) { return g_player; }

/* ------------------------------------------------------------ each poll */

static unsigned long long pressed(const pad_snapshot_t* s) {
    unsigned long long d = 0;
    if (!s->connected) return 0;
    for (int i = 0; i <= P_Y; i++)
        if (s->buttons & g_bits[i]) d |= 1ull << i;
    if (s->lt > g_trigger) d |= 1ull << P_LT;
    if (s->rt > g_trigger) d |= 1ull << P_RT;
    if (s->ly >  g_deadzone) d |= 1ull << P_LSTICK_UP;
    if (s->ly < -g_deadzone) d |= 1ull << P_LSTICK_DOWN;
    if (s->lx < -g_deadzone) d |= 1ull << P_LSTICK_LEFT;
    if (s->lx >  g_deadzone) d |= 1ull << P_LSTICK_RIGHT;
    if (s->ry >  g_deadzone) d |= 1ull << P_RSTICK_UP;
    if (s->ry < -g_deadzone) d |= 1ull << P_RSTICK_DOWN;
    if (s->rx < -g_deadzone) d |= 1ull << P_RSTICK_LEFT;
    if (s->rx >  g_deadzone) d |= 1ull << P_RSTICK_RIGHT;
    return d;
}

/* Keys are counted, so two inputs on one key (DPAD_UP and RT both UP) hold
 * it until both let go. */
static int g_keycount[256];
static void key(const pad_out_t* o, int vk, int down) {
    if (!vk) return;
    vk &= 0xFF;
    if (down) { if (g_keycount[vk]++ == 0) o->key(vk, 1); }
    else if (g_keycount[vk] > 0 && --g_keycount[vk] == 0) o->key(vk, 0);
}
static void input(const pad_out_t* o, int i, int down) {
    key(o, g_map[i][0], down);
    key(o, g_map[i][1], down);
}

/* Stick to pointer speed: nothing in the dead zone, then a curve so small
 * pushes are fine and a full push crosses the screen in about a second. */
static int axis_speed(int v) {
    int a = v < 0 ? -v : v;
    if (a <= g_deadzone) return 0;
    double t = (double)(a - g_deadzone) / (32767 - g_deadzone);
    int s = (int)(t * t * g_cursor_speed + 0.5);
    if (!s) s = 1;
    return v < 0 ? -s : s;
}

static unsigned long long g_was;
static int g_cursor;

static void release(const pad_out_t* o, unsigned long long bits) {
    for (int i = 0; i < P_COUNT; i++)
        if (i != g_toggle && (bits >> i & 1)) {
            if (g_cursor && (i == g_click || i == g_rclick)) o->mouse_button(i == g_rclick, 0);
            else if (!g_cursor || i < P_LSTICK_UP) input(o, i, 0);
        }
}

void pad_map_step(const pad_snapshot_t* s, int focused, const pad_out_t* o) {
    unsigned long long now = focused ? pressed(s) : 0;
    unsigned long long went = now & ~g_was, left = g_was & ~now;
    if (g_toggle >= 0 && (went >> g_toggle & 1)) {
        release(o, g_was);                       /* let go of everything held in the old mode */
        g_cursor = !g_cursor;
        g_was = now & (1ull << g_toggle);
        if (o->log) o->log(g_cursor ? "[pad] cursor mode" : "[pad] drive mode");
        return;
    }
    for (int i = 0; i < P_COUNT; i++) {
        if (i == g_toggle) continue;
        int d = (int)(went >> i & 1), u = (int)(left >> i & 1);
        if (!d && !u) continue;
        if (g_cursor) {
            if (i == g_click || i == g_rclick) o->mouse_button(i == g_rclick, d);
            else if (i < P_LSTICK_UP) input(o, i, d);         /* the sticks steer the pointer */
        } else {
            input(o, i, d);
        }
    }
    g_was = now;
    if (g_cursor && focused && s->connected) {
        int dx = axis_speed(s->lx) + axis_speed(s->rx);
        int dy = -axis_speed(s->ly) - axis_speed(s->ry);
        if (dx || dy) o->cursor_move(dx, dy);
    }
}
