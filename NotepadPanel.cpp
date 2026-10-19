// NotepadPanel.cpp -- Embeddable Notepad clone panel DLL
//
// Full refactor of the standalone Notepad clone into a DLL that can be
// embedded as a child window inside a host application.  All per-editor
// state lives in a NotepadInstance struct resolved from the panel HWND
// via GWLP_USERDATA.  The public C API is exported via __declspec(dllexport).
//
// Build: configure as a DLL with NOTEPAD_PANEL_EXPORTS defined.
//        Links comctl32, shell32, dwmapi, uxtheme, msftedit (delay-load).

#ifndef WIN32_LEAN_AND_MEAN // Use this compatibility fallback only when WIN32_LEAN_AND_MEAN is not defined.
#define WIN32_LEAN_AND_MEAN // Define WIN32_LEAN_AND_MEAN as a compile-time setting.
#endif // Close the current compile-time condition.
#ifndef NOMINMAX // Use this compatibility fallback only when NOMINMAX is not defined.
#define NOMINMAX // Define NOMINMAX as a compile-time setting.
#endif // Close the current compile-time condition.
#ifndef _WIN32_IE // Use this compatibility fallback only when _WIN32_IE is not defined.
#define _WIN32_IE 0x0600 // Define _WIN32_IE as a compile-time setting with value 0x0600.
#endif // Close the current compile-time condition.

#include <windows.h> // Import the Win32 API.
#include <windowsx.h> // Import Win32 helper macros.
#include <commctrl.h> // Import common controls.
#include <richedit.h> // Import Rich Edit messages.
#include <shellapi.h> // Import Windows shell integration.
#include <commdlg.h> // Import standard file and font dialogs.
#include <dwmapi.h> // Import Desktop Window Manager attributes.
#include <uxtheme.h> // Import Windows theme APIs.
#include <string> // Import Unicode strings.
#include <vector> // Import dynamic arrays.
#include <fstream> // Import file streams.
#include <sstream> // Import string streams.
#include <filesystem> // Import filesystem paths.
#include <algorithm> // Import standard algorithms.
#include <cwchar> // Import wide-character string functions.
#include <unordered_set> // Import hash-based lookup sets.

#ifndef NOTEPAD_PANEL_EXPORTS // Use this compatibility fallback only when NOTEPAD_PANEL_EXPORTS is not defined.
#define NOTEPAD_PANEL_EXPORTS // Define NOTEPAD_PANEL_EXPORTS as a compile-time setting.
#endif // Close the current compile-time condition.
#include "NotepadPanel.h" // Import the editor panel API.

#pragma comment(lib, "comctl32.lib") // Link the comctl32.lib Windows library.
#pragma comment(lib, "shell32.lib") // Link the shell32.lib Windows library.
#pragma comment(lib, "dwmapi.lib") // Link the dwmapi.lib Windows library.
#pragma comment(lib, "uxtheme.lib") // Link the uxtheme.lib Windows library.
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"") // Embed the Common Controls v6 manifest dependency in the executable.

namespace fs = std::filesystem; // Create a namespace scope or alias for related declarations.

// ============================================================
//  Constants
// ============================================================
static const wchar_t PANEL_CLASS[]  = L"NotepadPanel"; // Name the panel window class that Win32 registers for this component.
static const wchar_t TOOLBAR_CLASS[] = L"NotepadPanelToolbar"; // Name the toolbar window class that Win32 registers for this component.
static const wchar_t MENU_CLASS[]   = L"NotepadPanelMenu"; // Name the menu window class that Win32 registers for this component.
static const wchar_t APP_NAME[]     = L"Neage Notepad"; // Declare app name and start with L"Notepad".
static const wchar_t REG_KEY[]      = L"Software\\ModernNotepad"; // Declare reg key and start with L"Software\\ModernNotepad".

// Control IDs (same values as the standalone, kept for compatibility)
#define IDC_EDIT            1001 // Define IDC_EDIT as a child-control ID with value 1001.
#define IDC_SEARCH_FIND     1010 // Define IDC_SEARCH_FIND as a child-control ID with value 1010.
#define IDC_SEARCH_REPL     1011 // Define IDC_SEARCH_REPL as a child-control ID with value 1011.
#define IDC_SEARCH_NEXT     1012 // Define IDC_SEARCH_NEXT as a child-control ID with value 1012.
#define IDC_SEARCH_PREV     1013 // Define IDC_SEARCH_PREV as a child-control ID with value 1013.
#define IDC_SEARCH_CLOSE    1014 // Define IDC_SEARCH_CLOSE as a child-control ID with value 1014.
#define IDC_SEARCH_CASE     1015 // Define IDC_SEARCH_CASE as a child-control ID with value 1015.
#define IDC_SEARCH_REPL_ONE 1016 // Define IDC_SEARCH_REPL_ONE as a child-control ID with value 1016.
#define IDC_SEARCH_REPL_ALL 1017 // Define IDC_SEARCH_REPL_ALL as a child-control ID with value 1017.
#define IDC_TB_MENU     1100 // Define IDC_TB_MENU as a child-control ID with value 1100.
#define IDC_TB_NEW      1101 // Define IDC_TB_NEW as a child-control ID with value 1101.
#define IDC_TB_OPEN     1102 // Define IDC_TB_OPEN as a child-control ID with value 1102.
#define IDC_TB_SAVE     1103 // Define IDC_TB_SAVE as a child-control ID with value 1103.
#define IDC_TB_UNDO     1104 // Define IDC_TB_UNDO as a child-control ID with value 1104.
#define IDC_TB_REDO     1105 // Define IDC_TB_REDO as a child-control ID with value 1105.
#define IDC_TB_FIND     1106 // Define IDC_TB_FIND as a child-control ID with value 1106.
#define IDC_TB_ZOOM_OUT 1107 // Define IDC_TB_ZOOM_OUT as a child-control ID with value 1107.
#define IDC_TB_ZOOM_IN  1108 // Define IDC_TB_ZOOM_IN as a child-control ID with value 1108.
#define IDC_TB_WRAP     1109 // Define IDC_TB_WRAP as a child-control ID with value 1109.
// IDM_* match NP_CMD_* values in the public header
#define IDM_NEW          NP_CMD_NEW // Define IDM_NEW as a menu command ID with value NP_CMD_NEW.
#define IDM_OPEN         NP_CMD_OPEN // Define IDM_OPEN as a menu command ID with value NP_CMD_OPEN.
#define IDM_SAVE         NP_CMD_SAVE // Define IDM_SAVE as a menu command ID with value NP_CMD_SAVE.
#define IDM_SAVEAS       NP_CMD_SAVEAS // Define IDM_SAVEAS as a menu command ID with value NP_CMD_SAVEAS.
#define IDM_EXIT         NP_CMD_EXIT // Define IDM_EXIT as a menu command ID with value NP_CMD_EXIT.
#define IDM_UNDO         NP_CMD_UNDO // Define IDM_UNDO as a menu command ID with value NP_CMD_UNDO.
#define IDM_REDO         NP_CMD_REDO // Define IDM_REDO as a menu command ID with value NP_CMD_REDO.
#define IDM_CUT          NP_CMD_CUT // Define IDM_CUT as a menu command ID with value NP_CMD_CUT.
#define IDM_COPY         NP_CMD_COPY // Define IDM_COPY as a menu command ID with value NP_CMD_COPY.
#define IDM_PASTE        NP_CMD_PASTE // Define IDM_PASTE as a menu command ID with value NP_CMD_PASTE.
#define IDM_DELETE       NP_CMD_DELETE // Define IDM_DELETE as a menu command ID with value NP_CMD_DELETE.
#define IDM_SELECTALL    NP_CMD_SELECTALL // Define IDM_SELECTALL as a menu command ID with value NP_CMD_SELECTALL.
#define IDM_DATETIME     NP_CMD_DATETIME // Define IDM_DATETIME as a menu command ID with value NP_CMD_DATETIME.
#define IDM_FIND         NP_CMD_FIND // Define IDM_FIND as a menu command ID with value NP_CMD_FIND.
#define IDM_REPLACE      NP_CMD_REPLACE // Define IDM_REPLACE as a menu command ID with value NP_CMD_REPLACE.
#define IDM_FINDNEXT     NP_CMD_FINDNEXT // Define IDM_FINDNEXT as a menu command ID with value NP_CMD_FINDNEXT.
#define IDM_WRAP         NP_CMD_WRAP // Define IDM_WRAP as a menu command ID with value NP_CMD_WRAP.
#define IDM_FONT         NP_CMD_FONT // Define IDM_FONT as a menu command ID with value NP_CMD_FONT.
#define IDM_ZOOM_IN      NP_CMD_ZOOM_IN // Define IDM_ZOOM_IN as a menu command ID with value NP_CMD_ZOOM_IN.
#define IDM_ZOOM_OUT     NP_CMD_ZOOM_OUT // Define IDM_ZOOM_OUT as a menu command ID with value NP_CMD_ZOOM_OUT.
#define IDM_ZOOM_RESET   NP_CMD_ZOOM_RESET // Define IDM_ZOOM_RESET as a menu command ID with value NP_CMD_ZOOM_RESET.
#define IDM_STATUS       NP_CMD_STATUS // Define IDM_STATUS as a menu command ID with value NP_CMD_STATUS.
#define IDM_TOOLBAR      NP_CMD_TOOLBAR // Define IDM_TOOLBAR as a menu command ID with value NP_CMD_TOOLBAR.
#define IDM_ABOUT        2050 // Define IDM_ABOUT as a menu command ID with value 2050.
#define IDM_TB_DOCK_TOP     3001 // Define IDM_TB_DOCK_TOP as a menu command ID with value 3001.
#define IDM_TB_DOCK_BOTTOM  3002 // Define IDM_TB_DOCK_BOTTOM as a menu command ID with value 3002.
#define IDM_TB_DOCK_LEFT    3003 // Define IDM_TB_DOCK_LEFT as a menu command ID with value 3003.
#define IDM_TB_DOCK_RIGHT   3004 // Define IDM_TB_DOCK_RIGHT as a menu command ID with value 3004.
#define IDM_TB_DOCK_FLOAT   3005 // Define IDM_TB_DOCK_FLOAT as a menu command ID with value 3005.
#define IDM_TB_SIZE_SMALL   3011 // Define IDM_TB_SIZE_SMALL as a menu command ID with value 3011.
#define IDM_TB_SIZE_MEDIUM  3012 // Define IDM_TB_SIZE_MEDIUM as a menu command ID with value 3012.
#define IDM_TB_SIZE_LARGE   3013 // Define IDM_TB_SIZE_LARGE as a menu command ID with value 3013.
#define IDM_TAB_RENAME      4001 // Define IDM_TAB_RENAME as a menu command ID with value 4001.
#define IDM_TAB_CLOSE       4002 // Define IDM_TAB_CLOSE as a menu command ID with value 4002.
#define IDM_TAB_CLOSE_OTHERS 4003 // Define IDM_TAB_CLOSE_OTHERS as a menu command ID with value 4003.
#define IDM_TAB_CLOSE_RIGHT  4004 // Define IDM_TAB_CLOSE_RIGHT as a menu command ID with value 4004.
#define IDM_TAB_CLOSE_ALL    4005 // Define IDM_TAB_CLOSE_ALL as a menu command ID with value 4005.
#define IDM_TAB_SAVE_AS     4006 // Define IDM_TAB_SAVE_AS as a menu command ID with value 4006.
#define IDM_TAB_REVEAL      4007 // Define IDM_TAB_REVEAL as a menu command ID with value 4007.
#define WM_RENAME_COMMIT (WM_APP + 1) // Define WM_RENAME_COMMIT as a compile-time setting with value (WM_APP + 1).

// ============================================================
//  Enums
// ============================================================
enum ToolbarDock { // Define ToolbarDock to group the state or values used by this component.
    TBDOCK_TOP = 0, TBDOCK_BOTTOM, TBDOCK_LEFT, // Update tbdock top with the result of this expression.
    TBDOCK_RIGHT, TBDOCK_FLOAT // Continue the current declaration or expression.
}; // Finish the current type declaration.
enum ToolbarSize { TBSIZE_SMALL = 0, TBSIZE_MEDIUM, TBSIZE_LARGE }; // Define ToolbarSize to group the state or values used by this component.

// ============================================================
//  Default theme palette
// ============================================================
struct Palette { // Define Palette to group the state or values used by this component.
    COLORREF BgApp, BgPanel, BgCard, BgBtn, BgBtnHi, BgRailHi; // Complete the current operation before proceeding to the next statement.
    COLORREF Border, BorderSoft, TextHi, TextMid, TextLow; // Complete the current operation before proceeding to the next statement.
    COLORREF Accent, SelBg, IconIdle, IconActive; // Complete the current operation before proceeding to the next statement.
}; // Finish the current type declaration.

static Palette DefaultDarkTheme() { // Define DefaultDarkTheme as the function that implements this part of the panel.
    Palette t; // Declare t for use by this block.
    t.BgApp      = RGB(22, 22, 25); // Update the bg app field with the calculated value.
    t.BgPanel    = RGB(30, 30, 34); // Update the bg panel field with the calculated value.
    t.BgCard     = RGB(38, 38, 43); // Update the bg card field with the calculated value.
    t.BgBtn      = RGB(48, 48, 54); // Update the bg btn field with the calculated value.
    t.BgBtnHi    = RGB(60, 60, 66); // Update the bg btn hi field with the calculated value.
    t.BgRailHi   = RGB(52, 52, 58); // Update the bg rail hi field with the calculated value.
    t.Border     = RGB(56, 56, 62); // Update the border field with the calculated value.
    t.BorderSoft = RGB(44, 44, 48); // Update the border soft field with the calculated value.
    t.TextHi     = RGB(232, 232, 236); // Update the text hi field with the calculated value.
    t.TextMid    = RGB(160, 160, 168); // Update the text mid field with the calculated value.
    t.TextLow    = RGB(108, 108, 116); // Update the text low field with the calculated value.
    t.Accent     = RGB(96, 165, 250); // Update the accent field with the calculated value.
    t.SelBg      = RGB(48, 70, 100); // Update the sel bg field with the calculated value.
    t.IconIdle   = RGB(200, 200, 208); // Update the icon idle field with the calculated value.
    t.IconActive = RGB(255, 255, 255); // Update the icon active field with the calculated value.
    return t; // Return the computed t to the caller.
} // Close the current block.

// ============================================================
//  Document model (unchanged struct, but no longer global)
// ============================================================
struct Document { // Define Document to group the state or values used by this component.
    HWND         hEdit = nullptr; // Declare the Rich Edit control that displays document text and start with no handle assigned.
    std::wstring filePath; // Declare the backing file path for this document for use by this block.
    std::wstring titleOverride; // Declare a custom title displayed instead of the file name for use by this block.
    bool         modified = false; // Declare whether this document contains unsaved edits and start disabled.
    int          zoom = 100; // Declare the current text zoom percentage and start with 100.
    bool         wordWrap = true; // Declare whether the editor wraps long lines and start enabled.
    bool         readOnly = false; // Declare whether edits are disabled for this document and start disabled.

    std::wstring Title() const { // Define Title as the function that implements this part of the panel.
        std::wstring name; // Declare name for use by this block.
        if (!titleOverride.empty()) name = titleOverride; // Continue this branch only when a custom title displayed instead of the file name contains at least one item.
        else if (filePath.empty())  name = L"Untitled"; // Continue this branch only when file path.empty() is true.
        else { // Run this fallback branch when the previous condition did not match.
            size_t p = filePath.find_last_of(L"\\/"); // Declare p and start with filePath.find_last_of(L"\\/").
            name = (p == std::wstring::npos) ? filePath : filePath.substr(p + 1); // Complete the current operation before proceeding to the next statement.
        } // Close the current block.
        if (modified) name = L"\u25CF " + name; // Continue this branch only when whether this document contains unsaved edits is true.
        return name; // Return the computed name to the caller.
    } // Close the current block.
    std::wstring RenameInitial() const { // Define RenameInitial as the function that implements this part of the panel.
        if (!titleOverride.empty()) return titleOverride; // Continue this branch only when a custom title displayed instead of the file name contains at least one item.
        if (filePath.empty())       return L"Untitled"; // Continue this branch only when file path.empty() is true.
        fs::path p(filePath); // Complete the current operation before proceeding to the next statement.
        std::wstring stem = p.stem().wstring(); // Declare stem and start with p.stem().wstring().
        return stem.empty() ? p.filename().wstring() : stem; // Return the computed stem.empty() ? p.filename().wstring() : stem to the caller.
    } // Close the current block.
}; // Finish the current type declaration.

// ============================================================
//  Tab hit-test / menu / button types (unchanged)
// ============================================================
struct TabRect { // Define TabRect to group the state or values used by this component.
    RECT rc; // Declare the rectangle used to measure a window’s client area for use by this block.
    RECT rcClose; // Declare the tab close-button hit-test rectangle for use by this block.
    int  index = -1; // Declare the tab or item index and start with -1.
}; // Finish the current type declaration.

struct MenuItem { // Define MenuItem to group the state or values used by this component.
    int          id = 0; // Declare the command or control identifier and start with 0.
    std::wstring label; // Declare the visible text for this item for use by this block.
    std::wstring shortcut; // Declare the keyboard shortcut shown beside the item for use by this block.
    bool         separator = false; // Declare whether the menu entry is a divider and start disabled.
    bool         checked = false; // Declare whether the menu item is checked and start disabled.
    bool         disabled = false; // Declare whether the menu item cannot be selected and start disabled.
}; // Finish the current type declaration.

enum class Icon { // Define Icon to group the state or values used by this component.
    New, Open, Save, Undo, Redo, Find, Menu, ZoomIn, ZoomOut, Wrap, // Supply this parameter or list item to the enclosing call.
    Close, ChevronDown, Case, Replace, ReplaceAll, Prev, Next, Check, Plus, // Supply this parameter or list item to the enclosing call.
}; // Finish the current type declaration.

struct BtnStyle { // Define BtnStyle to group the state or values used by this component.
    Icon icon; // Declare icon for use by this block.
    bool chipBg; // Declare chip bg for use by this block.
    bool rail; // Declare rail for use by this block.
    bool active; // Declare whether the control is in its active visual state for use by this block.
    COLORREF fg; // Declare fg for use by this block.
}; // Finish the current type declaration.

// ============================================================
//  Instance context -- replaces all former globals
// ============================================================
struct NotepadInstance { // Define NotepadInstance to group the state or values used by this component.
    // Window handles
    HWND        hSelf = nullptr; // Declare this panel window handle and start with no handle assigned.
    HWND        hParent = nullptr; // Declare the parent or host window handle and start with no handle assigned.
    HINSTANCE   hInst = nullptr; // Declare the module handle used to create this panel’s controls and start with no handle assigned.
    int         instanceId = 0; // Declare the unique identifier for this panel instance and start with 0.

    // Documents
    std::vector<Document*> docs; // Declare the collection of open document tabs for use by this block.
    int                     activeDoc = -1; // Declare the index of the active document and start with -1.

    // Aliases to the active document
    HWND         hEdit = nullptr; // Declare the Rich Edit control that displays document text and start with no handle assigned.
    std::wstring currentFile; // Declare current file for use by this block.
    bool         modified = false; // Declare whether this document contains unsaved edits and start disabled.
    int          zoom = 100; // Declare the current text zoom percentage and start with 100.
    bool         wordWrap = true; // Declare whether the editor wraps long lines and start enabled.
    bool         readOnly = false; // Declare whether edits are disabled for this document and start disabled.

    // Search bar state
    bool         searchOpen = false; // Declare search open and start disabled.
    bool         replaceOpen = false; // Declare replace open and start disabled.
    bool         searchCase = false; // Declare search case and start disabled.
    HWND         hSearchFind = nullptr; // Declare the find-text input control and start with no handle assigned.
    HWND         hSearchRepl = nullptr; // Declare the replace-text input control and start with no handle assigned.
    HWND         hSearchNext = nullptr; // Declare h search next and start with no handle assigned.
    HWND         hSearchPrev = nullptr; // Declare h search prev and start with no handle assigned.
    HWND         hSearchClose = nullptr; // Declare h search close and start with no handle assigned.
    HWND         hSearchCase = nullptr; // Declare h search case and start with no handle assigned.
    HWND         hSearchReplOne = nullptr; // Declare h search repl one and start with no handle assigned.
    HWND         hSearchReplAll = nullptr; // Declare h search repl all and start with no handle assigned.

    // Toolbar state
    ToolbarDock  tbDock = TBDOCK_TOP; // Declare tb dock and start with TBDOCK_TOP.
    ToolbarSize  tbSize = TBSIZE_MEDIUM; // Declare tb size and start with TBSIZE_MEDIUM.
    bool         tbFloatHoriz = true; // Declare tb float horiz and start enabled.
    bool         tbVisible = true; // Keep the toolbar visible by default, including before its parent window is shown.
    HFONT        iconFont = nullptr; // Keep the Windows system icon font for every toolbar and search glyph.
    HWND         hToolbarHost = nullptr; // Declare the child window that hosts toolbar buttons and start with no handle assigned.
    HWND         hTbBtns[10] = {}; // Declare the toolbar button handles, indexed by command and start with {}.
    HWND         hotBtn = nullptr; // Declare hot btn and start with no handle assigned.

    // Tab bar state
    std::vector<TabRect> tabRects; // Declare tab rects for use by this block.
    RECT                 tabNewBtnRect{}; // Complete the current operation before proceeding to the next statement.
    int                  tabHot = -1; // Declare tab hot and start with -1.
    int                  tabCloseHot = -1; // Declare tab close hot and start with -1.
    bool                 trackingMouse = false; // Declare tracking mouse and start disabled.

    // Inline rename state
    HWND         hRenameEdit = nullptr; // Declare h rename edit and start with no handle assigned.
    int          renamingTab = -1; // Declare renaming tab and start with -1.

    // Layout rects
    RECT         tabBarRect{}; // Complete the current operation before proceeding to the next statement.
    RECT         searchRect{}; // Complete the current operation before proceeding to the next statement.
    RECT         statusRect{}; // Complete the current operation before proceeding to the next statement.
    RECT         editRect{}; // Complete the current operation before proceeding to the next statement.

    // Display state
    bool         showStatus = true; // Declare show status and start enabled.
    int          dpi = 96; // Declare the current display density used for scaling and start with 96.

    // Fonts
    HFONT        font = nullptr; // Declare the normal editor font and start with no handle assigned.
    HFONT        fontBold = nullptr; // Declare the semibold interface font and start with no handle assigned.
    HFONT        fontSmall = nullptr; // Declare the small interface font and start with no handle assigned.
    LOGFONTW     logFont{}; // Complete the current operation before proceeding to the next statement.

    // Theme
    Palette      theme; // Declare the active editor color palette for use by this block.

    // Callback
    NPNOTIFYPROC notifyProc = nullptr; // Declare the host callback used for panel notifications and start with no handle assigned.
    void*        notifyUserData = nullptr; // Declare the host value passed back with notifications and start with no handle assigned.

    // Suppressed shortcuts
    std::unordered_set<int> suppressedCmds; // Declare suppressed cmds for use by this block.

    // Menu state (transient, per ShowThemedMenu call)
    bool                   menuDone = false; // Declare whether the custom menu has finished tracking and start disabled.
    int                    menuResult = 0; // Declare the selected command returned by the custom menu and start with 0.
    HWND                   menuWnd = nullptr; // Declare the popup window used to draw the custom menu and start with no handle assigned.
    std::vector<MenuItem>* menuItems = nullptr; // Declare the items currently displayed in the custom menu and start with no handle assigned.
    int                    menuHot = -1; // Declare the menu item currently under the pointer and start with -1.
    int                    menuItemH = 0; // Declare menu item h and start with 0.
    int                    menuPadY = 0; // Declare menu pad y and start with 0.

    // Child mode (WS_CHILD)
    bool         isChild = false; // Declare is child and start disabled.

    // Host-managed settings (skip registry)
    bool         hostManaged = false; // Declare host managed and start disabled.

    // Static brush for WM_CTLCOLOR* (per-instance to match theme)
    HBRUSH       brSearchBg = nullptr; // Declare br search bg and start with no handle assigned.
    HBRUSH       brAppBg = nullptr; // Declare br app bg and start with no handle assigned.

    // Instance ID counter
    static int   s_nextId; // Declare s next id for use by this block.
}; // Finish the current type declaration.

int NotepadInstance::s_nextId = 1; // Update notepad instance::s next id with the result of this expression.

// ============================================================
//  Process-global state (shared across all panel instances)
// ============================================================
static HINSTANCE g_hInst = nullptr; // Declare the application module handle used to register and create windows and start with no handle assigned.
static bool     g_classesRegistered = false; // Declare classes registered and start disabled.
static HMODULE  g_msftedit = nullptr; // Declare the loaded Rich Edit module required by the editor and start with no handle assigned.

// ============================================================
//  Instance resolution
// ============================================================
static NotepadInstance* GetInstance(HWND hWnd) { // Define GetInstance as the function that implements this part of the panel.
    if (!hWnd) return nullptr; // Continue this branch only when the window receiving this message is unavailable.
    return reinterpret_cast<NotepadInstance*>( // Return the computed reinterpret cast<notepad instance*>( to the caller.
        GetWindowLongPtrW(hWnd, GWLP_USERDATA)); // Call GetWindowLongPtrW to read style or instance data stored on a window.
} // Close the current block.

static inline int Dpi(NotepadInstance* inst, int v) { // Define Dpi as the function that implements this part of the panel.
    return MulDiv(v, inst->dpi, 96); // Return the computed mul div(v, inst->dpi, 96) to the caller.
} // Close the current block.

// ============================================================
//  String helpers
// ============================================================
static std::wstring Widen(const std::string& s) { // Define Widen as the function that implements this part of the panel.
    if (s.empty()) return L""; // Continue this branch only when s.empty() is true.
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0); // Declare n and start with MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0).
    std::wstring w(n - 1, 0); // Complete the current operation before proceeding to the next statement.
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n); // Call MultiByteToWideChar to convert UTF-8 text into Windows UTF-16 text.
    return w; // Return the computed w to the caller.
} // Close the current block.
static std::string ToUtf8(const std::wstring& w) { // Define ToUtf8 as the function that implements this part of the panel.
    if (w.empty()) return ""; // Continue this branch only when w.empty() is true.
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr); // Declare n and start with WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr).
    std::string s(n - 1, 0); // Complete the current operation before proceeding to the next statement.
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr); // Call WideCharToMultiByte to convert Windows UTF-16 text into UTF-8.
    return s; // Return the computed s to the caller.
} // Close the current block.
static std::wstring BaseNameW(const std::wstring& path) { // Define BaseNameW as the function that implements this part of the panel.
    size_t p = path.find_last_of(L"\\/"); // Declare p and start with path.find_last_of(L"\\/").
    return (p == std::wstring::npos) ? path : path.substr(p + 1); // Return the computed (p == std::wstring::npos) ? path : path.substr(p + 1) to the caller.
} // Close the current block.

// ============================================================
//  Registry helpers (only used in standalone mode)
// ============================================================
static std::wstring RegReadStr(const wchar_t* name, const std::wstring& def) { // Define RegReadStr as the function that implements this part of the panel.
    HKEY k; // Declare k for use by this block.
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &k) != ERROR_SUCCESS) // Continue this branch only when reg open key ex w(hkey current user, reg key, 0, key read, &k) is different from ERROR_SUCCESS.
        return def; // Return the computed def to the caller.
    wchar_t buf[512]; // Declare buf for use by this block.
    DWORD sz = sizeof(buf); // Declare sz and start with sizeof(buf).
    DWORD type = REG_SZ; // Declare type and start with REG_SZ.
    std::wstring out = def; // Declare out and start with def.
    if (RegQueryValueExW(k, name, nullptr, &type, (LPBYTE)buf, &sz) == ERROR_SUCCESS) { // Continue this branch only when reg query value ex w(k, name, nullptr, &type, (lpbyte)buf, &sz) is equal to ERROR_SUCCESS.
        out.assign(buf, sz / sizeof(wchar_t) - 1); // Complete the current operation before proceeding to the next statement.
    } // Close the current block.
    RegCloseKey(k); // Call RegCloseKey to close the registry key after use.
    return out; // Return the computed out to the caller.
} // Close the current block.
static void RegWriteStr(const wchar_t* name, const std::wstring& val) { // Define RegWriteStr as the function that implements this part of the panel.
    HKEY k; // Declare k for use by this block.
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0, // Continue this branch only when reg create key ex w(hkey current user, reg key, 0, nullptr, 0, is true.
        KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return; // Complete the current operation before proceeding to the next statement.
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val.c_str(), // Call RegSetValueExW to save an application setting.
        (DWORD)((val.size() + 1) * sizeof(wchar_t))); // Complete the current operation before proceeding to the next statement.
    RegCloseKey(k); // Call RegCloseKey to close the registry key after use.
} // Close the current block.
static int RegReadInt(const wchar_t* name, int def) { // Define RegReadInt as the function that implements this part of the panel.
    try { return std::stoi(RegReadStr(name, std::to_wstring(def))); } // Begin a block whose failures are handled by the following catch clause.
    catch (...) { return def; } // Handle an exception raised by the preceding try block.
} // Close the current block.

// ============================================================
//  Notification helper
// ============================================================
static void FireNotify(NotepadInstance* inst, UINT code, int tabIndex) { // Define FireNotify as the function that implements this part of the panel.
    if (!inst) return; // Continue this branch only when the state object for this Notepad panel is unavailable.
    if (inst->notifyProc) { // Continue this branch only when inst- is greater than notifyProc.
        NPNotify n = {}; // Declare n and start with {}.
        n.hPanel     = inst->hSelf; // Update the the editor panel handle passed to the API field with the calculated value.
        n.instanceId = inst->instanceId; // Update the the unique identifier for this panel instance field with the calculated value.
        n.code       = code; // Update the code field with the calculated value.
        n.tabIndex   = tabIndex; // Update the tab index field with the calculated value.
        n.userData   = inst->notifyUserData; // Update the the opaque value supplied by the host application field with the calculated value.
        inst->notifyProc(&n); // Complete the current operation before proceeding to the next statement.
    } else if (inst->hParent) { // Define if as the function that implements this part of the panel.
        NMHDR nmh = {}; // Declare nmh and start with {}.
        nmh.hwndFrom = inst->hSelf; // Update the hwnd from field with the calculated value.
        nmh.idFrom   = (ULONG_PTR)inst->instanceId; // Update the id from field with the calculated value.
        nmh.code     = code; // Update the code field with the calculated value.
        SendMessageW(inst->hParent, WM_NOTIFY, (WPARAM)nmh.idFrom, (LPARAM)&nmh); // Call SendMessageW to send a control message and receive its result immediately.
    } // Close the current block.
} // Close the current block.

