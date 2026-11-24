// theme.cpp: light and dark, the title bar, and the window's O icons (OrrePort Studio, Windows).
//
// Light / dark follows OrrePort's shared Theme setting (settings.ini "ui_appearance": 0 Follow Windows, 1 Light,
// 2 Dark; the hub's Settings > Appearance), as decided for Studio. The colours start from the project's design sandbox
// (build-pc/mockups/studio-sandbox, 2026-11-17) for dark; the light set is new (exploring: snatcherLeo refines both).
//
// The icons: the hub's own O (port/ui/icons/orreport-icon-rust*.svg) recoloured to sage, #C8452E -> #8FA98A and
// #642217 -> #485445 (Studio's brief, 2026-11-17), drawn with Direct2D's SVG support at each size Windows asks for.
// Dark theme: the full icon from 48 px up, the small glyph below (as the hub does); light theme: the "light O" file.
//
// Platforms: Windows-only (DWM, Direct2D SVG, HICON). Qt would set a QIcon from the same SVGs (QSvgRenderer) and
// follow QStyleHints::colorScheme; AppKit an NSImage and NSApp.effectiveAppearance.
#include "app.hpp"

#include <dwmapi.h>
#include <shlwapi.h>
#include <wincodec.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace studio {

namespace {
D2D1_COLOR_F rgb(uint32_t v, float a = 1.0f) {
    return D2D1::ColorF(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, a);
}

void replace_all(std::string& s, const std::string& a, const std::string& b) {
    for (size_t p = 0; (p = s.find(a, p)) != std::string::npos; p += b.size()) s.replace(p, a.size(), b);
}

// one SVG drawn into a size x size icon (Direct2D's SVG renderer; Windows 10 1703 and later)
HICON svg_icon(ID2D1Factory* factory, IWICImagingFactory* wic, const std::string& svg, int size) {
    IWICBitmap* bmp = nullptr;
    if (FAILED(wic->CreateBitmap(size, size, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &bmp))) return nullptr;
    ID2D1RenderTarget* rt = nullptr;
    const auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
    HICON icon = nullptr;
    if (SUCCEEDED(factory->CreateWicBitmapRenderTarget(bmp, props, &rt))) {
        ID2D1DeviceContext5* dc = nullptr;
        if (SUCCEEDED(rt->QueryInterface(__uuidof(ID2D1DeviceContext5), reinterpret_cast<void**>(&dc)))) {
            IStream* st = SHCreateMemStream(reinterpret_cast<const BYTE*>(svg.data()), (UINT)svg.size());
            ID2D1SvgDocument* doc = nullptr;
            if (st && SUCCEEDED(dc->CreateSvgDocument(st, D2D1::SizeF((float)size, (float)size), &doc))) {
                dc->BeginDraw();
                dc->Clear(D2D1::ColorF(0, 0, 0, 0));
                dc->DrawSvgDocument(doc);
                dc->EndDraw();
                doc->Release();
            }
            if (st) st->Release();
            dc->Release();
        }
        rt->Release();
        // HICON from straight-alpha BGRA (CreateIconIndirect expects unpremultiplied colour)
        std::vector<uint8_t> px((size_t)size * size * 4);
        WICRect r{0, 0, size, size};
        if (SUCCEEDED(bmp->CopyPixels(&r, size * 4, (UINT)px.size(), px.data()))) {
            for (size_t i = 0; i < px.size(); i += 4)
                if (px[i + 3] && px[i + 3] < 255)
                    for (int k = 0; k < 3; ++k) px[i + k] = (uint8_t)std::min(255, px[i + k] * 255 / px[i + 3]);
            BITMAPV5HEADER bi{};
            bi.bV5Size = sizeof bi;
            bi.bV5Width = size;
            bi.bV5Height = -size;
            bi.bV5Planes = 1;
            bi.bV5BitCount = 32;
            bi.bV5Compression = BI_BITFIELDS;
            bi.bV5RedMask = 0x00FF0000;
            bi.bV5GreenMask = 0x0000FF00;
            bi.bV5BlueMask = 0x000000FF;
            bi.bV5AlphaMask = 0xFF000000;
            void* bits = nullptr;
            HDC hdc = GetDC(nullptr);
            HBITMAP color = CreateDIBSection(hdc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
            ReleaseDC(nullptr, hdc);
            if (color) {
                std::memcpy(bits, px.data(), px.size());
                HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
                ICONINFO ii{TRUE, 0, 0, mask, color};
                icon = CreateIconIndirect(&ii);
                DeleteObject(mask);
                DeleteObject(color);
            }
        }
    }
    bmp->Release();
    return icon;
}
}  // namespace

// The root <svg>'s own width / height attributes removed, so the picture fills the size Direct2D is given (its
// viewBox then scales it); the files say 512 or 24 for browsers.
void svg_fit(std::string& svg) {
    const size_t a = svg.find("<svg"), b = a == std::string::npos ? a : svg.find('>', a);
    if (b == std::string::npos) return;
    std::string tag = svg.substr(a, b - a);
    for (const char* attr : {" width=\"", " height=\""}) {
        const size_t p = tag.find(attr);
        if (p == std::string::npos) continue;
        const size_t q = tag.find('"', p + std::strlen(attr));
        if (q != std::string::npos) tag.erase(p, q + 1 - p);
    }
    svg.replace(a, b - a, tag);
}

// a whole file as bytes (_wfopen: MinGW's streams can't open wide paths, and paths may hold any letters)
std::string load_text_file(const std::wstring& path) {
    std::string s;
    if (FILE* f = _wfopen(path.c_str(), L"rb")) {
        char buf[65536];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
        std::fclose(f);
    }
    return s;
}

Palette make_palette(int theme) {
    Palette p;
    p.dark = theme == 2 || (theme != 1 && windows_uses_dark());
    // *Updated 2026-11-18 (v3):* the dark set is the project's sandbox v3 tokens exactly (--bg #181a18, --side #161816, --card
    // #1d201d, --card2 #232723, --hover #252925, --line #2b302b, --line2 #363c36, --tx #e4e8e1, --tx2 #bcc3b7, --mut
    // #8e968a, --faint #5e655b, --acc #9dbb95, --acc2 #b6d0ae, --accbg / --accline the accent at 10% / 42%, --win
    // #141614); the light set is Studio's own in the same spirit (exploring: snatcherLeo refines it)
    if (p.dark) {
        p.bg = rgb(0x181A18); p.panel = rgb(0x161816); p.panel2 = rgb(0x232723); p.line = rgb(0x2B302B);
        p.text = rgb(0xE4E8E1); p.mute = rgb(0x8E968A); p.accent = rgb(0x9DBB95); p.on_accent = rgb(0x13200F);
        p.stage_hi = rgb(0x262B26); p.stage_lo = rgb(0x1A1D1A); p.chip = rgb(0x232723);
        p.win = rgb(0x141614); p.card = rgb(0x1D201D); p.card2 = rgb(0x232723); p.hover = rgb(0x252925);
        p.line2 = rgb(0x363C36); p.text2 = rgb(0xBCC3B7); p.faint = rgb(0x5E655B); p.accent2 = rgb(0xB6D0AE);
        p.accent_bg = rgb(0x9DBB95, 0.10f); p.accent_line = rgb(0x9DBB95, 0.42f);
    } else {
        p.bg = rgb(0xF4F5F1); p.panel = rgb(0xECEEE9); p.panel2 = rgb(0xF0F2ED); p.line = rgb(0xDADFD6);
        p.text = rgb(0x1D221D); p.mute = rgb(0x646C61); p.accent = rgb(0x5E7A59); p.on_accent = rgb(0xFFFFFF);
        p.stage_hi = rgb(0xFFFFFF); p.stage_lo = rgb(0xE6E9E3); p.chip = rgb(0xEEF1EB);
        // (hover *darkened 2026-11-23*, snatcherLeo: "make hover regions in light mode darker, just so it's easier to see ...
        // we can make all hover regions just slightly darker in light mode": #E8EBE4 sat 2% from the rail's #ECEEE9)
        p.win = rgb(0xE5E8E2); p.card = rgb(0xFBFCFA); p.card2 = rgb(0xF0F2ED); p.hover = rgb(0xDCE0D7);
        p.line2 = rgb(0xCBD1C7); p.text2 = rgb(0x3E463D); p.faint = rgb(0x9AA196); p.accent2 = rgb(0x4A6446);
        p.accent_bg = rgb(0x5E7A59, 0.12f); p.accent_line = rgb(0x5E7A59, 0.45f);
    }
    return p;
}

// Windows' app theme: HKCU\...\Themes\Personalize AppsUseLightTheme (0 = dark)
bool windows_uses_dark() {
    DWORD v = 1, n = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme",
                     RRF_RT_REG_DWORD, nullptr, &v, &n) != ERROR_SUCCESS)
        return false;
    return v == 0;
}

