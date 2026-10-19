# Embedding Integration Specification

## Neage Notepad as a Child Window Inside a Host AI Application

**Version:** 1.0  
**Date:** 2026-10-11  
**Status:** Draft  

---

## 1. Goal

Refactor the standalone Neage Notepad (`Notepad clone.cpp`, ~2900 lines, pure Win32 C++) so it can be loaded as a DLL and embedded as a child window panel inside a host application (e.g., an AI-powered IDE or workspace). The host creates one or more editor panels, pushes text into them, reads content back, and receives event notifications -- all without the Notepad owning its own process, message loop, or settings store.

## 2. Current State

| Property | Today | Target |
|---|---|---|
| Build output | Standalone `.exe` | DLL + thin exe wrapper |
| Entry point | `WinMain` (owns message loop) | `DllMain` + exported `CreateNotepadPanel` |
| Window style | `WS_OVERLAPPEDWINDOW` (top-level) | `WS_CHILD` when embedded |
| State | ~40 file-scope globals | Per-instance context struct |
| Data I/O | File dialogs, clipboard only | Programmatic API + file dialogs |
| Notifications | None | `WM_NOTIFY` to parent window |
| Theme | Hardcoded `RGB` constants | Configurable via `SetTheme` call |
| Settings | `HKCU\Software\ModernNotepad` (shared) | Per-instance or host-managed |
| Keyboard | Owns all accelerators | Host gets first crack, forwards unhandled |

## 3. Non-Goals

- Cross-platform support (remains Win32-only)
- Replacing RichEdit50W with another editor control
- Changing the custom-drawn tab/toolbar/menu rendering
- Adding network, cloud-sync, or AI inference features to the editor itself

## 4. Deliverables

1. **`NotepadPanel.dll`** -- the embeddable library
2. **`NotepadPanel.h`** -- public C API header
3. **`NotepadPanel.lib`** -- import library
4. **`NEWAGENOTEPAD.exe`** -- thin wrapper that calls `CreateNotepadPanel` inside its own window (preserves current standalone behavior)
5. Updated `Neage Notepad.vcxproj` with DLL + EXE targets

---

## 5. Architecture

### 5.1 Instance Context

All per-editor state moves into a heap-allocated context struct. The HWND's `GWLP_USERDATA` stores a pointer to it. Every function that currently reads a global resolves the instance from the HWND it receives.

```cpp
// NotepadPanelInternal.h (not exported)
struct NotepadInstance {
    // Window handles
    HWND        hSelf;          // the panel's own HWND
    HWND        hParent;        // host window
    HINSTANCE   hInst;

    // Instance ID (for settings isolation, callback routing)
    int         instanceId;

    // Documents
    std::vector<Document*>  docs;
    int                     activeDoc;

    // Editor state (was g_hEdit, g_currentFile, g_modified, g_zoom, g_wordWrap)
    HWND         hEdit;
    std::wstring currentFile;
    bool         modified;
    int          zoom;
    bool         wordWrap;

    // Search bar state (was g_searchOpen, g_replaceOpen, g_searchCase, ...)
    bool         searchOpen;
    bool         replaceOpen;
    bool         searchCase;
    HWND         hSearchFind;
    HWND         hSearchRepl;
    // ... all search-bar HWNDs

    // Toolbar state (was g_tbDock, g_tbSize, g_hToolbarHost, g_hTbBtns, ...)
    ToolbarDock  tbDock;
    ToolbarSize  tbSize;
    bool         tbFloatHoriz;
    HWND         hToolbarHost;
    HWND         hTbBtns[10];
    HWND         hotBtn;

    // Tab bar state (was g_tabRects, g_tabHot, g_tabCloseHot, ...)
    std::vector<TabRect> tabRects;
    RECT                 tabNewBtnRect;
    int                  tabHot;
    int                  tabCloseHot;
    bool                 trackingMouse;

    // Inline rename state (was g_hRenameEdit, g_renamingTab)
    HWND         hRenameEdit;
    int          renamingTab;

    // Layout rects (was g_tabBarRect, g_searchRect, g_statusRect, g_editRect)
    RECT         tabBarRect;
    RECT         searchRect;
    RECT         statusRect;
    RECT         editRect;

    // Display state (was g_showStatus, g_dpi)
    bool         showStatus;
    int          dpi;

    // Fonts (was g_font, g_fontBold, g_fontSmall)
    HFONT        font;
    HFONT        fontBold;
    HFONT        fontSmall;
    LOGFONTW     logFont;

    // Theme (copy, not global)
    Theme::Palette theme;

    // Callback
    NPNOTIFYPROC  notifyProc;
    void*         notifyUserData;
};
```