// ============================================================
//  GDI helpers (theme-aware via instance)
// ============================================================
static void FillSolid(HDC hdc, const RECT& rc, COLORREF c) { // Define FillSolid as the function that implements this part of the panel.
    HBRUSH br = CreateSolidBrush(c); // Declare br and start with CreateSolidBrush(c).
    FillRect(hdc, &rc, br); // Call FillRect to fill the requested rectangle with a brush.
    DeleteObject(br); // Call DeleteObject to release the GDI font, brush, or bitmap.
} // Close the current block.
static void FillRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF fill) { // Define FillRoundRect as the function that implements this part of the panel.
    HBRUSH br = CreateSolidBrush(fill); // Declare br and start with CreateSolidBrush(fill).
    HPEN pen = CreatePen(PS_SOLID, 1, fill); // Declare pen and start with CreatePen(PS_SOLID, 1, fill).
    HGDIOBJ ob = SelectObject(hdc, br); // Declare ob and start with SelectObject(hdc, br).
    HGDIOBJ op = SelectObject(hdc, pen); // Declare op and start with SelectObject(hdc, pen).
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2); // Call RoundRect to perform this part of the Notepad operation.
    SelectObject(hdc, ob); // Call SelectObject to select a GDI object into the drawing context.
    SelectObject(hdc, op); // Call SelectObject to select a GDI object into the drawing context.
    DeleteObject(br); // Call DeleteObject to release the GDI font, brush, or bitmap.
    DeleteObject(pen); // Call DeleteObject to release the GDI font, brush, or bitmap.
} // Close the current block.
static void StrokeRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF stroke) { // Define StrokeRoundRect as the function that implements this part of the panel.
    HBRUSH br = (HBRUSH)GetStockObject(NULL_BRUSH); // Declare br and start with (HBRUSH)GetStockObject(NULL_BRUSH).
    HPEN pen = CreatePen(PS_SOLID, 1, stroke); // Declare pen and start with CreatePen(PS_SOLID, 1, stroke).
    HGDIOBJ ob = SelectObject(hdc, br); // Declare ob and start with SelectObject(hdc, br).
    HGDIOBJ op = SelectObject(hdc, pen); // Declare op and start with SelectObject(hdc, pen).
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2); // Call RoundRect to perform this part of the Notepad operation.
    SelectObject(hdc, ob); // Call SelectObject to select a GDI object into the drawing context.
    SelectObject(hdc, op); // Call SelectObject to select a GDI object into the drawing context.
    DeleteObject(pen); // Call DeleteObject to release the GDI font, brush, or bitmap.
} // Close the current block.
static void Circle(HDC hdc, int cx, int cy, int r, COLORREF fill) { // Define Circle as the function that implements this part of the panel.
    HBRUSH br = CreateSolidBrush(fill); // Declare br and start with CreateSolidBrush(fill).
    HGDIOBJ ob = SelectObject(hdc, br); // Declare ob and start with SelectObject(hdc, br).
    HGDIOBJ op = SelectObject(hdc, GetStockObject(NULL_PEN)); // Declare op and start with SelectObject(hdc, GetStockObject(NULL_PEN)).
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r); // Call Ellipse to perform this part of the Notepad operation.
    SelectObject(hdc, ob); // Call SelectObject to select a GDI object into the drawing context.
    SelectObject(hdc, op); // Call SelectObject to select a GDI object into the drawing context.
    DeleteObject(br); // Call DeleteObject to release the GDI font, brush, or bitmap.
} // Close the current block.

// ============================================================
//  Icon drawing (unchanged logic, uses instance dpi)
// ============================================================
static void DrawIcon(HDC hdc, NotepadInstance* inst, Icon icon, // Draw a named Windows Fluent icon glyph.
                     const RECT& r, COLORREF color, int) { // Keep the existing call signature while no longer drawing custom vectors.
    static constexpr wchar_t glyphs[] = { // Use Microsoft Segoe Fluent Icons glyphs for every icon in the application.
        L'\uEA1A', L'\uE8E5', L'\uE74E', L'\uE7A7', L'\uE7A6', L'\uE721', L'\uE700', L'\uE8A3', L'\uE71F', L'\uE8E4', // Document, open, save, edit, menu, zoom, and wrap glyphs.
        L'\uE711', L'\uE70D', L'\uE8D2', L'\uE8AB', L'\uE8EE', L'\uE892', L'\uE893', L'\uE73E', L'\uE710' // Close, case, replace, navigation, check, and add glyphs.
    }; // Finish the complete system glyph map.
    const size_t glyphIndex = static_cast<size_t>(icon); // Convert the contiguous icon enum into its matching glyph index.
    if (!inst || !inst->iconFont || glyphIndex >= (sizeof(glyphs) / sizeof(glyphs[0]))) return; // Skip drawing if the font or icon mapping is unavailable.
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, inst->iconFont)); // Select the Windows icon font into the paint context.
    SetBkMode(hdc, TRANSPARENT); // Keep the icon background transparent.
    SetTextColor(hdc, color); // Match the icon color to the current control state.
    wchar_t glyphText[2] = { glyphs[glyphIndex], L'\0' }; // Make a null-terminated string for DrawTextW.
    RECT glyphRect = r; // Copy the drawing bounds before adjusting the glyph alignment.
    DrawTextW(hdc, glyphText, 1, &glyphRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX); // Draw the official Windows Fluent glyph.
    SelectObject(hdc, oldFont); // Restore the paint context's previous font.
} // Finish drawing a standard Windows icon.

// ============================================================
//  Forward declarations
// ============================================================
LRESULT CALLBACK PanelWndProc(HWND, UINT, WPARAM, LPARAM); // Complete the current operation before proceeding to the next statement.
LRESULT CALLBACK ToolbarHostProc(HWND, UINT, WPARAM, LPARAM); // Complete the current operation before proceeding to the next statement.
LRESULT CALLBACK MenuProc(HWND, UINT, WPARAM, LPARAM); // Complete the current operation before proceeding to the next statement.
LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR); // Complete the current operation before proceeding to the next statement.
LRESULT CALLBACK RenameEditProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR); // Complete the current operation before proceeding to the next statement.
LRESULT CALLBACK ToolbarBtnProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR); // Complete the current operation before proceeding to the next statement.

void UpdateLayout(NotepadInstance* inst, HWND hWnd); // Complete the current operation before proceeding to the next statement.
void SetToolbarDock(NotepadInstance* inst, ToolbarDock dock); // Complete the current operation before proceeding to the next statement.
void SetToolbarSize(NotepadInstance* inst, ToolbarSize size); // Complete the current operation before proceeding to the next statement.
void DoSave(NotepadInstance* inst, bool saveAs); // Complete the current operation before proceeding to the next statement.
void ApplyEditorFont(NotepadInstance* inst); // Complete the current operation before proceeding to the next statement.
void ApplyWordWrap(NotepadInstance* inst); // Complete the current operation before proceeding to the next statement.
void ApplyZoom(NotepadInstance* inst); // Complete the current operation before proceeding to the next statement.
void ShowSearch(NotepadInstance* inst, bool open, bool withReplace); // Complete the current operation before proceeding to the next statement.
void ShowMainMenu(NotepadInstance* inst, HWND hWnd); // Complete the current operation before proceeding to the next statement.
void BeginRenameTab(NotepadInstance* inst, int index); // Complete the current operation before proceeding to the next statement.
void CommitRename(NotepadInstance* inst, bool accept); // Complete the current operation before proceeding to the next statement.
void DispatchCommand(NotepadInstance* inst, HWND hWnd, int cmd); // Complete the current operation before proceeding to the next statement.
bool CloseDoc(NotepadInstance* inst, int index); // Complete the current operation before proceeding to the next statement.
Document* CreateDoc(NotepadInstance* inst, const std::wstring& initialText, // Supply this parameter or list item to the enclosing call.
                    const std::wstring& filePath); // Complete the current operation before proceeding to the next statement.
void SetActiveDoc(NotepadInstance* inst, int index); // Complete the current operation before proceeding to the next statement.
void SetModified(NotepadInstance* inst, bool modified); // Complete the current operation before proceeding to the next statement.
void ShowTabMenu(NotepadInstance* inst, int index, int x, int y); // Complete the current operation before proceeding to the next statement.
void ShowToolbarMenu(NotepadInstance* inst, HWND owner, int x, int y); // Complete the current operation before proceeding to the next statement.

// ============================================================
//  Document management
// ============================================================
static Document* Active(NotepadInstance* inst) { // Define Active as the function that implements this part of the panel.
    if (inst->activeDoc < 0 || inst->activeDoc >= (int)inst->docs.size()) return nullptr; // Continue this branch only when inst- is greater than activeDoc < 0 or inst- is greater than activeDoc >= (int)inst->docs.size().
    return inst->docs[inst->activeDoc]; // Return the computed inst->docs[inst->active doc] to the caller.
} // Close the current block.
static void SyncFromActive(NotepadInstance* inst) { // Define SyncFromActive as the function that implements this part of the panel.
    Document* d = Active(inst); if (!d) return; // Declare d and start with Active(inst); if (!d) return.
    inst->hEdit = d->hEdit; // Store the new value in the the Rich Edit control that displays document text field of the current instance.
    inst->currentFile = d->filePath; // Store the new value in the current file field of the current instance.
    inst->modified = d->modified; // Store the new value in the whether this document contains unsaved edits field of the current instance.
    inst->zoom = d->zoom; // Store the new value in the the current text zoom percentage field of the current instance.
    inst->wordWrap = d->wordWrap; // Store the new value in the whether the editor wraps long lines field of the current instance.
    inst->readOnly = d->readOnly; // Store the new value in the whether edits are disabled for this document field of the current instance.
} // Close the current block.
static void SyncToActive(NotepadInstance* inst) { // Define SyncToActive as the function that implements this part of the panel.
    Document* d = Active(inst); if (!d) return; // Declare d and start with Active(inst); if (!d) return.
    d->filePath = inst->currentFile; // Store the new value in the the backing file path for this document field of the current instance.
    d->modified = inst->modified; // Store the new value in the whether this document contains unsaved edits field of the current instance.
    d->zoom = inst->zoom; // Store the new value in the the current text zoom percentage field of the current instance.
    d->wordWrap = inst->wordWrap; // Store the new value in the whether the editor wraps long lines field of the current instance.
    d->readOnly = inst->readOnly; // Store the new value in the whether edits are disabled for this document field of the current instance.
} // Close the current block.

void SetActiveDoc(NotepadInstance* inst, int index) { // Define SetActiveDoc as the function that implements this part of the panel.
    if (index < 0 || index >= (int)inst->docs.size()) return; // Continue this branch only when the tab or item index is less than 0 or the tab or item index is at least (int)inst->docs.size().
    if (index == inst->activeDoc) return; // Continue this branch only when the tab or item index is equal to inst->activeDoc.
    if (inst->activeDoc >= 0 && inst->activeDoc < (int)inst->docs.size()) { // Continue this branch only when inst- is greater than activeDoc >= 0 and inst- is greater than activeDoc < (int)inst->docs.size().
        SyncToActive(inst); // Call SyncToActive to perform this part of the Notepad operation.
        Document* old = inst->docs[inst->activeDoc]; // Declare old and start with inst->docs[inst->activeDoc].
        if (old->hEdit) ShowWindow(old->hEdit, SW_HIDE); // Continue this branch only when old- is greater than hEdit.
    } // Close the current block.
    inst->activeDoc = index; // Store the new value in the the index of the active document field of the current instance.
    SyncFromActive(inst); // Call SyncFromActive to perform this part of the Notepad operation.
    Document* doc = Active(inst); // Declare the document currently being operated on and start with Active(inst).
    if (doc && doc->hEdit) ShowWindow(doc->hEdit, SW_SHOW); // Continue this branch only when the document currently being operated on is true and doc- is greater than hEdit.
    ApplyWordWrap(inst); ApplyZoom(inst); // Call ApplyWordWrap to apply the requested state or value.
    if (!inst->isChild && inst->hSelf) { // Continue this branch only when inst->is child && inst->h self evaluates to false.
        std::wstring title; // Declare title for use by this block.
        if (inst->currentFile.empty()) title = L"Untitled"; // Continue this branch only when inst- is greater than currentFile.empty().
        else title = BaseNameW(inst->currentFile); // Run this fallback branch when the previous condition did not match.
        if (inst->modified) title = L"\u25CF " + title; // Continue this branch only when inst- is greater than modified.
        title += L"  \u2014  " + std::wstring(APP_NAME); // Update + with the result of this expression.
        SetWindowTextW(inst->hSelf, title.c_str()); // Call SetWindowTextW to change the text displayed by a window or control.
    } // Close the current block.
    if (inst->hSelf) { UpdateLayout(inst, inst->hSelf); InvalidateRect(inst->hSelf, nullptr, TRUE); } // Continue this branch only when inst- is greater than hSelf.
    if (inst->hEdit) SetFocus(inst->hEdit); // Continue this branch only when inst- is greater than hEdit.
    FireNotify(inst, NP_NOTIFY_TAB_CHANGED, index); // Call FireNotify to perform this part of the Notepad operation.
} // Close the current block.

Document* CreateDoc(NotepadInstance* inst, const std::wstring& initialText, // Supply this parameter or list item to the enclosing call.
                    const std::wstring& filePath) { // Continue the current declaration or expression.
    Document* doc = new Document(); // Declare the document currently being operated on and start with new Document().
    doc->filePath = filePath; // Store the new value in the the backing file path for this document field of the current instance.
    doc->zoom = inst->zoom; // Store the new value in the the current text zoom percentage field of the current instance.
    doc->wordWrap = inst->wordWrap; // Store the new value in the whether the editor wraps long lines field of the current instance.
    doc->readOnly = inst->readOnly; // Store the new value in the whether edits are disabled for this document field of the current instance.
    HWND hParent = inst->hSelf; // Declare the parent or host window handle and start with inst->hSelf.
    doc->hEdit = CreateWindowExW(0, L"RICHEDIT50W", L"", // Store the new value in the the Rich Edit control that displays document text field of the current instance.
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | // Continue the current declaration or expression.
        ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL, // Supply this parameter or list item to the enclosing call.
        0, 0, 100, 100, hParent, nullptr, inst->hInst, nullptr); // Complete the current operation before proceeding to the next statement.
    SendMessageW(doc->hEdit, EM_SETBKGNDCOLOR, 0, inst->theme.BgApp); // Call SendMessageW to send a control message and receive its result immediately.
    SendMessageW(doc->hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, // Call SendMessageW to send a control message and receive its result immediately.
        MAKELPARAM(Dpi(inst, 16), Dpi(inst, 16))); // Call MAKELPARAM to perform this part of the Notepad operation.
    SendMessageW(doc->hEdit, EM_SETUNDOLIMIT, 500, 0); // Call SendMessageW to send a control message and receive its result immediately.
    SetWindowTheme(doc->hEdit, L"DarkMode_Explorer", nullptr); // Call SetWindowTheme to apply the selected control theme.
    SetWindowSubclass(doc->hEdit, EditProc, 1, (DWORD_PTR)inst); // Call SetWindowSubclass to install a callback that customizes a native control.
    if (inst->readOnly) SendMessageW(doc->hEdit, EM_SETREADONLY, TRUE, 0); // Continue this branch only when inst- is greater than readOnly.
    if (!initialText.empty()) { // Continue this branch only when initial text contains at least one item.
        SETTEXTEX st{}; st.flags = ST_DEFAULT; st.codepage = 1200; // Update the flags field with the calculated value.
        std::wstring normalized; // Declare normalized for use by this block.
        normalized.reserve(initialText.size() + 16); // Complete the current operation before proceeding to the next statement.
        for (wchar_t c : initialText) { // Visit each item in the collection and apply the loop body.
            if (c == L'\r') continue; // Continue this branch only when c is equal to L'\r'.
            if (c == L'\n') normalized += L"\r\n"; // Normalize a newline to the Rich Edit CRLF format.
            else normalized += c; // Run this fallback branch when the previous condition did not match.
        } // Close the current block.
        SendMessageW(doc->hEdit, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)normalized.c_str()); // Call SendMessageW to send a control message and receive its result immediately.
    } // Close the current block.
    SendMessageW(doc->hEdit, EM_SETMODIFY, FALSE, 0); // Call SendMessageW to send a control message and receive its result immediately.
    doc->modified = false; // Store the new value in the whether this document contains unsaved edits field of the current instance.
    CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf); // Update the cb size field with the calculated value.
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR; // Update the dw mask field with the calculated value.
    cf.crTextColor = inst->theme.TextHi; // Update the cr text color field with the calculated value.
    cf.yHeight = (LONG)(MulDiv(inst->logFont.lfHeight, 1440, inst->dpi)); // Update the y height field with the calculated value.
    if (cf.yHeight < 0) cf.yHeight = -cf.yHeight; // Continue this branch only when cf.y height is less than 0.
    wcscpy_s(cf.szFaceName, inst->logFont.lfFaceName); // Call wcscpy_s to perform this part of the Notepad operation.
    SendMessageW(doc->hEdit, EM_SETCHARFORMAT, SCF_DEFAULT | SCF_ALL, (LPARAM)&cf); // Call SendMessageW to send a control message and receive its result immediately.
    if (doc->wordWrap) SendMessageW(doc->hEdit, EM_SETTARGETDEVICE, 0, 0); // Continue this branch only when doc- is greater than wordWrap.
    else              SendMessageW(doc->hEdit, EM_SETTARGETDEVICE, 0, 1); // Run this fallback branch when the previous condition did not match.
    SendMessageW(doc->hEdit, EM_SETZOOM, (WPARAM)doc->zoom, (WPARAM)100); // Call SendMessageW to send a control message and receive its result immediately.
    inst->docs.push_back(doc); // Complete the current operation before proceeding to the next statement.
    return doc; // Return the computed the document currently being operated on to the caller.
} // Close the current block.

static bool ConfirmSaveDoc(NotepadInstance* inst, Document* doc) { // Define ConfirmSaveDoc as the function that implements this part of the panel.
    if (!doc || !doc->modified) return true; // Continue this branch only when doc || !doc->modified evaluates to false.
    std::wstring name = doc->filePath.empty() // Declare name and start with doc->filePath.empty().
        ? (doc->titleOverride.empty() ? L"Untitled" : doc->titleOverride) // Complete the current call or parameter list.
        : BaseNameW(doc->filePath); // Complete the current operation before proceeding to the next statement.
    std::wstring msg = L"Save changes to " + name + L"?"; // Declare the Win32 message currently being dispatched and start with L"Save changes to " + name + L"?".
    HWND hOwner = inst->isChild ? inst->hParent : inst->hSelf; // Declare h owner and start with inst->isChild ? inst->hParent : inst->hSelf.
    int r = MessageBoxW(hOwner, msg.c_str(), APP_NAME, MB_YESNOCANCEL | MB_ICONQUESTION); // Declare r and start with MessageBoxW(hOwner, msg.c_str(), APP_NAME, MB_YESNOCANCEL | MB_ICONQUESTION).
    if (r == IDCANCEL) return false; // Continue this branch only when r is equal to IDCANCEL.
    if (r == IDNO) return true; // Continue this branch only when r is equal to IDNO.
    DoSave(inst, false); // Call DoSave to perform this part of the Notepad operation.
    return !inst->modified; // Return the computed !inst->modified to the caller.
} // Close the current block.

bool CloseDoc(NotepadInstance* inst, int index) { // Define CloseDoc as the function that implements this part of the panel.
    if (index < 0 || index >= (int)inst->docs.size()) return false; // Continue this branch only when the tab or item index is less than 0 or the tab or item index is at least (int)inst->docs.size().
    if (inst->hRenameEdit && inst->renamingTab == index) CommitRename(inst, true); // Continue this branch only when inst- is greater than hRenameEdit and inst- is greater than renamingTab == index.
    Document* doc = inst->docs[index]; // Declare the document currently being operated on and start with inst->docs[index].
    bool wasActive = (index == inst->activeDoc); // Complete the current operation before proceeding to the next statement.
    if (!wasActive) SetActiveDoc(inst, index); // Continue this branch only when was active evaluates to false.
    if (doc->modified) { if (!ConfirmSaveDoc(inst, doc)) return false; } // Continue this branch only when doc- is greater than modified.
    HWND h = doc->hEdit; doc->hEdit = nullptr; // Declare h and start with doc->hEdit; doc->hEdit = nullptr.
    if (h) DestroyWindow(h); // Continue this branch only when h is true.
    inst->docs.erase(inst->docs.begin() + index); // Complete the current operation before proceeding to the next statement.
    delete doc; // Free the object allocated earlier after it is no longer needed.
    FireNotify(inst, NP_NOTIFY_TAB_CLOSED, index); // Call FireNotify to perform this part of the Notepad operation.
    if (inst->docs.empty()) { // Continue this branch only when inst- is greater than docs.empty().
        inst->activeDoc = -1; inst->hEdit = nullptr; // Store the new value in the the index of the active document field of the current instance.
        CreateDoc(inst, L"", L""); // Call CreateDoc to create the requested interface object or resource.
        SetActiveDoc(inst, 0); // Call SetActiveDoc to apply the requested state or value.
        return true; // Return the computed true to the caller.
    } // Close the current block.
    int newActive; // Declare new active for use by this block.
    if (inst->activeDoc == index) newActive = (index < (int)inst->docs.size()) ? index : (int)inst->docs.size() - 1; // Continue this branch only when inst- is greater than activeDoc == index.
    else if (inst->activeDoc > index) newActive = inst->activeDoc - 1; // Continue this branch only when inst- is greater than activeDoc > index.
    else newActive = inst->activeDoc; // Run this fallback branch when the previous condition did not match.
    inst->activeDoc = -1; // Store the new value in the the index of the active document field of the current instance.
    SetActiveDoc(inst, newActive); // Call SetActiveDoc to apply the requested state or value.
    return true; // Return the computed true to the caller.
} // Close the current block.

static bool CloseAllDocs(NotepadInstance* inst) { // Define CloseAllDocs as the function that implements this part of the panel.
    while (!inst->docs.empty()) { if (!CloseDoc(inst, 0)) return false; } // Repeat the body while inst->docs contains at least one item.
    return true; // Return the computed true to the caller.
} // Close the current block.

// ============================================================
//  Editor helpers
// ============================================================
static int GetCaretLine(NotepadInstance* inst) { // Define GetCaretLine as the function that implements this part of the panel.
    if (!inst->hEdit) return 1; // Continue this branch only when inst->h edit evaluates to false.
    DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0); // Declare sel and start with (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0).
    return (int)SendMessageW(inst->hEdit, EM_LINEFROMCHAR, LOWORD(sel), 0) + 1; // Return the computed (int)send message w(inst->h edit, em linefromchar, loword(sel), 0) + 1 to the caller.
} // Close the current block.
static int GetCaretCol(NotepadInstance* inst) { // Define GetCaretCol as the function that implements this part of the panel.
    if (!inst->hEdit) return 1; // Continue this branch only when inst->h edit evaluates to false.
    DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0); // Declare sel and start with (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0).
    int pos = LOWORD(sel); // Declare pos and start with LOWORD(sel).
    int line = (int)SendMessageW(inst->hEdit, EM_LINEFROMCHAR, pos, 0); // Declare line and start with (int)SendMessageW(inst->hEdit, EM_LINEFROMCHAR, pos, 0).
    return pos - (int)SendMessageW(inst->hEdit, EM_LINEINDEX, line, 0) + 1; // Return the computed pos - (int)send message w(inst->h edit, em lineindex, line, 0) + 1 to the caller.
} // Close the current block.
static int GetLineCount(NotepadInstance* inst) { // Define GetLineCount as the function that implements this part of the panel.
    if (!inst->hEdit) return 1; // Continue this branch only when inst->h edit evaluates to false.
    int n = (int)SendMessageW(inst->hEdit, EM_GETLINECOUNT, 0, 0); // Declare n and start with (int)SendMessageW(inst->hEdit, EM_GETLINECOUNT, 0, 0).
    return n < 1 ? 1 : n; // Return the computed n < 1 ? 1 : n to the caller.
} // Close the current block.
static int GetCharCount(NotepadInstance* inst) { // Define GetCharCount as the function that implements this part of the panel.
    if (!inst->hEdit) return 0; // Continue this branch only when inst->h edit evaluates to false.
    GETTEXTLENGTHEX gt{}; gt.flags = GTL_NUMCHARS | GTL_PRECISE; gt.codepage = 1200; // Update the flags field with the calculated value.
    LRESULT n = SendMessageW(inst->hEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gt, 0); // Declare n and start with SendMessageW(inst->hEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gt, 0).
    return n < 0 ? 0 : (int)n; // Return the computed n < 0 ? 0 : (int)n to the caller.
} // Close the current block.
void ApplyZoom(NotepadInstance* inst) { // Define ApplyZoom as the function that implements this part of the panel.
    if (!inst->hEdit) return; // Continue this branch only when inst->h edit evaluates to false.
    SendMessageW(inst->hEdit, EM_SETZOOM, (WPARAM)inst->zoom, (WPARAM)100); // Call SendMessageW to send a control message and receive its result immediately.
} // Close the current block.
void ApplyWordWrap(NotepadInstance* inst) { // Define ApplyWordWrap as the function that implements this part of the panel.
    if (!inst->hEdit) return; // Continue this branch only when inst->h edit evaluates to false.
    if (inst->wordWrap) SendMessageW(inst->hEdit, EM_SETTARGETDEVICE, 0, 0); // Continue this branch only when inst- is greater than wordWrap.
    else               SendMessageW(inst->hEdit, EM_SETTARGETDEVICE, 0, 1); // Run this fallback branch when the previous condition did not match.
} // Close the current block.
void ApplyEditorFont(NotepadInstance* inst) { // Define ApplyEditorFont as the function that implements this part of the panel.
    CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf); // Update the cb size field with the calculated value.
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR; // Update the dw mask field with the calculated value.
    cf.crTextColor = inst->theme.TextHi; // Update the cr text color field with the calculated value.
    cf.yHeight = (LONG)(MulDiv(inst->logFont.lfHeight, 1440, inst->dpi)); // Update the y height field with the calculated value.
    if (cf.yHeight < 0) cf.yHeight = -cf.yHeight; // Continue this branch only when cf.y height is less than 0.
    wcscpy_s(cf.szFaceName, inst->logFont.lfFaceName); // Call wcscpy_s to perform this part of the Notepad operation.
    for (Document* d : inst->docs) // Visit each item in the collection and apply the loop body.
        if (d->hEdit) // Continue this branch only when d- is greater than hEdit.
            SendMessageW(d->hEdit, EM_SETCHARFORMAT, SCF_DEFAULT | SCF_ALL, (LPARAM)&cf); // Call SendMessageW to send a control message and receive its result immediately.
} // Close the current block.
static void UpdateTitle(NotepadInstance* inst) { // Define UpdateTitle as the function that implements this part of the panel.
    if (inst->isChild || !inst->hSelf) return; // Continue this branch only when inst- is greater than isChild or inst->h self evaluates to false.
    std::wstring title; // Declare title for use by this block.
    if (inst->currentFile.empty()) title = L"Untitled"; // Continue this branch only when inst- is greater than currentFile.empty().
    else title = BaseNameW(inst->currentFile); // Run this fallback branch when the previous condition did not match.
    if (inst->modified) title = L"\u25CF " + title; // Continue this branch only when inst- is greater than modified.
    title += L"  \u2014  " + std::wstring(APP_NAME); // Update + with the result of this expression.
    SetWindowTextW(inst->hSelf, title.c_str()); // Call SetWindowTextW to change the text displayed by a window or control.
} // Close the current block.
void SetModified(NotepadInstance* inst, bool modified) { // Define SetModified as the function that implements this part of the panel.
    bool prev = inst->modified; // Declare prev and start with inst->modified.
    inst->modified = modified; // Store the new value in the whether this document contains unsaved edits field of the current instance.
    Document* d = Active(inst); if (d) d->modified = modified; // Declare d and start with Active(inst); if (d) d->modified = modified.
    if (inst->hEdit) SendMessageW(inst->hEdit, EM_SETMODIFY, modified ? TRUE : FALSE, 0); // Continue this branch only when inst- is greater than hEdit.
    UpdateTitle(inst); // Call UpdateTitle to apply the requested state or value.
    if (inst->hSelf) InvalidateRect(inst->hSelf, &inst->tabBarRect, FALSE); // Continue this branch only when inst- is greater than hSelf.
    if (prev != modified) FireNotify(inst, NP_NOTIFY_MODIFIED_CHANGED, inst->activeDoc); // Continue this branch only when prev is different from modified.
} // Close the current block.

// ============================================================
//  File operations
// ============================================================
static bool ReadFileText(const std::wstring& path, std::wstring& out) { // Define ReadFileText as the function that implements this part of the panel.
    std::ifstream f(path, std::ios::binary); // Complete the current operation before proceeding to the next statement.
    if (!f) return false; // Continue this branch only when f evaluates to false.
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); // Complete the current operation before proceeding to the next statement.
    if (data.size() >= 3 && (unsigned char)data[0]==0xEF && (unsigned char)data[1]==0xBB && (unsigned char)data[2]==0xBF) // Continue this branch only when data.size() is at least 3 and (unsigned char)data[0] is equal to 0xEF and (unsigned char)data[1] is equal to 0xBB and (unsigned char)data[2] is equal to 0xBF.
        data.erase(0, 3); // Complete the current operation before proceeding to the next statement.
    out = Widen(data); // Update out with the result of this expression.
    return true; // Return the computed true to the caller.
} // Close the current block.
static bool WriteFileText(const std::wstring& path, const std::wstring& text) { // Define WriteFileText as the function that implements this part of the panel.
    std::ofstream f(path, std::ios::binary | std::ios::trunc); // Complete the current operation before proceeding to the next statement.
    if (!f) return false; // Continue this branch only when f evaluates to false.
    const unsigned char bom[3] = {0xEF,0xBB,0xBF}; // Declare bom and start with {0xEF,0xBB,0xBF}.
    f.write((const char*)bom, 3); // Complete the current operation before proceeding to the next statement.
    std::string utf8 = ToUtf8(text); // Declare utf8 and start with ToUtf8(text).
    f.write(utf8.data(), (std::streamsize)utf8.size()); // Complete the current operation before proceeding to the next statement.
    return true; // Return the computed true to the caller.
} // Close the current block.
static std::wstring GetEditorText(NotepadInstance* inst) { // Define GetEditorText as the function that implements this part of the panel.
    if (!inst->hEdit) return L""; // Continue this branch only when inst->h edit evaluates to false.
    int len = GetWindowTextLengthW(inst->hEdit); // Declare len and start with GetWindowTextLengthW(inst->hEdit).
    if (len <= 0) return L""; // Continue this branch only when len is at most 0.
    std::vector<wchar_t> buf(len + 2, 0); // Complete the current operation before proceeding to the next statement.
    GETTEXTEX gt{}; gt.cb = (DWORD)((len + 2) * sizeof(wchar_t)); // Update the cb field with the calculated value.
    gt.flags = GT_USECRLF; gt.codepage = 1200; // Update the flags field with the calculated value.
    SendMessageW(inst->hEdit, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)buf.data()); // Call SendMessageW to send a control message and receive its result immediately.
    return std::wstring(buf.data()); // Return the computed std::wstring(buf.data()) to the caller.
} // Close the current block.
void DoSave(NotepadInstance* inst, bool saveAs) { // Define DoSave as the function that implements this part of the panel.
    if (!inst->hEdit) return; // Continue this branch only when inst->h edit evaluates to false.
    std::wstring target = inst->currentFile; // Declare target and start with inst->currentFile.
    if (target.empty() || saveAs) { // Continue this branch only when target.empty() is true or save as is true.
        wchar_t file[MAX_PATH] = L""; // Declare file and start with L"".
        if (!inst->currentFile.empty()) wcsncpy_s(file, BaseNameW(inst->currentFile).c_str(), _TRUNCATE); // Continue this branch only when inst->current file contains at least one item.
        else { // Run this fallback branch when the previous condition did not match.
            Document* d = Active(inst); // Declare d and start with Active(inst).
            if (d && !d->titleOverride.empty()) { // Continue this branch only when d is true and d->title override contains at least one item.
                std::wstring suggested = d->titleOverride + L".txt"; // Declare suggested and start with d->titleOverride + L".txt".
                wcsncpy_s(file, suggested.c_str(), _TRUNCATE); // Call wcsncpy_s to perform this part of the Notepad operation.
            } // Close the current block.
        } // Close the current block.
        OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); // Update the l struct size field with the calculated value.
        ofn.hwndOwner = inst->isChild ? inst->hParent : inst->hSelf; // Update the hwnd owner field with the calculated value.
        ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0"; // Update the lpstr filter field with the calculated value.
        ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; // Update the lpstr file field with the calculated value.
        ofn.lpstrDefExt = L"txt"; ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER; // Update the lpstr def ext field with the calculated value.
        if (!GetSaveFileNameW(&ofn)) return; // Continue this branch only when get save file name w(&ofn) evaluates to false.
        target = file; // Update target with the result of this expression.
    } // Close the current block.
    if (!WriteFileText(target, GetEditorText(inst))) { // Continue this branch only when write file text(target, get editor text(inst)) evaluates to false.
        MessageBoxW(inst->isChild ? inst->hParent : inst->hSelf, // Call MessageBoxW to show the initialization error to the user.
            L"Could not save the file.", APP_NAME, MB_OK | MB_ICONWARNING); // Complete the current operation before proceeding to the next statement.
        return; // Return the computed  to the caller.
    } // Close the current block.
    inst->currentFile = target; // Store the new value in the current file field of the current instance.
    Document* d = Active(inst); // Declare d and start with Active(inst).
    if (d) { d->filePath = target; d->titleOverride.clear(); } // Continue this branch only when d is true.
    SetModified(inst, false); // Call SetModified to apply the requested state or value.
    if (inst->hSelf) InvalidateRect(inst->hSelf, &inst->tabBarRect, FALSE); // Continue this branch only when inst- is greater than hSelf.
    FireNotify(inst, NP_NOTIFY_FILE_SAVED, inst->activeDoc); // Call FireNotify to perform this part of the Notepad operation.
} // Close the current block.