void apply_title_bar(HWND hwnd, bool dark) {
    const BOOL on = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &on, sizeof on);
    apply_menu_theme(hwnd, dark);
}

// Windows' own popup menus (the tab list "▾", the tab's right-click menu, Target, the frame rate) in Studio's theme.
// snatcherLeo, 2026-11-22, with screenshots of white menus over the dark app: "For dark mode especialy, looks too bright".
// Windows has no documented switch for dark menus in a Win32 app; File Explorer and Notepad++ use uxtheme.dll's
// unnamed exports: SetPreferredAppMode (ordinal 135: 2 = force dark, 3 = force light), AllowDarkModeForWindow
// (ordinal 133) and FlushMenuThemes (ordinal 136), from Windows 10 1903 (build 18362). Older builds (where 135 was
// AllowDarkModeForApp) are skipped, so their menus stay light. Looked up by ordinal at run time, so a Windows without
// them just keeps light menus. Windows-only; Linux / macOS: the toolkit's menus follow the app's own theme.
void apply_menu_theme(HWND hwnd, bool dark) {
    static HMODULE ux = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    static bool new_enough = [] {
        using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
        OSVERSIONINFOW v{sizeof v};
        const auto get = (RtlGetVersionFn)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
        return get && get(&v) == 0 && (v.dwMajorVersion > 10 || (v.dwMajorVersion == 10 && v.dwBuildNumber >= 18362));
    }();
    if (!ux || !new_enough) return;
    using SetModeFn = int(WINAPI*)(int);
    using AllowFn = BOOL(WINAPI*)(HWND, BOOL);
    using FlushFn = void(WINAPI*)();
    const auto set_mode = (SetModeFn)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    const auto allow = (AllowFn)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(133));
    const auto flush = (FlushFn)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (set_mode) set_mode(dark ? 2 : 3);
    if (allow && hwnd) allow(hwnd, dark ? TRUE : FALSE);
    if (flush) flush();
}

