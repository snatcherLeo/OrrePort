/*
 * pc_ui_shaping.cpp: installs the HarfBuzz font engine for OrrePort's RmlUi pages (pc_ui_shaping.h says why). the project's
 * build helper, 2026-11-16.
 *
 * How: RmlUi takes its font engine from Rml::SetFontEngineInterface, read once by Rml::Initialise (RmlUi's Core.cpp:
 * a font engine set before then is used instead of its built-in FreeType one). Aurora calls Rml::Initialise inside
 * aurora_initialize (extern/aurora/lib/rmlui.cpp), so pc_platform.c calls pc_ui_font_engine_install just before it: no
 * Aurora change. The engine (RmlUi's HarfBuzz sample, with OrrePort's FontFaceHandleHarfBuzz.cpp) then answers every
 * Rml::LoadFontFace, so pc_ui_load_fonts' Inter, Saira and the Noto fallback chain load into it unchanged.
 *
 * Platforms: plain C++; the same on Windows, Linux and macOS (likely).
 */
#include "pc_ui_shaping.h"

#include "FontEngineInterfaceHarfBuzz.h" /* RmlUi's sample engine (port/CMakeLists.txt adds its folder) */

#include <RmlUi/Core/Core.h>
#include <SDL3/SDL.h>
#include <hb.h>

#include <cstdio>

namespace {
int s_shaped = 0;
}

extern "C" void pc_ui_font_engine_install(void) {
    static bool s_done = false;
    if (s_done) return;
    s_done = true;
    const char* off = SDL_getenv("ORRE_UI_SHAPING"); /* a test switch, never a setting */
    if (off != nullptr && off[0] == '0') {
        std::printf("[ui] ORRE_UI_SHAPING=0: RmlUi's own font engine (no shaping, no bidi)\n");
        return;
    }
    /* never freed: RmlUi keeps the pointer until its Shutdown, which runs at exit; deleting it from a static destructor
     * could run after RmlUi's own globals are gone */
    static FontEngineInterfaceHarfBuzz* s_engine = new FontEngineInterfaceHarfBuzz();
    Rml::SetFontEngineInterface(s_engine);
    s_shaped = 1;
    std::printf("[ui] text shaping on: HarfBuzz %s, SheenBidi, RmlUi's HarfBuzz font engine\n", HB_VERSION_STRING);
}

extern "C" int pc_ui_text_shaped(void) { return s_shaped; }
