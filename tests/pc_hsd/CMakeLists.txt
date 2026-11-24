# PC rendering test for HSD (toward Milestone 1): load one model archive, relocate it with the disc
# marks (include/pc/disc_types.h), build HSD's runtime objects with the matched src/hsd code and draw one frame
# through Aurora. Separate from the GameCube build and from orreport; reuses port/ (B's platform layer and Aurora).
#
#   cmake -S tests/pc_hsd -B build-pc/hsdtest -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
#         -DCMAKE_BUILD_TYPE=RelWithDebInfo          (plus the FETCHCONTENT_* options in README.md)
#   cmake --build build-pc/hsdtest --target pc_hsd_test
#   build-pc/hsdtest/pc_hsd_test.exe <model archive .dat>
#
# Game data never enters git: the generated data definitions (tools/local/port/pcdata.py) live in build-pc/.
cmake_minimum_required(VERSION 3.25)
project(PcHsdTest C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
# B's platform layer and Aurora. Development only: build-pc/overlay/port (with overlay/include and overlay/extern
# next to it, exported from the project's branch) is used instead while this branch waits for a merge.
if (EXISTS ${ROOT}/build-pc/overlay/port/CMakeLists.txt)
  add_subdirectory(${ROOT}/build-pc/overlay/port ${CMAKE_BINARY_DIR}/port EXCLUDE_FROM_ALL)
else ()
  add_subdirectory(${ROOT}/port ${CMAKE_BINARY_DIR}/port EXCLUDE_FROM_ALL)
endif ()

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/hsd_units.txt HSD_UNITS)
list(TRANSFORM HSD_UNITS PREPEND ${ROOT}/)
set(GENERATED_DATA ${ROOT}/build-pc/generated/pc_hsd_data.c)
set(EXTRA_SOURCES)
if (EXISTS ${GENERATED_DATA})
  list(APPEND EXTRA_SOURCES ${GENERATED_DATA})
endif ()

# Stubs and platform gaps are compiled without the game's forced prototypes header (their signatures are generic)
set(GENERATED_STUBS ${ROOT}/build-pc/generated/pc_hsd_stubs.c)
add_library(pc_hsd_gaps STATIC platform_gaps.c gx_trace.c)
if (EXISTS ${GENERATED_STUBS})
  target_sources(pc_hsd_gaps PRIVATE ${GENERATED_STUBS})
endif ()
target_compile_options(pc_hsd_gaps PRIVATE -w)

# One test program: its sources, its gaps library (stubs and platform gaps, built without the forced headers)
function(pc_hsd_program name gaps)
  add_executable(${name} ${ARGN})
  # Development only: build-pc/overlay/ (git-ignored) can hold newer copies of shared headers from the project's branch while
  # this branch waits for a merge; it is searched first. Empty or missing in normal use.
  if (EXISTS ${ROOT}/build-pc/overlay)
    target_include_directories(${name} BEFORE PRIVATE ${ROOT}/build-pc/overlay)
  endif ()
  target_include_directories(${name} PRIVATE ${ROOT}/include ${ROOT}/build/GC6E01/include ${ROOT}
                             ${ROOT}/extern/aurora/include)
  target_compile_definitions(${name} PRIVATE TARGET_PC=1 BUILD_VERSION=0 VERSION_GC6E01 NDEBUG=1)
  # The SDK / game alias headers come first, as in the PC link (pc_probe.py --alias): HSD calls SDK functions by
  # GameCube address name, and its local declarations of them are hidden on the PC behind Aurora's prototypes.
  target_compile_options(${name} PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${ROOT}/port/src/sdk_pc_alias.h>"
    "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${ROOT}/include/pc/game_pc_alias.h>"
    "$<$<COMPILE_LANGUAGE:C>:SHELL:-include pc/prototypes.h>"
    "$<$<COMPILE_LANGUAGE:C>:-w>" "$<$<COMPILE_LANGUAGE:C>:-fno-strict-aliasing>")
  target_link_libraries(${name} PRIVATE ${gaps} orre_gs_thread_host orre_platform aurora::core aurora::gx aurora::main aurora::os aurora::vi)
  if (TARGET aurora::dvd)
    target_link_libraries(${name} PRIVATE aurora::dvd)
  endif ()
  orre_low_image(${name})
  if (MINGW)
    # -Wl,--allow-multiple-definition: include/crt/math_ppc.h's extern inlines (sinf, cosf, ...) are emitted in every
    # unit on the PC (identical copies; reported to Main)
    target_link_options(${name} PRIVATE -Wl,-u,WinMain -static-libgcc -static-libstdc++ -Wl,--allow-multiple-definition
                        -Wl,--wrap=GXCallDisplayList
                        -Wl,--wrap=GXSetTevColorIn -Wl,--wrap=GXSetTevAlphaIn -Wl,--wrap=GXSetTevOrder -Wl,--wrap=GXSetChanCtrl -Wl,--wrap=GXSetChanMatColor -Wl,--wrap=GXSetChanAmbColor -Wl,--wrap=GXLoadTexObj -Wl,--wrap=GXSetNumTevStages -Wl,--wrap=GXSetNumChans -Wl,--wrap=GXSetTevOp -Wl,--wrap=GXSetTexCoordGen2 -Wl,--wrap=GXLoadTexMtxImm
                        -Wl,--wrap=GXSetTevColorS10 -Wl,--wrap=GXSetTevColor -Wl,--wrap=GXSetTevKColor -Wl,--wrap=GXSetTevKColorSel
                        -Wl,--wrap=GXSetTevColorOp -Wl,--wrap=GXSetTevAlphaOp -Wl,--wrap=GXSetBlendMode -Wl,--wrap=GXSetTevSwapMode
                        -Wl,--wrap=GXSetTevSwapModeTable -Wl,--wrap=GXSetAlphaCompare -Wl,--wrap=GXSetCullMode -Wl,--wrap=GXSetFog
                        -Wl,--wrap=GXBegin -Wl,--wrap=GXSetVtxDesc -Wl,--wrap=GXSetVtxAttrFmt -Wl,--wrap=GXClearVtxDesc
                        -Wl,--wrap=GXSetColorUpdate -Wl,--wrap=GXSetAlphaUpdate -Wl,--wrap=GXSetZMode -Wl,--wrap=GXSetProjection -Wl,--wrap=GXLoadPosMtxImm -Wl,--wrap=GXSetCurrentMtx -Wl,--wrap=GXSetViewport -Wl,--wrap=GXSetScissor)
  endif ()
  aurora_copy_runtime_dlls(${name})
