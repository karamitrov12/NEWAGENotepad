# NotepadPanel API Documentation

**Version:** 1.0
**Library:** `NotepadPanel.dll` / `NotepadPanel.lib`
**Header:** `NotepadPanel.h`

---

## Quick Start

```cpp
#include "NotepadPanel.h"

// 1. Create a panel inside your window
HWND hPanel = CreateNotepadPanel(
    hWnd,               // your host window
    nullptr,            // fill parent's client area
    1234,               // child window ID
    MyNotifyCallback,   // event callback (or NULL)
    nullptr             // user data
);

// 2. Push text into it
SetTextW(hPanel, L"Hello, world!");

// 3. Read text back
int len = GetTextLengthW(hPanel);
wchar_t* buf = new wchar_t[len + 1];
GetTextW(hPanel, buf, len + 1);

// 4. Listen for events
void CALLBACK MyNotifyCallback(const NPNotify* n) {
    if (n->code == NP_NOTIFY_CONTENT_CHANGED) {
        // user typed something -- auto-save, sync, etc.
    }
}

// 5. Destroy when done
DestroyNotepadPanel(hPanel);
```

---

## Architecture

The DLL exports a C API for creating, controlling, and destroying editor panels.
Each panel is a child window (`WS_CHILD`) with its own tab bar, toolbar, search bar,
and RichEdit 50W editor. All per-panel state is isolated in an instance struct
resolved from the panel's HWND -- multiple panels can coexist in the same process
without interference.

The DLL does **not** own a message loop. The host process runs `GetMessage` /
`DispatchMessage` as usual. The panel's `WndProc` handles all its own input, paint,
and layout internally.

### Process-Global State

These items are shared across all panel instances (set once in `DllMain`):
- DLL instance handle
- Window class registrations (`NotepadPanel`, `NotepadPanelToolbar`, `NotepadPanelMenu`)
- `msftedit.dll` load (RichEdit 50W)

### Thread Safety

The DLL is single-threaded (STA). All API calls must come from the thread that
owns the panel window. To push data from a worker thread, `PostMessage` a custom
`WM_USER` message to the panel and handle it in the panel's `WndProc`.

---

## API Reference

### Creation / Destruction

#### `InitializeNotepadPanel`
```cpp
BOOL InitializeNotepadPanel(HINSTANCE hInstance);
```
Initializes the panel classes and Rich Edit dependency. Call this before
`CreateNotepadPanel` when the panel source is linked directly into an executable.
DLL builds initialize automatically when the DLL is loaded.

#### `CreateNotepadPanel`
```cpp
HWND CreateNotepadPanel(
    HWND         hParent,
    const RECT*  rc,           // NULL or {0,0,0,0} = fill parent
    int          id,           // child window ID
    NPNOTIFYPROC notifyProc,   // NULL for no callback
    void*        userData      // passed back to notifyProc
);
```
Creates an embedded editor panel as a child of `hParent`. Returns the panel HWND
or NULL on failure. The panel is created with `WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN`.

If `notifyProc` is non-NULL, the panel enters **host-managed mode**: it skips all
registry reads/writes and expects the host to configure settings via the API.
If `notifyProc` is NULL, the panel loads settings from `HKCU\Software\ModernNotepad`
(standalone behavior).

#### `DestroyNotepadPanel`
```cpp
BOOL DestroyNotepadPanel(HWND hPanel);
```
Destroys the panel and frees all resources (documents, fonts, brushes). Safe to
call on an already-destroyed HWND (returns FALSE).

#### `ResizeNotepadPanel`
```cpp
BOOL ResizeNotepadPanel(HWND hPanel, const RECT* rc);
```
Moves/resizes the panel within its parent. Pass NULL or `{0,0,0,0}` to fill the
parent's entire client area. The host should call this on its `WM_SIZE`.

---

### Content I/O

#### `GetTextW`
```cpp
int GetTextW(HWND hPanel, wchar_t* buf, int bufLen);
```
Returns the full text of the active tab. If `buf` is NULL, returns the required
length (excluding null terminator). Otherwise copies up to `bufLen-1` characters.

```cpp
int len = GetTextLengthW(hPanel);
std::wstring text(len, 0);
GetTextW(hPanel, text.data(), len + 1);
```

