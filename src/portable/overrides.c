/*
 * overrides.c - win32hle shims answered differently here, registered ahead
 * of win32hle's own (mtm_hle.c), so these win. Each is a fix to send
 * upstream to pcrecomp; once win32hle has it, it comes out of this file.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "win32hle.h"

/* GetFullPathNameA(name, n, buf, &filePart). win32hle's joins a relative
 * name to the host's working folder ("/home/.../game/x"), a path the guest
 * then hands back to CreateFileA as if it were its own. Here a relative
 * name is joined to the guest's current folder (GetCurrentDirectoryA, "C:\"
 * for the game folder), with "." and ".." resolved, in Windows' form. */
static void o_GetFullPathNameA(void) {
    const char *name = A32(0) ? (const char *)hle_ptr(A32(0)) : "";
    char path[1024];
    if ((name[0] && name[1] == ':' && (name[2] == '\\' || name[2] == '/')) || (name[0] == '\\' && name[1] == '\\')) {
        snprintf(path, sizeof path, "%s", name);                 /* absolute */
    } else {
        uint32_t mark = hle_scratch_mark(), cwd = hle_scratch(512);
        uint32_t a[2] = { 512, cwd };
        hle_call_guest(hle_resolve("GetCurrentDirectoryA"), 2, a);
        const char *c = (const char *)hle_ptr(cwd);
        if (name[0] == '\\' || name[0] == '/') snprintf(path, sizeof path, "%.2s%s", c, name);   /* rooted: the drive */
        else if (name[0] && name[1] == ':') snprintf(path, sizeof path, "%.2s\\%s", name, name + 2); /* "C:x" */
        else snprintf(path, sizeof path, "%s%s%s", c, c[0] && c[strlen(c) - 1] != '\\' ? "\\" : "", name);
        hle_scratch_release(mark);
    }
    /* One separator, and "." and ".." resolved, after the "X:\" (or "\\server\"). */
    char out[1024];
    size_t k = 0, root;
    for (const char *p = path; *p && k + 1 < sizeof out; p++) out[k++] = *p == '/' ? '\\' : *p;
    out[k] = 0;
    root = out[1] == ':' ? 3 : 2;
    if (strlen(out) < root) { out[root - 1] = '\\'; out[root] = 0; }
    char res[1024];
    size_t r = root;
    memcpy(res, out, root);
    for (const char *p = out + root; *p;) {
        const char *e = strchr(p, '\\');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len == 0 || (len == 1 && p[0] == '.')) {
            /* nothing */
        } else if (len == 2 && p[0] == '.' && p[1] == '.') {
            while (r > root && res[r - 1] == '\\') r--;
            while (r > root && res[r - 1] != '\\') r--;
        } else {
            if (r > root && res[r - 1] != '\\') res[r++] = '\\';
            if (r + len + 1 >= sizeof res) break;
            memcpy(res + r, p, len), r += len;
        }
        if (!e) break;
        p = e + 1;
    }
    if (k && (out[k - 1] == '\\') && r > root && res[r - 1] != '\\') res[r++] = '\\';   /* keep a trailing separator */
    res[r] = 0;

    uint32_t n = A32(1), len = (uint32_t)strlen(res);
    char *buf = A32(2) ? (char *)hle_ptr(A32(2)) : NULL;
    if (!buf || n <= len) RET(len + 1, 4);                       /* the size it needs */
    memcpy(buf, res, len + 1);
    if (A32(3)) {
        char *slash = strrchr(buf, '\\');
        MEM32(A32(3)) = slash && slash[1] ? A32(2) + (uint32_t)(slash + 1 - buf) : 0;
    }
    RET(len, 4);
}

/* ------------------------------------------------------------------ MDI */

/* The MDI client's window procedure ("hle.MDIClient", the class win32hle
 * registers). win32hle's answers WM_MDIGETACTIVE with the first child it
 * finds; Windows answers the active one, which is the child created (or
 * activated) last. MTM2 creates its front end's child, then the game's, and
 * looks for the game's document through the active child: with the first
 * one it found none, and called IsKindOf on NULL. This one keeps the order
 * the children became active in, and sends WM_MDIACTIVATE as Windows does,
 * which MFC's child frames use to make their view the active one. It also
 * gives the child the MDICREATESTRUCT as its creation parameter (below). */
