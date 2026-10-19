// NotepadPanel.cpp -- Embeddable Notepad clone panel DLL
//
// Full refactor of the standalone Notepad clone into a DLL that can be
// embedded as a child window inside a host application.  All per-editor
// state lives in a NotepadInstance struct resolved from the panel HWND
// via GWLP_USERDATA.  The public C API is exported via __declspec(dllexport).
//
// Build: configure as a DLL with NOTEPAD_PANEL_EXPORTS defined.
//        Links comctl32, shell32, dwmapi, uxtheme, msftedit (delay-load).

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <richedit.h>
#include <shellapi.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cwchar>
#include <unordered_set>

#ifndef NOTEPAD_PANEL_EXPORTS
#define NOTEPAD_PANEL_EXPORTS
#endif
#include "NotepadPanel.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;

// ============================================================
//  Constants
// ============================================================
static const wchar_t PANEL_CLASS[]  = L"NotepadPanel";
static const wchar_t TOOLBAR_CLASS[] = L"NotepadPanelToolbar";
static const wchar_t MENU_CLASS[]   = L"NotepadPanelMenu";
static const wchar_t APP_NAME[]     = L"Neage Notepad";
static const wchar_t REG_KEY[]      = L"Software\\ModernNotepad";

// Control IDs (same values as the standalone, kept for compatibility)
#define IDC_EDIT            1001
#define IDC_SEARCH_FIND     1010
#define IDC_SEARCH_REPL     1011
#define IDC_SEARCH_NEXT     1012
#define IDC_SEARCH_PREV     1013
#define IDC_SEARCH_CLOSE    1014
#define IDC_SEARCH_CASE     1015
#define IDC_SEARCH_REPL_ONE 1016
#define IDC_SEARCH_REPL_ALL 1017
#define IDC_TB_MENU     1100
#define IDC_TB_NEW      1101
#define IDC_TB_OPEN     1102
#define IDC_TB_SAVE     1103
#define IDC_TB_UNDO     1104
#define IDC_TB_REDO     1105
#define IDC_TB_FIND     1106
#define IDC_TB_ZOOM_OUT 1107
#define IDC_TB_ZOOM_IN  1108
#define IDC_TB_WRAP     1109
// IDM_* match NP_CMD_* values in the public header
#define IDM_NEW          NP_CMD_NEW
#define IDM_OPEN         NP_CMD_OPEN
#define IDM_SAVE         NP_CMD_SAVE
#define IDM_SAVEAS       NP_CMD_SAVEAS
#define IDM_EXIT         NP_CMD_EXIT
#define IDM_UNDO         NP_CMD_UNDO
#define IDM_REDO         NP_CMD_REDO
#define IDM_CUT          NP_CMD_CUT
#define IDM_COPY         NP_CMD_COPY
#define IDM_PASTE        NP_CMD_PASTE
#define IDM_DELETE       NP_CMD_DELETE
#define IDM_SELECTALL    NP_CMD_SELECTALL
#define IDM_DATETIME     NP_CMD_DATETIME
#define IDM_FIND         NP_CMD_FIND
#define IDM_REPLACE      NP_CMD_REPLACE
#define IDM_FINDNEXT     NP_CMD_FINDNEXT
#define IDM_WRAP         NP_CMD_WRAP
#define IDM_FONT         NP_CMD_FONT
#define IDM_ZOOM_IN      NP_CMD_ZOOM_IN
#define IDM_ZOOM_OUT     NP_CMD_ZOOM_OUT
#define IDM_ZOOM_RESET   NP_CMD_ZOOM_RESET
#define IDM_STATUS       NP_CMD_STATUS
#define IDM_TOOLBAR      NP_CMD_TOOLBAR
#define IDM_ABOUT        2050
#define IDM_TB_DOCK_TOP     3001
#define IDM_TB_DOCK_BOTTOM  3002
#define IDM_TB_DOCK_LEFT    3003
#define IDM_TB_DOCK_RIGHT   3004
#define IDM_TB_DOCK_FLOAT   3005
#define IDM_TB_SIZE_SMALL   3011
#define IDM_TB_SIZE_MEDIUM  3012
#define IDM_TB_SIZE_LARGE   3013
#define IDM_TAB_RENAME      4001
#define IDM_TAB_CLOSE       4002
#define IDM_TAB_CLOSE_OTHERS 4003
#define IDM_TAB_CLOSE_RIGHT  4004
#define IDM_TAB_CLOSE_ALL    4005
#define IDM_TAB_SAVE_AS     4006
#define IDM_TAB_REVEAL      4007
#define WM_RENAME_COMMIT (WM_APP + 1)

// ============================================================
//  Enums
// ============================================================
enum ToolbarDock {
    TBDOCK_TOP = 0, TBDOCK_BOTTOM, TBDOCK_LEFT,
    TBDOCK_RIGHT, TBDOCK_FLOAT
};
enum ToolbarSize { TBSIZE_SMALL = 0, TBSIZE_MEDIUM, TBSIZE_LARGE };

// ============================================================
//  Default theme palette
// ============================================================
struct Palette {
    COLORREF BgApp, BgPanel, BgCard, BgBtn, BgBtnHi, BgRailHi;
    COLORREF Border, BorderSoft, TextHi, TextMid, TextLow;
    COLORREF Accent, SelBg, IconIdle, IconActive;
};

static Palette DefaultDarkTheme() {
    Palette t;
    t.BgApp      = RGB(22, 22, 25);
    t.BgPanel    = RGB(30, 30, 34);
    t.BgCard     = RGB(38, 38, 43);
    t.BgBtn      = RGB(48, 48, 54);
    t.BgBtnHi    = RGB(60, 60, 66);
    t.BgRailHi   = RGB(52, 52, 58);
    t.Border     = RGB(56, 56, 62);
    t.BorderSoft = RGB(44, 44, 48);
    t.TextHi     = RGB(232, 232, 236);
    t.TextMid    = RGB(160, 160, 168);
    t.TextLow    = RGB(108, 108, 116);
    t.Accent     = RGB(96, 165, 250);
    t.SelBg      = RGB(48, 70, 100);
    t.IconIdle   = RGB(200, 200, 208);
    t.IconActive = RGB(255, 255, 255);
    return t;
}

// ============================================================
//  Document model (unchanged struct, but no longer global)
// ============================================================
struct Document {
    HWND         hEdit = nullptr;
    std::wstring filePath;
    std::wstring titleOverride;
    bool         modified = false;
    int          zoom = 100;
    bool         wordWrap = true;
    bool         readOnly = false;

    std::wstring Title() const {
        std::wstring name;
        if (!titleOverride.empty()) name = titleOverride;
        else if (filePath.empty())  name = L"Untitled";
        else {
            size_t p = filePath.find_last_of(L"\\/");
            name = (p == std::wstring::npos) ? filePath : filePath.substr(p + 1);
        }
        if (modified) name = L"\u25CF " + name;
        return name;
    }
    std::wstring RenameInitial() const {
        if (!titleOverride.empty()) return titleOverride;
        if (filePath.empty())       return L"Untitled";
        fs::path p(filePath);
        std::wstring stem = p.stem().wstring();
        return stem.empty() ? p.filename().wstring() : stem;
    }
};

// ============================================================
//  Tab hit-test / menu / button types (unchanged)
// ============================================================
struct TabRect {
    RECT rc;
    RECT rcClose;
    int  index = -1;
};

struct MenuItem {
    int          id = 0;
    std::wstring label;
    std::wstring shortcut;
    bool         separator = false;
    bool         checked = false;
    bool         disabled = false;
};

enum class Icon {
    New, Open, Save, Undo, Redo, Find, Menu, ZoomIn, ZoomOut, Wrap,
    Close, ChevronDown, Case, Replace, ReplaceAll, Prev, Next, Check, Plus,
};

struct BtnStyle {
    Icon icon;
    bool chipBg;
    bool rail;
    bool active;
    COLORREF fg;
};

// ============================================================
//  Instance context -- replaces all former globals
// ============================================================
struct NotepadInstance {
    // Window handles
    HWND        hSelf = nullptr;
    HWND        hParent = nullptr;
    HINSTANCE   hInst = nullptr;
    int         instanceId = 0;

    // Documents
    std::vector<Document*> docs;
    int                     activeDoc = -1;

    // Aliases to the active document
    HWND         hEdit = nullptr;
    std::wstring currentFile;
    bool         modified = false;
    int          zoom = 100;
    bool         wordWrap = true;
    bool         readOnly = false;

    // Search bar state
    bool         searchOpen = false;
    bool         replaceOpen = false;
    bool         searchCase = false;
    HWND         hSearchFind = nullptr;
    HWND         hSearchRepl = nullptr;
    HWND         hSearchNext = nullptr;
    HWND         hSearchPrev = nullptr;
    HWND         hSearchClose = nullptr;
    HWND         hSearchCase = nullptr;
    HWND         hSearchReplOne = nullptr;
    HWND         hSearchReplAll = nullptr;

    // Toolbar state
    ToolbarDock  tbDock = TBDOCK_TOP;
    ToolbarSize  tbSize = TBSIZE_MEDIUM;
    bool         tbFloatHoriz = true;
    bool         tbVisible = true;
    HFONT        iconFont = nullptr;
    HWND         hToolbarHost = nullptr;
    HWND         hTbBtns[10] = {};
    HWND         hotBtn = nullptr;

    // Tab bar state
    std::vector<TabRect> tabRects;
    RECT                 tabNewBtnRect{};
    int                  tabHot = -1;
    int                  tabCloseHot = -1;
    bool                 trackingMouse = false;

    // Inline rename state
    HWND         hRenameEdit = nullptr;
    int          renamingTab = -1;

    // Layout rects
    RECT         tabBarRect{};
    RECT         searchRect{};
    RECT         statusRect{};
    RECT         editRect{};

    // Display state
    bool         showStatus = true;
    int          dpi = 96;

    // Fonts
    HFONT        font = nullptr;
    HFONT        fontBold = nullptr;
    HFONT        fontSmall = nullptr;
    LOGFONTW     logFont{};

    // Theme
    Palette      theme;

    // Callback
    NPNOTIFYPROC notifyProc = nullptr;
    void*        notifyUserData = nullptr;

    // Suppressed shortcuts
    std::unordered_set<int> suppressedCmds;

    // Menu state (transient, per ShowThemedMenu call)
    bool                   menuDone = false;
    int                    menuResult = 0;
    HWND                   menuWnd = nullptr;
    std::vector<MenuItem>* menuItems = nullptr;
    int                    menuHot = -1;
    int                    menuItemH = 0;
    int                    menuPadY = 0;

    // Child mode (WS_CHILD)
    bool         isChild = false;

    // Host-managed settings (skip registry)
    bool         hostManaged = false;

    // Static brush for WM_CTLCOLOR* (per-instance to match theme)
    HBRUSH       brSearchBg = nullptr;
    HBRUSH       brAppBg = nullptr;

    // Instance ID counter
    static int   s_nextId;
};

int NotepadInstance::s_nextId = 1;

// ============================================================
//  Process-global state (shared across all panel instances)
// ============================================================
static HINSTANCE g_hInst = nullptr;
static bool     g_classesRegistered = false;
static HMODULE  g_msftedit = nullptr;

// ============================================================
//  Instance resolution
// ============================================================
static NotepadInstance* GetInstance(HWND hWnd) {
    if (!hWnd) return nullptr;
    return reinterpret_cast<NotepadInstance*>(
        GetWindowLongPtrW(hWnd, GWLP_USERDATA));
}

static inline int Dpi(NotepadInstance* inst, int v) {
    return MulDiv(v, inst->dpi, 96);
}

// ============================================================
//  String helpers
// ============================================================
static std::wstring Widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}
static std::string ToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}
static std::wstring BaseNameW(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? path : path.substr(p + 1);
}

// ============================================================
//  Registry helpers (only used in standalone mode)
// ============================================================
static std::wstring RegReadStr(const wchar_t* name, const std::wstring& def) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return def;
    wchar_t buf[512];
    DWORD sz = sizeof(buf);
    DWORD type = REG_SZ;
    std::wstring out = def;
    if (RegQueryValueExW(k, name, nullptr, &type, (LPBYTE)buf, &sz) == ERROR_SUCCESS) {
        out.assign(buf, sz / sizeof(wchar_t) - 1);
    }
    RegCloseKey(k);
    return out;
}
static void RegWriteStr(const wchar_t* name, const std::wstring& val) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0,
        KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val.c_str(),
        (DWORD)((val.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
}
static int RegReadInt(const wchar_t* name, int def) {
    try { return std::stoi(RegReadStr(name, std::to_wstring(def))); }
    catch (...) { return def; }
}

// ============================================================
//  Notification helper
// ============================================================
static void FireNotify(NotepadInstance* inst, UINT code, int tabIndex) {
    if (!inst) return;
    if (inst->notifyProc) {
        NPNotify n = {};
        n.hPanel     = inst->hSelf;
        n.instanceId = inst->instanceId;
        n.code       = code;
        n.tabIndex   = tabIndex;
        n.userData   = inst->notifyUserData;
        inst->notifyProc(&n);
    } else if (inst->hParent) {
        NMHDR nmh = {};
        nmh.hwndFrom = inst->hSelf;
        nmh.idFrom   = (ULONG_PTR)inst->instanceId;
        nmh.code     = code;
        SendMessageW(inst->hParent, WM_NOTIFY, (WPARAM)nmh.idFrom, (LPARAM)&nmh);
    }
}

// ============================================================
//  GDI helpers (theme-aware via instance)
// ============================================================
static void FillSolid(HDC hdc, const RECT& rc, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
}
static void FillRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF fill) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, fill);
    HGDIOBJ ob = SelectObject(hdc, br);
    HGDIOBJ op = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(br);
    DeleteObject(pen);
}
static void StrokeRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF stroke) {
    HBRUSH br = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN pen = CreatePen(PS_SOLID, 1, stroke);
    HGDIOBJ ob = SelectObject(hdc, br);
    HGDIOBJ op = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(pen);
}
static void Circle(HDC hdc, int cx, int cy, int r, COLORREF fill) {
    HBRUSH br = CreateSolidBrush(fill);
    HGDIOBJ ob = SelectObject(hdc, br);
    HGDIOBJ op = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(br);
}

