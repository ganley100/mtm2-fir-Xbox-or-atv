/*
 * extra_shims_ui.c - the USER32, GDI32 and KERNEL32 calls in the demo's
 * import table that pcrecomp's win32hle does not answer (yet), as
 * extra_shims.c does for the smaller DLLs. Most come with MFC and may never
 * be called (printing, metafiles, DDE, regions); each is answered the way a
 * plain 640x480 desktop with one screen DC would, and where the answer
 * needs win32hle's own state (text, DCs, pens) it calls win32hle's shim for
 * that.
 *
 * Registered after win32hle's own (mtm_hle.c), so a name win32hle answers
 * later is answered by win32hle, not here.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "win32hle.h"

#define WM_SETTEXT    0x000Cu
#define WM_GETTEXT    0x000Du
#define BM_SETCHECK   0x00F1u
#define SIMPLEREGION  2u

/* Another shim, called as the guest would. */
static uint32_t call(const char *name, int n, const uint32_t *args) {
    uint32_t va = hle_resolve(name);
    return va ? hle_call_guest(va, n, args) : 0;
}

/* ================================================================ USER32 */

static void u_GetLastActivePopup(void) { RET(A32(0), 1); }
static void u_IsChild(void) {                          /* (parent, hwnd) */
    for (uint32_t h = hle_window_parent(A32(1)); h; h = hle_window_parent(h))
        if (h == A32(0)) RET(1, 2);
    RET(0, 2);
}
static void u_IsIconic(void) { RET(0, 1); }
static void u_GetWindowThreadProcessId(void) {         /* (hwnd, &pid): the game's only thread with windows */
    if (A32(1)) MEM32(A32(1)) = 1;
    RET(call("GetCurrentThreadId", 0, NULL), 2);
}

/* Window properties: a small table by window and name (or atom). */
#define MAX_PROPS 256
static struct { uint32_t hwnd, atom, data; char name[48]; } g_prop[MAX_PROPS];
static int prop_find(uint32_t hwnd, uint32_t key) {
    for (int i = 0; i < MAX_PROPS; i++) {
        if (!g_prop[i].hwnd || g_prop[i].hwnd != hwnd) continue;
        if (key < 0x10000u ? g_prop[i].atom == key : !g_prop[i].atom && !strcmp(g_prop[i].name, (const char *)hle_ptr(key)))
            return i;
    }
    return -1;
}
static void u_SetPropA(void) {                         /* (hwnd, name, data) */
    int i = prop_find(A32(0), A32(1));
    for (int j = 0; i < 0 && j < MAX_PROPS; j++) if (!g_prop[j].hwnd) i = j;
    if (i < 0) RET(0, 3);
    g_prop[i].hwnd = A32(0), g_prop[i].data = A32(2);
    g_prop[i].atom = A32(1) < 0x10000u ? A32(1) : 0;
    if (!g_prop[i].atom) snprintf(g_prop[i].name, sizeof g_prop[i].name, "%s", (const char *)hle_ptr(A32(1)));
    RET(1, 3);
}
static void u_GetPropA(void) { int i = prop_find(A32(0), A32(1)); RET(i < 0 ? 0 : g_prop[i].data, 2); }
static void u_RemovePropA(void) {
    int i = prop_find(A32(0), A32(1));
    uint32_t d = i < 0 ? 0 : g_prop[i].data;
    if (i >= 0) g_prop[i].hwnd = 0;
    RET(d, 2);
}

/* Code page 1252: one byte a character, OEM the same as ANSI. */
static void u_CharNextA(void) { uint32_t p = A32(0); RET(p && MEM8(p) ? p + 1 : p, 1); }
static void u_CharPrevA(void) { uint32_t s = A32(0), p = A32(1); RET(p > s ? p - 1 : s, 2); }
static void u_CharToOemA(void) {                       /* (src, dst) */
    if (A32(0) && A32(1) && A32(0) != A32(1)) strcpy((char *)hle_ptr(A32(1)), (const char *)hle_ptr(A32(0)));
    RET(1, 2);
}