uint32_t hle_create_window(uint32_t exstyle, const char *cls, const char *title, uint32_t style, int px, int py,
                           int pw, int ph, uint32_t parent, uint32_t menu, uint32_t hinst, uint32_t param);

#define MAX_MDI 32
static struct { uint32_t client, child; } g_mdi[MAX_MDI];   /* oldest first; the last live one is active */
static int g_nmdi;

static void mdi_forget(uint32_t child) {
    for (int i = 0; i < g_nmdi; i++)
        if (g_mdi[i].child == child) { memmove(&g_mdi[i], &g_mdi[i + 1], (size_t)(g_nmdi - i - 1) * sizeof g_mdi[0]); g_nmdi--; i--; }
}
static uint32_t mdi_active(uint32_t client) {
    for (int i = g_nmdi - 1; i >= 0; i--) {
        if (g_mdi[i].client != client) continue;
        if (hle_is_window(g_mdi[i].child)) return g_mdi[i].child;
        mdi_forget(g_mdi[i].child);                     /* destroyed without WM_MDIDESTROY */
    }
    return 0;
}
static uint32_t def_proc(uint32_t h, uint32_t m, uint32_t w, uint32_t l) {
    uint32_t a[4] = { h, m, w, l };
    return hle_call_guest(hle_resolve("DefWindowProcA"), 4, a);
}
static void mdi_activate(uint32_t client, uint32_t child) {
    uint32_t old = mdi_active(client);
    if (!child || child == old) return;
    mdi_forget(child);
    if (g_nmdi < MAX_MDI) g_mdi[g_nmdi].client = client, g_mdi[g_nmdi++].child = child;
    if (old) hle_send(old, 0x0222u, old, child);        /* WM_MDIACTIVATE: (deactivated, activated) */
    hle_send(child, 0x0222u, old, child);
}

static void o_MDIClient(void) {
    uint32_t h = A32(0), m = A32(1), w = A32(2), l = A32(3);
    switch (m) {
    case 0x0220u: {                                      /* WM_MDICREATE: MDICREATESTRUCT* */
        if (!l) RET(0, 4);
        uint32_t c = MEM32(l);
        const char *cls = c < 0x10000u ? (const char *)(uintptr_t)c : (const char *)hle_ptr(c);
        const char *title = MEM32(l + 4) ? (const char *)hle_ptr(MEM32(l + 4)) : "";
        uint32_t child = hle_create_window(0x40u /* WS_EX_MDICHILD */, cls, title,
            0x50000000u | MEM32(l + 28) /* WS_CHILD | WS_VISIBLE | its style */,
            (int)MEM32(l + 12), (int)MEM32(l + 16), (int)MEM32(l + 20), (int)MEM32(l + 24),
            h, 0, MEM32(l + 8), l);
        /* lpCreateParams is the MDICREATESTRUCT itself, as on Windows: MFC's
         * CMDIChildWnd::OnCreate takes its CCreateContext (which view to
         * make) from that struct's lParam. win32hle passes the lParam, and
         * the child frame came up with no view. */
        if (child) mdi_activate(h, child);
        RET(child, 4);
    }
    case 0x0229u: {                                      /* WM_MDIGETACTIVE(0, &maximized) */
        if (l) MEM32(l) = 0;
        RET(mdi_active(h), 4);
    }
    case 0x0222u:                                        /* WM_MDIACTIVATE(child) */
        mdi_activate(h, w);
        RET(0, 4);
    case 0x0221u: {                                      /* WM_MDIDESTROY(child) */
        uint32_t was = mdi_active(h);
        mdi_forget(w);
        uint32_t a = w;
        hle_call_guest(hle_resolve("DestroyWindow"), 1, &a);
        uint32_t now = mdi_active(h);
        if (now && now != was) hle_send(now, 0x0222u, w, now);
        RET(0, 4);
    }
    case 0x0224u: {                                      /* WM_MDINEXT: the oldest becomes active */
        for (int i = 0; i < g_nmdi; i++)
            if (g_mdi[i].client == h && hle_is_window(g_mdi[i].child)) { mdi_activate(h, g_mdi[i].child); break; }
        RET(0, 4);
    }
    case 0x0223u: case 0x0225u:                          /* WM_MDIRESTORE, WM_MDIMAXIMIZE */
    case 0x0226u: case 0x0227u: case 0x0228u:            /* TILE, CASCADE, ICONARRANGE */
    case 0x0234u:                                        /* WM_MDIREFRESHMENU */
        RET(0, 4);
    case 0x0230u:                                        /* WM_MDISETMENU: the previous frame menu */
        RET(0, 4);
    default:
        RET(def_proc(h, m, w, l), 4);
    }
}

