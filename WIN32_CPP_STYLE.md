# Win32 C++ style guide

Neage Notepad targets C++20 and the Windows SDK. These rules follow the NEWAGEAI
application and the NEWAGEWIN32UI control layout. They apply to every file in
this repository.

## Source layout and naming

- Keep one public module API in its matching `.h` file and its non-inline
  implementation in the matching `.cpp` file. Use `#pragma once` and include
  `"pch.h"` first in every project `.cpp` file.
- Place a module in its own directory when it owns more than one file:
  `Theme/Theme.h`, `Theme/Theme.cpp`. The panel host stays at the repository
  root because `NotepadPanel.h` is the published embedding contract.
- Use `PascalCase` for types and public functions, `camelCase` for parameters
  and locals, `m_` for members, `g_` for process globals, `s_` for file statics,
  and a `k` prefix for named constants.
- Keep the `NP_` prefix on everything that crosses the public C boundary.
- Include the header that directly declares each dependency. Keep project
  headers before standard-library headers. Source files are Unicode and use
  `W`-suffixed Win32 APIs with UTF-16 strings.
- Use four spaces, braces on their own line, and one statement per line.
  `.clang-format` and `.editorconfig` hold the editor defaults.

### Module tiers

Follow the NEWAGEWIN32UI split when a module wraps a control:

```
Controls/Standard/    common-control wrappers (edit, list, status, tab, ...)
Controls/Advanced/    owner-drawn or multi-part controls (menu, split button, ...)
```

A wrapper owns its child `HWND`, is non-copyable, and destroys the child in
`Reset()`. The parent window stays responsible for routing notifications and
must outlive the wrapper. Modules that are not controls (theme, configuration,
persistence) live in their own top-level directory instead.

## Comments

- Write comments for public contracts, ownership and lifetime, required
  parent-window message handling, units, and non-obvious Win32 behaviour.
  Do not narrate each statement.
- Use `///` comments immediately above public types and APIs. State what the
  caller must provide, what the callee owns, and any thread or DPI constraint.
  `NotepadPanel.h` is a C-compatible header and keeps `//` block comments so the
  same text reads correctly in C and C++ documentation tools.
- Use concise `// ---- Section ----` headings for substantial implementation
  sections. Do not use `// =====` banners.
- Add a short inline comment only when it explains a non-obvious invariant or a
  Win32-specific reason, for example:

```cpp
// Ctrl+W closes the tab; the host keeps Ctrl+F4 for the frame.
case IDM_TAB_CLOSE:
```

```cpp
// EM_SETBKGNDCOLOR takes the colour in lParam; wParam must be zero.
SendMessageW(inst->hEdit, EM_SETBKGNDCOLOR, 0, inst->theme.BgApp);
```

- Keep comments accurate when the code changes. Prefer descriptive names and
  small functions over a comment that restates the line.

### What not to write

Comments that restate the statement add noise and rot quickly. All of the
following were removed from this codebase and must not come back:

```cpp
} // Close the current block.
return TRUE; // Report success to the caller.
if (!inst) return FALSE; // Continue this branch only when the state object is unavailable.
Palette t; // Declare t for use by this block.
t.BgApp = RGB(22, 22, 25); // Update the bg app field with the calculated value.
size_t p = path.find_last_of(L"\\/"); // Declare p and start with path.find_last_of(L"\\/").
```

If a block needs a signpost, name it:

```cpp
struct NotepadInstance
{
    // Window handles
    HWND hSelf = nullptr;
    HWND hParent = nullptr;

    // Documents
    std::vector<Document *> docs;
};
```

## Theme

- All colours come from `Theme/Theme.h`. No `RGB()` constant belongs in a paint
  function.
- A component stores a `ThemePalette` copy and reads it at paint time. It never
  caches brushes across a theme change; brushes are recreated and the window is
  invalidated.
- `ThemeMode::System` is resolved through `ResolveDarkMode()` at the moment the
  palette is needed. A component that reacts to live changes handles
  `WM_SETTINGCHANGE` with `lParam == L"ImmersiveColorSet"`.
- Child controls follow the mode through `ApplyControlTheme()`; top-level frames
  call `ApplyTitleBarTheme()`. The embedded panel does not touch the host frame.
- Layout metrics are expressed in 96-DPI units and converted with
  `ScaleMetric(value, dpi)`. Font sizes are points and use `MulDiv(size, dpi, 72)`.

## Ownership and errors

- Treat every Win32 handle as either owned or borrowed. An owning wrapper is
  non-copyable and movable; `Reset()` destroys the owned resource and `Release()`
  transfers it. Never destroy a borrowed handle.
- The panel owns its child windows, fonts, brushes, and the instance record. The
  host owns the panel window itself and must call `DestroyNotepadPanel` before
  destroying its own window.
- Validate parameters at public boundaries. Return `bool` for success/failure,
  `std::optional` where an empty value differs from failure, and document when a
  function returns a native Win32 previous-state/index value.
- Callbacks fired from the panel are synchronous. A callback must not destroy the
  panel from inside the notification; post a message instead.

## Adding a module

1. Create `Module/Module.h` with the public contract only: enums, structs, and
   `///`-documented function declarations. No implementation details, no
   `static` helpers.
2. Create `Module/Module.cpp`, starting with `"pch.h"` and then the module
   header, followed by `// ---- Name ----` sections.
3. Keep the module free of dependencies on `NotepadPanel.h`. Convert types at the
   boundary instead, so the module stays reusable by the host application.
4. Register both files in `Neage Notepad.vcxproj` and add them to a matching
   filter in `Neage Notepad.vcxproj.filters`.
5. Document the module in `README.md` and, when it changes the embedding
   surface, in `NOTEPAD_PANEL_API.md`.
6. Build `Debug|x64` and `Release|x64` before committing.

## Example API documentation

```cpp
/// Returns the palette for a concrete light/dark decision.
/// The caller owns the returned value; no GDI objects are created here.
ThemePalette GetPalette(bool dark) noexcept;

/// Applies the immersive dark caption to a top-level window.
/// Safe to call before the window is shown; ignored on systems without DWM.
void ApplyTitleBarTheme(HWND hwnd, bool dark) noexcept;
```

## Build integration

- Every `.cpp` implementation is listed in `Neage Notepad.vcxproj`; headers and
  sources also appear under a filter in `Neage Notepad.vcxproj.filters`.
- Link Win32 dependencies through the project settings
  (`comctl32.lib`, `comdlg32.lib`, `shell32.lib`, `dwmapi.lib`, `uxtheme.lib`)
  rather than source-level `#pragma comment(lib, ...)`.
- The Common Controls v6 dependency is declared in the project manifest
  settings, not with a linker pragma.
- Request the `ICC_*` initialization flags before creating a common control.