### 5.2 Global State That Remains Global

These are process-wide and shared across all instances:

| Item | Why it stays global |
|---|---|
| `HINSTANCE g_hInst` | DLL instance handle, set once in `DllMain` |
| Window class registrations | Registered once in `DllMain`, shared by all panels |
| `LoadLibraryW(L"msftedit.dll")` | Loaded once per process |
| `INITCOMMONCONTROLSEX` | Called once per process |

### 5.3 DLL Lifecycle

```
Host process loads NotepadPanel.dll
  └─ DllMain(DLL_PROCESS_ATTACH)
       ├─ LoadLibrary("msftedit.dll")
       ├─ InitCommonControlsEx
       ├─ RegisterClassW(WINDOW_CLASS)   // "NotepadPanel"
       ├─ RegisterClassW(TOOLBAR_CLASS)   // "NotepadPanelToolbar"
       └─ RegisterClassW(MENU_CLASS)     // "NotepadPanelMenu"

Host calls CreateNotepadPanel(hwndParent, ...)
  └─ CreateWindowExW(WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, ...)
       └─ WM_CREATE handler:
            ├─ alloc NotepadInstance
            ├─ store in GWLP_USERDATA
            ├─ create fonts
            ├─ load settings (per-instance or host-injected)
            ├─ create first Document
            └─ create toolbar host

... host and user interact ...

Host calls DestroyNotepadPanel(hwndPanel)
  └─ WM_DESTROY handler:
        ├─ close all docs, free Documents
        ├─ delete fonts
        └─ delete NotepadInstance

Host process unloads NotepadPanel.dll
  └─ DllMain(DLL_PROCESS_DETACH)
       └─ (classes auto-unregistered by OS)
```

### 5.4 Thin EXE Wrapper

The standalone exe becomes a ~50-line file that reproduces current behavior:

```cpp
// NEWAGENOTEPADMain.cpp
#include "NotepadPanel.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    EnableDpiAwareness();

    // Create a top-level host window
    HWND hFrame = CreateHostFrame(hInst, 980, 680);

    // Embed the panel as a child
    RECT rc; GetClientRect(hFrame, &rc);
    HWND hPanel = CreateNotepadPanel(hFrame, rc, 0);

    // Standard message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
```

The host frame window forwards `WM_SIZE` to the panel, handles `WM_CLOSE`, and optionally installs a menu bar. This wrapper is what ships as the standalone product.

---

## 6. Public C API

### 6.1 Header: `NotepadPanel.h`

