// NEWAGENOTEPADMain.cpp -- Standalone frame that hosts a NotepadPanel
//
// The executable owns the top-level window and the message loop; the panel owns
// everything inside it. Closing the frame destroys the panel, which releases its
// child windows, fonts, and document records.
//
// Build: configure as an Application (.exe) that compiles NotepadPanel.cpp with
//        NOTEPAD_PANEL_STATIC. msftedit.dll is loaded on demand.

#include "pch.h"

#include "NotepadPanel.h"
#include "Theme/Theme.h"

#include <windowsx.h>

// ---- Constants ----

static const wchar_t FRAME_CLASS[] = L"NEWAGENOTEPADFrame";
static const wchar_t WINDOW_TITLE[] = L"Neage Notepad";
static const int kDefaultWidth = 980;
static const int kDefaultHeight = 680;
static const int kMinWidth = 560;
static const int kMinHeight = 360;

// ---- Global state ----

static HINSTANCE g_hInst = nullptr;
static HWND g_hFrame = nullptr;
static HWND g_hPanel = nullptr;

// The standalone app follows Windows; SetThemeMode resolves the registry value.
static const int kThemeMode = NP_THEME_SYSTEM;

// ---- Dpi awareness ----

static void EnableDpiAwareness()
{
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT) - 4)
#endif
    using SetContextFn = BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32)
    {
        SetContextFn setContext =
            reinterpret_cast<SetContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setContext && setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
        {
            return;
        }
    }
    SetProcessDPIAware();
}

static int GetSystemDpi()
{
    HDC screen = GetDC(nullptr);
    if (!screen)
    {
        return 96;
    }
    const int dpi = GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(nullptr, screen);
    return dpi > 0 ? dpi : 96;
}

// ---- Theme ----

static bool IsDarkTheme()
{
    return ResolveDarkMode(static_cast<ThemeMode>(kThemeMode));
}

static void ApplyChromeTheme()
{
    ApplyTitleBarTheme(g_hFrame, IsDarkTheme());
}

// ---- Frame procedure ----

static LRESULT CALLBACK FrameProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        if (g_hPanel)
        {
            RECT client = {};
            GetClientRect(hWnd, &client);
            ResizeNotepadPanel(g_hPanel, &client);
        }
        return 0;

    case WM_SETTINGCHANGE:
        // ImmersiveColorSet is posted when the user flips the Windows app theme.
        // The panel is a child window, so the frame forwards the change to it.
        if (kThemeMode == NP_THEME_SYSTEM && lParam &&
            wcscmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0)
        {
            ApplyChromeTheme();
            if (g_hPanel)
            {
                SendMessageW(g_hPanel, WM_SETTINGCHANGE, wParam, lParam);
            }
        }
        return 0;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *info = reinterpret_cast<MINMAXINFO *>(lParam);
        const int dpi = GetSystemDpi();
        info->ptMinTrackSize.x = ScaleMetric(kMinWidth, dpi);
        info->ptMinTrackSize.y = ScaleMetric(kMinHeight, dpi);
        return 0;
    }

    case WM_CLOSE:
        if (g_hPanel)
        {
            DestroyNotepadPanel(g_hPanel);
            g_hPanel = nullptr;
        }
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---- Entry point ----

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    g_hInst = hInstance;
    EnableDpiAwareness();

    if (!InitializeNotepadPanel(hInstance))
    {
        MessageBoxW(nullptr,
                    L"The Notepad editor component could not initialize. Check that msftedit.dll "
                    L"is available.",
                    WINDOW_TITLE, MB_OK | MB_ICONERROR);
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
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr, L"The Notepad window class could not be registered.", WINDOW_TITLE,
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    const int dpi = GetSystemDpi();
    RECT frame = {0, 0, ScaleMetric(kDefaultWidth, dpi), ScaleMetric(kDefaultHeight, dpi)};
    AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, FALSE);

    g_hFrame = CreateWindowExW(0, FRAME_CLASS, WINDOW_TITLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                               CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left,
                               frame.bottom - frame.top, nullptr, nullptr, hInstance, nullptr);
    if (!g_hFrame)
    {
        MessageBoxW(nullptr, L"The Notepad window could not be created.", WINDOW_TITLE,
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    SendMessageW(g_hFrame, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(windowClass.hIcon));

    RECT client = {};
    GetClientRect(g_hFrame, &client);
    g_hPanel = CreateNotepadPanel(g_hFrame, &client, 0, nullptr, nullptr);
    if (!g_hPanel)
    {
        MessageBoxW(g_hFrame, L"The editor panel could not be created.", WINDOW_TITLE,
                    MB_OK | MB_ICONERROR);
        DestroyWindow(g_hFrame);
        return 1;
    }

    SetThemeMode(g_hPanel, kThemeMode);
    ApplyChromeTheme();

    ShowWindow(g_hFrame, SW_SHOW);
    UpdateWindow(g_hFrame);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