static void DoNewTab(NotepadInstance* inst) { // Define DoNewTab as the function that implements this part of the panel.
    if (inst->hRenameEdit) CommitRename(inst, true); // Continue this branch only when inst- is greater than hRenameEdit.
    CreateDoc(inst, L"", L""); // Call CreateDoc to create the requested interface object or resource.
    SetActiveDoc(inst, (int)inst->docs.size() - 1); // Call SetActiveDoc to apply the requested state or value.
    UpdateLayout(inst, inst->hSelf); // Call UpdateLayout to apply the requested state or value.
    InvalidateRect(inst->hSelf, nullptr, TRUE); // Call InvalidateRect to mark the requested area for repainting.
    FireNotify(inst, NP_NOTIFY_TAB_OPENED, (int)inst->docs.size() - 1); // Call FireNotify to perform this part of the Notepad operation.
} // Close the current block.

static void DoOpen(NotepadInstance* inst) { // Define DoOpen as the function that implements this part of the panel.
    if (inst->hRenameEdit) CommitRename(inst, true); // Continue this branch only when inst- is greater than hRenameEdit.
    std::vector<wchar_t> big(32768, 0); // Complete the current operation before proceeding to the next statement.
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); // Update the l struct size field with the calculated value.
    ofn.hwndOwner = inst->isChild ? inst->hParent : inst->hSelf; // Update the hwnd owner field with the calculated value.
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0"; // Update the lpstr filter field with the calculated value.
    ofn.lpstrFile = big.data(); ofn.nMaxFile = (DWORD)big.size(); // Update the lpstr file field with the calculated value.
    ofn.lpstrDefExt = L"txt"; // Update the lpstr def ext field with the calculated value.
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT; // Update the flags field with the calculated value.
    if (!GetOpenFileNameW(&ofn)) return; // Continue this branch only when get open file name w(&ofn) evaluates to false.
    std::vector<std::wstring> paths; // Declare paths for use by this block.
    if (big[0]) { // Continue this branch only when big[0] is true.
        std::wstring first(big.data()); // Complete the current operation before proceeding to the next statement.
        size_t pos = first.size() + 1; // Declare pos and start with first.size() + 1.
        if (pos < big.size() && big[pos] != 0) { // Continue this branch only when pos is less than big.size() and big[pos] is different from 0.
            std::wstring dir = first; // Declare dir and start with first.
            while (pos < big.size() && big[pos] != 0) { // Repeat the body while pos is less than big.size() and big[pos] is different from 0.
                std::wstring fn(big.data() + pos); // Complete the current operation before proceeding to the next statement.
                paths.push_back(dir + L"\\" + fn); // Complete the current operation before proceeding to the next statement.
                pos += fn.size() + 1; // Update + with the result of this expression.
            } // Close the current block.
        } else paths.push_back(first); // Complete the current operation before proceeding to the next statement.
    } // Close the current block.
    for (const std::wstring& p : paths) { // Visit each item in the collection and apply the loop body.
        std::wstring text; // Declare text for use by this block.
        if (!ReadFileText(p, text)) { // Continue this branch only when read file text(p, text) evaluates to false.
            std::wstring m = L"Could not read:\r\n" + p; // Declare m and start with L"Could not read:\r\n" + p.
            MessageBoxW(inst->isChild ? inst->hParent : inst->hSelf, // Call MessageBoxW to show the initialization error to the user.
                m.c_str(), APP_NAME, MB_OK | MB_ICONWARNING); // Complete the current operation before proceeding to the next statement.
            continue; // Skip to the next iteration of the current loop.
        } // Close the current block.
        CreateDoc(inst, text, p); // Call CreateDoc to create the requested interface object or resource.
    } // Close the current block.
    if (!paths.empty()) { // Continue this branch only when paths contains at least one item.
        SetActiveDoc(inst, (int)inst->docs.size() - 1); // Call SetActiveDoc to apply the requested state or value.
        UpdateLayout(inst, inst->hSelf); // Call UpdateLayout to apply the requested state or value.
        InvalidateRect(inst->hSelf, nullptr, TRUE); // Call InvalidateRect to mark the requested area for repainting.
        FireNotify(inst, NP_NOTIFY_TAB_OPENED, (int)inst->docs.size() - 1); // Call FireNotify to perform this part of the Notepad operation.
    } // Close the current block.
} // Close the current block.

// ============================================================
//  Search / Replace
// ============================================================
static std::wstring ReadControlText(HWND h) { // Define ReadControlText as the function that implements this part of the panel.
    int len = GetWindowTextLengthW(h); // Declare len and start with GetWindowTextLengthW(h).
    if (len <= 0) return L""; // Continue this branch only when len is at most 0.
    std::vector<wchar_t> buf(len + 1, 0); // Complete the current operation before proceeding to the next statement.
    GetWindowTextW(h, buf.data(), len + 1); // Call GetWindowTextW to read the text currently displayed by a control.
    return std::wstring(buf.data()); // Return the computed std::wstring(buf.data()) to the caller.
} // Close the current block.
static bool FindNext(NotepadInstance* inst, bool backwards) { // Define FindNext as the function that implements this part of the panel.
    if (!inst->hEdit || !inst->hSearchFind) return false; // Continue this branch only when inst->h edit || !inst->h search find evaluates to false.
    std::wstring needle = ReadControlText(inst->hSearchFind); // Declare needle and start with ReadControlText(inst->hSearchFind).
    if (needle.empty()) return false; // Continue this branch only when needle.empty() is true.
    DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0); // Declare sel and start with (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0).
    int start = LOWORD(sel), end = HIWORD(sel); // Declare start and start with LOWORD(sel), end = HIWORD(sel).
    FINDTEXTEXW ft{}; ft.lpstrText = (LPWSTR)needle.c_str(); // Update the lpstr text field with the calculated value.
    if (backwards) { ft.chrg.cpMin = 0; ft.chrg.cpMax = start; } // Continue this branch only when backwards is true.
    else { ft.chrg.cpMin = end; ft.chrg.cpMax = -1; } // Run this fallback branch when the previous condition did not match.
    DWORD flags = backwards ? 0 : FR_DOWN; // Declare flags and start with backwards ? 0 : FR_DOWN.
    if (inst->searchCase) flags |= FR_MATCHCASE; // Continue this branch only when inst- is greater than searchCase.
    LRESULT pos = SendMessageW(inst->hEdit, EM_FINDTEXTEXW, (WPARAM)flags, (LPARAM)&ft); // Declare pos and start with SendMessageW(inst->hEdit, EM_FINDTEXTEXW, (WPARAM)flags, (LPARAM)&ft).
    if (pos < 0) { // Continue this branch only when pos is less than 0.
        if (backwards) { ft.chrg.cpMin = -1; ft.chrg.cpMax = 0; } // Continue this branch only when backwards is true.
        else { ft.chrg.cpMin = 0; ft.chrg.cpMax = -1; } // Run this fallback branch when the previous condition did not match.
        pos = SendMessageW(inst->hEdit, EM_FINDTEXTEXW, (WPARAM)flags, (LPARAM)&ft); // Update pos with the result of this expression.
        if (pos < 0) return false; // Continue this branch only when pos is less than 0.
    } // Close the current block.
    SendMessageW(inst->hEdit, EM_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax); // Call SendMessageW to send a control message and receive its result immediately.
    SendMessageW(inst->hEdit, EM_SCROLLCARET, 0, 0); // Call SendMessageW to send a control message and receive its result immediately.
    SetFocus(inst->hEdit); // Call SetFocus to move keyboard focus to the requested control.
    return true; // Return the computed true to the caller.
} // Close the current block.
static int ReplaceOne(NotepadInstance* inst) { // Define ReplaceOne as the function that implements this part of the panel.
    if (!inst->hEdit || !inst->hSearchFind || !inst->hSearchRepl) return 0; // Continue this branch only when inst->h edit || !inst->h search find || !inst->h search repl evaluates to false.
    std::wstring needle = ReadControlText(inst->hSearchFind); // Declare needle and start with ReadControlText(inst->hSearchFind).
    std::wstring repl = ReadControlText(inst->hSearchRepl); // Declare repl and start with ReadControlText(inst->hSearchRepl).
    if (needle.empty()) return 0; // Continue this branch only when needle.empty() is true.
    DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0); // Declare sel and start with (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0).
    int s = LOWORD(sel), e = HIWORD(sel); // Declare s and start with LOWORD(sel), e = HIWORD(sel).
    if (e - s == (int)needle.size()) { // Continue this branch only when e - s is equal to (int)needle.size().
        std::wstring cur(e - s, 0); // Complete the current operation before proceeding to the next statement.
        TEXTRANGEW tr{}; tr.chrg.cpMin = s; tr.chrg.cpMax = e; tr.lpstrText = cur.data(); // Update the cp min field with the calculated value.
        SendMessageW(inst->hEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr); // Call SendMessageW to send a control message and receive its result immediately.
        bool match = inst->searchCase ? (cur == needle) : // Continue the current declaration or expression.
            std::equal(cur.begin(), cur.end(), needle.begin(), // Call std::equal to perform this part of the Notepad operation.
                [](wchar_t a, wchar_t b) { return towlower(a) == towlower(b); }); // Complete the current operation before proceeding to the next statement.
        if (!match) { FindNext(inst, false); return 0; } // Continue this branch only when match evaluates to false.
        SendMessageW(inst->hEdit, EM_REPLACESEL, TRUE, (LPARAM)repl.c_str()); // Call SendMessageW to send a control message and receive its result immediately.
    } else { FindNext(inst, false); return 0; } // Close the current nested block.
    FindNext(inst, false); // Call FindNext to retrieve the requested state or value.
    return 1; // Return the computed 1 to the caller.
} // Close the current block.
static int ReplaceAll(NotepadInstance* inst) { // Define ReplaceAll as the function that implements this part of the panel.
    if (!inst->hEdit || !inst->hSearchFind || !inst->hSearchRepl) return 0; // Continue this branch only when inst->h edit || !inst->h search find || !inst->h search repl evaluates to false.
    std::wstring needle = ReadControlText(inst->hSearchFind); // Declare needle and start with ReadControlText(inst->hSearchFind).
    std::wstring repl = ReadControlText(inst->hSearchRepl); // Declare repl and start with ReadControlText(inst->hSearchRepl).
    if (needle.empty()) return 0; // Continue this branch only when needle.empty() is true.
    SendMessageW(inst->hEdit, WM_SETREDRAW, FALSE, 0); // Call SendMessageW to send a control message and receive its result immediately.
    int count = 0; // Declare count and start with 0.
    FINDTEXTEXW ft{}; ft.chrg.cpMin = 0; ft.chrg.cpMax = -1; ft.lpstrText = (LPWSTR)needle.c_str(); // Update the cp min field with the calculated value.
    DWORD flags = FR_DOWN; // Declare flags and start with FR_DOWN.
    if (inst->searchCase) flags |= FR_MATCHCASE; // Continue this branch only when inst- is greater than searchCase.
    for (;;) { // Repeat the body using  and stop when .
        LRESULT pos = SendMessageW(inst->hEdit, EM_FINDTEXTEXW, (WPARAM)flags, (LPARAM)&ft); // Declare pos and start with SendMessageW(inst->hEdit, EM_FINDTEXTEXW, (WPARAM)flags, (LPARAM)&ft).
        if (pos < 0) break; // Continue this branch only when pos is less than 0.
        SendMessageW(inst->hEdit, EM_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax); // Call SendMessageW to send a control message and receive its result immediately.
        SendMessageW(inst->hEdit, EM_REPLACESEL, TRUE, (LPARAM)repl.c_str()); // Call SendMessageW to send a control message and receive its result immediately.
        ft.chrg.cpMin = ft.chrgText.cpMin + (LONG)repl.size(); // Update the cp min field with the calculated value.
        ft.chrg.cpMax = -1; ++count; // Update the cp max field with the calculated value.
        if (count > 100000) break; // Continue this branch only when count is greater than 100000.
    } // Close the current block.
    SendMessageW(inst->hEdit, WM_SETREDRAW, TRUE, 0); // Call SendMessageW to send a control message and receive its result immediately.
    InvalidateRect(inst->hEdit, nullptr, TRUE); // Call InvalidateRect to mark the requested area for repainting.
    if (count) SetModified(inst, true); // Continue this branch only when count is true.
    return count; // Return the computed count to the caller.
} // Close the current block.

// ============================================================
//  Tab bar layout + paint
// ============================================================
static void LayoutTabs(NotepadInstance* inst) { // Define LayoutTabs as the function that implements this part of the panel.
    inst->tabRects.clear(); // Complete the current operation before proceeding to the next statement.
    inst->tabNewBtnRect = {}; // Store the new value in the tab new btn rect field of the current instance.
    if (inst->docs.empty()) return; // Continue this branch only when inst- is greater than docs.empty().
    HDC hdc = CreateCompatibleDC(nullptr); // Declare hdc and start with CreateCompatibleDC(nullptr).
    HFONT old = (HFONT)SelectObject(hdc, inst->font); // Declare old and start with (HFONT)SelectObject(hdc, inst->font).
    SetBkMode(hdc, TRANSPARENT); // Call SetBkMode to apply the requested state or value.
    int pad = Dpi(inst, 8), gap = Dpi(inst, 4); // Declare pad and start with Dpi(inst, 8), gap = Dpi(inst, 4).
    int x = inst->tabBarRect.left + pad; // Declare x and start with inst->tabBarRect.left + pad.
    int y = inst->tabBarRect.top + Dpi(inst, 4); // Declare y and start with inst->tabBarRect.top + Dpi(inst, 4).
    int h = inst->tabBarRect.bottom - inst->tabBarRect.top - Dpi(inst, 8); // Declare h and start with inst->tabBarRect.bottom - inst->tabBarRect.top - Dpi(inst, 8).
    if (h < Dpi(inst, 20)) h = Dpi(inst, 20); // Continue this branch only when h is less than Dpi(inst, 20).
    int minTabW = Dpi(inst, 110), maxTabW = Dpi(inst, 220); // Declare min tab w and start with Dpi(inst, 110), maxTabW = Dpi(inst, 220).
    int rightLimit = inst->tabBarRect.right - Dpi(inst, 8) - h - Dpi(inst, 8); // Declare right limit and start with inst->tabBarRect.right - Dpi(inst, 8) - h - Dpi(inst, 8).
    for (size_t i = 0; i < inst->docs.size(); ++i) { // Repeat the body using size_t i = 0 and stop when i < inst->docs.size().
        std::wstring title = inst->docs[i]->Title(); // Declare title and start with inst->docs[i]->Title().
        SIZE sz{}; GetTextExtentPoint32W(hdc, title.c_str(), (int)title.size(), &sz); // Complete the current operation before proceeding to the next statement.
        int w = sz.cx + Dpi(inst, 10) + Dpi(inst, 22) + Dpi(inst, 10); // Declare w and start with sz.cx + Dpi(inst, 10) + Dpi(inst, 22) + Dpi(inst, 10).
        if (w < minTabW) w = minTabW; if (w > maxTabW) w = maxTabW; // Continue this branch only when w is less than minTabW.
        if (x + w > rightLimit) break; // Continue this branch only when x + w is greater than rightLimit.
        TabRect tr{}; tr.index = (int)i; // Update the the tab or item index field with the calculated value.
        tr.rc = { x, y, x + w, y + h }; // Update the the rectangle used to measure a window’s client area field with the calculated value.
        tr.rcClose = { tr.rc.right - Dpi(inst, 20), tr.rc.top + Dpi(inst, 4), // Update the the tab close-button hit-test rectangle field with the calculated value.
                       tr.rc.right - Dpi(inst, 4), tr.rc.bottom - Dpi(inst, 4) }; // Complete the current operation before proceeding to the next statement.
        inst->tabRects.push_back(tr); // Complete the current operation before proceeding to the next statement.
        x += w + gap; // Update + with the result of this expression.
    } // Close the current block.
    SelectObject(hdc, old); DeleteDC(hdc); // Call SelectObject to select a GDI object into the drawing context.
    inst->tabNewBtnRect = { x, y, x + h, y + h }; // Store the new value in the tab new btn rect field of the current instance.
    if (inst->hRenameEdit && inst->renamingTab >= 0) { // Continue this branch only when inst- is greater than hRenameEdit and inst- is greater than renamingTab >= 0.
        for (const TabRect& tr : inst->tabRects) { // Visit each item in the collection and apply the loop body.
            if (tr.index == inst->renamingTab) { // Continue this branch only when tr.index is equal to inst->renamingTab.
                SetWindowPos(inst->hRenameEdit, nullptr, // Call SetWindowPos to update a window’s position, size, or z-order.
                    tr.rc.left + Dpi(inst, 8), tr.rc.top + Dpi(inst, 6), // Supply this parameter or list item to the enclosing call.
                    tr.rcClose.left - Dpi(inst, 2) - (tr.rc.left + Dpi(inst, 8)), // Supply this parameter or list item to the enclosing call.
                    tr.rc.bottom - Dpi(inst, 6) - (tr.rc.top + Dpi(inst, 6)), // Supply this parameter or list item to the enclosing call.
                    SWP_NOZORDER | SWP_NOACTIVATE); // Complete the current operation before proceeding to the next statement.
                break; // Stop processing the current switch case or loop.
            } // Close the current block.
        } // Close the current block.
    } // Close the current block.
} // Close the current block.

static void PaintTabs(NotepadInstance* inst, HDC hdc) { // Define PaintTabs as the function that implements this part of the panel.
    RECT rc = inst->tabBarRect; // Declare the rectangle used to measure a window’s client area and start with inst->tabBarRect.
    FillSolid(hdc, rc, inst->theme.BgPanel); // Call FillSolid to render the requested interface element.
    RECT sep = { rc.left, rc.bottom - 1, rc.right, rc.bottom }; // Declare sep and start with { rc.left, rc.bottom - 1, rc.right, rc.bottom }.
    FillSolid(hdc, sep, inst->theme.BorderSoft); // Call FillSolid to render the requested interface element.
    if (inst->docs.empty()) return; // Continue this branch only when inst- is greater than docs.empty().
    LayoutTabs(inst); // Call LayoutTabs to perform this part of the Notepad operation.
    int saved = SaveDC(hdc); // Declare saved and start with SaveDC(hdc).
    IntersectClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom); // Call IntersectClipRect to perform this part of the Notepad operation.
    for (const TabRect& tr : inst->tabRects) { // Visit each item in the collection and apply the loop body.
        bool active = (tr.index == inst->activeDoc); // Complete the current operation before proceeding to the next statement.
        bool hover = (tr.index == inst->tabHot) && !active; // Complete the current operation before proceeding to the next statement.
        COLORREF bg = active ? inst->theme.BgCard // Declare bg and start with active ? inst->theme.BgCard.
            : (hover ? inst->theme.BgRailHi : inst->theme.BgPanel); // Complete the current operation before proceeding to the next statement.
        FillRoundRect(hdc, tr.rc, Dpi(inst, 6), bg); // Call FillRoundRect to render the requested interface element.
        if (!active && !hover) StrokeRoundRect(hdc, tr.rc, Dpi(inst, 6), inst->theme.BorderSoft); // Continue this branch only when active && !hover evaluates to false.
        if (active) { // Continue this branch only when whether the control is in its active visual state is true.
            RECT bar = { tr.rc.left + Dpi(inst, 6), tr.rc.bottom - Dpi(inst, 2), // Declare bar and start with { tr.rc.left + Dpi(inst, 6), tr.rc.bottom - Dpi(inst, 2),.
                         tr.rc.right - Dpi(inst, 6), tr.rc.bottom }; // Complete the current operation before proceeding to the next statement.
            FillSolid(hdc, bar, inst->theme.Accent); // Call FillSolid to render the requested interface element.
        } // Close the current block.
        Document* d = inst->docs[tr.index]; // Declare d and start with inst->docs[tr.index].
        std::wstring title = d->Title(); // Declare title and start with d->Title().
        bool renamingThis = (tr.index == inst->renamingTab && inst->hRenameEdit); // Complete the current operation before proceeding to the next statement.
        if (!renamingThis) { // Continue this branch only when renaming this evaluates to false.
            HFONT old = (HFONT)SelectObject(hdc, inst->font); // Declare old and start with (HFONT)SelectObject(hdc, inst->font).
            SetBkMode(hdc, TRANSPARENT); // Call SetBkMode to apply the requested state or value.
            SetTextColor(hdc, active ? inst->theme.TextHi : inst->theme.TextMid); // Call SetTextColor to apply the requested state or value.
            RECT txt = { tr.rc.left + Dpi(inst, 10), tr.rc.top, // Declare txt and start with { tr.rc.left + Dpi(inst, 10), tr.rc.top,.
                         tr.rcClose.left - Dpi(inst, 2), tr.rc.bottom }; // Complete the current operation before proceeding to the next statement.
            DrawTextW(hdc, title.c_str(), -1, &txt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS); // Call DrawTextW to draw text within the requested rectangle.
            SelectObject(hdc, old); // Call SelectObject to select a GDI object into the drawing context.
        } // Close the current block.
        bool showClose = (active || tr.index == inst->tabHot || tr.index == inst->tabCloseHot) && !renamingThis; // Complete the current operation before proceeding to the next statement.
        if (showClose) { // Continue this branch only when show close is true.
            RECT x = tr.rcClose; // Declare x and start with tr.rcClose.
            if (tr.index == inst->tabCloseHot) // Continue this branch only when tr.index is equal to inst->tabCloseHot.
                Circle(hdc, (x.left + x.right) / 2, (x.top + x.bottom) / 2, // Call Circle to perform this part of the Notepad operation.
                    (x.right - x.left) / 2, inst->theme.BgRailHi); // Complete the current operation before proceeding to the next statement.
            DrawIcon(hdc, inst, Icon::Close, x, // Call DrawIcon to render the requested interface element.
                (tr.index == inst->tabCloseHot) ? inst->theme.TextHi : inst->theme.TextMid, 2); // Complete the current operation before proceeding to the next statement.
        } // Close the current block.
    } // Close the current block.
    {   // "+" button
        bool newHover = (inst->tabHot == -2); // Complete the current operation before proceeding to the next statement.
        if (newHover) FillRoundRect(hdc, inst->tabNewBtnRect, Dpi(inst, 6), inst->theme.BgRailHi); // Continue this branch only when new hover is true.
        RECT ir = inst->tabNewBtnRect; // Declare ir and start with inst->tabNewBtnRect.
        int inset = (ir.right - ir.left) / 4; // Declare inset and start with (ir.right - ir.left) / 4.
        RECT i2 = { ir.left + inset, ir.top + inset, ir.right - inset, ir.bottom - inset }; // Declare i2 and start with { ir.left + inset, ir.top + inset, ir.right - inset, ir.bottom - inset }.
        DrawIcon(hdc, inst, Icon::Plus, i2, newHover ? inst->theme.TextHi : inst->theme.TextMid, 2); // Call DrawIcon to render the requested interface element.
    } // Close the current block.
    RestoreDC(hdc, saved); // Call RestoreDC to perform this part of the Notepad operation.
} // Close the current block.

static int HitTestTab(NotepadInstance* inst, int x, int y, bool& onClose) { // Define HitTestTab as the function that implements this part of the panel.
    onClose = false; // Update on close with the result of this expression.
    POINT pt = { x, y }; // Declare pt and start with { x, y }.
    if (PtInRect(&inst->tabNewBtnRect, pt)) return -2; // Continue this branch only when pt in rect(&inst- is greater than tabNewBtnRect, pt).
    for (const TabRect& tr : inst->tabRects) { // Visit each item in the collection and apply the loop body.
        if (PtInRect(&tr.rc, pt)) { // Continue this branch only when pt in rect(&tr.rc, pt) is true.
            if (PtInRect(&tr.rcClose, pt)) onClose = true; // Continue this branch only when pt in rect(&tr.rc close, pt) is true.
            return tr.index; // Return the computed tr.index to the caller.
        } // Close the current block.
    } // Close the current block.
    return -1; // Return the computed -1 to the caller.
} // Close the current block.

// ============================================================
//  Inline rename
// ============================================================
LRESULT CALLBACK RenameEditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) { // Define RenameEditProc as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(GetParent(h)); // Declare the state object for this Notepad panel and start with GetInstance(GetParent(h)).
    switch (m) { // Choose the handler based on the current message or command.
    case WM_KEYDOWN: // Handle WM_KEYDOWN to handle keyboard shortcuts.
        if (w == VK_RETURN) { PostMessageW(GetParent(h), WM_RENAME_COMMIT, 1, 0); return 0; } // Continue this branch only when w is equal to VK_RETURN.
        if (w == VK_ESCAPE) { PostMessageW(GetParent(h), WM_RENAME_COMMIT, 0, 0); return 0; } // Continue this branch only when w is equal to VK_ESCAPE.
        break; // Stop processing the current switch case or loop.
    case WM_KILLFOCUS: // Handle WM_KILLFOCUS to react to losing keyboard focus.
        PostMessageW(GetParent(h), WM_RENAME_COMMIT, 1, 0); // Call PostMessageW to queue a message for the target window to handle later.
        break; // Stop processing the current switch case or loop.
    case WM_CHAR: // Handle WM_CHAR to handle typed characters.
        if (w == VK_RETURN || w == VK_ESCAPE) return 0; // Continue this branch only when w is equal to VK_RETURN or w is equal to VK_ESCAPE.
        break; // Stop processing the current switch case or loop.
    } // Close the current block.
    return DefSubclassProc(h, m, w, l); // Return the computed def subclass proc(h, m, w, l) to the caller.
} // Close the current block.

void BeginRenameTab(NotepadInstance* inst, int index) { // Define BeginRenameTab as the function that implements this part of the panel.
    if (index < 0 || index >= (int)inst->docs.size()) return; // Continue this branch only when the tab or item index is less than 0 or the tab or item index is at least (int)inst->docs.size().
    if (inst->hRenameEdit) CommitRename(inst, true); // Continue this branch only when inst- is greater than hRenameEdit.
    LayoutTabs(inst); // Call LayoutTabs to perform this part of the Notepad operation.
    TabRect foundTab{}; bool found = false; // Update found with the result of this expression.
    for (const TabRect& tr : inst->tabRects) { // Visit each item in the collection and apply the loop body.
        if (tr.index == index) { foundTab = tr; found = true; break; } // Continue this branch only when tr.index is equal to index.
    } // Close the current block.
    if (!found) return; // Continue this branch only when found evaluates to false.
    Document* d = inst->docs[index]; // Declare d and start with inst->docs[index].
    std::wstring initial = d->RenameInitial(); // Declare initial and start with d->RenameInitial().
    RECT r; // Declare r for use by this block.
    r.left = foundTab.rc.left + Dpi(inst, 8); // Update the left field with the calculated value.
    r.right = foundTab.rcClose.left - Dpi(inst, 2); // Update the right field with the calculated value.
    r.top = foundTab.rc.top + Dpi(inst, 6); // Update the top field with the calculated value.
    r.bottom = foundTab.rc.bottom - Dpi(inst, 6); // Update the bottom field with the calculated value.
    if (r.right <= r.left + Dpi(inst, 20)) return; // Continue this branch only when r.right is at most r.left + Dpi(inst, 20).
    inst->hRenameEdit = CreateWindowExW(0, L"EDIT", initial.c_str(), // Store the new value in the h rename edit field of the current instance.
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER, // Supply this parameter or list item to the enclosing call.
        r.left, r.top, r.right - r.left, r.bottom - r.top, // Supply this parameter or list item to the enclosing call.
        inst->hSelf, nullptr, inst->hInst, nullptr); // Complete the current operation before proceeding to the next statement.
    SendMessageW(inst->hRenameEdit, WM_SETFONT, (WPARAM)inst->font, TRUE); // Call SendMessageW to send a control message and receive its result immediately.
    SetWindowTheme(inst->hRenameEdit, L"DarkMode_Explorer", nullptr); // Call SetWindowTheme to apply the selected control theme.
    SetWindowSubclass(inst->hRenameEdit, RenameEditProc, 200, 0); // Call SetWindowSubclass to install a callback that customizes a native control.
    inst->renamingTab = index; // Store the new value in the renaming tab field of the current instance.
    SetFocus(inst->hRenameEdit); // Call SetFocus to move keyboard focus to the requested control.
    SendMessageW(inst->hRenameEdit, EM_SETSEL, 0, -1); // Call SendMessageW to send a control message and receive its result immediately.
    InvalidateRect(inst->hSelf, &inst->tabBarRect, FALSE); // Call InvalidateRect to mark the requested area for repainting.
} // Close the current block.

