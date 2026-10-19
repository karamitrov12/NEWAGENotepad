// NotepadPanel.h -- Public C API for embedding the Notepad clone as a child window
//
// This header is the sole public interface for host applications that want to
// embed one or more editor panels.  Link against NotepadPanel.lib (import
// library) or load NotepadPanel.dll dynamically with LoadLibrary.
//
// All string parameters are wide-character (UTF-16).  All functions must be
// called from the thread that owns the panel window (UI thread / STA).
// See NOTEPAD_PANEL_API.md for full documentation.

#ifndef NOTEPAD_PANEL_H // Use this compatibility fallback only when NOTEPAD_PANEL_H is not defined.
#define NOTEPAD_PANEL_H // Define NOTEPAD_PANEL_H as a compile-time setting.

#include <windows.h> // Import the Win32 API.

#ifdef NOTEPAD_PANEL_STATIC // Compile this API branch only when NOTEPAD_PANEL_STATIC is defined.
  #define NP_API // Define NP_API as a compile-time setting.
#elif defined(NOTEPAD_PANEL_EXPORTS) // Select this compile-time alternative when defined(NOTEPAD_PANEL_EXPORTS) holds.
  #define NP_API __declspec(dllexport) // Define NP_API as a compile-time setting with value __declspec(dllexport).
#else // Select the fallback branch of the current compile-time condition.
  #define NP_API __declspec(dllimport) // Define NP_API as a compile-time setting with value __declspec(dllimport).
#endif // Close the current compile-time condition.