void apply_window_icons(HWND hwnd, const std::wstring& dir, bool dark) {
    static HICON s_small = nullptr, s_big = nullptr;
    ID2D1Factory* factory = nullptr;
    IWICImagingFactory* wic = nullptr;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory))) return;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) {
        factory->Release();
        return;
    }
    auto sage = [&](const wchar_t* file) {
        std::string s = load_text_file(dir + L"\\" + file);
        replace_all(s, "#C8452E", "#8FA98A");
        replace_all(s, "#642217", "#485445");
        svg_fit(s);
        return s;
    };
    const std::string full = dark ? sage(L"orreport-icon-rust.svg") : sage(L"orreport-icon-rust-light-o.svg");
    const std::string small = dark ? sage(L"orreport-icon-small-rust-glyph.svg") : full;
    const UINT dpi = GetDpiForWindow(hwnd);
    const int sm = GetSystemMetricsForDpi(SM_CXSMICON, dpi), bg = GetSystemMetricsForDpi(SM_CXICON, dpi);
    HICON ns = small.empty() ? nullptr : svg_icon(factory, wic, small, sm);
    HICON nb = full.empty() ? nullptr : svg_icon(factory, wic, full, std::max(bg, 48));
    if (ns) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)ns);
    if (nb) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)nb);
    if (ns && s_small) DestroyIcon(s_small);
    if (nb && s_big) DestroyIcon(s_big);
    if (ns) s_small = ns;
    if (nb) s_big = nb;
    wic->Release();
    factory->Release();
    logf(Level::Detail, "theme", "icons %s (%d / %d px)", dark ? "dark" : "light", sm, bg);
}

}  // namespace studio