#### `SetTextW`
```cpp
BOOL SetTextW(HWND hPanel, const wchar_t* text);
```
Replaces the entire content of the active tab. Normalizes `\n` to `\r\n` for
RichEdit compatibility. Resets the modified flag.

#### `GetTextLengthW`
```cpp
int GetTextLengthW(HWND hPanel);
```
Returns the character count of the active tab.

#### `GetSelectedTextW`
```cpp
int GetSelectedTextW(HWND hPanel, wchar_t* buf, int bufLen);
```
Returns the selected text of the active tab. NULL buffer returns required length.

#### `SetSelectedTextW`
```cpp
BOOL SetSelectedTextW(HWND hPanel, const wchar_t* text);
```
Replaces the current selection, or inserts at cursor if no selection. Fails if
the panel is in read-only mode.

---

### Tab Management

#### `GetTabCount`
```cpp
int GetTabCount(HWND hPanel);
```
Number of open tabs.

#### `GetActiveTab`
```cpp
int GetActiveTab(HWND hPanel);
```
0-based index of the active tab, or -1 if none.

#### `SetActiveTab`
```cpp
BOOL SetActiveTab(HWND hPanel, int index);
```
Switches to the tab at `index`. Returns FALSE on invalid index.

#### `OpenDocument`
```cpp
int OpenDocument(HWND hPanel, const wchar_t* path);
```
Opens a file from disk into a new tab. If `path` is NULL, creates an empty
untitled tab. Returns the new tab index, or -1 on failure.

The file is read as UTF-8 (BOM detected and stripped). The text is pushed into
a new RichEdit child with the panel's current font/zoom/wrap settings.

#### `CloseTab`
```cpp
BOOL CloseTab(HWND hPanel, int index, BOOL promptSave);
```
Closes the tab at `index`. If `promptSave` is TRUE and the tab has unsaved
changes, shows a Yes/No/Cancel dialog parented to the host window. Returns
FALSE if the user cancelled or the close failed.

#### `GetTabFilePathW`
```cpp
int GetTabFilePathW(HWND hPanel, int index, wchar_t* buf, int bufLen);
```
Returns the file path of the tab at `index`. Returns 0 if no path (untitled) or
invalid index. NULL buffer returns required length.

#### `GetTabTitleW`
```cpp
int GetTabTitleW(HWND hPanel, int index, wchar_t* buf, int bufLen);
```
Returns the display title (includes `●` prefix if modified). NULL buffer returns
required length.

#### `IsTabModified`
```cpp
BOOL IsTabModified(HWND hPanel, int index);
```
Returns TRUE if the tab has unsaved changes.

#### `SaveTab`
```cpp
BOOL SaveTab(HWND hPanel, int index, BOOL saveAs);
```
Saves the tab at `index` to disk. If `saveAs` is TRUE, shows a Save dialog.
Returns TRUE on success.

---

### Theme

Theme modes:

```c
#define NP_THEME_SYSTEM 0   // follow the Windows app theme
#define NP_THEME_LIGHT  1
#define NP_THEME_DARK   2
```

Panels start in `NP_THEME_DARK`, which is the palette this library has always
shipped, so an existing host sees no visual change. The standalone application
selects `NP_THEME_SYSTEM` after creating its panel.

#### `NPTheme` struct
```cpp
typedef struct NPTheme {
    COLORREF bgApp;        // main editor background
    COLORREF bgPanel;      // tab bar / toolbar / status bar
    COLORREF bgCard;       // active tab / search input
    COLORREF bgBtn;        // chip buttons
    COLORREF bgBtnHi;      // pressed buttons and hovered tabs
    COLORREF border;       // strong borders
    COLORREF borderSoft;   // subtle separators
    COLORREF textHi;       // primary text
    COLORREF textMid;      // secondary text
    COLORREF textLow;      // disabled text
    COLORREF accent;       // accent (active tab underline)
    COLORREF selBg;        // selection highlight
    COLORREF iconIdle;     // icon default
    COLORREF iconActive;   // icon hover/active
} NPTheme;
```

#### `SetTheme`
```cpp
BOOL SetTheme(HWND hPanel, const NPTheme* theme);
```
Applies a custom color palette. Pass NULL to restore the palette of the current
theme mode. The struct is copied, so the pointer may be released on return.
Also updates RichEdit background and text color for all tabs and triggers a full
repaint. A custom palette survives `WM_SETTINGCHANGE` until the mode is changed.