/* ------------------------------------------------------------ owner draw */

/* WM_DRAWITEM carries the DC a control is painted with. win32hle's is a
 * placeholder handle (0x0DC00001) that no GDI call knows, so MTM2's
 * owner-drawn menu buttons drew nowhere. mtm_owner_draw_fix, called for
 * every message before its window procedure (mtm_hle.c), puts a window DC
 * there instead, with its viewport origin at the button's place on the
 * screen, so what the game draws at the button's (0,0) lands on it. One DC,
 * reused: the game never releases an owner-draw DC. */
#define WM_DRAWITEM   0x002Bu
#define PLACEHOLDER_DC 0x0DC00001u
static uint32_t g_draw_dc;
void mtm_owner_draw_fix(uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp) {
    (void)hwnd; (void)wp;
    if (msg != WM_DRAWITEM || !lp || MEM32(lp + 24) != PLACEHOLDER_DC) return;
    uint32_t item = MEM32(lp + 20);                     /* DRAWITEMSTRUCT.hwndItem */
    if (!g_draw_dc) { uint32_t none = 0; g_draw_dc = hle_call_guest(hle_resolve("GetDC"), 1, &none); }
    if (!g_draw_dc) return;
    int l = 0, t = 0, r = 0, b = 0;
    hle_window_screen_rect(item, &l, &t, &r, &b);
    uint32_t a[4] = { g_draw_dc, (uint32_t)l, (uint32_t)t, 0 };
    hle_call_guest(hle_resolve("SetViewportOrgEx"), 4, a);
    MEM32(lp + 24) = g_draw_dc;
}

/* ------------------------------------------------------------ the originals */

/* win32hle's own shim for a name these override. Shims sit at
 * 0x7F000000 + 16 * their registration index (host_lite.c), these first, so
 * the original is the later one of the same name. */
static uint32_t orig(const char *name) {
    static struct { const char *name; uint32_t va; } cache[16];
    for (int i = 0; i < 16 && cache[i].name; i++) if (!strcmp(cache[i].name, name)) return cache[i].va;
    int seen = 0;
    for (uint32_t va = 0x7F000000u; ; va += 16u) {
        const char *n = hle_name(va);
        if (!n) break;
        if (!strcmp(n, name) && seen++) {
            for (int i = 0; i < 16; i++) if (!cache[i].name) { cache[i].name = name, cache[i].va = va; break; }
            return va;
        }
    }
    return 0;
}
static uint32_t call_orig(const char *name, int n, const uint32_t *args) {
    uint32_t va = orig(name);
    return va ? hle_call_guest(va, n, args) : 0;
}

/* ------------------------------------------------------------ palettes */

/* CreateDIBSection(hdc, bmi, usage, &bits, section, offset) with usage
 * DIB_PAL_COLORS: the colour table is WORD indices into the palette selected
 * into hdc, not RGB. win32hle reads every table as RGB, and MTM2's menu
 * bitmaps came out as noise. Here such a table is turned into RGB from that
 * palette first. SelectPalette is followed to know which palette it is. */