static void u_CheckRadioButton(void) {                 /* (dlg, first, last, check) */
    for (uint32_t id = A32(1); id <= A32(2); id++) {
        uint32_t item = hle_dialog_item(A32(0), id);
        if (item) hle_send(item, BM_SETCHECK, id == A32(3), 0);
    }
    RET(1, 4);
}
static void u_DestroyIconOrCursor(void) { RET(1, 1); }
static void u_GetDCEx(void) { uint32_t a = A32(0); RET(call("GetDC", 1, &a), 3); }
static void u_GetWindowDC(void) { uint32_t a = A32(0); RET(call("GetDC", 1, &a), 1); }
/* MS Sans Serif 8: 6 x 13 pixels a dialog base unit pair. */
static void u_GetDialogBaseUnits(void) { RET(13u << 16 | 6u, 0); }
static void u_MapDialogRect(void) {                    /* (dlg, &rect) in dialog units -> pixels */
    uint32_t r = A32(1);
    if (r) {
        MEM32(r) = (uint32_t)((int32_t)MEM32(r) * 6 / 4);
        MEM32(r + 4) = (uint32_t)((int32_t)MEM32(r + 4) * 13 / 8);
        MEM32(r + 8) = (uint32_t)((int32_t)MEM32(r + 8) * 6 / 4);
        MEM32(r + 12) = (uint32_t)((int32_t)MEM32(r + 12) * 13 / 8);
    }
    RET(1, 2);
}
static void u_GetDlgItemInt(void) {                    /* (dlg, id, &ok, signed) */
    uint32_t item = hle_dialog_item(A32(0), A32(1)), mark = hle_scratch_mark(), buf = hle_scratch(32);
    uint32_t n = item ? hle_send(item, WM_GETTEXT, 32, buf) : 0;
    char *end = NULL, *s = (char *)hle_ptr(buf);
    long v = n ? strtol(s, &end, 10) : 0;
    int ok = n && end && end != s;
    if (A32(2)) MEM32(A32(2)) = (uint32_t)ok;
    hle_scratch_release(mark);
    RET(ok ? (uint32_t)v : 0, 4);
}
static void u_SetDlgItemInt(void) {                    /* (dlg, id, value, signed) */
    uint32_t item = hle_dialog_item(A32(0), A32(1)), mark = hle_scratch_mark(), buf = hle_scratch(16);
    snprintf((char *)hle_ptr(buf), 16, A32(3) ? "%d" : "%u", A32(2));
    if (item) hle_send(item, WM_SETTEXT, 0, buf);
    hle_scratch_release(mark);
    RET(item ? 1 : 0, 4);
}
static void u_GetMenuCheckMarkDimensions(void) { RET(13u << 16 | 13u, 0); }
static void u_GetMenuStringA(void) {                   /* (menu, id, buf, max, flags) */
    if (A32(2) && A32(3)) MEM8(A32(2)) = 0;
    RET(0, 5);
}
static void u_GetMessagePos(void) {
    uint32_t mark = hle_scratch_mark(), pt = hle_scratch(8);
    call("GetCursorPos", 1, &pt);
    uint32_t r = (MEM32(pt + 4) & 0xFFFFu) << 16 | (MEM32(pt) & 0xFFFFu);
    hle_scratch_release(mark);
    RET(r, 0);
}
static void u_GetMessageTime(void) { RET(hle_ticks_ms(), 0); }

/* Scroll bars: the game's windows have none. */
static void u_GetScrollInfo(void)  { RET(0, 3); }
static void u_GetScrollPos(void)   { RET(0, 2); }
static void u_GetScrollRange(void) {
    if (A32(2)) MEM32(A32(2)) = 0;
    if (A32(3)) MEM32(A32(3)) = 0;
    RET(1, 4);
}
static void u_SetScrollInfo(void)  { RET(0, 4); }
static void u_SetScrollPos(void)   { RET(0, 4); }
static void u_SetScrollRange(void) { RET(1, 5); }
static void u_ShowScrollBar(void)  { RET(1, 3); }
static void u_ScrollWindow(void)   { RET(1, 5); }
static void u_ScrollWindowEx(void) { RET(SIMPLEREGION, 8); }

static void u_GetSystemMenu(void) { RET(0, 2); }

