/* A game controller (XInput) on the Windows host: pad.c, with the mapping
 * from controller.ini in ../pad/pad_map.c. */
#pragma once

/* Read the mapping from ini (NULL: controller.ini next to mtm.exe; the
 * built-in mapping if there is none) and start polling. Returns 0 if XInput
 * is missing; the game then runs without a pad. */
int pad_start(const char* ini);

/* host.c: a key name as --key takes it ("UP", "ENTER", "F4", "A", "0x20"). */
int host_vk(const char* name);
/* host.c: hold or release a key the way --key does: GetKeyState sees it and
 * WM_KEYDOWN/WM_KEYUP go to the game thread's focus window. */
void host_key(int vk, int down);