#### `GetTheme`
```cpp
BOOL GetTheme(HWND hPanel, NPTheme* outTheme);
```
Reads the current theme palette.

#### `SetThemeMode`
```cpp
BOOL SetThemeMode(HWND hPanel, int mode);
```
Selects a built-in palette. `NP_THEME_SYSTEM` reads
`HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme`
each time it is applied. Returns FALSE for an unknown mode.

A host that wants live theme changes forwards the broadcast to the panel:

```cpp
case WM_SETTINGCHANGE:
    if (lParam && wcscmp((LPCWSTR)lParam, L"ImmersiveColorSet") == 0)
        SendMessageW(hPanel, WM_SETTINGCHANGE, wParam, lParam);
    break;
```

#### `GetThemeMode`
```cpp
int GetThemeMode(HWND hPanel);
```
Returns `NP_THEME_SYSTEM`, `NP_THEME_LIGHT`, or `NP_THEME_DARK`.

Changing the mode raises `NP_NOTIFY_THEME_CHANGED`.

#### Light theme example
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

---

### Font

#### `SetFontW`
```cpp
BOOL SetFontW(HWND hPanel, const LOGFONTW* logFont);
```
Sets the editor font for all tabs. Pass NULL to use the default (Segoe UI 11pt).

#### `GetFontW`
```cpp
BOOL GetFontW(HWND hPanel, LOGFONTW* outLogFont);
```
Reads the current editor font.

---

### View Settings

#### `SetWordWrap` / `GetWordWrap`
```cpp
BOOL SetWordWrap(HWND hPanel, BOOL wrap);
BOOL GetWordWrap(HWND hPanel);
```

#### `SetZoom` / `GetZoom`
```cpp
BOOL SetZoom(HWND hPanel, int percent);   // clamped to 30..500
int  GetZoom(HWND hPanel);
```

#### `SetStatusBarVisible` / `GetStatusBarVisible`
```cpp
BOOL SetStatusBarVisible(HWND hPanel, BOOL visible);
BOOL GetStatusBarVisible(HWND hPanel);
```

#### `SetToolbarVisible` / `GetToolbarVisible`
```cpp
BOOL SetToolbarVisible(HWND hPanel, BOOL visible);
BOOL GetToolbarVisible(HWND hPanel);
```

---

### Read-Only Mode

#### `SetReadOnly` / `GetReadOnly`
```cpp
BOOL SetReadOnly(HWND hPanel, BOOL readOnly);
BOOL GetReadOnly(HWND hPanel);
```
When TRUE, all RichEdit controls are set to read-only. Editing commands (undo,
redo, cut, paste, delete, date/time) are disabled in the menu and toolbar.
Find, zoom, and tab management remain functional. New tabs created while in
read-only mode inherit the flag.

Use this to display AI-generated content that the user should view but not
edit, or to lock a panel during a background operation.

---

### Notifications

#### `NPNotify` struct
```cpp
typedef struct NPNotify {
    HWND    hPanel;      // which panel sent this
    int     instanceId;  // panel instance ID
    UINT    code;        // one of NP_NOTIFY_*
    int     tabIndex;    // affected tab, or -1
    void*   userData;    // pointer from CreateNotepadPanel
} NPNotify;
```

#### Notification codes
| Code | Value | When fired |
|---|---|---|
| `NP_NOTIFY_CONTENT_CHANGED` | 1 | User typed or deleted text |
| `NP_NOTIFY_TAB_CHANGED` | 2 | Active tab switched |
| `NP_NOTIFY_TAB_OPENED` | 3 | A new tab was created |
| `NP_NOTIFY_TAB_CLOSED` | 4 | A tab was closed |
| `NP_NOTIFY_FILE_SAVED` | 5 | A tab was saved to disk |
| `NP_NOTIFY_MODIFIED_CHANGED` | 6 | Dirty flag transitioned |
| `NP_NOTIFY_ZOOM_CHANGED` | 7 | Zoom level changed |
| `NP_NOTIFY_FOCUS_CHANGED` | 8 | Panel gained/lost focus |
| `NP_NOTIFY_THEME_CHANGED` | 9 | Palette or theme mode changed |

#### `NPNOTIFYPROC` callback
```cpp
typedef void (CALLBACK* NPNOTIFYPROC)(const NPNotify* notify);
```
Set at creation time via `CreateNotepadPanel`, or changed later via `SetNotifyProc`.