static void u_GetTabbedTextExtentA(void) {             /* (hdc, str, n, ntabs, tabs) */
    uint32_t mark = hle_scratch_mark(), sz = hle_scratch(8);
    uint32_t a[4] = { A32(0), A32(1), A32(2), sz };
    call("GetTextExtentPoint32A", 4, a);
    uint32_t r = (MEM32(sz + 4) & 0xFFFFu) << 16 | (MEM32(sz) & 0xFFFFu);
    hle_scratch_release(mark);
    RET(r, 5);
}
static void u_TabbedTextOutA(void) {                   /* (hdc, x, y, str, n, ntabs, tabs, origin) */
    uint32_t a[5] = { A32(0), A32(1), A32(2), A32(3), A32(4) };
    call("TextOutA", 5, a);
    uint32_t mark = hle_scratch_mark(), sz = hle_scratch(8);
    uint32_t b[4] = { A32(0), A32(3), A32(4), sz };
    call("GetTextExtentPoint32A", 4, b);
    uint32_t r = (MEM32(sz + 4) & 0xFFFFu) << 16 | (MEM32(sz) & 0xFFFFu);
    hle_scratch_release(mark);
    RET(r, 8);
}
static void u_GrayStringA(void) {                      /* (hdc, brush, proc, data, n, x, y, w, h) */
    if (!A32(2) && A32(3)) {                           /* no callback: data is the text */
        uint32_t n = A32(4) ? A32(4) : (uint32_t)strlen((const char *)hle_ptr(A32(3)));
        uint32_t a[5] = { A32(0), A32(5), A32(6), A32(3), n };
        call("TextOutA", 5, a);
    }
    RET(1, 9);
}
static void u_InvertRect(void) { RET(1, 2); }

static void u_GetWindowPlacement(void) {               /* (hwnd, WINDOWPLACEMENT*) */
    uint32_t p = A32(1);
    if (!p) RET(0, 2);
    int l = 0, t = 0, r = 640, b = 480;
    hle_window_screen_rect(A32(0), &l, &t, &r, &b);
    memset(hle_ptr(p), 0, 44);
    MEM32(p) = 44, MEM32(p + 8) = 1;                   /* length, SW_SHOWNORMAL */
    MEM32(p + 28) = (uint32_t)l, MEM32(p + 32) = (uint32_t)t, MEM32(p + 36) = (uint32_t)r, MEM32(p + 40) = (uint32_t)b;
    RET(1, 2);
}
static void u_SetWindowPlacement(void) { RET(1, 2); }

/* DDE packing (MFC's OLE server code): the two halves of lParam. */
static void u_UnpackDDElParam(void) {                  /* (msg, lp, &lo, &hi) */
    if (A32(2)) MEM32(A32(2)) = A32(1) & 0xFFFFu;
    if (A32(3)) MEM32(A32(3)) = A32(1) >> 16;
    RET(1, 4);
}
static void u_ReuseDDElParam(void) { RET((A32(4) & 0xFFFFu) << 16 | (A32(3) & 0xFFFFu), 5); }

static void u_SetMenuItemBitmaps(void)     { RET(1, 5); }
static void u_SetParent(void) {                        /* (hwnd, parent): the old parent; not moved */
    static int said;
    if (!said++) fprintf(stderr, "[mtm] SetParent(%08X, %08X): not moved\n", A32(0), A32(1));
    RET(hle_window_parent(A32(0)), 2);
}
static void u_SetWindowContextHelpId(void) { RET(1, 2); }
static void u_SetWindowRgn(void)           { RET(1, 3); }
static void u_ShowOwnedPopups(void)        { RET(1, 2); }
static void u_TranslateMDISysAccel(void)   { RET(0, 2); }
static void u_UnregisterClassA(void)       { RET(1, 2); }

/* ================================================================ GDI32 */
/* MM_TEXT mapping, no clipping region beyond the window, one 640x480 screen. */

static void g_ret1_1(void) { RET(1, 1); }
static void g_ret1_2(void) { RET(1, 2); }
static void g_ret1_3(void) { RET(1, 3); }
static void g_ret1_4(void) { RET(1, 4); }
static void g_ret1_5(void) { RET(1, 5); }
static void g_ret0_1(void) { RET(0, 1); }
static void g_ret0_2(void) { RET(0, 2); }
static void g_ret0_3(void) { RET(0, 3); }
static void g_ret0_4(void) { RET(0, 4); }
static void g_ret0_5(void) { RET(0, 5); }
static void g_region_3(void) { RET(SIMPLEREGION, 3); }
static void g_region_4(void) { RET(SIMPLEREGION, 4); }
static void g_region_5(void) { RET(SIMPLEREGION, 5); }

