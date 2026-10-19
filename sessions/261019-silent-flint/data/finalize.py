import re
import sys

src = open(sys.argv[1], encoding='utf-8').read()

HEADER = '''// NotepadPanel.cpp -- Embeddable Notepad clone panel DLL
//
// Each panel window owns a NotepadInstance resolved from GWLP_USERDATA, so
// several panels can coexist in one process. This translation unit exports the
// public C API documented in NotepadPanel.h and owns every child window,
// font, brush, and document record the panel creates.
//
// Build: NOTEPAD_PANEL_STATIC for the static library configuration,
//        NOTEPAD_PANEL_EXPORTS for the DLL configuration.
//        msftedit.dll is loaded on demand by InitializeNotepadPanel.

#include "pch.h"

#if !defined(NOTEPAD_PANEL_STATIC) && !defined(NOTEPAD_PANEL_EXPORTS)
#define NOTEPAD_PANEL_EXPORTS
#endif

#include "NotepadPanel.h"
#include "Theme/Theme.h"

#include <windowsx.h>
#include <commctrl.h>
#include <richedit.h>
#include <shellapi.h>
#include <commdlg.h>

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

'''

marker = 'namespace fs = std::filesystem;'
idx = src.index(marker)
src = HEADER + src[idx:]

# Banner blocks -> // ---- Name ----
pattern = re.compile(
    r'// ={20,}\n//  ([^\n]+)\n// ={20,}\n', re.MULTILINE)
src = pattern.sub(lambda m: '// ---- %s ----\n' % m.group(1).strip(), src)

open(sys.argv[2], 'w', encoding='utf-8', newline='\n').write(src)
print('banners:', len(pattern.findall(open(sys.argv[1], encoding='utf-8').read())))