void CommitRename(NotepadInstance* inst, bool accept) { // Define CommitRename as the function that implements this part of the panel.
    if (!inst->hRenameEdit) return; // Continue this branch only when inst->h rename edit evaluates to false.
    int index = inst->renamingTab; // Declare the tab or item index and start with inst->renamingTab.
    std::wstring newName; // Declare new name for use by this block.
    if (accept) { // Continue this branch only when accept is true.
        int len = GetWindowTextLengthW(inst->hRenameEdit); // Declare len and start with GetWindowTextLengthW(inst->hRenameEdit).
        std::vector<wchar_t> buf(len + 2, 0); // Complete the current operation before proceeding to the next statement.
        GetWindowTextW(inst->hRenameEdit, buf.data(), len + 1); // Call GetWindowTextW to read the text currently displayed by a control.
        newName.assign(buf.data()); // Complete the current operation before proceeding to the next statement.
        size_t a = 0, b = newName.size(); // Declare a and start with 0, b = newName.size().
        while (a < b && (newName[a] == L' ' || newName[a] == L'\t')) ++a; // Repeat the body while a is less than b and (new name[a] is equal to L' ' or new name[a] is equal to L'\t').
        while (b > a && (newName[b-1] == L' ' || newName[b-1] == L'\t')) --b; // Repeat the body while b is greater than a and (new name[b-1] is equal to L' ' or new name[b-1] is equal to L'\t').
        newName = (a < b) ? newName.substr(a, b - a) : L""; // Update new name with the result of this expression.
    } // Close the current block.
    HWND edit = inst->hRenameEdit; // Declare edit and start with inst->hRenameEdit.
    inst->hRenameEdit = nullptr; inst->renamingTab = -1; // Store the new value in the h rename edit field of the current instance.
    if (edit) { RemoveWindowSubclass(edit, RenameEditProc, 200); DestroyWindow(edit); } // Continue this branch only when edit is true.
    if (!accept || newName.empty()) { // Continue this branch only when accept || new name contains at least one item.
        InvalidateRect(inst->hSelf, &inst->tabBarRect, FALSE); // Call InvalidateRect to mark the requested area for repainting.
        if (inst->hEdit) SetFocus(inst->hEdit); // Continue this branch only when inst- is greater than hEdit.
        return; // Return the computed  to the caller.
    } // Close the current block.
    Document* d = (index >= 0 && index < (int)inst->docs.size()) ? inst->docs[index] : nullptr; // Complete the current operation before proceeding to the next statement.
    if (!d) { if (inst->hEdit) SetFocus(inst->hEdit); return; } // Continue this branch only when d evaluates to false.
    if (d->filePath.empty()) { // Continue this branch only when d- is greater than filePath.empty().
        d->titleOverride = newName; // Store the new value in the a custom title displayed instead of the file name field of the current instance.
    } else { // Continue the current declaration or expression.
        fs::path oldP(d->filePath); // Complete the current operation before proceeding to the next statement.
        std::wstring ext = oldP.extension().wstring(); // Declare ext and start with oldP.extension().wstring().
        if (ext.empty()) ext = L".txt"; // Continue this branch only when ext.empty() is true.
        fs::path newP = oldP.parent_path() / (newName + ext); // Declare new p and start with oldP.parent_path() / (newName + ext).
        if (newP == oldP) { /* no change */ } // Continue this branch only when new p is equal to oldP.
        else { // Run this fallback branch when the previous condition did not match.
            std::error_code ec; // Declare ec for use by this block.
            if (fs::exists(newP, ec)) { // Continue this branch only when fs::exists(new p, ec) is true.
                MessageBoxW(inst->isChild ? inst->hParent : inst->hSelf, // Call MessageBoxW to show the initialization error to the user.
                    L"A file with that name already exists in this folder.", // Supply this parameter or list item to the enclosing call.
                    APP_NAME, MB_OK | MB_ICONWARNING); // Complete the current operation before proceeding to the next statement.
            } else { // Continue the current declaration or expression.
                fs::rename(oldP, newP, ec); // Call fs::rename to perform this part of the Notepad operation.
                if (ec) { // Continue this branch only when ec is true.
                    MessageBoxW(inst->isChild ? inst->hParent : inst->hSelf, // Call MessageBoxW to show the initialization error to the user.
                        L"Could not rename the file on disk.", // Supply this parameter or list item to the enclosing call.
                        APP_NAME, MB_OK | MB_ICONWARNING); // Complete the current operation before proceeding to the next statement.
                } else { // Continue the current declaration or expression.
                    d->filePath = newP.wstring(); d->titleOverride.clear(); // Store the new value in the the backing file path for this document field of the current instance.
                    if (index == inst->activeDoc) { inst->currentFile = d->filePath; UpdateTitle(inst); } // Continue this branch only when the tab or item index is equal to inst->activeDoc.
                } // Close the current block.
            } // Close the current block.
        } // Close the current block.
    } // Close the current block.
    InvalidateRect(inst->hSelf, &inst->tabBarRect, FALSE); // Call InvalidateRect to mark the requested area for repainting.
    UpdateWindow(inst->hSelf); // Call UpdateWindow to request an immediate paint for the frame.
    if (inst->hEdit) SetFocus(inst->hEdit); // Continue this branch only when inst- is greater than hEdit.
} // Close the current block.

// ============================================================
//  Layout
// ============================================================
static void LayoutSearchBar(NotepadInstance* inst) { // Define LayoutSearchBar as the function that implements this part of the panel.
    if (!inst->searchOpen) return; // Continue this branch only when inst->search open evaluates to false.
    RECT sr = inst->searchRect; // Declare sr and start with inst->searchRect.
    const int pad = Dpi(inst, 10), btn = Dpi(inst, 28), rowH = Dpi(inst, 30); // Declare pad and start with Dpi(inst, 10), btn = Dpi(inst, 28), rowH = Dpi(inst, 30).
    int x = sr.left + pad, y = sr.top + Dpi(inst, 8); // Declare x and start with sr.left + pad, y = sr.top + Dpi(inst, 8).
    int rightLimit = sr.right - pad; // Declare right limit and start with sr.right - pad.
    int buttonsW = btn * 4 + Dpi(inst, 3) * 3; // Declare buttons w and start with btn * 4 + Dpi(inst, 3) * 3.
    int inputW = rightLimit - x - buttonsW - Dpi(inst, 12); // Declare input w and start with rightLimit - x - buttonsW - Dpi(inst, 12).
    if (inputW < Dpi(inst, 100)) inputW = Dpi(inst, 100); // Continue this branch only when input w is less than Dpi(inst, 100).
    SetWindowPos(inst->hSearchFind, nullptr, x, y + Dpi(inst, 1), inputW, rowH, SWP_NOZORDER); // Call SetWindowPos to update a window’s position, size, or z-order.
    x += inputW + Dpi(inst, 12); // Update + with the result of this expression.
    SetWindowPos(inst->hSearchCase, nullptr, x, y + Dpi(inst, 1), btn, btn, SWP_NOZORDER); x += btn + Dpi(inst, 3); // Store the new value in the h search case, nullptr, x, y + dpi(inst, 1), btn, btn, swp nozorder); x + field of the current instance.
    SetWindowPos(inst->hSearchPrev, nullptr, x, y + Dpi(inst, 1), btn, btn, SWP_NOZORDER); x += btn + Dpi(inst, 3); // Store the new value in the h search prev, nullptr, x, y + dpi(inst, 1), btn, btn, swp nozorder); x + field of the current instance.
    SetWindowPos(inst->hSearchNext, nullptr, x, y + Dpi(inst, 1), btn, btn, SWP_NOZORDER); x += btn + Dpi(inst, 3); // Store the new value in the h search next, nullptr, x, y + dpi(inst, 1), btn, btn, swp nozorder); x + field of the current instance.
    SetWindowPos(inst->hSearchClose, nullptr, x, y + Dpi(inst, 1), btn, btn, SWP_NOZORDER); // Call SetWindowPos to update a window’s position, size, or z-order.
    if (inst->replaceOpen) { // Continue this branch only when inst- is greater than replaceOpen.
        int y2 = sr.top + Dpi(inst, 8) + rowH + Dpi(inst, 8); // Declare y2 and start with sr.top + Dpi(inst, 8) + rowH + Dpi(inst, 8).
        int x2 = sr.left + pad; // Declare x2 and start with sr.left + pad.
        int buttons2W = btn * 2 + Dpi(inst, 3); // Declare buttons2 w and start with btn * 2 + Dpi(inst, 3).
        int input2W = rightLimit - x2 - buttons2W - Dpi(inst, 12); // Declare input2 w and start with rightLimit - x2 - buttons2W - Dpi(inst, 12).
        if (input2W < Dpi(inst, 100)) input2W = Dpi(inst, 100); // Continue this branch only when input2 w is less than Dpi(inst, 100).
        SetWindowPos(inst->hSearchRepl, nullptr, x2, y2, input2W, rowH, SWP_NOZORDER); x2 += input2W + Dpi(inst, 12); // Store the new value in the h search repl, nullptr, x2, y2, input2 w, row h, swp nozorder); x2 + field of the current instance.
        SetWindowPos(inst->hSearchReplOne, nullptr, x2, y2 + Dpi(inst, 1), btn, btn, SWP_NOZORDER); x2 += btn + Dpi(inst, 3); // Store the new value in the h search repl one, nullptr, x2, y2 + dpi(inst, 1), btn, btn, swp nozorder); x2 + field of the current instance.
        SetWindowPos(inst->hSearchReplAll, nullptr, x2, y2 + Dpi(inst, 1), btn, btn, SWP_NOZORDER); // Call SetWindowPos to update a window’s position, size, or z-order.
    } // Close the current block.
} // Close the current block.

static int BtnSizeFor(NotepadInstance* inst, ToolbarSize s) { // Define BtnSizeFor as the function that implements this part of the panel.
    switch (s) { // Choose the handler based on the current message or command.
    case TBSIZE_SMALL:  return Dpi(inst, 26); // Handle TBSIZE_SMALL to process this window message or command.
    case TBSIZE_LARGE:  return Dpi(inst, 42); // Handle TBSIZE_LARGE to process this window message or command.
    default:            return Dpi(inst, 32); // Handle any switch value not covered by the named cases.
    } // Close the current block.
} // Close the current block.
static int GapFor(NotepadInstance* inst, ToolbarSize s) { // Define GapFor as the function that implements this part of the panel.
    switch (s) { // Choose the handler based on the current message or command.
    case TBSIZE_SMALL:  return Dpi(inst, 3); // Handle TBSIZE_SMALL to process this window message or command.
    case TBSIZE_LARGE:  return Dpi(inst, 6); // Handle TBSIZE_LARGE to process this window message or command.
    default:            return Dpi(inst, 4); // Handle any switch value not covered by the named cases.
    } // Close the current block.
} // Close the current block.
static SIZE ToolbarDesiredClientSize(NotepadInstance* inst) { // Define ToolbarDesiredClientSize as the function that implements this part of the panel.
    int btn = BtnSizeFor(inst, inst->tbSize), gap = GapFor(inst, inst->tbSize), pad = Dpi(inst, 10), n = 10; // Declare btn and start with BtnSizeFor(inst, inst->tbSize), gap = GapFor(inst, inst->tbSize), pad = Dpi(inst, 10), n = 10.
    SIZE sz{}; bool horiz = (inst->tbDock == TBDOCK_TOP || inst->tbDock == TBDOCK_BOTTOM); // Complete the current operation before proceeding to the next statement.
    if (inst->tbDock == TBDOCK_FLOAT) horiz = inst->tbFloatHoriz; // Continue this branch only when inst- is greater than tbDock == TBDOCK_FLOAT.
    if (horiz) { sz.cx = pad*2 + n*btn + (n-1)*gap; sz.cy = pad*2 + btn; } // Continue this branch only when horiz is true.
    else       { sz.cx = pad*2 + btn; sz.cy = pad*2 + n*btn + (n-1)*gap; } // Run this fallback branch when the previous condition did not match.
    return sz; // Return the computed sz to the caller.
} // Close the current block.
static void LayoutToolbarChildren(NotepadInstance* inst) { // Define LayoutToolbarChildren as the function that implements this part of the panel.
    if (!inst->hToolbarHost) return; // Continue this branch only when inst->h toolbar host evaluates to false.
    int btn = BtnSizeFor(inst, inst->tbSize), gap = GapFor(inst, inst->tbSize), pad = Dpi(inst, 10); // Declare btn and start with BtnSizeFor(inst, inst->tbSize), gap = GapFor(inst, inst->tbSize), pad = Dpi(inst, 10).
    bool horiz = (inst->tbDock == TBDOCK_TOP || inst->tbDock == TBDOCK_BOTTOM); // Complete the current operation before proceeding to the next statement.
    if (inst->tbDock == TBDOCK_FLOAT) horiz = inst->tbFloatHoriz; // Continue this branch only when inst- is greater than tbDock == TBDOCK_FLOAT.
    int cx = pad, cy = pad; // Declare cx and start with pad, cy = pad.
    for (int i = 0; i < 10; ++i) { // Repeat the body using int i = 0 and stop when i < 10.
        if (!inst->hTbBtns[i]) continue; // Continue this branch only when inst->h tb btns[i] evaluates to false.
        SetWindowPos(inst->hTbBtns[i], nullptr, cx, cy, btn, btn, SWP_NOZORDER); // Call SetWindowPos to update a window’s position, size, or z-order.
        if (horiz) cx += btn + gap; else cy += btn + gap; // Continue this branch only when horiz is true.
    } // Close the current block.
} // Close the current block.

void UpdateLayout(NotepadInstance* inst, HWND hWnd) { // Define UpdateLayout as the function that implements this part of the panel.
    RECT rc; GetClientRect(hWnd, &rc); // Declare the rectangle used to measure a window’s client area for use by this block.
    int topInset = 0, bottomInset = 0, leftInset = 0, rightInset = 0; // Declare top inset and start with 0, bottomInset = 0, leftInset = 0, rightInset = 0.
    SIZE tbsz{0, 0}; // Complete the current operation before proceeding to the next statement.
    if (inst->hToolbarHost && inst->tbVisible && inst->tbDock != TBDOCK_FLOAT) { // Measure the toolbar using its explicit state, even before its parent is shown.
        tbsz = ToolbarDesiredClientSize(inst); // Update tbsz with the result of this expression.
        switch (inst->tbDock) { // Choose the handler based on the current message or command.
        case TBDOCK_TOP:    topInset = tbsz.cy; break; // Handle TBDOCK_TOP to process this window message or command.
        case TBDOCK_BOTTOM: bottomInset = tbsz.cy; break; // Handle TBDOCK_BOTTOM to process this window message or command.
        case TBDOCK_LEFT:   leftInset = tbsz.cx; break; // Handle TBDOCK_LEFT to process this window message or command.
        case TBDOCK_RIGHT:  rightInset = tbsz.cx; break; // Handle TBDOCK_RIGHT to process this window message or command.
        default: break; // Handle any switch value not covered by the named cases.
        } // Close the current block.
    } // Close the current block.
    const int statusH = inst->showStatus ? Dpi(inst, 26) : 0; // Declare status h and start with inst->showStatus ? Dpi(inst, 26) : 0.
    if (inst->hToolbarHost && inst->tbVisible && inst->tbDock != TBDOCK_FLOAT) { // Lay out the toolbar whenever its explicit state is visible.
        RECT tb{}; // Complete the current operation before proceeding to the next statement.
        switch (inst->tbDock) { // Choose the handler based on the current message or command.
        case TBDOCK_TOP:    tb = {0, 0, rc.right, tbsz.cy}; break; // Handle TBDOCK_TOP to process this window message or command.
        case TBDOCK_BOTTOM: tb = {0, rc.bottom - statusH - tbsz.cy, rc.right, rc.bottom - statusH}; break; // Handle TBDOCK_BOTTOM to process this window message or command.
        case TBDOCK_LEFT:   tb = {0, 0, tbsz.cx, rc.bottom}; break; // Handle TBDOCK_LEFT to process this window message or command.
        case TBDOCK_RIGHT:  tb = {rc.right - tbsz.cx, 0, rc.right, rc.bottom}; break; // Handle TBDOCK_RIGHT to process this window message or command.
        } // Close the current block.
        SetWindowPos(inst->hToolbarHost, nullptr, tb.left, tb.top, // Call SetWindowPos to update a window’s position, size, or z-order.
            tb.right - tb.left, tb.bottom - tb.top, SWP_NOZORDER); // Complete the current operation before proceeding to the next statement.
        LayoutToolbarChildren(inst); // Call LayoutToolbarChildren to perform this part of the Notepad operation.
    } // Close the current block.
    int top = topInset; // Declare top and start with topInset.
    if (inst->searchOpen) { // Continue this branch only when inst- is greater than searchOpen.
        int searchH = inst->replaceOpen ? Dpi(inst, 84) : Dpi(inst, 46); // Declare search h and start with inst->replaceOpen ? Dpi(inst, 84) : Dpi(inst, 46).
        inst->searchRect = { leftInset, top, rc.right - rightInset, top + searchH }; // Store the new value in the search rect field of the current instance.
        top += searchH; // Update + with the result of this expression.
    } else inst->searchRect = {}; // Store the new value in the search rect field of the current instance.
    const int tabH = Dpi(inst, 36); // Declare tab h and start with Dpi(inst, 36).
    inst->tabBarRect = { leftInset, top, rc.right - rightInset, top + tabH }; // Store the new value in the tab bar rect field of the current instance.
    top += tabH; // Update + with the result of this expression.
    inst->statusRect = { leftInset, rc.bottom - statusH, rc.right - rightInset, rc.bottom }; // Store the new value in the status rect field of the current instance.
    inst->editRect = { leftInset, top, rc.right - rightInset, rc.bottom - statusH - bottomInset }; // Store the new value in the edit rect field of the current instance.
    for (Document* d : inst->docs) { // Visit each item in the collection and apply the loop body.
        if (!d->hEdit) continue; // Continue this branch only when d->h edit evaluates to false.
        SetWindowPos(d->hEdit, nullptr, inst->editRect.left, inst->editRect.top, // Call SetWindowPos to update a window’s position, size, or z-order.
            inst->editRect.right - inst->editRect.left, // Supply this parameter or list item to the enclosing call.
            inst->editRect.bottom - inst->editRect.top, SWP_NOZORDER); // Complete the current operation before proceeding to the next statement.
    } // Close the current block.
    if (inst->searchOpen) LayoutSearchBar(inst); // Continue this branch only when inst- is greater than searchOpen.
} // Close the current block.

// ============================================================
//  Toolbar context menu
// ============================================================
void SetToolbarSize(NotepadInstance* inst, ToolbarSize size) { // Define SetToolbarSize as the function that implements this part of the panel.
    if (size == inst->tbSize) return; // Continue this branch only when size is equal to inst->tbSize.
    inst->tbSize = size; // Store the new value in the tb size field of the current instance.
    if (!inst->hostManaged) RegWriteStr(L"ToolbarSize", std::to_wstring((int)inst->tbSize)); // Continue this branch only when inst->host managed evaluates to false.
    if (inst->tbDock == TBDOCK_FLOAT && inst->hToolbarHost) { // Continue this branch only when inst- is greater than tbDock == TBDOCK_FLOAT and inst- is greater than hToolbarHost.
        SIZE sz = ToolbarDesiredClientSize(inst); // Declare sz and start with ToolbarDesiredClientSize(inst).
        RECT wr = {0, 0, sz.cx, sz.cy}; // Declare wr and start with {0, 0, sz.cx, sz.cy}.
        DWORD st = (DWORD)GetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE); // Declare st and start with (DWORD)GetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE).
        DWORD ex = (DWORD)GetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE); // Declare ex and start with (DWORD)GetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE).
        AdjustWindowRectEx(&wr, st, FALSE, ex); // Call AdjustWindowRectEx to perform this part of the Notepad operation.
        SetWindowPos(inst->hToolbarHost, nullptr, 0, 0, wr.right-wr.left, wr.bottom-wr.top, // Call SetWindowPos to update a window’s position, size, or z-order.
            SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED); // Complete the current operation before proceeding to the next statement.
    } // Close the current block.
    if (inst->hSelf) UpdateLayout(inst, inst->hSelf); // Continue this branch only when inst- is greater than hSelf.
    if (inst->hToolbarHost) InvalidateRect(inst->hToolbarHost, nullptr, TRUE); // Continue this branch only when inst- is greater than hToolbarHost.
} // Close the current block.

void SetToolbarDock(NotepadInstance* inst, ToolbarDock dock) { // Define SetToolbarDock as the function that implements this part of the panel.
    // Disable floating in child mode
    if (inst->isChild && dock == TBDOCK_FLOAT) return; // Continue this branch only when inst- is greater than isChild and dock is equal to TBDOCK_FLOAT.
    if (dock == inst->tbDock) return; // Continue this branch only when dock is equal to inst->tbDock.
    if (dock == TBDOCK_FLOAT) inst->tbFloatHoriz = (inst->tbDock == TBDOCK_TOP || inst->tbDock == TBDOCK_BOTTOM); // Continue this branch only when dock is equal to TBDOCK_FLOAT.
    inst->tbDock = dock; // Store the new value in the tb dock field of the current instance.
    if (!inst->hToolbarHost) return; // Continue this branch only when inst->h toolbar host evaluates to false.
    LONG_PTR style = GetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE); // Declare style and start with GetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE).
    bool wasChild = (style & WS_CHILD) != 0, willBeChild = (dock != TBDOCK_FLOAT); // Complete the current operation before proceeding to the next statement.
    if (wasChild && !willBeChild) { // Continue this branch only when was child is true and will be child evaluates to false.
        SetParent(inst->hToolbarHost, nullptr); // Call SetParent to apply the requested state or value.
        SetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE, (style & ~(LONG_PTR)WS_CHILD) | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN); // Call SetWindowLongPtrW to attach or update data associated with a window.
        SetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE, GetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE) | WS_EX_TOOLWINDOW); // Call SetWindowLongPtrW to attach or update data associated with a window.
        SetWindowLongPtrW(inst->hToolbarHost, GWLP_HWNDPARENT, (LONG_PTR)inst->hSelf); // Call SetWindowLongPtrW to attach or update data associated with a window.
        SetWindowTextW(inst->hToolbarHost, L"Toolbar"); // Call SetWindowTextW to change the text displayed by a window or control.
        SIZE sz = ToolbarDesiredClientSize(inst); // Declare sz and start with ToolbarDesiredClientSize(inst).
        RECT wr = {0, 0, sz.cx, sz.cy}; // Declare wr and start with {0, 0, sz.cx, sz.cy}.
        AdjustWindowRectEx(&wr, (DWORD)GetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE), FALSE, (DWORD)GetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE)); // Call AdjustWindowRectEx to perform this part of the Notepad operation.
        RECT mr; GetWindowRect(inst->hSelf, &mr); // Declare mr for use by this block.
        SetWindowPos(inst->hToolbarHost, HWND_TOP, mr.left + Dpi(inst, 80), mr.top + Dpi(inst, 80), wr.right-wr.left, wr.bottom-wr.top, SWP_FRAMECHANGED); // Call SetWindowPos to update a window’s position, size, or z-order.
        LayoutToolbarChildren(inst); // Call LayoutToolbarChildren to perform this part of the Notepad operation.
    } else if (!wasChild && willBeChild) { // Define if as the function that implements this part of the panel.
        SetParent(inst->hToolbarHost, inst->hSelf); // Call SetParent to apply the requested state or value.
        SetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE, (GetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE) & ~(LONG_PTR)(WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME)) | WS_CHILD | WS_CLIPCHILDREN); // Call SetWindowLongPtrW to attach or update data associated with a window.
        SetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE, GetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE) & ~(LONG_PTR)WS_EX_TOOLWINDOW); // Call SetWindowLongPtrW to attach or update data associated with a window.
        SetWindowTextW(inst->hToolbarHost, L""); // Call SetWindowTextW to change the text displayed by a window or control.
        SetWindowPos(inst->hToolbarHost, nullptr, 0, 0, 100, 100, SWP_FRAMECHANGED | SWP_NOZORDER); // Call SetWindowPos to update a window’s position, size, or z-order.
    } // Close the current block.
    if (!inst->hostManaged) RegWriteStr(L"ToolbarDock", std::to_wstring((int)inst->tbDock)); // Continue this branch only when inst->host managed evaluates to false.
    if (inst->hSelf) UpdateLayout(inst, inst->hSelf); // Continue this branch only when inst- is greater than hSelf.
    if (inst->hToolbarHost) InvalidateRect(inst->hToolbarHost, nullptr, TRUE); // Continue this branch only when inst- is greater than hToolbarHost.
    if (inst->hSelf) InvalidateRect(inst->hSelf, nullptr, TRUE); // Continue this branch only when inst- is greater than hSelf.
} // Close the current block.

void ShowToolbarMenu(NotepadInstance* inst, HWND owner, int x, int y) { // Define ShowToolbarMenu as the function that implements this part of the panel.
    std::vector<MenuItem> items; // Declare items for use by this block.
    auto add = [&](int id, const wchar_t* label, bool checked = false) { // Declare add and start with [&](int id, const wchar_t* label, bool checked = false) {.
        MenuItem it; it.id = id; it.label = label; it.checked = checked; items.push_back(it); }; // Update the the command or control identifier field with the calculated value.
    auto sep = [&]() { MenuItem it; it.separator = true; items.push_back(it); }; // Declare sep and start with [&]() { MenuItem it; it.separator = true; items.push_back(it); }.
    add(IDM_TB_DOCK_TOP, L"Dock to Top", inst->tbDock == TBDOCK_TOP); // Call add to perform this part of the Notepad operation.
    add(IDM_TB_DOCK_BOTTOM, L"Dock to Bottom", inst->tbDock == TBDOCK_BOTTOM); // Call add to perform this part of the Notepad operation.
    add(IDM_TB_DOCK_LEFT, L"Dock to Left", inst->tbDock == TBDOCK_LEFT); // Call add to perform this part of the Notepad operation.
    add(IDM_TB_DOCK_RIGHT, L"Dock to Right", inst->tbDock == TBDOCK_RIGHT); // Call add to perform this part of the Notepad operation.
    if (!inst->isChild) add(IDM_TB_DOCK_FLOAT, L"Floating", inst->tbDock == TBDOCK_FLOAT); // Continue this branch only when inst->is child evaluates to false.
    sep(); // Call sep to perform this part of the Notepad operation.
    add(IDM_TB_SIZE_SMALL, L"Small icons", inst->tbSize == TBSIZE_SMALL); // Call add to perform this part of the Notepad operation.
    add(IDM_TB_SIZE_MEDIUM, L"Medium icons", inst->tbSize == TBSIZE_MEDIUM); // Call add to perform this part of the Notepad operation.
    add(IDM_TB_SIZE_LARGE, L"Large icons", inst->tbSize == TBSIZE_LARGE); // Call add to perform this part of the Notepad operation.
    // (ShowThemedMenu is defined further down -- we forward to it)
    extern int ShowThemedMenuInst(NotepadInstance*, HWND, int, int, const std::vector<MenuItem>&); // Complete the current operation before proceeding to the next statement.
    int chosen = ShowThemedMenuInst(inst, owner, x, y, items); // Declare chosen and start with ShowThemedMenuInst(inst, owner, x, y, items).
    switch (chosen) { // Choose the handler based on the current message or command.
    case IDM_TB_DOCK_TOP:    SetToolbarDock(inst, TBDOCK_TOP); break; // Handle IDM_TB_DOCK_TOP to process this window message or command.
    case IDM_TB_DOCK_BOTTOM: SetToolbarDock(inst, TBDOCK_BOTTOM); break; // Handle IDM_TB_DOCK_BOTTOM to process this window message or command.
    case IDM_TB_DOCK_LEFT:   SetToolbarDock(inst, TBDOCK_LEFT); break; // Handle IDM_TB_DOCK_LEFT to process this window message or command.
    case IDM_TB_DOCK_RIGHT:  SetToolbarDock(inst, TBDOCK_RIGHT); break; // Handle IDM_TB_DOCK_RIGHT to process this window message or command.
    case IDM_TB_DOCK_FLOAT:  SetToolbarDock(inst, TBDOCK_FLOAT); break; // Handle IDM_TB_DOCK_FLOAT to process this window message or command.
    case IDM_TB_SIZE_SMALL:  SetToolbarSize(inst, TBSIZE_SMALL); break; // Handle IDM_TB_SIZE_SMALL to process this window message or command.
    case IDM_TB_SIZE_MEDIUM: SetToolbarSize(inst, TBSIZE_MEDIUM); break; // Handle IDM_TB_SIZE_MEDIUM to process this window message or command.
    case IDM_TB_SIZE_LARGE:  SetToolbarSize(inst, TBSIZE_LARGE); break; // Handle IDM_TB_SIZE_LARGE to process this window message or command.
    } // Close the current block.
} // Close the current block.

// ============================================================
//  Themed popup menu (instance-aware)
// ============================================================
static int MenuItemAtY(NotepadInstance* inst, int y) { // Define MenuItemAtY as the function that implements this part of the panel.
    if (!inst->menuItems) return -1; // Continue this branch only when inst->menu items evaluates to false.
    int top = inst->menuPadY; // Declare top and start with inst->menuPadY.
    for (size_t i = 0; i < inst->menuItems->size(); ++i) { // Repeat the body using size_t i = 0 and stop when i < inst->menuItems->size().
        const MenuItem& it = (*inst->menuItems)[i]; // Declare it and start with (*inst->menuItems)[i].
        int h = it.separator ? Dpi(inst, 9) : inst->menuItemH; // Declare h and start with it.separator ? Dpi(inst, 9) : inst->menuItemH.
        if (y >= top && y < top + h) return (int)i; // Continue this branch only when y is at least top and y is less than top + h.
        top += h; // Update + with the result of this expression.
    } // Close the current block.
    return -1; // Return the computed -1 to the caller.
} // Close the current block.

