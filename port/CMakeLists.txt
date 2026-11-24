# OrrePort: the native PC build of Pokémon Colosseum (platform-layer plan: docs/port/platform-layer-plan.md).
#
# Completely separate from the GameCube matching build (configure.py / ninja / build/): it has its own
# CMake project in port/, its own build folder (build-pc/, git-ignored), and never touches build/ or
# config/GC6E01. Toolchain: MSYS2 mingw64 GCC (decision 4), CMake 4.4.3 and Ninja (docs/port/setup.md).
#
#   cmake -S port -B build-pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
#   cmake --build build-pc
#
# Stage 1: a window from Aurora, proving the toolchain and the prebuilt Dawn / SDL3. Stage 3 groundwork (now):
# GameCube memory at 0x80000000, GS threads on host fibres, disc access through nod (AURORA_ENABLE_DVD=ON).
# Next stages add the decompiled game code (src/game, src/hsd, src/musyx) with TARGET_PC.
cmake_minimum_required(VERSION 3.25)
project(OrrePort C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# RmlUi on by default (2026-11-24): the hub, the settings overlay and the in-game menus are RmlUi pages, so OrrePort
# doesn't link without it (found by the public export's first clean build: Aurora's own default is OFF, and this
# checkout's build folder had it switched on by hand since the RmlUi trial). A cache entry set here before Aurora's
# option() keeps any value given on the command line. All platforms the same.
set(AURORA_ENABLE_RMLUI ON CACHE BOOL "OrrePort's hub and menus are RmlUi pages (needed to link)")
# Aurora: vendored at extern/aurora (decision 2; version in docs/port/setup.md)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora ${CMAKE_BINARY_DIR}/aurora EXCLUDE_FROM_ALL)

# Text shaping and the Unicode bidi algorithm (approved by snatcherLeo 2026-11-16, the decision log "HarfBuzz and a bidi library,
# added now"; docs/port/font-renderer.md "Shaping"). Why: Indic, Thai and Khmer letters must join (conjuncts, vowel
# signs moved and stacked), Arabic must join and read from the right, and emoji sequences (a skin tone, a family) must
# become one picture. That is a shaping engine's job (HarfBuzz) plus the bidi algorithm for the order of mixed runs
# (SheenBidi, chosen over FriBidi: it builds as a small static library, so no LGPL DLL ships). Used by the hub's RmlUi
# pages (port/src/hub_text/) and by the game's names (pc_shape.cpp, drawn by part D), and later by Studio.
# How: both fetched from their official GitHub release at a pinned version, checked by SHA-256 (the same rule as
# Aurora's dependencies), and built here from each library's own one-file ("unity") source as a static library, so
# nothing new ships next to the exe. We don't use their own CMake files: HarfBuzz's builds a shared library when
# BUILD_SHARED_LIBS is on (Aurora turns that on for Windows), and both pull in options we don't need.
# Platforms: the same sources on Windows, Linux and macOS (likely: both are plain C / C++ with no OS calls in the parts
# used; HarfBuzz's Windows-only DirectWrite / GDI / Uniscribe and macOS-only CoreText backends stay off, they are only
# compiled when their HAVE_* switch is set). A Linux distribution build could use the system's libharfbuzz instead
# (same API); macOS has no system HarfBuzz, so the fetched one is used there too.
include(FetchContent)
# HarfBuzz 15.0.0 (2026-10-22), Old MIT licence (COPYING), the release's own tarball; its SHA-256 matches the digest
# GitHub lists for the asset (checked 2026-11-16). 15.0.0 rather than 15.1.0: the newest release that had been out for
# more than two weeks.
FetchContent_Declare(orre_harfbuzz
  URL https://github.com/harfbuzz/harfbuzz/releases/download/15.0.0/harfbuzz-15.0.0.tar.xz
  URL_HASH SHA256=968536e9abd782f07f7954044c0c4020199fcdd39b41d221ec42d2a3be4a44df
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  SOURCE_SUBDIR orre_not_a_cmake_dir # download only: no CMakeLists.txt there, so MakeAvailable adds no subdirectory
)
# SheenBidi 3.0.0 (2026-01-24), Apache License 2.0 (LICENSE), GitHub's archive of the v3.0.0 tag.
FetchContent_Declare(orre_sheenbidi
  URL https://github.com/Tehreer/SheenBidi/archive/refs/tags/v3.0.0.tar.gz
  URL_HASH SHA256=86c56014034739ba39a24c23eb00323b0bf6f737354f665786015fca842af786
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  SOURCE_SUBDIR orre_not_a_cmake_dir
)
FetchContent_MakeAvailable(orre_harfbuzz orre_sheenbidi)
# HarfBuzz from src/harfbuzz.cc (its amalgamated source, which includes every other file). HAVE_FREETYPE adds hb-ft,
# which the hub's RmlUi font engine uses to read glyph tables through FreeType faces it already has open (FreeType is
# Aurora's, built static). The game's names use HarfBuzz's own font loading (pc_shape.cpp), no FreeType needed there.
add_library(orre_harfbuzz STATIC ${orre_harfbuzz_SOURCE_DIR}/src/harfbuzz.cc)
target_include_directories(orre_harfbuzz PUBLIC ${orre_harfbuzz_SOURCE_DIR}/src)
target_compile_definitions(orre_harfbuzz PRIVATE HAVE_FREETYPE=1)
target_link_libraries(orre_harfbuzz PUBLIC Freetype::Freetype)
# SheenBidi from Source/SheenBidi.c, its own unity file (SB_CONFIG_UNITY), static: SB_PUBLIC stays empty, so no
# dllimport / dllexport on Windows.
add_library(orre_sheenbidi STATIC ${orre_sheenbidi_SOURCE_DIR}/Source/SheenBidi.c)
target_compile_definitions(orre_sheenbidi PRIVATE SB_CONFIG_UNITY)
target_include_directories(orre_sheenbidi PUBLIC ${orre_sheenbidi_SOURCE_DIR}/Headers
                           PRIVATE ${orre_sheenbidi_SOURCE_DIR}/Source)
# FreeType keeps FT_DISABLE_HARFBUZZ ON (extern/aurora/extern/CMakeLists.txt): FreeType would use HarfBuzz only for its
# auto-hinter's coverage of OpenType features (small caps and the like), not for shaping, and turning it on makes the
# two libraries depend on each other (FreeType needs HarfBuzz built first, HarfBuzz needs FreeType). OrrePort draws the
# hub's text and part D's letters with the fonts' own hinting or none, so nothing would change on screen (likely).

# OrrePort's own platform layer (src/pc in the plan; here port/src): GameCube memory at 0x80000000, the low-memory
# allocator, fibres and the CPU-register stubs. One file per operating system behind each interface, so
# Windows-only code stays in the *_win32 files.
if (WIN32)
  set(ORRE_PLATFORM_OS_SOURCES src/pc_mem_win32.c src/pc_fibre_win32.c src/pc_fibre_switch_win64.S src/pc_crash_win32.c)
else ()
  set(ORRE_PLATFORM_OS_SOURCES src/pc_mem_posix.c src/pc_fibre_posix.c src/pc_crash_posix.c)
endif ()
add_library(orre_platform STATIC ${ORRE_PLATFORM_OS_SOURCES} src/pc_lowmem.c src/pc_cpu.c src/pc_ptr32.c
            src/pc_os_sdk.c src/pc_os_misc.c src/pc_gba_stub.c src/pc_mtx_ps.c src/pc_mtx_ps_exact.c src/pc_rel.c)
target_include_directories(orre_platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include)
target_compile_definitions(orre_platform PRIVATE TARGET_PC=1)
target_include_directories(orre_platform PUBLIC src)
# SDL3 (Aurora's) for the interrupt lock; aurora::mtx for the C math the PS forwarders call
target_link_libraries(orre_platform PUBLIC ${AURORA_SDL3_TARGET} aurora::mtx)

# The frame boundary (VIWaitForRetrace and the rest of VI that Aurora leaves out), the alarms it runs and the SDK
# threads that run while it waits (docs/port/sdk-threads.md): they need
# Aurora's frame calls and clock (Aurora's core), so they are apart from orre_platform, which the audio tests link
# without a window
add_library(orre_platform_vi STATIC src/pc_vi.c src/pc_renderdoc.c src/pc_os_alarm.c src/pc_os_thread.c src/pc_os_sync.c src/pc_gx_hooks.c src/pc_thp_draw.c src/pc_frame_dump.c src/pc_pad_script.c src/pc_pad_record.c src/pc_frame_times.c src/pc_save_fields.c
            src/pc_platform.c src/pc_debug.c src/pc_debug_ui.cpp src/pc_log.c src/pc_heap.c src/pc_settings.c src/pc_textures.cpp
            src/pc_paths.c # where every file lives: the portable and flat layouts in one table (4.1, 2026-11-21; pc_paths.h)
            src/pc_menu_bar.cpp src/pc_options.c src/pc_arq.c src/pc_event_log.c src/pc_controls.c
            src/pc_health.c # the app's own health: hang watch, performance log, memory (snatcherLeo, 2026-11-13)
            src/pc_text.cpp # every word from ui/lang/<code>.txt, for translators (snatcherLeo, 2026-11-13; pc_text.h)
            src/pc_cheats.c # the Cheats menu's switches and the game hooks they set (snatcherLeo, 2026-11-13; pc_cheats.h)
            src/pc_speed.c # Settings > Enhancements > Speed: game, battle and sound speed (snatcherLeo, 2026-11-13; pc_speed.h)
            src/pc_smooth.c # frame smoothing's port side: switches, rate, step stamps, [smooth] log (2026-11-16)
            src/pc_layout.c # widescreen groundwork: the UI-per-ratio layout table's reader (2026-11-19; pc_layout.h)
            src/pc_ui_inventory.c) # widescreen groundwork: the 2D inventory test switch, ORRE_UI_INVENTORY (2026-11-19)
# the debug toolkit's menu draws with Aurora's ImGui (pc_debug_ui.cpp; ORRE_DEBUG=1 only)
target_link_libraries(orre_platform_vi PUBLIC imgui)
# the RmlUi trial's Settings window (docs/port/rmlui-trial.md): only with Aurora's RmlUi switched on; RmlUi's headers
# and libraries come through aurora::core
if (AURORA_ENABLE_RMLUI)
  target_sources(orre_platform_vi PRIVATE src/pc_settings_overlay.cpp src/pc_report.cpp src/pc_window_icon.cpp
                 src/pc_hub.cpp src/pc_svg.cpp src/pc_controls_ui.cpp src/pc_pad_shape.cpp src/pc_disc_label.cpp
                 src/pc_hub_saves.cpp # the hub's Saves page (step 2.1; pc_hub_saves.h)
                 src/pc_hub_langdiscs.cpp) # the hub's Language discs (2.2 step 8, piece 2; pc_hub_langdiscs.h)
  target_compile_definitions(orre_platform_vi PRIVATE ORRE_SETTINGS_OVERLAY=1)
  # The hub's and the Settings overlay's text shaped by HarfBuzz and ordered by SheenBidi (2026-11-16; hub_text/
  # pc_ui_shaping.cpp says how). RmlUi ships a HarfBuzz font engine as a sample (Samples/basic/harfbuzz, "until the
  # HarfBuzz font engine is merged into Core"), not in its library; its files are compiled here from RmlUi's own
  # fetched source, unchanged, except FontFaceHandleHarfBuzz.cpp, OrrePort's copy in hub_text/ (bidi runs and a font
  # per run, so a name with Latin, Arabic and an emoji shapes each run with the font that has it). The sample uses
  # RmlUi's private FreeType helpers (Source/Core), which are in the static RmlUi library already. Platforms: all.
  FetchContent_GetProperties(rmlui)
  set(ORRE_RMLUI_HB ${rmlui_SOURCE_DIR}/Samples/basic/harfbuzz/src)
  target_sources(orre_platform_vi PRIVATE src/hub_text/pc_ui_shaping.cpp src/hub_text/FontFaceHandleHarfBuzz.cpp
                 ${ORRE_RMLUI_HB}/FontEngineInterfaceHarfBuzz.cpp ${ORRE_RMLUI_HB}/FontFace.cpp
                 ${ORRE_RMLUI_HB}/FontFaceLayer.cpp ${ORRE_RMLUI_HB}/FontFamily.cpp ${ORRE_RMLUI_HB}/FontProvider.cpp
                 ${ORRE_RMLUI_HB}/FreeTypeInterface.cpp ${ORRE_RMLUI_HB}/TextureLayout.cpp
                 ${ORRE_RMLUI_HB}/TextureLayoutRectangle.cpp ${ORRE_RMLUI_HB}/TextureLayoutRow.cpp
                 ${ORRE_RMLUI_HB}/TextureLayoutTexture.cpp)
  target_include_directories(orre_platform_vi PRIVATE ${ORRE_RMLUI_HB} ${rmlui_SOURCE_DIR}/Source/Core)
  # The disc art page's picture formats (snatcherLeo, 2026-11-13: "approve stb_image and libwebp"; docs/licensing.md):
  # stb_image.h (v2.32, public domain / MIT: JPG, GIF, TGA, PSD, BMP) and libwebp's decoder (1.5.0, BSD-3, built here
  # as a static library from its release, so no DLL ships; its licence travels in licenses/, firstlink.py).
  # *Changed 2026-11-24 (snatcherLeo, option C):* both are vendored in extern/, unchanged, and required. They used to be read
  # from decomp/_external/ beside the repo and left out silently when missing (the page then read PNG and BMP only),
  # so a clean checkout of the public repo would have built a quietly smaller program. Platforms: the same sources build
  # on Linux and macOS (likely).
  set(ORRE_STB ${CMAKE_CURRENT_SOURCE_DIR}/../extern/stb)
  set(ORRE_WEBP ${CMAKE_CURRENT_SOURCE_DIR}/../extern/libwebp-1.5.0)
  if (NOT EXISTS ${ORRE_STB}/stb_image.h OR NOT EXISTS ${ORRE_WEBP}/src/webp/decode.h)
    message(FATAL_ERROR "extern/stb or extern/libwebp-1.5.0 is missing (the disc art page needs both)")
  endif ()
  target_include_directories(orre_platform_vi PRIVATE ${ORRE_STB})
  target_compile_definitions(orre_platform_vi PRIVATE ORRE_HAVE_STB_IMAGE=1)
  # libwebp's own CMake, the decoder library only (tools, muxer, extras and the JavaScript build off), as the
  # decomp/_external/libwebp build was made
  foreach (_opt WEBP_BUILD_ANIM_UTILS WEBP_BUILD_CWEBP WEBP_BUILD_DWEBP WEBP_BUILD_GIF2WEBP WEBP_BUILD_IMG2WEBP
           WEBP_BUILD_VWEBP WEBP_BUILD_WEBPINFO WEBP_BUILD_WEBPMUX WEBP_BUILD_EXTRAS WEBP_BUILD_WEBP_JS
           WEBP_BUILD_LIBWEBPMUX WEBP_BUILD_FUZZTEST)
    set(${_opt} OFF CACHE BOOL "" FORCE)
  endforeach ()
  add_subdirectory(${ORRE_WEBP} ${CMAKE_BINARY_DIR}/libwebp EXCLUDE_FROM_ALL)
  target_include_directories(orre_platform_vi PRIVATE ${ORRE_WEBP}/src)
  target_compile_definitions(orre_platform_vi PRIVATE ORRE_HAVE_WEBP=1)
  # PUBLIC: firstlink.py links the game from pc_audio_test's library list, which carries orre_platform_vi's
  target_link_libraries(orre_platform_vi PUBLIC webpdecoder)
endif ()

# The updater both programs share (pc_update.c, 2026-11-23; docs/port/updater-plan.md) and the release signing tool.
# Monocypher 4.0.3 (BSD-2 / CC-0; snatcherLeo approved it 2026-11-23) for the Ed25519 signature and SHA-512, vendored in
# extern/ unchanged (*changed 2026-11-24*, snatcherLeo's option C; it was read from decomp/_external/ and the updater was left
# out silently without it); zlib (MSYS2's, already shipped as libzlib1.dll) unpacks the release zip; WinHTTP (part of
# Windows) fetches it. Studio compiles pc_update.c the same way. Platforms: WinHTTP is Windows-only (Linux / macOS:
# libcurl, not written yet; pc_update.c's fallback then offers no updates).
set(ORRE_MONOCYPHER ${CMAKE_CURRENT_SOURCE_DIR}/../extern/monocypher-4.0.3/src)
if (NOT EXISTS ${ORRE_MONOCYPHER}/monocypher.c)
  message(FATAL_ERROR "extern/monocypher-4.0.3 is missing (the updater needs it)")
endif ()
if (TRUE)
  add_library(orre_update STATIC src/pc_update.c src/pc_zip.c ${ORRE_MONOCYPHER}/monocypher.c
              ${ORRE_MONOCYPHER}/optional/monocypher-ed25519.c)
  target_include_directories(orre_update PUBLIC src ${ORRE_MONOCYPHER} ${ORRE_MONOCYPHER}/optional)
  find_package(ZLIB REQUIRED)
  target_link_libraries(orre_update PUBLIC ZLIB::ZLIB)
  if (WIN32)
    target_link_libraries(orre_update PUBLIC winhttp)
  endif ()
  target_link_libraries(orre_platform_vi PUBLIC orre_update)
  # the version both programs show and the updater compares (the repo's VERSION file, one number for both programs)
  file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION ORRE_VERSION_LINE LIMIT_COUNT 1)
  target_compile_definitions(orre_platform_vi PRIVATE ORRE_VERSION="${ORRE_VERSION_LINE}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION)
  add_executable(orre_sign tools/orre_sign.c ${ORRE_MONOCYPHER}/monocypher.c ${ORRE_MONOCYPHER}/optional/monocypher-ed25519.c)
  target_include_directories(orre_sign PRIVATE ${ORRE_MONOCYPHER} ${ORRE_MONOCYPHER}/optional)
  if (WIN32)
    target_link_libraries(orre_sign PRIVATE bcrypt)
  endif ()
  if (MINGW)
    target_link_options(orre_sign PRIVATE -static-libgcc -static)
  endif ()
  # the module alone against a local test release (port/test/pc_update_test.c says how)
  add_executable(pc_update_test test/pc_update_test.c)
  target_link_libraries(pc_update_test PRIVATE orre_update)
endif ()
target_include_directories(orre_platform_vi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include)
target_compile_definitions(orre_platform_vi PRIVATE TARGET_PC=1)
target_link_libraries(orre_platform_vi PUBLIC orre_platform aurora::core aurora::gx aurora::vi aurora::os)
# the controller (pad.cpp) and its serial-interface layer: PADRead etc., and PADSetVirtualStatus for ORRE_PAD_SCRIPT
if (TARGET aurora::pad)
  target_link_libraries(orre_platform_vi PUBLIC aurora::pad)
endif ()
if (TARGET aurora::si)
  target_link_libraries(orre_platform_vi PUBLIC aurora::si)
endif ()
if (TARGET aurora::dvd)
  # DVD completion callbacks on the game thread (Aurora's dispatcher hook)
  target_sources(orre_platform_vi PRIVATE src/pc_dvd.c src/pc_disc_check.c src/pc_disc_manifest_none.c)
  target_link_libraries(orre_platform_vi PUBLIC aurora::dvd)
  target_compile_definitions(orre_platform_vi PUBLIC PC_HAVE_AURORA_DVD=1)
endif ()
if (TARGET aurora::card)
  # memory card setup at startup (the game's CARDInit(void) vs Aurora's CARDInit(game, maker))
  target_sources(orre_platform_vi PRIVATE src/pc_card.c src/pc_card_timing.c
                 src/pc_saves.c # the save slots, previous versions and importing (step 2.1; pc_saves.h)
                 src/pc_save_cipher.c # SHA-1 and the save slot's cipher both ways (3.5: the GameCube copy re-encrypts)
                 src/pc_save_region.c # a save's region stamp and the import's game-code change (3.5 steps 6-7)
                 src/pc_gcopy.c # "Make a GameCube copy...": the checklist and the copy (foundation; pc_gcopy.h)
                 src/pc_gcopy_region.c # the copy's bytes for another region (3.5 step 9; plain C, tested alone)
                 # (src/pc_zip.c, the stored zips, moved into orre_update above, 2026-11-23: the updater uses it too)
                 src/pc_scrub.c # personal details out of report texts (pc_scrub.h; no SDL, Studio shares it)
                 src/pc_clip_file.c # the saved report on the clipboard as a file (pc_clip_file.h; Studio shares it)
                 src/pc_apps.c) # the hub and Studio find, show, start and close each other (pc_apps.h; Studio shares it)
  target_link_libraries(orre_platform_vi PUBLIC aurora::card)
  target_compile_definitions(orre_platform_vi PUBLIC PC_HAVE_AURORA_CARD=1)
endif ()
# Settings > Controls' "GameCube adapter" row: finds Nintendo's adapter on the wrong driver and starts "OrrePort Adapter
# Setup.exe" (2026-11-13; built by tools/local/port/build_adapter_setup.py). Windows only does anything (SetupAPI
# to look, shell32 to start it); on Linux and macOS the file compiles to "nothing to set up" until their own way exists.
target_sources(orre_platform_vi PRIVATE src/pc_adapter_setup.c)
# The font renderer (step 2.2 part A, docs/port/font-renderer.md): FreeType, already built for Dear ImGui by Aurora.
# Platform-neutral code; where its fonts come from is per system (the file says which).
target_sources(orre_platform_vi PRIVATE src/pc_font_render.cpp)
# The shipped fonts' folder (Fonts/ next to the exe), the Noto last-resort chain, the CJK region rule and the stand-in
# codes for characters past U+FFFF, shared by the game's renderer and OrrePort's own pages (the project's helper, 2026-11-16).
# Platform-neutral C++ over SDL.
target_sources(orre_platform_vi PRIVATE src/pc_font_files.cpp)
# Right-to-left names (Arabic joining, Hebrew precomposed letters, a simple bidi pass) at draw time, shared by the game's
# text, the naming field and the hub's pages, with no shaping library (the project's helper, 2026-11-16; pc_rtl.h). Pure C++,
# the same on every OS.
target_sources(orre_platform_vi PRIVATE src/pc_rtl.cpp)
# Names shaped by HarfBuzz with the bidi order from SheenBidi (2026-11-16; pc_shape.h): a name with Indic, Thai, Khmer,
# Arabic, Hebrew or an emoji sequence becomes shaped clusters, each one letter in the game's text, drawn by part D from
# its shaped glyphs. pc_rtl.cpp stays as the fallback. Pure C++ over the two libraries, the same on every OS.
target_sources(orre_platform_vi PRIVATE src/pc_shape.cpp)
target_link_libraries(orre_platform_vi PUBLIC orre_harfbuzz orre_sheenbidi)
# Visible letters (Unicode's extended grapheme clusters, UAX #29) for the 12-letter name limit, Backspace and the hub's
# name cut (snatcherLeo, 2026-11-16: "the 12-letter limit counts visible letters"; pc_grapheme.h). Pure C++ over a table
# generated from Unicode's data (tools/local/gen_grapheme_table.py), no library; the same on every OS.
target_sources(orre_platform_vi PRIVATE src/pc_grapheme.cpp)
# The font renderer's part B (2026-11-13): the game's letters traced sharp at the drawing resolution, in a
# high-resolution copy of the text atlas that Aurora draws instead (aurora_orre_hires_texture_register). Platform-
# neutral (standard C++ threads; Aurora's WebGPU upload).
target_sources(orre_platform_vi PRIVATE src/pc_text_atlas.cpp)
# The language switch for the game's own messages (2.2 step 4, 2026-11-13; docs/port/translation.md): the text for
# each message key in the chosen language, through OrreHook_MessageText. Platform-neutral C++.
target_sources(orre_platform_vi PRIVATE src/pc_game_text.cpp)
# The European languages' text from the player's own European disc in ROM Other Regions\ (2.2 step 8, piece 1; Main
# 2026-11-15): read with nod, which aurora::dvd brings (without it the file compiles to "no language discs").
# Platform-neutral C++.
target_sources(orre_platform_vi PRIVATE src/pc_region_text.cpp)
# The word pictures per language (language piece 4, 2026-11-15; docs/port/word-pictures.md): the menu pictures
# with words swapped for the language disc's own, through three hook points (sprite record, texture lookup, item size).
# Platform-neutral C++ (the discs are read by pc_region_text.cpp's nod handles).
target_sources(orre_platform_vi PRIVATE src/pc_word_pictures.cpp)
# The Japanese release look's one table of regional differences (3.5 steps 2 and 3, 2026-11-19; docs/port/
# word-pictures.md "The Japanese release look"): reads ui/regions/jp_look.txt and tells the word pictures and the
# language files which pictures to swap in Japanese with the Japanese disc. Platform-neutral C++.
target_sources(orre_platform_vi PRIVATE src/pc_region_look.cpp)
# Files from the language discs (language piece 5, 2026-11-15; docs/port/sequences-per-language.md): the credits
# movie, the title demo and Battle Mode's round titles served from the language's own disc through Aurora's overlay
# files (aurora/dvd.h). Platform-neutral C++ (nod reads the discs; without aurora::dvd it compiles to "nothing swapped").
target_sources(orre_platform_vi PRIVATE src/pc_lang_files.cpp)
# The Language row on the game's Options screen (2026-11-13, mockup 78): the game's rows moved, OrrePort's row
# added past the menu tables' ends, its label drawn in the game's style. Platform-neutral C++.
target_sources(orre_platform_vi PRIVATE src/pc_options_lang.cpp)
# The main menu's LOAD SAVE button and the Continue question's YES-first tweak (2.2 step 6, 2026-11-14; plan:
# docs/port/title-load-save.md): pictures made from the game's own Quit picture, entries past the menu table's end.
# Platform-neutral C++ (no OS calls: Windows, Linux and macOS the same).
target_sources(orre_platform_vi PRIVATE src/pc_title_loadsave.cpp)
# Its word "LOAD SAVE", made at start-up from the player's own disc (the Battle Mode word sheet read through the plain
# DVD read, traced and styled like the game's menu words). Platform-neutral C++.
target_sources(orre_platform_vi PRIVATE src/pc_title_word.cpp)
# OrrePort's naming keyboard, version B (2026-11-14; docs/port/naming-screen.md): QWERTY pages drawn in the game's
# own name screen through its hooks, off in GameCube-accurate mode. Platform-neutral C++ (keyboard typing, the mouse and
# the IME go through SDL's text input; only who draws the IME's windows differs per OS, see pc_ime_win32.cpp).
target_sources(orre_platform_vi PRIVATE src/pc_name_keyboard.cpp)
# Who draws the input method's composition and choice list on Windows (2026-11-15; the file says why): the name
# keyboard while it types, Windows itself for the hub's and overlay's text fields. Windows only (imm32); Linux and
# macOS need no such file (SDL's IBus / fcitx read the hint live; Cocoa always hands composition to the app).
if (WIN32)
  target_sources(orre_platform_vi PRIVATE src/pc_ime_win32.cpp)
  target_link_libraries(orre_platform_vi PUBLIC imm32)
endif ()
# The field pause menu's PC MENU entry and Start closing the menu ("Access PC menu anywhere", 2026-11-15): one more
# button past the menu tables' end, the game's own PC menu room behind it; off in GameCube-accurate mode.
# Platform-neutral C++ (no OS calls: Windows, Linux and macOS the same).
target_sources(orre_platform_vi PRIVATE src/pc_field_menu.cpp)
# OrrePort's rows on the game's PC screen, first "Main Menu" (2026-11-15; docs/port/pc-menu-anywhere.md): rows read
# from ui/game/pc_menu.ini, records past the menu tables' end, the wheel piece from shipped PNGs, the way back to the
# title's main menu. Platform-neutral C++ (SDL's file and PNG calls: Windows, Linux and macOS the same).
target_sources(orre_platform_vi PRIVATE src/pc_pc_screen.cpp)
# PC MENU saves made in the middle of a story scene, the softlock fix (2026-11-20; docs/port/pc-menu-anywhere.md, "Saves
# made in the middle of a scene"): the open-scene table (ui/game/save_windows.ini) rolls a scene's flags back in the
# written save, and a hang watch offers the previous version after a stuck Continue; off in GameCube-accurate mode.
# Platform-neutral C++ (SDL's file calls and pc_saves: Windows, Linux and macOS the same).
target_sources(orre_platform_vi PRIVATE src/pc_save_windows.cpp)
# The autosave (2026-11-20; docs/port/autosave.md): the game's own save format written beside each slot's save, in
# Slot N\Autosave\, when a field room has settled and nothing in it has changed yet; a background thread writes the file.
# Off in GameCube-accurate mode. Platform-neutral C++ (SDL's file, thread and time calls: Windows, Linux and macOS the same).
target_sources(orre_platform_vi PRIVATE src/pc_autosave.cpp)
# The save-check sweep's test switches (2026-11-20; the project notes, "The save-check sweep"):
# ORRE_SAVECHECK writes a test save at every moment a player could save during a replay (through the autosave's writer),
# ORRE_SAVECHECK_PROBE logs control, holds and people after a reload. Off unless set in an automated run.
# Platform-neutral C++ (SDL's environment calls: Windows, Linux and macOS the same).
target_sources(orre_platform_vi PRIVATE src/pc_savecheck.cpp)
# Step 3.7's item tweaks (2026-11-20; docs/port/tweaks-37-research.md sections 1-1i): the party list after a use or a
# give, the TM design and LEARNED, infinite TMs, the remembered scent cursors, and the bag's USE / READ / CHECK / TOSS
# rule (ui/game/item_menu.ini); off in GameCube-accurate mode. Platform-neutral C++ (SDL's file calls: Windows, Linux
# and macOS the same).
target_sources(orre_platform_vi PRIVATE src/pc_bag_tweaks.cpp)
# Step 3.7's Relic Forest shortcuts (2026-11-20; docs/port/tweaks-37-research.md sections 2-3): PURIFY in the party menu
# for a heart that is open, and the Time Flute anywhere in the forest; one switch, off in GameCube-accurate mode.
# Platform-neutral C++ (no OS calls).
target_sources(orre_platform_vi PRIVATE src/pc_relic_forest.cpp)
# Step 3.7's right-stick walking (2026-11-20; docs/port/tweaks-37-research.md section 4): the C-stick walks Wes in the
# field (a full push runs), through the walking code's own hook; off in GameCube-accurate mode. Platform-neutral C++
# (the stick comes from the game's input layer: Windows, Linux and macOS the same).
target_sources(orre_platform_vi PRIVATE src/pc_cstick_walk.cpp)
# Step 3.7's Smooth Party UI (2026-11-20; docs/port/tweaks-37-research.md sections 6b-6d): the party screen's portraits
# are all there on its first frame (*changed 2026-11-21:* stand-ins drawn from OrrePort's own copy of the portrait
# archive while the game's loads run; the list never waits); off in GameCube-accurate mode. Platform-neutral C++ (no OS
# calls; the archive read through Aurora's disc layer, the same everywhere).
target_sources(orre_platform_vi PRIVATE src/pc_party_ui.cpp)
# The pose rows (2026-11-21; ui/game/pose_rows.ini, docs/port/pc-menu-anywhere.md "Pose rows"): after a Continue from a
# PC MENU spot or an autosave, people a scene left lying (Vander on Mt. Battle) are put back as the scene left them; off
# in GameCube-accurate mode. Platform-neutral C++ (no OS calls).
target_sources(orre_platform_vi PRIVATE src/pc_pose_rows.cpp)
# Step 3.7's last item (2026-11-20; docs/port/tweaks-37-research.md section 8): the logs carry every setting (a start-up
# snapshot: display, controllers, settings.ini, the tweaks, ORRE_ variables). Platform-neutral C++ (SDL's display,
# gamepad and environment calls).
target_sources(orre_platform_vi PRIVATE src/pc_settings_log.cpp)
# Dialogue re-wrapping (2026-11-14; docs/port/text-rewrap.md): a line that would run past its message box moves its
# last words down, off in GameCube-accurate mode. Platform-neutral C++ (no OS calls).
target_sources(orre_platform_vi PRIVATE src/pc_msg_rewrap.cpp)
# The full names beside the save's GameCube names, and the extras file (2.4, 2026-11-14; docs/port/orresave.md).
# Platform-neutral C++ over SDL's file calls.
target_sources(orre_platform_vi PRIVATE src/pc_names.cpp)
# Only one OrrePort per install (rule R8, 2026-11-14): a second launch brings the first window forward and exits.
# Windows: a named mutex and the first window found by its exe (user32 / kernel32, linked already); Linux and macOS: a
# lock file (fcntl), not run there yet.
target_sources(orre_platform_vi PRIVATE src/pc_single_instance.c)
if (TARGET Freetype::Freetype)
  target_link_libraries(orre_platform_vi PUBLIC Freetype::Freetype)
endif ()
if (WIN32)
  # Windows only: asking Windows where its font folders are (SHGetKnownFolderPath, shell32 / ole32 / uuid)
  target_link_libraries(orre_platform_vi PUBLIC shell32 ole32 uuid)
endif ()
if (WIN32)
  target_link_libraries(orre_platform_vi PUBLIC setupapi shell32)
endif ()

# The GS engine's and HSD's first-party assembly routines in C: the thread switches on host fibres
# (docs/port/gs-fibres.md) and the rest (docs/port/asm-routines.md). They include the game's headers (include/
# first, so our SDK headers can hand over to Aurora's with #include_next).
add_library(orre_gs_thread_host STATIC src/gs_thread_host.c src/gs_asm_host.c src/hsd_debug_host.c)
target_include_directories(orre_gs_thread_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include)
target_compile_definitions(orre_gs_thread_host PRIVATE TARGET_PC=1)
target_link_libraries(orre_gs_thread_host PUBLIC orre_platform)

add_executable(orreport src/main.c)
target_link_libraries(orreport PRIVATE orre_platform_vi orre_platform aurora::core aurora::gx aurora::main aurora::os aurora::vi)
if (TARGET aurora::dvd)
  target_link_libraries(orreport PRIVATE aurora::dvd)
endif ()
# Aurora's THP (movie) decoder, built with every build (2026-11-24). Why: firstlink.py links every library it finds in
# build-pc/aurora, and nothing here asked for libaurora_thp.a any more; the main checkout still had one built on
# 2026-10-16, so its game had movies, but a fresh build (the public repo, a new PC) linked the stand-in decoder and
# would have shown no cutscenes (found by comparing the public export's exe with the private one, function by
# function). Linking it into orreport makes the build produce it. All platforms the same.
if (TARGET aurora::thp)
  target_link_libraries(orreport PRIVATE aurora::thp)
endif ()
target_compile_definitions(orreport PRIVATE TARGET_PC=1)

# snatcherLeo's 32-bit pointer decision (docs/port/low-memory.md): the program image (code, static data, string constants)
# must sit below 4 GB, so link every OrrePort executable at a fixed low base without ASLR relocation.
function(orre_low_image target)
  if (MINGW)
    target_link_options(${target} PRIVATE -Wl,--image-base,0x10000000 -Wl,--disable-dynamicbase
                        -Wl,--disable-high-entropy-va)
  elseif (APPLE)
    # macOS reserves the low 4 GB with __PAGEZERO by default; shrink it and link non-PIE low (low-memory.md)
    target_link_options(${target} PRIVATE -Wl,-pagezero_size,0x10000 -Wl,-image_base,0x10000000 -no-pie)
  elseif (UNIX)
    target_link_options(${target} PRIVATE -no-pie -Wl,-Ttext-segment=0x10000000)
  endif ()
endfunction()
orre_low_image(orreport)

if (MINGW)
  # aurora::main (a static library) defines WinMain, but MinGW's startup code only asks for WinMain after the
  # libraries have been scanned; mark it as needed up front so the linker takes it from libaurora_main.a.
  target_link_options(orreport PRIVATE -Wl,-u,WinMain)
  # Link the GCC / C++ / pthread runtimes into the exe so it runs by double-click (no MSYS2 DLLs on PATH)
  target_link_options(orreport PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)
endif ()

# Put the runtime DLLs (SDL3, Dawn + its shader compilers, libpng, zlib) next to orreport.exe
include(${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/cmake/AuroraCopyRuntimeDLLs.cmake)
aurora_copy_runtime_dlls(orreport)

# save_cipher_test: the save slot's encrypt proven the exact inverse of its decrypt on real saves (3.5, 2026-11-19).
# Run build-pc/save_cipher_test.exe <save.gci> ... (the player's own saves; none in the repo). Plain C, every OS.
add_executable(save_cipher_test test/save_cipher_test.c src/pc_save_cipher.c)
if (MINGW)
  target_link_options(save_cipher_test PRIVATE -static-libgcc)
endif ()

# save_region_test: each save's region stamp read by the same code the hub and the SAVE FILES list use (the newest
# valid slot), and the import's change to a European or Japanese .gci proven to touch bytes 0-3 only (3.5 steps 6-7,
# 2026-11-19). Run build-pc/save_region_test.exe <save.gci> [expect] ... (the player's own saves; none in the repo).
# Plain C, no SDL: every OS.
add_executable(save_region_test test/save_region_test.c src/pc_save_region.c src/pc_save_cipher.c)
if (MINGW)
  target_link_options(save_region_test PRIVATE -static-libgcc)
endif ()

# gcopy_region_test: "Make a GameCube copy..." for another region (3.5 step 9, 2026-11-19) on real saves, through the
# same byte work the hub's copy window uses (pc_gcopy_region.c): game code, card header and its sum, stamps and language
# bytes, species names, every changed slot sealed again; round trips give the original back. Run with the player's own
# saves and discs' files (none in the repo; the usage is in the file's comment). Plain C, no SDL: every OS.
add_executable(gcopy_region_test test/gcopy_region_test.c src/pc_gcopy_region.c src/pc_save_region.c src/pc_save_cipher.c)
if (MINGW)
  target_link_options(gcopy_region_test PRIVATE -static-libgcc)
endif ()

# pc_layout_test: the UI-per-ratio layout table's reader (widescreen groundwork, 2026-11-19; docs/port/widescreen-plan.md
# section 6): sample rows parsed and looked up (a piece with no row keeps its 4:3 place, centred), and with the ui
# folder given (build-pc/pc_layout_test.exe port/ui/) the shipped 16x9.ini / 16x10.ini checked valid. Plain C, every OS.
add_executable(pc_layout_test test/pc_layout_test.c src/pc_layout.c)
target_include_directories(pc_layout_test PRIVATE src)
if (MINGW)
  target_link_options(pc_layout_test PRIVATE -static-libgcc)
endif ()

# pc_zip_scrub_test: the report's zip writer and personal-details scrub, which Studio shares (2026-11-22, 4.3 part 3):
# the scrub rule with a home folder outside ASCII, and a zip written into a folder outside ASCII, over itself, and read
# back (build-pc/pc_zip_scrub_test.exe <scratch folder>). Plain C, no SDL; its environment set-up is Windows-only so far.
add_executable(pc_zip_scrub_test test/pc_zip_scrub_test.c src/pc_zip.c src/pc_scrub.c src/pc_clip_file.c)
target_include_directories(pc_zip_scrub_test PRIVATE src)
if (WIN32)
  target_link_libraries(pc_zip_scrub_test PRIVATE shell32) # DragQueryFileW: the clipboard test reads a paste back
endif ()
if (MINGW)
  target_link_options(pc_zip_scrub_test PRIVATE -static-libgcc)
endif ()
# pc_settings_merge_test: settings.ini with two writers, OrrePort and Studio (2026-11-23, Studio's question): Studio's
# change survives OrrePort's next save; OrrePort's newer unsaved change wins (build-pc/pc_settings_merge_test.exe
# <scratch folder ending in a slash>). Plain C and SDL: every OS.
add_executable(pc_settings_merge_test test/pc_settings_merge_test.c src/pc_settings.c)
target_include_directories(pc_settings_merge_test PRIVATE src)
target_link_libraries(pc_settings_merge_test PRIVATE ${AURORA_SDL3_TARGET})
# pc_apps_test: the hub / Studio markers seen from outside both programs (pc_apps.c, 2026-11-23): "self" checks the
# rules alone; "fake-studio" stands in for Studio (and, copied as "OrrePort Studio.exe", for its launcher) so the hub's
# Open Studio / Studio is open / Close Studio can be checked before Studio's own side exists. Windows (its stubs
# elsewhere just print that).
add_executable(pc_apps_test test/pc_apps_test.c src/pc_apps.c)
target_include_directories(pc_apps_test PRIVATE src)
if (WIN32)
  target_link_libraries(pc_apps_test PRIVATE shell32) # ShellExecuteW (pc_apps_start)
endif ()
if (MINGW)
  target_link_options(pc_apps_test PRIVATE -static-libgcc)
endif ()

# species_names_test: un-nicknamed Pokémon in imported European and Japanese saves shown with the species name in the
# play language (3.5 step 10, 2026-11-19): pc_game_text_species.h's lookup (the one pc_names.cpp uses) fed the three
# discs' species names, every Pokémon of the player's own saves mapped (none in the repo; the usage is in the file's
# comment). Plain C++, no SDL: every OS.
add_executable(species_names_test test/species_names_test.cpp src/pc_gcopy_region.c src/pc_save_region.c
               src/pc_save_cipher.c)
if (MINGW)
  target_link_options(species_names_test PRIVATE -static-libgcc -static-libstdc++)
endif ()
if (WIN32) # Windows only: CommandLineToArgvW, the arguments as UTF-8 (Japanese --expect names); not needed elsewhere
  target_link_libraries(species_names_test PRIVATE shell32)
endif ()

# gs_fibre_test: the game's real GSthread scheduler (src/game/gs_thread_exact_800F0424.c, unchanged) on the host
# fibre layer, with a stand-in GSmem inside the GameCube memory range. Run build-pc/gs_fibre_test.exe.
add_executable(gs_fibre_test test/gs_fibre_test.c ${CMAKE_CURRENT_SOURCE_DIR}/../src/game/gs_thread_exact_800F0424.c)
target_include_directories(gs_fibre_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include)
target_compile_definitions(gs_fibre_test PRIVATE TARGET_PC=1)
# GSthreadSetArgs uses MWCC's varargs internals; the test does not call it, so let it compile to nothing.
# The scheduler keeps pointers in u32 globals by design (they point into MEM1), so silence those casts here.
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../src/game/gs_thread_exact_800F0424.c PROPERTIES
  COMPILE_OPTIONS "-D__builtin_va_info(x)=0;-Wno-pointer-to-int-cast;-Wno-int-to-pointer-cast")
# Game code on the PC is compiled with the canonical prototypes force-included (include/pc/prototypes.h, session
# main); the scheduler file relies on them for its GSmem calls.
target_compile_options(gs_fibre_test PRIVATE "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/../include/pc/prototypes.h")
target_link_libraries(gs_fibre_test PRIVATE orre_gs_thread_host)
orre_low_image(gs_fibre_test)
if (MINGW)
  target_link_options(gs_fibre_test PRIVATE -static-libgcc)
endif ()

# gs_asm_test: the remaining platform routines (PPCArch accessors, GSmathInit, GSscratchInit with the game's real
# scratch allocator, the script VM's native call, HSD_SaveContext). Run build-pc/gs_asm_test.exe.
set(ORRE_GAME_SCRATCH ${CMAKE_CURRENT_SOURCE_DIR}/../src/game/gs_scratch_800EE928.c
                      ${CMAKE_CURRENT_SOURCE_DIR}/../src/game/gs_scratch_alloc_800EEC38.c)
add_executable(gs_asm_test test/gs_asm_test.c ${ORRE_GAME_SCRATCH})
target_include_directories(gs_asm_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include)
target_compile_definitions(gs_asm_test PRIVATE TARGET_PC=1)
set_source_files_properties(${ORRE_GAME_SCRATCH} PROPERTIES
  COMPILE_OPTIONS "-Wno-pointer-to-int-cast;-Wno-int-to-pointer-cast")
target_link_libraries(gs_asm_test PRIVATE orre_gs_thread_host)
orre_low_image(gs_asm_test)
if (MINGW)
  target_link_options(gs_asm_test PRIVATE -static-libgcc)
endif ()

# MusyX audio, step 1 (docs/port/musyx-evaluation.md): AxioDL MusyX (extern/musyx-pc, CC0) with its PC backend,
# rendering without a device. musyx_play plays one Colosseum sound effect from the player's extracted data into a WAV.
# Step 2 replaces AxioDL's runtime with our matched src/musyx.
set(MUSYX_ENABLE_SDL OFF CACHE BOOL "" FORCE)
set(MUSYX_ENABLE_DEBUGGER OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../extern/musyx-pc ${CMAKE_BINARY_DIR}/musyx-pc EXCLUDE_FROM_ALL)
# AxioDL's PC runtime calls the debugger's C hooks (pause check, voice events) even with MUSYX_ENABLE_DEBUGGER off;
# add that C half (no UI) so it links, without changing the vendored copy.
target_sources(musyx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../extern/musyx-pc/src/musyx/debugger/debugger.c)
add_executable(musyx_play test/musyx_play.c)
target_link_libraries(musyx_play PRIVATE musyx)
if (MINGW)
  target_link_options(musyx_play PRIVATE -static-libgcc)
endif ()

# MusyX audio, step 2 (in progress): our matched runtime (src/musyx, minus its GameCube hardware units) with AxioDL's
# PC hardware layer, joined by port/src/musyx_pc_alias.h (force-included into our units only). Not linked into a
# program yet: see docs/port/musyx-evaluation.md "Audio path, step 2" for what is still missing.
# the runtime units of the GameCube build (configure.py), without the hardware layer: hw_dolphin, hw_aramdma,
# hw_dspctrl_exact_* and hardware.c (AxioDL's versions take their place: snatcherLeo's rule, audio hardware -> AxioDL)
set(ORRE_MUSYX_RUNTIME
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/seq_get_private_id_exact_8014635C.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/seq_residual_801463C4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/seq_exact_80146E88.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/seq_residual_801485FC.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/seq_exact_80149090.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/synth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/seq_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/snd_synthapi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/stream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/synthdata.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/synthmacros.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/synthvoice.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/synth_ac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/synth_adsr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/synth_vsamples.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/s_data.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/hw_volconv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/snd3d.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/snd_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/snd_math.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/snd_midictrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/snd_service_exact_80162070.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/musyx_candidate_801643D8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/musyx_exact_80164488.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/musyx_r50_801644E0_prefix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/musyx/runtime/reverb_candidate_80164520.c)
# + one PC-only, observe-only helper compiled with our runtime's headers: what a voice plays (music, sound effect or
# stream), for the sound split prototype's per-voice output gain (docs/port/sound-split.md)
list(APPEND ORRE_MUSYX_RUNTIME ${CMAKE_CURRENT_SOURCE_DIR}/src/musyx_pc_voiceclass.c)
add_library(orre_musyx_runtime OBJECT ${ORRE_MUSYX_RUNTIME})
target_include_directories(orre_musyx_runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include)
target_compile_definitions(orre_musyx_runtime PRIVATE TARGET_PC=1)
target_compile_options(orre_musyx_runtime PRIVATE -fpermissive -w
                       "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/../include/pc/prototypes.h"
                       "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/musyx_pc_alias.h")

# musyx_layout_check: structures shared by our MusyX runtime and AxioDL's PC hardware layer must have the same layout
# on the PC. One half compiles with our headers, the other with AxioDL's; main compares sizes and field offsets.
add_library(musyx_layout_ours OBJECT test/layout/layout_ours.c)
target_include_directories(musyx_layout_ours PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include test/layout)
target_compile_definitions(musyx_layout_ours PRIVATE TARGET_PC=1)
add_library(musyx_layout_axio OBJECT test/layout/layout_axio.c)
target_include_directories(musyx_layout_axio PRIVATE test/layout)
target_link_libraries(musyx_layout_axio PRIVATE musyx)
add_executable(musyx_layout_check test/layout/layout_main.c $<TARGET_OBJECTS:musyx_layout_ours>
               $<TARGET_OBJECTS:musyx_layout_axio>)
target_include_directories(musyx_layout_check PRIVATE test/layout)
if (MINGW)
  target_link_options(musyx_layout_check PRIVATE -static-libgcc)
endif ()

# MusyX audio, step 2: our matched runtime + AxioDL's PC hardware layer as one library (orre_musyx).
#  - our 27 runtime units (orre_musyx_runtime above) and the PC data file, both with musyx_pc_alias.h force-included;
#  - MusyX's initialised tables, generated into build-pc/generated/ (untracked) from the game's data units;
#  - AxioDL's hardware-layer files compiled with AxioDL's include paths and definitions, without AxioDL's runtime.
set(ORRE_MUSYX_TABLES ${CMAKE_BINARY_DIR}/generated/musyx_tables.c)
add_custom_command(OUTPUT ${ORRE_MUSYX_TABLES}
  COMMAND python -I ${CMAKE_CURRENT_SOURCE_DIR}/tools/musyx_tables.py ${ORRE_MUSYX_TABLES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/musyx_tables.py ${CMAKE_CURRENT_SOURCE_DIR}/../src/game/data/data_80363CA8.c
          ${CMAKE_CURRENT_SOURCE_DIR}/../src/game/data/data_80369D20.c
  COMMENT "Generating MusyX tables (native byte order) into build-pc/generated")
add_library(orre_musyx_data OBJECT src/musyx_pc_data.c ${ORRE_MUSYX_TABLES})
# two typed constants MusyX reads that the game's own data units define; the game's link has them, the standalone
# audio tests compile the two files
set(ORRE_MUSYX_GAME_DATA ${CMAKE_CURRENT_SOURCE_DIR}/../src/game/data/sdata2_8047D3B0.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/game/data/sdata2_8047D3C0.c)
set_source_files_properties(${ORRE_MUSYX_GAME_DATA} PROPERTIES
  INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../include;${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include"
  COMPILE_DEFINITIONS TARGET_PC=1)
target_include_directories(orre_musyx_data PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include)
target_compile_definitions(orre_musyx_data PRIVATE TARGET_PC=1)
target_compile_options(orre_musyx_data PRIVATE "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/musyx_pc_alias.h")

set(AXIO_RT ${CMAKE_CURRENT_SOURCE_DIR}/../extern/musyx-pc/src/musyx/runtime)
add_library(orre_musyx_hw OBJECT ${AXIO_RT}/hardware.c ${AXIO_RT}/hw_dolphin.c ${AXIO_RT}/hw_dspctrl.c ${AXIO_RT}/hw_aramdma.c
            ${AXIO_RT}/hw_memory.c ${AXIO_RT}/hw_lib_dolphin.c ${AXIO_RT}/hw_pc.c ${AXIO_RT}/hw_pc_output.c
            ${AXIO_RT}/hw_pc_rate.c ${AXIO_RT}/hw_pc_vsample.c ${AXIO_RT}/hw_pc_song.c ${AXIO_RT}/hw_pc_samples.c
            ${AXIO_RT}/hw_pc_assets.c ${AXIO_RT}/hw_pc_decode.c ${AXIO_RT}/hw_pc_sdl.c src/musyx_pc_groups.c)
target_include_directories(orre_musyx_hw PRIVATE $<TARGET_PROPERTY:musyx,INTERFACE_INCLUDE_DIRECTORIES> ${AXIO_RT})
target_compile_definitions(orre_musyx_hw PRIVATE $<TARGET_PROPERTY:musyx,INTERFACE_COMPILE_DEFINITIONS>)
if (WIN32)
  target_compile_definitions(orre_musyx_hw PRIVATE _USE_MATH_DEFINES)
endif ()

add_library(orre_musyx STATIC $<TARGET_OBJECTS:orre_musyx_runtime> $<TARGET_OBJECTS:orre_musyx_data>
            $<TARGET_OBJECTS:orre_musyx_hw>)
# AxioDL's SDL3 device adapter (hw_pc_sdl.c) plays the rendered audio live; SDL3 is Aurora's (prebuilt)
target_link_libraries(orre_musyx_hw PRIVATE ${AURORA_SDL3_TARGET})
target_link_libraries(orre_musyx PUBLIC orre_platform aurora::os ${AURORA_SDL3_TARGET})

# The sound split prototype (EXPERIMENTAL, 2026-11-16; docs/port/sound-split.md): a second, audible copy of the whole
# MusyX library above, built from the same sources with every global renamed aud_<name> (a force-included header of
# #defines that port/tools/musyx_prefix.py generates from liborre_musyx.a's own symbol table, so a new MusyX global is
# picked up by the next build). It plays the music at its normal tempo while the game runs faster; the game's instance
# (orre_musyx) is unchanged. Only pc_sound_split.c calls it; with ORRE_SOUND_SPLIT off it is linked but never set up.
# After it is built, the same tool checks the copy defines nothing unprefixed and refers to none of the game
# instance's globals (a name a macro hid from the header would otherwise share the game instance's state silently).
# Platforms: CMake + GNU binutils' nm; the same on Windows (MinGW), Linux and macOS (with GNU nm or llvm-nm).
set(ORRE_MUSYX_AUD_PREFIX ${CMAKE_BINARY_DIR}/generated/musyx_audible_prefix.h)
add_custom_command(OUTPUT ${ORRE_MUSYX_AUD_PREFIX}
  COMMAND python -I ${CMAKE_CURRENT_SOURCE_DIR}/tools/musyx_prefix.py header ${CMAKE_NM} $<TARGET_FILE:orre_musyx>
          ${ORRE_MUSYX_AUD_PREFIX} ${CMAKE_CURRENT_SOURCE_DIR}/src/musyx_pc_alias.h
  DEPENDS orre_musyx ${CMAKE_CURRENT_SOURCE_DIR}/tools/musyx_prefix.py ${CMAKE_CURRENT_SOURCE_DIR}/src/musyx_pc_alias.h
  COMMENT "Generating the sound split's MusyX prefix header into build-pc/generated")
add_custom_target(orre_musyx_aud_prefix DEPENDS ${ORRE_MUSYX_AUD_PREFIX})
add_library(orre_musyx_aud_runtime OBJECT ${ORRE_MUSYX_RUNTIME})
target_include_directories(orre_musyx_aud_runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include)
target_compile_definitions(orre_musyx_aud_runtime PRIVATE TARGET_PC=1 ORRE_MUSYX_AUDIBLE_COPY=1)
target_compile_options(orre_musyx_aud_runtime PRIVATE -fpermissive -w
                       "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/../include/pc/prototypes.h"
                       "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/musyx_pc_alias.h"
                       "SHELL:-include ${ORRE_MUSYX_AUD_PREFIX}")
add_library(orre_musyx_aud_data OBJECT src/musyx_pc_data.c ${ORRE_MUSYX_TABLES})
target_include_directories(orre_musyx_aud_data PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include)
target_compile_definitions(orre_musyx_aud_data PRIVATE TARGET_PC=1 ORRE_MUSYX_AUDIBLE_COPY=1)
target_compile_options(orre_musyx_aud_data PRIVATE "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/musyx_pc_alias.h"
                       "SHELL:-include ${ORRE_MUSYX_AUD_PREFIX}")
add_library(orre_musyx_aud_hw OBJECT ${AXIO_RT}/hardware.c ${AXIO_RT}/hw_dolphin.c ${AXIO_RT}/hw_dspctrl.c
            ${AXIO_RT}/hw_aramdma.c ${AXIO_RT}/hw_memory.c ${AXIO_RT}/hw_lib_dolphin.c ${AXIO_RT}/hw_pc.c
            ${AXIO_RT}/hw_pc_output.c ${AXIO_RT}/hw_pc_rate.c ${AXIO_RT}/hw_pc_vsample.c ${AXIO_RT}/hw_pc_song.c
            ${AXIO_RT}/hw_pc_samples.c ${AXIO_RT}/hw_pc_assets.c ${AXIO_RT}/hw_pc_decode.c ${AXIO_RT}/hw_pc_sdl.c
            src/musyx_pc_groups.c)
target_include_directories(orre_musyx_aud_hw PRIVATE $<TARGET_PROPERTY:musyx,INTERFACE_INCLUDE_DIRECTORIES> ${AXIO_RT})
target_compile_definitions(orre_musyx_aud_hw PRIVATE $<TARGET_PROPERTY:musyx,INTERFACE_COMPILE_DEFINITIONS>
                           ORRE_MUSYX_AUDIBLE_COPY=1 ORRE_SOUND_LOG_ENV="ORRE_SOUND_LOG_AUDIBLE")
target_compile_options(orre_musyx_aud_hw PRIVATE "SHELL:-include ${ORRE_MUSYX_AUD_PREFIX}")
if (WIN32)
  target_compile_definitions(orre_musyx_aud_hw PRIVATE _USE_MATH_DEFINES)
endif ()
target_link_libraries(orre_musyx_aud_hw PRIVATE ${AURORA_SDL3_TARGET})
foreach (t orre_musyx_aud_runtime orre_musyx_aud_data orre_musyx_aud_hw)
  add_dependencies(${t} orre_musyx_aud_prefix)
endforeach ()
add_library(orre_musyx_aud STATIC $<TARGET_OBJECTS:orre_musyx_aud_runtime> $<TARGET_OBJECTS:orre_musyx_aud_data>
            $<TARGET_OBJECTS:orre_musyx_aud_hw>)
target_link_libraries(orre_musyx_aud PUBLIC orre_platform aurora::os ${AURORA_SDL3_TARGET})
add_custom_command(TARGET orre_musyx_aud POST_BUILD
  COMMAND python -I ${CMAKE_CURRENT_SOURCE_DIR}/tools/musyx_prefix.py check ${CMAKE_NM} $<TARGET_FILE:orre_musyx_aud>
          $<TARGET_FILE:orre_musyx>
  COMMENT "Checking the sound split's audible MusyX copy shares nothing with the game's")

# The game's sound on the PC device: lock + paused device at startup, started after the game's sndInit.
# pc_sound_split.c: the sound split prototype's wrappers (EXPERIMENTAL; linked into the game exe only, through the
# --wrap list in pc_sound_split.wrap). Compiled with our runtime's headers (the reverb's layout is ours).
add_library(orre_audio STATIC src/pc_audio.c src/pc_sound_split.c)
set_source_files_properties(src/pc_sound_split.c PROPERTIES
  INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../include;${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include"
  COMPILE_DEFINITIONS TARGET_PC=1)
target_include_directories(orre_audio PRIVATE $<TARGET_PROPERTY:musyx,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(orre_audio PRIVATE $<TARGET_PROPERTY:musyx,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(orre_audio PUBLIC orre_musyx_aud orre_musyx orre_platform_vi)

# pc_audio_test: that start-up order, headless (exit 77 = no audio device)
add_executable(pc_audio_test test/pc_audio_test.c test/musyx_crt_stub.c ${ORRE_MUSYX_GAME_DATA})
target_include_directories(pc_audio_test PRIVATE $<TARGET_PROPERTY:musyx,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(pc_audio_test PRIVATE $<TARGET_PROPERTY:musyx,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(pc_audio_test PRIVATE orre_audio
                      "$<LINK_GROUP:RESCAN,aurora::core,aurora::gx,aurora::os,aurora::vi>")
orre_low_image(pc_audio_test)
if (MINGW)
  target_link_options(pc_audio_test PRIVATE -static-libgcc)
endif ()

# musyx_play_ours: the same test as musyx_play, on our runtime (step 1's numbers are the reference)
add_executable(musyx_play_ours test/musyx_play.c test/musyx_crt_stub.c ${ORRE_MUSYX_GAME_DATA})
target_include_directories(musyx_play_ours PRIVATE $<TARGET_PROPERTY:musyx,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(musyx_play_ours PRIVATE $<TARGET_PROPERTY:musyx,INTERFACE_COMPILE_DEFINITIONS>
                           MUSYX_PLAY_OURS=1)
target_link_libraries(musyx_play_ours PRIVATE orre_musyx)
orre_low_image(musyx_play_ours) # our runtime keeps callbacks in 32-bit slots: the image must sit below 4 GB
if (MINGW)
  target_link_options(musyx_play_ours PRIVATE -static-libgcc)
endif ()

# pc_card_test: a save round trip through Aurora's CARD library (pass a scratch folder)
if (TARGET aurora::card)
  add_executable(pc_card_test test/pc_card_test.c)
  target_include_directories(pc_card_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../extern/aurora/include)
  target_compile_definitions(pc_card_test PRIVATE TARGET_PC=1)
  target_link_libraries(pc_card_test PRIVATE
                        "$<LINK_GROUP:RESCAN,aurora::card,aurora::core,aurora::gx,aurora::os,aurora::vi>")
  if (MINGW)
    target_link_options(pc_card_test PRIVATE -static-libgcc)
  endif ()
endif ()

# pc_rel_test: OSLink / OSUnlink over real REL modules (pass the .rel files; port/tools/rel_check.py compares the
# prolog stores with the decompiled _prolog in src/rel/)
add_executable(pc_rel_test test/pc_rel_test.c)
target_link_libraries(pc_rel_test PRIVATE orre_platform)
orre_low_image(pc_rel_test)
if (MINGW)
  target_link_options(pc_rel_test PRIVATE -static-libgcc)
endif ()

# pc_thread_test: the SDK threads on fibres (priorities, sleep / wakeup, yield, join, the idle loop, interrupt state
# per thread), without a window
add_executable(pc_thread_test test/pc_thread_test.c)
target_link_libraries(pc_thread_test PRIVATE orre_platform_vi
                      "$<LINK_GROUP:RESCAN,aurora::core,aurora::gx,aurora::os,aurora::vi>")
orre_low_image(pc_thread_test)
if (MINGW)
  target_link_options(pc_thread_test PRIVATE -static-libgcc)
endif ()

# pc_sdk_test: interrupt masking (nesting rules, cross-thread exclusion) and the PS math forwarders
add_executable(pc_sdk_test test/pc_sdk_test.c)
# Aurora's core and gx refer to each other; without aurora::main nothing pulls core in early, so link them as a group
target_link_libraries(pc_sdk_test PRIVATE orre_platform_vi
                      "$<LINK_GROUP:RESCAN,aurora::core,aurora::gx,aurora::os,aurora::vi>")
if (MINGW)
  target_link_options(pc_sdk_test PRIVATE -static-libgcc)
endif ()

# pc_osalloc_test: OSAlloc's free-list insert on the PC (port/src/pc_sdk_gaps.c)
add_executable(pc_osalloc_test test/pc_osalloc_test.c src/pc_sdk_gaps.c)
if (MINGW)
  target_link_options(pc_osalloc_test PRIVATE -static-libgcc)
endif ()