#define MAX_DCPAL 64
static struct { uint32_t dc, pal; } g_dcpal[MAX_DCPAL];
static void o_SelectPalette(void) {                      /* (hdc, hpal, background) */
    uint32_t dc = A32(0), pal = A32(1);
    int free_slot = -1, i;
    for (i = 0; i < MAX_DCPAL; i++) {
        if (g_dcpal[i].dc == dc) break;
        if (!g_dcpal[i].dc && free_slot < 0) free_slot = i;
    }
    if (i == MAX_DCPAL) i = free_slot;
    if (i >= 0) g_dcpal[i].dc = dc, g_dcpal[i].pal = pal;
    uint32_t a[3] = { A32(0), A32(1), A32(2) };
    RET(call_orig("SelectPalette", 3, a), 3);
}
static uint32_t dc_palette(uint32_t dc) {
    for (int i = 0; i < MAX_DCPAL; i++) if (g_dcpal[i].dc == dc) return g_dcpal[i].pal;
    return 0;
}

static void o_CreateDIBSection(void) {
    uint32_t dc = A32(0), bmi = A32(1), usage = A32(2);
    uint32_t a[6] = { A32(0), A32(1), A32(2), A32(3), A32(4), A32(5) };
    uint32_t hsize = bmi ? MEM32(bmi) : 0;
    uint32_t bpp = bmi ? (MEM32(bmi + 12) >> 16) : 0;
    if (usage != 1 || !bmi || bpp > 8 || hsize < 40) RET(call_orig("CreateDIBSection", 6, a), 6);
    uint32_t n = MEM32(bmi + 32) ? MEM32(bmi + 32) : 1u << bpp;
    if (n > 256) n = 256;
    uint32_t mark = hle_scratch_mark();
    uint32_t nb = hle_scratch(hsize + 256 * 4), pe = hle_scratch(256 * 4);
    memcpy(hle_ptr(nb), hle_ptr(bmi), hsize);
    uint32_t pal = dc_palette(dc), got = 0;
    if (pal) { uint32_t g[4] = { pal, 0, 256, pe }; got = hle_call_guest(hle_resolve("GetPaletteEntries"), 4, g); }
    if (!got) { uint32_t g[4] = { dc, 0, 256, pe }; got = hle_call_guest(hle_resolve("GetSystemPaletteEntries"), 4, g); }
    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = MEM16(bmi + hsize + 2 * i) & 0xFFu;
        uint32_t e = got ? MEM32(pe + 4 * idx) : idx * 0x010101u;   /* PALETTEENTRY r, g, b, flags */
        MEM32(nb + hsize + 4 * i) = (e & 0xFFu) << 16 | (e & 0xFF00u) | (e >> 16 & 0xFFu);   /* RGBQUAD b, g, r */
    }
    a[1] = nb, a[2] = 0;                                 /* DIB_RGB_COLORS */
    uint32_t h = call_orig("CreateDIBSection", 6, a);
    hle_scratch_release(mark);
    RET(h, 6);
}

/* ------------------------------------------------------------ painting */

/* BeginPaint(hwnd, &ps). Windows sends WM_ERASEBKGND from it, with the paint
 * DC, before the window paints; MTM2's menu screens draw their background
 * picture there. win32hle sends none, and the menu came up black behind its
 * buttons. The DC's origin is also put at the window's place on the screen,
 * as a window DC's is on Windows (win32hle's are all at the screen's corner). */
static void o_BeginPaint(void) {
    uint32_t hwnd = A32(0), ps = A32(1);
    uint32_t a[2] = { hwnd, ps };
    uint32_t dc = call_orig("BeginPaint", 2, a);
    if (dc) {
        int l = 0, t = 0, r = 0, b = 0;
        if (hle_window_screen_rect(hwnd, &l, &t, &r, &b) && (l || t)) {
            uint32_t o[4] = { dc, (uint32_t)l, (uint32_t)t, 0 };
            hle_call_guest(hle_resolve("SetViewportOrgEx"), 4, o);
        }
        uint32_t erased = hle_send(hwnd, 0x0014u /* WM_ERASEBKGND */, dc, 0);
        if (ps) MEM32(ps + 4) = erased ? 0 : 1;          /* PAINTSTRUCT.fErase: still to erase */
    }
    RET(dc, 2);
}

const win32hle_shim mtm_overrides[] = {
    { "GetFullPathNameA", o_GetFullPathNameA },
    { "hle.MDIClient", o_MDIClient },
    { "SelectPalette", o_SelectPalette },
    { "BeginPaint", o_BeginPaint },
    { "CreateDIBSection", o_CreateDIBSection },
    { NULL, NULL },
};