LRESULT CALLBACK MenuProc(HWND h, UINT m, WPARAM w, LPARAM l) { // Define MenuProc as the function that implements this part of the panel.
    NotepadInstance* inst = reinterpret_cast<NotepadInstance*>(GetWindowLongPtrW(h, GWLP_USERDATA)); // Declare the state object for this Notepad panel and start with reinterpret_cast<NotepadInstance*>(GetWindowLongPtrW(h, GWLP_USERDATA)).
    switch (m) { // Choose the handler based on the current message or command.
    case WM_ERASEBKGND: return 1; // Handle WM_ERASEBKGND to avoid flicker during custom painting.
    case WM_PAINT: { // Handle WM_PAINT to draw the panel surface.
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps); // Update hdc with the result of this expression.
        RECT rc; GetClientRect(h, &rc); // Declare the rectangle used to measure a window’s client area for use by this block.
        if (!inst) { EndPaint(h, &ps); return 0; } // Continue this branch only when the state object for this Notepad panel is unavailable.
        FillRoundRect(hdc, rc, Dpi(inst, 8), inst->theme.BgCard); // Call FillRoundRect to render the requested interface element.
        StrokeRoundRect(hdc, rc, Dpi(inst, 8), inst->theme.Border); // Call StrokeRoundRect to perform this part of the Notepad operation.
        if (!inst->menuItems) { EndPaint(h, &ps); return 0; } // Continue this branch only when inst->menu items evaluates to false.
        int top = inst->menuPadY; // Declare top and start with inst->menuPadY.
        for (size_t i = 0; i < inst->menuItems->size(); ++i) { // Repeat the body using size_t i = 0 and stop when i < inst->menuItems->size().
            const MenuItem& it = (*inst->menuItems)[i]; // Declare it and start with (*inst->menuItems)[i].
            if (it.separator) { // Continue this branch only when it.separator is true.
                int y = top + Dpi(inst, 4); // Declare y and start with top + Dpi(inst, 4).
                RECT sep = { rc.left + Dpi(inst, 10), y, rc.right - Dpi(inst, 10), y + 1 }; // Declare sep and start with { rc.left + Dpi(inst, 10), y, rc.right - Dpi(inst, 10), y + 1 }.
                FillSolid(hdc, sep, inst->theme.BorderSoft); // Call FillSolid to render the requested interface element.
                top += Dpi(inst, 9); continue; // Update + with the result of this expression.
            } // Close the current block.
            RECT row = { rc.left + Dpi(inst, 4), top, rc.right - Dpi(inst, 4), top + inst->menuItemH }; // Declare row and start with { rc.left + Dpi(inst, 4), top, rc.right - Dpi(inst, 4), top + inst->menuItemH }.
            bool hot = ((int)i == inst->menuHot) && !it.disabled; // Complete the current operation before proceeding to the next statement.
            if (hot) FillRoundRect(hdc, row, Dpi(inst, 6), inst->theme.BgRailHi); // Continue this branch only when hot is true.
            COLORREF fg = it.disabled ? inst->theme.TextLow : inst->theme.TextHi; // Declare fg and start with it.disabled ? inst->theme.TextLow : inst->theme.TextHi.
            if (it.checked) { // Continue this branch only when it.checked is true.
                RECT ir = { row.left + Dpi(inst, 8), row.top + Dpi(inst, 6), row.left + Dpi(inst, 22), row.bottom - Dpi(inst, 6) }; // Declare ir and start with { row.left + Dpi(inst, 8), row.top + Dpi(inst, 6), row.left + Dpi(inst, 22), row.bottom - Dpi(inst, 6) }.
                DrawIcon(hdc, inst, Icon::Check, ir, inst->theme.Accent, 2); // Call DrawIcon to render the requested interface element.
            } // Close the current block.
            HFONT of = (HFONT)SelectObject(hdc, inst->font); // Declare of and start with (HFONT)SelectObject(hdc, inst->font).
            SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, fg); // Call SetBkMode to apply the requested state or value.
            RECT tr = { row.left + Dpi(inst, 28), row.top, row.right - Dpi(inst, 80), row.bottom }; // Declare tr and start with { row.left + Dpi(inst, 28), row.top, row.right - Dpi(inst, 80), row.bottom }.
            DrawTextW(hdc, it.label.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS); // Call DrawTextW to draw text within the requested rectangle.
            if (!it.shortcut.empty()) { // Continue this branch only when it.shortcut contains at least one item.
                SetTextColor(hdc, inst->theme.TextLow); // Call SetTextColor to apply the requested state or value.
                HFONT os = (HFONT)SelectObject(hdc, inst->fontSmall); // Declare os and start with (HFONT)SelectObject(hdc, inst->fontSmall).
                RECT sr = { row.right - Dpi(inst, 80), row.top, row.right - Dpi(inst, 12), row.bottom }; // Declare sr and start with { row.right - Dpi(inst, 80), row.top, row.right - Dpi(inst, 12), row.bottom }.
                DrawTextW(hdc, it.shortcut.c_str(), -1, &sr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE); // Call DrawTextW to draw text within the requested rectangle.
                SelectObject(hdc, os); // Call SelectObject to select a GDI object into the drawing context.
            } // Close the current block.
            SelectObject(hdc, of); // Call SelectObject to select a GDI object into the drawing context.
            top += inst->menuItemH; // Update + with the result of this expression.
        } // Close the current block.
        EndPaint(h, &ps); return 0; // Call EndPaint to finish the current window painting operation.
    } // Close the current block.
    case WM_MOUSEMOVE: { // Handle WM_MOUSEMOVE to update hover feedback.
        int idx = MenuItemAtY(inst, GET_Y_LPARAM(l)); // Declare idx and start with MenuItemAtY(inst, GET_Y_LPARAM(l)).
        if (idx >= 0 && inst->menuItems && (*inst->menuItems)[idx].separator) idx = -1; // Continue this branch only when idx is at least 0 and inst- is greater than menuItems and (*inst- is greater than menuItems)[idx].separator.
        if (idx != inst->menuHot) { inst->menuHot = idx; InvalidateRect(h, nullptr, FALSE); } // Continue this branch only when idx is different from inst->menuHot.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.
    case WM_LBUTTONDOWN: { // Handle WM_LBUTTONDOWN to begin the left-button interaction.
        int idx = MenuItemAtY(inst, GET_Y_LPARAM(l)); // Declare idx and start with MenuItemAtY(inst, GET_Y_LPARAM(l)).
        if (idx >= 0 && inst->menuItems) { // Continue this branch only when idx is at least 0 and inst- is greater than menuItems.
            const MenuItem& it = (*inst->menuItems)[idx]; // Declare it and start with (*inst->menuItems)[idx].
            if (!it.separator && !it.disabled) { // Continue this branch only when it.separator && !it.disabled evaluates to false.
                inst->menuResult = it.id; inst->menuDone = true; // Store the new value in the the selected command returned by the custom menu field of the current instance.
                PostMessageW(h, WM_NULL, 0, 0); // Call PostMessageW to queue a message for the target window to handle later.
            } // Close the current block.
        } // Close the current block.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.
    case WM_LBUTTONUP: return 0; // Handle WM_LBUTTONUP to finish the left-button interaction.
    case WM_KILLFOCUS: inst->menuDone = true; PostMessageW(h, WM_NULL, 0, 0); return 0; // Handle WM_KILLFOCUS to react to losing keyboard focus.
    case WM_KEYDOWN: // Handle WM_KEYDOWN to handle keyboard shortcuts.
        if (w == VK_ESCAPE) { inst->menuResult = 0; inst->menuDone = true; PostMessageW(h, WM_NULL, 0, 0); return 0; } // Continue this branch only when w is equal to VK_ESCAPE.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    case WM_CLOSE: inst->menuResult = 0; inst->menuDone = true; PostMessageW(h, WM_NULL, 0, 0); return 0; // Handle WM_CLOSE to close the editor and frame.
    } // Close the current block.
    return DefWindowProcW(h, m, w, l); // Let Win32 perform its default handling for this unhandled message.
} // Close the current block.

static void EnsureMenuClass() { // Define EnsureMenuClass as the function that implements this part of the panel.
    static bool registered = false; // Declare registered and start disabled.
    if (registered) return; // Continue this branch only when registered is true.
    WNDCLASSW wc{}; wc.lpfnWndProc = MenuProc; // Update the lpfn wnd proc field with the calculated value.
    wc.hInstance = g_hInst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); // Update the the executable instance handle supplied by Windows field with the calculated value.
    wc.lpszClassName = MENU_CLASS; RegisterClassW(&wc); // Update the lpsz class name field with the calculated value.
    registered = true; // Update registered with the result of this expression.
} // Close the current block.

int ShowThemedMenuInst(NotepadInstance* inst, HWND owner, int x, int y, // Supply this parameter or list item to the enclosing call.
                       const std::vector<MenuItem>& items) { // Continue the current declaration or expression.
    EnsureMenuClass(); // Call EnsureMenuClass to perform this part of the Notepad operation.
    inst->menuItems = new std::vector<MenuItem>(items); // Store the new value in the the items currently displayed in the custom menu field of the current instance.
    inst->menuHot = -1; inst->menuResult = 0; inst->menuDone = false; // Store the new value in the the menu item currently under the pointer field of the current instance.
    inst->menuItemH = Dpi(inst, 32); inst->menuPadY = Dpi(inst, 6); // Store the new value in the menu item h field of the current instance.
    HDC hdc = GetDC(nullptr); // Declare hdc and start with GetDC(nullptr).
    HFONT of = (HFONT)SelectObject(hdc, inst->font); // Declare of and start with (HFONT)SelectObject(hdc, inst->font).
    int maxW = Dpi(inst, 160); // Declare max w and start with Dpi(inst, 160).
    for (const auto& it : items) { // Visit each item in the collection and apply the loop body.
        if (it.separator) continue; // Continue this branch only when it.separator is true.
        SIZE sz{}; GetTextExtentPoint32W(hdc, it.label.c_str(), (int)it.label.size(), &sz); // Complete the current operation before proceeding to the next statement.
        int need = sz.cx + Dpi(inst, 28 + 80 + 32); // Declare need and start with sz.cx + Dpi(inst, 28 + 80 + 32).
        if (need > maxW) maxW = need; // Continue this branch only when need is greater than maxW.
    } // Close the current block.
    SelectObject(hdc, of); ReleaseDC(nullptr, hdc); // Call SelectObject to select a GDI object into the drawing context.
    int totalH = inst->menuPadY * 2; // Declare total h and start with inst->menuPadY * 2.
    for (const auto& it : items) totalH += it.separator ? Dpi(inst, 9) : inst->menuItemH; // Visit each item in the collection and apply the loop body.
    RECT wa{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0); // Complete the current operation before proceeding to the next statement.
    if (x + maxW > wa.right)   x = wa.right - maxW; // Continue this branch only when x + max w is greater than wa.right.
    if (y + totalH > wa.bottom) y = wa.bottom - totalH; // Continue this branch only when y + total h is greater than wa.bottom.
    if (x < wa.left) x = wa.left; if (y < wa.top) y = wa.top; // Continue this branch only when x is less than wa.left.
    inst->menuWnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, // Store the new value in the the popup window used to draw the custom menu field of the current instance.
        MENU_CLASS, L"", WS_POPUP, x, y, maxW, totalH, owner, nullptr, g_hInst, nullptr); // Complete the current operation before proceeding to the next statement.
    if (!inst->menuWnd) { delete inst->menuItems; inst->menuItems = nullptr; return 0; } // Continue this branch only when inst->menu wnd evaluates to false.
    SetWindowLongPtrW(inst->menuWnd, GWLP_USERDATA, (LONG_PTR)inst); // Call SetWindowLongPtrW to attach or update data associated with a window.
    ShowWindow(inst->menuWnd, SW_SHOW); // Call ShowWindow to show the frame using the selected display state.
    SetForegroundWindow(inst->menuWnd); // Call SetForegroundWindow to apply the requested state or value.
    SetFocus(inst->menuWnd); // Call SetFocus to move keyboard focus to the requested control.
    MSG msg; // Declare the Win32 message currently being dispatched for use by this block.
    while (!inst->menuDone && GetMessageW(&msg, nullptr, 0, 0)) { // Repeat the body while inst->menu done && get message w(&msg, nullptr, 0, 0) evaluates to false.
        TranslateMessage(&msg); DispatchMessageW(&msg); // Call TranslateMessage to translate keyboard messages into character messages.
    } // Close the current block.
    DestroyWindow(inst->menuWnd); // Call DestroyWindow to release the requested resource.
    inst->menuWnd = nullptr; // Store the new value in the the popup window used to draw the custom menu field of the current instance.
    delete inst->menuItems; inst->menuItems = nullptr; // Free the object allocated earlier after it is no longer needed.
    return inst->menuResult; // Return the computed inst->menu result to the caller.
} // Close the current block.

// ============================================================
//  Owner-draw buttons (instance-aware)
// ============================================================
static bool ButtonStyleFor(NotepadInstance* inst, int id, BtnStyle& st) { // Define ButtonStyleFor as the function that implements this part of the panel.
    switch (id) { // Choose the handler based on the current message or command.
    case IDC_TB_MENU:     st = {Icon::Menu, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_TB_MENU to process this window message or command.
    case IDC_TB_NEW:      st = {Icon::New, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_TB_NEW to process this window message or command.
    case IDC_TB_OPEN:     st = {Icon::Open, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_TB_OPEN to process this window message or command.
    case IDC_TB_SAVE:     st = {Icon::Save, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_TB_SAVE to process this window message or command.
    case IDC_TB_UNDO:     st = {Icon::Undo, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_TB_UNDO to process this window message or command.
    case IDC_TB_REDO:     st = {Icon::Redo, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_TB_REDO to process this window message or command.
    case IDC_TB_FIND:     st = {Icon::Find, false, true, inst->searchOpen, inst->theme.IconIdle}; return true; // Handle IDC_TB_FIND to process this window message or command.
    case IDC_TB_ZOOM_IN:  st = {Icon::ZoomIn, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_TB_ZOOM_IN to process this window message or command.
    case IDC_TB_ZOOM_OUT: st = {Icon::ZoomOut, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_TB_ZOOM_OUT to process this window message or command.
    case IDC_TB_WRAP:     st = {Icon::Wrap, false, true, inst->wordWrap, inst->theme.IconIdle}; return true; // Handle IDC_TB_WRAP to process this window message or command.
    case IDC_SEARCH_NEXT:     st = {Icon::Next, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_SEARCH_NEXT to process this window message or command.
    case IDC_SEARCH_PREV:     st = {Icon::Prev, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_SEARCH_PREV to process this window message or command.
    case IDC_SEARCH_CLOSE:    st = {Icon::Close, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_SEARCH_CLOSE to process this window message or command.
    case IDC_SEARCH_CASE:     st = {Icon::Case, false, true, inst->searchCase, inst->theme.IconIdle}; return true; // Handle IDC_SEARCH_CASE to process this window message or command.
    case IDC_SEARCH_REPL_ONE: st = {Icon::Replace, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_SEARCH_REPL_ONE to process this window message or command.
    case IDC_SEARCH_REPL_ALL: st = {Icon::ReplaceAll, false, true, false, inst->theme.IconIdle}; return true; // Handle IDC_SEARCH_REPL_ALL to process this window message or command.
    } // Close the current block.
    return false; // Return the computed false to the caller.
} // Close the current block.

static void DrawButton(NotepadInstance* inst, const DRAWITEMSTRUCT* d) { // Define DrawButton as the function that implements this part of the panel.
    BtnStyle st{}; // Complete the current operation before proceeding to the next statement.
    if (!ButtonStyleFor(inst, (int)d->CtlID, st)) { FillSolid(d->hDC, d->rcItem, inst->theme.BgApp); return; } // Continue this branch only when button style for(inst, (int)d->ctl id, st) evaluates to false.
    RECT rc = d->rcItem; // Declare the rectangle used to measure a window’s client area and start with d->rcItem.
    bool down = (d->itemState & ODS_SELECTED) != 0; // Complete the current operation before proceeding to the next statement.
    bool hot = !down && (d->hwndItem == inst->hotBtn); // Complete the current operation before proceeding to the next statement.
    bool disabled = (d->itemState & ODS_DISABLED) != 0; // Complete the current operation before proceeding to the next statement.
    // Read-only disables editing buttons
    if (inst->readOnly) { // Continue this branch only when inst- is greater than readOnly.
        int id = (int)d->CtlID; // Declare the command or control identifier and start with (int)d->CtlID.
        if (id == IDC_TB_UNDO || id == IDC_TB_REDO) disabled = true; // Continue this branch only when the command or control identifier is equal to IDC_TB_UNDO or the command or control identifier is equal to IDC_TB_REDO.
    } // Close the current block.
    COLORREF parentBg = inst->theme.BgApp; // Declare parent bg and start with inst->theme.BgApp.
    bool onPanel = (d->CtlID >= IDC_SEARCH_NEXT && d->CtlID <= IDC_SEARCH_REPL_ALL) || // Continue the current declaration or expression.
                   (d->CtlID >= IDC_TB_MENU && d->CtlID <= IDC_TB_WRAP); // Complete the current operation before proceeding to the next statement.
    if (onPanel) parentBg = inst->theme.BgPanel; // Continue this branch only when on panel is true.
    FillSolid(d->hDC, rc, parentBg); // Call FillSolid to render the requested interface element.
    COLORREF fg = st.fg; // Declare fg and start with st.fg.
    if (disabled)       fg = inst->theme.TextLow; // Continue this branch only when whether the menu item cannot be selected is true.
    else if (st.active) fg = inst->theme.IconActive; // Continue this branch only when st.active is true.
    else if (hot)       fg = inst->theme.IconActive; // Continue this branch only when hot is true.
    bool drawBg = hot || st.active || down; // Declare draw bg and start with hot || st.active || down.
    if (st.chipBg)     FillRoundRect(d->hDC, rc, Dpi(inst, 10), down ? inst->theme.BgBtnHi : inst->theme.BgBtn); // Continue this branch only when st.chip bg is true.
    else if (st.rail)  { if (drawBg) FillRoundRect(d->hDC, rc, Dpi(inst, 10), down ? inst->theme.BgBtnHi : inst->theme.BgRailHi); } // Continue this branch only when st.rail is true.
    else               { if (drawBg) FillRoundRect(d->hDC, rc, Dpi(inst, 8),  down ? inst->theme.BgBtnHi : inst->theme.BgRailHi); } // Run this fallback branch when the previous condition did not match.
    int minDim = (std::min)(rc.right - rc.left, rc.bottom - rc.top); // Declare min dim and start with (std::min)(rc.right - rc.left, rc.bottom - rc.top).
    int inset = minDim / 4; // Declare inset and start with minDim / 4.
    RECT ir = { rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset }; // Declare ir and start with { rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset }.
    DrawIcon(d->hDC, inst, st.icon, ir, fg, 2); // Call DrawIcon to render the requested interface element.
} // Close the current block.

static void UpdateHotButton(NotepadInstance* inst, HWND parent, LPARAM lParam) { // Define UpdateHotButton as the function that implements this part of the panel.
    POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; // Declare p and start with { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }.
    HWND child = ChildWindowFromPointEx(parent, p, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT); // Declare child and start with ChildWindowFromPointEx(parent, p, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT).
    if (child == parent) child = nullptr; // Continue this branch only when child is equal to parent.
    if (child != inst->hotBtn) { // Continue this branch only when child is different from inst->hotBtn.
        HWND old = inst->hotBtn; inst->hotBtn = child; // Declare old and start with inst->hotBtn; inst->hotBtn = child.
        if (old)   InvalidateRect(old, nullptr, TRUE); // Continue this branch only when old is true.
        if (child) InvalidateRect(child, nullptr, TRUE); // Continue this branch only when child is true.
    } // Close the current block.
} // Close the current block.

static HWND MakeButton(NotepadInstance* inst, HWND parent, int id, const wchar_t* text = L"") { // Update text with the result of this expression.
    HWND h = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, // Declare h and start with CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,.
        0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, inst->hInst, nullptr); // Complete the current operation before proceeding to the next statement.
    SendMessageW(h, WM_SETFONT, (WPARAM)inst->font, TRUE); // Call SendMessageW to send a control message and receive its result immediately.
    return h; // Return the computed h to the caller.
} // Close the current block.

// ============================================================
//  Toolbar button subclass
// ============================================================
LRESULT CALLBACK ToolbarBtnProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) { // Define ToolbarBtnProc as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(GetParent(h)); // Declare the state object for this Notepad panel and start with GetInstance(GetParent(h)).
    if (!inst) { /* toolbar host may be the parent, try grandparent */ } // Continue this branch only when the state object for this Notepad panel is unavailable.
    switch (m) { // Choose the handler based on the current message or command.
    case WM_MOUSEMOVE: { // Handle WM_MOUSEMOVE to update hover feedback.
        if (inst && inst->hotBtn != h) { // Continue this branch only when the state object for this Notepad panel is true and inst- is greater than hotBtn != h.
            HWND old = inst->hotBtn; inst->hotBtn = h; // Declare old and start with inst->hotBtn; inst->hotBtn = h.
            if (old) InvalidateRect(old, nullptr, TRUE); // Continue this branch only when old is true.
            InvalidateRect(h, nullptr, TRUE); // Call InvalidateRect to mark the requested area for repainting.
        } // Close the current block.
        TRACKMOUSEEVENT tme{}; tme.cbSize = sizeof(tme); tme.dwFlags = TME_LEAVE; tme.hwndTrack = h; // Update the cb size field with the calculated value.
        TrackMouseEvent(&tme); return 0; // Call TrackMouseEvent to request a notification when the pointer leaves the control.
    } // Close the current block.
    case WM_MOUSELEAVE: // Handle WM_MOUSELEAVE to clear hover feedback.
        if (inst && inst->hotBtn == h) { inst->hotBtn = nullptr; InvalidateRect(h, nullptr, TRUE); } // Continue this branch only when the state object for this Notepad panel is true and inst- is greater than hotBtn == h.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    case WM_RBUTTONUP: { // Handle WM_RBUTTONUP to show the context menu.
        POINT pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) }; ClientToScreen(h, &pt); // Declare pt and start with { GET_X_LPARAM(l), GET_Y_LPARAM(l) }; ClientToScreen(h, &pt).
        if (inst) ShowToolbarMenu(inst, inst->hSelf, pt.x, pt.y); // Continue this branch only when the state object for this Notepad panel is true.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.
    case WM_CONTEXTMENU: { // Handle WM_CONTEXTMENU to process this window message or command.
        int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l); // Declare x and start with GET_X_LPARAM(l), y = GET_Y_LPARAM(l).
        if (x == -1 && y == -1) { RECT br; GetWindowRect(h, &br); x = br.left + 10; y = br.top + 10; } // Continue this branch only when x is equal to -1 and y is equal to -1.
        if (inst) ShowToolbarMenu(inst, inst->hSelf, x, y); // Continue this branch only when the state object for this Notepad panel is true.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.
    } // Close the current block.
    return DefSubclassProc(h, m, w, l); // Return the computed def subclass proc(h, m, w, l) to the caller.
} // Close the current block.

LRESULT CALLBACK ToolbarHostProc(HWND h, UINT m, WPARAM w, LPARAM l) { // Define ToolbarHostProc as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(GetParent(h)); // Declare the state object for this Notepad panel and start with GetInstance(GetParent(h)).
    switch (m) { // Choose the handler based on the current message or command.
    case WM_ERASEBKGND: return 1; // Handle WM_ERASEBKGND to avoid flicker during custom painting.
    case WM_PAINT: { // Handle WM_PAINT to draw the panel surface.
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps); // Update hdc with the result of this expression.
        RECT rc; GetClientRect(h, &rc); // Declare the rectangle used to measure a window’s client area for use by this block.
        if (inst) { // Continue this branch only when the state object for this Notepad panel is true.
            FillSolid(hdc, rc, inst->theme.BgPanel); // Call FillSolid to render the requested interface element.
            RECT sep = rc; // Declare sep and start with rc.
            switch (inst->tbDock) { // Choose the handler based on the current message or command.
            case TBDOCK_TOP:    sep.top = rc.bottom - 1; break; // Handle TBDOCK_TOP to process this window message or command.
            case TBDOCK_BOTTOM: sep.bottom = rc.top + 1; break; // Handle TBDOCK_BOTTOM to process this window message or command.
            case TBDOCK_LEFT:   sep.left = rc.right - 1; break; // Handle TBDOCK_LEFT to process this window message or command.
            case TBDOCK_RIGHT:  sep.right = rc.left + 1; break; // Handle TBDOCK_RIGHT to process this window message or command.
            default:            sep.top = rc.bottom - 1; break; // Handle any switch value not covered by the named cases.
            } // Close the current block.
            FillSolid(hdc, sep, inst->theme.BorderSoft); // Call FillSolid to render the requested interface element.
        } // Close the current block.
        EndPaint(h, &ps); return 0; // Call EndPaint to finish the current window painting operation.
    } // Close the current block.
    case WM_MOUSEMOVE: { // Handle WM_MOUSEMOVE to update hover feedback.
        if (inst) UpdateHotButton(inst, h, l); // Continue this branch only when the state object for this Notepad panel is true.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.
    case WM_CONTEXTMENU: { // Handle WM_CONTEXTMENU to process this window message or command.
        int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l); // Declare x and start with GET_X_LPARAM(l), y = GET_Y_LPARAM(l).
        if (x == -1 && y == -1) { RECT br; GetWindowRect(h, &br); x = br.left + 20; y = br.top + 20; } // Continue this branch only when x is equal to -1 and y is equal to -1.
        if (inst) ShowToolbarMenu(inst, inst->hSelf, x, y); // Continue this branch only when the state object for this Notepad panel is true.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.
    case WM_CLOSE: // Handle WM_CLOSE to close the editor and frame.
        if (inst && inst->tbDock == TBDOCK_FLOAT) { SetToolbarDock(inst, TBDOCK_TOP); return 0; } // Continue this branch only when the state object for this Notepad panel is true and inst- is greater than tbDock == TBDOCK_FLOAT.
        break; // Stop processing the current switch case or loop.
    case WM_COMMAND: case WM_DRAWITEM: case WM_NOTIFY: // Handle WM_COMMAND to dispatch a user command.
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORBTN: // Handle WM_CTLCOLORSTATIC to process this window message or command.
        if (inst && inst->hSelf) return SendMessageW(inst->hSelf, m, w, l); // Continue this branch only when the state object for this Notepad panel is true and inst- is greater than hSelf.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    case WM_SETFOCUS: // Handle WM_SETFOCUS to restore keyboard focus.
        if (inst && inst->hSelf) SetFocus(inst->hSelf); // Continue this branch only when the state object for this Notepad panel is true and inst- is greater than hSelf.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.
    return DefWindowProcW(h, m, w, l); // Let Win32 perform its default handling for this unhandled message.
} // Close the current block.

// ============================================================
//  Search visibility
// ============================================================
void ShowSearch(NotepadInstance* inst, bool open, bool withReplace) { // Define ShowSearch as the function that implements this part of the panel.
    inst->searchOpen = open; inst->replaceOpen = open && withReplace; // Store the new value in the search open field of the current instance.
    int cmdShow = open ? SW_SHOW : SW_HIDE; // Declare cmd show and start with open ? SW_SHOW : SW_HIDE.
    HWND firstRow[5] = { inst->hSearchFind, inst->hSearchNext, inst->hSearchPrev, inst->hSearchClose, inst->hSearchCase }; // Declare first row and start with { inst->hSearchFind, inst->hSearchNext, inst->hSearchPrev, inst->hSearchClose, inst->hSearchCase }.
    for (HWND h : firstRow) if (h) ShowWindow(h, cmdShow); // Visit each item in the collection and apply the loop body.
    HWND secondRow[3] = { inst->hSearchRepl, inst->hSearchReplOne, inst->hSearchReplAll }; // Declare second row and start with { inst->hSearchRepl, inst->hSearchReplOne, inst->hSearchReplAll }.
    for (HWND h : secondRow) if (h) ShowWindow(h, (open && withReplace) ? SW_SHOW : SW_HIDE); // Visit each item in the collection and apply the loop body.
    if (inst->hTbBtns[6]) InvalidateRect(inst->hTbBtns[6], nullptr, TRUE); // Continue this branch only when inst- is greater than hTbBtns[6].
    UpdateLayout(inst, inst->hSelf); // Call UpdateLayout to apply the requested state or value.
    InvalidateRect(inst->hSelf, nullptr, TRUE); // Call InvalidateRect to mark the requested area for repainting.
    if (open) { // Continue this branch only when open is true.
        SetFocus(inst->hSearchFind); // Call SetFocus to move keyboard focus to the requested control.
        DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0); // Declare sel and start with (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0).
        int s = LOWORD(sel), e = HIWORD(sel); // Declare s and start with LOWORD(sel), e = HIWORD(sel).
        if (e > s && (e - s) < 128) { // Continue this branch only when e is greater than s and (e - s) is less than 128.
            std::wstring cur(e - s, 0); // Complete the current operation before proceeding to the next statement.
            TEXTRANGEW tr{}; tr.chrg.cpMin = s; tr.chrg.cpMax = e; tr.lpstrText = cur.data(); // Update the cp min field with the calculated value.
            SendMessageW(inst->hEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr); // Call SendMessageW to send a control message and receive its result immediately.
            if (!cur.empty() && cur.find(L"\r\n") == std::wstring::npos && cur.find(L'\r') == std::wstring::npos) { // Continue this branch only when cur.empty() && cur.find(l'\r\n') == std::wstring::npos && cur.find(l'\r') == std::wstring::npos evaluates to false.
                SetWindowTextW(inst->hSearchFind, cur.c_str()); // Call SetWindowTextW to change the text displayed by a window or control.
                SendMessageW(inst->hSearchFind, EM_SETSEL, 0, -1); // Call SendMessageW to send a control message and receive its result immediately.
            } // Close the current block.
        } // Close the current block.
    } else SetFocus(inst->hEdit); // Complete the current operation before proceeding to the next statement.
} // Close the current block.

// ============================================================
//  Tab context menu
// ============================================================
void ShowTabMenu(NotepadInstance* inst, int index, int x, int y) { // Define ShowTabMenu as the function that implements this part of the panel.
    std::vector<MenuItem> items; // Declare items for use by this block.
    auto add = [&](int id, const wchar_t* label, const wchar_t* sc = L"", bool disabled = false) { // Declare add and start with [&](int id, const wchar_t* label, const wchar_t* sc = L"", bool disabled = false) {.
        MenuItem it; it.id = id; it.label = label; it.shortcut = sc; it.disabled = disabled; items.push_back(it); }; // Update the the command or control identifier field with the calculated value.
    auto sep = [&]() { MenuItem it; it.separator = true; items.push_back(it); }; // Declare sep and start with [&]() { MenuItem it; it.separator = true; items.push_back(it); }.
    bool hasTab = (index >= 0 && index < (int)inst->docs.size()); // Complete the current operation before proceeding to the next statement.
    bool hasFile = hasTab && !inst->docs[index]->filePath.empty(); // Declare has file and start with hasTab && !inst->docs[index]->filePath.empty().
    add(IDM_TAB_RENAME, L"Rename...", L"F2", !hasTab); sep(); // Call add to perform this part of the Notepad operation.
    add(IDM_TAB_CLOSE, L"Close", L"Ctrl+W", !hasTab); // Call add to perform this part of the Notepad operation.
    add(IDM_TAB_CLOSE_OTHERS, L"Close others", L"", !hasTab || inst->docs.size() <= 1); // Call add to perform this part of the Notepad operation.
    add(IDM_TAB_CLOSE_RIGHT, L"Close to the right", L"", !hasTab || index >= (int)inst->docs.size() - 1); // Call add to perform this part of the Notepad operation.
    add(IDM_TAB_CLOSE_ALL, L"Close all", L""); sep(); // Call add to perform this part of the Notepad operation.
    add(IDM_TAB_SAVE_AS, L"Save as...", L"Ctrl+Shift+S", !hasTab); // Call add to perform this part of the Notepad operation.
    add(IDM_TAB_REVEAL, L"Reveal in Explorer", L"", !hasFile); // Call add to perform this part of the Notepad operation.
    int chosen = ShowThemedMenuInst(inst, inst->hSelf, x, y, items); // Declare chosen and start with ShowThemedMenuInst(inst, inst->hSelf, x, y, items).
    switch (chosen) { // Choose the handler based on the current message or command.
    case IDM_TAB_RENAME: BeginRenameTab(inst, index); break; // Handle IDM_TAB_RENAME to process this window message or command.
    case IDM_TAB_CLOSE: CloseDoc(inst, index); break; // Handle IDM_TAB_CLOSE to process this window message or command.
    case IDM_TAB_CLOSE_OTHERS: { // Handle IDM_TAB_CLOSE_OTHERS to process this window message or command.
        if (!hasTab) break; // Continue this branch only when has tab evaluates to false.
        for (int i = (int)inst->docs.size() - 1; i >= 0; --i) { // Repeat the body using int i = (int)inst->docs.size() - 1 and stop when i >= 0.
            if (i == index) continue; // Continue this branch only when i is equal to index.
            if (!CloseDoc(inst, i)) break; // Continue this branch only when close doc(inst, i) evaluates to false.
            if (i < index) index--; // Continue this branch only when i is less than index.
        } // Close the current block.
        SetActiveDoc(inst, index); // Call SetActiveDoc to apply the requested state or value.
        break; // Stop processing the current switch case or loop.
    } // Close the current block.
    case IDM_TAB_CLOSE_RIGHT: { // Handle IDM_TAB_CLOSE_RIGHT to process this window message or command.
        if (!hasTab) break; // Continue this branch only when has tab evaluates to false.
        for (int i = (int)inst->docs.size() - 1; i > index; --i) // Repeat the body using int i = (int)inst->docs.size() - 1 and stop when i > index.
            if (!CloseDoc(inst, i)) break; // Continue this branch only when close doc(inst, i) evaluates to false.
        break; // Stop processing the current switch case or loop.
    } // Close the current block.
    case IDM_TAB_CLOSE_ALL: CloseAllDocs(inst); break; // Handle IDM_TAB_CLOSE_ALL to process this window message or command.
    case IDM_TAB_SAVE_AS: SetActiveDoc(inst, index); DoSave(inst, true); break; // Handle IDM_TAB_SAVE_AS to process this window message or command.
    case IDM_TAB_REVEAL: { // Handle IDM_TAB_REVEAL to process this window message or command.
        if (!hasFile) break; // Continue this branch only when has file evaluates to false.
        Document* d = inst->docs[index]; // Declare d and start with inst->docs[index].
        std::wstring arg = L"/select,\"" + d->filePath + L"\""; // Declare arg and start with L"/select,\"" + d->filePath + L"\"".
        ShellExecuteW(inst->isChild ? inst->hParent : inst->hSelf, // Call ShellExecuteW to open the file or location through the Windows shell.
            L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL); // Complete the current operation before proceeding to the next statement.
        break; // Stop processing the current switch case or loop.
    } // Close the current block.
    } // Close the current block.
} // Close the current block.

// ============================================================
//  Command dispatch
// ============================================================
void DispatchCommand(NotepadInstance* inst, HWND hWnd, int cmd) { // Define DispatchCommand as the function that implements this part of the panel.
    switch (cmd) { // Choose the handler based on the current message or command.
    case IDM_NEW:    DoNewTab(inst); return; // Handle IDM_NEW to process this window message or command.
    case IDM_OPEN:   DoOpen(inst); return; // Handle IDM_OPEN to process this window message or command.
    case IDM_SAVE:   DoSave(inst, false); return; // Handle IDM_SAVE to process this window message or command.
    case IDM_SAVEAS: DoSave(inst, true); return; // Handle IDM_SAVEAS to process this window message or command.
    case IDM_EXIT:   SendMessageW(hWnd, WM_CLOSE, 0, 0); return; // Handle IDM_EXIT to process this window message or command.
    case IDM_UNDO:   if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, EM_UNDO, 0, 0); return; // Handle IDM_UNDO to process this window message or command.
    case IDM_REDO:   if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, EM_REDO, 0, 0); return; // Handle IDM_REDO to process this window message or command.
    case IDM_CUT:    if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, WM_CUT, 0, 0); return; // Handle IDM_CUT to process this window message or command.
    case IDM_COPY:   if (inst->hEdit) SendMessageW(inst->hEdit, WM_COPY, 0, 0); return; // Handle IDM_COPY to process this window message or command.
    case IDM_PASTE:  if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, WM_PASTE, 0, 0); return; // Handle IDM_PASTE to process this window message or command.
    case IDM_DELETE: if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, EM_REPLACESEL, TRUE, (LPARAM)L""); return; // Handle IDM_DELETE to process this window message or command.
    case IDM_SELECTALL: if (inst->hEdit) SendMessageW(inst->hEdit, EM_SETSEL, 0, -1); return; // Handle IDM_SELECTALL to process this window message or command.
    case IDM_DATETIME: { // Handle IDM_DATETIME to process this window message or command.
        if (inst->hEdit && !inst->readOnly) { // Continue this branch only when inst- is greater than hEdit and inst->read only evaluates to false.
            SYSTEMTIME st; GetLocalTime(&st); // Declare st for use by this block.
            wchar_t t[64]; // Declare t for use by this block.
            wsprintfW(t, L"%d:%02d %s %d/%d/%d", // Call wsprintfW to perform this part of the Notepad operation.
                (st.wHour % 12) ? (st.wHour % 12) : 12, st.wMinute, // Supply this parameter or list item to the enclosing call.
                st.wHour < 12 ? L"AM" : L"PM", st.wMonth, st.wDay, st.wYear); // Complete the current operation before proceeding to the next statement.
            SendMessageW(inst->hEdit, EM_REPLACESEL, TRUE, (LPARAM)t); // Call SendMessageW to send a control message and receive its result immediately.
        } // Close the current block.
        return; // Return the computed  to the caller.
    } // Close the current block.
    case IDM_FIND:     ShowSearch(inst, true, false); return; // Handle IDM_FIND to process this window message or command.
    case IDM_REPLACE:  ShowSearch(inst, true, true); return; // Handle IDM_REPLACE to process this window message or command.
    case IDM_FINDNEXT: FindNext(inst, false); return; // Handle IDM_FINDNEXT to process this window message or command.
    case IDM_WRAP: // Handle IDM_WRAP to process this window message or command.
        inst->wordWrap = !inst->wordWrap; SyncToActive(inst); // Store the new value in the whether the editor wraps long lines field of the current instance.
        if (!inst->hostManaged) RegWriteStr(L"WordWrap", inst->wordWrap ? L"1" : L"0"); // Continue this branch only when inst->host managed evaluates to false.
        ApplyWordWrap(inst); // Call ApplyWordWrap to apply the requested state or value.
        if (inst->hTbBtns[9]) InvalidateRect(inst->hTbBtns[9], nullptr, TRUE); // Continue this branch only when inst- is greater than hTbBtns[9].
        return; // Return the computed  to the caller.
    case IDM_ZOOM_IN: // Handle IDM_ZOOM_IN to process this window message or command.
        if (inst->zoom < 500) { inst->zoom += 10; SyncToActive(inst); ApplyZoom(inst); // Continue this branch only when inst- is greater than zoom < 500.
            if (!inst->hostManaged) RegWriteStr(L"Zoom", std::to_wstring(inst->zoom)); // Continue this branch only when inst->host managed evaluates to false.
            if (inst->hSelf) InvalidateRect(inst->hSelf, &inst->statusRect, FALSE); // Continue this branch only when inst- is greater than hSelf.
            FireNotify(inst, NP_NOTIFY_ZOOM_CHANGED, inst->activeDoc); } // Close the current nested block.
        return; // Return the computed  to the caller.
    case IDM_ZOOM_OUT: // Handle IDM_ZOOM_OUT to process this window message or command.
        if (inst->zoom > 30) { inst->zoom -= 10; SyncToActive(inst); ApplyZoom(inst); // Continue this branch only when inst- is greater than zoom > 30.
            if (!inst->hostManaged) RegWriteStr(L"Zoom", std::to_wstring(inst->zoom)); // Continue this branch only when inst->host managed evaluates to false.
            if (inst->hSelf) InvalidateRect(inst->hSelf, &inst->statusRect, FALSE); // Continue this branch only when inst- is greater than hSelf.
            FireNotify(inst, NP_NOTIFY_ZOOM_CHANGED, inst->activeDoc); } // Close the current nested block.
        return; // Return the computed  to the caller.
    case IDM_ZOOM_RESET: // Handle IDM_ZOOM_RESET to process this window message or command.
        inst->zoom = 100; SyncToActive(inst); ApplyZoom(inst); // Store the new value in the the current text zoom percentage field of the current instance.
        if (!inst->hostManaged) RegWriteStr(L"Zoom", L"100"); // Continue this branch only when inst->host managed evaluates to false.
        if (inst->hSelf) InvalidateRect(inst->hSelf, &inst->statusRect, FALSE); // Continue this branch only when inst- is greater than hSelf.
        FireNotify(inst, NP_NOTIFY_ZOOM_CHANGED, inst->activeDoc); // Call FireNotify to perform this part of the Notepad operation.
        return; // Return the computed  to the caller.
    case IDM_STATUS: // Handle IDM_STATUS to process this window message or command.
        inst->showStatus = !inst->showStatus; // Store the new value in the show status field of the current instance.
        if (!inst->hostManaged) RegWriteStr(L"ShowStatus", inst->showStatus ? L"1" : L"0"); // Continue this branch only when inst->host managed evaluates to false.
        UpdateLayout(inst, hWnd); InvalidateRect(hWnd, nullptr, TRUE); // Call UpdateLayout to apply the requested state or value.
        return; // Return the computed  to the caller.
    case IDM_TOOLBAR: // Handle IDM_TOOLBAR to process this window message or command.
        if (inst->hToolbarHost) { // Continue this branch only when inst- is greater than hToolbarHost.
            inst->tbVisible = !inst->tbVisible; // Toggle the toolbar state independently from parent visibility.
            ShowWindow(inst->hToolbarHost, inst->tbVisible ? SW_SHOW : SW_HIDE); // Show or hide the toolbar to match its state.
            UpdateLayout(inst, hWnd); InvalidateRect(hWnd, nullptr, TRUE); // Call UpdateLayout to apply the requested state or value.
        } // Close the current block.
        return; // Return the computed  to the caller.
    case IDM_FONT: { // Handle IDM_FONT to process this window message or command.
        CHOOSEFONTW cf{}; cf.lStructSize = sizeof(cf); // Update the l struct size field with the calculated value.
        cf.hwndOwner = inst->isChild ? inst->hParent : hWnd; // Update the hwnd owner field with the calculated value.
        cf.lpLogFont = &inst->logFont; // Update the lp log font field with the calculated value.
        cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_NOVERTFONTS; // Update the flags field with the calculated value.
        if (ChooseFontW(&cf)) { // Continue this branch only when choose font w(&cf) is true.
            ApplyEditorFont(inst); // Call ApplyEditorFont to apply the requested state or value.
            if (!inst->hostManaged) { // Continue this branch only when inst->host managed evaluates to false.
                RegWriteStr(L"FontFace", inst->logFont.lfFaceName); // Call RegWriteStr to perform this part of the Notepad operation.
                RegWriteStr(L"FontSize", std::to_wstring(inst->logFont.lfHeight)); // Call RegWriteStr to perform this part of the Notepad operation.
            } // Close the current block.
        } // Close the current block.
        return; // Return the computed  to the caller.
    } // Close the current block.
    case IDM_ABOUT: // Handle IDM_ABOUT to process this window message or command.
        MessageBoxW(inst->isChild ? inst->hParent : hWnd, // Call MessageBoxW to show the initialization error to the user.
            L"Neage Notepad\r\nA small, dark, Win32 text editor.\r\n\r\n" // Continue the current declaration or expression.
            L"Ctrl+N/O/S, Ctrl+Z/Y, Ctrl+F/H, F3, Ctrl+W (close tab)\r\n" // Continue the current declaration or expression.
            L"Ctrl+T (new tab), Ctrl+Tab (next tab), Ctrl+Plus/Minus for zoom.\r\n\r\n" // Continue the current declaration or expression.
            L"F2 or right-click a tab to rename it.", // Supply this parameter or list item to the enclosing call.
            APP_NAME, MB_OK | MB_ICONINFORMATION); // Complete the current operation before proceeding to the next statement.
        return; // Return the computed  to the caller.
    } // Close the current block.
} // Close the current block.

void ShowMainMenu(NotepadInstance* inst, HWND hWnd) { // Define ShowMainMenu as the function that implements this part of the panel.
    RECT br{}; // Complete the current operation before proceeding to the next statement.
    if (inst->hTbBtns[0]) GetWindowRect(inst->hTbBtns[0], &br); // Continue this branch only when inst- is greater than hTbBtns[0].
    else GetWindowRect(hWnd, &br); // Run this fallback branch when the previous condition did not match.
    bool tbVisible = inst->tbVisible; // Use the explicit toolbar state for the menu checkmark.
    std::vector<MenuItem> items; // Declare items for use by this block.
    auto add = [&](int id, const wchar_t* label, const wchar_t* sc, bool checked = false, bool disabled = false) { // Declare add and start with [&](int id, const wchar_t* label, const wchar_t* sc, bool checked = false, bool disabled = false) {.
        MenuItem it; it.id = id; it.label = label; it.shortcut = sc; it.checked = checked; it.disabled = disabled; items.push_back(it); }; // Update the the command or control identifier field with the calculated value.
    auto sep = [&]() { MenuItem it; it.separator = true; items.push_back(it); }; // Declare sep and start with [&]() { MenuItem it; it.separator = true; items.push_back(it); }.
    add(IDM_NEW, L"New Tab", L"Ctrl+T"); // Call add to perform this part of the Notepad operation.
    add(IDM_OPEN, L"Open...", L"Ctrl+O"); // Call add to perform this part of the Notepad operation.
    add(IDM_SAVE, L"Save", L"Ctrl+S"); // Call add to perform this part of the Notepad operation.
    add(IDM_SAVEAS, L"Save As...", L"Ctrl+Shift+S"); sep(); // Call add to perform this part of the Notepad operation.
    add(IDM_UNDO, L"Undo", L"Ctrl+Z", false, inst->readOnly); // Call add to perform this part of the Notepad operation.
    add(IDM_REDO, L"Redo", L"Ctrl+Y", false, inst->readOnly); sep(); // Call add to perform this part of the Notepad operation.
    add(IDM_CUT, L"Cut", L"Ctrl+X", false, inst->readOnly); // Call add to perform this part of the Notepad operation.
    add(IDM_COPY, L"Copy", L"Ctrl+C"); // Call add to perform this part of the Notepad operation.
    add(IDM_PASTE, L"Paste", L"Ctrl+V", false, inst->readOnly); // Call add to perform this part of the Notepad operation.
    add(IDM_DELETE, L"Delete", L"Del", false, inst->readOnly); // Call add to perform this part of the Notepad operation.
    add(IDM_SELECTALL, L"Select All", L"Ctrl+A"); // Call add to perform this part of the Notepad operation.
    add(IDM_DATETIME, L"Time / Date", L"F5", false, inst->readOnly); sep(); // Call add to perform this part of the Notepad operation.
    add(IDM_FIND, L"Find...", L"Ctrl+F"); // Call add to perform this part of the Notepad operation.
    add(IDM_REPLACE, L"Replace...", L"Ctrl+H"); // Call add to perform this part of the Notepad operation.
    add(IDM_FINDNEXT, L"Find Next", L"F3"); sep(); // Call add to perform this part of the Notepad operation.
    add(IDM_WRAP, L"Word Wrap", L"", inst->wordWrap); // Call add to perform this part of the Notepad operation.
    add(IDM_FONT, L"Font...", L""); // Call add to perform this part of the Notepad operation.
    add(IDM_ZOOM_IN, L"Zoom In", L"Ctrl++"); // Call add to perform this part of the Notepad operation.
    add(IDM_ZOOM_OUT, L"Zoom Out", L"Ctrl+-"); // Call add to perform this part of the Notepad operation.
    add(IDM_ZOOM_RESET, L"Reset Zoom", L"Ctrl+0"); sep(); // Call add to perform this part of the Notepad operation.
    add(IDM_STATUS, L"Status Bar", L"", inst->showStatus); // Call add to perform this part of the Notepad operation.
    add(IDM_TOOLBAR, L"Toolbar", L"", tbVisible); sep(); // Call add to perform this part of the Notepad operation.
    add(IDM_ABOUT, L"About", L""); // Call add to perform this part of the Notepad operation.
    if (!inst->isChild) add(IDM_EXIT, L"Exit", L"Alt+F4"); // Continue this branch only when inst->is child evaluates to false.
    int chosen = ShowThemedMenuInst(inst, hWnd, br.left, br.bottom + Dpi(inst, 6), items); // Declare chosen and start with ShowThemedMenuInst(inst, hWnd, br.left, br.bottom + Dpi(inst, 6), items).
    if (chosen) DispatchCommand(inst, hWnd, chosen); // Continue this branch only when chosen is true.
} // Close the current block.

// ============================================================
//  Editor subclass
// ============================================================
LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR refData) { // Define EditProc as the function that implements this part of the panel.
    NotepadInstance* inst = reinterpret_cast<NotepadInstance*>(refData); // Declare the state object for this Notepad panel and start with reinterpret_cast<NotepadInstance*>(refData).
    if (!inst) inst = GetInstance(GetParent(h)); // Continue this branch only when the state object for this Notepad panel is unavailable.
    switch (m) { // Choose the handler based on the current message or command.
    case WM_KEYDOWN: // Handle WM_KEYDOWN to handle keyboard shortcuts.
        if (w == VK_ESCAPE && inst && inst->searchOpen) { ShowSearch(inst, false, false); return 0; } // Continue this branch only when w is equal to VK_ESCAPE and the state object for this Notepad panel is true and inst- is greater than searchOpen.
        break; // Stop processing the current switch case or loop.
    case WM_CHAR: // Handle WM_CHAR to handle typed characters.
        if (w == VK_TAB) { SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)L"    "); return 0; } // Continue this branch only when w is equal to VK_TAB.
        break; // Stop processing the current switch case or loop.
    case WM_SETFOCUS: // Handle WM_SETFOCUS to restore keyboard focus.
    case WM_KILLFOCUS: // Handle WM_KILLFOCUS to react to losing keyboard focus.
        if (inst && inst->hSelf) InvalidateRect(inst->hSelf, &inst->statusRect, FALSE); // Continue this branch only when the state object for this Notepad panel is true and inst- is greater than hSelf.
        if (inst) FireNotify(inst, NP_NOTIFY_FOCUS_CHANGED, inst->activeDoc); // Continue this branch only when the state object for this Notepad panel is true.
        break; // Stop processing the current switch case or loop.
    } // Close the current block.
    LRESULT r = DefSubclassProc(h, m, w, l); // Declare r and start with DefSubclassProc(h, m, w, l).
    if (inst && inst->hSelf && (m == WM_KEYUP || m == WM_LBUTTONUP || m == WM_CHAR)) // Continue this branch only when the state object for this Notepad panel is true and inst- is greater than hSelf and (m is equal to WM_KEYUP or m is equal to WM_LBUTTONUP or m is equal to WM_CHAR).
        InvalidateRect(inst->hSelf, &inst->statusRect, FALSE); // Call InvalidateRect to mark the requested area for repainting.
    return r; // Return the computed r to the caller.
} // Close the current block.

