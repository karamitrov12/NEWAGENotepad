# Adopt NEWAGEAI theme + Win32 C++ conventions in Neage Notepad

## Goal

Bring `D:\CPROJECTS\NEAGENOTEPAD` up to the standard set by the two reference projects:

1. **Theme** — implement NEWAGEAI's `ThemeMode` / `ThemeColors` model (System / Light / Dark, system- dark detection, DWM title bar theming, DPI scaling) as a real module in Neage Notepad.
2. **Programming style + commenting convention** — apply `NEWAGEAI\WIN32_CPP_STYLE.md` to every source file.
3. **Organization** — use `NEWAGEWIN32UI`'s one-component-per-`.h`/`.cpp` layout (Standard vs Advanced control tiers) as the rule for creating new modules, and document it.

No behaviour regressions: the public C API stays source- and binary-compatible, and the reformat is proven token-identical to the current code.

---

## What I found

### Reference: NEWAGEAI

`WIN32_CPP_STYLE.md` + `.clang-format` + `.editorconfig` define the convention:

| Rule | Reference implementation |
|---|---|
| Header layout | `#pragma once` → includes → `// ---- Section ----` → enums → structs → `///`-documented API |
| Public docs | `///` immediately above public types/functions, stating contract, ownership, units |
| Implementation sections | `// ---- Button Factory ----`, short, not `// =========` banners |
| Comments | Only for contracts, ownership/lifetime, required message handling, units, non-obvious Win32 behaviour — **never per-statement narration** |
| Formatting | 4 spaces, Allman braces, one statement per line, 100 col, UTF-16 `W` APIs |
| Ownership | Owned vs borrowed handles, `Reset()` / `Release()`, parent outlives child |
| Build | Every `.cpp` in `.vcxproj` + filters; libs in project settings, **not** `#pragma comment(lib, …)` |

Theme model lives in `Config.h`:

```cpp
enum class ThemeMode { System, Light, Dark };
struct ThemeColors { … };              // bg, text, caption, status, menu colours
inline ThemeColors GetThemeColors(bool dark);
```

with `ReadSystemDarkMode()` (reads `AppsUseLightTheme`), `ResolveTheme()` and `ApplyTitleBarTheme()` (`DwmSetWindowAttribute` 20→19, caption 35/36, border 34) in `main.cpp`.

### Reference: NEWAGEWIN32UI

`Controls/Standard/*` (common controls) and `Controls/Advanced/*` (owner-drawn/complex), **one type per header/cpp pair**, thin headers, implementation in the `.cpp`, reusable modules in `namespace win32::controls`.

### Target: NEAGENOTEPAD (current state)

| Problem | Evidence |
|---|---|
| Narration comments on almost every line | 2,579 of 2,742 lines in `NotepadPanel.cpp` carry `//`; 257× `// Close the current block.`, 132× `// Complete the current operation…`, 50× `// Supply this parameter…` |
| Wrong brace/format style | K&R braces, multiple statements per line (`wc.lpfnWndProc = …; wc.hInstance = …;`) |
| Banner style not in convention | `// ============================================================` |
| No theme module | `Palette` + `DefaultDarkTheme()` are file-static inside `NotepadPanel.cpp`; dark-only; `SetWindowTheme(hEdit, L"DarkMode_Explorer")` hardcoded |
| No system/light theme | No `ThemeMode`, no registry read, no light palette; `SetTheme(NULL)` always restores dark |
| No DPI/title-bar module | Inline `MulDiv` helper per file; `DwmSetWindowAttribute` inline in `NEWAGENOTEPADMain.cpp` |
| No PCH / style docs / formatter | no `pch.*`, no `WIN32_CPP_STYLE.md`, no `.clang-format`, no `.editorconfig` |
| Build drift | `NEWAGENOTEPADMain.cpp` missing from `.filters`; libs via `#pragma comment`; PCH unused |

`EMBEDDING_SPEC.md` §11 already specifies the intended end state (per-instance palette, light palette, `SetWindowTheme` per mode) — the code just never got there.

