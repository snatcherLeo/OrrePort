/*
 * pc_ui_shaping.h: OrrePort's own pages (the hub and the Settings overlay, RmlUi) drawn with HarfBuzz shaping and the
 * Unicode bidi algorithm (the project's build helper, 2026-11-16; approved by snatcherLeo the same day, the decision log "HarfBuzz and a
 * bidi library, added now"; docs/port/font-renderer.md "Shaping in the hub").
 *
 * Why: RmlUi's built-in font engine draws one glyph per character, left to right, so Arabic came out unjoined and
 * backwards, Indic, Thai and Khmer marks didn't join or move, and an emoji with a skin tone showed as two pictures. RmlUi
 * ships a HarfBuzz font engine as a sample (Samples/basic/harfbuzz); OrrePort installs it, with its own copy of the
 * per-size face file (FontFaceHandleHarfBuzz.cpp in this folder) that adds the bidi order (SheenBidi) and a font per
 * run from the Noto fallback chain (pc_font_files.h), before RmlUi starts.
 *
 * Studio reuses this folder unchanged when it exists: an RmlUi text box there gets shaped text and the same fallback
 * fonts from bin\Fonts\ (font-renderer.md).
 *
 * Platforms: plain C++ over RmlUi, HarfBuzz, SheenBidi and FreeType; the same on Windows, Linux and macOS (likely).
 */
#ifndef PC_UI_SHAPING_H
#define PC_UI_SHAPING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Installs the shaping font engine as RmlUi's font engine. Call once before RmlUi starts (Rml::Initialise, inside
 * aurora_initialize). ORRE_UI_SHAPING=0 (a test switch, never a setting) leaves RmlUi's own engine in for one run: the
 * before / after pictures. */
void pc_ui_font_engine_install(void);

/* 1 when the pages' text is shaped by the engine above (so names go to the page as typed), 0 with RmlUi's own engine. */
int pc_ui_text_shaped(void);

#ifdef __cplusplus
}
#endif

#endif
