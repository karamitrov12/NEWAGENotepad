#pragma once

#include <windows.h>

// ---- Theme modes ----

/// Selects how a component picks its palette. System is resolved against the
/// current user preference each time a palette is requested.
enum class ThemeMode
{
    System = 0,
    Light = 1,
    Dark = 2
};

// ---- Palette ----

/// Colours used by the panel chrome and by the host frame.
/// Values are COLORREF (0x00BBGGRR). A component stores a copy of this struct;
/// no GDI object is owned by or returned from the theme module.
struct ThemePalette
{
    COLORREF BgApp;      // editor surface and window background
    COLORREF BgPanel;    // tab strip, toolbar host, status bar
    COLORREF BgCard;     // active tab and inline input background
    COLORREF BgBtn;      // chip button face
    COLORREF BgBtnHi;    // pressed chip button face
    COLORREF BgRailHi;   // hovered tab or menu row
    COLORREF Border;     // strong border and focused outline
    COLORREF BorderSoft; // separators inside the chrome
    COLORREF TextHi;     // primary text and active icons
    COLORREF TextMid;    // tab labels and idle icons
    COLORREF TextLow;    // disabled text and placeholders
    COLORREF Accent;     // active tab underline and checks
    COLORREF SelBg;      // editor selection
    COLORREF IconIdle;   // toolbar icon at rest
    COLORREF IconActive; // toolbar icon under the pointer
};

/// Built-in palettes. Dark keeps the values the panel shipped with.
ThemePalette DarkPalette() noexcept;
ThemePalette LightPalette() noexcept;

/// Returns the palette for a concrete decision or for the requested mode.
/// ThemeMode::System performs a registry read; prefer the bool overload when the
/// caller already resolved the mode.
ThemePalette GetPalette(bool dark) noexcept;
ThemePalette GetPalette(ThemeMode mode) noexcept;

// ---- System preference ----

/// Reads HKCU\...\Themes\Personalize\AppsUseLightTheme.
/// Returns false when the value is missing or unreadable.
bool IsSystemDarkMode() noexcept;

/// Collapses a mode to a light/dark decision. Only System touches the registry.
bool ResolveDarkMode(ThemeMode mode) noexcept;

// ---- Dpi ----

/// Converts a 96-DPI metric to the given DPI. dpi values below 96 are honoured
/// as-is, so a caller that failed to query the monitor still gets a usable size.
int ScaleMetric(int value, int dpi) noexcept;

// ---- Win32 integration ----

/// Applies the immersive dark caption and, on Windows 11, the caption colours.
/// Safe to call before the window is shown; failures are ignored.
/// Only top-level frames call this: an embedded panel leaves the host frame alone.
void ApplyTitleBarTheme(HWND hwnd, bool dark) noexcept;

/// Applies the Explorer or DarkMode_Explorer visual style to a child control,
/// which is what gives Rich Edit and the inline editor matching scrollbars.
void ApplyControlTheme(HWND hwnd, bool dark) noexcept;