// ============================================================
//  Icon drawing (unchanged logic, uses instance dpi)
// ============================================================
static void DrawIcon(HDC hdc, NotepadInstance* inst, Icon icon,
                     const RECT& r, COLORREF color, int) {
    static constexpr wchar_t glyphs[] = {
        L'\uEA1A', L'\uE8E5', L'\uE74E', L'\uE7A7', L'\uE7A6', L'\uE721', L'\uE700', L'\uE8A3', L'\uE71F', L'\uE8E4',
        L'\uE711', L'\uE70D', L'\uE8D2', L'\uE8AB', L'\uE8EE', L'\uE892', L'\uE893', L'\uE73E', L'\uE710'
    };
    const size_t glyphIndex = static_cast<size_t>(icon);
    if (!inst || !inst->iconFont || glyphIndex >= (sizeof(glyphs) / sizeof(glyphs[0]))) return;
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, inst->iconFont));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    wchar_t glyphText[2] = { glyphs[glyphIndex], L'\0' };
    RECT glyphRect = r;
    DrawTextW(hdc, glyphText, 1, &glyphRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

// ============================================================
//  Forward declarations
// ============================================================
LRESULT CALLBACK PanelWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ToolbarHostProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MenuProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK RenameEditProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK ToolbarBtnProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

void UpdateLayout(NotepadInstance* inst, HWND hWnd);
void SetToolbarDock(NotepadInstance* inst, ToolbarDock dock);
void SetToolbarSize(NotepadInstance* inst, ToolbarSize size);
void DoSave(NotepadInstance* inst, bool saveAs);
void ApplyEditorFont(NotepadInstance* inst);
void ApplyWordWrap(NotepadInstance* inst);
void ApplyZoom(NotepadInstance* inst);
void ShowSearch(NotepadInstance* inst, bool open, bool withReplace);
void ShowMainMenu(NotepadInstance* inst, HWND hWnd);
void BeginRenameTab(NotepadInstance* inst, int index);
void CommitRename(NotepadInstance* inst, bool accept);
void DispatchCommand(NotepadInstance* inst, HWND hWnd, int cmd);
bool CloseDoc(NotepadInstance* inst, int index);
Document* CreateDoc(NotepadInstance* inst, const std::wstring& initialText,
                    const std::wstring& filePath);
void SetActiveDoc(NotepadInstance* inst, int index);
void SetModified(NotepadInstance* inst, bool modified);
void ShowTabMenu(NotepadInstance* inst, int index, int x, int y);
void ShowToolbarMenu(NotepadInstance* inst, HWND owner, int x, int y);

// ============================================================
//  Document management
// ============================================================
static Document* Active(NotepadInstance* inst) {
    if (inst->activeDoc < 0 || inst->activeDoc >= (int)inst->docs.size()) return nullptr;
    return inst->docs[inst->activeDoc];
}
static void SyncFromActive(NotepadInstance* inst) {
    Document* d = Active(inst); if (!d) return;
    inst->hEdit = d->hEdit;
    inst->currentFile = d->filePath;
    inst->modified = d->modified;
    inst->zoom = d->zoom;
    inst->wordWrap = d->wordWrap;
    inst->readOnly = d->readOnly;
}
static void SyncToActive(NotepadInstance* inst) {
    Document* d = Active(inst); if (!d) return;
    d->filePath = inst->currentFile;
    d->modified = inst->modified;
    d->zoom = inst->zoom;
    d->wordWrap = inst->wordWrap;
    d->readOnly = inst->readOnly;
}

void SetActiveDoc(NotepadInstance* inst, int index) {
    if (index < 0 || index >= (int)inst->docs.size()) return;
    if (index == inst->activeDoc) return;
    if (inst->activeDoc >= 0 && inst->activeDoc < (int)inst->docs.size()) {
        SyncToActive(inst);
        Document* old = inst->docs[inst->activeDoc];
        if (old->hEdit) ShowWindow(old->hEdit, SW_HIDE);
    }
    inst->activeDoc = index;
    SyncFromActive(inst);
    Document* doc = Active(inst);
    if (doc && doc->hEdit) ShowWindow(doc->hEdit, SW_SHOW);
    ApplyWordWrap(inst); ApplyZoom(inst);
    if (!inst->isChild && inst->hSelf) {
        std::wstring title;
        if (inst->currentFile.empty()) title = L"Untitled";
        else title = BaseNameW(inst->currentFile);
        if (inst->modified) title = L"\u25CF " + title;
        title += L"  \u2014  " + std::wstring(APP_NAME);
        SetWindowTextW(inst->hSelf, title.c_str());
    }
    if (inst->hSelf) { UpdateLayout(inst, inst->hSelf); InvalidateRect(inst->hSelf, nullptr, TRUE); }
    if (inst->hEdit) SetFocus(inst->hEdit);
    FireNotify(inst, NP_NOTIFY_TAB_CHANGED, index);
}

Document* CreateDoc(NotepadInstance* inst, const std::wstring& initialText,
                    const std::wstring& filePath) {
    Document* doc = new Document();
    doc->filePath = filePath;
    doc->zoom = inst->zoom;
    doc->wordWrap = inst->wordWrap;
    doc->readOnly = inst->readOnly;
    HWND hParent = inst->hSelf;
    doc->hEdit = CreateWindowExW(0, L"RICHEDIT50W", L"",
        WS_CHILD | WS_TABSTOP | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
        0, 0, 100, 100, hParent, nullptr, inst->hInst, nullptr);
    SendMessageW(doc->hEdit, EM_SETBKGNDCOLOR, 0, inst->theme.BgApp);
    SendMessageW(doc->hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
        MAKELPARAM(Dpi(inst, 16), Dpi(inst, 16)));
    SendMessageW(doc->hEdit, EM_SETUNDOLIMIT, 500, 0);
    SetWindowTheme(doc->hEdit, L"DarkMode_Explorer", nullptr);
    SetWindowSubclass(doc->hEdit, EditProc, 1, (DWORD_PTR)inst);
    if (inst->readOnly) SendMessageW(doc->hEdit, EM_SETREADONLY, TRUE, 0);
    if (!initialText.empty()) {
        SETTEXTEX st{}; st.flags = ST_DEFAULT; st.codepage = 1200;
        std::wstring normalized;
        normalized.reserve(initialText.size() + 16);
        for (wchar_t c : initialText) {
            if (c == L'\r') continue;
            if (c == L'\n') normalized += L"\r\n";
            else normalized += c;
        }
        SendMessageW(doc->hEdit, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)normalized.c_str());
    }
    SendMessageW(doc->hEdit, EM_SETMODIFY, FALSE, 0);
    doc->modified = false;
    CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.crTextColor = inst->theme.TextHi;
    cf.yHeight = (LONG)(MulDiv(inst->logFont.lfHeight, 1440, inst->dpi));
    if (cf.yHeight < 0) cf.yHeight = -cf.yHeight;
    wcscpy_s(cf.szFaceName, inst->logFont.lfFaceName);
    SendMessageW(doc->hEdit, EM_SETCHARFORMAT, SCF_DEFAULT | SCF_ALL, (LPARAM)&cf);
    if (doc->wordWrap) SendMessageW(doc->hEdit, EM_SETTARGETDEVICE, 0, 0);
    else              SendMessageW(doc->hEdit, EM_SETTARGETDEVICE, 0, 1);
    SendMessageW(doc->hEdit, EM_SETZOOM, (WPARAM)doc->zoom, (WPARAM)100);
    inst->docs.push_back(doc);
    return doc;
}

static bool ConfirmSaveDoc(NotepadInstance* inst, Document* doc) {
    if (!doc || !doc->modified) return true;
    std::wstring name = doc->filePath.empty()
        ? (doc->titleOverride.empty() ? L"Untitled" : doc->titleOverride)
        : BaseNameW(doc->filePath);
    std::wstring msg = L"Save changes to " + name + L"?";
    HWND hOwner = inst->isChild ? inst->hParent : inst->hSelf;
    int r = MessageBoxW(hOwner, msg.c_str(), APP_NAME, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) return false;
    if (r == IDNO) return true;
    DoSave(inst, false);
    return !inst->modified;
}

bool CloseDoc(NotepadInstance* inst, int index) {
    if (index < 0 || index >= (int)inst->docs.size()) return false;
    if (inst->hRenameEdit && inst->renamingTab == index) CommitRename(inst, true);
    Document* doc = inst->docs[index];
    bool wasActive = (index == inst->activeDoc);
    if (!wasActive) SetActiveDoc(inst, index);
    if (doc->modified) { if (!ConfirmSaveDoc(inst, doc)) return false; }
    HWND h = doc->hEdit; doc->hEdit = nullptr;
    if (h) DestroyWindow(h);
    inst->docs.erase(inst->docs.begin() + index);
    delete doc;
    FireNotify(inst, NP_NOTIFY_TAB_CLOSED, index);
    if (inst->docs.empty()) {
        inst->activeDoc = -1; inst->hEdit = nullptr;
        CreateDoc(inst, L"", L"");
        SetActiveDoc(inst, 0);
        return true;
    }
    int newActive;
    if (inst->activeDoc == index) newActive = (index < (int)inst->docs.size()) ? index : (int)inst->docs.size() - 1;
    else if (inst->activeDoc > index) newActive = inst->activeDoc - 1;
    else newActive = inst->activeDoc;
    inst->activeDoc = -1;
    SetActiveDoc(inst, newActive);
    return true;
}

static bool CloseAllDocs(NotepadInstance* inst) {
    while (!inst->docs.empty()) { if (!CloseDoc(inst, 0)) return false; }
    return true;
}

// ============================================================
//  Editor helpers
// ============================================================
static int GetCaretLine(NotepadInstance* inst) {
    if (!inst->hEdit) return 1;
    DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0);
    return (int)SendMessageW(inst->hEdit, EM_LINEFROMCHAR, LOWORD(sel), 0) + 1;
}
static int GetCaretCol(NotepadInstance* inst) {
    if (!inst->hEdit) return 1;
    DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0);
    int pos = LOWORD(sel);
    int line = (int)SendMessageW(inst->hEdit, EM_LINEFROMCHAR, pos, 0);
    return pos - (int)SendMessageW(inst->hEdit, EM_LINEINDEX, line, 0) + 1;
}
static int GetLineCount(NotepadInstance* inst) {
    if (!inst->hEdit) return 1;
    int n = (int)SendMessageW(inst->hEdit, EM_GETLINECOUNT, 0, 0);
    return n < 1 ? 1 : n;
}
static int GetCharCount(NotepadInstance* inst) {
    if (!inst->hEdit) return 0;
    GETTEXTLENGTHEX gt{}; gt.flags = GTL_NUMCHARS | GTL_PRECISE; gt.codepage = 1200;
    LRESULT n = SendMessageW(inst->hEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gt, 0);
    return n < 0 ? 0 : (int)n;
}
void ApplyZoom(NotepadInstance* inst) {
    if (!inst->hEdit) return;
    SendMessageW(inst->hEdit, EM_SETZOOM, (WPARAM)inst->zoom, (WPARAM)100);
}
void ApplyWordWrap(NotepadInstance* inst) {
    if (!inst->hEdit) return;
    if (inst->wordWrap) SendMessageW(inst->hEdit, EM_SETTARGETDEVICE, 0, 0);
    else               SendMessageW(inst->hEdit, EM_SETTARGETDEVICE, 0, 1);
}
void ApplyEditorFont(NotepadInstance* inst) {
    CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.crTextColor = inst->theme.TextHi;
    cf.yHeight = (LONG)(MulDiv(inst->logFont.lfHeight, 1440, inst->dpi));
    if (cf.yHeight < 0) cf.yHeight = -cf.yHeight;
    wcscpy_s(cf.szFaceName, inst->logFont.lfFaceName);
    for (Document* d : inst->docs)
        if (d->hEdit)
            SendMessageW(d->hEdit, EM_SETCHARFORMAT, SCF_DEFAULT | SCF_ALL, (LPARAM)&cf);
}
static void UpdateTitle(NotepadInstance* inst) {
    if (inst->isChild || !inst->hSelf) return;
    std::wstring title;
    if (inst->currentFile.empty()) title = L"Untitled";
    else title = BaseNameW(inst->currentFile);
    if (inst->modified) title = L"\u25CF " + title;
    title += L"  \u2014  " + std::wstring(APP_NAME);
    SetWindowTextW(inst->hSelf, title.c_str());
}
void SetModified(NotepadInstance* inst, bool modified) {
    bool prev = inst->modified;
    inst->modified = modified;
    Document* d = Active(inst); if (d) d->modified = modified;
    if (inst->hEdit) SendMessageW(inst->hEdit, EM_SETMODIFY, modified ? TRUE : FALSE, 0);
    UpdateTitle(inst);
    if (inst->hSelf) InvalidateRect(inst->hSelf, &inst->tabBarRect, FALSE);
    if (prev != modified) FireNotify(inst, NP_NOTIFY_MODIFIED_CHANGED, inst->activeDoc);
}

// ============================================================
//  File operations
// ============================================================
static bool ReadFileText(const std::wstring& path, std::wstring& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.size() >= 3 && (unsigned char)data[0]==0xEF && (unsigned char)data[1]==0xBB && (unsigned char)data[2]==0xBF)
        data.erase(0, 3);
    out = Widen(data);
    return true;
}
static bool WriteFileText(const std::wstring& path, const std::wstring& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    const unsigned char bom[3] = {0xEF,0xBB,0xBF};
    f.write((const char*)bom, 3);
    std::string utf8 = ToUtf8(text);
    f.write(utf8.data(), (std::streamsize)utf8.size());
    return true;
}
static std::wstring GetEditorText(NotepadInstance* inst) {
    if (!inst->hEdit) return L"";
    int len = GetWindowTextLengthW(inst->hEdit);
    if (len <= 0) return L"";
    std::vector<wchar_t> buf(len + 2, 0);
    GETTEXTEX gt{}; gt.cb = (DWORD)((len + 2) * sizeof(wchar_t));
    gt.flags = GT_USECRLF; gt.codepage = 1200;
    SendMessageW(inst->hEdit, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)buf.data());
    return std::wstring(buf.data());
}
void DoSave(NotepadInstance* inst, bool saveAs) {
    if (!inst->hEdit) return;
    std::wstring target = inst->currentFile;
    if (target.empty() || saveAs) {
        wchar_t file[MAX_PATH] = L"";
        if (!inst->currentFile.empty()) wcsncpy_s(file, BaseNameW(inst->currentFile).c_str(), _TRUNCATE);
        else {
            Document* d = Active(inst);
            if (d && !d->titleOverride.empty()) {
                std::wstring suggested = d->titleOverride + L".txt";
                wcsncpy_s(file, suggested.c_str(), _TRUNCATE);
            }
        }
        OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = inst->isChild ? inst->hParent : inst->hSelf;
        ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"txt"; ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
        if (!GetSaveFileNameW(&ofn)) return;
        target = file;
    }
    if (!WriteFileText(target, GetEditorText(inst))) {
        MessageBoxW(inst->isChild ? inst->hParent : inst->hSelf,
            L"Could not save the file.", APP_NAME, MB_OK | MB_ICONWARNING);
        return;
    }
    inst->currentFile = target;
    Document* d = Active(inst);
    if (d) { d->filePath = target; d->titleOverride.clear(); }
    SetModified(inst, false);
    if (inst->hSelf) InvalidateRect(inst->hSelf, &inst->tabBarRect, FALSE);
    FireNotify(inst, NP_NOTIFY_FILE_SAVED, inst->activeDoc);
}

static void DoNewTab(NotepadInstance* inst) {
    if (inst->hRenameEdit) CommitRename(inst, true);
    CreateDoc(inst, L"", L"");
    SetActiveDoc(inst, (int)inst->docs.size() - 1);
    UpdateLayout(inst, inst->hSelf);
    InvalidateRect(inst->hSelf, nullptr, TRUE);
    FireNotify(inst, NP_NOTIFY_TAB_OPENED, (int)inst->docs.size() - 1);
}

static void DoOpen(NotepadInstance* inst) {
    if (inst->hRenameEdit) CommitRename(inst, true);
    std::vector<wchar_t> big(32768, 0);
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = inst->isChild ? inst->hParent : inst->hSelf;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = big.data(); ofn.nMaxFile = (DWORD)big.size();
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
    if (!GetOpenFileNameW(&ofn)) return;
    std::vector<std::wstring> paths;
    if (big[0]) {
        std::wstring first(big.data());
        size_t pos = first.size() + 1;
        if (pos < big.size() && big[pos] != 0) {
            std::wstring dir = first;
            while (pos < big.size() && big[pos] != 0) {
                std::wstring fn(big.data() + pos);
                paths.push_back(dir + L"\\" + fn);
                pos += fn.size() + 1;
            }
        } else paths.push_back(first);
    }
    for (const std::wstring& p : paths) {
        std::wstring text;
        if (!ReadFileText(p, text)) {
            std::wstring m = L"Could not read:\r\n" + p;
            MessageBoxW(inst->isChild ? inst->hParent : inst->hSelf,
                m.c_str(), APP_NAME, MB_OK | MB_ICONWARNING);
            continue;
        }
        CreateDoc(inst, text, p);
    }
    if (!paths.empty()) {
        SetActiveDoc(inst, (int)inst->docs.size() - 1);
        UpdateLayout(inst, inst->hSelf);
        InvalidateRect(inst->hSelf, nullptr, TRUE);
        FireNotify(inst, NP_NOTIFY_TAB_OPENED, (int)inst->docs.size() - 1);
    }
}

