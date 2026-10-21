#include "PdfPanel.h"
#include <windows.h>
#include <commctrl.h>
#include <string>

struct PdfDocument {
    HWND hPanel = nullptr;
    void* doc = nullptr; // placeholder for PDF engine handle
    int pageCount = 0;
    int currentPage = 0;
};

static const wchar_t PDF_PANEL_CLASS[] = L"PdfPanelClass";
static HINSTANCE g_hInst = nullptr;

static BOOL RegisterPdfPanelClass(HINSTANCE hInst) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInst;
    wc.lpszClassName = PDF_PANEL_CLASS;
    return RegisterClassW(&wc);
}

extern "C" {

NP_API BOOL PdfInitialize(HINSTANCE hInstance) {
    g_hInst = hInstance;
    return RegisterPdfPanelClass(hInst);
}

// Helper to get document pointer from window GWLP_USERDATA
static PdfDocument* GetDocFromWindow(HWND hWnd) {
    return reinterpret_cast<PdfDocument*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
}

NP_API HWND PdfCreatePanel(HWND hParent, const RECT* rc, int id, PdfNotifyProc notifyProc, void* userData) {
    if (!g_hInst) return nullptr;
    RegisterPdfPanelClass(g_hInst);

    PdfDocument* doc = new PdfDocument();
    doc->hPanel = nullptr; // will be set after window creation
    doc->doc = nullptr;
    doc->pageCount = 0;
    doc->currentPage = 0;

    HWND hWnd = CreateWindowExW(0, PDF_PANEL_CLASS, L"PdfPanel", WS_CHILD | WS_VISIBLE,
                               rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                               hParent, reinterpret_cast<HMENU>(id), g_hInst, doc);
    if (!hWnd) {
        delete doc;
        return nullptr;
    }
    doc->hPanel = hWnd;

    // Store notification callback and userData in window GWLP_USERDATA for simplicity
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(doc));
    // In a real implementation we would store notifyProc/userData elsewhere (e.g., a map).

    return hWnd;
}

NP_API BOOL PdfDestroyPanel(HWND hPanel) {
    if (!hPanel) return FALSE;
    PdfDocument* doc = GetDocFromWindow(hPanel);
    if (doc) {
        delete doc;
    }
    DestroyWindow(hPanel);
    return TRUE;
}

// Stub implementations
NP_API BOOL PdfLoadFile(HWND hPanel, const wchar_t* path) {
    PdfDocument* doc = GetDocFromWindow(hPanel);
    if (!doc) return FALSE;

    // Placeholder: treat file as single-page document.
    doc->pageCount = 1;
    doc->currentPage = 0;
    doc->doc = nullptr; // actual PDF engine would be here
    return TRUE;
}

NP_API BOOL PdfCloseFile(HWND hPanel) {
    PdfDocument* doc = GetDocFromWindow(hPanel);
    if (!doc) return FALSE;
    doc->pageCount = 0;
    doc->currentPage = -1;
    return TRUE;
}

NP_API int PdfGetPageCount(HWND hPanel) {
    PdfDocument* doc = GetDocFromWindow(hPanel);
    return doc ? doc->pageCount : 0;
}

NP_API BOOL PdfSetPage(HWND hPanel, int index) {
    PdfDocument* doc = GetDocFromWindow(hPanel);
    if (!doc) return FALSE;
    if (index < 0 || index >= doc->pageCount) return FALSE;
    doc->currentPage = index;
    UpdateWindow(hPanel, SW_PAINT);
    return TRUE;
}

NP_API BOOL PdfRenderPage(HWND hPanel, HDC hdc, const RECT* rc) {
    PdfDocument* doc = GetDocFromWindow(hPanel);
    if (!doc || !hdc || !rc) return FALSE;

    // Clear background
    FillRect(hdc, rc, (HBRUSH)(COLOR_WINDOW + 1));

    // Draw page indicator
    wchar_t buf[64];
    swprintf_s(buf, L"Page %d", doc->currentPage + 1);
    DrawTextW(hdc, buf, -1, rc, DT_CENTER | DT_VCENTER);

    return TRUE;
}

NP_API BOOL PdfSearchText(HWND hPanel, const wchar_t* text, BOOL forward) {
    // Stub: always not found
    MessageBoxW(hPanel, L"Search not implemented", L"Info", MB_OK);
    return FALSE;
}

// Notification registration (stub)
NP_API BOOL PdfSetNotifyProc(HWND hPanel, PdfNotifyProc proc, void* userData) {
    // No implementation; ignore.
    return TRUE;
}

NP_API void PdfUpdateDpi(HWND hPanel, int dpi) {
    // Resize layout if needed; no-op for stub.
}

NP_API BOOL PdfSetReadOnly(HWND hPanel, BOOL readOnly) {
    PdfDocument* doc = GetDocFromWindow(hPanel);
    if (doc) doc->readOnly = readOnly;
    return TRUE;
}

NP_API BOOL PdfGetReadOnly(HWND hPanel) {
    PdfDocument* doc = GetDocFromWindow(hPanel);
    return doc ? doc->readOnly : FALSE;
}

} // extern "C"