// ============================================================
//  Paint helpers
// ============================================================
static void PaintSearchBar(NotepadInstance* inst, HDC hdc, const RECT& rc) { // Define PaintSearchBar as the function that implements this part of the panel.
    FillSolid(hdc, rc, inst->theme.BgPanel); // Call FillSolid to render the requested interface element.
    RECT line = { rc.left, rc.bottom - 1, rc.right, rc.bottom }; // Declare line and start with { rc.left, rc.bottom - 1, rc.right, rc.bottom }.
    FillSolid(hdc, line, inst->theme.BorderSoft); // Call FillSolid to render the requested interface element.
    RECT card = rc; // Declare card and start with rc.
    card.left += Dpi(inst, 6); card.right -= Dpi(inst, 6); // Update the left + field with the calculated value.
    card.top += Dpi(inst, 6); card.bottom -= Dpi(inst, 6); // Update the top + field with the calculated value.
    StrokeRoundRect(hdc, card, Dpi(inst, 8), inst->theme.BorderSoft); // Call StrokeRoundRect to perform this part of the Notepad operation.
} // Close the current block.

static void PaintStatusBar(NotepadInstance* inst, HDC hdc, const RECT& rc) { // Define PaintStatusBar as the function that implements this part of the panel.
    FillSolid(hdc, rc, inst->theme.BgPanel); // Call FillSolid to render the requested interface element.
    RECT sep = { rc.left, rc.top, rc.right, rc.top + 1 }; // Declare sep and start with { rc.left, rc.top, rc.right, rc.top + 1 }.
    FillSolid(hdc, sep, inst->theme.BorderSoft); // Call FillSolid to render the requested interface element.
    if (!inst->hEdit) return; // Continue this branch only when inst->h edit evaluates to false.
    wchar_t buf[256]; // Declare buf for use by this block.
    HFONT of = (HFONT)SelectObject(hdc, inst->fontSmall); // Declare of and start with (HFONT)SelectObject(hdc, inst->fontSmall).
    SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, inst->theme.TextMid); // Call SetBkMode to apply the requested state or value.
    wsprintfW(buf, L"Ln %d, Col %d", GetCaretLine(inst), GetCaretCol(inst)); // Call wsprintfW to perform this part of the Notepad operation.
    RECT r1 = { rc.left + Dpi(inst, 14), rc.top, rc.left + Dpi(inst, 180), rc.bottom }; // Declare r1 and start with { rc.left + Dpi(inst, 14), rc.top, rc.left + Dpi(inst, 180), rc.bottom }.
    DrawTextW(hdc, buf, -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE); // Call DrawTextW to draw text within the requested rectangle.
    wsprintfW(buf, L"%d lines   %d chars", GetLineCount(inst), GetCharCount(inst)); // Call wsprintfW to perform this part of the Notepad operation.
    RECT r2 = { rc.right - Dpi(inst, 280), rc.top, rc.right - Dpi(inst, 140), rc.bottom }; // Declare r2 and start with { rc.right - Dpi(inst, 280), rc.top, rc.right - Dpi(inst, 140), rc.bottom }.
    DrawTextW(hdc, buf, -1, &r2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE); // Call DrawTextW to draw text within the requested rectangle.
    wsprintfW(buf, L"%d tabs", (int)inst->docs.size()); // Call wsprintfW to perform this part of the Notepad operation.
    RECT r3 = { rc.right - Dpi(inst, 140), rc.top, rc.right - Dpi(inst, 60), rc.bottom }; // Declare r3 and start with { rc.right - Dpi(inst, 140), rc.top, rc.right - Dpi(inst, 60), rc.bottom }.
    DrawTextW(hdc, buf, -1, &r3, DT_RIGHT | DT_VCENTER | DT_SINGLELINE); // Call DrawTextW to draw text within the requested rectangle.
    wsprintfW(buf, L"%d%%", inst->zoom); // Call wsprintfW to perform this part of the Notepad operation.
    RECT r4 = { rc.right - Dpi(inst, 60), rc.top, rc.right - Dpi(inst, 14), rc.bottom }; // Declare r4 and start with { rc.right - Dpi(inst, 60), rc.top, rc.right - Dpi(inst, 14), rc.bottom }.
    DrawTextW(hdc, buf, -1, &r4, DT_RIGHT | DT_VCENTER | DT_SINGLELINE); // Call DrawTextW to draw text within the requested rectangle.
    SelectObject(hdc, of); // Call SelectObject to select a GDI object into the drawing context.
} // Close the current block.

// ============================================================
//  WndProc (instance-aware)
// ============================================================
static bool MouseOverTabBar(NotepadInstance* inst, LPARAM lParam) { // Define MouseOverTabBar as the function that implements this part of the panel.
    int y = GET_Y_LPARAM(lParam); // Declare y and start with GET_Y_LPARAM(lParam).
    return (y >= inst->tabBarRect.top && y < inst->tabBarRect.bottom); // Return the computed (y >= inst->tab bar rect.top && y < inst->tab bar rect.bottom) to the caller.
} // Close the current block.