static void g_GetClipBox(void) {                       /* (hdc, &rect) */
    if (A32(1)) MEM32(A32(1)) = 0, MEM32(A32(1) + 4) = 0, MEM32(A32(1) + 8) = 640, MEM32(A32(1) + 12) = 480;
    RET(SIMPLEREGION, 2);
}
static void g_GetRgnBox(void) {
    if (A32(1)) memset(hle_ptr(A32(1)), 0, 16);
    RET(SIMPLEREGION, 2);
}
/* The get/set of an extent or origin: a POINT/SIZE out; extents 1x1, origins 0. */
static void g_get_ext(void) {                          /* (hdc, &size) */
    if (A32(1)) MEM32(A32(1)) = 1, MEM32(A32(1) + 4) = 1;
    RET(1, 2);
}
static void g_get_org(void) {
    if (A32(1)) MEM32(A32(1)) = 0, MEM32(A32(1) + 4) = 0;
    RET(1, 2);
}
static void g_set_ext_4(void) {                        /* (hdc, x, y, &old) */
    if (A32(3)) MEM32(A32(3)) = 1, MEM32(A32(3) + 4) = 1;
    RET(1, 4);
}
static void g_set_org_4(void) {
    if (A32(3)) MEM32(A32(3)) = 0, MEM32(A32(3) + 4) = 0;
    RET(1, 4);
}
static void g_scale_ext_6(void) {                      /* (hdc, xn, xd, yn, yd, &old) */
    if (A32(5)) MEM32(A32(5)) = 1, MEM32(A32(5) + 4) = 1;
    RET(1, 6);
}
static void g_GetMapMode(void)       { RET(1, 1); }    /* MM_TEXT */
static void g_SetMapMode(void)       { RET(1, 2); }
static void g_GetROP2(void)          { RET(13, 1); }   /* R2_COPYPEN */
static void g_SetROP2(void)          { RET(13, 2); }
static void g_GetStretchBltMode(void){ RET(1, 1); }
static void g_SetStretchBltMode(void){ RET(1, 2); }
static void g_GetPolyFillMode(void)  { RET(1, 1); }
static void g_SetPolyFillMode(void)  { RET(1, 2); }
static void g_GetTextAlign(void)     { RET(0, 1); }
static void g_GetNearestColor(void)  { RET(A32(1) & 0xFFFFFFu, 2); }
static void g_GetObjectType(void)    { RET(0, 1); }
static void g_GetTextFaceA(void) {                     /* (hdc, n, buf) */
    const char *face = "MS Sans Serif";
    uint32_t n = A32(1), len = (uint32_t)strlen(face);
    if (A32(2) && n) { snprintf((char *)hle_ptr(A32(2)), n, "%s", face); RET(len < n ? len : n - 1, 3); }
    RET(len + 1, 3);
}
static void g_GetCharWidthA(void) {                    /* (hdc, first, last, int*) */
    uint32_t mark = hle_scratch_mark(), ch = hle_scratch(4), sz = hle_scratch(8);
    for (uint32_t c = A32(1), i = 0; c <= A32(2) && A32(3); c++, i++) {
        MEM8(ch) = (uint8_t)c;
        uint32_t a[4] = { A32(0), ch, 1, sz };
        call("GetTextExtentPoint32A", 4, a);
        MEM32(A32(3) + 4 * i) = MEM32(sz);
    }
    hle_scratch_release(mark);
    RET(1, 4);
}
static void g_ExtTextOutA(void) {                      /* (hdc, x, y, opts, rect, str, n, dx) */
    if (A32(5) && A32(6)) {
        uint32_t a[5] = { A32(0), A32(1), A32(2), A32(5), A32(6) };
        call("TextOutA", 5, a);
    }
    RET(1, 8);
}
static void g_GetCurrentPositionEx(void) { if (A32(1)) MEM32(A32(1)) = 0, MEM32(A32(1) + 4) = 0; RET(1, 2); }
static void g_ExtCreatePen(void) {                     /* (style, width, LOGBRUSH*, n, styles) */
    uint32_t a[3] = { A32(0) & 0xFu, A32(1), A32(2) ? MEM32(A32(2) + 4) : 0 };
    RET(call("CreatePen", 3, a), 5);
}
static void g_pattern_brush(int n) {
    uint32_t gray = 0x808080u;
    uint32_t b = call("CreateSolidBrush", 1, &gray);
    RET(b, (uint32_t)n);
}
static void g_CreatePatternBrush(void)      { g_pattern_brush(1); }
static void g_CreateDIBPatternBrushPt(void) { g_pattern_brush(2); }
static void g_LPtoDP(void)          { RET(1, 3); }
static void g_GetDIBColorTable(void){ RET(0, 4); }
static void g_StartDocA(void)       { RET((uint32_t)-1, 2); }   /* no printer */
static void g_Escape(void)          { RET(0, 5); }
static void g_SetTextCharacterExtra(void) { RET(0, 2); }
static void g_SetTextJustification(void)  { RET(1, 3); }
static void g_SetMapperFlags(void)  { RET(0, 2); }
static void g_ExtCreateRegion(void) { RET(0, 3); }
static void g_GetRegionData(void)   { RET(0, 3); }
static void g_ArcTo(void)           { RET(1, 9); }
static void g_PolyDraw(void)        { RET(1, 4); }