```cpp
#ifndef NOTEPAD_PANEL_H
#define NOTEPAD_PANEL_H

#include <windows.h>

#ifdef NOTEPAD_PANEL_EXPORTS
  #define NP_API __declspec(dllexport)
#else
  #define NP_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
//  Creation / destruction
// ============================================================

// Create an embedded editor panel as a child of hParent.
// Returns the panel HWND, or NULL on failure.
//   hParent   -- host window that will contain the panel
//   rc        -- initial position/size in parent client coords
//               (pass {0,0,0,0} to fill parent's client area)
//   id        -- child window ID (for WM_COMMAND routing)
//   notifyProc-- optional callback for editor events (see §8)
//   userData  -- opaque pointer passed back to notifyProc
NP_API HWND CreateNotepadPanel(
    HWND        hParent,
    const RECT* rc,
    int         id,
    NPNOTIFYPROC notifyProc,    // optional, may be NULL
    void*       userData        // optional, may be NULL
);

// Destroy a panel and free all its resources.
// Safe to call on an already-destroyed HWND (returns FALSE).
NP_API BOOL DestroyNotepadPanel(HWND hPanel);

// Resize / reposition a panel within its parent.
// Pass {0,0,0,0} to fill the parent's client area.
NP_API BOOL ResizeNotepadPanel(HWND hPanel, const RECT* rc);

// ============================================================
//  Content I/O
// ============================================================

// Get the full text of the active tab.
// Returns the string length (excluding null terminator).
// If buf is NULL, returns the required length only.
NP_API int GetTextW(HWND hPanel, wchar_t* buf, int bufLen);

// Replace the entire content of the active tab.
NP_API BOOL SetTextW(HWND hPanel, const wchar_t* text);

// Get the text length (character count) of the active tab.
NP_API int GetTextLengthW(HWND hPanel);

// Get the selected text of the active tab.
NP_API int GetSelectedTextW(HWND hPanel, wchar_t* buf, int bufLen);

// Replace the current selection (or insert at cursor if none).
NP_API BOOL SetSelectedTextW(HWND hPanel, const wchar_t* text);

// ============================================================
//  Tab management
// ============================================================

// Number of open tabs.
NP_API int  GetTabCount(HWND hPanel);

// Index of the active tab (0-based), or -1 if none.
NP_API int  GetActiveTab(HWND hPanel);

// Switch to tab at index. Returns TRUE on success.
NP_API BOOL SetActiveTab(HWND hPanel, int index);

// Open a file into a new tab. If path is NULL, creates an empty tab.
// Returns the new tab index, or -1 on failure.
NP_API int  OpenDocument(HWND hPanel, const wchar_t* path);

// Close a tab. If the tab has unsaved changes and promptSave is TRUE,
// shows a confirmation dialog parented to the host.
// Returns TRUE if the tab was closed.
NP_API BOOL CloseTab(HWND hPanel, int index, BOOL promptSave);

// Get the file path of the tab at index.
// Returns the string length, or 0 if no path / index invalid.
NP_API int  GetTabFilePathW(HWND hPanel, int index, wchar_t* buf, int bufLen);

// Get the display title of the tab at index.
NP_API int  GetTabTitleW(HWND hPanel, int index, wchar_t* buf, int bufLen);

// Check whether the tab at index has unsaved modifications.
NP_API BOOL IsTabModified(HWND hPanel, int index);

// Save the tab at index. If saveAs is TRUE, shows a Save dialog.
NP_API BOOL SaveTab(HWND hPanel, int index, BOOL saveAs);

// ============================================================
//  Theme
// ============================================================

typedef struct NPTheme {
    COLORREF bgApp;
    COLORREF bgPanel;
    COLORREF bgCard;
    COLORREF bgBtn;
    COLORREF bgBtnHi;
    COLORREF border;
    COLORREF borderSoft;
    COLORREF textHi;
    COLORREF textMid;
    COLORREF textLow;
    COLORREF accent;
    COLORREF selBg;
    COLORREF iconIdle;
    COLORREF iconActive;
} NPTheme;

// Apply a custom theme. Pass NULL to revert to the default dark theme.
NP_API BOOL SetTheme(HWND hPanel, const NPTheme* theme);

// Get the current theme.
NP_API BOOL GetTheme(HWND hPanel, NPTheme* outTheme);

// ============================================================
//  Font
// ============================================================

// Set the editor font. Pass NULL to use the default (Segoe UI 11pt).
NP_API BOOL SetFontW(HWND hPanel, const LOGFONTW* logFont);

// Get the current editor font.
NP_API BOOL GetFontW(HWND hPanel, LOGFONTW* outLogFont);

// ============================================================
//  View settings
// ============================================================

NP_API BOOL SetWordWrap(HWND hPanel, BOOL wrap);
NP_API BOOL GetWordWrap(HWND hPanel);

NP_API BOOL SetZoom(HWND hPanel, int percent);  // 30..500
NP_API int  GetZoom(HWND hPanel);

NP_API BOOL SetStatusBarVisible(HWND hPanel, BOOL visible);
NP_API BOOL GetStatusBarVisible(HWND hPanel);

NP_API BOOL SetToolbarVisible(HWND hPanel, BOOL visible);
NP_API BOOL GetToolbarVisible(HWND hPanel);

// ============================================================
//  Read-only / lock mode
// ============================================================

NP_API BOOL SetReadOnly(HWND hPanel, BOOL readOnly);
NP_API BOOL GetReadOnly(HWND hPanel);

// ============================================================
//  Notifications
// ============================================================

// Notification codes sent via the callback (or WM_NOTIFY to parent)
#define NP_NOTIFY_CONTENT_CHANGED  1   // user typed/deleted text
#define NP_NOTIFY_TAB_CHANGED      2   // active tab switched
#define NP_NOTIFY_TAB_OPENED       3   // a new tab was created
#define NP_NOTIFY_TAB_CLOSED       4   // a tab was closed
#define NP_NOTIFY_FILE_SAVED       5   // a tab was saved to disk
#define NP_NOTIFY_MODIFIED_CHANGED 6   // dirty flag flipped
#define NP_NOTIFY_ZOOM_CHANGED     7   // zoom level changed
#define NP_NOTIFY_FOCUS_CHANGED    8   // panel gained/lost focus

typedef struct NPNotify {
    HWND        hPanel;      // which panel sent this
    int         instanceId;  // panel instance ID
    UINT        code;        // one of NP_NOTIFY_*
    int         tabIndex;     // affected tab, or -1
    void*       userData;     // pointer passed to CreateNotepadPanel
} NPNotify;

typedef void (CALLBACK* NPNOTIFYPROC)(const NPNotify* notify);

// Change the notification callback at runtime.
NP_API BOOL SetNotifyProc(HWND hPanel, NPNOTIFYPROC proc, void* userData);

#ifdef __cplusplus
}
#endif

#endif // NOTEPAD_PANEL_H
```