// ============================================================
//  Search / Replace
// ============================================================
static std::wstring ReadControlText(HWND h) {
    int len = GetWindowTextLengthW(h);
    if (len <= 0) return L"";
    std::vector<wchar_t> buf(len + 1, 0);
    GetWindowTextW(h, buf.data(), len + 1);
    return std::wstring(buf.data());
}
static bool FindNext(NotepadInstance* inst, bool backwards) {
    if (!inst->hEdit || !inst->hSearchFind) return false;
    std::wstring needle = ReadControlText(inst->hSearchFind);
    if (needle.empty()) return false;
    DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0);
    int start = LOWORD(sel), end = HIWORD(sel);
    FINDTEXTEXW ft{}; ft.lpstrText = (LPWSTR)needle.c_str();
    if (backwards) { ft.chrg.cpMin = 0; ft.chrg.cpMax = start; }
    else { ft.chrg.cpMin = end; ft.chrg.cpMax = -1; }
    DWORD flags = backwards ? 0 : FR_DOWN;
    if (inst->searchCase) flags |= FR_MATCHCASE;
    LRESULT pos = SendMessageW(inst->hEdit, EM_FINDTEXTEXW, (WPARAM)flags, (LPARAM)&ft);
    if (pos < 0) {
        if (backwards) { ft.chrg.cpMin = -1; ft.chrg.cpMax = 0; }
        else { ft.chrg.cpMin = 0; ft.chrg.cpMax = -1; }
        pos = SendMessageW(inst->hEdit, EM_FINDTEXTEXW, (WPARAM)flags, (LPARAM)&ft);
        if (pos < 0) return false;
    }
    SendMessageW(inst->hEdit, EM_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax);
    SendMessageW(inst->hEdit, EM_SCROLLCARET, 0, 0);
    SetFocus(inst->hEdit);
    return true;
}
static int ReplaceOne(NotepadInstance* inst) {
    if (!inst->hEdit || !inst->hSearchFind || !inst->hSearchRepl) return 0;
    std::wstring needle = ReadControlText(inst->hSearchFind);
    std::wstring repl = ReadControlText(inst->hSearchRepl);
    if (needle.empty()) return 0;
    DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0);
    int s = LOWORD(sel), e = HIWORD(sel);
    if (e - s == (int)needle.size()) {
        std::wstring cur(e - s, 0);
        TEXTRANGEW tr{}; tr.chrg.cpMin = s; tr.chrg.cpMax = e; tr.lpstrText = cur.data();
        SendMessageW(inst->hEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
        bool match = inst->searchCase ? (cur == needle) :
            std::equal(cur.begin(), cur.end(), needle.begin(),
                [](wchar_t a, wchar_t b) { return towlower(a) == towlower(b); });
        if (!match) { FindNext(inst, false); return 0; }
        SendMessageW(inst->hEdit, EM_REPLACESEL, TRUE, (LPARAM)repl.c_str());
    } else { FindNext(inst, false); return 0; }
    FindNext(inst, false);
    return 1;
}
static int ReplaceAll(NotepadInstance* inst) {
    if (!inst->hEdit || !inst->hSearchFind || !inst->hSearchRepl) return 0;
    std::wstring needle = ReadControlText(inst->hSearchFind);
    std::wstring repl = ReadControlText(inst->hSearchRepl);
    if (needle.empty()) return 0;
    SendMessageW(inst->hEdit, WM_SETREDRAW, FALSE, 0);
    int count = 0;
    FINDTEXTEXW ft{}; ft.chrg.cpMin = 0; ft.chrg.cpMax = -1; ft.lpstrText = (LPWSTR)needle.c_str();
    DWORD flags = FR_DOWN;
    if (inst->searchCase) flags |= FR_MATCHCASE;
    for (;;) {
        LRESULT pos = SendMessageW(inst->hEdit, EM_FINDTEXTEXW, (WPARAM)flags, (LPARAM)&ft);
        if (pos < 0) break;
        SendMessageW(inst->hEdit, EM_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax);
        SendMessageW(inst->hEdit, EM_REPLACESEL, TRUE, (LPARAM)repl.c_str());
        ft.chrg.cpMin = ft.chrgText.cpMin + (LONG)repl.size();
        ft.chrg.cpMax = -1; ++count;
        if (count > 100000) break;
    }
    SendMessageW(inst->hEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(inst->hEdit, nullptr, TRUE);
    if (count) SetModified(inst, true);
    return count;
}

// ============================================================
//  Tab bar layout + paint
// ============================================================
static void LayoutTabs(NotepadInstance* inst) {
    inst->tabRects.clear();
    inst->tabNewBtnRect = {};
    if (inst->docs.empty()) return;
    HDC hdc = CreateCompatibleDC(nullptr);
    HFONT old = (HFONT)SelectObject(hdc, inst->font);
    SetBkMode(hdc, TRANSPARENT);
    int pad = Dpi(inst, 8), gap = Dpi(inst, 4);
    int x = inst->tabBarRect.left + pad;
    int y = inst->tabBarRect.top + Dpi(inst, 4);
    int h = inst->tabBarRect.bottom - inst->tabBarRect.top - Dpi(inst, 8);
    if (h < Dpi(inst, 20)) h = Dpi(inst, 20);
    int minTabW = Dpi(inst, 110), maxTabW = Dpi(inst, 220);
    int rightLimit = inst->tabBarRect.right - Dpi(inst, 8) - h - Dpi(inst, 8);
    for (size_t i = 0; i < inst->docs.size(); ++i) {
        std::wstring title = inst->docs[i]->Title();
        SIZE sz{}; GetTextExtentPoint32W(hdc, title.c_str(), (int)title.size(), &sz);
        int w = sz.cx + Dpi(inst, 10) + Dpi(inst, 22) + Dpi(inst, 10);
        if (w < minTabW) w = minTabW; if (w > maxTabW) w = maxTabW;
        if (x + w > rightLimit) break;
        TabRect tr{}; tr.index = (int)i;
        tr.rc = { x, y, x + w, y + h };
        tr.rcClose = { tr.rc.right - Dpi(inst, 20), tr.rc.top + Dpi(inst, 4),
                       tr.rc.right - Dpi(inst, 4), tr.rc.bottom - Dpi(inst, 4) };
        inst->tabRects.push_back(tr);
        x += w + gap;
    }
    SelectObject(hdc, old); DeleteDC(hdc);
    inst->tabNewBtnRect = { x, y, x + h, y + h };
    if (inst->hRenameEdit && inst->renamingTab >= 0) {
        for (const TabRect& tr : inst->tabRects) {
            if (tr.index == inst->renamingTab) {
                SetWindowPos(inst->hRenameEdit, nullptr,
                    tr.rc.left + Dpi(inst, 8), tr.rc.top + Dpi(inst, 6),
                    tr.rcClose.left - Dpi(inst, 2) - (tr.rc.left + Dpi(inst, 8)),
                    tr.rc.bottom - Dpi(inst, 6) - (tr.rc.top + Dpi(inst, 6)),
                    SWP_NOZORDER | SWP_NOACTIVATE);
                break;
            }
        }
    }
}

static void PaintTabs(NotepadInstance* inst, HDC hdc) {
    RECT rc = inst->tabBarRect;
    FillSolid(hdc, rc, inst->theme.BgPanel);
    RECT sep = { rc.left, rc.bottom - 1, rc.right, rc.bottom };
    FillSolid(hdc, sep, inst->theme.BorderSoft);
    if (inst->docs.empty()) return;
    LayoutTabs(inst);
    int saved = SaveDC(hdc);
    IntersectClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);
    for (const TabRect& tr : inst->tabRects) {
        bool active = (tr.index == inst->activeDoc);
        bool hover = (tr.index == inst->tabHot) && !active;
        COLORREF bg = active ? inst->theme.BgCard
            : (hover ? inst->theme.BgRailHi : inst->theme.BgPanel);
        FillRoundRect(hdc, tr.rc, Dpi(inst, 6), bg);
        if (!active && !hover) StrokeRoundRect(hdc, tr.rc, Dpi(inst, 6), inst->theme.BorderSoft);
        if (active) {
            RECT bar = { tr.rc.left + Dpi(inst, 6), tr.rc.bottom - Dpi(inst, 2),
                         tr.rc.right - Dpi(inst, 6), tr.rc.bottom };
            FillSolid(hdc, bar, inst->theme.Accent);
        }
        Document* d = inst->docs[tr.index];
        std::wstring title = d->Title();
        bool renamingThis = (tr.index == inst->renamingTab && inst->hRenameEdit);
        if (!renamingThis) {
            HFONT old = (HFONT)SelectObject(hdc, inst->font);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, active ? inst->theme.TextHi : inst->theme.TextMid);
            RECT txt = { tr.rc.left + Dpi(inst, 10), tr.rc.top,
                         tr.rcClose.left - Dpi(inst, 2), tr.rc.bottom };
            DrawTextW(hdc, title.c_str(), -1, &txt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, old);
        }
        bool showClose = (active || tr.index == inst->tabHot || tr.index == inst->tabCloseHot) && !renamingThis;
        if (showClose) {
            RECT x = tr.rcClose;
            if (tr.index == inst->tabCloseHot)
                Circle(hdc, (x.left + x.right) / 2, (x.top + x.bottom) / 2,
                    (x.right - x.left) / 2, inst->theme.BgRailHi);
            DrawIcon(hdc, inst, Icon::Close, x,
                (tr.index == inst->tabCloseHot) ? inst->theme.TextHi : inst->theme.TextMid, 2);
        }
    }
    {
        bool newHover = (inst->tabHot == -2);
        if (newHover) FillRoundRect(hdc, inst->tabNewBtnRect, Dpi(inst, 6), inst->theme.BgRailHi);
        RECT ir = inst->tabNewBtnRect;
        int inset = (ir.right - ir.left) / 4;
        RECT i2 = { ir.left + inset, ir.top + inset, ir.right - inset, ir.bottom - inset };
        DrawIcon(hdc, inst, Icon::Plus, i2, newHover ? inst->theme.TextHi : inst->theme.TextMid, 2);
    }
    RestoreDC(hdc, saved);
}

static int HitTestTab(NotepadInstance* inst, int x, int y, bool& onClose) {
    onClose = false;
    POINT pt = { x, y };
    if (PtInRect(&inst->tabNewBtnRect, pt)) return -2;
    for (const TabRect& tr : inst->tabRects) {
        if (PtInRect(&tr.rc, pt)) {
            if (PtInRect(&tr.rcClose, pt)) onClose = true;
            return tr.index;
        }
    }
    return -1;
}

// ============================================================
//  Inline rename
// ============================================================
LRESULT CALLBACK RenameEditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    NotepadInstance* inst = GetInstance(GetParent(h));
    switch (m) {
    case WM_KEYDOWN:
        if (w == VK_RETURN) { PostMessageW(GetParent(h), WM_RENAME_COMMIT, 1, 0); return 0; }
        if (w == VK_ESCAPE) { PostMessageW(GetParent(h), WM_RENAME_COMMIT, 0, 0); return 0; }
        break;
    case WM_KILLFOCUS:
        PostMessageW(GetParent(h), WM_RENAME_COMMIT, 1, 0);
        break;
    case WM_CHAR:
        if (w == VK_RETURN || w == VK_ESCAPE) return 0;
        break;
    }
    return DefSubclassProc(h, m, w, l);
}

void BeginRenameTab(NotepadInstance* inst, int index) {
    if (index < 0 || index >= (int)inst->docs.size()) return;
    if (inst->hRenameEdit) CommitRename(inst, true);
    LayoutTabs(inst);
    TabRect foundTab{}; bool found = false;
    for (const TabRect& tr : inst->tabRects) {
        if (tr.index == index) { foundTab = tr; found = true; break; }
    }
    if (!found) return;
    Document* d = inst->docs[index];
    std::wstring initial = d->RenameInitial();
    RECT r;
    r.left = foundTab.rc.left + Dpi(inst, 8);
    r.right = foundTab.rcClose.left - Dpi(inst, 2);
    r.top = foundTab.rc.top + Dpi(inst, 6);
    r.bottom = foundTab.rc.bottom - Dpi(inst, 6);
    if (r.right <= r.left + Dpi(inst, 20)) return;
    inst->hRenameEdit = CreateWindowExW(0, L"EDIT", initial.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER,
        r.left, r.top, r.right - r.left, r.bottom - r.top,
        inst->hSelf, nullptr, inst->hInst, nullptr);
    SendMessageW(inst->hRenameEdit, WM_SETFONT, (WPARAM)inst->font, TRUE);
    SetWindowTheme(inst->hRenameEdit, L"DarkMode_Explorer", nullptr);
    SetWindowSubclass(inst->hRenameEdit, RenameEditProc, 200, 0);
    inst->renamingTab = index;
    SetFocus(inst->hRenameEdit);
    SendMessageW(inst->hRenameEdit, EM_SETSEL, 0, -1);
    InvalidateRect(inst->hSelf, &inst->tabBarRect, FALSE);
}

void CommitRename(NotepadInstance* inst, bool accept) {
    if (!inst->hRenameEdit) return;
    int index = inst->renamingTab;
    std::wstring newName;
    if (accept) {
        int len = GetWindowTextLengthW(inst->hRenameEdit);
        std::vector<wchar_t> buf(len + 2, 0);
        GetWindowTextW(inst->hRenameEdit, buf.data(), len + 1);
        newName.assign(buf.data());
        size_t a = 0, b = newName.size();
        while (a < b && (newName[a] == L' ' || newName[a] == L'\t')) ++a;
        while (b > a && (newName[b-1] == L' ' || newName[b-1] == L'\t')) --b;
        newName = (a < b) ? newName.substr(a, b - a) : L"";
    }
    HWND edit = inst->hRenameEdit;
    inst->hRenameEdit = nullptr; inst->renamingTab = -1;
    if (edit) { RemoveWindowSubclass(edit, RenameEditProc, 200); DestroyWindow(edit); }
    if (!accept || newName.empty()) {
        InvalidateRect(inst->hSelf, &inst->tabBarRect, FALSE);
        if (inst->hEdit) SetFocus(inst->hEdit);
        return;
    }
    Document* d = (index >= 0 && index < (int)inst->docs.size()) ? inst->docs[index] : nullptr;
    if (!d) { if (inst->hEdit) SetFocus(inst->hEdit); return; }
    if (d->filePath.empty()) {
        d->titleOverride = newName;
    } else {
        fs::path oldP(d->filePath);
        std::wstring ext = oldP.extension().wstring();
        if (ext.empty()) ext = L".txt";
        fs::path newP = oldP.parent_path() / (newName + ext);
        if (newP == oldP) { /* no change */ }
        else {
            std::error_code ec;
            if (fs::exists(newP, ec)) {
                MessageBoxW(inst->isChild ? inst->hParent : inst->hSelf,
                    L"A file with that name already exists in this folder.",
                    APP_NAME, MB_OK | MB_ICONWARNING);
            } else {
                fs::rename(oldP, newP, ec);
                if (ec) {
                    MessageBoxW(inst->isChild ? inst->hParent : inst->hSelf,
                        L"Could not rename the file on disk.",
                        APP_NAME, MB_OK | MB_ICONWARNING);
                } else {
                    d->filePath = newP.wstring(); d->titleOverride.clear();
                    if (index == inst->activeDoc) { inst->currentFile = d->filePath; UpdateTitle(inst); }
                }
            }
        }
    }
    InvalidateRect(inst->hSelf, &inst->tabBarRect, FALSE);
    UpdateWindow(inst->hSelf);
    if (inst->hEdit) SetFocus(inst->hEdit);
}