---

## Deliverables

### New files

| File | Purpose |
|---|---|
| `WIN32_CPP_STYLE.md` | Project convention: layout, naming, comment rules, ownership, theme rules, "how to add a module" checklist (adapted from NEWAGEAI, includes the `Controls/Standard` vs `Controls/Advanced` organization rule from NEWAGEWIN32UI) |
| `.clang-format`, `.editorconfig` | Exact copies of the NEWAGEAI profiles (Microsoft base, Allman, 4 spaces, 100 col) |
| `pch.h`, `pch.cpp` | `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, `_WIN32_IE`, `windows.h`, `<string>`, `resource.h` — reference convention |
| `Theme/Theme.h` | `ThemeMode`, `ThemePalette` (15 colours), `DarkPalette()`, `LightPalette()`, `GetPalette(ThemeMode)`, `IsSystemDarkMode()`, `ResolveDarkMode()`, `ScaleMetric()`, `ApplyTitleBarTheme()`, `ApplyControlTheme()` — all `///`-documented |
| `Theme/Theme.cpp` | Palette tables, registry read, DWM attribute application (20→19, 35/36/34), `SetWindowTheme` per mode. Sections as `// ---- Palette ----`, `// ---- System preference ----`, `// ---- DWM integration ----` |

### Modified files

| File | Change |
|---|---|
| `NotepadPanel.cpp` | Remove narration comments, Allman reformat, one statement per line, `// ---- … ----` sections; use `Theme/Theme.h`; new theme-mode exports + View-menu entries |
| `NotepadPanel.h` | Comment cleanup to the reference block style; additive `NP_THEME_*`, `NP_CMD_THEME_*`, `NP_NOTIFY_THEME_CHANGED`, `SetThemeMode` / `GetThemeMode` with `///`-level contract docs |
| `NEWAGENOTEPADMain.cpp` | Convention rewrite; resolve system theme, apply title bar via `Theme`, react to `WM_SETTINGCHANGE`, use PCH |
| `Neage Notepad.vcxproj` | Add `Theme/Theme.cpp`, `Theme/Theme.h`, `pch.*`; enable PCH; move libs to project settings (`comctl32;comdlg32;shell32;dwmapi;uxtheme`); Common-Controls v6 manifest via project |
| `Neage Notepad.vcxproj.filters` | Add `Theme` filter; register `NEWAGENOTEPADMain.cpp` (currently missing), `pch.*`, `Theme/*` |
| `README.md` | Project map, conventions pointer, build steps |
| `NOTEPAD_PANEL_API.md` | Document `SetThemeMode` / `GetThemeMode`, theme-mode constants, new notification, revised `SetTheme(NULL)` semantics |
| `EMBEDDING_SPEC.md` | §11 rewritten to match the shipped `Theme/` module |

---

## Theme design

```cpp
enum class ThemeMode
{
    System = 0,   // resolve against AppsUseLightTheme at the moment of use
    Light  = 1,
    Dark   = 2
};

struct ThemePalette
{
    COLORREF BgApp, BgPanel, BgCard, BgBtn, BgBtnHi, BgRailHi;
    COLORREF Border, BorderSoft, TextHi, TextMid, TextLow;
    COLORREF Accent, SelBg, IconIdle, IconActive;
};
```

- **Dark** = the current `DefaultDarkTheme()` values (pixel-identical, no visual change).
- **Light** = the palette already specified in `EMBEDDING_SPEC.md` §11.3.
- `ThemePalette` keeps the existing PascalCase field names, so the ~40 paint sites keep `inst->theme.BgApp`.
- `NPTheme` (the C struct) is converted at the API boundary; `Theme` has **no** dependency on the public header.
- `SetTheme(NULL)` restores the palette of the instance's current theme mode (dark when unset) — documented change from "always dark".
- Panel default stays **Dark** for embedders (no visual surprise); the standalone app switches itself to **System**.
- RichEdit + rename edit use `ApplyControlTheme(hwnd, dark)`; the frame uses `ApplyTitleBarTheme(hwnd, dark)`.
- New exports: `SetThemeMode(HWND, int)` / `GetThemeMode(HWND)`, plus `NP_NOTIFY_THEME_CHANGED`.
- View menu gains `Theme: System / Light / Dark` with check marks (`NP_CMD_THEME_SYSTEM/LIGHT/DARK`, 2050–2052).

