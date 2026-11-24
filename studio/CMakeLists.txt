# OrrePort Studio: the editor app, and the shared reader core under it (the project notes, "10. What Studio
# is", "Studio's architecture" and "The Win32 foundation, settled in the Studio review", 2026-11-17).
#
# Why a CMake project of its own: Studio is a separate program from OrrePort (a native desktop app, not the game's
# window), so it never links the game, Aurora or SDL. It shares only two things with the port: the disc reader (nod)
# and the GPU layer (Dawn / WebGPU) for the 3D view, both fetched exactly as Aurora fetches them (Aurora's own provider
# modules are included below, so the versions are pinned in one place: extern/aurora/cmake/AuroraDependencyVersions.cmake).
#
#   cmake -S studio -B build-studio -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-studio -j 16
#
# To reuse the packages the port already downloaded (no network), add
#   -DFETCHCONTENT_SOURCE_DIR_NOD_PREBUILT=<main checkout>/build-pc/_deps/nod_prebuilt-src
#   -DFETCHCONTENT_SOURCE_DIR_DAWN_PREBUILT=<main checkout>/build-pc/_deps/dawn_prebuilt-src
# (studio/build.ps1 does this).
#
# Targets:
#   orre_core        the shared reader core: a static library with a C API (include/orre_core.h), C++ inside. No
#                    interface code and no OS calls besides plain C/C++ file and thread use, so it builds unchanged on
#                    Windows, Linux and macOS (likely; only Windows built so far). The port can link it later.
#   orrestudio-cli   the core's command-line mode (`orrestudio-cli --disc <iso> dump species 1`): the test harness and,
#                    later, the modders' batch tool. Plain console program, every OS.
#   orrestudio       the Win32 shell (Windows only). Linux gets a Qt shell and macOS an AppKit shell later, over the
#                    same core (the plan's table); this file then picks the shell per OS.
# 3.26: `cmake -E copy_directory_if_different` (the data folders below; 2026-11-22). The build uses CMake 4.4.
cmake_minimum_required(VERSION 3.26)
project(OrrePortStudio C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ORRE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# nod and Dawn through Aurora's provider modules (prebuilt packages from encounter's GitHub releases, the same ones the
# port uses; already approved for the port). Shared linkage, as in the port: nod.dll and webgpu_dawn.dll ship beside
# the exe. Platforms: the modules pick the package per OS (Windows x86-64, Linux, macOS arm64).
set(AURORA_NOD_PROVIDER "package" CACHE STRING "")
set(AURORA_NOD_LINKAGE "shared" CACHE STRING "")
set(AURORA_DAWN_PROVIDER "package" CACHE STRING "")
set(AURORA_DAWN_LINKAGE "shared" CACHE STRING "")
include(${ORRE_ROOT}/extern/aurora/cmake/AuroraDependencyVersions.cmake)
include(${ORRE_ROOT}/extern/aurora/cmake/AuroraNodProvider.cmake)

# ---- the shared reader core ----
add_library(orre_core STATIC
  core/src/disc.cpp
  core/src/fsys.cpp
  core/src/msg.cpp
  core/src/species.cpp
  core/src/cry.cpp
  core/src/gx_texture.cpp
  core/src/hsd_model.cpp
  core/src/particles.cpp
  core/src/wzx.cpp
  core/src/battle_camera.cpp
  core/src/bolt.cpp
  core/src/leaves.cpp
  core/src/png_write.cpp
  core/src/layout_check.c
)
target_include_directories(orre_core PUBLIC core/include PRIVATE core/src)
# debug information, as the window's exe (below): a crash inside the core (the 2026-11-22 particle crash was) then
# decodes to its function and line
target_compile_options(orre_core PRIVATE -g)
# layout_check.c includes the decomp's own header for the species record (include/game/pokemon_data.h) and checks at
# compile time that every field the core reads sits where the decomp says (the review's decision 3).
# (Aurora's include folder after it: the decomp's dolphin/types.h reaches Aurora's for the basic types on the PC.)
target_include_directories(orre_core PRIVATE ${ORRE_ROOT}/include ${ORRE_ROOT}/extern/aurora/include)
set_source_files_properties(core/src/layout_check.c PROPERTIES COMPILE_DEFINITIONS "TARGET_PC")
target_link_libraries(orre_core PUBLIC nod::nod)
if (MINGW)
  # Why -static: the exe then needs none of MSYS2's runtime DLLs beside it (libstdc++, libgcc and libwinpthread: the
  # threads library std::thread uses). Without it Studio ran only where MSYS2 is on PATH (snatcherLeo's first start failed
  # with "libwinpthread-1.dll was not found", 2026-11-18). nod.dll, webgpu_dawn.dll and Windows' own DLLs stay shared:
  # their import libraries link as usual. Windows only; Linux and macOS use their system's C++ runtime.
  target_link_options(orre_core INTERFACE -static)
endif ()

# The nod DLL beside every exe (Windows needs DLLs next to the exe or on PATH; Linux and macOS find the shared library
# through the rpath CMake sets, so nothing to copy there).
function(studio_copy_runtime target)
  if (WIN32)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:nod::nod_shared> $<TARGET_FILE_DIR:${target}>)
  endif ()