### 6.2 Calling Convention

- All exported functions use `__cdecl` (C linkage, `extern "C"`).
- All string parameters are `wchar_t` (UTF-16), consistent with the existing codebase.
- Length-followed-by-buffer pattern for getters: call with `buf=NULL` to get required length, then allocate and call again.
- All functions validate `hPanel` and return `FALSE`/`-1`/`0` on invalid HWND.

### 6.3 Thread Safety

The DLL is single-threaded (STA). All calls must come from the thread that owns the panel's window (the UI thread). The host must not call API functions from background threads. If the host needs to push data from a worker thread, it should `PostMessage` a custom `WM_USER` message to the panel and handle the data in the panel's `WndProc`.

---

## 7. Child Window Creation

### 7.1 Window Style

| Mode | Style |
|---|---|
| Embedded (DLL) | `WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_TABSTOP` |
| Standalone (exe wrapper) | `WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN` (host frame) with panel as `WS_CHILD` inside |

The window proc checks `GetWindowLongPtr(hWnd, GWL_STYLE) & WS_CHILD` at runtime to decide:
- **Title bar updates**: skipped in child mode (the host owns the title bar).
- **`WM_CLOSE`**: in child mode, sends a `NP_NOTIFY_TAB_CLOSED` notification and lets the host decide; does not call `DestroyWindow` directly.
- **`WM_GETMINMAXINFO`**: skipped in child mode (the host controls sizing).
- **`WM_MENUCHAR` / `Alt` key handling**: forwarded to parent in child mode.

### 7.2 Window Class

```
Class name:  "NotepadPanel" (was "ModernNotepad")
Style:       CS_DBLCLKS
Background:  NULL (custom-painted, double-buffered)
Cursor:      IDC_ARROW
```

The class is registered once in `DllMain` and shared by all panel instances.

### 7.3 Resize Behavior

The host calls `ResizeNotepadPanel(hPanel, &rc)` on its `WM_SIZE`. The panel:
1. `MoveWindow(hPanel, rc.left, rc.top, width, height, TRUE)`
2. `UpdateLayout` recalculates `tabBarRect`, `searchRect`, `statusRect`, `editRect` based on the new client size.
3. `InvalidateRect` triggers a repaint.

If the host passes `{0,0,0,0}`, the panel fills the parent's entire client area via `GetClientRect(hParent, &rc)`.

---

## 8. Notification System

### 8.1 Callback Path

The host registers a `NPNOTIFYPROC` at creation time (or via `SetNotifyProc` later). When an event occurs, the panel calls:

```cpp
NPNotify n = {};
n.hPanel    = instance->hSelf;
n.instanceId = instance->instanceId;
n.code      = NP_NOTIFY_CONTENT_CHANGED;
n.tabIndex  = instance->activeDoc;
n.userData  = instance->notifyUserData;
instance->notifyProc(&n);
```

### 8.2 Event Sources

| Notification | Trigger | Current code location |
|---|---|---|
| `NP_NOTIFY_CONTENT_CHANGED` | RichEdit `EN_CHANGE` forwarded from `EditProc` | `EditProc` subclass |
| `NP_NOTIFY_TAB_CHANGED` | `SetActiveDoc` completes | `SetActiveDoc()` |
| `NP_NOTIFY_TAB_OPENED` | `CreateDoc` + `SetActiveDoc` for new tab | `DoNewTab()`, `DoOpen()` |
| `NP_NOTIFY_TAB_CLOSED` | `CloseDoc` finishes, tab destroyed | `CloseDoc()` |
| `NP_NOTIFY_FILE_SAVED` | `DoSave` writes file successfully | `DoSave()` |
| `NP_NOTIFY_MODIFIED_CHANGED` | `doc->modified` transitions true/false | `SyncToActive()` / `SetModifiedFlag()` |
| `NP_NOTIFY_ZOOM_CHANGED` | `ApplyZoom` called | `DispatchCommand` IDM_ZOOM_* |
| `NP_NOTIFY_FOCUS_CHANGED` | `WM_SETFOCUS` / `WM_KILLFOCUS` on panel | `WndProc` |

### 8.3 Fallback: WM_NOTIFY

