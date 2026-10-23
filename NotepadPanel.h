// NotepadPanel.h -- Public C API for embedding the Notepad clone as a child window
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

// ---- Forward declarations ----
struct NPTheme;
struct NPNotify;
typedef void(CALLBACK *NPNOTIFYPROC)(const NPNotify *notify);

// ---- Creation / destruction ----
NP_API BOOL InitializeNotepadPanel(HINSTANCE hInstance);
NP_API HWND CreateNotepadPanel(HWND hParent, const RECT *rc, int id, NPNOTIFYPROC notifyProc,
                               void *userData);
NP_API BOOL DestroyNotepadPanel(HWND hPanel);
NP_API BOOL ResizeNotepadPanel(HWND hPanel, const RECT *rc);

// ---- Content I/O ----
NP_API int GetTextW(HWND hPanel, wchar_t *buf, int bufLen);
NP_API BOOL SetTextW(HWND hPanel, const wchar_t *text);
NP_API int GetTextLengthW(HWND hPanel);
NP_API int GetSelectedTextW(HWND hPanel, wchar_t *buf, int bufLen);
NP_API BOOL SetSelectedTextW(HWND hPanel, const wchar_t *text);

// ---- Tab management ----
NP_API int GetTabCount(HWND hPanel);
NP_API int GetActiveTab(HWND hPanel);
NP_API BOOL SetActiveTab(HWND hPanel, int index);
NP_API int OpenDocument(HWND hPanel, const wchar_t *path);
NP_API BOOL CloseTab(HWND hPanel, int index, BOOL promptSave);
NP_API int GetTabFilePathW(HWND hPanel, int index, wchar_t *buf, int bufLen);
NP_API int GetTabTitleW(HWND hPanel, int index, wchar_t *buf, int bufLen);
NP_API BOOL IsTabModified(HWND hPanel, int index);
NP_API BOOL SaveTab(HWND hPanel, int index, BOOL saveAs);

// ---- Theme ----
#define NP_THEME_SYSTEM 0
#define NP_THEME_LIGHT 1
#define NP_THEME_DARK 2

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

NP_API BOOL SetTheme(HWND hPanel, const NPTheme *theme);
NP_API BOOL GetTheme(HWND hPanel, NPTheme *outTheme);
NP_API BOOL SetThemeMode(HWND hPanel, int mode);
NP_API int GetThemeMode(HWND hPanel);

// ---- Font ----
NP_API BOOL SetFontW(HWND hPanel, const LOGFONTW *logFont);
NP_API BOOL GetFontW(HWND hPanel, LOGFONTW *outLogFont);

// ---- View settings ----
NP_API BOOL SetWordWrap(HWND hPanel, BOOL wrap);
NP_API BOOL GetWordWrap(HWND hPanel);
NP_API BOOL SetZoom(HWND hPanel, int percent);
NP_API int GetZoom(HWND hPanel);
NP_API BOOL SetStatusBarVisible(HWND hPanel, BOOL visible);
NP_API BOOL GetStatusBarVisible(HWND hPanel);
NP_API BOOL SetToolbarVisible(HWND hPanel, BOOL visible);
NP_API BOOL GetToolbarVisible(HWND hPanel);

// ---- Read-only / lock mode ----
NP_API BOOL SetReadOnly(HWND hPanel, BOOL readOnly);
NP_API BOOL GetReadOnly(HWND hPanel);

// ---- Notifications ----
#define NP_NOTIFY_CONTENT_CHANGED 1
#define NP_NOTIFY_TAB_CHANGED 2
#define NP_NOTIFY_TAB_OPENED 3
#define NP_NOTIFY_TAB_CLOSED 4
#define NP_NOTIFY_FILE_SAVED 5
#define NP_NOTIFY_MODIFIED_CHANGED 6
#define NP_NOTIFY_ZOOM_CHANGED 7
#define NP_NOTIFY_FOCUS_CHANGED 8
#define NP_NOTIFY_THEME_CHANGED 9

typedef struct NPNotify
{
    HWND hPanel;
    int instanceId;
    UINT code;
    int tabIndex;
    void *userData;
} NPNotify;

NP_API BOOL SetNotifyProc(HWND hPanel, NPNOTIFYPROC proc, void *userData);

// ---- Keyboard accelerator coordination ----
typedef struct NPAccel
{
    int cmd;
    WORD virtKey;
    WORD key;
} NPAccel;
NP_API int GetAccelerators(NPAccel *outAccels, int maxCount);
NP_API void SetSuppressedShortcuts(HWND hPanel, const int *cmdIds, int count);

// ---- Command IDs ----
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
#define NP_CMD_THEME_SYSTEM 2060
#define NP_CMD_THEME_LIGHT 2061
#define NP_CMD_THEME_DARK 2062

#ifdef __cplusplus
}
#endif

#endif // NOTEPAD_PANEL_H
