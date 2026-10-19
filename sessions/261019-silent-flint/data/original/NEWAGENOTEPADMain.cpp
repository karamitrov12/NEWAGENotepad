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
#include <dwmapi.h> // Import Desktop Window Manager attributes.
#include "NotepadPanel.h" // Import the editor panel API.
#include "resource.h" // Import application resource identifiers.

#pragma comment(lib, "dwmapi.lib") // Link the dwmapi.lib Windows library.

static const wchar_t FRAME_CLASS[] = L"NEWAGENOTEPADFrame"; // Name the frame window class that Win32 registers for this component.
static HWND g_hFrame = nullptr; // Declare the top-level Notepad frame window and start with no handle assigned.
static HWND g_hPanel = nullptr; // Declare the editor panel hosted inside the frame and start with no handle assigned.
static HINSTANCE g_hInst = nullptr; // Declare the application module handle used to register and create windows and start with no handle assigned.

static LRESULT CALLBACK FrameProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) { // Define FrameProc as the function that implements this part of the panel.
    switch (msg) { // Choose the handler based on the current message or command.
    case WM_SIZE: // Handle WM_SIZE to resize child controls with the host.
        if (g_hPanel) { // Continue this branch only when the editor panel hosted inside the frame is true.
            RECT rc; GetClientRect(hWnd, &rc); // Declare the rectangle used to measure a window’s client area for use by this block.
            ResizeNotepadPanel(g_hPanel, &rc); // Call ResizeNotepadPanel to resize the embedded editor to fill its host window.
        } // Close the current block.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.

    case WM_CLOSE: // Handle WM_CLOSE to close the editor and frame.
        if (g_hPanel) DestroyNotepadPanel(g_hPanel); // Continue this branch only when the editor panel hosted inside the frame is true.
        g_hPanel = nullptr; // Set the editor panel hosted inside the frame from the value or result on the right.
        DestroyWindow(hWnd); // Call DestroyWindow to release the requested resource.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.

    case WM_DESTROY: // Handle WM_DESTROY to stop the message loop.
        PostQuitMessage(0); // Call PostQuitMessage to post the quit message that ends the application loop.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.

    case WM_GETMINMAXINFO: { // Handle WM_GETMINMAXINFO to set the minimum frame size.
        MINMAXINFO* p = (MINMAXINFO*)lParam; // Declare p and start with (MINMAXINFO*)lParam.
        p->ptMinTrackSize.x = 560; // Store the new value in the pt min track size.x field of the current instance.
        p->ptMinTrackSize.y = 360; // Store the new value in the pt min track size.y field of the current instance.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.
    } // Close the current block.
    return DefWindowProcW(hWnd, msg, wParam, lParam); // Let Win32 perform its default handling for this unhandled message.
} // Close the current block.

static void EnableDpiAwareness() { // Define EnableDpiAwareness as the function that implements this part of the panel.
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 // Use this compatibility fallback only when DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is not defined.
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4) // Define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 as a compile-time setting with value ((DPI_AWARENESS_CONTEXT)-4).
#endif // Close the current compile-time condition.
    typedef BOOL(WINAPI* SetCtxFn)(DPI_AWARENESS_CONTEXT); // Create a reusable alias for this type or callback signature.
    HMODULE u32 = GetModuleHandleW(L"user32.dll"); // Declare u32 and start with GetModuleHandleW(L"user32.dll").
    if (u32) { // Continue this branch only when u32 is true.
        SetCtxFn fn = (SetCtxFn)GetProcAddress(u32, "SetProcessDpiAwarenessContext"); // Declare fn and start with (SetCtxFn)GetProcAddress(u32, "SetProcessDpiAwarenessContext").
        if (fn && fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return; // Continue this branch only when fn is true and fn(dpi awareness context per monitor aware v2) is true.
    } // Close the current block.
    SetProcessDPIAware(); // Call SetProcessDPIAware to enable system DPI scaling on older Windows versions.
} // Close the current block.

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) { // Define WinMain as the function that implements this part of the panel.
    g_hInst = hInstance; // Set the application module handle used to register and create windows from the value or result on the right.
    EnableDpiAwareness(); // Call EnableDpiAwareness to perform this part of the Notepad operation.

    if (!InitializeNotepadPanel(hInstance)) { // Continue this branch only when initialize notepad panel(h instance) evaluates to false.
        MessageBoxW(nullptr, // Call MessageBoxW to show the initialization error to the user.
            L"The Notepad editor component could not initialize. Check that msftedit.dll is available.", // Supply this parameter or list item to the enclosing call.
            L"Neage Notepad", MB_OK | MB_ICONERROR); // Complete the current operation before proceeding to the next statement.
        return 1; // Return the computed 1 to the caller.
    } // Close the current block.

    WNDCLASSW wc{}; // Complete the current operation before proceeding to the next statement.
    wc.lpfnWndProc = FrameProc; // Update the lpfn wnd proc field with the calculated value.
    wc.hInstance = hInstance; // Update the the executable instance handle supplied by Windows field with the calculated value.
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); // Update the h cursor field with the calculated value.
    wc.hbrBackground = nullptr; // Update the hbr background field with the calculated value.
    wc.lpszClassName = FRAME_CLASS; // Update the lpsz class name field with the calculated value.
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON)); // Load the application icon embedded in the executable.
    wc.style = CS_DBLCLKS; // Update the style field with the calculated value.
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { // Continue this branch only when register class w(&wc) && get last error() != error class already exists evaluates to false.
        MessageBoxW(nullptr, L"The Notepad window class could not be registered.", // Call MessageBoxW to show the initialization error to the user.
            L"Neage Notepad", MB_OK | MB_ICONERROR); // Complete the current operation before proceeding to the next statement.
        return 1; // Return the computed 1 to the caller.
    } // Close the current block.

    HDC screenDc = GetDC(nullptr); // Declare screen dc and start with GetDC(nullptr).
    const int dpi = screenDc ? GetDeviceCaps(screenDc, LOGPIXELSY) : 96; // Declare the current display density used for scaling and start with screenDc ? GetDeviceCaps(screenDc, LOGPIXELSY) : 96.
    if (screenDc) ReleaseDC(nullptr, screenDc); // Continue this branch only when screen dc is true.
    RECT rc = { 0, 0, MulDiv(980, dpi, 96), MulDiv(680, dpi, 96) }; // Declare the rectangle used to measure a window’s client area and start with { 0, 0, MulDiv(980, dpi, 96), MulDiv(680, dpi, 96) }.
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, FALSE); // Call AdjustWindowRect to calculate the outer frame size for the requested client area.

    g_hFrame = CreateWindowExW(0, FRAME_CLASS, L"Neage Notepad", // Set the top-level application frame window.
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, // Supply this parameter or list item to the enclosing call.
        CW_USEDEFAULT, CW_USEDEFAULT, // Supply this parameter or list item to the enclosing call.
        rc.right - rc.left, rc.bottom - rc.top, // Supply this parameter or list item to the enclosing call.
        nullptr, nullptr, hInstance, nullptr); // Complete the current operation before proceeding to the next statement.
    if (g_hFrame) SendMessageW(g_hFrame, WM_SETICON, ICON_SMALL, (LPARAM)wc.hIcon); // Use the app icon in the title bar and task switcher.
    if (!g_hFrame) { // Continue this branch only when the top-level application frame window is unavailable.
        MessageBoxW(nullptr, L"The Notepad window could not be created.", // Call MessageBoxW to show the initialization error to the user.
            L"Neage Notepad", MB_OK | MB_ICONERROR); // Complete the current operation before proceeding to the next statement.
        return 1; // Return the computed 1 to the caller.
    } // Close the current block.

    // Apply dark title bar
    BOOL dark = TRUE; // Declare dark and start enabled.
    DwmSetWindowAttribute(g_hFrame, 20, &dark, sizeof(dark)); // Call DwmSetWindowAttribute to apply the dark title-bar preference through DWM.


    RECT clientRc; // Declare the frame client rectangle that the editor fills for use by this block.
    GetClientRect(g_hFrame, &clientRc); // Call GetClientRect to read the client bounds used to lay out child controls.
    g_hPanel = CreateNotepadPanel(g_hFrame, &clientRc, 0, nullptr, nullptr); // Set the editor panel hosted inside the frame from the value or result on the right.
    if (!g_hPanel) { // Continue this branch only when the editor panel hosted inside the frame is unavailable.
        MessageBoxW(g_hFrame, L"The editor panel could not be created.", // Call MessageBoxW to show the initialization error to the user.
            L"Neage Notepad", MB_OK | MB_ICONERROR); // Complete the current operation before proceeding to the next statement.
        DestroyWindow(g_hFrame); // Call DestroyWindow to release the requested resource.
        return 1; // Return the computed 1 to the caller.
    } // Close the current block.

    ShowWindow(g_hFrame, SW_SHOW); // Call ShowWindow to show the frame using the selected display state.
    UpdateWindow(g_hFrame); // Call UpdateWindow to request an immediate paint for the frame.

    MSG msg; // Declare the Win32 message currently being dispatched for use by this block.
    while (GetMessageW(&msg, nullptr, 0, 0)) { // Repeat the body while get message w(&msg, nullptr, 0, 0) is true.
        TranslateMessage(&msg); // Call TranslateMessage to translate keyboard messages into character messages.
        DispatchMessageW(&msg); // Call DispatchMessageW to deliver the message to its window procedure.
    } // Close the current block.
    return (int)msg.wParam; // Return the exit code carried by the application quit message.
} // Close the current block.