// ============================================================
//  Layout
// ============================================================
static void LayoutSearchBar(NotepadInstance* inst) {
    if (!inst->searchOpen) return;
    RECT sr = inst->searchRect;
    const int pad = Dpi(inst, 10), btn = Dpi(inst, 28), rowH = Dpi(inst, 30);
    int x = sr.left + pad, y = sr.top + Dpi(inst, 8);
    int rightLimit = sr.right - pad;
    int buttonsW = btn * 4 + Dpi(inst, 3) * 3;
    int inputW = rightLimit - x - buttonsW - Dpi(inst, 12);
    if (inputW < Dpi(inst, 100)) inputW = Dpi(inst, 100);
    SetWindowPos(inst->hSearchFind, nullptr, x, y + Dpi(inst, 1), inputW, rowH, SWP_NOZORDER);
    x += inputW + Dpi(inst, 12);
    SetWindowPos(inst->hSearchCase, nullptr, x, y + Dpi(inst, 1), btn, btn, SWP_NOZORDER); x += btn + Dpi(inst, 3);
    SetWindowPos(inst->hSearchPrev, nullptr, x, y + Dpi(inst, 1), btn, btn, SWP_NOZORDER); x += btn + Dpi(inst, 3);
    SetWindowPos(inst->hSearchNext, nullptr, x, y + Dpi(inst, 1), btn, btn, SWP_NOZORDER); x += btn + Dpi(inst, 3);
    SetWindowPos(inst->hSearchClose, nullptr, x, y + Dpi(inst, 1), btn, btn, SWP_NOZORDER);
    if (inst->replaceOpen) {
        int y2 = sr.top + Dpi(inst, 8) + rowH + Dpi(inst, 8);
        int x2 = sr.left + pad;
        int buttons2W = btn * 2 + Dpi(inst, 3);
        int input2W = rightLimit - x2 - buttons2W - Dpi(inst, 12);
        if (input2W < Dpi(inst, 100)) input2W = Dpi(inst, 100);
        SetWindowPos(inst->hSearchRepl, nullptr, x2, y2, input2W, rowH, SWP_NOZORDER); x2 += input2W + Dpi(inst, 12);
        SetWindowPos(inst->hSearchReplOne, nullptr, x2, y2 + Dpi(inst, 1), btn, btn, SWP_NOZORDER); x2 += btn + Dpi(inst, 3);
        SetWindowPos(inst->hSearchReplAll, nullptr, x2, y2 + Dpi(inst, 1), btn, btn, SWP_NOZORDER);
    }
}

static int BtnSizeFor(NotepadInstance* inst, ToolbarSize s) {
    switch (s) {
    case TBSIZE_SMALL:  return Dpi(inst, 26);
    case TBSIZE_LARGE:  return Dpi(inst, 42);
    default:            return Dpi(inst, 32);
    }
}
static int GapFor(NotepadInstance* inst, ToolbarSize s) {
    switch (s) {
    case TBSIZE_SMALL:  return Dpi(inst, 3);
    case TBSIZE_LARGE:  return Dpi(inst, 6);
    default:            return Dpi(inst, 4);
    }
}
static SIZE ToolbarDesiredClientSize(NotepadInstance* inst) {
    int btn = BtnSizeFor(inst, inst->tbSize), gap = GapFor(inst, inst->tbSize), pad = Dpi(inst, 10), n = 10;
    SIZE sz{}; bool horiz = (inst->tbDock == TBDOCK_TOP || inst->tbDock == TBDOCK_BOTTOM);
    if (inst->tbDock == TBDOCK_FLOAT) horiz = inst->tbFloatHoriz;
    if (horiz) { sz.cx = pad*2 + n*btn + (n-1)*gap; sz.cy = pad*2 + btn; }
    else       { sz.cx = pad*2 + btn; sz.cy = pad*2 + n*btn + (n-1)*gap; }
    return sz;
}
static void LayoutToolbarChildren(NotepadInstance* inst) {
    if (!inst->hToolbarHost) return;
    int btn = BtnSizeFor(inst, inst->tbSize), gap = GapFor(inst, inst->tbSize), pad = Dpi(inst, 10);
    bool horiz = (inst->tbDock == TBDOCK_TOP || inst->tbDock == TBDOCK_BOTTOM);
    if (inst->tbDock == TBDOCK_FLOAT) horiz = inst->tbFloatHoriz;
    int cx = pad, cy = pad;
    for (int i = 0; i < 10; ++i) {
        if (!inst->hTbBtns[i]) continue;
        SetWindowPos(inst->hTbBtns[i], nullptr, cx, cy, btn, btn, SWP_NOZORDER);
        if (horiz) cx += btn + gap; else cy += btn + gap;
    }
}

void UpdateLayout(NotepadInstance* inst, HWND hWnd) {
    RECT rc; GetClientRect(hWnd, &rc);
    int topInset = 0, bottomInset = 0, leftInset = 0, rightInset = 0;
    SIZE tbsz{0, 0};
    if (inst->hToolbarHost && inst->tbVisible && inst->tbDock != TBDOCK_FLOAT) {
        tbsz = ToolbarDesiredClientSize(inst);
        switch (inst->tbDock) {
        case TBDOCK_TOP:    topInset = tbsz.cy; break;
        case TBDOCK_BOTTOM: bottomInset = tbsz.cy; break;
        case TBDOCK_LEFT:   leftInset = tbsz.cx; break;
        case TBDOCK_RIGHT:  rightInset = tbsz.cx; break;
        default: break;
        }
    }
    const int statusH = inst->showStatus ? Dpi(inst, 26) : 0;
    if (inst->hToolbarHost && inst->tbVisible && inst->tbDock != TBDOCK_FLOAT) {
        RECT tb{};
        switch (inst->tbDock) {
        case TBDOCK_TOP:    tb = {0, 0, rc.right, tbsz.cy}; break;
        case TBDOCK_BOTTOM: tb = {0, rc.bottom - statusH - tbsz.cy, rc.right, rc.bottom - statusH}; break;
        case TBDOCK_LEFT:   tb = {0, 0, tbsz.cx, rc.bottom}; break;
        case TBDOCK_RIGHT:  tb = {rc.right - tbsz.cx, 0, rc.right, rc.bottom}; break;
        }
        SetWindowPos(inst->hToolbarHost, nullptr, tb.left, tb.top,
            tb.right - tb.left, tb.bottom - tb.top, SWP_NOZORDER);
        LayoutToolbarChildren(inst);
    }
    int top = topInset;
    if (inst->searchOpen) {
        int searchH = inst->replaceOpen ? Dpi(inst, 84) : Dpi(inst, 46);
        inst->searchRect = { leftInset, top, rc.right - rightInset, top + searchH };
        top += searchH;
    } else inst->searchRect = {};
    const int tabH = Dpi(inst, 36);
    inst->tabBarRect = { leftInset, top, rc.right - rightInset, top + tabH };
    top += tabH;
    inst->statusRect = { leftInset, rc.bottom - statusH, rc.right - rightInset, rc.bottom };
    inst->editRect = { leftInset, top, rc.right - rightInset, rc.bottom - statusH - bottomInset };
    for (Document* d : inst->docs) {
        if (!d->hEdit) continue;
        SetWindowPos(d->hEdit, nullptr, inst->editRect.left, inst->editRect.top,
            inst->editRect.right - inst->editRect.left,
            inst->editRect.bottom - inst->editRect.top, SWP_NOZORDER);
    }
    if (inst->searchOpen) LayoutSearchBar(inst);
}

// ============================================================
//  Toolbar context menu
// ============================================================
void SetToolbarSize(NotepadInstance* inst, ToolbarSize size) {
    if (size == inst->tbSize) return;
    inst->tbSize = size;
    if (!inst->hostManaged) RegWriteStr(L"ToolbarSize", std::to_wstring((int)inst->tbSize));
    if (inst->tbDock == TBDOCK_FLOAT && inst->hToolbarHost) {
        SIZE sz = ToolbarDesiredClientSize(inst);
        RECT wr = {0, 0, sz.cx, sz.cy};
        DWORD st = (DWORD)GetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE);
        DWORD ex = (DWORD)GetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE);
        AdjustWindowRectEx(&wr, st, FALSE, ex);
        SetWindowPos(inst->hToolbarHost, nullptr, 0, 0, wr.right-wr.left, wr.bottom-wr.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    if (inst->hSelf) UpdateLayout(inst, inst->hSelf);
    if (inst->hToolbarHost) InvalidateRect(inst->hToolbarHost, nullptr, TRUE);
}

void SetToolbarDock(NotepadInstance* inst, ToolbarDock dock) {
    // Disable floating in child mode
    if (inst->isChild && dock == TBDOCK_FLOAT) return;
    if (dock == inst->tbDock) return;
    if (dock == TBDOCK_FLOAT) inst->tbFloatHoriz = (inst->tbDock == TBDOCK_TOP || inst->tbDock == TBDOCK_BOTTOM);
    inst->tbDock = dock;
    if (!inst->hToolbarHost) return;
    LONG_PTR style = GetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE);
    bool wasChild = (style & WS_CHILD) != 0, willBeChild = (dock != TBDOCK_FLOAT);
    if (wasChild && !willBeChild) {
        SetParent(inst->hToolbarHost, nullptr);
        SetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE, (style & ~(LONG_PTR)WS_CHILD) | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN);
        SetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE, GetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
        SetWindowLongPtrW(inst->hToolbarHost, GWLP_HWNDPARENT, (LONG_PTR)inst->hSelf);
        SetWindowTextW(inst->hToolbarHost, L"Toolbar");
        SIZE sz = ToolbarDesiredClientSize(inst);
        RECT wr = {0, 0, sz.cx, sz.cy};
        AdjustWindowRectEx(&wr, (DWORD)GetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE), FALSE, (DWORD)GetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE));
        RECT mr; GetWindowRect(inst->hSelf, &mr);
        SetWindowPos(inst->hToolbarHost, HWND_TOP, mr.left + Dpi(inst, 80), mr.top + Dpi(inst, 80), wr.right-wr.left, wr.bottom-wr.top, SWP_FRAMECHANGED);
        LayoutToolbarChildren(inst);
    } else if (!wasChild && willBeChild) {
        SetParent(inst->hToolbarHost, inst->hSelf);
        SetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE, (GetWindowLongPtrW(inst->hToolbarHost, GWL_STYLE) & ~(LONG_PTR)(WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME)) | WS_CHILD | WS_CLIPCHILDREN);
        SetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE, GetWindowLongPtrW(inst->hToolbarHost, GWL_EXSTYLE) & ~(LONG_PTR)WS_EX_TOOLWINDOW);
        SetWindowTextW(inst->hToolbarHost, L"");
        SetWindowPos(inst->hToolbarHost, nullptr, 0, 0, 100, 100, SWP_FRAMECHANGED | SWP_NOZORDER);
    }
    if (!inst->hostManaged) RegWriteStr(L"ToolbarDock", std::to_wstring((int)inst->tbDock));
    if (inst->hSelf) UpdateLayout(inst, inst->hSelf);
    if (inst->hToolbarHost) InvalidateRect(inst->hToolbarHost, nullptr, TRUE);
    if (inst->hSelf) InvalidateRect(inst->hSelf, nullptr, TRUE);
}

void ShowToolbarMenu(NotepadInstance* inst, HWND owner, int x, int y) {
    std::vector<MenuItem> items;
    auto add = [&](int id, const wchar_t* label, bool checked = false) {
        MenuItem it; it.id = id; it.label = label; it.checked = checked; items.push_back(it); };
    auto sep = [&]() { MenuItem it; it.separator = true; items.push_back(it); };
    add(IDM_TB_DOCK_TOP, L"Dock to Top", inst->tbDock == TBDOCK_TOP);
    add(IDM_TB_DOCK_BOTTOM, L"Dock to Bottom", inst->tbDock == TBDOCK_BOTTOM);
    add(IDM_TB_DOCK_LEFT, L"Dock to Left", inst->tbDock == TBDOCK_LEFT);
    add(IDM_TB_DOCK_RIGHT, L"Dock to Right", inst->tbDock == TBDOCK_RIGHT);
    if (!inst->isChild) add(IDM_TB_DOCK_FLOAT, L"Floating", inst->tbDock == TBDOCK_FLOAT);
    sep();
    add(IDM_TB_SIZE_SMALL, L"Small icons", inst->tbSize == TBSIZE_SMALL);
    add(IDM_TB_SIZE_MEDIUM, L"Medium icons", inst->tbSize == TBSIZE_MEDIUM);
    add(IDM_TB_SIZE_LARGE, L"Large icons", inst->tbSize == TBSIZE_LARGE);
    // (ShowThemedMenu is defined further down -- we forward to it)
    extern int ShowThemedMenuInst(NotepadInstance*, HWND, int, int, const std::vector<MenuItem>&);
    int chosen = ShowThemedMenuInst(inst, owner, x, y, items);
    switch (chosen) {
    case IDM_TB_DOCK_TOP:    SetToolbarDock(inst, TBDOCK_TOP); break;
    case IDM_TB_DOCK_BOTTOM: SetToolbarDock(inst, TBDOCK_BOTTOM); break;
    case IDM_TB_DOCK_LEFT:   SetToolbarDock(inst, TBDOCK_LEFT); break;
    case IDM_TB_DOCK_RIGHT:  SetToolbarDock(inst, TBDOCK_RIGHT); break;
    case IDM_TB_DOCK_FLOAT:  SetToolbarDock(inst, TBDOCK_FLOAT); break;
    case IDM_TB_SIZE_SMALL:  SetToolbarSize(inst, TBSIZE_SMALL); break;
    case IDM_TB_SIZE_MEDIUM: SetToolbarSize(inst, TBSIZE_MEDIUM); break;
    case IDM_TB_SIZE_LARGE:  SetToolbarSize(inst, TBSIZE_LARGE); break;
    }
}

// ============================================================
//  Themed popup menu (instance-aware)
// ============================================================
static int MenuItemAtY(NotepadInstance* inst, int y) {
    if (!inst->menuItems) return -1;
    int top = inst->menuPadY;
    for (size_t i = 0; i < inst->menuItems->size(); ++i) {
        const MenuItem& it = (*inst->menuItems)[i];
        int h = it.separator ? Dpi(inst, 9) : inst->menuItemH;
        if (y >= top && y < top + h) return (int)i;
        top += h;
    }
    return -1;
}

