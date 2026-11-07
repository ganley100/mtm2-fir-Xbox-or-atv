/*
 * extra_shims.c - Windows calls Monster Truck Madness 2 makes that pcrecomp's
 * win32hle does not answer (yet). Registered by mtm_hle.c after win32hle's
 * own, so they only fill gaps; each one is general Win32, not MTM2's own, and
 * belongs upstream in win32hle when it is right there too.
 *
 * Found by running the demo on the Linux host until the next
 * "unimplemented import" (docs/android.md, "When it stops").
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "win32hle.h"

static void hle_log_once_spi(uint32_t action, uint32_t ui) {
    static uint32_t said[64];
    static int n;
    for (int i = 0; i < n; i++) if (said[i] == action) return;
    if (n < 64) said[n++] = action;
    fprintf(stderr, "[mtm] SystemParametersInfoA(0x%X, %u): answered as done, nothing written\n", action, ui);
}

/* GetKeyboardType(which): an enhanced 101/102-key keyboard, 12 function keys. */
static void x_GetKeyboardType(void) {
    uint32_t which = A32(0);
    RET(which == 0 ? 4 : which == 2 ? 12 : 0, 1);
}

/* MSACM32, the audio compression manager: a system with no codecs. Counts
 * are 0, and a format that would need converting can't be ("not
 * possible"), so the game keeps to PCM. */
#define MMSYSERR_INVALHANDLE 5u
#define ACMERR_NOTPOSSIBLE   512u
static void x_acmMetrics(void) {                       /* (hao, metric, out) */
    if (A32(2)) MEM32(A32(2)) = 0;
    RET(0, 3);
}
static void x_acmFormatSuggest(void)       { RET(ACMERR_NOTPOSSIBLE, 5); }
static void x_acmStreamOpen(void)          { if (A32(0)) MEM32(A32(0)) = 0; RET(ACMERR_NOTPOSSIBLE, 8); }
static void x_acmStreamClose(void)         { RET(MMSYSERR_INVALHANDLE, 2); }
static void x_acmStreamConvert(void)       { RET(MMSYSERR_INVALHANDLE, 3); }
static void x_acmStreamPrepareHeader(void) { RET(MMSYSERR_INVALHANDLE, 3); }
static void x_acmStreamUnprepareHeader(void) { RET(MMSYSERR_INVALHANDLE, 3); }

/* SystemParametersInfoA(action, uiParam, pvParam, winini): the queries an
 * MFC program makes. The desktop is the game's 640x480 (the host shows that
 * scaled to the screen); the UI font is MS Sans Serif 8, as on Windows 9x.
 * Anything else answers "done" without writing and is named in the log. */
static void logfont(uint32_t lf, int height, int weight) {
    memset(hle_ptr(lf), 0, 60);
    MEM32(lf) = (uint32_t)height;                    /* lfHeight */
    MEM32(lf + 16) = (uint32_t)weight;               /* lfWeight */
    strcpy((char *)hle_ptr(lf + 28), "MS Sans Serif"); /* lfFaceName */
}
static void x_SystemParametersInfoA(void) {
    uint32_t action = A32(0), ui = A32(1), pv = A32(2);
    int w, h, bpp;
    hle_dd_mode(&w, &h, &bpp);
    if (w <= 0 || h <= 0) w = 640, h = 480;
    switch (action) {
    case 0x30:                                       /* SPI_GETWORKAREA: a RECT */
        if (pv) MEM32(pv) = 0, MEM32(pv + 4) = 0, MEM32(pv + 8) = (uint32_t)w, MEM32(pv + 12) = (uint32_t)h;
        RET(1, 4);
    case 0x29: {                                     /* SPI_GETNONCLIENTMETRICS */
        if (!pv) RET(0, 4);
        uint32_t size = MEM32(pv) ? MEM32(pv) : 340;
        memset(hle_ptr(pv + 4), 0, size - 4);
        MEM32(pv + 4) = 1, MEM32(pv + 8) = 16, MEM32(pv + 12) = 16;   /* border, scroll bar */
        MEM32(pv + 16) = 18, MEM32(pv + 20) = 18;                   /* caption button */
        logfont(pv + 24, -11, 700);                                 /* caption */
        MEM32(pv + 84) = 12, MEM32(pv + 88) = 15;
        logfont(pv + 92, -11, 400);                                 /* small caption */
        MEM32(pv + 152) = 18, MEM32(pv + 156) = 18;
        logfont(pv + 160, -11, 400);                                /* menu */
        logfont(pv + 220, -11, 400);                                /* status */
        logfont(pv + 280, -11, 400);                                /* message */
        RET(1, 4);
    }
    case 0x1F:                                       /* SPI_GETICONTITLELOGFONT */
        if (pv) logfont(pv, -11, 400);
        RET(1, 4);
    case 0x10: case 0x0E: case 0x0A: case 0x16:      /* screen saver active / timeout, keyboard speed, delay */
    case 0x03: case 0x68: case 0x26:                 /* mouse, wheel lines, icon spacing ... */
        if (pv) MEM32(pv) = action == 0x10 ? 0 : action == 0x68 ? 3 : 0;
        RET(1, 4);
    case 0x11: case 0x0F: case 0x0B: case 0x17: case 0x61:          /* the matching SETs, power ... */
        RET(1, 4);
    default:
        hle_log_once_spi(action, ui);
        RET(1, 4);
    }
}