endfunction()

pc_hsd_program(pc_hsd_test pc_hsd_gaps main.c hsd_data_pc.c array_sizes.c ${HSD_UNITS} ${EXTRA_SOURCES})

# Second program: the game's own graphics layer (GSgfx, its layers, lights, cameras) on top of HSD: GSgfxInit and
# the frame cycle as the game's main loop runs them (main_gfx.c). Generated inputs: pc_gfx_data.c / pc_gfx_stubs.c.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/gfx_units.txt GFX_UNITS)
list(TRANSFORM GFX_UNITS PREPEND ${ROOT}/)
# what GSgfx needs from the rest of the game engine: GSmem, the scratch allocator, the math library, GStexture and
# the camera update (gfx_support_units.txt); models, particles and the debug log are stubbed for now
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/gfx_support_units.txt GFX_SUPPORT_UNITS)
list(TRANSFORM GFX_SUPPORT_UNITS PREPEND ${ROOT}/)
list(APPEND GFX_UNITS ${GFX_SUPPORT_UNITS})
# the game's model code (GSmodel, GSpart, GSmaterial): --model loads and draws through it
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/gfx_model_units.txt GFX_MODEL_UNITS)
list(TRANSFORM GFX_MODEL_UNITS PREPEND ${ROOT}/)
list(APPEND GFX_UNITS ${GFX_MODEL_UNITS})
# Test-local: pointer-type mismatches (f32* for Mtx, struct views) are reported but don't stop the build; the
# dangerous kinds (int / pointer conversions, implicit declarations) stay errors
set_source_files_properties(${GFX_MODEL_UNITS} PROPERTIES COMPILE_OPTIONS "-Wno-error=incompatible-pointer-types")
# the game data files that define what these units read (float constants, built-in vertex descriptors, render modes)
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/gfx_data_units.txt GFX_DATA_UNITS)
list(TRANSFORM GFX_DATA_UNITS PREPEND ${ROOT}/)
list(APPEND GFX_UNITS ${GFX_DATA_UNITS})
# Test-local: the math units hide their SDK declarations on the PC ("Aurora's apply") without including Aurora's
# header, so the SDK vector functions are implicitly declared (int returns; reported to Main). Built permissively
# until that is fixed: results of those SDK vector calls are not trustworthy in this test.
set_source_files_properties(${GFX_SUPPORT_UNITS} PROPERTIES COMPILE_OPTIONS "-fpermissive")
set(GFX_EXTRA)
if (EXISTS ${ROOT}/build-pc/generated/pc_gfx_data.c)
  list(APPEND GFX_EXTRA ${ROOT}/build-pc/generated/pc_gfx_data.c)
endif ()
add_library(pc_gfx_gaps STATIC platform_gaps.c gx_trace.c)
if (EXISTS ${ROOT}/build-pc/generated/pc_gfx_stubs.c)
  target_sources(pc_gfx_gaps PRIVATE ${ROOT}/build-pc/generated/pc_gfx_stubs.c)
endif ()
target_compile_options(pc_gfx_gaps PRIVATE -w)
pc_hsd_program(pc_gfx_test pc_gfx_gaps main_gfx.c hsd_data_pc.c array_sizes.c ${HSD_UNITS} ${GFX_UNITS} ${GFX_EXTRA})
