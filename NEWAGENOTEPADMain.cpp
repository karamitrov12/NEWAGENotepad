// NEWAGENOTEPADMain.cpp -- Thin exe wrapper that hosts a NotepadPanel as a standalone app
//
 // Build: configure as an Application (.exe) that links NotepadPanel.lib.
 // This reproduces the original standalone behavior by creating a top-level
 // frame window and embedding the panel as a child.

#ifndef WIN32_LEAN_AND_MEAN // Use this compatibility fallback only when WIN32_LEAN_AND_MEAN is not defined.
#define WIN32_LEAN_AND_MEAN // Define WIN32_LEAN_AND_MEAN as a compile-time setting.
#endif // Close the current compile-time condition.
#ifndef NOMINMAX // Use this compatibility fallback only when NOMINMAX is not defined.
#define NOMINMAX // Define NOMINMAX as a compile-time setting.
#endif // Close the current compile-time condition.

#include <windows.h> // Import the Win32 API.
#include "NotepadPanel.h" // Import the editor panel API.
#include "PdfPanel/PdfPanel.h" // Import PDF panel API.
#include "resource.h" // Import application resource identifiers.
#include <commdlg.h> // For file dialogs

#pragma comment(lib, "dwmapi.lib") // Link the dwmapi.lib Windows library.

static const wchar_t FRAME_CLASS[] = L"NEWAGENOTEPADFrame"; // Name the frame window class that Win32 registers for this component.
static HWND g_hFrame = nullptr; // Declare the top-level Notepad frame window and start with no handle assigned.
static HWND g_hPanel = nullptr; // Declare the editor panel hosted inside the frame and start with no handle assigned.
static HWND g_hPdfViewer = nullptr; // Declare the PDF viewer panel
static HINSTANCE g_hInst = nullptr; // Declare the application module handle used to register and create windows and start with no handle assigned.

static LRESULT CALLBACK FrameProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) { // Define FrameProc as the function that implements this part of the panel.
    switch (msg) { // Choose the handler based on the current message or command.
    case WM_SIZE: // Handle WM_SIZE to resize child controls with the host.
        if (g_hPanel) {
            RECT rc; GetClientRect(hWnd, &rc);
            ResizeNotepadPanel(g_hPanel, &rc);
        }
        return 0;
    case WM_CLOSE:
        if (g_hPanel) {
            DestroyNotepadPanel(g_hPanel);
            g_hPanel = nullptr;
        }
        if (g_hPdfViewer) {
            PdfDestroyPanel(g_hPdfViewer);
            g_hPdfViewer = nullptr;
        }
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* p = (MINMAXINFO*)lParam;
        p->ptMinTrackSize.x = 560;
        p->ptMinTrackSize.y = 360;
        return 0;
    }
    case WM_COMMAND:
    {
        const int cmd = static_cast<int>(wParam);
        if (cmd == IDM_PDF_OPEN) {
            // Open PDF File Dialog
            OPENFILENAMEW ofn = {0};
            wchar_t szFile[MAX_PATH] = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = g_hFrame;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"PDF Files\0*.pdf\0All Files\0*.*\0";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileNameW(&ofn)) {
                if (!g_hPdfViewer) {
                    RECT rcClient = {};
                    GetClientRect(g_hFrame, &rcClient);
                    g_hPdfViewer = PdfCreatePanel(g_hFrame, &rcClient, 1001, nullptr, nullptr);
                }
                if (g_hPdfViewer) {
                    PdfLoadFile(g_hPdfViewer, szFile);
                    SetForegroundWindow(g_hPdfViewer);
                }
            }
            return 0;
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        const NMHDR* header = reinterpret_cast<const NMHDR*>(lParam);
        if (header && header->code == NP_NOTIFY_THEME_CHANGED) {
            // Apply theme changes to both panels
            ApplyChromeTheme();
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        const NMHDR* header = reinterpret_cast<const NMHDR*>(lParam);
        if (header && header->code == NP_NOTIFY_THEME_CHANGED) {
            ApplyChromeTheme();
        }
        return 0;
    }

    default:
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void EnableDpiAwareness() { // Define EnableDpiAwareness as the function that implements this part of the panel.
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 // Use this compatibility fallback only when DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is not defined.
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4) // Define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 as a compile-time setting with value ((DPI_AWARENESS_CONTEXT)-4).
#endif // Close the current compile-time condition.
    using SetContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        SetContextFn setContext =
            reinterpret_cast<SetContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setContext && setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }
    SetProcessDPIAware();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) { // Define WinMain as the function that implements this part of the panel.
    g_hInst = hInstance;
    EnableDpiAwareness();

    if (!InitializeNotepadPanel(hInstance)) {
        MessageBoxW(nullptr,
            L"The Notepad editor component could not initialize. Check that msftedit.dll "
            L"is available.",
            WINDOW_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    // Initialize PDF component
    if (!PdfInitialize(hInstance)) {
        MessageBoxW(nullptr, L"Failed to initialize PDF engine.", WINDOW_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = FrameProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = FRAME_CLASS;
    windowClass.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.style = CS_DBLCLKS;
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"The Notepad window class could not be registered.", WINDOW_TITLE,
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    const int dpi = GetSystemDpi();
    RECT frame = {0, 0, ScaleMetric(980, dpi), ScaleMetric(680, dpi)};
    AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, FALSE);

    g_hFrame = CreateWindowExW(0, FRAME_CLASS, WINDOW_TITLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                               CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left,
                               frame.bottom - frame.top, nullptr, nullptr, hInstance, nullptr);
    if (!g_hFrame) {
        MessageBoxW(nullptr, L"The Notepad window could not be created.", WINDOW_TITLE,
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    SendMessageW(g_hFrame, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(windowClass.hIcon));

    RECT client = {};
    GetClientRect(g_hFrame, &client);
    g_hPanel = CreateNotepadPanel(g_hFrame, &client, 0, nullptr, nullptr);
    if (!g_hPanel) {
        MessageBoxW(g_hFrame, L"The editor panel could not be created.", WINDOW_TITLE,
                    MB_OK | MB_ICONERROR);
        DestroyWindow(g_hFrame);
        return 1;
    }

    SetThemeMode(g_hPanel, kThemeMode);
    ApplyChromeTheme();

    BuildMainMenu(); // Add PDF menu item

    ShowWindow(g_hFrame, SW_SHOW);
    UpdateWindow(g_hFrame);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

static void BuildMainMenu() { // Add PDF menu item
    HMENU hMenuBar = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();

    AppendMenuW(hFileMenu, MF_STRING, 100, L"New");
    AppendMenuW(hFileMenu, MF_STRING, 101, L"Open");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_PDF_OPEN, L"Open PDF...");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, 102, L"Exit");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"File");
    SetMenu(g_hFrame, hMenuBar);
}

static int GetSystemDpi() {
    HDC screen = GetDC(nullptr);
    if (!screen) return 96;
    const int dpi = GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(nullptr, screen);
    return dpi > 0 ? dpi : 96;
}