If no callback is registered, the panel sends `WM_NOTIFY` to the parent window:
```cpp
NMHDR nmh;
nmh.hwndFrom = hPanel;
nmh.idFrom   = instanceId;
nmh.code     = NP_NOTIFY_CONTENT_CHANGED;  // etc
SendMessageW(hParent, WM_NOTIFY, (WPARAM)nmh.idFrom, (LPARAM)&nmh);
```

#### `SetNotifyProc`
```cpp
BOOL SetNotifyProc(HWND hPanel, NPNOTIFYPROC proc, void* userData);
```
Changes the notification callback at runtime. Pass NULL proc to switch to
the `WM_NOTIFY` fallback.

#### Example callback
```cpp
void CALLBACK MyNotify(const NPNotify* n) {
    switch (n->code) {
    case NP_NOTIFY_CONTENT_CHANGED:
        // Mark the document as dirty in your model
        MarkDirty(n->tabIndex);
        break;
    case NP_NOTIFY_FILE_SAVED:
        // The user saved -- update your file watcher
        OnFileSaved(n->tabIndex);
        break;
    case NP_NOTIFY_TAB_CHANGED:
        // Update your UI to reflect the active tab
        UpdateActiveTabIndicator(n->tabIndex);
        break;
    }
}
```

---

### Keyboard Accelerator Coordination

When the panel is embedded, the host and panel may both want to handle the
same keyboard shortcuts (e.g., Ctrl+S). The panel exports its accelerator
table so the host can merge it, and provides a suppression mechanism.

#### `GetAccelerators`
```cpp
int GetAccelerators(NPAccel* outAccels, int maxCount);
```
Returns the panel's full accelerator list. If `outAccels` is NULL, returns the
total count. The host can merge these into its own `ACCEL` table.

```cpp
int count = GetAccelerators(nullptr, 0);
NPAccel* accels = new NPAccel[count];
GetAccelerators(accels, count);
```

#### `SetSuppressedShortcuts`
```cpp
void SetSuppressedShortcuts(HWND hPanel, const int* cmdIds, int count);
```
Tells the panel to stop handling the listed command IDs. The panel checks this
set before processing keyboard shortcuts; if a command is suppressed, it calls
`DefWindowProc` and lets the message bubble to the parent.

Pass `NULL` and `0` to clear all suppressions.

#### Recommended suppressions for AI app embedding
```cpp
int suppressed[] = {
    NP_CMD_SAVE,    // host saves the project
    NP_CMD_OPEN,    // host manages file open
    NP_CMD_NEW,     // host controls document lifecycle
    NP_CMD_EXIT,    // host controls app lifecycle
};
SetSuppressedShortcuts(hPanel, suppressed, 4);
```

The panel retains: undo/redo, find/replace, zoom, wrap, font, tab switching,
tab close, tab rename.

#### Command ID constants
| Constant | Value | Default key |
|---|---|---|
| `NP_CMD_NEW` | 2001 | Ctrl+N, Ctrl+T |
| `NP_CMD_OPEN` | 2002 | Ctrl+O |
| `NP_CMD_SAVE` | 2003 | Ctrl+S |
| `NP_CMD_SAVEAS` | 2004 | Ctrl+Shift+S |
| `NP_CMD_EXIT` | 2005 | (handled as Ctrl+W close-tab) |
| `NP_CMD_UNDO` | 2010 | Ctrl+Z |
| `NP_CMD_REDO` | 2011 | Ctrl+Y |
| `NP_CMD_CUT` | 2012 | Ctrl+X |
| `NP_CMD_COPY` | 2013 | Ctrl+C |
| `NP_CMD_PASTE` | 2014 | Ctrl+V |
| `NP_CMD_DELETE` | 2015 | Del |
| `NP_CMD_SELECTALL` | 2016 | Ctrl+A |
| `NP_CMD_DATETIME` | 2017 | F5 |
| `NP_CMD_FIND` | 2020 | Ctrl+F |
| `NP_CMD_REPLACE` | 2021 | Ctrl+H |
| `NP_CMD_FINDNEXT` | 2022 | F3 |
| `NP_CMD_WRAP` | 2030 | -- |
| `NP_CMD_FONT` | 2031 | -- |
| `NP_CMD_ZOOM_IN` | 2032 | Ctrl++ |
| `NP_CMD_ZOOM_OUT` | 2033 | Ctrl+- |
| `NP_CMD_ZOOM_RESET` | 2034 | Ctrl+0 |
| `NP_CMD_STATUS` | 2040 | -- |
| `NP_CMD_TOOLBAR` | 2041 | -- |