/* ================================================================ KERNEL32 */

static void k_GetShortPathNameA(void) {                /* (long, short, n): the same name */
    const char *s = (const char *)hle_ptr(A32(0));
    uint32_t len = s ? (uint32_t)strlen(s) : 0;
    if (!A32(1) || A32(2) <= len) RET(len + 1, 3);
    memcpy(hle_ptr(A32(1)), s, len + 1);
    RET(len, 3);
}
static void k_HeapLockUnlock(void) { RET(1, 1); }
static void k_HeapWalk(void)       { hle_set_last_error(259); RET(0, 2); }   /* ERROR_NO_MORE_ITEMS */
static void k_IsDBCSLeadByte(void) { RET(0, 1); }
static void k_SuspendThread(void)  { RET(0, 1); }

const win32hle_shim mtm_extra_shims_ui[] = {
    /* USER32 */
    { "GetLastActivePopup", u_GetLastActivePopup }, { "IsChild", u_IsChild }, { "IsIconic", u_IsIconic },
    { "GetWindowThreadProcessId", u_GetWindowThreadProcessId },
    { "SetPropA", u_SetPropA }, { "GetPropA", u_GetPropA }, { "RemovePropA", u_RemovePropA },
    { "CharNextA", u_CharNextA }, { "CharPrevA", u_CharPrevA },
    { "CharToOemA", u_CharToOemA }, { "OemToCharA", u_CharToOemA },
    { "CheckRadioButton", u_CheckRadioButton },
    { "DestroyCursor", u_DestroyIconOrCursor }, { "DestroyIcon", u_DestroyIconOrCursor },
    { "GetDCEx", u_GetDCEx }, { "GetWindowDC", u_GetWindowDC },
    { "GetDialogBaseUnits", u_GetDialogBaseUnits }, { "MapDialogRect", u_MapDialogRect },
    { "GetDlgItemInt", u_GetDlgItemInt }, { "SetDlgItemInt", u_SetDlgItemInt },
    { "GetMenuCheckMarkDimensions", u_GetMenuCheckMarkDimensions }, { "GetMenuStringA", u_GetMenuStringA },
    { "GetMessagePos", u_GetMessagePos }, { "GetMessageTime", u_GetMessageTime },
    { "GetScrollInfo", u_GetScrollInfo }, { "GetScrollPos", u_GetScrollPos }, { "GetScrollRange", u_GetScrollRange },
    { "SetScrollInfo", u_SetScrollInfo }, { "SetScrollPos", u_SetScrollPos }, { "SetScrollRange", u_SetScrollRange },
    { "ShowScrollBar", u_ShowScrollBar }, { "ScrollWindow", u_ScrollWindow }, { "ScrollWindowEx", u_ScrollWindowEx },
    { "GetSystemMenu", u_GetSystemMenu },
    { "GetTabbedTextExtentA", u_GetTabbedTextExtentA }, { "TabbedTextOutA", u_TabbedTextOutA },
    { "GrayStringA", u_GrayStringA }, { "InvertRect", u_InvertRect },
    { "GetWindowPlacement", u_GetWindowPlacement }, { "SetWindowPlacement", u_SetWindowPlacement },
    { "UnpackDDElParam", u_UnpackDDElParam }, { "ReuseDDElParam", u_ReuseDDElParam },
    { "SetMenuItemBitmaps", u_SetMenuItemBitmaps }, { "SetParent", u_SetParent },
    { "SetWindowContextHelpId", u_SetWindowContextHelpId }, { "SetWindowRgn", u_SetWindowRgn },
    { "ShowOwnedPopups", u_ShowOwnedPopups }, { "TranslateMDISysAccel", u_TranslateMDISysAccel },
    { "UnregisterClassA", u_UnregisterClassA },
    /* GDI32 */
    { "GetClipBox", g_GetClipBox }, { "GetRgnBox", g_GetRgnBox }, { "GetClipRgn", g_ret0_2 },
    { "IntersectClipRect", g_region_5 }, { "ExcludeClipRect", g_region_5 }, { "OffsetClipRgn", g_region_3 },
    { "ExtSelectClipRgn", g_region_3 }, { "CombineRgn", g_region_4 }, { "SetRectRgn", g_ret1_5 },
    { "SelectClipPath", g_ret1_2 }, { "PtVisible", g_ret1_3 }, { "RectVisible", g_ret1_2 },
    { "ExtCreateRegion", g_ExtCreateRegion }, { "GetRegionData", g_GetRegionData },
    { "GetViewportExtEx", g_get_ext }, { "GetWindowExtEx", g_get_ext },
    { "GetViewportOrgEx", g_get_org }, { "GetWindowOrgEx", g_get_org },
    { "SetViewportExtEx", g_set_ext_4 }, { "SetWindowExtEx", g_set_ext_4 },
    { "OffsetViewportOrgEx", g_set_org_4 }, { "OffsetWindowOrgEx", g_set_org_4 },
    { "ScaleViewportExtEx", g_scale_ext_6 }, { "ScaleWindowExtEx", g_scale_ext_6 },
    { "GetMapMode", g_GetMapMode }, { "SetMapMode", g_SetMapMode }, { "LPtoDP", g_LPtoDP },
    { "GetROP2", g_GetROP2 }, { "SetROP2", g_SetROP2 },
    { "GetStretchBltMode", g_GetStretchBltMode }, { "SetStretchBltMode", g_SetStretchBltMode },
    { "GetPolyFillMode", g_GetPolyFillMode }, { "SetPolyFillMode", g_SetPolyFillMode },
    { "GetTextAlign", g_GetTextAlign }, { "GetTextFaceA", g_GetTextFaceA }, { "GetCharWidthA", g_GetCharWidthA },
    { "SetTextCharacterExtra", g_SetTextCharacterExtra }, { "SetTextJustification", g_SetTextJustification },
    { "SetMapperFlags", g_SetMapperFlags }, { "ExtTextOutA", g_ExtTextOutA },
    { "GetNearestColor", g_GetNearestColor }, { "GetObjectType", g_GetObjectType },
    { "GetCurrentPositionEx", g_GetCurrentPositionEx }, { "GetDIBColorTable", g_GetDIBColorTable },
    { "ExtCreatePen", g_ExtCreatePen },
    { "CreatePatternBrush", g_CreatePatternBrush }, { "CreateDIBPatternBrushPt", g_CreateDIBPatternBrushPt },
    { "Ellipse", g_ret1_5 }, { "Polygon", g_ret1_3 }, { "PolylineTo", g_ret1_3 }, { "PolyBezierTo", g_ret1_3 },
    { "PolyDraw", g_PolyDraw }, { "ArcTo", g_ArcTo }, { "SetArcDirection", g_ret1_2 },
    { "SetColorAdjustment", g_ret1_2 },
    { "CreateDCA", g_ret0_4 }, { "StartDocA", g_StartDocA }, { "EndDoc", g_ret0_1 }, { "AbortDoc", g_ret0_1 },
    { "StartPage", g_ret0_1 }, { "EndPage", g_ret0_1 }, { "SetAbortProc", g_ret0_2 }, { "Escape", g_Escape },
    { "EnumMetaFile", g_ret0_4 }, { "PlayMetaFile", g_ret1_2 }, { "PlayMetaFileRecord", g_ret1_4 },
    /* KERNEL32 */
    { "GetShortPathNameA", k_GetShortPathNameA },
    { "HeapLock", k_HeapLockUnlock }, { "HeapUnlock", k_HeapLockUnlock }, { "HeapWalk", k_HeapWalk },
    { "IsDBCSLeadByte", k_IsDBCSLeadByte }, { "SuspendThread", k_SuspendThread },
    { NULL, NULL },
};