LRESULT CALLBACK MenuProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    NotepadInstance* inst = reinterpret_cast<NotepadInstance*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        if (!inst) { EndPaint(h, &ps); return 0; }
        FillRoundRect(hdc, rc, Dpi(inst, 8), inst->theme.BgCard);
        StrokeRoundRect(hdc, rc, Dpi(inst, 8), inst->theme.Border);
        if (!inst->menuItems) { EndPaint(h, &ps); return 0; }
        int top = inst->menuPadY;
        for (size_t i = 0; i < inst->menuItems->size(); ++i) {
            const MenuItem& it = (*inst->menuItems)[i];
            if (it.separator) {
                int y = top + Dpi(inst, 4);
                RECT sep = { rc.left + Dpi(inst, 10), y, rc.right - Dpi(inst, 10), y + 1 };
                FillSolid(hdc, sep, inst->theme.BorderSoft);
                top += Dpi(inst, 9); continue;
            }
            RECT row = { rc.left + Dpi(inst, 4), top, rc.right - Dpi(inst, 4), top + inst->menuItemH };
            bool hot = ((int)i == inst->menuHot) && !it.disabled;
            if (hot) FillRoundRect(hdc, row, Dpi(inst, 6), inst->theme.BgRailHi);
            COLORREF fg = it.disabled ? inst->theme.TextLow : inst->theme.TextHi;
            if (it.checked) {
                RECT ir = { row.left + Dpi(inst, 8), row.top + Dpi(inst, 6), row.left + Dpi(inst, 22), row.bottom - Dpi(inst, 6) };
                DrawIcon(hdc, inst, Icon::Check, ir, inst->theme.Accent, 2);
            }
            HFONT of = (HFONT)SelectObject(hdc, inst->font);
            SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, fg);
            RECT tr = { row.left + Dpi(inst, 28), row.top, row.right - Dpi(inst, 80), row.bottom };
            DrawTextW(hdc, it.label.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (!it.shortcut.empty()) {
                SetTextColor(hdc, inst->theme.TextLow);
                HFONT os = (HFONT)SelectObject(hdc, inst->fontSmall);
                RECT sr = { row.right - Dpi(inst, 80), row.top, row.right - Dpi(inst, 12), row.bottom };
                DrawTextW(hdc, it.shortcut.c_str(), -1, &sr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, os);
            }
            SelectObject(hdc, of);
            top += inst->menuItemH;
        }
        EndPaint(h, &ps); return 0;
    }
    case WM_MOUSEMOVE: {
        int idx = MenuItemAtY(inst, GET_Y_LPARAM(l));
        if (idx >= 0 && inst->menuItems && (*inst->menuItems)[idx].separator) idx = -1;
        if (idx != inst->menuHot) { inst->menuHot = idx; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int idx = MenuItemAtY(inst, GET_Y_LPARAM(l));
        if (idx >= 0 && inst->menuItems) {
            const MenuItem& it = (*inst->menuItems)[idx];
            if (!it.separator && !it.disabled) {
                inst->menuResult = it.id; inst->menuDone = true;
                PostMessageW(h, WM_NULL, 0, 0);
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: return 0;
    case WM_KILLFOCUS: inst->menuDone = true; PostMessageW(h, WM_NULL, 0, 0); return 0;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) { inst->menuResult = 0; inst->menuDone = true; PostMessageW(h, WM_NULL, 0, 0); return 0; }
        return 0;
    case WM_CLOSE: inst->menuResult = 0; inst->menuDone = true; PostMessageW(h, WM_NULL, 0, 0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void EnsureMenuClass() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSW wc{}; wc.lpfnWndProc = MenuProc;
    wc.hInstance = g_hInst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = MENU_CLASS; RegisterClassW(&wc);
    registered = true;
}

int ShowThemedMenuInst(NotepadInstance* inst, HWND owner, int x, int y,
                       const std::vector<MenuItem>& items) {
    EnsureMenuClass();
    inst->menuItems = new std::vector<MenuItem>(items);
    inst->menuHot = -1; inst->menuResult = 0; inst->menuDone = false;
    inst->menuItemH = Dpi(inst, 32); inst->menuPadY = Dpi(inst, 6);
    HDC hdc = GetDC(nullptr);
    HFONT of = (HFONT)SelectObject(hdc, inst->font);
    int maxW = Dpi(inst, 160);
    for (const auto& it : items) {
        if (it.separator) continue;
        SIZE sz{}; GetTextExtentPoint32W(hdc, it.label.c_str(), (int)it.label.size(), &sz);
        int need = sz.cx + Dpi(inst, 28 + 80 + 32);
        if (need > maxW) maxW = need;
    }
    SelectObject(hdc, of); ReleaseDC(nullptr, hdc);
    int totalH = inst->menuPadY * 2;
    for (const auto& it : items) totalH += it.separator ? Dpi(inst, 9) : inst->menuItemH;
    RECT wa{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (x + maxW > wa.right)   x = wa.right - maxW;
    if (y + totalH > wa.bottom) y = wa.bottom - totalH;
    if (x < wa.left) x = wa.left; if (y < wa.top) y = wa.top;
    inst->menuWnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        MENU_CLASS, L"", WS_POPUP, x, y, maxW, totalH, owner, nullptr, g_hInst, nullptr);
    if (!inst->menuWnd) { delete inst->menuItems; inst->menuItems = nullptr; return 0; }
    SetWindowLongPtrW(inst->menuWnd, GWLP_USERDATA, (LONG_PTR)inst);
    ShowWindow(inst->menuWnd, SW_SHOW);
    SetForegroundWindow(inst->menuWnd);
    SetFocus(inst->menuWnd);
    MSG msg;
    while (!inst->menuDone && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    DestroyWindow(inst->menuWnd);
    inst->menuWnd = nullptr;
    delete inst->menuItems; inst->menuItems = nullptr;
    return inst->menuResult;
}

// ============================================================
//  Owner-draw buttons (instance-aware)
// ============================================================
static bool ButtonStyleFor(NotepadInstance* inst, int id, BtnStyle& st) {
    switch (id) {
    case IDC_TB_MENU:     st = {Icon::Menu, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_TB_NEW:      st = {Icon::New, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_TB_OPEN:     st = {Icon::Open, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_TB_SAVE:     st = {Icon::Save, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_TB_UNDO:     st = {Icon::Undo, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_TB_REDO:     st = {Icon::Redo, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_TB_FIND:     st = {Icon::Find, false, true, inst->searchOpen, inst->theme.IconIdle}; return true;
    case IDC_TB_ZOOM_IN:  st = {Icon::ZoomIn, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_TB_ZOOM_OUT: st = {Icon::ZoomOut, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_TB_WRAP:     st = {Icon::Wrap, false, true, inst->wordWrap, inst->theme.IconIdle}; return true;
    case IDC_SEARCH_NEXT:     st = {Icon::Next, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_SEARCH_PREV:     st = {Icon::Prev, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_SEARCH_CLOSE:    st = {Icon::Close, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_SEARCH_CASE:     st = {Icon::Case, false, true, inst->searchCase, inst->theme.IconIdle}; return true;
    case IDC_SEARCH_REPL_ONE: st = {Icon::Replace, false, true, false, inst->theme.IconIdle}; return true;
    case IDC_SEARCH_REPL_ALL: st = {Icon::ReplaceAll, false, true, false, inst->theme.IconIdle}; return true;
    }
    return false;
}

static void DrawButton(NotepadInstance* inst, const DRAWITEMSTRUCT* d) {
    BtnStyle st{};
    if (!ButtonStyleFor(inst, (int)d->CtlID, st)) { FillSolid(d->hDC, d->rcItem, inst->theme.BgApp); return; }
    RECT rc = d->rcItem;
    bool down = (d->itemState & ODS_SELECTED) != 0;
    bool hot = !down && (d->hwndItem == inst->hotBtn);
    bool disabled = (d->itemState & ODS_DISABLED) != 0;
    // Read-only disables editing buttons
    if (inst->readOnly) {
        int id = (int)d->CtlID;
        if (id == IDC_TB_UNDO || id == IDC_TB_REDO) disabled = true;
    }
    COLORREF parentBg = inst->theme.BgApp;
    bool onPanel = (d->CtlID >= IDC_SEARCH_NEXT && d->CtlID <= IDC_SEARCH_REPL_ALL) ||
                   (d->CtlID >= IDC_TB_MENU && d->CtlID <= IDC_TB_WRAP);
    if (onPanel) parentBg = inst->theme.BgPanel;
    FillSolid(d->hDC, rc, parentBg);
    COLORREF fg = st.fg;
    if (disabled)       fg = inst->theme.TextLow;
    else if (st.active) fg = inst->theme.IconActive;
    else if (hot)       fg = inst->theme.IconActive;
    bool drawBg = hot || st.active || down;
    if (st.chipBg)     FillRoundRect(d->hDC, rc, Dpi(inst, 10), down ? inst->theme.BgBtnHi : inst->theme.BgBtn);
    else if (st.rail)  { if (drawBg) FillRoundRect(d->hDC, rc, Dpi(inst, 10), down ? inst->theme.BgBtnHi : inst->theme.BgRailHi); }
    else               { if (drawBg) FillRoundRect(d->hDC, rc, Dpi(inst, 8),  down ? inst->theme.BgBtnHi : inst->theme.BgRailHi); }
    int minDim = (std::min)(rc.right - rc.left, rc.bottom - rc.top);
    int inset = minDim / 4;
    RECT ir = { rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset };
    DrawIcon(d->hDC, inst, st.icon, ir, fg, 2);
}

static void UpdateHotButton(NotepadInstance* inst, HWND parent, LPARAM lParam) {
    POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    HWND child = ChildWindowFromPointEx(parent, p, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT);
    if (child == parent) child = nullptr;
    if (child != inst->hotBtn) {
        HWND old = inst->hotBtn; inst->hotBtn = child;
        if (old)   InvalidateRect(old, nullptr, TRUE);
        if (child) InvalidateRect(child, nullptr, TRUE);
    }
}

static HWND MakeButton(NotepadInstance* inst, HWND parent, int id, const wchar_t* text = L"") {
    HWND h = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, inst->hInst, nullptr);
    SendMessageW(h, WM_SETFONT, (WPARAM)inst->font, TRUE);
    return h;
}

// ============================================================
//  Toolbar button subclass
// ============================================================
LRESULT CALLBACK ToolbarBtnProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    NotepadInstance* inst = GetInstance(GetParent(h));
    if (!inst) { /* toolbar host may be the parent, try grandparent */ }
    switch (m) {
    case WM_MOUSEMOVE: {
        if (inst && inst->hotBtn != h) {
            HWND old = inst->hotBtn; inst->hotBtn = h;
            if (old) InvalidateRect(old, nullptr, TRUE);
            InvalidateRect(h, nullptr, TRUE);
        }
        TRACKMOUSEEVENT tme{}; tme.cbSize = sizeof(tme); tme.dwFlags = TME_LEAVE; tme.hwndTrack = h;
        TrackMouseEvent(&tme); return 0;
    }
    case WM_MOUSELEAVE:
        if (inst && inst->hotBtn == h) { inst->hotBtn = nullptr; InvalidateRect(h, nullptr, TRUE); }
        return 0;
    case WM_RBUTTONUP: {
        POINT pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) }; ClientToScreen(h, &pt);
        if (inst) ShowToolbarMenu(inst, inst->hSelf, pt.x, pt.y);
        return 0;
    }
    case WM_CONTEXTMENU: {
        int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        if (x == -1 && y == -1) { RECT br; GetWindowRect(h, &br); x = br.left + 10; y = br.top + 10; }
        if (inst) ShowToolbarMenu(inst, inst->hSelf, x, y);
        return 0;
    }
    }
    return DefSubclassProc(h, m, w, l);
}

LRESULT CALLBACK ToolbarHostProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    NotepadInstance* inst = GetInstance(GetParent(h));
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        if (inst) {
            FillSolid(hdc, rc, inst->theme.BgPanel);
            RECT sep = rc;
            switch (inst->tbDock) {
            case TBDOCK_TOP:    sep.top = rc.bottom - 1; break;
            case TBDOCK_BOTTOM: sep.bottom = rc.top + 1; break;
            case TBDOCK_LEFT:   sep.left = rc.right - 1; break;
            case TBDOCK_RIGHT:  sep.right = rc.left + 1; break;
            default:            sep.top = rc.bottom - 1; break;
            }
            FillSolid(hdc, sep, inst->theme.BorderSoft);
        }
        EndPaint(h, &ps); return 0;
    }
    case WM_MOUSEMOVE: {
        if (inst) UpdateHotButton(inst, h, l);
        return 0;
    }
    case WM_CONTEXTMENU: {
        int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        if (x == -1 && y == -1) { RECT br; GetWindowRect(h, &br); x = br.left + 20; y = br.top + 20; }
        if (inst) ShowToolbarMenu(inst, inst->hSelf, x, y);
        return 0;
    }
    case WM_CLOSE:
        if (inst && inst->tbDock == TBDOCK_FLOAT) { SetToolbarDock(inst, TBDOCK_TOP); return 0; }
        break;
    case WM_COMMAND: case WM_DRAWITEM: case WM_NOTIFY:
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORBTN:
        if (inst && inst->hSelf) return SendMessageW(inst->hSelf, m, w, l);
        return 0;
    case WM_SETFOCUS:
        if (inst && inst->hSelf) SetFocus(inst->hSelf);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ============================================================
//  Search visibility
// ============================================================
void ShowSearch(NotepadInstance* inst, bool open, bool withReplace) {
    inst->searchOpen = open; inst->replaceOpen = open && withReplace;
    int cmdShow = open ? SW_SHOW : SW_HIDE;
    HWND firstRow[5] = { inst->hSearchFind, inst->hSearchNext, inst->hSearchPrev, inst->hSearchClose, inst->hSearchCase };
    for (HWND h : firstRow) if (h) ShowWindow(h, cmdShow);
    HWND secondRow[3] = { inst->hSearchRepl, inst->hSearchReplOne, inst->hSearchReplAll };
    for (HWND h : secondRow) if (h) ShowWindow(h, (open && withReplace) ? SW_SHOW : SW_HIDE);
    if (inst->hTbBtns[6]) InvalidateRect(inst->hTbBtns[6], nullptr, TRUE);
    UpdateLayout(inst, inst->hSelf);
    InvalidateRect(inst->hSelf, nullptr, TRUE);
    if (open) {
        SetFocus(inst->hSearchFind);
        DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0);
        int s = LOWORD(sel), e = HIWORD(sel);
        if (e > s && (e - s) < 128) {
            std::wstring cur(e - s, 0);
            TEXTRANGEW tr{}; tr.chrg.cpMin = s; tr.chrg.cpMax = e; tr.lpstrText = cur.data();
            SendMessageW(inst->hEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
            if (!cur.empty() && cur.find(L"\r\n") == std::wstring::npos && cur.find(L'\r') == std::wstring::npos) {
                SetWindowTextW(inst->hSearchFind, cur.c_str());
                SendMessageW(inst->hSearchFind, EM_SETSEL, 0, -1);
            }
        }
    } else SetFocus(inst->hEdit);
}

// ============================================================
//  Tab context menu
// ============================================================
void ShowTabMenu(NotepadInstance* inst, int index, int x, int y) {
    std::vector<MenuItem> items;
    auto add = [&](int id, const wchar_t* label, const wchar_t* sc = L"", bool disabled = false) {
        MenuItem it; it.id = id; it.label = label; it.shortcut = sc; it.disabled = disabled; items.push_back(it); };
    auto sep = [&]() { MenuItem it; it.separator = true; items.push_back(it); };
    bool hasTab = (index >= 0 && index < (int)inst->docs.size());
    bool hasFile = hasTab && !inst->docs[index]->filePath.empty();
    add(IDM_TAB_RENAME, L"Rename...", L"F2", !hasTab); sep();
    add(IDM_TAB_CLOSE, L"Close", L"Ctrl+W", !hasTab);
    add(IDM_TAB_CLOSE_OTHERS, L"Close others", L"", !hasTab || inst->docs.size() <= 1);
    add(IDM_TAB_CLOSE_RIGHT, L"Close to the right", L"", !hasTab || index >= (int)inst->docs.size() - 1);
    add(IDM_TAB_CLOSE_ALL, L"Close all", L""); sep();
    add(IDM_TAB_SAVE_AS, L"Save as...", L"Ctrl+Shift+S", !hasTab);
    add(IDM_TAB_REVEAL, L"Reveal in Explorer", L"", !hasFile);
    int chosen = ShowThemedMenuInst(inst, inst->hSelf, x, y, items);
    switch (chosen) {
    case IDM_TAB_RENAME: BeginRenameTab(inst, index); break;
    case IDM_TAB_CLOSE: CloseDoc(inst, index); break;
    case IDM_TAB_CLOSE_OTHERS: {
        if (!hasTab) break;
        for (int i = (int)inst->docs.size() - 1; i >= 0; --i) {
            if (i == index) continue;
            if (!CloseDoc(inst, i)) break;
            if (i < index) index--;
        }
        SetActiveDoc(inst, index);
        break;
    }
    case IDM_TAB_CLOSE_RIGHT: {
        if (!hasTab) break;
        for (int i = (int)inst->docs.size() - 1; i > index; --i)
            if (!CloseDoc(inst, i)) break;
        break;
    }
    case IDM_TAB_CLOSE_ALL: CloseAllDocs(inst); break;
    case IDM_TAB_SAVE_AS: SetActiveDoc(inst, index); DoSave(inst, true); break;
    case IDM_TAB_REVEAL: {
        if (!hasFile) break;
        Document* d = inst->docs[index];
        std::wstring arg = L"/select,\"" + d->filePath + L"\"";
        ShellExecuteW(inst->isChild ? inst->hParent : inst->hSelf,
            L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
        break;
    }
    }
}

// ============================================================
//  Command dispatch
// ============================================================
void DispatchCommand(NotepadInstance* inst, HWND hWnd, int cmd) {
    switch (cmd) {
    case IDM_NEW:    DoNewTab(inst); return;
    case IDM_OPEN:   DoOpen(inst); return;
    case IDM_SAVE:   DoSave(inst, false); return;
    case IDM_SAVEAS: DoSave(inst, true); return;
    case IDM_EXIT:   SendMessageW(hWnd, WM_CLOSE, 0, 0); return;
    case IDM_UNDO:   if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, EM_UNDO, 0, 0); return;
    case IDM_REDO:   if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, EM_REDO, 0, 0); return;
    case IDM_CUT:    if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, WM_CUT, 0, 0); return;
    case IDM_COPY:   if (inst->hEdit) SendMessageW(inst->hEdit, WM_COPY, 0, 0); return;
    case IDM_PASTE:  if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, WM_PASTE, 0, 0); return;
    case IDM_DELETE: if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, EM_REPLACESEL, TRUE, (LPARAM)L""); return;
    case IDM_SELECTALL: if (inst->hEdit) SendMessageW(inst->hEdit, EM_SETSEL, 0, -1); return;
    case IDM_DATETIME: {
        if (inst->hEdit && !inst->readOnly) {
            SYSTEMTIME st; GetLocalTime(&st);
            wchar_t t[64];
            wsprintfW(t, L"%d:%02d %s %d/%d/%d",
                (st.wHour % 12) ? (st.wHour % 12) : 12, st.wMinute,
                st.wHour < 12 ? L"AM" : L"PM", st.wMonth, st.wDay, st.wYear);
            SendMessageW(inst->hEdit, EM_REPLACESEL, TRUE, (LPARAM)t);
        }
        return;
    }
    case IDM_FIND:     ShowSearch(inst, true, false); return;
    case IDM_REPLACE:  ShowSearch(inst, true, true); return;
    case IDM_FINDNEXT: FindNext(inst, false); return;
    case IDM_WRAP:
        inst->wordWrap = !inst->wordWrap; SyncToActive(inst);
        if (!inst->hostManaged) RegWriteStr(L"WordWrap", inst->wordWrap ? L"1" : L"0");
        ApplyWordWrap(inst);
        if (inst->hTbBtns[9]) InvalidateRect(inst->hTbBtns[9], nullptr, TRUE);
        return;
    case IDM_ZOOM_IN:
        if (inst->zoom < 500) { inst->zoom += 10; SyncToActive(inst); ApplyZoom(inst);
            if (!inst->hostManaged) RegWriteStr(L"Zoom", std::to_wstring(inst->zoom));
            if (inst->hSelf) InvalidateRect(inst->hSelf, &inst->statusRect, FALSE);
            FireNotify(inst, NP_NOTIFY_ZOOM_CHANGED, inst->activeDoc); }
        return;
    case IDM_ZOOM_OUT:
        if (inst->zoom > 30) { inst->zoom -= 10; SyncToActive(inst); ApplyZoom(inst);
            if (!inst->hostManaged) RegWriteStr(L"Zoom", std::to_wstring(inst->zoom));
            if (inst->hSelf) InvalidateRect(inst->hSelf, &inst->statusRect, FALSE);
            FireNotify(inst, NP_NOTIFY_ZOOM_CHANGED, inst->activeDoc); }
        return;
    case IDM_ZOOM_RESET:
        inst->zoom = 100; SyncToActive(inst); ApplyZoom(inst);
        if (!inst->hostManaged) RegWriteStr(L"Zoom", L"100");
        if (inst->hSelf) InvalidateRect(inst->hSelf, &inst->statusRect, FALSE);
        FireNotify(inst, NP_NOTIFY_ZOOM_CHANGED, inst->activeDoc);
        return;
    case IDM_STATUS:
        inst->showStatus = !inst->showStatus;
        if (!inst->hostManaged) RegWriteStr(L"ShowStatus", inst->showStatus ? L"1" : L"0");
        UpdateLayout(inst, hWnd); InvalidateRect(hWnd, nullptr, TRUE);
        return;
    case IDM_TOOLBAR:
        if (inst->hToolbarHost) {
            inst->tbVisible = !inst->tbVisible;
            ShowWindow(inst->hToolbarHost, inst->tbVisible ? SW_SHOW : SW_HIDE);
            UpdateLayout(inst, hWnd); InvalidateRect(hWnd, nullptr, TRUE);
        }
        return;
    case IDM_FONT: {
        CHOOSEFONTW cf{}; cf.lStructSize = sizeof(cf);
        cf.hwndOwner = inst->isChild ? inst->hParent : hWnd;
        cf.lpLogFont = &inst->logFont;
        cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_NOVERTFONTS;
        if (ChooseFontW(&cf)) {
            ApplyEditorFont(inst);
            if (!inst->hostManaged) {
                RegWriteStr(L"FontFace", inst->logFont.lfFaceName);
                RegWriteStr(L"FontSize", std::to_wstring(inst->logFont.lfHeight));
            }
        }
        return;
    }
    case IDM_ABOUT:
        MessageBoxW(inst->isChild ? inst->hParent : hWnd,
            L"Neage Notepad\r\nA small, dark, Win32 text editor.\r\n\r\n"
            L"Ctrl+N/O/S, Ctrl+Z/Y, Ctrl+F/H, F3, Ctrl+W (close tab)\r\n"
            L"Ctrl+T (new tab), Ctrl+Tab (next tab), Ctrl+Plus/Minus for zoom.\r\n\r\n"
            L"F2 or right-click a tab to rename it.",
            APP_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }
}

void ShowMainMenu(NotepadInstance* inst, HWND hWnd) {
    RECT br{};
    if (inst->hTbBtns[0]) GetWindowRect(inst->hTbBtns[0], &br);
    else GetWindowRect(hWnd, &br);
    bool tbVisible = inst->tbVisible;
    std::vector<MenuItem> items;
    auto add = [&](int id, const wchar_t* label, const wchar_t* sc, bool checked = false, bool disabled = false) {
        MenuItem it; it.id = id; it.label = label; it.shortcut = sc; it.checked = checked; it.disabled = disabled; items.push_back(it); };
    auto sep = [&]() { MenuItem it; it.separator = true; items.push_back(it); };
    add(IDM_NEW, L"New Tab", L"Ctrl+T");
    add(IDM_OPEN, L"Open...", L"Ctrl+O");
    add(IDM_SAVE, L"Save", L"Ctrl+S");
    add(IDM_SAVEAS, L"Save As...", L"Ctrl+Shift+S"); sep();
    add(IDM_UNDO, L"Undo", L"Ctrl+Z", false, inst->readOnly);
    add(IDM_REDO, L"Redo", L"Ctrl+Y", false, inst->readOnly); sep();
    add(IDM_CUT, L"Cut", L"Ctrl+X", false, inst->readOnly);
    add(IDM_COPY, L"Copy", L"Ctrl+C");
    add(IDM_PASTE, L"Paste", L"Ctrl+V", false, inst->readOnly);
    add(IDM_DELETE, L"Delete", L"Del", false, inst->readOnly);
    add(IDM_SELECTALL, L"Select All", L"Ctrl+A");
    add(IDM_DATETIME, L"Time / Date", L"F5", false, inst->readOnly); sep();
    add(IDM_FIND, L"Find...", L"Ctrl+F");
    add(IDM_REPLACE, L"Replace...", L"Ctrl+H");
    add(IDM_FINDNEXT, L"Find Next", L"F3"); sep();
    add(IDM_WRAP, L"Word Wrap", L"", inst->wordWrap);
    add(IDM_FONT, L"Font...", L"");
    add(IDM_ZOOM_IN, L"Zoom In", L"Ctrl++");
    add(IDM_ZOOM_OUT, L"Zoom Out", L"Ctrl+-");
    add(IDM_ZOOM_RESET, L"Reset Zoom", L"Ctrl+0"); sep();
    add(IDM_STATUS, L"Status Bar", L"", inst->showStatus);
    add(IDM_TOOLBAR, L"Toolbar", L"", tbVisible); sep();
    add(IDM_ABOUT, L"About", L"");
    if (!inst->isChild) add(IDM_EXIT, L"Exit", L"Alt+F4");
    int chosen = ShowThemedMenuInst(inst, hWnd, br.left, br.bottom + Dpi(inst, 6), items);
    if (chosen) DispatchCommand(inst, hWnd, chosen);
}

// ============================================================
//  Editor subclass
// ============================================================
LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR refData) {
    NotepadInstance* inst = reinterpret_cast<NotepadInstance*>(refData);
    if (!inst) inst = GetInstance(GetParent(h));
    switch (m) {
    case WM_KEYDOWN:
        if (w == VK_ESCAPE && inst && inst->searchOpen) { ShowSearch(inst, false, false); return 0; }
        break;
    case WM_CHAR:
        if (w == VK_TAB) { SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)L"    "); return 0; }
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        if (inst && inst->hSelf) InvalidateRect(inst->hSelf, &inst->statusRect, FALSE);
        if (inst) FireNotify(inst, NP_NOTIFY_FOCUS_CHANGED, inst->activeDoc);
        break;
    }
    LRESULT r = DefSubclassProc(h, m, w, l);
    if (inst && inst->hSelf && (m == WM_KEYUP || m == WM_LBUTTONUP || m == WM_CHAR))
        InvalidateRect(inst->hSelf, &inst->statusRect, FALSE);
    return r;
}

// ============================================================
//  Paint helpers
// ============================================================
static void PaintSearchBar(NotepadInstance* inst, HDC hdc, const RECT& rc) {
    FillSolid(hdc, rc, inst->theme.BgPanel);
    RECT line = { rc.left, rc.bottom - 1, rc.right, rc.bottom };
    FillSolid(hdc, line, inst->theme.BorderSoft);
    RECT card = rc;
    card.left += Dpi(inst, 6); card.right -= Dpi(inst, 6);
    card.top += Dpi(inst, 6); card.bottom -= Dpi(inst, 6);
    StrokeRoundRect(hdc, card, Dpi(inst, 8), inst->theme.BorderSoft);
}

static void PaintStatusBar(NotepadInstance* inst, HDC hdc, const RECT& rc) {
    FillSolid(hdc, rc, inst->theme.BgPanel);
    RECT sep = { rc.left, rc.top, rc.right, rc.top + 1 };
    FillSolid(hdc, sep, inst->theme.BorderSoft);
    if (!inst->hEdit) return;
    wchar_t buf[256];
    HFONT of = (HFONT)SelectObject(hdc, inst->fontSmall);
    SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, inst->theme.TextMid);
    wsprintfW(buf, L"Ln %d, Col %d", GetCaretLine(inst), GetCaretCol(inst));
    RECT r1 = { rc.left + Dpi(inst, 14), rc.top, rc.left + Dpi(inst, 180), rc.bottom };
    DrawTextW(hdc, buf, -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    wsprintfW(buf, L"%d lines   %d chars", GetLineCount(inst), GetCharCount(inst));
    RECT r2 = { rc.right - Dpi(inst, 280), rc.top, rc.right - Dpi(inst, 140), rc.bottom };
    DrawTextW(hdc, buf, -1, &r2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    wsprintfW(buf, L"%d tabs", (int)inst->docs.size());
    RECT r3 = { rc.right - Dpi(inst, 140), rc.top, rc.right - Dpi(inst, 60), rc.bottom };
    DrawTextW(hdc, buf, -1, &r3, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    wsprintfW(buf, L"%d%%", inst->zoom);
    RECT r4 = { rc.right - Dpi(inst, 60), rc.top, rc.right - Dpi(inst, 14), rc.bottom };
    DrawTextW(hdc, buf, -1, &r4, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, of);
}

// ============================================================
//  WndProc (instance-aware)
// ============================================================
static bool MouseOverTabBar(NotepadInstance* inst, LPARAM lParam) {
    int y = GET_Y_LPARAM(lParam);
    return (y >= inst->tabBarRect.top && y < inst->tabBarRect.bottom);
}

LRESULT CALLBACK PanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NotepadInstance* inst = GetInstance(hWnd);
    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_SIZE:
        if (inst) { UpdateLayout(inst, hWnd); InvalidateRect(hWnd, nullptr, TRUE); }
        return 0;

    case WM_GETMINMAXINFO:
        if (inst && inst->isChild) return 0;
        if (inst) {
            MINMAXINFO* p = (MINMAXINFO*)lParam;
            p->ptMinTrackSize.x = Dpi(inst, 560);
            p->ptMinTrackSize.y = Dpi(inst, 360);
        }
        return 0;

    case WM_ERASEBKGND: return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        if (!inst) { EndPaint(hWnd, &ps); return 0; }
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ old = SelectObject(mem, bmp);
        FillSolid(mem, rc, inst->theme.BgApp);
        if (inst->searchOpen) PaintSearchBar(inst, mem, inst->searchRect);
        PaintTabs(inst, mem);
        if (inst->showStatus) PaintStatusBar(inst, mem, inst->statusRect);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hWnd, &ps); return 0;
    }

    case WM_DRAWITEM: {
        if (inst) { DrawButton(inst, (const DRAWITEMSTRUCT*)lParam); return TRUE; }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!inst) return 0;
        if (!inst->trackingMouse) {
            TRACKMOUSEEVENT tme{}; tme.cbSize = sizeof(tme); tme.dwFlags = TME_LEAVE; tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme); inst->trackingMouse = true;
        }
        if (MouseOverTabBar(inst, lParam)) {
            bool onClose = false;
            int idx = HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose);
            int newClose = onClose ? idx : -1;
            if (idx != inst->tabHot || newClose != inst->tabCloseHot) {
                inst->tabHot = idx; inst->tabCloseHot = newClose;
                InvalidateRect(hWnd, &inst->tabBarRect, FALSE);
            }
            SetCursor(LoadCursorW(nullptr, onClose ? IDC_HAND : IDC_ARROW));
            return 0;
        }
        if (inst->tabHot != -1 || inst->tabCloseHot != -1) {
            inst->tabHot = -1; inst->tabCloseHot = -1;
            InvalidateRect(hWnd, &inst->tabBarRect, FALSE);
        }
        UpdateHotButton(inst, hWnd, lParam);
        return 0;
    }

    case WM_MOUSELEAVE:
        if (inst) {
            inst->trackingMouse = false;
            if (inst->tabHot != -1 || inst->tabCloseHot != -1) {
                inst->tabHot = -1; inst->tabCloseHot = -1;
                InvalidateRect(hWnd, &inst->tabBarRect, FALSE);
            }
        }
        return 0;

    case WM_LBUTTONDOWN: {
        if (!inst) return 0;
        if (MouseOverTabBar(inst, lParam)) {
            if (inst->hRenameEdit) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                RECT r; GetWindowRect(inst->hRenameEdit, &r);
                POINT tl = { r.left, r.top }; ScreenToClient(hWnd, &tl);
                RECT er = { tl.x, tl.y, tl.x + (r.right-r.left), tl.y + (r.bottom-r.top) };
                if (!PtInRect(&er, pt)) CommitRename(inst, true);
            }
            bool onClose = false;
            int idx = HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose);
            if (idx == -2) DoNewTab(inst);
            else if (idx >= 0) { if (onClose) CloseDoc(inst, idx); else SetActiveDoc(inst, idx); }
            return 0;
        }
        break;
    }

    case WM_MBUTTONDOWN:
        if (inst && MouseOverTabBar(inst, lParam)) {
            if (inst->hRenameEdit) CommitRename(inst, true);
            bool onClose = false;
            int idx = HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose);
            if (idx >= 0) CloseDoc(inst, idx);
            return 0;
        }
        break;

    case WM_RBUTTONUP:
        if (inst && MouseOverTabBar(inst, lParam)) {
            if (inst->hRenameEdit) CommitRename(inst, true);
            bool onClose = false;
            int idx = HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose);
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hWnd, &pt);
            ShowTabMenu(inst, idx, pt.x, pt.y);
            return 0;
        }
        break;

    case WM_LBUTTONDBLCLK:
        if (inst && MouseOverTabBar(inst, lParam)) {
            bool onClose = false;
            int idx = HitTestTab(inst, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), onClose);
            if (idx < 0) DoNewTab(inst);
            return 0;
        }
        break;

    case WM_RENAME_COMMIT:
        if (inst) CommitRename(inst, wParam != 0);
        return 0;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        if (!inst) return DefWindowProcW(hWnd, msg, wParam, lParam);
        HDC hdc = (HDC)wParam; HWND child = (HWND)lParam;
        if (child == inst->hSearchFind || child == inst->hSearchRepl || child == inst->hRenameEdit) {
            SetTextColor(hdc, inst->theme.TextHi);
            SetBkColor(hdc, inst->theme.BgCard);
            if (inst->brSearchBg) DeleteObject(inst->brSearchBg);
            inst->brSearchBg = CreateSolidBrush(inst->theme.BgCard);
            return (LRESULT)inst->brSearchBg;
        }
        SetTextColor(hdc, inst->theme.TextMid);
        SetBkColor(hdc, inst->theme.BgApp);
        if (inst->brAppBg) DeleteObject(inst->brAppBg);
        inst->brAppBg = CreateSolidBrush(inst->theme.BgApp);
        return (LRESULT)inst->brAppBg;
    }

    case WM_COMMAND: {
        if (!inst) return 0;
        int id = LOWORD(wParam), code = HIWORD(wParam);
        switch (id) {
        case IDC_TB_MENU:  ShowMainMenu(inst, hWnd); return 0;
        case IDC_TB_NEW:   DoNewTab(inst); return 0;
        case IDC_TB_OPEN:  DoOpen(inst); return 0;
        case IDC_TB_SAVE:  DoSave(inst, false); return 0;
        case IDC_TB_UNDO:  if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, EM_UNDO, 0, 0); return 0;
        case IDC_TB_REDO:  if (inst->hEdit && !inst->readOnly) SendMessageW(inst->hEdit, EM_REDO, 0, 0); return 0;
        case IDC_TB_FIND:  ShowSearch(inst, !inst->searchOpen, false); return 0;
        case IDC_TB_ZOOM_IN:  DispatchCommand(inst, hWnd, IDM_ZOOM_IN); return 0;
        case IDC_TB_ZOOM_OUT: DispatchCommand(inst, hWnd, IDM_ZOOM_OUT); return 0;
        case IDC_TB_WRAP:     DispatchCommand(inst, hWnd, IDM_WRAP); return 0;
        case IDC_SEARCH_NEXT:    FindNext(inst, false); return 0;
        case IDC_SEARCH_PREV:    FindNext(inst, true); return 0;
        case IDC_SEARCH_CLOSE:   ShowSearch(inst, false, false); return 0;
        case IDC_SEARCH_CASE:
            inst->searchCase = !inst->searchCase;
            if (inst->hSearchCase) InvalidateRect(inst->hSearchCase, nullptr, TRUE);
            return 0;
        case IDC_SEARCH_REPL_ONE: ReplaceOne(inst); return 0;
        case IDC_SEARCH_REPL_ALL: { int n = ReplaceAll(inst); if (n == 0) MessageBeep(MB_OK); return 0; }
        case IDC_SEARCH_FIND: (void)code; return 0;
        }
        return 0;
    }

    case WM_NOTIFY: {
        if (!inst) return 0;
        NMHDR* p = (NMHDR*)lParam;
        if (p->hwndFrom == inst->hEdit && p->code == EN_CHANGE) {
            bool modified = (BOOL)SendMessageW(inst->hEdit, EM_GETMODIFY, 0, 0);
            if (modified != inst->modified) {
                inst->modified = modified;
                Document* d = Active(inst); if (d) d->modified = modified;
                UpdateTitle(inst);
                InvalidateRect(hWnd, &inst->tabBarRect, FALSE);
                FireNotify(inst, NP_NOTIFY_MODIFIED_CHANGED, inst->activeDoc);
            }
            InvalidateRect(hWnd, &inst->statusRect, FALSE);
            FireNotify(inst, NP_NOTIFY_CONTENT_CHANGED, inst->activeDoc);
        }
        return 0;
    }

    case WM_SETFOCUS:
        if (inst && inst->hEdit) SetFocus(inst->hEdit);
        return 0;

    case WM_KEYDOWN:
        if (!inst) break;
        // Check suppressed shortcuts
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            int cmd = 0;
            switch (wParam) {
            case 'N': cmd = IDM_NEW; break;
            case 'T': cmd = IDM_NEW; break;
            case 'W': cmd = IDM_EXIT; break;
            case VK_TAB: {
                if (inst->docs.size() > 1) {
                    int next = (inst->activeDoc + 1) % (int)inst->docs.size();
                    SetActiveDoc(inst, next);
                }
                return 0;
            }
            case 'O': cmd = IDM_OPEN; break;
            case 'S': cmd = (GetKeyState(VK_SHIFT) & 0x8000) ? IDM_SAVEAS : IDM_SAVE; break;
            case 'F': cmd = IDM_FIND; break;
            case 'H': cmd = IDM_REPLACE; break;
            case VK_OEM_PLUS: case VK_ADD: cmd = IDM_ZOOM_IN; break;
            case VK_OEM_MINUS: case VK_SUBTRACT: cmd = IDM_ZOOM_OUT; break;
            case '0': cmd = IDM_ZOOM_RESET; break;
            }
            // Ctrl+W is close-tab, not exit
            if (wParam == 'W') {
                if (inst->suppressedCmds.count(IDM_EXIT)) return DefWindowProcW(hWnd, msg, wParam, lParam);
                if (inst->activeDoc >= 0) CloseDoc(inst, inst->activeDoc);
                return 0;
            }
            // Ctrl+T is new tab
            if (wParam == 'T') {
                if (inst->suppressedCmds.count(IDM_NEW)) return DefWindowProcW(hWnd, msg, wParam, lParam);
                DoNewTab(inst); return 0;
            }
            if (cmd && inst->suppressedCmds.count(cmd)) return DefWindowProcW(hWnd, msg, wParam, lParam);
            if (cmd) { DispatchCommand(inst, hWnd, cmd); return 0; }
        } else {
            if (wParam == VK_F2) { if (inst->activeDoc >= 0) BeginRenameTab(inst, inst->activeDoc); return 0; }
            if (wParam == VK_F3) { FindNext(inst, false); return 0; }
            if (wParam == VK_F5) { DispatchCommand(inst, hWnd, IDM_DATETIME); return 0; }
        }
        break;

    case WM_CLOSE:
        if (!inst) return 0;
        if (inst->isChild) {
            // In child mode, notify host and let it decide
            FireNotify(inst, NP_NOTIFY_TAB_CLOSED, -1);
            return 0;
        }
        if (!CloseAllDocs(inst)) return 0;
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY: {
        if (inst) {
            // Save settings if standalone
            if (!inst->hostManaged && !inst->isChild) {
                SyncToActive(inst);
                RegWriteStr(L"ShowStatus", inst->showStatus ? L"1" : L"0");
                RegWriteStr(L"Zoom", std::to_wstring(inst->zoom));
                RegWriteStr(L"WordWrap", inst->wordWrap ? L"1" : L"0");
                RegWriteStr(L"FontFace", inst->logFont.lfFaceName);
                RegWriteStr(L"FontSize", std::to_wstring(inst->logFont.lfHeight));
                RegWriteStr(L"ToolbarDock", std::to_wstring((int)inst->tbDock));
                RegWriteStr(L"ToolbarSize", std::to_wstring((int)inst->tbSize));
            }
            for (Document* d : inst->docs) { if (d->hEdit) DestroyWindow(d->hEdit); delete d; }
            inst->docs.clear();
            if (inst->font) DeleteObject(inst->font);
            if (inst->fontBold) DeleteObject(inst->fontBold);
            if (inst->fontSmall) DeleteObject(inst->fontSmall);
            if (inst->brSearchBg) DeleteObject(inst->brSearchBg);
            if (inst->brAppBg) DeleteObject(inst->brAppBg);
            if (inst->iconFont) DeleteObject(inst->iconFont);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
            delete inst;
        }
        if (!GetInstance(hWnd) && !FindWindowW(PANEL_CLASS, nullptr))
            PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================
//  DLL initialization + class registration
// ============================================================
static void RegisterClasses() {
    if (g_classesRegistered) return;
    WNDCLASSW wc{};
    wc.lpfnWndProc = PanelWndProc; wc.hInstance = g_hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; wc.lpszClassName = PANEL_CLASS;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION); wc.style = CS_DBLCLKS;
    RegisterClassW(&wc);
    WNDCLASSW tbc{};
    tbc.lpfnWndProc = ToolbarHostProc; tbc.hInstance = g_hInst;
    tbc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    tbc.hbrBackground = nullptr; tbc.lpszClassName = TOOLBAR_CLASS;
    RegisterClassW(&tbc);
    EnsureMenuClass();
    g_classesRegistered = true;
}

