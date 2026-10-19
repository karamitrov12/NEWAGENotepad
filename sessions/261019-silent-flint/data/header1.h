// NotepadPanel.h -- Public C API for embedding the Notepad clone as a child window
//
// This header is the sole public interface for host applications that want to
// embed one or more editor panels.  Link against NotepadPanel.lib (import
// library) or load NotepadPanel.dll dynamically with LoadLibrary.
//
// All string parameters are wide-character (UTF-16).  All functions must be
// called from the thread that owns the panel window (UI thread / STA).
// See NOTEPAD_PANEL_API.md for full documentation.

#ifndef NOTEPAD_PANEL_H
#define NOTEPAD_PANEL_H

#include <windows.h>

#ifdef NOTEPAD_PANEL_STATIC
#define NP_API
#elif defined(NOTEPAD_PANEL_EXPORTS)
#define NP_API __declspec(dllexport)
#else
#define NP_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    // ============================================================
    //  Forward declarations
    // ============================================================

    struct NPTheme;
    struct NPNotify;
    typedef void(CALLBACK *NPNOTIFYPROC)(const NPNotify *notify);

    // ============================================================
    //  Creation / destruction
    // ============================================================

    // initialize themselves during process attach, so callers may omit this.
    NP_API BOOL InitializeNotepadPanel(HINSTANCE hInstance);

    // Returns the panel HWND, or NULL on failure.
    //
    //   hParent    -- host window that will contain the panel
    //   rc         -- initial position/size in parent client coords
    //                 (pass NULL or {0,0,0,0} to fill parent's client area)
    //   id         -- child window ID (for WM_COMMAND routing)
    //   notifyProc -- optional callback for editor events (may be NULL)
    //   userData   -- opaque pointer passed back to notifyProc
    NP_API HWND CreateNotepadPanel(HWND hParent, const RECT *rc, int id, NPNOTIFYPROC notifyProc,
                                   void *userData);

    // Destroy a panel and free all its resources.
    // Safe to call on an already-destroyed HWND (returns FALSE).
    NP_API BOOL DestroyNotepadPanel(HWND hPanel);

    // Resize / reposition a panel within its parent.

    NP_API BOOL ResizeNotepadPanel(HWND hPanel, const RECT *rc);

    // ============================================================
    //  Content I/O
    // ============================================================

    // Get the full text of the active tab.
    // If buf is NULL, returns the required length (excluding null terminator).
    // Otherwise copies up to bufLen-1 characters and returns the actual length.
    NP_API int GetTextW(HWND hPanel, wchar_t *buf, int bufLen);

    // Replace the entire content of the active tab.
    NP_API BOOL SetTextW(HWND hPanel, const wchar_t *text);

    // Get the text length (character count) of the active tab.
    NP_API int GetTextLengthW(HWND hPanel);

    // Get the selected text of the active tab.
    // If buf is NULL, returns the required length.
    NP_API int GetSelectedTextW(HWND hPanel, wchar_t *buf, int bufLen);

    // Replace the current selection (or insert at cursor if none).
    NP_API BOOL SetSelectedTextW(HWND hPanel, const wchar_t *text);

    // ============================================================
    //  Tab management
    // ============================================================

    // Number of open tabs.
    NP_API int GetTabCount(HWND hPanel);

    // Index of the active tab (0-based), or -1 if none.
    NP_API int GetActiveTab(HWND hPanel);

    // Switch to tab at index. Returns TRUE on success.
    NP_API BOOL SetActiveTab(HWND hPanel, int index);

    // Open a file into a new tab. If path is NULL, creates an empty tab.
    // Returns the new tab index, or -1 on failure.
    NP_API int OpenDocument(HWND hPanel, const wchar_t *path);

    // Close a tab. If promptSave is TRUE and the tab has unsaved changes,
    // shows a confirmation dialog parented to the host.
    // Returns TRUE if the tab was closed.
    NP_API BOOL CloseTab(HWND hPanel, int index, BOOL promptSave);

    // Get the file path of the tab at index.
    // Returns the string length (excluding null), or 0 if no path / invalid index.
    // If buf is NULL, returns the required length.
    NP_API int GetTabFilePathW(HWND hPanel, int index, wchar_t *buf, int bufLen);

    // Get the display title of the tab at index.
    // If buf is NULL, returns the required length.
    NP_API int GetTabTitleW(HWND hPanel, int index, wchar_t *buf, int bufLen);

    // Check whether the tab at index has unsaved modifications.
    NP_API BOOL IsTabModified(HWND hPanel, int index);

    // Save the tab at index. If saveAs is TRUE, shows a Save dialog.
    NP_API BOOL SaveTab(HWND hPanel, int index, BOOL saveAs);

    // ============================================================
    //  Theme
    // ============================================================

    typedef struct NPTheme
    {
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
    NP_API BOOL SetTheme(HWND hPanel, const NPTheme *theme);

    // Get the current theme.
    NP_API BOOL GetTheme(HWND hPanel, NPTheme *outTheme);

    // ============================================================
    //  Font
    // ============================================================

    NP_API BOOL SetFontW(HWND hPanel, const LOGFONTW *logFont);

    // Get the current editor font.
    NP_API BOOL GetFontW(HWND hPanel, LOGFONTW *outLogFont);

    // ============================================================
    //  View settings
    // ============================================================

    NP_API BOOL SetWordWrap(HWND hPanel, BOOL wrap);
    NP_API BOOL GetWordWrap(HWND hPanel);

    NP_API BOOL SetZoom(HWND hPanel, int percent);
    NP_API int GetZoom(HWND hPanel);

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

#define NP_NOTIFY_CONTENT_CHANGED 1
#define NP_NOTIFY_TAB_CHANGED 2
#define NP_NOTIFY_TAB_OPENED 3
#define NP_NOTIFY_TAB_CLOSED 4
#define NP_NOTIFY_FILE_SAVED 5
#define NP_NOTIFY_MODIFIED_CHANGED 6
#define NP_NOTIFY_ZOOM_CHANGED 7
#define NP_NOTIFY_FOCUS_CHANGED 8

    typedef struct NPNotify
    {
        HWND hPanel;
        int instanceId;
        UINT code;
        int tabIndex;
        void *userData;
    } NPNotify;

    // Change the notification callback at runtime.
    NP_API BOOL SetNotifyProc(HWND hPanel, NPNOTIFYPROC proc, void *userData);

    // ============================================================
    //  Keyboard accelerator coordination
    // ============================================================

    typedef struct NPAccel
    {
        int cmd;
        WORD virtKey;
        WORD key;
    } NPAccel;

    // Get the panel's accelerator list. The host merges these into its own table.
    // If outAccels is NULL, returns the total count.
    NP_API int GetAccelerators(NPAccel *outAccels, int maxCount);

    // Tell the panel to stop handling the listed command IDs.
    // The host should call this for shortcuts it wants to own (e.g. IDM_SAVE).

    NP_API void SetSuppressedShortcuts(HWND hPanel, const int *cmdIds, int count);

    // ============================================================
    //  Command IDs (for accelerator coordination)
    // ============================================================

#define NP_CMD_NEW 2001
#define NP_CMD_OPEN 2002
#define NP_CMD_SAVE 2003
#define NP_CMD_SAVEAS 2004
#define NP_CMD_EXIT 2005
#define NP_CMD_UNDO 2010
#define NP_CMD_REDO 2011
#define NP_CMD_CUT 2012
#define NP_CMD_COPY 2013
#define NP_CMD_PASTE 2014
#define NP_CMD_DELETE 2015
#define NP_CMD_SELECTALL 2016
#define NP_CMD_DATETIME 2017
#define NP_CMD_FIND 2020
#define NP_CMD_REPLACE 2021
#define NP_CMD_FINDNEXT 2022
#define NP_CMD_WRAP 2030
#define NP_CMD_FONT 2031
#define NP_CMD_ZOOM_IN 2032
#define NP_CMD_ZOOM_OUT 2033
#define NP_CMD_ZOOM_RESET 2034
#define NP_CMD_STATUS 2040
#define NP_CMD_TOOLBAR 2041

#ifdef __cplusplus
}
#endif

#endif