If no callback is registered, the panel sends `WM_NOTIFY` to the parent window:

```cpp
NMHDR nmh = {};
nmh.hwndFrom = instance->hSelf;
nmh.idFrom   = instance->instanceId;
nmh.code    = NP_NOTIFY_CONTENT_CHANGED;
SendMessageW(instance->hParent, WM_NOTIFY, (WPARAM)nmh.idFrom, (LPARAM)&nmh);
```

The host can handle this in its own `WindowProc` if it prefers the Win32 message pattern over a callback.

---

## 9. Data Exchange Details

### 9.1 GetTextW / SetTextW

`GetTextW` reads from the active tab's RichEdit:
```cpp
GETTEXTLENGTHEX gtl = { GTL_DEFAULT, 1200 };  // Unicode
int len = SendMessageW(hEdit, EM_GETTEXTLENGTHEX, 0, (LPARAM)&gtl);
// ... allocate, then:
GETTEXTEX gt = { sizeof(GETTEXTEX), GTL_DEFAULT, 1200, nullptr, nullptr, nullptr };
SendMessageW(hEdit, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)buf);
```

`SetTextW` uses `EM_SETTEXTEX` (already implemented in `CreateDoc`):
```cpp
SETTEXTEX st = { ST_DEFAULT, 1200 };
// Normalize \n -> \r\n (existing logic in CreateDoc)
SendMessageW(hEdit, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)normalized);
SendMessageW(hEdit, EM_SETMODIFY, FALSE, 0);
```

### 9.2 OpenDocument

When the host calls `OpenDocument(hPanel, path)`:

1. Read the file from disk (existing `ReadAllText` logic: detect BOM, convert to UTF-16).
2. `CreateDoc(text, path)` -- creates a new `Document` with its own RichEdit child.
3. `SetActiveDoc(newIndex)` -- switches the tab.
4. Fire `NP_NOTIFY_TAB_OPENED`.

When `path` is NULL, creates an empty untitled tab (same as `DoNewTab`).

### 9.3 Read-Only Mode

`SetReadOnly(hPanel, TRUE)` sends `EM_SETREADONLY` to the active RichEdit and all existing tabs. New tabs created while in read-only mode inherit the flag. The host uses this to display AI-generated content that the user should view but not edit, or to lock a panel during a background operation.

The toolbar/menu undo/redo/cut/paste/delete buttons are disabled in read-only mode. Find and zoom remain enabled.

---

## 10. Settings Isolation

### 10.1 Problem

Today all panels share `HKCU\Software\ModernNotepad`. If the host creates two panels, they clobber each other's zoom, font, wrap, and toolbar settings.

### 10.2 Solution

Two modes:

**Host-managed (default for embedded):**
The host calls `SetFontW`, `SetWordWrap`, `SetZoom`, `SetStatusBarVisible`, `SetToolbarVisible` after creating the panel. The panel does not read or write the registry. The host is responsible for persisting settings through its own mechanism.

**Self-managed (standalone exe):**
The exe wrapper calls `LoadAppSettings` / `SaveAppSettings` which use the existing registry code. This path is only used when running standalone.

The panel detects its mode at creation time:
- If `notifyProc != NULL` or the host calls any `Set*` API within the first second, it enters host-managed mode.
- Otherwise it loads from registry (standalone behavior).

### 10.3 Registry Key Change

For backward compatibility, the standalone exe keeps `HKCU\Software\ModernNotepad`. The DLL does not touch the registry unless explicitly told to via a `LoadSettings`/`SaveSettings` export (added in v2 if needed).

---

## 11. Theme Coordination

### 11.1 Default Theme

The built-in dark theme (current `Theme::Palette`) is the default. It is copied into each `NotepadInstance` at creation time.

### 11.2 Custom Theme

`SetTheme(hPanel, &theme)` copies the provided palette into the instance and triggers a full repaint. Every paint function (`PaintTabs`, `PaintSearchBar`, `PaintStatusBar`, `DrawButton`, icon drawing) reads from `instance->theme` instead of the global `Theme::` namespace.

### 11.3 Light Theme Support

The color set is already well-structured (14 named colors). A light theme is just a different `NPTheme` struct:

```cpp
NPTheme light = {
    .bgApp      = RGB(255, 255, 255),
    .bgPanel    = RGB(248, 248, 248),
    .bgCard     = RGB(238, 238, 238),
    .bgBtn      = RGB(228, 228, 230),
    .bgBtnHi    = RGB(218, 218, 222),
    .border     = RGB(200, 200, 204),
    .borderSoft = RGB(220, 220, 224),
    .textHi     = RGB(28, 28, 32),
    .textMid    = RGB(100, 100, 108),
    .textLow    = RGB(150, 150, 158),
    .accent     = RGB(37, 99, 235),
    .selBg      = RGB(206, 224, 255),
    .iconIdle   = RGB(60, 60, 68),
    .iconActive = RGB(20, 20, 28),
};
SetTheme(hPanel, &light);
```