NP_API BOOL InitializeNotepadPanel(HINSTANCE hInstance) {
    if (!g_hInst) g_hInst = hInstance ? hInstance : GetModuleHandleW(nullptr);
    if (!g_hInst) return FALSE;

    if (!g_msftedit) g_msftedit = LoadLibraryW(L"msftedit.dll");
    if (!g_msftedit) return FALSE;

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    RegisterClasses();
    return g_classesRegistered ? TRUE : FALSE;
}

static void CreateInstanceFonts(NotepadInstance* inst) {
    auto mk = [&](int pt, int weight, const wchar_t* face) -> HFONT {
        return CreateFontW(-MulDiv(pt, inst->dpi, 72), 0, 0, 0, weight, 0, 0, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, face); };
    inst->font = mk(10, FW_NORMAL, L"Segoe UI");
    inst->fontBold = mk(10, FW_SEMIBOLD, L"Segoe UI");
    inst->fontSmall = mk(9, FW_NORMAL, L"Segoe UI");
}

static void LoadAppSettings(NotepadInstance* inst) {
    inst->showStatus = RegReadStr(L"ShowStatus", L"1") == L"1";
    inst->zoom = RegReadInt(L"Zoom", 100);
    if (inst->zoom < 30) inst->zoom = 30;
    if (inst->zoom > 500) inst->zoom = 500;
    inst->wordWrap = RegReadStr(L"WordWrap", L"1") == L"1";
    int d = RegReadInt(L"ToolbarDock", (int)TBDOCK_TOP);
    if (d < TBDOCK_TOP || d > TBDOCK_FLOAT) d = TBDOCK_TOP;
    inst->tbDock = (ToolbarDock)d;
    int s = RegReadInt(L"ToolbarSize", (int)TBSIZE_MEDIUM);
    if (s < TBSIZE_SMALL || s > TBSIZE_LARGE) s = TBSIZE_MEDIUM;
    inst->tbSize = (ToolbarSize)s;
    ZeroMemory(&inst->logFont, sizeof(inst->logFont));
    std::wstring face = RegReadStr(L"FontFace", L"Segoe UI");
    int size = RegReadInt(L"FontSize", -MulDiv(11, inst->dpi, 72));
    wcscpy_s(inst->logFont.lfFaceName, face.c_str());
    inst->logFont.lfHeight = size;
    inst->logFont.lfWeight = FW_NORMAL;
    inst->logFont.lfCharSet = DEFAULT_CHARSET;
    inst->logFont.lfQuality = CLEARTYPE_QUALITY;
}