endfunction()

# ---- the command-line mode ----
add_executable(orrestudio-cli cli/main.cpp)
target_link_libraries(orrestudio-cli PRIVATE orre_core)
studio_copy_runtime(orrestudio-cli)

# ---- the Win32 shell ----
# STUDIO_SHELL=OFF builds only the core and the CLI (a headless test machine, or Linux / macOS before their shells exist)
option(STUDIO_SHELL "Build Studio's window (the Win32 shell on Windows)" ON)
if (WIN32 AND STUDIO_SHELL)
  enable_language(RC)  # windres: the manifest (studio.rc)
  include(${ORRE_ROOT}/extern/aurora/cmake/AuroraDawnProvider.cmake)
  add_executable(orrestudio WIN32
    win32/app.cpp
    win32/lang.cpp
    win32/log.cpp
    win32/theme.cpp
    win32/viewer.cpp
    win32/studio.rc
    # OrrePort's crash handler, used unchanged (2026-11-22, after a crash snatcherLeo hit with no trace left): a crash prints
    # the exception and the call stack to stderr, decodable with addr2line. Windows x64 only (pc_crash_posix.c is the
    # Linux / macOS twin). Step 4.2e moves both programs onto a shared diagnostics library that writes it to a file
    ${ORRE_ROOT}/port/src/pc_crash_win32.c
    # the action trail (part 3 of the loggers, 2026-11-22): plain C with no Studio types, so Main can move it into the
    # shared diagnostics library unchanged; every platform
    diag/trail.c
    # the build's identity for the logs and reports (4.3 part 3, item 1): its header is made at every build below
    diag/build_stamp.c
    # the machine and its screens for the logs and reports (4.3 part 3, items 2 and 5)
    diag/sysinfo.c
    # Report a problem's zip, the user's name hidden in its text, and its copy to the clipboard: OrrePort's own shared
    # plain-C files (2026-11-22: "the files stay in port/src ... and you compile them from there"), so both programs
    # make reports the same way with one version to fix. pc_clip_file: Windows' clipboard (user32); Linux / macOS return 0
    ${ORRE_ROOT}/port/src/pc_zip.c
    ${ORRE_ROOT}/port/src/pc_scrub.c
    ${ORRE_ROOT}/port/src/pc_clip_file.c
    # how OrrePort and Studio find, show, start and close each other (4.4, 2026-11-23; the project's plain-C pc_apps, Studio's
    # design approved by snatcherLeo): one copy each per OrrePort folder, the hub's "Open Studio" and Studio's "Open OrrePort",
    # "Close OrrePort" to finish an update. Windows (named mutexes and shared memory; shell32 for ShellExecute);
    # Linux / macOS: the module's stubs until their twins exist
    ${ORRE_ROOT}/port/src/pc_apps.c
  )
  target_include_directories(orrestudio PRIVATE win32 diag ${ORRE_ROOT}/port/src ${CMAKE_CURRENT_BINARY_DIR}/gen)
  # The updater both programs share (4.4, 2026-11-23; port/src/pc_update.h, docs/port/updater-plan.md): the project's plain-C
  # module, compiled here as OrrePort compiles it (port/CMakeLists.txt's orre_update), with Monocypher (the Ed25519
  # signature check, 4.0.3, CC0 / BSD). Where Monocypher is found, in order: the repo's own extern/ (vendored there
  # unchanged, 2026-11-24, so a clean checkout of the public repo builds Studio with its updater, as it builds
  # OrrePort), then the older _external folder beside the repo, or beside the main checkout for a worktree (the
  # fallbacks for a checkout from before extern/). zlib unpacks the update and links
  # statically like the rest of Studio (-static: MSYS2's libz.a; no zlib1.dll needed); winhttp is Windows' own. The
  # version is the repo's VERSION file, the same number OrrePort shows. Without Monocypher (none of those folders),
  # Studio builds without the updater (STUDIO_NO_UPDATER) and simply shows no update. Windows today;
  # Linux / macOS: pc_update_init returns 0 there until its twin exists (the module's own note)
  foreach (_mono ${ORRE_ROOT}/extern/monocypher-4.0.3/src ${ORRE_ROOT}/../_external/monocypher-4.0.3/src
                 ${ORRE_ROOT}/../../../../_external/monocypher-4.0.3/src)
    if (NOT STUDIO_MONOCYPHER AND EXISTS ${_mono}/monocypher.c)
      get_filename_component(STUDIO_MONOCYPHER ${_mono} ABSOLUTE)
    endif ()
  endforeach ()
  if (STUDIO_MONOCYPHER)
    target_sources(orrestudio PRIVATE ${ORRE_ROOT}/port/src/pc_update.c ${STUDIO_MONOCYPHER}/monocypher.c
                   ${STUDIO_MONOCYPHER}/optional/monocypher-ed25519.c)
    target_include_directories(orrestudio PRIVATE ${STUDIO_MONOCYPHER} ${STUDIO_MONOCYPHER}/optional)
    target_link_libraries(orrestudio PRIVATE z winhttp)
  else ()
    target_compile_definitions(orrestudio PRIVATE STUDIO_NO_UPDATER)
    message(WARNING "Monocypher not found (extern/monocypher-4.0.3): Studio builds without the updater")
  endif ()
  file(STRINGS ${ORRE_ROOT}/VERSION STUDIO_VERSION_LINE LIMIT_COUNT 1)
  target_compile_definitions(orrestudio PRIVATE ORRE_VERSION="${STUDIO_VERSION_LINE}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ORRE_ROOT}/VERSION)
  # the build stamp, remade at every build (commit, uncommitted changes, branch, time; diag/build_stamp.cmake says
  # why). Plain CMake and git; every platform
  add_custom_target(studio_stamp ALL
    COMMAND ${CMAKE_COMMAND} -DSRC=${ORRE_ROOT} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/gen/build_stamp_gen.h
            "-DPROGRAM=OrrePort Studio" -P ${CMAKE_CURRENT_SOURCE_DIR}/diag/build_stamp.cmake
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/gen/build_stamp_gen.h)
  add_dependencies(orrestudio studio_stamp)
  # debug information kept in the exe (2026-11-22: OrrePort's exe keeps -g so a crash's addresses decode with
  # addr2line against that exact build; a stripped exe with the full one kept privately is a Phase 5 question). Every
  # platform's GCC / Clang take -g the same way
  target_compile_options(orrestudio PRIVATE -g)
  # Windows 10 (version 1809 or later) is the floor: per-monitor DPI v2, Direct2D's SVG drawing and DirectWrite's
  # font sets need it; MinGW's headers hide those APIs unless the version is set.
  target_compile_definitions(orrestudio PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00
                             WINVER=0x0A00 NTDDI_VERSION=0x0A000006)
  # d2d1 / dwrite: Studio's own surfaces (decision 1, hybrid drawing); comctl32: Windows' standard controls (the search
  # box); dwmapi: the dark title bar; shcore: per-monitor DPI queries; uxtheme: dark scroll bars / controls.
  # shlwapi: in-memory streams for the SVGs; psapi: the memory watch; d3d11 / dxgi: the shared Direct2D device and the
  # windows' swap chains (the multi-window plan, step 1c: pictures drawn in any window), Windows' own system libraries
  target_link_libraries(orrestudio PRIVATE orre_core dawn::webgpu_dawn d2d1 dwrite windowscodecs comctl32 dwmapi
                        shcore uxtheme comdlg32 ole32 shlwapi psapi d3d11 dxgi winmm shell32)  # (winmm: PlaySound, the cries;
                        # shell32: pc_apps' ShellExecute, starting OrrePort)
  target_link_options(orrestudio PRIVATE -municode)
  studio_copy_runtime(orrestudio)
  # beside the exe: Dawn's DLLs (webgpu_dawn and the shader compiler Direct3D 12 needs), the hub's O icons, the
  # editor symbols and Studio's language files (development layout: ui\, lang\ beside the exe; in the portable layout
  # the project's port/tools/stage_bin.py copies them to app\UI\Studio\ and app\UI\lang\Studio\, which Studio reads first;
  # app\ was bin\ until layout A, 2026-11-22)
  get_target_property(_dawn_dll dawn::webgpu_dawn IMPORTED_LOCATION_RELEASE)
  if (NOT _dawn_dll)
    get_target_property(_dawn_dll dawn::webgpu_dawn IMPORTED_LOCATION)
  endif ()
  get_filename_component(_dawn_bin "${_dawn_dll}" DIRECTORY)
  add_custom_command(TARGET orrestudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:dawn::webgpu_dawn> $<TARGET_FILE_DIR:orrestudio>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_dawn_bin}/dxcompiler.dll ${_dawn_bin}/dxil.dll $<TARGET_FILE_DIR:orrestudio>)
  # the data folders on every build (a separate target: a change to a language file alone doesn't relink the exe, so
  # a post-link copy would miss it). Only files that changed are copied (*changed 2026-11-22*: each staging copied
  # all 81 of them again, as copy_directory gave every file a new time on every build)
  add_custom_target(studio_data ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${ORRE_ROOT}/port/ui/icons ${CMAKE_BINARY_DIR}/ui/icons
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${ORRE_ROOT}/docs/design/brand/studio-symbols ${CMAKE_BINARY_DIR}/ui/symbols
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/lang ${CMAKE_BINARY_DIR}/lang
    # OrrePort's own language files, for the lines both programs share word for word (the disc messages, 2026-11-23;
    # lang_load_shared); in the portable layout they are app\UI\lang\ already
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${ORRE_ROOT}/port/ui/lang ${CMAKE_BINARY_DIR}/ui/lang)
endif ()