LRESULT CALLBACK PanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) { // Define PanelWndProc as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hWnd); // Declare the state object for this Notepad panel and start with GetInstance(hWnd).
    switch (msg) { // Choose the handler based on the current message or command.
    case WM_CREATE: // Handle WM_CREATE to accept creation of the panel.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.

    case WM_SIZE: // Handle WM_SIZE to resize child controls with the host.
        if (inst) { UpdateLayout(inst, hWnd); InvalidateRect(hWnd, nullptr, TRUE); } // Continue this branch only when the state object for this Notepad panel is true.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.

    case WM_GETMINMAXINFO: // Handle WM_GETMINMAXINFO to set the minimum frame size.
        if (inst && inst->isChild) return 0; // Continue this branch only when the state object for this Notepad panel is true and inst- is greater than isChild.
        if (inst) { // Continue this branch only when the state object for this Notepad panel is true.
            MINMAXINFO* p = (MINMAXINFO*)lParam; // Declare p and start with (MINMAXINFO*)lParam.
            p->ptMinTrackSize.x = Dpi(inst, 560); // Store the new value in the pt min track size.x field of the current instance.
            p->ptMinTrackSize.y = Dpi(inst, 360); // Store the new value in the pt min track size.y field of the current instance.
        } // Close the current block.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.

    case WM_ERASEBKGND: return 1; // Handle WM_ERASEBKGND to avoid flicker during custom painting.

    case WM_PAINT: { // Handle WM_PAINT to draw the panel surface.
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps); // Update hdc with the result of this expression.
        RECT rc; GetClientRect(hWnd, &rc); // Declare the rectangle used to measure a window’s client area for use by this block.
        if (!inst) { EndPaint(hWnd, &ps); return 0; } // Continue this branch only when the state object for this Notepad panel is unavailable.
        HDC mem = CreateCompatibleDC(hdc); // Declare mem and start with CreateCompatibleDC(hdc).
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom); // Declare bmp and start with CreateCompatibleBitmap(hdc, rc.right, rc.bottom).
        HGDIOBJ old = SelectObject(mem, bmp); // Declare old and start with SelectObject(mem, bmp).
        FillSolid(mem, rc, inst->theme.BgApp); // Call FillSolid to render the requested interface element.
        if (inst->searchOpen) PaintSearchBar(inst, mem, inst->searchRect); // Continue this branch only when inst- is greater than searchOpen.
        PaintTabs(inst, mem); // Call PaintTabs to render the requested interface element.
        if (inst->showStatus) PaintStatusBar(inst, mem, inst->statusRect); // Continue this branch only when inst- is greater than showStatus.
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); // Call BitBlt to copy the completed off-screen drawing to the window.
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); // Call SelectObject to select a GDI object into the drawing context.
        EndPaint(hWnd, &ps); return 0; // Call EndPaint to finish the current window painting operation.
    } // Close the current block.

    case WM_DRAWITEM: { // Handle WM_DRAWITEM to process this window message or command.
        if (inst) { DrawButton(inst, (const DRAWITEMSTRUCT*)lParam); return TRUE; } // Continue this branch only when the state object for this Notepad panel is true.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.

    case WM_MOUSEMOVE: { // Handle WM_MOUSEMOVE to update hover feedback.
        if (!inst) return 0; // Continue this branch only when the state object for this Notepad panel is unavailable.
        if (!inst->trackingMouse) { // Continue this branch only when inst->tracking mouse evaluates to false.
            TRACKMOUSEEVENT tme{}; tme.cbSize = sizeof(tme); tme.dwFlags = TME_LEAVE; tme.hwndTrack = hWnd; // Update the cb size field with the calculated value.
            TrackMouseEvent(&tme); inst->trackingMouse = true; // Store the new value in the tracking mouse field of the current instance.
        } // Close the current block.
        if (MouseOverTabBar(inst, lParam)) { // Continue this branch only when mouse over tab bar(inst, l param) is true.
            bool onClose = false; // Declare on close and start disabled.
            int idx = HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose); // Declare idx and start with HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose).
            int newClose = onClose ? idx : -1; // Declare new close and start with onClose ? idx : -1.
            if (idx != inst->tabHot || newClose != inst->tabCloseHot) { // Continue this branch only when idx is different from inst->tabHot or new close is different from inst->tabCloseHot.
                inst->tabHot = idx; inst->tabCloseHot = newClose; // Store the new value in the tab hot field of the current instance.
                InvalidateRect(hWnd, &inst->tabBarRect, FALSE); // Call InvalidateRect to mark the requested area for repainting.
            } // Close the current block.
            SetCursor(LoadCursorW(nullptr, onClose ? IDC_HAND : IDC_ARROW)); // Call SetCursor to apply the requested state or value.
            return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
        } // Close the current block.
        if (inst->tabHot != -1 || inst->tabCloseHot != -1) { // Continue this branch only when inst- is greater than tabHot != -1 or inst- is greater than tabCloseHot != -1.
            inst->tabHot = -1; inst->tabCloseHot = -1; // Store the new value in the tab hot field of the current instance.
            InvalidateRect(hWnd, &inst->tabBarRect, FALSE); // Call InvalidateRect to mark the requested area for repainting.
        } // Close the current block.
        UpdateHotButton(inst, hWnd, lParam); // Call UpdateHotButton to apply the requested state or value.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.

    case WM_MOUSELEAVE: // Handle WM_MOUSELEAVE to clear hover feedback.
        if (inst) { // Continue this branch only when the state object for this Notepad panel is true.
            inst->trackingMouse = false; // Store the new value in the tracking mouse field of the current instance.
            if (inst->tabHot != -1 || inst->tabCloseHot != -1) { // Continue this branch only when inst- is greater than tabHot != -1 or inst- is greater than tabCloseHot != -1.
                inst->tabHot = -1; inst->tabCloseHot = -1; // Store the new value in the tab hot field of the current instance.
                InvalidateRect(hWnd, &inst->tabBarRect, FALSE); // Call InvalidateRect to mark the requested area for repainting.
            } // Close the current block.
        } // Close the current block.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.

    case WM_LBUTTONDOWN: { // Handle WM_LBUTTONDOWN to begin the left-button interaction.
        if (!inst) return 0; // Continue this branch only when the state object for this Notepad panel is unavailable.
        if (MouseOverTabBar(inst, lParam)) { // Continue this branch only when mouse over tab bar(inst, l param) is true.
            if (inst->hRenameEdit) { // Continue this branch only when inst- is greater than hRenameEdit.
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; // Declare pt and start with { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }.
                RECT r; GetWindowRect(inst->hRenameEdit, &r); // Declare r for use by this block.
                POINT tl = { r.left, r.top }; ScreenToClient(hWnd, &tl); // Declare tl and start with { r.left, r.top }; ScreenToClient(hWnd, &tl).
                RECT er = { tl.x, tl.y, tl.x + (r.right-r.left), tl.y + (r.bottom-r.top) }; // Declare er and start with { tl.x, tl.y, tl.x + (r.right-r.left), tl.y + (r.bottom-r.top) }.
                if (!PtInRect(&er, pt)) CommitRename(inst, true); // Continue this branch only when pt in rect(&er, pt) evaluates to false.
            } // Close the current block.
            bool onClose = false; // Declare on close and start disabled.
            int idx = HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose); // Declare idx and start with HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose).
            if (idx == -2) DoNewTab(inst); // Continue this branch only when idx is equal to -2.
            else if (idx >= 0) { if (onClose) CloseDoc(inst, idx); else SetActiveDoc(inst, idx); } // Continue this branch only when idx is at least 0.
            return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
        } // Close the current block.
        break; // Stop processing the current switch case or loop.
    } // Close the current block.

    case WM_MBUTTONDOWN: // Handle WM_MBUTTONDOWN to process this window message or command.
        if (inst && MouseOverTabBar(inst, lParam)) { // Continue this branch only when the state object for this Notepad panel is true and mouse over tab bar(inst, l param) is true.
            if (inst->hRenameEdit) CommitRename(inst, true); // Continue this branch only when inst- is greater than hRenameEdit.
            bool onClose = false; // Declare on close and start disabled.
            int idx = HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose); // Declare idx and start with HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose).
            if (idx >= 0) CloseDoc(inst, idx); // Continue this branch only when idx is at least 0.
            return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
        } // Close the current block.
        break; // Stop processing the current switch case or loop.

    case WM_RBUTTONUP: // Handle WM_RBUTTONUP to show the context menu.
        if (inst && MouseOverTabBar(inst, lParam)) { // Continue this branch only when the state object for this Notepad panel is true and mouse over tab bar(inst, l param) is true.
            if (inst->hRenameEdit) CommitRename(inst, true); // Continue this branch only when inst- is greater than hRenameEdit.
            bool onClose = false; // Declare on close and start disabled.
            int idx = HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose); // Declare idx and start with HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose).
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; // Declare pt and start with { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }.
            ClientToScreen(hWnd, &pt); // Call ClientToScreen to perform this part of the Notepad operation.
            ShowTabMenu(inst, idx, pt.x, pt.y); // Call ShowTabMenu to perform this part of the Notepad operation.
            return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
        } // Close the current block.
        break; // Stop processing the current switch case or loop.

    case WM_LBUTTONDBLCLK: // Handle WM_LBUTTONDBLCLK to process this window message or command.
        if (inst && MouseOverTabBar(inst, lParam)) { // Continue this branch only when the state object for this Notepad panel is true and mouse over tab bar(inst, l param) is true.
            bool onClose = false; // Declare on close and start disabled.
            int idx = HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose); // Declare idx and start with HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose).
            if (idx < 0) DoNewTab(inst); // Continue this branch only when idx is less than 0.
            return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
        } // Close the current block.
        break; // Stop processing the current switch case or loop.

    case WM_RENAME_COMMIT: // Handle WM_RENAME_COMMIT to process this window message or command.
        if (inst) CommitRename(inst, wParam != 0); // Continue this branch only when the state object for this Notepad panel is true.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.

    case WM_CTLCOLOREDIT: // Handle WM_CTLCOLOREDIT to process this window message or command.
    case WM_CTLCOLORSTATIC: { // Handle WM_CTLCOLORSTATIC to process this window message or command.
        if (!inst) return DefWindowProcW(hWnd, msg, wParam, lParam); // Continue this branch only when the state object for this Notepad panel is unavailable.
        HDC hdc = (HDC)wParam; HWND child = (HWND)lParam; // Declare hdc and start with (HDC)wParam; HWND child = (HWND)lParam.
        if (child == inst->hSearchFind || child == inst->hSearchRepl || child == inst->hRenameEdit) { // Continue this branch only when child is equal to inst->hSearchFind or child is equal to inst->hSearchRepl or child is equal to inst->hRenameEdit.
            SetTextColor(hdc, inst->theme.TextHi); // Call SetTextColor to apply the requested state or value.
            SetBkColor(hdc, inst->theme.BgCard); // Call SetBkColor to apply the requested state or value.
            if (inst->brSearchBg) DeleteObject(inst->brSearchBg); // Continue this branch only when inst- is greater than brSearchBg.
            inst->brSearchBg = CreateSolidBrush(inst->theme.BgCard); // Store the new value in the br search bg field of the current instance.
            return (LRESULT)inst->brSearchBg; // Return the computed (lresult)inst->br search bg to the caller.
        } // Close the current block.
        SetTextColor(hdc, inst->theme.TextMid); // Call SetTextColor to apply the requested state or value.
        SetBkColor(hdc, inst->theme.BgApp); // Call SetBkColor to apply the requested state or value.
        if (inst->brAppBg) DeleteObject(inst->brAppBg); // Continue this branch only when inst- is greater than brAppBg.
        inst->brAppBg = CreateSolidBrush(inst->theme.BgApp); // Store the new value in the br app bg field of the current instance.
        return (LRESULT)inst->brAppBg; // Return the computed (lresult)inst->br app bg to the caller.
    } // Close the current block.

    case WM_COMMAND: { // Handle WM_COMMAND to dispatch a user command.
        if (!inst) return 0; // Continue this branch only when the state object for this Notepad panel is unavailable.
        int id = LOWORD(wParam), code = HIWORD(wParam); // Declare the command or control identifier and start with LOWORD(wParam), code = HIWORD(wParam).
        switch (id) { // Choose the handler based on the current message or command.
        case IDC_TB_MENU:  ShowMainMenu(inst, hWnd); return 0; // Handle IDC_TB_MENU to process this window message or command.
        case IDC_TB_NEW:   DoNewTab(inst); return 0; // Handle IDC_TB_NEW to process this window message or command.
        case IDC_TB_OPEN:  DoOpen(inst); return 0; // Handle IDC_TB_OPEN to process this window message or command.
        case IDC_TB_SAVE:  DoSave(inst, false); return 0; // Handle IDC_TB_SAVE to process this window message or command.
        case IDC_TB_UNDO:  if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, EM_UNDO, 0, 0); return 0; // Handle IDC_TB_UNDO to process this window message or command.
        case IDC_TB_REDO:  if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, EM_REDO, 0, 0); return 0; // Handle IDC_TB_REDO to process this window message or command.
        case IDC_TB_FIND:  ShowSearch(inst, !inst->searchOpen, false); return 0; // Handle IDC_TB_FIND to process this window message or command.
        case IDC_TB_ZOOM_IN:  DispatchCommand(inst, hWnd, IDM_ZOOM_IN); return 0; // Handle IDC_TB_ZOOM_IN to process this window message or command.
        case IDC_TB_ZOOM_OUT: DispatchCommand(inst, hWnd, IDM_ZOOM_OUT); return 0; // Handle IDC_TB_ZOOM_OUT to process this window message or command.
        case IDC_TB_WRAP:     DispatchCommand(inst, hWnd, IDM_WRAP); return 0; // Handle IDC_TB_WRAP to process this window message or command.
        case IDC_SEARCH_NEXT:    FindNext(inst, false); return 0; // Handle IDC_SEARCH_NEXT to process this window message or command.
        case IDC_SEARCH_PREV:    FindNext(inst, true); return 0; // Handle IDC_SEARCH_PREV to process this window message or command.
        case IDC_SEARCH_CLOSE:   ShowSearch(inst, false, false); return 0; // Handle IDC_SEARCH_CLOSE to process this window message or command.
        case IDC_SEARCH_CASE: // Handle IDC_SEARCH_CASE to process this window message or command.
            inst->searchCase = !inst->searchCase; // Store the new value in the search case field of the current instance.
            if (inst->hSearchCase) InvalidateRect(inst->hSearchCase, nullptr, TRUE); // Continue this branch only when inst- is greater than hSearchCase.
            return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
        case IDC_SEARCH_REPL_ONE: ReplaceOne(inst); return 0; // Handle IDC_SEARCH_REPL_ONE to process this window message or command.
        case IDC_SEARCH_REPL_ALL: { int n = ReplaceAll(inst); if (n == 0) MessageBeep(MB_OK); return 0; } // Handle IDC_SEARCH_REPL_ALL to process this window message or command.
        case IDC_SEARCH_FIND: (void)code; return 0; // Handle IDC_SEARCH_FIND to process this window message or command.
        } // Close the current block.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.

    case WM_NOTIFY: { // Handle WM_NOTIFY to handle a child-control notification.
        if (!inst) return 0; // Continue this branch only when the state object for this Notepad panel is unavailable.
        NMHDR* p = (NMHDR*)lParam; // Declare p and start with (NMHDR*)lParam.
        if (p->hwndFrom == inst->hEdit && p->code == EN_CHANGE) { // Continue this branch only when p- is greater than hwndFrom == inst->hEdit and p- is greater than code == EN_CHANGE.
            bool modified = (BOOL)SendMessageW(inst->hEdit, EM_GETMODIFY, 0, 0); // Declare whether this document contains unsaved edits and start with (BOOL)SendMessageW(inst->hEdit, EM_GETMODIFY, 0, 0).
            if (modified != inst->modified) { // Continue this branch only when whether this document contains unsaved edits is different from inst->modified.
                inst->modified = modified; // Store the new value in the whether this document contains unsaved edits field of the current instance.
                Document* d = Active(inst); if (d) d->modified = modified; // Declare d and start with Active(inst); if (d) d->modified = modified.
                UpdateTitle(inst); // Call UpdateTitle to apply the requested state or value.
                InvalidateRect(hWnd, &inst->tabBarRect, FALSE); // Call InvalidateRect to mark the requested area for repainting.
                FireNotify(inst, NP_NOTIFY_MODIFIED_CHANGED, inst->activeDoc); // Call FireNotify to perform this part of the Notepad operation.
            } // Close the current block.
            InvalidateRect(hWnd, &inst->statusRect, FALSE); // Call InvalidateRect to mark the requested area for repainting.
            FireNotify(inst, NP_NOTIFY_CONTENT_CHANGED, inst->activeDoc); // Call FireNotify to perform this part of the Notepad operation.
        } // Close the current block.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.

    case WM_SETFOCUS: // Handle WM_SETFOCUS to restore keyboard focus.
        if (inst && inst->hEdit) SetFocus(inst->hEdit); // Continue this branch only when the state object for this Notepad panel is true and inst- is greater than hEdit.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.

    case WM_KEYDOWN: // Handle WM_KEYDOWN to handle keyboard shortcuts.
        if (!inst) break; // Continue this branch only when the state object for this Notepad panel is unavailable.
        // Check suppressed shortcuts
        if (GetKeyState(VK_CONTROL) & 0x8000) { // Continue this branch only when get key state(vk control) & 0x8000 is true.
            int cmd = 0; // Declare cmd and start with 0.
            switch (wParam) { // Choose the handler based on the current message or command.
            case 'N': cmd = IDM_NEW; break; // Handle 'N' to process this window message or command.
            case 'T': cmd = IDM_NEW; break;  // Ctrl+T = new tab (same as new)
            case 'W': cmd = IDM_EXIT; break;  // close tab -> handled specially below
            case VK_TAB: { // Handle VK_TAB to process this window message or command.
                if (inst->docs.size() > 1) { // Continue this branch only when inst- is greater than docs.size() > 1.
                    int next = (inst->activeDoc + 1) % (int)inst->docs.size(); // Declare next and start with (inst->activeDoc + 1) % (int)inst->docs.size().
                    SetActiveDoc(inst, next); // Call SetActiveDoc to apply the requested state or value.
                } // Close the current block.
                return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
            } // Close the current block.
            case 'O': cmd = IDM_OPEN; break; // Handle 'O' to process this window message or command.
            case 'S': cmd = (GetKeyState(VK_SHIFT) & 0x8000) ? IDM_SAVEAS : IDM_SAVE; break; // Handle 'S' to process this window message or command.
            case 'F': cmd = IDM_FIND; break; // Handle 'F' to process this window message or command.
            case 'H': cmd = IDM_REPLACE; break; // Handle 'H' to process this window message or command.
            case VK_OEM_PLUS: case VK_ADD: cmd = IDM_ZOOM_IN; break; // Handle VK_OEM_PLUS to process this window message or command.
            case VK_OEM_MINUS: case VK_SUBTRACT: cmd = IDM_ZOOM_OUT; break; // Handle VK_OEM_MINUS to process this window message or command.
            case '0': cmd = IDM_ZOOM_RESET; break; // Handle '0' to process this window message or command.
            } // Close the current block.
            // Ctrl+W is close-tab, not exit
            if (wParam == 'W') { // Continue this branch only when the first message-specific parameter is equal to 'W'.
                if (inst->suppressedCmds.count(IDM_EXIT)) return DefWindowProcW(hWnd, msg, wParam, lParam); // Continue this branch only when inst- is greater than suppressedCmds.count(IDM_EXIT).
                if (inst->activeDoc >= 0) CloseDoc(inst, inst->activeDoc); // Continue this branch only when inst- is greater than activeDoc >= 0.
                return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
            } // Close the current block.
            // Ctrl+T is new tab
            if (wParam == 'T') { // Continue this branch only when the first message-specific parameter is equal to 'T'.
                if (inst->suppressedCmds.count(IDM_NEW)) return DefWindowProcW(hWnd, msg, wParam, lParam); // Continue this branch only when inst- is greater than suppressedCmds.count(IDM_NEW).
                DoNewTab(inst); return 0; // Call DoNewTab to perform this part of the Notepad operation.
            } // Close the current block.
            if (cmd && inst->suppressedCmds.count(cmd)) return DefWindowProcW(hWnd, msg, wParam, lParam); // Continue this branch only when cmd is true and inst- is greater than suppressedCmds.count(cmd).
            if (cmd) { DispatchCommand(inst, hWnd, cmd); return 0; } // Continue this branch only when cmd is true.
        } else { // Continue the current declaration or expression.
            if (wParam == VK_F2) { if (inst->activeDoc >= 0) BeginRenameTab(inst, inst->activeDoc); return 0; } // Continue this branch only when the first message-specific parameter is equal to VK_F2.
            if (wParam == VK_F3) { FindNext(inst, false); return 0; } // Continue this branch only when the first message-specific parameter is equal to VK_F3.
            if (wParam == VK_F5) { DispatchCommand(inst, hWnd, IDM_DATETIME); return 0; } // Continue this branch only when the first message-specific parameter is equal to VK_F5.
        } // Close the current block.
        break; // Stop processing the current switch case or loop.

    case WM_CLOSE: // Handle WM_CLOSE to close the editor and frame.
        if (!inst) return 0; // Continue this branch only when the state object for this Notepad panel is unavailable.
        if (inst->isChild) { // Continue this branch only when inst- is greater than isChild.
            // In child mode, notify host and let it decide
            FireNotify(inst, NP_NOTIFY_TAB_CLOSED, -1); // Call FireNotify to perform this part of the Notepad operation.
            return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
        } // Close the current block.
        if (!CloseAllDocs(inst)) return 0; // Continue this branch only when close all docs(inst) evaluates to false.
        DestroyWindow(hWnd); // Call DestroyWindow to release the requested resource.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.

    case WM_DESTROY: { // Handle WM_DESTROY to stop the message loop.
        if (inst) { // Continue this branch only when the state object for this Notepad panel is true.
            // Save settings if standalone
            if (!inst->hostManaged && !inst->isChild) { // Continue this branch only when inst->host managed && !inst->is child evaluates to false.
                SyncToActive(inst); // Call SyncToActive to perform this part of the Notepad operation.
                RegWriteStr(L"ShowStatus", inst->showStatus ? L"1" : L"0"); // Call RegWriteStr to perform this part of the Notepad operation.
                RegWriteStr(L"Zoom", std::to_wstring(inst->zoom)); // Call RegWriteStr to perform this part of the Notepad operation.
                RegWriteStr(L"WordWrap", inst->wordWrap ? L"1" : L"0"); // Call RegWriteStr to perform this part of the Notepad operation.
                RegWriteStr(L"FontFace", inst->logFont.lfFaceName); // Call RegWriteStr to perform this part of the Notepad operation.
                RegWriteStr(L"FontSize", std::to_wstring(inst->logFont.lfHeight)); // Call RegWriteStr to perform this part of the Notepad operation.
                RegWriteStr(L"ToolbarDock", std::to_wstring((int)inst->tbDock)); // Call RegWriteStr to perform this part of the Notepad operation.
                RegWriteStr(L"ToolbarSize", std::to_wstring((int)inst->tbSize)); // Call RegWriteStr to perform this part of the Notepad operation.
            } // Close the current block.
            for (Document* d : inst->docs) { if (d->hEdit) DestroyWindow(d->hEdit); delete d; } // Visit each item in the collection and apply the loop body.
            inst->docs.clear(); // Complete the current operation before proceeding to the next statement.
            if (inst->font) DeleteObject(inst->font); // Continue this branch only when inst- is greater than font.
            if (inst->fontBold) DeleteObject(inst->fontBold); // Continue this branch only when inst- is greater than fontBold.
            if (inst->fontSmall) DeleteObject(inst->fontSmall); // Continue this branch only when inst- is greater than fontSmall.
            if (inst->brSearchBg) DeleteObject(inst->brSearchBg); // Continue this branch only when inst- is greater than brSearchBg.
            if (inst->brAppBg) DeleteObject(inst->brAppBg); // Continue this branch only when inst- is greater than brAppBg.
            if (inst->iconFont) DeleteObject(inst->iconFont); // Release the Windows icon font owned by this panel.
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0); // Call SetWindowLongPtrW to attach or update data associated with a window.
            delete inst; // Free the object allocated earlier after it is no longer needed.
        } // Close the current block.
        if (!GetInstance(hWnd) && !FindWindowW(PANEL_CLASS, nullptr)) // Continue this branch only when get instance(h wnd) && !find window w(panel class, nullptr) evaluates to false.
            PostQuitMessage(0); // Call PostQuitMessage to post the quit message that ends the application loop.
        return 0; // Tell Win32 the message was handled, or return a successful zero exit code.
    } // Close the current block.
    } // Close the current block.
    return DefWindowProcW(hWnd, msg, wParam, lParam); // Let Win32 perform its default handling for this unhandled message.
} // Close the current block.

// ============================================================
//  DLL initialization + class registration
// ============================================================
static void RegisterClasses() { // Define RegisterClasses as the function that implements this part of the panel.
    if (g_classesRegistered) return; // Continue this branch only when classes registered is true.
    WNDCLASSW wc{}; // Complete the current operation before proceeding to the next statement.
    wc.lpfnWndProc = PanelWndProc; wc.hInstance = g_hInst; // Update the lpfn wnd proc field with the calculated value.
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); // Update the h cursor field with the calculated value.
    wc.hbrBackground = nullptr; wc.lpszClassName = PANEL_CLASS; // Update the hbr background field with the calculated value.
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION); wc.style = CS_DBLCLKS; // Update the h icon field with the calculated value.
    RegisterClassW(&wc); // Call RegisterClassW to register a window class and its message procedure with Win32.
    WNDCLASSW tbc{}; // Complete the current operation before proceeding to the next statement.
    tbc.lpfnWndProc = ToolbarHostProc; tbc.hInstance = g_hInst; // Update the lpfn wnd proc field with the calculated value.
    tbc.hCursor = LoadCursorW(nullptr, IDC_ARROW); // Update the h cursor field with the calculated value.
    tbc.hbrBackground = nullptr; tbc.lpszClassName = TOOLBAR_CLASS; // Update the hbr background field with the calculated value.
    RegisterClassW(&tbc); // Call RegisterClassW to register a window class and its message procedure with Win32.
    EnsureMenuClass(); // Call EnsureMenuClass to perform this part of the Notepad operation.
    g_classesRegistered = true; // Update classes registered with the result of this expression.
} // Close the current block.

NP_API BOOL InitializeNotepadPanel(HINSTANCE hInstance) { // Define InitializeNotepadPanel as the function that implements this part of the panel.
    if (!g_hInst) g_hInst = hInstance ? hInstance : GetModuleHandleW(nullptr); // Continue this branch only when the application module handle used to register and create windows evaluates to false.
    if (!g_hInst) return FALSE; // Continue this branch only when the application module handle used to register and create windows evaluates to false.

    if (!g_msftedit) g_msftedit = LoadLibraryW(L"msftedit.dll"); // Continue this branch only when the loaded Rich Edit module required by the editor is unavailable.
    if (!g_msftedit) return FALSE; // Continue this branch only when the loaded Rich Edit module required by the editor is unavailable.

    INITCOMMONCONTROLSEX controls{}; // Complete the current operation before proceeding to the next statement.
    controls.dwSize = sizeof(controls); // Update the dw size field with the calculated value.
    controls.dwICC = ICC_STANDARD_CLASSES; // Update the dw icc field with the calculated value.
    InitCommonControlsEx(&controls); // Call InitCommonControlsEx to initialize the standard Windows control classes.

    RegisterClasses(); // Call RegisterClasses to perform this part of the Notepad operation.
    return g_classesRegistered ? TRUE : FALSE; // Return the computed classes registered ? true : false to the caller.
} // Close the current block.

static void CreateInstanceFonts(NotepadInstance* inst) { // Define CreateInstanceFonts as the function that implements this part of the panel.
    auto mk = [&](int pt, int weight, const wchar_t* face) -> HFONT { // Declare mk and start with [&](int pt, int weight, const wchar_t* face) -> HFONT {.
        return CreateFontW(-MulDiv(pt, inst->dpi, 72), 0, 0, 0, weight, 0, 0, 0, // Return the computed create font w(-mul div(pt, inst->dpi, 72), 0, 0, 0, weight, 0, 0, 0, to the caller.
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, // Supply this parameter or list item to the enclosing call.
            CLEARTYPE_QUALITY, DEFAULT_PITCH, face); }; // Complete the current operation before proceeding to the next statement.
    inst->font = mk(10, FW_NORMAL, L"Segoe UI"); // Store the new value in the the normal editor font field of the current instance.
    inst->fontBold = mk(10, FW_SEMIBOLD, L"Segoe UI"); // Store the new value in the the semibold interface font field of the current instance.
    inst->fontSmall = mk(9, FW_NORMAL, L"Segoe UI"); // Store the new value in the the small interface font field of the current instance.
} // Close the current block.

static void LoadAppSettings(NotepadInstance* inst) { // Define LoadAppSettings as the function that implements this part of the panel.
    inst->showStatus = RegReadStr(L"ShowStatus", L"1") == L"1"; // Complete the current operation before proceeding to the next statement.
    inst->zoom = RegReadInt(L"Zoom", 100); // Store the new value in the the current text zoom percentage field of the current instance.
    if (inst->zoom < 30) inst->zoom = 30; // Continue this branch only when inst- is greater than zoom < 30.
    if (inst->zoom > 500) inst->zoom = 500; // Continue this branch only when inst- is greater than zoom > 500.
    inst->wordWrap = RegReadStr(L"WordWrap", L"1") == L"1"; // Complete the current operation before proceeding to the next statement.
    int d = RegReadInt(L"ToolbarDock", (int)TBDOCK_TOP); // Declare d and start with RegReadInt(L"ToolbarDock", (int)TBDOCK_TOP).
    if (d < TBDOCK_TOP || d > TBDOCK_FLOAT) d = TBDOCK_TOP; // Continue this branch only when d is less than TBDOCK_TOP or d is greater than TBDOCK_FLOAT.
    inst->tbDock = (ToolbarDock)d; // Store the new value in the tb dock field of the current instance.
    int s = RegReadInt(L"ToolbarSize", (int)TBSIZE_MEDIUM); // Declare s and start with RegReadInt(L"ToolbarSize", (int)TBSIZE_MEDIUM).
    if (s < TBSIZE_SMALL || s > TBSIZE_LARGE) s = TBSIZE_MEDIUM; // Continue this branch only when s is less than TBSIZE_SMALL or s is greater than TBSIZE_LARGE.
    inst->tbSize = (ToolbarSize)s; // Store the new value in the tb size field of the current instance.
    ZeroMemory(&inst->logFont, sizeof(inst->logFont)); // Call ZeroMemory to perform this part of the Notepad operation.
    std::wstring face = RegReadStr(L"FontFace", L"Segoe UI"); // Declare face and start with RegReadStr(L"FontFace", L"Segoe UI").
    int size = RegReadInt(L"FontSize", -MulDiv(11, inst->dpi, 72)); // Declare size and start with RegReadInt(L"FontSize", -MulDiv(11, inst->dpi, 72)).
    wcscpy_s(inst->logFont.lfFaceName, face.c_str()); // Call wcscpy_s to perform this part of the Notepad operation.
    inst->logFont.lfHeight = size; // Store the new value in the log font.lf height field of the current instance.
    inst->logFont.lfWeight = FW_NORMAL; // Store the new value in the log font.lf weight field of the current instance.
    inst->logFont.lfCharSet = DEFAULT_CHARSET; // Store the new value in the log font.lf char set field of the current instance.
    inst->logFont.lfQuality = CLEARTYPE_QUALITY; // Store the new value in the log font.lf quality field of the current instance.
} // Close the current block.

static void InitInstance(NotepadInstance* inst, HWND hWnd, HWND hParent, // Supply this parameter or list item to the enclosing call.
                         HINSTANCE hInst, NPNOTIFYPROC notifyProc, void* userData) { // Continue the current declaration or expression.
    inst->hSelf = hWnd; // Store the new value in the this panel window handle field of the current instance.
    inst->hParent = hParent; // Store the new value in the the parent or host window handle field of the current instance.
    inst->hInst = hInst; // Store the new value in the the module handle used to create this panel’s controls field of the current instance.
    inst->instanceId = NotepadInstance::s_nextId++; // Advance the counter or index by one.
    inst->theme = DefaultDarkTheme(); // Store the new value in the the active editor color palette field of the current instance.
    inst->notifyProc = notifyProc; // Store the new value in the the host callback used for panel notifications field of the current instance.
    inst->notifyUserData = userData; // Store the new value in the the host value passed back with notifications field of the current instance.
    inst->isChild = (GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_CHILD) != 0; // Complete the current operation before proceeding to the next statement.
    inst->hostManaged = (notifyProc != nullptr) || inst->isChild; // Complete the current operation before proceeding to the next statement.
    inst->dpi = GetDeviceCaps(GetDC(hWnd), LOGPIXELSY); // Store the new value in the the current display density used for scaling field of the current instance.

    CreateInstanceFonts(inst); // Call CreateInstanceFonts to create the requested interface object or resource.
    inst->iconFont = CreateFontW(-Dpi(inst, 18), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe Fluent Icons"); // Prefer the current Windows 11 system icon font.
    HDC iconDc = GetDC(hWnd); // Get a drawing context to verify that the preferred icon font exists.
    WORD menuGlyphIndex = 0; // Store the menu glyph's resolved font index for the font availability check.
    HGDIOBJ previousIconFont = iconDc && inst->iconFont ? SelectObject(iconDc, inst->iconFont) : nullptr; // Select the preferred font for glyph lookup.
    DWORD glyphLookup = iconDc && inst->iconFont ? GetGlyphIndicesW(iconDc, L"\uE700", 1, &menuGlyphIndex, GGI_MARK_NONEXISTING_GLYPHS) : GDI_ERROR; // Detect if Segoe Fluent Icons is unavailable.
    if (iconDc && previousIconFont) SelectObject(iconDc, previousIconFont); // Restore the original font after checking.
    if (iconDc) ReleaseDC(hWnd, iconDc); // Release the temporary drawing context.
    if (glyphLookup == GDI_ERROR || menuGlyphIndex == 0xFFFF) { // Use the Windows 10 system icon font when Fluent Icons is unavailable.
        if (inst->iconFont) DeleteObject(inst->iconFont); // Discard the unresolved preferred font.
        inst->iconFont = CreateFontW(-Dpi(inst, 18), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe MDL2 Assets"); // Load the compatible Windows 10 icon font.
    } // Finish selecting a supported Windows icon font.
    if (!inst->hostManaged) LoadAppSettings(inst); // Continue this branch only when inst->host managed evaluates to false.
    else { // Run this fallback branch when the previous condition did not match.
        // Default settings for host-managed mode
        ZeroMemory(&inst->logFont, sizeof(inst->logFont)); // Call ZeroMemory to perform this part of the Notepad operation.
        wcscpy_s(inst->logFont.lfFaceName, L"Segoe UI"); // Call wcscpy_s to perform this part of the Notepad operation.
        inst->logFont.lfHeight = -MulDiv(11, inst->dpi, 72); // Store the new value in the log font.lf height field of the current instance.
        inst->logFont.lfWeight = FW_NORMAL; // Store the new value in the log font.lf weight field of the current instance.
        inst->logFont.lfCharSet = DEFAULT_CHARSET; // Store the new value in the log font.lf char set field of the current instance.
        inst->logFont.lfQuality = CLEARTYPE_QUALITY; // Store the new value in the log font.lf quality field of the current instance.
    } // Close the current block.

    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)inst); // Call SetWindowLongPtrW to attach or update data associated with a window.



    CreateDoc(inst, L"", L""); // Call CreateDoc to create the requested interface object or resource.
    SetActiveDoc(inst, 0); // Call SetActiveDoc to apply the requested state or value.


    inst->hToolbarHost = CreateWindowW(TOOLBAR_CLASS, L"", // Store the new value in the the child window that hosts toolbar buttons field of the current instance.
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, // Supply this parameter or list item to the enclosing call.
        0, 0, 100, Dpi(inst, 46), hWnd, nullptr, hInst, nullptr); // Complete the current operation before proceeding to the next statement.
    static const int tbIds[10] = { // Declare tb ids and start with {.
        IDC_TB_MENU, IDC_TB_NEW, IDC_TB_OPEN, IDC_TB_SAVE, // Supply this parameter or list item to the enclosing call.
        IDC_TB_UNDO, IDC_TB_REDO, IDC_TB_FIND, // Supply this parameter or list item to the enclosing call.
        IDC_TB_ZOOM_OUT, IDC_TB_ZOOM_IN, IDC_TB_WRAP // Continue the current declaration or expression.
    }; // Finish the current type declaration.
    for (int i = 0; i < 10; ++i) { // Repeat the body using int i = 0 and stop when i < 10.
        inst->hTbBtns[i] = MakeButton(inst, inst->hToolbarHost, tbIds[i]); // Store the new value in the h tb btns[i] field of the current instance.
        SetWindowSubclass(inst->hTbBtns[i], ToolbarBtnProc, 100 + i, 0); // Call SetWindowSubclass to install a callback that customizes a native control.
    } // Close the current block.


    inst->hSearchFind = CreateWindowExW(0, L"EDIT", L"", // Store the new value in the the find-text input control field of the current instance.
        WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 28, // Supply this parameter or list item to the enclosing call.
        hWnd, (HMENU)(INT_PTR)IDC_SEARCH_FIND, hInst, nullptr); // Complete the current operation before proceeding to the next statement.
    inst->hSearchRepl = CreateWindowExW(0, L"EDIT", L"", // Store the new value in the the replace-text input control field of the current instance.
        WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 28, // Supply this parameter or list item to the enclosing call.
        hWnd, (HMENU)(INT_PTR)IDC_SEARCH_REPL, hInst, nullptr); // Complete the current operation before proceeding to the next statement.
    SendMessageW(inst->hSearchFind, WM_SETFONT, (WPARAM)inst->font, TRUE); // Call SendMessageW to send a control message and receive its result immediately.
    SendMessageW(inst->hSearchRepl, WM_SETFONT, (WPARAM)inst->font, TRUE); // Call SendMessageW to send a control message and receive its result immediately.
    SetWindowTheme(inst->hSearchFind, L"DarkMode_Explorer", nullptr); // Call SetWindowTheme to apply the selected control theme.
    SetWindowTheme(inst->hSearchRepl, L"DarkMode_Explorer", nullptr); // Call SetWindowTheme to apply the selected control theme.
    inst->hSearchNext = MakeButton(inst, hWnd, IDC_SEARCH_NEXT); // Store the new value in the h search next field of the current instance.
    inst->hSearchPrev = MakeButton(inst, hWnd, IDC_SEARCH_PREV); // Store the new value in the h search prev field of the current instance.
    inst->hSearchClose = MakeButton(inst, hWnd, IDC_SEARCH_CLOSE); // Store the new value in the h search close field of the current instance.
    inst->hSearchCase = MakeButton(inst, hWnd, IDC_SEARCH_CASE); // Store the new value in the h search case field of the current instance.
    inst->hSearchReplOne = MakeButton(inst, hWnd, IDC_SEARCH_REPL_ONE); // Store the new value in the h search repl one field of the current instance.
    inst->hSearchReplAll = MakeButton(inst, hWnd, IDC_SEARCH_REPL_ALL); // Store the new value in the h search repl all field of the current instance.
    HWND searchCtrls[8] = { // Declare search ctrls and start with {.
        inst->hSearchFind, inst->hSearchRepl, inst->hSearchNext, inst->hSearchPrev, // Supply this parameter or list item to the enclosing call.
        inst->hSearchClose, inst->hSearchCase, inst->hSearchReplOne, inst->hSearchReplAll // Continue the current declaration or expression.
    }; // Finish the current type declaration.
    for (HWND h : searchCtrls) if (h) ShowWindow(h, SW_HIDE); // Visit each item in the collection and apply the loop body.

    if (inst->tbDock != TBDOCK_TOP) { // Continue this branch only when inst- is greater than tbDock != TBDOCK_TOP.
        ToolbarDock want = inst->tbDock; inst->tbDock = TBDOCK_TOP; // Declare want and start with inst->tbDock; inst->tbDock = TBDOCK_TOP.
        SetToolbarDock(inst, want); // Call SetToolbarDock to apply the requested state or value.
    } // Close the current block.

    UpdateLayout(inst, hWnd); // Call UpdateLayout to apply the requested state or value.
} // Close the current block.