static void InitInstance(NotepadInstance* inst, HWND hWnd, HWND hParent,
                         HINSTANCE hInst, NPNOTIFYPROC notifyProc, void* userData) {
    inst->hSelf = hWnd;
    inst->hParent = hParent;
    inst->hInst = hInst;
    inst->instanceId = NotepadInstance::s_nextId++;
    inst->theme = DefaultDarkTheme();
    inst->notifyProc = notifyProc;
    inst->notifyUserData = userData;
    inst->isChild = (GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_CHILD) != 0;
    inst->hostManaged = (notifyProc != nullptr) || inst->isChild;
    inst->dpi = GetDeviceCaps(GetDC(hWnd), LOGPIXELSY);

    CreateInstanceFonts(inst);
    inst->iconFont = CreateFontW(-Dpi(inst, 18), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe Fluent Icons");
    HDC iconDc = GetDC(hWnd);
    WORD menuGlyphIndex = 0;
    HGDIOBJ previousIconFont = iconDc && inst->iconFont ? SelectObject(iconDc, inst->iconFont) : nullptr;
    DWORD glyphLookup = iconDc && inst->iconFont ? GetGlyphIndicesW(iconDc, L"\uE700", 1, &menuGlyphIndex, GGI_MARK_NONEXISTING_GLYPHS) : GDI_ERROR;
    if (iconDc && previousIconFont) SelectObject(iconDc, previousIconFont);
    if (iconDc) ReleaseDC(hWnd, iconDc);
    if (glyphLookup == GDI_ERROR || menuGlyphIndex == 0xFFFF) {
        if (inst->iconFont) DeleteObject(inst->iconFont);
        inst->iconFont = CreateFontW(-Dpi(inst, 18), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe MDL2 Assets");
    }
    if (!inst->hostManaged) LoadAppSettings(inst);
    else {
        // Default settings for host-managed mode
        ZeroMemory(&inst->logFont, sizeof(inst->logFont));
        wcscpy_s(inst->logFont.lfFaceName, L"Segoe UI");
        inst->logFont.lfHeight = -MulDiv(11, inst->dpi, 72);
        inst->logFont.lfWeight = FW_NORMAL;
        inst->logFont.lfCharSet = DEFAULT_CHARSET;
        inst->logFont.lfQuality = CLEARTYPE_QUALITY;
    }

    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)inst);



    CreateDoc(inst, L"", L"");
    SetActiveDoc(inst, 0);


    inst->hToolbarHost = CreateWindowW(TOOLBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 100, Dpi(inst, 46), hWnd, nullptr, hInst, nullptr);
    static const int tbIds[10] = {
        IDC_TB_MENU, IDC_TB_NEW, IDC_TB_OPEN, IDC_TB_SAVE,
        IDC_TB_UNDO, IDC_TB_REDO, IDC_TB_FIND,
        IDC_TB_ZOOM_OUT, IDC_TB_ZOOM_IN, IDC_TB_WRAP
    };
    for (int i = 0; i < 10; ++i) {
        inst->hTbBtns[i] = MakeButton(inst, inst->hToolbarHost, tbIds[i]);
        SetWindowSubclass(inst->hTbBtns[i], ToolbarBtnProc, 100 + i, 0);
    }


    inst->hSearchFind = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 28,
        hWnd, (HMENU)(INT_PTR)IDC_SEARCH_FIND, hInst, nullptr);
    inst->hSearchRepl = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 28,
        hWnd, (HMENU)(INT_PTR)IDC_SEARCH_REPL, hInst, nullptr);
    SendMessageW(inst->hSearchFind, WM_SETFONT, (WPARAM)inst->font, TRUE);
    SendMessageW(inst->hSearchRepl, WM_SETFONT, (WPARAM)inst->font, TRUE);
    SetWindowTheme(inst->hSearchFind, L"DarkMode_Explorer", nullptr);
    SetWindowTheme(inst->hSearchRepl, L"DarkMode_Explorer", nullptr);
    inst->hSearchNext = MakeButton(inst, hWnd, IDC_SEARCH_NEXT);
    inst->hSearchPrev = MakeButton(inst, hWnd, IDC_SEARCH_PREV);
    inst->hSearchClose = MakeButton(inst, hWnd, IDC_SEARCH_CLOSE);
    inst->hSearchCase = MakeButton(inst, hWnd, IDC_SEARCH_CASE);
    inst->hSearchReplOne = MakeButton(inst, hWnd, IDC_SEARCH_REPL_ONE);
    inst->hSearchReplAll = MakeButton(inst, hWnd, IDC_SEARCH_REPL_ALL);
    HWND searchCtrls[8] = {
        inst->hSearchFind, inst->hSearchRepl, inst->hSearchNext, inst->hSearchPrev,
        inst->hSearchClose, inst->hSearchCase, inst->hSearchReplOne, inst->hSearchReplAll
    };
    for (HWND h : searchCtrls) if (h) ShowWindow(h, SW_HIDE);

    if (inst->tbDock != TBDOCK_TOP) {
        ToolbarDock want = inst->tbDock; inst->tbDock = TBDOCK_TOP;
        SetToolbarDock(inst, want);
    }

    UpdateLayout(inst, hWnd);
}

// ============================================================
//  DLL entry point
// ============================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        InitializeNotepadPanel((HINSTANCE)hModule);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_msftedit) { FreeLibrary(g_msftedit); g_msftedit = nullptr; }
    }
    return TRUE;
}

// ============================================================
//  Exported API: Creation / destruction
// ============================================================