### 11.4 RichEdit Theme

`SetWindowTheme(hEdit, L"DarkMode_Explorer", nullptr)` is called for dark mode. For light mode, call `SetWindowTheme(hEdit, L"Explorer", nullptr)` and set `EM_SETBKGNDCOLOR` to the theme's `bgApp`.

### 11.5 DWM Dark Title Bar

`DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))` is only called by the standalone exe wrapper, not by the DLL. In embedded mode the host owns the title bar and is responsible for its own dark/light appearance.

---

## 12. Keyboard Accelerator Coordination

### 12.1 Problem

The panel's `WndProc` handles Ctrl+S, Ctrl+F, Ctrl+H, Ctrl+W, Ctrl+T, Ctrl+Tab, Ctrl+Z, Ctrl+Y, Ctrl+Plus/Minus, F2, F3, F5. The host AI app likely wants some of these (e.g., Ctrl+S to save the AI project, Ctrl+F to search the AI's own content).

### 12.2 Solution: Host-First Accelerator Table

The panel exports its accelerator table so the host can merge it:

```cpp
// NotepadPanel.h
typedef struct NPAccel {
    int     cmd;       // command ID (IDM_SAVE, IDM_FIND, etc.)
    WORD    virtKey;   // FVIRTKEY | FCONTROL | FSHIFT | FALT
    WORD    key;       // virtual key code
} NPAccel;

// Get the panel's accelerator list. The host merges these
// into its own accelerator table. If the host consumes a
// shortcut, it should call SetSuppressedShortcuts to tell
// the panel to stop handling it.
NP_API int  GetAccelerators(NPAccel* outAccels, int maxCount);
NP_API void SetSuppressedShortcuts(HWND hPanel, const int* cmdIds, int count);
```

### 12.3 Implementation

The panel's `WndProc` checks the suppressed list before handling a shortcut. If the command ID is suppressed, it returns `DefWindowProc` (letting the message bubble to the parent).

For the standalone exe, no suppression is configured and all shortcuts work as today.

### 12.4 Default Suppression Recommendations

When embedded in an AI app, the host should suppress:
- `IDM_SAVE` (Ctrl+S) -- the host saves the project, not individual files
- `IDM_OPEN` (Ctrl+O) -- the host manages file open
- `IDM_NEW` / `IDM_EXIT` -- the host controls document lifecycle

The panel retains:
- `IDM_UNDO` / `IDM_REDO` (Ctrl+Z / Ctrl+Y)
- `IDM_FIND` / `IDM_REPLACE` / `IDM_FINDNEXT`
- `IDM_ZOOM_*`
- `IDM_WRAP`, `IDM_FONT`, `IDM_DATETIME`
- Tab switching (Ctrl+Tab), tab close (Ctrl+W), rename (F2)

---

## 13. File Dialog Parenting

### 13.1 Current Code

```cpp
ofn.hwndOwner = g_hMainWnd;
```

### 13.2 Embedded Mode

```cpp
ofn.hwndOwner = instance->hParent;   // host window
```

All `OPENFILENAMEW`, `CHOOSEFONTW`, and `MessageBoxW` calls use the host window as owner so dialogs appear modal to the correct window and do not get hidden behind the host's own windows.

---

## 14. Floating Toolbar

### 14.1 Current Behavior

When `g_tbDock == TBDOCK_FLOAT`, the toolbar is reparented to a `WS_POPUP | WS_CAPTION | WS_SYSMENU` window. This works in standalone mode.

### 14.2 Embedded Behavior

In embedded mode, a floating toolbar popup may appear outside the host window's bounds or get clipped. Two options:

**Option A (recommended):** Disable floating dock in embedded mode. The toolbar can dock top/bottom/left/right within the panel but cannot float. `SetToolbarDock(TBDOCK_FLOAT)` is a no-op in child mode.

**Option B:** The floating toolbar popup uses `WS_POPUP` with the host as owner (`WS_EX_TOOLWINDOW | WS_EX_TOPMOST`). The popup is positioned relative to the panel's screen coordinates. This works but may clip at host window edges.

The spec recommends Option A for v1.

---

## 15. Build System Changes

### 15.1 Project Structure