---

## Comment + format policy (the core of the request)

1. **Delete** all statement narration — the generated families
   (`// Close the current block.`, `// Call X to …`, `// Declare X and start with …`,
   `// Continue this branch only when …`, `// Return the computed … to the caller.`,
   `// Update the … field with the calculated value.`, …). 2,579 comment-bearing lines → roughly 120.
2. **Keep or rewrite** only comments that carry information: file headers, control-ID/MSG mapping notes, struct field group labels (`// Window handles`, `// Documents`), genuine invariants (`// Ctrl+W is close-tab, not exit`, `// no default key`, `// 30..500`), and Win32 reasons.
3. **Section headings** become `// ---- Name ----` (reference style), not `// ====`.
4. **Public contracts** are documented once, in the header, above the declaration.
5. **Formatting**: `.clang-format` applied (`clang-format -i`), plus a pass that splits `a; b; c;` into one statement per line (clang-format does not do this).
6. **Safety proof**: a token-normalizer compares the current and refactored `NotepadPanel.cpp` — comments/whitespace stripped, tokens must match exactly. Any unintended logic edit shows up as a diff.

---

## Execution phases

**Phase 1 — Convention files (zero code risk)**
`WIN32_CPP_STYLE.md`, `.clang-format`, `.editorconfig`, `pch.h`/`pch.cpp`, README structure section.

**Phase 2 — Theme module**
Write `Theme/Theme.h` + `Theme/Theme.cpp`, register in project, build standalone. No callers yet, so the app is unchanged.

**Phase 3 — NotepadPanel refactor (largest diff)**
Strip narration → merge `// ====` banners into `// ---- ----` → split multi-statement lines → `clang-format` → token-identity check → wire `Theme/` (palette, control theme, mode field, exports, menu entries) → build.

**Phase 4 — App + docs**
`NEWAGENOTEPADMain.cpp` on the theme module (system resolve, `WM_SETTINGCHANGE`, title bar, DPI helper); update API docs and spec.

**Phase 5 — Verification**
`MSBuild "Neage Notepad.vcxproj" /p:Configuration=Debug /p:Platform=x64` and `Release|x64`; fix warnings; confirm token diff and public export list (`dumpbin /exports`).

---

## Verification checklist

- [ ] Debug|x64 and Release|x64 build clean at `/W3` with `SDLCheck`.
- [ ] Token-normalized `NotepadPanel.cpp` matches the pre-refactor logic (only theme deltas allowed).
- [ ] All previously exported symbols still exported; new symbols added; nothing removed.
- [ ] App launches, tabs/editor/search/toolbar/menus behave as before.
- [ ] Dark default unchanged pixel-for-pixel for embedders.
- [ ] `NP_THEME_SYSTEM` follows Windows light/dark live (`WM_SETTINGCHANGE`).
- [ ] `.vcxproj.filters` lists every file; no orphan `#pragma comment(lib)` remains.
- [ ] `grep -c '// Close the current block'` = 0; `grep -c '// ===='` = 0.

---

## Decisions I will take unless you say otherwise

1. **Embedded default = Dark**, standalone app = System (backwards compatible).
2. **Add PCH** (reference convention) — moved defines live in `pch.h`.
3. **Do not split** `NotepadPanel.cpp` into multiple translation units yet; the module-layout rule is documented and the file is organised into reference-style sections. Splitting is a separate, higher-risk refactor.
4. **No public API breakage** — theme modes are additive; `SetTheme`/`GetTheme` keep their signatures.