#ifdef __cplusplus // Compile this API branch only when __cplusplus is defined.
extern "C" { // Continue the current declaration or expression.
#endif // Close the current compile-time condition.

// ============================================================
//  Forward declarations
// ============================================================

struct NPTheme; // Define NPTheme to group the state or values used by this component.
struct NPNotify; // Define NPNotify to group the state or values used by this component.
typedef void (CALLBACK* NPNOTIFYPROC)(const NPNotify* notify); // Create a reusable alias for this type or callback signature.

// ============================================================
//  Creation / destruction
// ============================================================


// initialize themselves during process attach, so callers may omit this.
NP_API BOOL InitializeNotepadPanel(HINSTANCE hInstance); // Complete the current operation before proceeding to the next statement.


// Returns the panel HWND, or NULL on failure.
//
//   hParent    -- host window that will contain the panel
//   rc         -- initial position/size in parent client coords
//                 (pass NULL or {0,0,0,0} to fill parent's client area)
//   id         -- child window ID (for WM_COMMAND routing)
//   notifyProc -- optional callback for editor events (may be NULL)
//   userData   -- opaque pointer passed back to notifyProc
NP_API HWND CreateNotepadPanel( // Continue the current declaration or expression.
    HWND         hParent, // Supply this parameter or list item to the enclosing call.
    const RECT*  rc, // Supply this parameter or list item to the enclosing call.
    int          id, // Supply this parameter or list item to the enclosing call.
    NPNOTIFYPROC notifyProc, // Supply this parameter or list item to the enclosing call.
    void*        userData // Continue the current declaration or expression.
); // Complete the current operation before proceeding to the next statement.

// Destroy a panel and free all its resources.
// Safe to call on an already-destroyed HWND (returns FALSE).
NP_API BOOL DestroyNotepadPanel(HWND hPanel); // Complete the current operation before proceeding to the next statement.

// Resize / reposition a panel within its parent.

NP_API BOOL ResizeNotepadPanel(HWND hPanel, const RECT* rc); // Complete the current operation before proceeding to the next statement.

// ============================================================
//  Content I/O
// ============================================================

// Get the full text of the active tab.
// If buf is NULL, returns the required length (excluding null terminator).
// Otherwise copies up to bufLen-1 characters and returns the actual length.
NP_API int  GetTextW(HWND hPanel, wchar_t* buf, int bufLen); // Complete the current operation before proceeding to the next statement.

// Replace the entire content of the active tab.
NP_API BOOL SetTextW(HWND hPanel, const wchar_t* text); // Complete the current operation before proceeding to the next statement.

// Get the text length (character count) of the active tab.
NP_API int  GetTextLengthW(HWND hPanel); // Complete the current operation before proceeding to the next statement.

// Get the selected text of the active tab.
// If buf is NULL, returns the required length.
NP_API int  GetSelectedTextW(HWND hPanel, wchar_t* buf, int bufLen); // Complete the current operation before proceeding to the next statement.

// Replace the current selection (or insert at cursor if none).
NP_API BOOL SetSelectedTextW(HWND hPanel, const wchar_t* text); // Complete the current operation before proceeding to the next statement.

// ============================================================
//  Tab management
// ============================================================

// Number of open tabs.
NP_API int  GetTabCount(HWND hPanel); // Complete the current operation before proceeding to the next statement.

// Index of the active tab (0-based), or -1 if none.
NP_API int  GetActiveTab(HWND hPanel); // Complete the current operation before proceeding to the next statement.

// Switch to tab at index. Returns TRUE on success.
NP_API BOOL SetActiveTab(HWND hPanel, int index); // Complete the current operation before proceeding to the next statement.

// Open a file into a new tab. If path is NULL, creates an empty tab.
// Returns the new tab index, or -1 on failure.
NP_API int  OpenDocument(HWND hPanel, const wchar_t* path); // Complete the current operation before proceeding to the next statement.

// Close a tab. If promptSave is TRUE and the tab has unsaved changes,
// shows a confirmation dialog parented to the host.
// Returns TRUE if the tab was closed.
NP_API BOOL CloseTab(HWND hPanel, int index, BOOL promptSave); // Complete the current operation before proceeding to the next statement.

// Get the file path of the tab at index.
// Returns the string length (excluding null), or 0 if no path / invalid index.
// If buf is NULL, returns the required length.
NP_API int  GetTabFilePathW(HWND hPanel, int index, wchar_t* buf, int bufLen); // Complete the current operation before proceeding to the next statement.

// Get the display title of the tab at index.
// If buf is NULL, returns the required length.
NP_API int  GetTabTitleW(HWND hPanel, int index, wchar_t* buf, int bufLen); // Complete the current operation before proceeding to the next statement.

// Check whether the tab at index has unsaved modifications.
NP_API BOOL IsTabModified(HWND hPanel, int index); // Complete the current operation before proceeding to the next statement.

// Save the tab at index. If saveAs is TRUE, shows a Save dialog.
NP_API BOOL SaveTab(HWND hPanel, int index, BOOL saveAs); // Complete the current operation before proceeding to the next statement.

// ============================================================
//  Theme
// ============================================================

typedef struct NPTheme { // Define NPTheme to group the state or values used by this component.
    COLORREF bgApp;        // main background
    COLORREF bgPanel;      // tab bar / toolbar / status bar background
    COLORREF bgCard;       // active tab / search bar input background
    COLORREF bgBtn;        // chip buttons
    COLORREF bgBtnHi;      // pressed button background
    COLORREF border;       // strong border
    COLORREF borderSoft;   // subtle separator
    COLORREF textHi;       // primary text
    COLORREF textMid;      // secondary text
    COLORREF textLow;      // disabled / placeholder text
    COLORREF accent;       // accent color (active tab underline)
    COLORREF selBg;        // selection highlight
    COLORREF iconIdle;     // icon default color
    COLORREF iconActive;   // icon active/hover color
} NPTheme; // Complete the current operation before proceeding to the next statement.

// Apply a custom theme. Pass NULL to revert to the default dark theme.
NP_API BOOL SetTheme(HWND hPanel, const NPTheme* theme); // Complete the current operation before proceeding to the next statement.

// Get the current theme.
NP_API BOOL GetTheme(HWND hPanel, NPTheme* outTheme); // Complete the current operation before proceeding to the next statement.

// ============================================================
//  Font
// ============================================================


NP_API BOOL SetFontW(HWND hPanel, const LOGFONTW* logFont); // Complete the current operation before proceeding to the next statement.

// Get the current editor font.
NP_API BOOL GetFontW(HWND hPanel, LOGFONTW* outLogFont); // Complete the current operation before proceeding to the next statement.

// ============================================================
//  View settings
// ============================================================

NP_API BOOL SetWordWrap(HWND hPanel, BOOL wrap); // Complete the current operation before proceeding to the next statement.
NP_API BOOL GetWordWrap(HWND hPanel); // Complete the current operation before proceeding to the next statement.

NP_API BOOL SetZoom(HWND hPanel, int percent);   // 30..500
NP_API int  GetZoom(HWND hPanel); // Complete the current operation before proceeding to the next statement.

NP_API BOOL SetStatusBarVisible(HWND hPanel, BOOL visible); // Complete the current operation before proceeding to the next statement.
NP_API BOOL GetStatusBarVisible(HWND hPanel); // Complete the current operation before proceeding to the next statement.

NP_API BOOL SetToolbarVisible(HWND hPanel, BOOL visible); // Complete the current operation before proceeding to the next statement.
NP_API BOOL GetToolbarVisible(HWND hPanel); // Complete the current operation before proceeding to the next statement.

// ============================================================
//  Read-only / lock mode
// ============================================================

NP_API BOOL SetReadOnly(HWND hPanel, BOOL readOnly); // Complete the current operation before proceeding to the next statement.
NP_API BOOL GetReadOnly(HWND hPanel); // Complete the current operation before proceeding to the next statement.

// ============================================================
//  Notifications
// ============================================================

#define NP_NOTIFY_CONTENT_CHANGED  1   // user typed/deleted text
#define NP_NOTIFY_TAB_CHANGED      2   // active tab switched
#define NP_NOTIFY_TAB_OPENED       3   // a new tab was created
#define NP_NOTIFY_TAB_CLOSED       4   // a tab was closed
#define NP_NOTIFY_FILE_SAVED       5   // a tab was saved to disk
#define NP_NOTIFY_MODIFIED_CHANGED 6   // dirty flag flipped
#define NP_NOTIFY_ZOOM_CHANGED     7   // zoom level changed
#define NP_NOTIFY_FOCUS_CHANGED    8   // panel gained/lost focus

typedef struct NPNotify { // Define NPNotify to group the state or values used by this component.
    HWND    hPanel;      // which panel sent this
    int     instanceId;  // panel instance ID
    UINT    code;        // one of NP_NOTIFY_*
    int     tabIndex;    // affected tab, or -1
    void*   userData;    // pointer passed to CreateNotepadPanel
} NPNotify; // Complete the current operation before proceeding to the next statement.

// Change the notification callback at runtime.
NP_API BOOL SetNotifyProc(HWND hPanel, NPNOTIFYPROC proc, void* userData); // Complete the current operation before proceeding to the next statement.

// ============================================================
//  Keyboard accelerator coordination
// ============================================================

typedef struct NPAccel { // Define NPAccel to group the state or values used by this component.
    int     cmd;       // command ID (see NOTEPAD_PANEL_API.md for the list)
    WORD    virtKey;   // FVIRTKEY | FCONTROL | FSHIFT | FALT
    WORD    key;       // virtual key code
} NPAccel; // Complete the current operation before proceeding to the next statement.

// Get the panel's accelerator list. The host merges these into its own table.
// If outAccels is NULL, returns the total count.
NP_API int  GetAccelerators(NPAccel* outAccels, int maxCount); // Complete the current operation before proceeding to the next statement.

// Tell the panel to stop handling the listed command IDs.
// The host should call this for shortcuts it wants to own (e.g. IDM_SAVE).

NP_API void SetSuppressedShortcuts(HWND hPanel, const int* cmdIds, int count); // Complete the current operation before proceeding to the next statement.

// ============================================================
//  Command IDs (for accelerator coordination)
// ============================================================

#define NP_CMD_NEW          2001 // Define NP_CMD_NEW as a menu command ID with value 2001.
#define NP_CMD_OPEN         2002 // Define NP_CMD_OPEN as a menu command ID with value 2002.
#define NP_CMD_SAVE         2003 // Define NP_CMD_SAVE as a menu command ID with value 2003.
#define NP_CMD_SAVEAS       2004 // Define NP_CMD_SAVEAS as a menu command ID with value 2004.
#define NP_CMD_EXIT         2005 // Define NP_CMD_EXIT as a menu command ID with value 2005.
#define NP_CMD_UNDO         2010 // Define NP_CMD_UNDO as a menu command ID with value 2010.
#define NP_CMD_REDO         2011 // Define NP_CMD_REDO as a menu command ID with value 2011.
#define NP_CMD_CUT          2012 // Define NP_CMD_CUT as a menu command ID with value 2012.
#define NP_CMD_COPY         2013 // Define NP_CMD_COPY as a menu command ID with value 2013.
#define NP_CMD_PASTE        2014 // Define NP_CMD_PASTE as a menu command ID with value 2014.
#define NP_CMD_DELETE       2015 // Define NP_CMD_DELETE as a menu command ID with value 2015.
#define NP_CMD_SELECTALL    2016 // Define NP_CMD_SELECTALL as a menu command ID with value 2016.
#define NP_CMD_DATETIME     2017 // Define NP_CMD_DATETIME as a menu command ID with value 2017.
#define NP_CMD_FIND         2020 // Define NP_CMD_FIND as a menu command ID with value 2020.
#define NP_CMD_REPLACE      2021 // Define NP_CMD_REPLACE as a menu command ID with value 2021.
#define NP_CMD_FINDNEXT     2022 // Define NP_CMD_FINDNEXT as a menu command ID with value 2022.
#define NP_CMD_WRAP         2030 // Define NP_CMD_WRAP as a menu command ID with value 2030.
#define NP_CMD_FONT         2031 // Define NP_CMD_FONT as a menu command ID with value 2031.
#define NP_CMD_ZOOM_IN     2032 // Define NP_CMD_ZOOM_IN as a menu command ID with value 2032.
#define NP_CMD_ZOOM_OUT    2033 // Define NP_CMD_ZOOM_OUT as a menu command ID with value 2033.
#define NP_CMD_ZOOM_RESET  2034 // Define NP_CMD_ZOOM_RESET as a menu command ID with value 2034.
#define NP_CMD_STATUS      2040 // Define NP_CMD_STATUS as a menu command ID with value 2040.
#define NP_CMD_TOOLBAR     2041 // Define NP_CMD_TOOLBAR as a menu command ID with value 2041.

#ifdef __cplusplus // Compile this API branch only when __cplusplus is defined.
} // Close the current block.
#endif // Close the current compile-time condition.

#endif // NOTEPAD_PANEL_H
