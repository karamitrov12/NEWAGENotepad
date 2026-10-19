# Neage Notepad

A tabbed Windows notepad built with native Win32 C++20 and no UI framework. The
editor is a Rich Edit 4.1 control hosted by a panel window that can run either
inside this executable or embedded in a host application.

## Files

```
NotepadPanel.h            public C API for embedding the panel
NotepadPanel.cpp          panel window, tabs, toolbar, search, menus, exports
NEWAGENOTEPADMain.cpp     standalone frame window and message loop
Theme/Theme.h             theme modes, palettes, DPI and Win32 theming contract
Theme/Theme.cpp           palette tables, registry lookup, DWM and uxtheme calls
pch.h / pch.cpp           precompiled header (NOMINMAX, WIN32_LEAN_AND_MEAN)
resource.h / app.rc       icon resource
WIN32_CPP_STYLE.md        ownership, naming, and comment conventions
NOTEPAD_PANEL_API.md      API reference for embedders
EMBEDDING_SPEC.md         design notes and integration contract
.clang-format             formatter profile
.editorconfig             whitespace and encoding defaults
Neage Notepad.vcxproj     MSBuild project and Win32 library links
```

## Build

Open `Neage Notepad.vcxproj` in Visual Studio 2022 (Desktop development with
C++), select `x64` / `Debug`, and press F5. Command-line equivalent:

```
msbuild "Neage Notepad.vcxproj" -p:Configuration=Release -p:Platform=x64
```

The executable compiles `NotepadPanel.cpp` with `NOTEPAD_PANEL_STATIC`. To build
the panel as a DLL, set `NOTEPAD_PANEL_EXPORTS` and link `NotepadPanel.lib`.

## Theme

`Theme/Theme.h` is the single source of colour and DPI decisions:

```cpp
#include "Theme/Theme.h"

const bool dark = ResolveDarkMode(ThemeMode::System);
ApplyTitleBarTheme(hwnd, dark);        // top-level frames only
ApplyControlTheme(hwndEdit, dark);     // Rich Edit and inline editors
```

The panel exposes the same model through its C API:

```cpp
SetThemeMode(hPanel, NP_THEME_SYSTEM);   // System, Light, or Dark
SetThemeMode(hPanel, NP_THEME_LIGHT);
```

Panels start in `NP_THEME_DARK` so an existing host keeps the original look. The
standalone application selects `NP_THEME_SYSTEM` and forwards
`WM_SETTINGCHANGE` (`lParam == L"ImmersiveColorSet"`) so the editor follows the
Windows app theme live. A host palette applied with `SetTheme` is kept until the
mode changes.

## Reusable modules

Every non-panel component follows the NEWAGEWIN32UI layout: one public header
with the contract, one `.cpp` with the implementation, registered in
`.vcxproj` and `.vcxproj.filters`.

```
Module/Module.h       enums, structs, ///-documented declarations
Module/Module.cpp     "pch.h", then the module header, then // ---- sections
```

Control wrappers use the `Controls/Standard` (common controls) and
`Controls/Advanced` (owner-drawn or multi-part) split. A wrapper owns its child
`HWND`; the parent window must outlive it and still routes notifications.
Modules must not depend on `NotepadPanel.h` — convert types at the boundary
instead, as `NotepadPanel.cpp` does with `NPTheme`.

See [WIN32_CPP_STYLE.md](WIN32_CPP_STYLE.md) for the comment rules, ownership
rules, and the full module checklist.