/* DPLAYX (by ordinal): DirectPlay with no service providers. Single-player
 * needs none of it; the game asks whether a lobby launched it. */
#define DPERR_UNAVAILABLE 0x88770442u                   /* MAKE_DPHRESULT(1090) */
static void x_DirectPlayCreate(void) {                  /* #1 (guid, IDirectPlay**, outer) */
    if (A32(1)) MEM32(A32(1)) = 0;
    RET(DPERR_UNAVAILABLE, 3);
}
static void x_DirectPlayEnumerateA(void) { RET(0, 2); } /* #2 (callback, context): none to list */
static void x_DirectPlayLobbyCreateA(void) {            /* #4 (guid, IDirectPlayLobby**, outer, data, size) */
    if (A32(1)) MEM32(A32(1)) = 0;
    RET(DPERR_UNAVAILABLE, 5);
}

/* SHELL32: nothing is ever dropped on the window; no icons from files. */
static void x_DragAcceptFiles(void) { RETV(2); }
static void x_DragFinish(void)      { RETV(1); }
static void x_DragQueryFileA(void)  { RET(0, 4); }
static void x_ExtractIconA(void)    { RET(0, 3); }
static void x_SHGetFileInfoA(void)  { RET(0, 5); }

/* IMM32: no input method editor (win32hle answers the rest of IMM32). */
static void x_ImmCreateContext(void)       { RET(0x1E000001u, 0); }
static void x_ImmDestroyContext(void)      { RET(1, 1); }
static void x_ImmSetCompositionFontA(void) { RET(0, 2); }

/* WINSPOOL: no printers. */
static void x_OpenPrinterA(void)        { if (A32(1)) MEM32(A32(1)) = 0; RET(0, 3); }
static void x_ClosePrinter(void)        { RET(1, 1); }
static void x_DocumentPropertiesA(void) { RET((uint32_t)-1, 6); }

/* COMDLG32: the file and print dialogs are cancelled (MFC's File menu,
 * which the game hides). GetFileTitleA is answered properly. */
static void x_CancelledDialog(void)      { RET(0, 1); }
static void x_CommDlgExtendedError(void) { RET(0, 0); }
static void x_GetFileTitleA(void) {                    /* (path, buf, size) */
    const char *p = ASTR(0), *t = p;
    for (const char *c = p; c && *c; c++) if (*c == '\\' || *c == '/' || *c == ':') t = c + 1;
    uint32_t need = t ? (uint32_t)strlen(t) + 1 : 1;
    if (!A32(1) || A32(2) < need) RET(need, 3);
    memcpy(ASTR(1), t ? t : "", need);
    RET(0, 3);
}

/* COMCTL32's image lists (MFC's CImageList): handles that hold nothing and
 * draw nothing; the game's own front end draws its pictures with GDI. */
