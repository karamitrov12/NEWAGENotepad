// Theme/Theme.cpp
#include "pch.h"

#include "Theme.h"

#include <dwmapi.h>
#include <uxtheme.h>

// ---- Palette ----

ThemePalette DarkPalette() noexcept
{
    ThemePalette palette = {};
    palette.BgApp = RGB(22, 22, 25);
    palette.BgPanel = RGB(30, 30, 34);
    palette.BgCard = RGB(38, 38, 43);
    palette.BgBtn = RGB(48, 48, 54);
    palette.BgBtnHi = RGB(60, 60, 66);
    palette.BgRailHi = RGB(52, 52, 58);
    palette.Border = RGB(56, 56, 62);
    palette.BorderSoft = RGB(44, 44, 48);
    palette.TextHi = RGB(232, 232, 236);
    palette.TextMid = RGB(160, 160, 168);
    palette.TextLow = RGB(108, 108, 116);
    palette.Accent = RGB(96, 165, 250);
    palette.SelBg = RGB(48, 70, 100);
    palette.IconIdle = RGB(200, 200, 208);
    palette.IconActive = RGB(255, 255, 255);
    return palette;
}

ThemePalette LightPalette() noexcept
{
    ThemePalette palette = {};
    palette.BgApp = RGB(255, 255, 255);
    palette.BgPanel = RGB(248, 248, 248);
    palette.BgCard = RGB(238, 238, 238);
    palette.BgBtn = RGB(228, 228, 230);
    palette.BgBtnHi = RGB(218, 218, 222);
    palette.BgRailHi = RGB(232, 232, 236);
    palette.Border = RGB(200, 200, 204);
    palette.BorderSoft = RGB(220, 220, 224);
    palette.TextHi = RGB(28, 28, 32);
    palette.TextMid = RGB(100, 100, 108);
    palette.TextLow = RGB(150, 150, 158);
    palette.Accent = RGB(37, 99, 235);
    palette.SelBg = RGB(206, 224, 255);
    palette.IconIdle = RGB(60, 60, 68);
    palette.IconActive = RGB(20, 20, 28);
    return palette;
}

ThemePalette GetPalette(bool dark) noexcept
{
    return dark ? DarkPalette() : LightPalette();
}

ThemePalette GetPalette(ThemeMode mode) noexcept
{
    return GetPalette(ResolveDarkMode(mode));
}

// ---- System preference ----

bool IsSystemDarkMode() noexcept
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                      KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    {
        return false;
    }

    DWORD appsUseLightTheme = 1;
    DWORD size = sizeof(appsUseLightTheme);
    DWORD type = 0;
    const LSTATUS status = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type,
                                            reinterpret_cast<LPBYTE>(&appsUseLightTheme), &size);
    RegCloseKey(key);

    if (status != ERROR_SUCCESS || type != REG_DWORD)
    {
        return false;
    }
    return appsUseLightTheme == 0;
}

bool ResolveDarkMode(ThemeMode mode) noexcept
{
    switch (mode)
    {
    case ThemeMode::Light:
        return false;
    case ThemeMode::Dark:
        return true;
    case ThemeMode::System:
    default:
        return IsSystemDarkMode();
    }
}

// ---- Dpi ----

int ScaleMetric(int value, int dpi) noexcept
{
    if (dpi <= 0)
    {
        dpi = 96;
    }
    return MulDiv(value, dpi, 96);
}

// ---- DWM integration ----

/// Attribute 20 is DWMWA_USE_IMMERSIVE_DARK_MODE from Windows 10 2004 onwards;
/// 19 is the pre-release value still used by earlier builds.
static void SetDarkCaption(HWND hwnd, BOOL dark) noexcept
{
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
    {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
}

/// Caption colour attributes only exist on Windows 11; older builds return
/// E_INVALIDARG and keep the system caption colours.
static void SetCaptionColors(HWND hwnd, bool dark) noexcept
{
    const ThemePalette palette = GetPalette(dark);
    const COLORREF caption = palette.BgApp;
    const COLORREF text = palette.TextHi;
    const COLORREF border = palette.Border;

    DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, 36, &text, sizeof(text));
    DwmSetWindowAttribute(hwnd, 34, &border, sizeof(border));
}

void ApplyTitleBarTheme(HWND hwnd, bool dark) noexcept
{
    if (!hwnd)
    {
        return;
    }
    SetDarkCaption(hwnd, dark ? TRUE : FALSE);
    SetCaptionColors(hwnd, dark);
}

void ApplyControlTheme(HWND hwnd, bool dark) noexcept
{
    if (!hwnd)
    {
        return;
    }
    SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
}