```
Neage Notepad/
├── NotepadPanel.h          -- public API header (new)
├── NotepadPanel.cpp        -- DLL exports + DllMain (new)
├── Notepad clone.cpp       -- core implementation (refactored)
├── NEWAGENOTEPADMain.cpp    -- thin exe wrapper (new)
├── NotepadPanel.vcxproj    -- DLL target (new)
├── Neage Notepad.vcxproj    -- EXE target (modified)
├── Neage Notepad.slnx      -- solution (updated)
└── ...
```

### 15.2 DLL Target Configuration

| Property | Value |
|---|---|
| Configuration Type | Dynamic Library (.dll) |
| Character Set | Unicode |
| C++ Standard | C++20 |
| Preprocessor | `NOTEPAD_PANEL_EXPORTS;_UNICODE;UNICODE` |
| Linker | `/DEF:NotepadPanel.def` or `__declspec(dllexport)` |
| Dependencies | `comctl32.lib;shell32.lib;dwmapi.lib;uxtheme.lib` |

### 15.3 EXE Target Configuration

| Property | Value |
|---|---|
| Configuration Type | Application (.exe) |
| C++ Standard | C++20 |
| Preprocessor | `_UNICODE;UNICODE` |
| Link Dependencies | `NotepadPanel.lib` (import library) |
| Source Files | `NEWAGENOTEPADMain.cpp` only |

### 15.4 Backward Compatibility

The standalone exe must produce identical behavior to the current build. The exe wrapper:
1. Calls `EnableDpiAwareness()`
2. Creates a top-level frame window with `WS_OVERLAPPEDWINDOW`
3. Creates the panel as a child filling the frame
4. Forwards `WM_SIZE` to the panel
5. Runs the message loop
6. On `WM_CLOSE` of the frame, calls `DestroyNotepadPanel`

No user-visible behavior changes in standalone mode.

---

## 16. Migration Plan

### Phase 1: Instance-ification (internal, no API change)

1. Create `NotepadInstance` struct
2. Move all globals into the struct
3. Store/retrieve via `GWLP_USERDATA` on every HWND
4. Refactor every function to take `NotepadInstance*` (or resolve from HWND)
5. Verify standalone exe still builds and works identically

**Risk:** Large mechanical refactor. ~40 globals, ~200 functions. Each function that reads `g_hEdit` or `g_docs` needs a pointer dereference instead. High volume, low complexity per change.

**Test:** Build and run the standalone exe. Verify tabs, search, toolbar docking, rename, settings persistence all work.

### Phase 2: DLL extraction

1. Split `WinMain` into `DllMain` (class registration, common controls init) + `CreateNotepadPanel` (window creation) + `DllMain` cleanup
2. Add `NotepadPanel.h` with the public API
3. Implement `CreateNotepadPanel`, `DestroyNotepadPanel`, `ResizeNotepadPanel`
4. Create `NEWAGENOTEPADMain.cpp` as the thin exe wrapper
5. Add DLL build target to `.vcxproj`

**Test:** Build DLL. Build exe that links the DLL. Verify standalone exe behavior is unchanged.

### Phase 3: Data API

1. Implement `GetTextW` / `SetTextW` / `GetTextLengthW`
2. Implement `GetSelectedTextW` / `SetSelectedTextW`
3. Implement `OpenDocument` / `CloseTab` / `SaveTab`
4. Implement `GetTabCount` / `GetActiveTab` / `SetActiveTab`
5. Implement `GetTabFilePathW` / `GetTabTitleW` / `IsTabModified`

**Test:** Write a test harness exe that creates a panel, pushes text, reads it back, opens a file, verifies tab count and content.

### Phase 4: Notifications

1. Define `NPNotify` struct and `NPNOTIFYPROC` typedef
2. Add `notifyProc` / `userData` to `NotepadInstance`
3. Fire notifications at each event source (see §8.2 table)
4. Implement `SetNotifyProc`
5. Implement `WM_NOTIFY` fallback when no callback is set

**Test:** Test harness registers a callback, types text, verifies `NP_NOTIFY_CONTENT_CHANGED` fires. Switch tabs, verify `NP_NOTIFY_TAB_CHANGED`.

### Phase 5: Theme + font + view APIs

1. Implement `SetTheme` / `GetTheme`
2. Implement `SetFontW` / `GetFontW`
3. Implement `SetWordWrap` / `SetZoom` / `SetStatusBarVisible` / `SetToolbarVisible`
4. Implement `SetReadOnly` / `GetReadOnly`
5. Refactor all paint functions to read from `instance->theme` instead of `Theme::`