static uint32_t g_next_il = 0x1D000001u;
static void x_ImageList_Create(void)       { RET(g_next_il++, 5); }
static void x_ImageList_LoadImageA(void)   { RET(g_next_il++, 7); }
static void x_ImageList_Merge(void)        { RET(g_next_il++, 6); }
static void x_ImageList_Read(void)         { RET(0, 1); }
static void x_ImageList_Write(void)        { RET(0, 2); }
static void x_ImageList_Add(void)          { RET(0, 3); }
static void x_ImageList_Draw(void)         { RET(1, 6); }
static void x_ImageList_GetImageInfo(void) { RET(0, 3); }
static void x_MakeDragList(void)           { RET(0, 1); }          /* comctl32 #13 */
static void x_LBItemFromPt(void)           { RET((uint32_t)-1, 4); } /* #14 (hwnd, POINT, scroll) */

/* ADVAPI32's older registry calls, and file security (FAT has none). */
static void x_RegQueryValueA(void)  { RET(2, 4); }     /* ERROR_FILE_NOT_FOUND: the game falls back to defaults */
static void x_RegSetValueA(void)    { RET(0, 5); }
static void x_GetFileSecurityA(void){ hle_set_last_error(50); RET(0, 5); }   /* ERROR_NOT_SUPPORTED */
static void x_SetFileSecurityA(void){ RET(1, 3); }

/* WINMM's MCI by command: no CD audio device (the demo plays its music
 * from Musicx.pod; monsterx.ini has UseRedBook=NOPE). */
static void x_mciSendCommandA(void) { RET(306, 4); }  /* MCIERR_DEVICE_NOT_INSTALLED */

const win32hle_shim mtm_extra_shims[] = {
    { "DragAcceptFiles", x_DragAcceptFiles },
    { "DragFinish", x_DragFinish },
    { "DragQueryFileA", x_DragQueryFileA },
    { "ExtractIconA", x_ExtractIconA },
    { "SHGetFileInfoA", x_SHGetFileInfoA },
    { "ImmCreateContext", x_ImmCreateContext },
    { "ImmDestroyContext", x_ImmDestroyContext },
    { "ImmSetCompositionFontA", x_ImmSetCompositionFontA },
    { "OpenPrinterA", x_OpenPrinterA },
    { "ClosePrinter", x_ClosePrinter },
    { "DocumentPropertiesA", x_DocumentPropertiesA },
    { "GetOpenFileNameA", x_CancelledDialog },
    { "GetSaveFileNameA", x_CancelledDialog },
    { "PageSetupDlgA", x_CancelledDialog },
    { "PrintDlgA", x_CancelledDialog },
    { "CommDlgExtendedError", x_CommDlgExtendedError },
    { "GetFileTitleA", x_GetFileTitleA },
    { "ImageList_Create", x_ImageList_Create },
    { "ImageList_LoadImageA", x_ImageList_LoadImageA },
    { "ImageList_Merge", x_ImageList_Merge },
    { "ImageList_Read", x_ImageList_Read },
    { "ImageList_Write", x_ImageList_Write },
    { "ImageList_Add", x_ImageList_Add },
    { "ImageList_Draw", x_ImageList_Draw },
    { "ImageList_GetImageInfo", x_ImageList_GetImageInfo },
    { "comctl32.dll#13", x_MakeDragList },
    { "comctl32.dll#14", x_LBItemFromPt },
    { "RegQueryValueA", x_RegQueryValueA },
    { "RegSetValueA", x_RegSetValueA },
    { "GetFileSecurityA", x_GetFileSecurityA },
    { "SetFileSecurityA", x_SetFileSecurityA },
    { "mciSendCommandA", x_mciSendCommandA },
    { "dplayx.dll#1", x_DirectPlayCreate },
    { "dplayx.dll#2", x_DirectPlayEnumerateA },
    { "dplayx.dll#4", x_DirectPlayLobbyCreateA },
    { "SystemParametersInfoA", x_SystemParametersInfoA },
    { "GetKeyboardType", x_GetKeyboardType },
    { "acmMetrics", x_acmMetrics },
    { "acmFormatSuggest", x_acmFormatSuggest },
    { "acmStreamOpen", x_acmStreamOpen },
    { "acmStreamClose", x_acmStreamClose },
    { "acmStreamConvert", x_acmStreamConvert },
    { "acmStreamPrepareHeader", x_acmStreamPrepareHeader },
    { "acmStreamUnprepareHeader", x_acmStreamUnprepareHeader },
    { NULL, NULL },
};