// ============================================================
//  DLL entry point
// ============================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) { // Define DllMain as the function that implements this part of the panel.
    if (reason == DLL_PROCESS_ATTACH) { // Continue this branch only when reason is equal to DLL_PROCESS_ATTACH.
        InitializeNotepadPanel((HINSTANCE)hModule); // Call InitializeNotepadPanel to perform this part of the Notepad operation.
    } else if (reason == DLL_PROCESS_DETACH) { // Define if as the function that implements this part of the panel.
        if (g_msftedit) { FreeLibrary(g_msftedit); g_msftedit = nullptr; } // Continue this branch only when the loaded Rich Edit module required by the editor is true.
    } // Close the current block.
    return TRUE; // Report success to the caller.
} // Close the current block.

// ============================================================
//  Exported API: Creation / destruction
// ============================================================

NP_API HWND CreateNotepadPanel(HWND hParent, const RECT* rc, int id, // Supply this parameter or list item to the enclosing call.
                               NPNOTIFYPROC notifyProc, void* userData) { // Continue the current declaration or expression.
    if (!hParent) return nullptr; // Continue this branch only when the parent or host window handle is unavailable.
    RECT actualRc; // Declare actual rc for use by this block.
    if (!rc || (rc->right == 0 && rc->bottom == 0)) { // Continue this branch only when rc || (rc->right == 0 && rc->bottom == 0) evaluates to false.
        GetClientRect(hParent, &actualRc); // Call GetClientRect to read the client bounds used to lay out child controls.
    } else { // Continue the current declaration or expression.
        actualRc = *rc; // Update actual rc with the result of this expression.
    } // Close the current block.
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_TABSTOP; // Declare style and start with WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_TABSTOP.
    HWND hWnd = CreateWindowExW(0, PANEL_CLASS, L"", style, // Declare the window receiving this message and start with CreateWindowExW(0, PANEL_CLASS, L"", style,.
        actualRc.left, actualRc.top, // Supply this parameter or list item to the enclosing call.
        actualRc.right - actualRc.left, // Supply this parameter or list item to the enclosing call.
        actualRc.bottom - actualRc.top, // Supply this parameter or list item to the enclosing call.
        hParent, (HMENU)(INT_PTR)id, g_hInst, nullptr); // Complete the current operation before proceeding to the next statement.
    if (!hWnd) return nullptr; // Continue this branch only when the window receiving this message is unavailable.

    NotepadInstance* inst = new NotepadInstance(); // Declare the state object for this Notepad panel and start with new NotepadInstance().
    InitInstance(inst, hWnd, hParent, g_hInst, notifyProc, userData); // Call InitInstance to perform this part of the Notepad operation.
    return hWnd; // Return the computed the window receiving this message to the caller.
} // Close the current block.

NP_API BOOL DestroyNotepadPanel(HWND hPanel) { // Define DestroyNotepadPanel as the function that implements this part of the panel.
    if (!hPanel || !IsWindow(hPanel)) return FALSE; // Continue this branch only when h panel || !is window(h panel) evaluates to false.
    if (!GetInstance(hPanel)) return FALSE; // Continue this branch only when get instance(h panel) evaluates to false.
    DestroyWindow(hPanel); // Call DestroyWindow to release the requested resource.
    return TRUE; // Report success to the caller.
} // Close the current block.

NP_API BOOL ResizeNotepadPanel(HWND hPanel, const RECT* rc) { // Define ResizeNotepadPanel as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return FALSE; // Continue this branch only when the state object for this Notepad panel is unavailable.
    RECT actualRc; // Declare actual rc for use by this block.
    if (!rc || (rc->right == 0 && rc->bottom == 0)) { // Continue this branch only when rc || (rc->right == 0 && rc->bottom == 0) evaluates to false.
        GetClientRect(inst->hParent, &actualRc); // Call GetClientRect to read the client bounds used to lay out child controls.
    } else { // Continue the current declaration or expression.
        actualRc = *rc; // Update actual rc with the result of this expression.
    } // Close the current block.
    MoveWindow(hPanel, actualRc.left, actualRc.top, // Call MoveWindow to move and resize the panel within its parent.
        actualRc.right - actualRc.left, // Supply this parameter or list item to the enclosing call.
        actualRc.bottom - actualRc.top, TRUE); // Complete the current operation before proceeding to the next statement.
    return TRUE; // Report success to the caller.
} // Close the current block.

// ============================================================
//  Exported API: Content I/O
// ============================================================

NP_API int GetTextW(HWND hPanel, wchar_t* buf, int bufLen) { // Define GetTextW as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || !inst->hEdit) return 0; // Continue this branch only when inst || !inst->h edit evaluates to false.
    std::wstring text = GetEditorText(inst); // Declare text and start with GetEditorText(inst).
    if (!buf) return (int)text.size(); // Continue this branch only when buf evaluates to false.
    int copy = (int)text.size(); // Declare copy and start with (int)text.size().
    if (copy >= bufLen) copy = bufLen - 1; // Continue this branch only when copy is at least bufLen.
    wcsncpy_s(buf, bufLen, text.c_str(), copy); // Call wcsncpy_s to perform this part of the Notepad operation.
    buf[copy] = 0; // Update buf[copy with the result of this expression.
    return copy; // Return the computed copy to the caller.
} // Close the current block.

NP_API BOOL SetTextW(HWND hPanel, const wchar_t* text) { // Define SetTextW as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || !inst->hEdit) return FALSE; // Continue this branch only when inst || !inst->h edit evaluates to false.
    std::wstring normalized; // Declare normalized for use by this block.
    if (text) { // Continue this branch only when text is true.
        normalized.reserve(std::wcslen(text) + 16); // Complete the current operation before proceeding to the next statement.
        for (const wchar_t* p = text; *p; ++p) { // Repeat the body using const wchar_t* p = text and stop when *p.
            if (*p == L'\r') continue; // Continue this branch only when *p is equal to L'\r'.
            if (*p == L'\n') normalized += L"\r\n"; // Normalize a newline to the Rich Edit CRLF format.
            else normalized += *p; // Run this fallback branch when the previous condition did not match.
        } // Close the current block.
    } // Close the current block.
    SETTEXTEX st{}; st.flags = ST_DEFAULT; st.codepage = 1200; // Update the flags field with the calculated value.
    SendMessageW(inst->hEdit, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)normalized.c_str()); // Call SendMessageW to send a control message and receive its result immediately.
    SendMessageW(inst->hEdit, EM_SETMODIFY, FALSE, 0); // Call SendMessageW to send a control message and receive its result immediately.
    SetModified(inst, false); // Call SetModified to apply the requested state or value.
    return TRUE; // Report success to the caller.
} // Close the current block.

NP_API int GetTextLengthW(HWND hPanel) { // Define GetTextLengthW as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || !inst->hEdit) return 0; // Continue this branch only when inst || !inst->h edit evaluates to false.
    return GetCharCount(inst); // Return the computed get char count(inst) to the caller.
} // Close the current block.

NP_API int GetSelectedTextW(HWND hPanel, wchar_t* buf, int bufLen) { // Define GetSelectedTextW as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || !inst->hEdit) return 0; // Continue this branch only when inst || !inst->h edit evaluates to false.
    DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0); // Declare sel and start with (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0).
    int s = LOWORD(sel), e = HIWORD(sel); // Declare s and start with LOWORD(sel), e = HIWORD(sel).
    int len = e - s; // Declare len and start with e - s.
    if (len <= 0) { if (buf && bufLen > 0) buf[0] = 0; return 0; } // Continue this branch only when len is at most 0.
    if (!buf) return len; // Continue this branch only when buf evaluates to false.
    int copy = (len < bufLen - 1) ? len : bufLen - 1; // Declare copy and start with (len < bufLen - 1) ? len : bufLen - 1.
    std::wstring cur(copy, 0); // Complete the current operation before proceeding to the next statement.
    TEXTRANGEW tr{}; tr.chrg.cpMin = s; tr.chrg.cpMax = s + copy; tr.lpstrText = cur.data(); // Update the cp min field with the calculated value.
    SendMessageW(inst->hEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr); // Call SendMessageW to send a control message and receive its result immediately.
    wcsncpy_s(buf, bufLen, cur.c_str(), copy); // Call wcsncpy_s to perform this part of the Notepad operation.
    buf[copy] = 0; // Update buf[copy with the result of this expression.
    return copy; // Return the computed copy to the caller.
} // Close the current block.

NP_API BOOL SetSelectedTextW(HWND hPanel, const wchar_t* text) { // Define SetSelectedTextW as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || !inst->hEdit || inst->readOnly) return FALSE; // Continue this branch only when inst || !inst->h edit || inst->read only evaluates to false.
    if (!text) return FALSE; // Continue this branch only when text evaluates to false.
    SendMessageW(inst->hEdit, EM_REPLACESEL, TRUE, (LPARAM)text); // Call SendMessageW to send a control message and receive its result immediately.
    return TRUE; // Report success to the caller.
} // Close the current block.

// ============================================================
//  Exported API: Tab management
// ============================================================

NP_API int GetTabCount(HWND hPanel) { // Define GetTabCount as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    return inst ? (int)inst->docs.size() : 0; // Return the computed inst ? (int)inst->docs.size() : 0 to the caller.
} // Close the current block.

NP_API int GetActiveTab(HWND hPanel) { // Define GetActiveTab as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    return inst ? inst->activeDoc : -1; // Return the computed inst ? inst->active doc : -1 to the caller.
} // Close the current block.

NP_API BOOL SetActiveTab(HWND hPanel, int index) { // Define SetActiveTab as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return FALSE; // Continue this branch only when the state object for this Notepad panel is unavailable.
    if (index < 0 || index >= (int)inst->docs.size()) return FALSE; // Continue this branch only when the tab or item index is less than 0 or the tab or item index is at least (int)inst->docs.size().
    SetActiveDoc(inst, index); // Call SetActiveDoc to apply the requested state or value.
    return TRUE; // Report success to the caller.
} // Close the current block.

NP_API int OpenDocument(HWND hPanel, const wchar_t* path) { // Define OpenDocument as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return -1; // Continue this branch only when the state object for this Notepad panel is unavailable.
    if (!path) { // Continue this branch only when path evaluates to false.
        DoNewTab(inst); // Call DoNewTab to perform this part of the Notepad operation.
        return (int)inst->docs.size() - 1; // Return the computed (int)inst->docs.size() - 1 to the caller.
    } // Close the current block.
    std::wstring text; // Declare text for use by this block.
    if (!ReadFileText(path, text)) return -1; // Continue this branch only when read file text(path, text) evaluates to false.
    CreateDoc(inst, text, path); // Call CreateDoc to create the requested interface object or resource.
    SetActiveDoc(inst, (int)inst->docs.size() - 1); // Call SetActiveDoc to apply the requested state or value.
    UpdateLayout(inst, hPanel); // Call UpdateLayout to apply the requested state or value.
    InvalidateRect(hPanel, nullptr, TRUE); // Call InvalidateRect to mark the requested area for repainting.
    FireNotify(inst, NP_NOTIFY_TAB_OPENED, (int)inst->docs.size() - 1); // Call FireNotify to perform this part of the Notepad operation.
    return (int)inst->docs.size() - 1; // Return the computed (int)inst->docs.size() - 1 to the caller.
} // Close the current block.

NP_API BOOL CloseTab(HWND hPanel, int index, BOOL promptSave) { // Define CloseTab as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return FALSE; // Continue this branch only when the state object for this Notepad panel is unavailable.
    if (index < 0 || index >= (int)inst->docs.size()) return FALSE; // Continue this branch only when the tab or item index is less than 0 or the tab or item index is at least (int)inst->docs.size().
    if (promptSave && inst->docs[index]->modified) { // Continue this branch only when prompt save is true and inst- is greater than docs[index]->modified.
        if (!ConfirmSaveDoc(inst, inst->docs[index])) return FALSE; // Continue this branch only when confirm save doc(inst, inst->docs[index]) evaluates to false.
    } // Close the current block.
    return CloseDoc(inst, index); // Return the computed close doc(inst, index) to the caller.
} // Close the current block.

NP_API int GetTabFilePathW(HWND hPanel, int index, wchar_t* buf, int bufLen) { // Define GetTabFilePathW as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || index < 0 || index >= (int)inst->docs.size()) return 0; // Continue this branch only when inst || index < 0 || index >= (int)inst->docs.size() evaluates to false.
    std::wstring path = inst->docs[index]->filePath; // Declare path and start with inst->docs[index]->filePath.
    if (!buf) return (int)path.size(); // Continue this branch only when buf evaluates to false.
    int copy = (int)path.size(); // Declare copy and start with (int)path.size().
    if (copy >= bufLen) copy = bufLen - 1; // Continue this branch only when copy is at least bufLen.
    wcsncpy_s(buf, bufLen, path.c_str(), copy); // Call wcsncpy_s to perform this part of the Notepad operation.
    buf[copy] = 0; // Update buf[copy with the result of this expression.
    return copy; // Return the computed copy to the caller.
} // Close the current block.

NP_API int GetTabTitleW(HWND hPanel, int index, wchar_t* buf, int bufLen) { // Define GetTabTitleW as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || index < 0 || index >= (int)inst->docs.size()) return 0; // Continue this branch only when inst || index < 0 || index >= (int)inst->docs.size() evaluates to false.
    std::wstring title = inst->docs[index]->Title(); // Declare title and start with inst->docs[index]->Title().
    if (!buf) return (int)title.size(); // Continue this branch only when buf evaluates to false.
    int copy = (int)title.size(); // Declare copy and start with (int)title.size().
    if (copy >= bufLen) copy = bufLen - 1; // Continue this branch only when copy is at least bufLen.
    wcsncpy_s(buf, bufLen, title.c_str(), copy); // Call wcsncpy_s to perform this part of the Notepad operation.
    buf[copy] = 0; // Update buf[copy with the result of this expression.
    return copy; // Return the computed copy to the caller.
} // Close the current block.

NP_API BOOL IsTabModified(HWND hPanel, int index) { // Define IsTabModified as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || index < 0 || index >= (int)inst->docs.size()) return FALSE; // Continue this branch only when inst || index < 0 || index >= (int)inst->docs.size() evaluates to false.
    return inst->docs[index]->modified ? TRUE : FALSE; // Return the computed inst->docs[index]->modified ? true : false to the caller.
} // Close the current block.

NP_API BOOL SaveTab(HWND hPanel, int index, BOOL saveAs) { // Define SaveTab as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || index < 0 || index >= (int)inst->docs.size()) return FALSE; // Continue this branch only when inst || index < 0 || index >= (int)inst->docs.size() evaluates to false.
    SetActiveDoc(inst, index); // Call SetActiveDoc to apply the requested state or value.
    DoSave(inst, saveAs != FALSE); // Call DoSave to perform this part of the Notepad operation.
    return !inst->modified; // Return the computed !inst->modified to the caller.
} // Close the current block.

// ============================================================
//  Exported API: Theme
// ============================================================

NP_API BOOL SetTheme(HWND hPanel, const NPTheme* theme) { // Define SetTheme as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return FALSE; // Continue this branch only when the state object for this Notepad panel is unavailable.
    if (!theme) { inst->theme = DefaultDarkTheme(); } // Continue this branch only when the active editor color palette evaluates to false.
    else { // Run this fallback branch when the previous condition did not match.
        inst->theme.BgApp = theme->bgApp; inst->theme.BgPanel = theme->bgPanel; // Store the new value in the theme.bg app field of the current instance.
        inst->theme.BgCard = theme->bgCard; inst->theme.BgBtn = theme->bgBtn; // Store the new value in the theme.bg card field of the current instance.
        inst->theme.BgBtnHi = theme->bgBtnHi; inst->theme.BgRailHi = theme->bgBtnHi; // Store the new value in the theme.bg btn hi field of the current instance.
        inst->theme.Border = theme->border; inst->theme.BorderSoft = theme->borderSoft; // Store the new value in the theme.border field of the current instance.
        inst->theme.TextHi = theme->textHi; inst->theme.TextMid = theme->textMid; // Store the new value in the theme.text hi field of the current instance.
        inst->theme.TextLow = theme->textLow; inst->theme.Accent = theme->accent; // Store the new value in the theme.text low field of the current instance.
        inst->theme.SelBg = theme->selBg; inst->theme.IconIdle = theme->iconIdle; // Store the new value in the theme.sel bg field of the current instance.
        inst->theme.IconActive = theme->iconActive; // Store the new value in the theme.icon active field of the current instance.
        // Apply to RichEdit
        for (Document* d : inst->docs) { // Visit each item in the collection and apply the loop body.
            if (d->hEdit) { // Continue this branch only when d- is greater than hEdit.
                SendMessageW(d->hEdit, EM_SETBKGNDCOLOR, 0, inst->theme.BgApp); // Call SendMessageW to send a control message and receive its result immediately.
                CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf); // Update the cb size field with the calculated value.
                cf.dwMask = CFM_COLOR; cf.crTextColor = inst->theme.TextHi; // Update the dw mask field with the calculated value.
                SendMessageW(d->hEdit, EM_SETCHARFORMAT, SCF_DEFAULT | SCF_ALL, (LPARAM)&cf); // Call SendMessageW to send a control message and receive its result immediately.
            } // Close the current block.
        } // Close the current block.
    } // Close the current block.
    if (inst->hSelf) InvalidateRect(inst->hSelf, nullptr, TRUE); // Continue this branch only when inst- is greater than hSelf.
    return TRUE; // Report success to the caller.
} // Close the current block.

NP_API BOOL GetTheme(HWND hPanel, NPTheme* outTheme) { // Define GetTheme as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || !outTheme) return FALSE; // Continue this branch only when inst || !out theme evaluates to false.
    outTheme->bgApp = inst->theme.BgApp; outTheme->bgPanel = inst->theme.BgPanel; // Store the new value in the bg app field of the current instance.
    outTheme->bgCard = inst->theme.BgCard; outTheme->bgBtn = inst->theme.BgBtn; // Store the new value in the bg card field of the current instance.
    outTheme->bgBtnHi = inst->theme.BgBtnHi; outTheme->border = inst->theme.Border; // Store the new value in the bg btn hi field of the current instance.
    outTheme->borderSoft = inst->theme.BorderSoft; outTheme->textHi = inst->theme.TextHi; // Store the new value in the border soft field of the current instance.
    outTheme->textMid = inst->theme.TextMid; outTheme->textLow = inst->theme.TextLow; // Store the new value in the text mid field of the current instance.
    outTheme->accent = inst->theme.Accent; outTheme->selBg = inst->theme.SelBg; // Store the new value in the accent field of the current instance.
    outTheme->iconIdle = inst->theme.IconIdle; outTheme->iconActive = inst->theme.IconActive; // Store the new value in the icon idle field of the current instance.
    return TRUE; // Report success to the caller.
} // Close the current block.

// ============================================================
//  Exported API: Font
// ============================================================

NP_API BOOL SetFontW(HWND hPanel, const LOGFONTW* logFont) { // Define SetFontW as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return FALSE; // Continue this branch only when the state object for this Notepad panel is unavailable.
    if (!logFont) { // Continue this branch only when log font evaluates to false.
        ZeroMemory(&inst->logFont, sizeof(inst->logFont)); // Call ZeroMemory to perform this part of the Notepad operation.
        wcscpy_s(inst->logFont.lfFaceName, L"Segoe UI"); // Call wcscpy_s to perform this part of the Notepad operation.
        inst->logFont.lfHeight = -MulDiv(11, inst->dpi, 72); // Store the new value in the log font.lf height field of the current instance.
        inst->logFont.lfWeight = FW_NORMAL; // Store the new value in the log font.lf weight field of the current instance.
        inst->logFont.lfCharSet = DEFAULT_CHARSET; // Store the new value in the log font.lf char set field of the current instance.
        inst->logFont.lfQuality = CLEARTYPE_QUALITY; // Store the new value in the log font.lf quality field of the current instance.
    } else { // Continue the current declaration or expression.
        inst->logFont = *logFont; // Store the new value in the log font field of the current instance.
    } // Close the current block.
    ApplyEditorFont(inst); // Call ApplyEditorFont to apply the requested state or value.
    return TRUE; // Report success to the caller.
} // Close the current block.

NP_API BOOL GetFontW(HWND hPanel, LOGFONTW* outLogFont) { // Define GetFontW as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || !outLogFont) return FALSE; // Continue this branch only when inst || !out log font evaluates to false.
    *outLogFont = inst->logFont; // Update out log font with the result of this expression.
    return TRUE; // Report success to the caller.
} // Close the current block.

// ============================================================
//  Exported API: View settings
// ============================================================

NP_API BOOL SetWordWrap(HWND hPanel, BOOL wrap) { // Define SetWordWrap as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return FALSE; // Continue this branch only when the state object for this Notepad panel is unavailable.
    inst->wordWrap = wrap != FALSE; SyncToActive(inst); ApplyWordWrap(inst); // Complete the current operation before proceeding to the next statement.
    return TRUE; // Report success to the caller.
} // Close the current block.
NP_API BOOL GetWordWrap(HWND hPanel) { // Define GetWordWrap as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    return inst ? (inst->wordWrap ? TRUE : FALSE) : FALSE; // Return the computed inst ? (inst->word wrap ? true : false) : false to the caller.
} // Close the current block.
NP_API BOOL SetZoom(HWND hPanel, int percent) { // Define SetZoom as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return FALSE; // Continue this branch only when the state object for this Notepad panel is unavailable.
    if (percent < 30) percent = 30; if (percent > 500) percent = 500; // Continue this branch only when percent is less than 30.
    inst->zoom = percent; SyncToActive(inst); ApplyZoom(inst); // Store the new value in the the current text zoom percentage field of the current instance.
    FireNotify(inst, NP_NOTIFY_ZOOM_CHANGED, inst->activeDoc); // Call FireNotify to perform this part of the Notepad operation.
    return TRUE; // Report success to the caller.
} // Close the current block.
NP_API int GetZoom(HWND hPanel) { // Define GetZoom as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    return inst ? inst->zoom : 0; // Return the computed inst ? inst->zoom : 0 to the caller.
} // Close the current block.
NP_API BOOL SetStatusBarVisible(HWND hPanel, BOOL visible) { // Define SetStatusBarVisible as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return FALSE; // Continue this branch only when the state object for this Notepad panel is unavailable.
    inst->showStatus = visible != FALSE; // Complete the current operation before proceeding to the next statement.
    UpdateLayout(inst, hPanel); InvalidateRect(hPanel, nullptr, TRUE); // Call UpdateLayout to apply the requested state or value.
    return TRUE; // Report success to the caller.
} // Close the current block.
NP_API BOOL GetStatusBarVisible(HWND hPanel) { // Define GetStatusBarVisible as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    return inst ? (inst->showStatus ? TRUE : FALSE) : FALSE; // Return the computed inst ? (inst->show status ? true : false) : false to the caller.
} // Close the current block.
NP_API BOOL SetToolbarVisible(HWND hPanel, BOOL visible) { // Define SetToolbarVisible as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst || !inst->hToolbarHost) return FALSE; // Continue this branch only when inst || !inst->h toolbar host evaluates to false.
    inst->tbVisible = visible != FALSE; // Store the requested state independently of parent visibility.
    ShowWindow(inst->hToolbarHost, inst->tbVisible ? SW_SHOW : SW_HIDE); // Show or hide the toolbar to match its state.
    UpdateLayout(inst, hPanel); InvalidateRect(hPanel, nullptr, TRUE); // Call UpdateLayout to apply the requested state or value.
    return TRUE; // Report success to the caller.
} // Close the current block.
NP_API BOOL GetToolbarVisible(HWND hPanel) { // Define GetToolbarVisible as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    return (inst && inst->hToolbarHost && inst->tbVisible) ? TRUE : FALSE; // Return the explicit toolbar state.
} // Close the current block.

// ============================================================
//  Exported API: Read-only
// ============================================================

NP_API BOOL SetReadOnly(HWND hPanel, BOOL readOnly) { // Define SetReadOnly as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return FALSE; // Continue this branch only when the state object for this Notepad panel is unavailable.
    inst->readOnly = readOnly != FALSE; // Complete the current operation before proceeding to the next statement.
    for (Document* d : inst->docs) // Visit each item in the collection and apply the loop body.
        if (d->hEdit) SendMessageW(d->hEdit, EM_SETREADONLY, inst->readOnly ? TRUE : FALSE, 0); // Continue this branch only when d- is greater than hEdit.
    if (inst->hSelf) InvalidateRect(inst->hSelf, nullptr, TRUE); // Continue this branch only when inst- is greater than hSelf.
    return TRUE; // Report success to the caller.
} // Close the current block.
NP_API BOOL GetReadOnly(HWND hPanel) { // Define GetReadOnly as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    return inst ? (inst->readOnly ? TRUE : FALSE) : FALSE; // Return the computed inst ? (inst->read only ? true : false) : false to the caller.
} // Close the current block.

// ============================================================
//  Exported API: Notifications
// ============================================================

NP_API BOOL SetNotifyProc(HWND hPanel, NPNOTIFYPROC proc, void* userData) { // Define SetNotifyProc as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return FALSE; // Continue this branch only when the state object for this Notepad panel is unavailable.
    inst->notifyProc = proc; // Store the new value in the the host callback used for panel notifications field of the current instance.
    inst->notifyUserData = userData; // Store the new value in the the host value passed back with notifications field of the current instance.
    return TRUE; // Report success to the caller.
} // Close the current block.

// ============================================================
//  Exported API: Keyboard accelerators
// ============================================================

static const NPAccel s_accelerators[] = { // Declare s accelerators and start with {.
    { IDM_NEW,        FVIRTKEY|FCONTROL, 'N' }, // Supply this parameter or list item to the enclosing call.
    { IDM_NEW,        FVIRTKEY|FCONTROL, 'T' }, // Supply this parameter or list item to the enclosing call.
    { IDM_OPEN,       FVIRTKEY|FCONTROL, 'O' }, // Supply this parameter or list item to the enclosing call.
    { IDM_SAVE,       FVIRTKEY|FCONTROL, 'S' }, // Supply this parameter or list item to the enclosing call.
    { IDM_SAVEAS,     FVIRTKEY|FCONTROL|FSHIFT, 'S' }, // Supply this parameter or list item to the enclosing call.
    { IDM_UNDO,       FVIRTKEY|FCONTROL, 'Z' }, // Supply this parameter or list item to the enclosing call.
    { IDM_REDO,       FVIRTKEY|FCONTROL, 'Y' }, // Supply this parameter or list item to the enclosing call.
    { IDM_CUT,        FVIRTKEY|FCONTROL, 'X' }, // Supply this parameter or list item to the enclosing call.
    { IDM_COPY,       FVIRTKEY|FCONTROL, 'C' }, // Supply this parameter or list item to the enclosing call.
    { IDM_PASTE,      FVIRTKEY|FCONTROL, 'V' }, // Supply this parameter or list item to the enclosing call.
    { IDM_SELECTALL,  FVIRTKEY|FCONTROL, 'A' }, // Supply this parameter or list item to the enclosing call.
    { IDM_FIND,       FVIRTKEY|FCONTROL, 'F' }, // Supply this parameter or list item to the enclosing call.
    { IDM_REPLACE,    FVIRTKEY|FCONTROL, 'H' }, // Supply this parameter or list item to the enclosing call.
    { IDM_FINDNEXT,   FVIRTKEY,          VK_F3 }, // Supply this parameter or list item to the enclosing call.
    { IDM_DATETIME,   FVIRTKEY,          VK_F5 }, // Supply this parameter or list item to the enclosing call.
    { IDM_ZOOM_IN,    FVIRTKEY|FCONTROL, VK_OEM_PLUS }, // Supply this parameter or list item to the enclosing call.
    { IDM_ZOOM_OUT,   FVIRTKEY|FCONTROL, VK_OEM_MINUS }, // Supply this parameter or list item to the enclosing call.
    { IDM_ZOOM_RESET, FVIRTKEY|FCONTROL, '0' }, // Supply this parameter or list item to the enclosing call.
    { IDM_WRAP,       0,                 0 },  // no default key
    { IDM_FONT,       0,                 0 }, // Supply this parameter or list item to the enclosing call.
    { IDM_STATUS,     0,                 0 }, // Supply this parameter or list item to the enclosing call.
    { IDM_TOOLBAR,    0,                 0 }, // Supply this parameter or list item to the enclosing call.
}; // Finish the current type declaration.
static const int s_accelCount = sizeof(s_accelerators) / sizeof(s_accelerators[0]); // Declare s accel count and start with sizeof(s_accelerators) / sizeof(s_accelerators[0]).

NP_API int GetAccelerators(NPAccel* outAccels, int maxCount) { // Define GetAccelerators as the function that implements this part of the panel.
    if (!outAccels) return s_accelCount; // Continue this branch only when out accels evaluates to false.
    int copy = (maxCount < s_accelCount) ? maxCount : s_accelCount; // Declare copy and start with (maxCount < s_accelCount) ? maxCount : s_accelCount.
    for (int i = 0; i < copy; ++i) outAccels[i] = s_accelerators[i]; // Repeat the body using int i = 0 and stop when i < copy.
    return copy; // Return the computed copy to the caller.
} // Close the current block.

NP_API void SetSuppressedShortcuts(HWND hPanel, const int* cmdIds, int count) { // Define SetSuppressedShortcuts as the function that implements this part of the panel.
    NotepadInstance* inst = GetInstance(hPanel); // Declare the state object for this Notepad panel and start with GetInstance(hPanel).
    if (!inst) return; // Continue this branch only when the state object for this Notepad panel is unavailable.
    inst->suppressedCmds.clear(); // Complete the current operation before proceeding to the next statement.
    if (cmdIds && count > 0) // Continue this branch only when cmd ids is true and count is greater than 0.
        for (int i = 0; i < count; ++i) inst->suppressedCmds.insert(cmdIds[i]); // Repeat the body using int i = 0 and stop when i < count.
} // Close the current block.