**Test:** Test harness applies a light theme, verifies visual output. Applies a custom font, verifies editor font changes.

### Phase 6: Keyboard coordination + dialog parenting + settings isolation

1. Implement `GetAccelerators` / `SetSuppressedShortcuts`
2. Update all `ofn.hwndOwner` / `MessageBoxW` calls to use `instance->hParent` in child mode
3. Add child-mode detection (`WS_CHILD` style check)
4. Skip registry reads/writes in host-managed mode
5. Disable floating toolbar in child mode

**Test:** Test harness suppresses Ctrl+S, verifies the panel does not handle it. Verifies file dialogs are parented to the host window.

### Phase 7: Documentation + sample host

1. Write `EMBEDDING_GUIDE.md` (quick start for host app developers)
2. Create a minimal sample host app (`SampleHost.cpp`) that:
   - Creates a top-level window
   - Loads the DLL
   - Creates two panels side by side
   - Pushes text into each
   - Receives notifications
   - Applies a custom theme
3. Update `README.md` with build instructions for both targets

---

## 17. API Reference Summary

| Function | Purpose | Phase |
|---|---|---|
| `CreateNotepadPanel` | Create embedded panel | 2 |
| `DestroyNotepadPanel` | Destroy panel | 2 |
| `ResizeNotepadPanel` | Resize within parent | 2 |
| `GetTextW` | Read active tab content | 3 |
| `SetTextW` | Write active tab content | 3 |
| `GetTextLengthW` | Get character count | 3 |
| `GetSelectedTextW` | Read selection | 3 |
| `SetSelectedTextW` | Replace selection | 3 |
| `OpenDocument` | Open file into new tab | 3 |
| `CloseTab` | Close a tab | 3 |
| `SaveTab` | Save a tab | 3 |
| `GetTabCount` | Number of tabs | 3 |
| `GetActiveTab` | Active tab index | 3 |
| `SetActiveTab` | Switch tab | 3 |
| `GetTabFilePathW` | Tab file path | 3 |
| `GetTabTitleW` | Tab display title | 3 |
| `IsTabModified` | Dirty flag | 3 |
| `SetTheme` | Apply custom palette | 5 |
| `GetTheme` | Read current palette | 5 |
| `SetFontW` | Set editor font | 5 |
| `GetFontW` | Read editor font | 5 |
| `SetWordWrap` | Toggle word wrap | 5 |
| `SetZoom` | Set zoom level | 5 |
| `SetStatusBarVisible` | Show/hide status bar | 5 |
| `SetToolbarVisible` | Show/hide toolbar | 5 |
| `SetReadOnly` | Lock editing | 5 |
| `GetReadOnly` | Check lock | 5 |
| `SetNotifyProc` | Change callback | 4 |
| `GetAccelerators` | Get shortcut list | 6 |
| `SetSuppressedShortcuts` | Suppress host-owned keys | 6 |

---

## 18. Constraints and Assumptions

- **Windows 10 1903+** required for `DwmSetWindowAttribute` dark title bar (standalone only).
- **RichEdit 50W** (`msftedit.dll`) must be available on the host system. The DLL loads it in `DllMain`.
- **DPI awareness** is set by the host process, not the DLL. The DLL reads `g_dpi` from the panel's window DPI at creation time. Per-monitor V2 is recommended for the host.
- **GDI only** -- no Direct2D, no DirectX. This limits animation but keeps the dependency surface minimal.
- **No COM initialization** -- the DLL does not call `CoInitializeEx`. If the host needs COM (e.g., for file dialogs on STA), the host must initialize COM before creating the panel.

---

## 19. Risk Register

| Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|
| Instance refactor introduces regressions | High | Medium | Phase 1 is pure mechanical refactor; test standalone exe thoroughly before proceeding |
| RichEdit child window focus issues in embedded mode | Medium | Medium | Test tab switching and typing when panel does not have focus; may need `SetFocus` adjustments |
| Toolbar floating popup clips outside host window | Low | High | Disable floating in child mode (§14, Option A) |
| Host and panel both handle same accelerator | Medium | High | Host-first accelerator table with suppression (§12) |
| Multiple panels share class registration but have separate state | Low | Low | `GWLP_USERDATA` is per-HWND; class registration is process-wide by design |
| Settings clobbering between instances | Medium | High | Host-managed mode skips registry (§10) |
| GDI resource leaks across panel create/destroy cycles | High | Low | Audit all `CreateFont`, `CreateCompatibleDC`, `CreateCompatibleBitmap` calls; ensure cleanup in `WM_DESTROY` |
