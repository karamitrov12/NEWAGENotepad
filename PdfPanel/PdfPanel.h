#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- Forward declarations ----
struct PdfDocument;
typedef void(CALLBACK *PdfNotifyProc)(const struct PdfNotify* notify);

// ---- Creation / destruction ----
NP_API BOOL PdfInitialize(HINSTANCE hInstance);
// Creates a PDF panel window. Returns HWND or NULL.
NP_API HWND PdfCreatePanel(HWND hParent, const RECT* rc, int id, PdfNotifyProc notifyProc, void* userData);
// Destroys the panel and frees resources.
NP_API BOOL PdfDestroyPanel(HWND hPanel);

// ---- Document handling ----
// Load a PDF file into the panel. Returns TRUE on success.
NP_API BOOL PdfLoadFile(HWND hPanel, const wchar_t* path);
// Close current PDF.
NP_API BOOL PdfCloseFile(HWND hPanel);
// Number of pages in the loaded PDF.
NP_API int PdfGetPageCount(HWND hPanel);
// Set the currently displayed page (0‑based). Invalid indices return FALSE.
NP_API BOOL PdfSetPage(HWND hPanel, int index);
// Render the current page into a DC, clipped by rc (in screen coordinates).
// Returns TRUE on success.
NP_API BOOL PdfRenderPage(HWND hPanel, HDC hdc, const RECT* rc);
 // Search for text. If forward is TRUE, search forward; otherwise backward.
// Results are reported via notification (PDF_NOTIFY_SEARCH_RESULT) or returned via callback.
NP_API BOOL PdfSearchText(HWND hPanel, const wchar_t* text, BOOL forward);

// ---- Notification callbacks ----
struct PdfNotify {
    HWND hPanel;
    int instanceId;
    UINT code;            // PDF_NOTIFY_*
    int pageIndex;        // Page where event occurred (if any)
    void* userData;
};

// Register a notification callback.
NP_API BOOL PdfSetNotifyProc(HWND hPanel, PdfNotifyProc proc, void* userData);

// ---- Theme support ----
#define PDF_THEME_SYSTEM 0
#define PDF_THEME_LIGHT 1
#define PDF_THEME_DARK 2

// Apply a custom palette (same struct as NotepadPanel's NPTheme but extended).
typedef struct PdfTheme {
    COLORREF bgApp;      // PDF viewing area background
    COLORREF bgPanel;    // toolbar, status bar
    COLORREF bgCard;     // page thumbnail / active page background
    COLORREF border;     // page outline
    COLORREF text;       // page text
    COLORREF selBg;      // selected text / highlight
} PdfTheme;

NP_API BOOL PdfSetTheme(HWND hPanel, const PdfTheme* theme);
NP_API BOOL PdfGetTheme(HWND hPanel, PdfTheme* outTheme);
NP_API int PdfGetThemeMode(HWND hPanel);

// ---- DPI awareness ----
// Call on DPI change; the panel will reposition and rescale.
NP_API void PdfUpdateDpi(HWND hPanel, int dpi);

// ---- Misc ----
NP_API BOOL PdfSetReadOnly(HWND hPanel, BOOL readOnly);
NP_API BOOL PdfGetReadOnly(HWND hPanel);

#ifdef __cplusplus
}
#endif

// Notification codes (extend Notepad's definitions)
#define PDF_NOTIFY_CONTENT_CHANGED  0x2001
#define PDF_NOTIFY_PAGE_CHANGED    0x2002
#define PDF_NOTIFY_SEARCH_RESULT   0x2003
#define PDF_NOTIFY_THEME_CHANGED   0x2004

#ifdef __cplusplus
}
#endif