NP_API HWND CreateNotepadPanel(HWND hParent, const RECT* rc, int id,
                               NPNOTIFYPROC notifyProc, void* userData) {
    if (!hParent) return nullptr;
    RECT actualRc;
    if (!rc || (rc->right == 0 && rc->bottom == 0)) {
        GetClientRect(hParent, &actualRc);
    } else {
        actualRc = *rc;
    }
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_TABSTOP;
    HWND hWnd = CreateWindowExW(0, PANEL_CLASS, L"", style,
        actualRc.left, actualRc.top,
        actualRc.right - actualRc.left,
        actualRc.bottom - actualRc.top,
        hParent, (HMENU)(INT_PTR)id, g_hInst, nullptr);
    if (!hWnd) return nullptr;

    NotepadInstance* inst = new NotepadInstance();
    InitInstance(inst, hWnd, hParent, g_hInst, notifyProc, userData);
    return hWnd;
}

NP_API BOOL DestroyNotepadPanel(HWND hPanel) {
    if (!hPanel || !IsWindow(hPanel)) return FALSE;
    if (!GetInstance(hPanel)) return FALSE;
    DestroyWindow(hPanel);
    return TRUE;
}

NP_API BOOL ResizeNotepadPanel(HWND hPanel, const RECT* rc) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return FALSE;
    RECT actualRc;
    if (!rc || (rc->right == 0 && rc->bottom == 0)) {
        GetClientRect(inst->hParent, &actualRc);
    } else {
        actualRc = *rc;
    }
    MoveWindow(hPanel, actualRc.left, actualRc.top,
        actualRc.right - actualRc.left,
        actualRc.bottom - actualRc.top, TRUE);
    return TRUE;
}

// ============================================================
//  Exported API: Content I/O
// ============================================================

NP_API int GetTextW(HWND hPanel, wchar_t* buf, int bufLen) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || !inst->hEdit) return 0;
    std::wstring text = GetEditorText(inst);
    if (!buf) return (int)text.size();
    int copy = (int)text.size();
    if (copy >= bufLen) copy = bufLen - 1;
    wcsncpy_s(buf, bufLen, text.c_str(), copy);
    buf[copy] = 0;
    return copy;
}

NP_API BOOL SetTextW(HWND hPanel, const wchar_t* text) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || !inst->hEdit) return FALSE;
    std::wstring normalized;
    if (text) {
        normalized.reserve(std::wcslen(text) + 16);
        for (const wchar_t* p = text; *p; ++p) {
            if (*p == L'\r') continue;
            if (*p == L'\n') normalized += L"\r\n";
            else normalized += *p;
        }
    }
    SETTEXTEX st{}; st.flags = ST_DEFAULT; st.codepage = 1200;
    SendMessageW(inst->hEdit, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)normalized.c_str());
    SendMessageW(inst->hEdit, EM_SETMODIFY, FALSE, 0);
    SetModified(inst, false);
    return TRUE;
}

NP_API int GetTextLengthW(HWND hPanel) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || !inst->hEdit) return 0;
    return GetCharCount(inst);
}

NP_API int GetSelectedTextW(HWND hPanel, wchar_t* buf, int bufLen) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || !inst->hEdit) return 0;
    DWORD sel = (DWORD)SendMessageW(inst->hEdit, EM_GETSEL, 0, 0);
    int s = LOWORD(sel), e = HIWORD(sel);
    int len = e - s;
    if (len <= 0) { if (buf && bufLen > 0) buf[0] = 0; return 0; }
    if (!buf) return len;
    int copy = (len < bufLen - 1) ? len : bufLen - 1;
    std::wstring cur(copy, 0);
    TEXTRANGEW tr{}; tr.chrg.cpMin = s; tr.chrg.cpMax = s + copy; tr.lpstrText = cur.data();
    SendMessageW(inst->hEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    wcsncpy_s(buf, bufLen, cur.c_str(), copy);
    buf[copy] = 0;
    return copy;
}

NP_API BOOL SetSelectedTextW(HWND hPanel, const wchar_t* text) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || !inst->hEdit || inst->readOnly) return FALSE;
    if (!text) return FALSE;
    SendMessageW(inst->hEdit, EM_REPLACESEL, TRUE, (LPARAM)text);
    return TRUE;
}

// ============================================================
//  Exported API: Tab management
// ============================================================

NP_API int GetTabCount(HWND hPanel) {
    NotepadInstance* inst = GetInstance(hPanel);
    return inst ? (int)inst->docs.size() : 0;
}

NP_API int GetActiveTab(HWND hPanel) {
    NotepadInstance* inst = GetInstance(hPanel);
    return inst ? inst->activeDoc : -1;
}

NP_API BOOL SetActiveTab(HWND hPanel, int index) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return FALSE;
    if (index < 0 || index >= (int)inst->docs.size()) return FALSE;
    SetActiveDoc(inst, index);
    return TRUE;
}

NP_API int OpenDocument(HWND hPanel, const wchar_t* path) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return -1;
    if (!path) {
        DoNewTab(inst);
        return (int)inst->docs.size() - 1;
    }
    std::wstring text;
    if (!ReadFileText(path, text)) return -1;
    CreateDoc(inst, text, path);
    SetActiveDoc(inst, (int)inst->docs.size() - 1);
    UpdateLayout(inst, hPanel);
    InvalidateRect(hPanel, nullptr, TRUE);
    FireNotify(inst, NP_NOTIFY_TAB_OPENED, (int)inst->docs.size() - 1);
    return (int)inst->docs.size() - 1;
}

NP_API BOOL CloseTab(HWND hPanel, int index, BOOL promptSave) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return FALSE;
    if (index < 0 || index >= (int)inst->docs.size()) return FALSE;
    if (promptSave && inst->docs[index]->modified) {
        if (!ConfirmSaveDoc(inst, inst->docs[index])) return FALSE;
    }
    return CloseDoc(inst, index);
}

NP_API int GetTabFilePathW(HWND hPanel, int index, wchar_t* buf, int bufLen) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || index < 0 || index >= (int)inst->docs.size()) return 0;
    std::wstring path = inst->docs[index]->filePath;
    if (!buf) return (int)path.size();
    int copy = (int)path.size();
    if (copy >= bufLen) copy = bufLen - 1;
    wcsncpy_s(buf, bufLen, path.c_str(), copy);
    buf[copy] = 0;
    return copy;
}

NP_API int GetTabTitleW(HWND hPanel, int index, wchar_t* buf, int bufLen) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || index < 0 || index >= (int)inst->docs.size()) return 0;
    std::wstring title = inst->docs[index]->Title();
    if (!buf) return (int)title.size();
    int copy = (int)title.size();
    if (copy >= bufLen) copy = bufLen - 1;
    wcsncpy_s(buf, bufLen, title.c_str(), copy);
    buf[copy] = 0;
    return copy;
}

NP_API BOOL IsTabModified(HWND hPanel, int index) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || index < 0 || index >= (int)inst->docs.size()) return FALSE;
    return inst->docs[index]->modified ? TRUE : FALSE;
}

NP_API BOOL SaveTab(HWND hPanel, int index, BOOL saveAs) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || index < 0 || index >= (int)inst->docs.size()) return FALSE;
    SetActiveDoc(inst, index);
    DoSave(inst, saveAs != FALSE);
    return !inst->modified;
}

// ============================================================
//  Exported API: Theme
// ============================================================

NP_API BOOL SetTheme(HWND hPanel, const NPTheme* theme) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return FALSE;
    if (!theme) { inst->theme = DefaultDarkTheme(); }
    else {
        inst->theme.BgApp = theme->bgApp; inst->theme.BgPanel = theme->bgPanel;
        inst->theme.BgCard = theme->bgCard; inst->theme.BgBtn = theme->bgBtn;
        inst->theme.BgBtnHi = theme->bgBtnHi; inst->theme.BgRailHi = theme->bgBtnHi;
        inst->theme.Border = theme->border; inst->theme.BorderSoft = theme->borderSoft;
        inst->theme.TextHi = theme->textHi; inst->theme.TextMid = theme->textMid;
        inst->theme.TextLow = theme->textLow; inst->theme.Accent = theme->accent;
        inst->theme.SelBg = theme->selBg; inst->theme.IconIdle = theme->iconIdle;
        inst->theme.IconActive = theme->iconActive;
        // Apply to RichEdit
        for (Document* d : inst->docs) {
            if (d->hEdit) {
                SendMessageW(d->hEdit, EM_SETBKGNDCOLOR, 0, inst->theme.BgApp);
                CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf);
                cf.dwMask = CFM_COLOR; cf.crTextColor = inst->theme.TextHi;
                SendMessageW(d->hEdit, EM_SETCHARFORMAT, SCF_DEFAULT | SCF_ALL, (LPARAM)&cf);
            }
        }
    }
    if (inst->hSelf) InvalidateRect(inst->hSelf, nullptr, TRUE);
    return TRUE;
}

NP_API BOOL GetTheme(HWND hPanel, NPTheme* outTheme) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || !outTheme) return FALSE;
    outTheme->bgApp = inst->theme.BgApp; outTheme->bgPanel = inst->theme.BgPanel;
    outTheme->bgCard = inst->theme.BgCard; outTheme->bgBtn = inst->theme.BgBtn;
    outTheme->bgBtnHi = inst->theme.BgBtnHi; outTheme->border = inst->theme.Border;
    outTheme->borderSoft = inst->theme.BorderSoft; outTheme->textHi = inst->theme.TextHi;
    outTheme->textMid = inst->theme.TextMid; outTheme->textLow = inst->theme.TextLow;
    outTheme->accent = inst->theme.Accent; outTheme->selBg = inst->theme.SelBg;
    outTheme->iconIdle = inst->theme.IconIdle; outTheme->iconActive = inst->theme.IconActive;
    return TRUE;
}

// ============================================================
//  Exported API: Font
// ============================================================

NP_API BOOL SetFontW(HWND hPanel, const LOGFONTW* logFont) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return FALSE;
    if (!logFont) {
        ZeroMemory(&inst->logFont, sizeof(inst->logFont));
        wcscpy_s(inst->logFont.lfFaceName, L"Segoe UI");
        inst->logFont.lfHeight = -MulDiv(11, inst->dpi, 72);
        inst->logFont.lfWeight = FW_NORMAL;
        inst->logFont.lfCharSet = DEFAULT_CHARSET;
        inst->logFont.lfQuality = CLEARTYPE_QUALITY;
    } else {
        inst->logFont = *logFont;
    }
    ApplyEditorFont(inst);
    return TRUE;
}

NP_API BOOL GetFontW(HWND hPanel, LOGFONTW* outLogFont) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || !outLogFont) return FALSE;
    *outLogFont = inst->logFont;
    return TRUE;
}

// ============================================================
//  Exported API: View settings
// ============================================================

NP_API BOOL SetWordWrap(HWND hPanel, BOOL wrap) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return FALSE;
    inst->wordWrap = wrap != FALSE; SyncToActive(inst); ApplyWordWrap(inst);
    return TRUE;
}
NP_API BOOL GetWordWrap(HWND hPanel) {
    NotepadInstance* inst = GetInstance(hPanel);
    return inst ? (inst->wordWrap ? TRUE : FALSE) : FALSE;
}
NP_API BOOL SetZoom(HWND hPanel, int percent) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return FALSE;
    if (percent < 30) percent = 30; if (percent > 500) percent = 500;
    inst->zoom = percent; SyncToActive(inst); ApplyZoom(inst);
    FireNotify(inst, NP_NOTIFY_ZOOM_CHANGED, inst->activeDoc);
    return TRUE;
}
NP_API int GetZoom(HWND hPanel) {
    NotepadInstance* inst = GetInstance(hPanel);
    return inst ? inst->zoom : 0;
}
NP_API BOOL SetStatusBarVisible(HWND hPanel, BOOL visible) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return FALSE;
    inst->showStatus = visible != FALSE;
    UpdateLayout(inst, hPanel); InvalidateRect(hPanel, nullptr, TRUE);
    return TRUE;
}
NP_API BOOL GetStatusBarVisible(HWND hPanel) {
    NotepadInstance* inst = GetInstance(hPanel);
    return inst ? (inst->showStatus ? TRUE : FALSE) : FALSE;
}
NP_API BOOL SetToolbarVisible(HWND hPanel, BOOL visible) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst || !inst->hToolbarHost) return FALSE;
    inst->tbVisible = visible != FALSE;
    ShowWindow(inst->hToolbarHost, inst->tbVisible ? SW_SHOW : SW_HIDE);
    UpdateLayout(inst, hPanel); InvalidateRect(hPanel, nullptr, TRUE);
    return TRUE;
}
NP_API BOOL GetToolbarVisible(HWND hPanel) {
    NotepadInstance* inst = GetInstance(hPanel);
    return (inst && inst->hToolbarHost && inst->tbVisible) ? TRUE : FALSE;
}

// ============================================================
//  Exported API: Read-only
// ============================================================

NP_API BOOL SetReadOnly(HWND hPanel, BOOL readOnly) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return FALSE;
    inst->readOnly = readOnly != FALSE;
    for (Document* d : inst->docs)
        if (d->hEdit) SendMessageW(d->hEdit, EM_SETREADONLY, inst->readOnly ? TRUE : FALSE, 0);
    if (inst->hSelf) InvalidateRect(inst->hSelf, nullptr, TRUE);
    return TRUE;
}
NP_API BOOL GetReadOnly(HWND hPanel) {
    NotepadInstance* inst = GetInstance(hPanel);
    return inst ? (inst->readOnly ? TRUE : FALSE) : FALSE;
}

// ============================================================
//  Exported API: Notifications
// ============================================================

NP_API BOOL SetNotifyProc(HWND hPanel, NPNOTIFYPROC proc, void* userData) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return FALSE;
    inst->notifyProc = proc;
    inst->notifyUserData = userData;
    return TRUE;
}

// ============================================================
//  Exported API: Keyboard accelerators
// ============================================================

static const NPAccel s_accelerators[] = {
    { IDM_NEW,        FVIRTKEY|FCONTROL, 'N' },
    { IDM_NEW,        FVIRTKEY|FCONTROL, 'T' },
    { IDM_OPEN,       FVIRTKEY|FCONTROL, 'O' },
    { IDM_SAVE,       FVIRTKEY|FCONTROL, 'S' },
    { IDM_SAVEAS,     FVIRTKEY|FCONTROL|FSHIFT, 'S' },
    { IDM_UNDO,       FVIRTKEY|FCONTROL, 'Z' },
    { IDM_REDO,       FVIRTKEY|FCONTROL, 'Y' },
    { IDM_CUT,        FVIRTKEY|FCONTROL, 'X' },
    { IDM_COPY,       FVIRTKEY|FCONTROL, 'C' },
    { IDM_PASTE,      FVIRTKEY|FCONTROL, 'V' },
    { IDM_SELECTALL,  FVIRTKEY|FCONTROL, 'A' },
    { IDM_FIND,       FVIRTKEY|FCONTROL, 'F' },
    { IDM_REPLACE,    FVIRTKEY|FCONTROL, 'H' },
    { IDM_FINDNEXT,   FVIRTKEY,          VK_F3 },
    { IDM_DATETIME,   FVIRTKEY,          VK_F5 },
    { IDM_ZOOM_IN,    FVIRTKEY|FCONTROL, VK_OEM_PLUS },
    { IDM_ZOOM_OUT,   FVIRTKEY|FCONTROL, VK_OEM_MINUS },
    { IDM_ZOOM_RESET, FVIRTKEY|FCONTROL, '0' },
    { IDM_WRAP,       0,                 0 },
    { IDM_FONT,       0,                 0 },
    { IDM_STATUS,     0,                 0 },
    { IDM_TOOLBAR,    0,                 0 },
};
static const int s_accelCount = sizeof(s_accelerators) / sizeof(s_accelerators[0]);

NP_API int GetAccelerators(NPAccel* outAccels, int maxCount) {
    if (!outAccels) return s_accelCount;
    int copy = (maxCount < s_accelCount) ? maxCount : s_accelCount;
    for (int i = 0; i < copy; ++i) outAccels[i] = s_accelerators[i];
    return copy;
}

NP_API void SetSuppressedShortcuts(HWND hPanel, const int* cmdIds, int count) {
    NotepadInstance* inst = GetInstance(hPanel);
    if (!inst) return;
    inst->suppressedCmds.clear();
    if (cmdIds && count > 0)
        for (int i = 0; i < count; ++i) inst->suppressedCmds.insert(cmdIds[i]);
}