---

## Child Window Behavior

When embedded as a `WS_CHILD`, the panel behaves differently from standalone:

| Feature | Standalone | Embedded |
|---|---|---|
| Title bar | Panel sets its own | Host owns it -- panel skips `SetWindowText` |
| `WM_CLOSE` | Closes the app | Notifies host via `NP_NOTIFY_TAB_CLOSED` with tabIndex=-1 |
| `WM_GETMINMAXINFO` | Enforces minimum size | Skipped (host controls sizing) |
| Floating toolbar | Allowed | Disabled (no `WS_POPUP` outside host bounds) |
| File dialogs | Parented to panel | Parented to host window (`hParent`) |
| Message boxes | Parented to panel | Parented to host window |
| Registry settings | Reads/writes `HKCU\Software\ModernNotepad` | Skipped (host-managed) |

---

## Settings Isolation

### Host-Managed Mode (default for embedded)
When `notifyProc != NULL` or the panel is created as `WS_CHILD`, the panel
enters host-managed mode. It does not read or write the registry. The host
is responsible for:
- Setting the font via `SetFontW`
- Setting zoom via `SetZoom`
- Setting word wrap via `SetWordWrap`
- Setting status bar / toolbar visibility via the respective APIs
- Persisting these settings through its own mechanism

### Self-Managed Mode (standalone exe)
When `notifyProc` is NULL and the panel is not a child window, the panel
reads settings from `HKCU\Software\ModernNotepad` at creation and writes them
on destruction. This preserves backward compatibility with the original
standalone Notepad clone.

---

## File Dialog Parenting

All `OPENFILENAMEW`, `CHOOSEFONTW`, and `MessageBoxW` calls use:
- `inst->hParent` (the host window) in child mode
- `inst->hSelf` (the panel itself) in standalone mode

This ensures dialogs appear modal to the correct window and don't get hidden
behind the host's windows.

---

## RichEdit Theme

The panel applies `SetWindowTheme(hEdit, L"DarkMode_Explorer", nullptr)` for
dark scrollbars by default. When the host applies a light theme via `SetTheme`,
the RichEdit background color is updated via `EM_SETBKGNDCOLOR` and text color
via `EM_SETCHARFORMAT`. The window theme string is not changed -- for a full
light-mode scrollbar appearance, the host would need to call
`SetWindowTheme` on the edit control directly (not exposed in v1).

---

## DWM Dark Title Bar

In standalone mode, the exe wrapper calls
`DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))` on the frame window.
In embedded mode, the host owns the title bar and is responsible for its own
dark/light appearance.

---

## Building

### DLL Target
| Property | Value |
|---|---|
| Configuration Type | Dynamic Library (.dll) |
| Character Set | Unicode |
| C++ Standard | C++20 |
| Preprocessor | `NOTEPAD_PANEL_EXPORTS;_UNICODE;UNICODE` |
| Sources | `NotepadPanel.cpp` |
| Link | `comctl32.lib;shell32.lib;dwmapi.lib;uxtheme.lib` |

### EXE Target
| Property | Value |
|---|---|
| Configuration Type | Application (.exe) |
| C++ Standard | C++20 |
| Preprocessor | `_UNICODE;UNICODE` |
| Sources | `NEWAGENOTEPADMain.cpp` |
| Link | `NotepadPanel.lib` (import library) |

### Original standalone (unchanged)
The original `Notepad clone.cpp` remains as a standalone single-file build.
The new files are additive -- the project can be configured with three
targets: the original exe, the DLL, and the thin wrapper exe.

---

## Constraints

- **Windows 10 1903+** for DWM dark title bar (standalone only)
- **RichEdit 50W** (`msftedit.dll`) must be available -- loaded in `DllMain`
- **DPI awareness** is set by the host process, not the DLL
- **GDI only** -- no Direct2D, no DirectX
- **No COM initialization** -- the DLL does not call `CoInitializeEx`
- **Single-threaded (STA)** -- all calls from the UI thread
