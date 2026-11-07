# =============================================================================
# Monster Truck Madness 2 - static recompilation host
#
#   py -3 run_lift.py        # -> src/recomp/gen/  (needs work/functions_monster.json)
#
# Two hosts run the same lift:
#
#   native32  Windows, 32-bit MSVC x86 (build.cmd -> build/mtm.exe). Imports go
#             to the real Windows. The default with MSVC.
#   win32hle  Android TV, Xbox Dev Mode and Linux (SDL2 + SDL2_ttf). pcrecomp's
#             win32hle answers the imports itself. The default with any other
#             compiler. android/ builds it into an APK (docs/android.md);
#             -DMTM_XBOX=ON with clang-cl builds the Xbox (UWP x64) app that
#             xbox/ packages (docs/xbox.md).
#
# -DMTM_NO_LIFT=ON builds without src/recomp/gen (a placeholder dispatch that
# refuses to run), to check a toolchain before the long compile of the lift.
#
# -DMTM_VARIANT=demo builds the 1998 demo instead of the full game: its lift is
# src/recomp/gen-demo (run_lift.py --demo), its files game-demo/
# (tools/variant.py). build.cmd takes BUILD_DIR=build-demo CMAKE_ARGS=-DMTM_VARIANT=demo.
# =============================================================================
cmake_minimum_required(VERSION 3.20)
project(mtm C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(MTM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(PCRECOMP "${MTM_ROOT}/../pcrecomp" CACHE PATH "pcrecomp checkout")
if(NOT EXISTS "${PCRECOMP}/runtime/recomp32/recomp_types.h")
  message(FATAL_ERROR "no pcrecomp at ${PCRECOMP}: clone it beside this repo or pass -DPCRECOMP=")
endif()
if(MSVC)
  set(MTM_HOST_DEFAULT native32)
else()
  set(MTM_HOST_DEFAULT win32hle)
endif()
set(MTM_HOST "${MTM_HOST_DEFAULT}" CACHE STRING "native32 (Windows) or win32hle (Android, Linux)")
set_property(CACHE MTM_HOST PROPERTY STRINGS native32 win32hle)
option(MTM_NO_LIFT "build without the lifted code, to check a toolchain" OFF)
set(MTM_VARIANT full CACHE STRING "full (the game) or demo (the 1998 demo)")
set_property(CACHE MTM_VARIANT PROPERTY STRINGS full demo)
if(MTM_VARIANT STREQUAL "full")
  set(GEN_DIR "${MTM_ROOT}/src/recomp/gen")
  set(MTM_GAME_DIR game)
  set(MTM_TITLE "Monster Truck Madness 2")
  set(LIFT_CMD "py -3 run_lift.py")
elseif(MTM_VARIANT STREQUAL "demo")
  set(GEN_DIR "${MTM_ROOT}/src/recomp/gen-demo")
  set(MTM_GAME_DIR game-demo)
  set(MTM_TITLE "Monster Truck Madness 2 Demo")
  set(LIFT_CMD "py -3 run_lift.py --demo")
else()
  message(FATAL_ERROR "MTM_VARIANT is full or demo, not ${MTM_VARIANT}")
endif()

if(MTM_NO_LIFT)
  set(GEN_SOURCES "${MTM_ROOT}/src/portable/no_lift.c")
else()
  file(GLOB GEN_SOURCES CONFIGURE_DEPENDS "${GEN_DIR}/*.c")
  if(NOT GEN_SOURCES)
    message(FATAL_ERROR "${GEN_DIR} is empty -- run `${LIFT_CMD}` first")
  endif()
endif()

option(MTM_TRACE "record a ring buffer of lifted function entries" OFF)

# =============================================================================
if(MTM_HOST STREQUAL "native32")
# =============================================================================

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "the host must be 32-bit: use build.cmd (vcvarsall amd64_x86)")
endif()
if(NOT MSVC)
  message(FATAL_ERROR "runtime/native32 is MSVC x86 inline assembly")
endif()

set(HOST_SOURCES src/runtime/host.c src/runtime/ddraw.c src/runtime/present.c src/runtime/pad.c
                 src/pad/pad_map.c)
add_executable(mtm
  ${HOST_SOURCES}
  ${PCRECOMP}/runtime/native32/native32.c
  ${PCRECOMP}/runtime/recomp32/image_loader.c
  ${PCRECOMP}/runtime/recomp32/recomp_trace.c
  ${GEN_SOURCES}
)
target_include_directories(mtm PRIVATE
  src/runtime src/pad ${GEN_DIR} ${PCRECOMP}/runtime/native32 ${PCRECOMP}/runtime/recomp32)
# RECOMP_RETADDR=0u: a stack imbalance during bring-up leaks a harmless 0, not
# 0xDEAD0000 into a size argument (forcecommander).
target_compile_definitions(mtm PRIVATE RECOMP_RETADDR=0u _CRT_SECURE_NO_WARNINGS
                                                      RECOMP_FLAT_MEMORY RECOMP_LOCAL_REGS)
# Linked high and not relocatable, so Monster.exe's 0x00400000 is free for the
# guest image.
target_link_options(mtm PRIVATE /BASE:0x60000000 /FIXED:NO /DYNAMICBASE:NO
                                               /NXCOMPAT /STACK:0x1000000 /MANIFESTUAC:NO)
set(GEN_OPT "/O2" CACHE STRING "optimisation for the generated C")
set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "/w;${GEN_OPT}")
set_source_files_properties(${HOST_SOURCES} PROPERTIES COMPILE_OPTIONS "/W3")
target_link_libraries(mtm PRIVATE user32 gdi32 kernel32 advapi32 winmm dwmapi d3d11 dxgi dxguid)

# The controller mapping goes beside mtm.exe, where pad.c looks for it. Copied
# only when missing, so edits made to the build's copy survive a rebuild.
if(NOT EXISTS "${CMAKE_BINARY_DIR}/controller.ini")
  file(COPY "${MTM_ROOT}/config/controller.ini" DESTINATION "${CMAKE_BINARY_DIR}")
endif()

if(MTM_TRACE)
  target_compile_definitions(mtm PRIVATE RECOMP_TRACE=1)
endif()

# =============================================================================
elseif(MTM_HOST STREQUAL "win32hle")
# =============================================================================

set(HLE "${PCRECOMP}/runtime/win32hle")
file(GLOB HLE_SOURCES "${HLE}/*.c")
list(FILTER HLE_SOURCES EXCLUDE REGEX "_selftest\\.c$")
option(MTM_XBOX "Xbox Dev Mode: a UWP x64 app (clang-cl, src/xbox/compat)" OFF)
if(MTM_XBOX)
  if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "MTM_XBOX builds with clang-cl for x64 (xbox/build.ps1 sets it up)")
  endif()
  # MTM2 imports no Winsock, and wsock32.c is BSD sockets.
  list(REMOVE_ITEM HLE_SOURCES "${HLE}/wsock32.c")
  # kernel32_ext.c reads glibc's tm_gmtoff once; the CRT's struct tm has none.
  list(REMOVE_ITEM HLE_SOURCES "${HLE}/kernel32_ext.c")
  file(READ "${HLE}/kernel32_ext.c" K32X)
  string(FIND "${K32X}" "(int32_t)t.tm_gmtoff" AT)
  if(AT EQUAL -1)
    message(FATAL_ERROR "kernel32_ext.c no longer reads tm_gmtoff as it did: update the rewrite in CMakeLists.txt")
  endif()
  string(REPLACE "(int32_t)t.tm_gmtoff" "(int32_t)compat_gmtoff()" K32X "${K32X}")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/hle/kernel32_ext.c.new" "${K32X}")
  configure_file("${CMAKE_CURRENT_BINARY_DIR}/hle/kernel32_ext.c.new" "${CMAKE_CURRENT_BINARY_DIR}/hle/kernel32_ext.c" COPYONLY)
  list(APPEND HLE_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/hle/kernel32_ext.c")
endif()

# Fonts. gdidc.c finds the faces the game asks for (Arial, MS Sans Serif) as
# Liberation fonts through fontconfig or the usual Linux font folders. Android
# has neither, so the build compiles a copy that looks first in the game
# folder's fonts/ (mtm_font_dir, set by mtm_hle.c), then /system/fonts, with
# Roboto as the last resort where DejaVu Sans would be.
list(REMOVE_ITEM HLE_SOURCES "${HLE}/gdidc.c")
file(READ "${HLE}/gdidc.c" GDIDC)
set(DIRS_OLD "static const char *const dirs[] = { ")
string(FIND "${GDIDC}" "${DIRS_OLD}" AT)
if(AT EQUAL -1)
  message(FATAL_ERROR "gdidc.c's font search has changed: update the rewrite in CMakeLists.txt")
endif()
string(REPLACE "${DIRS_OLD}" "extern char mtm_font_dir[];\n    static const char *const dirs[] = { mtm_font_dir, \"/system/fonts/\", " GDIDC "${GDIDC}")
if(ANDROID)
  string(REPLACE "\"DejaVuSans-Bold.ttf\" : \"DejaVuSans.ttf\"" "\"Roboto-Regular.ttf\" : \"Roboto-Regular.ttf\"" GDIDC "${GDIDC}")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/hle/gdidc.c.new" "${GDIDC}")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/hle/gdidc.c.new" "${CMAKE_CURRENT_BINARY_DIR}/hle/gdidc.c" COPYONLY)
list(APPEND HLE_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/hle/gdidc.c")

set(HOST_SOURCES src/portable/mtm_hle.c src/portable/pad_sdl.c src/pad/pad_map.c)
if(MTM_XBOX)
  list(APPEND HOST_SOURCES src/xbox/xbox_main.c src/xbox/compat/compat.c src/xbox/no_wsock.c)
endif()
if(ANDROID)
  # SDLActivity loads the game as libmain.so and calls its SDL_main.
  add_library(mtm SHARED ${HOST_SOURCES} ${HLE_SOURCES} ${GEN_SOURCES})
  set_target_properties(mtm PROPERTIES OUTPUT_NAME main)
  target_link_libraries(mtm PRIVATE log android)
elseif(MTM_XBOX)
  add_executable(mtm WIN32 ${HOST_SOURCES} ${HLE_SOURCES} ${GEN_SOURCES})
else()
  add_executable(mtm ${HOST_SOURCES} ${HLE_SOURCES} ${GEN_SOURCES})
endif()

if(TARGET SDL2 AND TARGET SDL2_ttf)      # built alongside (android/)
  target_link_libraries(mtm PRIVATE SDL2 SDL2_ttf)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL REQUIRED IMPORTED_TARGET sdl2 SDL2_ttf)
  target_link_libraries(mtm PRIVATE PkgConfig::SDL)
endif()
if(MTM_XBOX)
  # A UWP app: the app API family, POSIX from src/xbox/compat (force-included
  # into win32hle and the host, ahead of the CRT's headers), linked as an app
  # container against WindowsApp.lib and the Store CRT (vcruntime140_app.dll,
  # which the package takes from Microsoft.VCLibs).
  target_compile_definitions(mtm PRIVATE MTM_XBOX WINAPI_FAMILY=WINAPI_FAMILY_APP _WIN32_WINNT=0x0A00
                                         _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
  target_include_directories(mtm BEFORE PRIVATE src/xbox/compat src/xbox)
  if(NOT DEFINED ENV{VCToolsInstallDir})
    message(FATAL_ERROR "run from a Visual Studio x64 developer prompt (VCToolsInstallDir is not set)")
  endif()
  file(TO_CMAKE_PATH "$ENV{VCToolsInstallDir}" VCTOOLS)
  target_link_options(mtm PRIVATE /APPCONTAINER /SUBSYSTEM:WINDOWS /STACK:0x1000000
                      "/LIBPATH:${VCTOOLS}/lib/x64/store" /NODEFAULTLIB:kernel32.lib /NODEFAULTLIB:user32.lib)
  target_link_libraries(mtm PRIVATE WindowsApp.lib)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(mtm PRIVATE Threads::Threads m)
endif()

# On 32-bit ARM the lift sees an alignment-safe recomp_types.h first
# (src/portable/arm32/recomp_types.h explains).
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|ARM)")
  target_include_directories(mtm BEFORE PRIVATE src/portable/arm32)
endif()
target_include_directories(mtm PRIVATE
  src/portable src/pad ${GEN_DIR} ${HLE} ${PCRECOMP}/runtime/recomp32)
target_compile_definitions(mtm PRIVATE RECOMP_RETADDR=0u RECOMP_LOCAL_REGS)
# A 32-bit host (armeabi-v7a, Linux -m32) maps the guest where it asks: a
# guest address is a host address. A 64-bit one keeps the guest's 4 GB in one
# block (win32hle/README.md).
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  target_compile_definitions(mtm PRIVATE RECOMP_FLAT_MEMORY)
endif()
set(GEN_OPT "-O2" CACHE STRING "optimisation for the generated C")
if(MSVC)                                 # clang-cl (Xbox): its own spelling
  set(GEN_OPT "/O2" CACHE STRING "optimisation for the generated C" FORCE)
  set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "/w;${GEN_OPT};-fno-strict-aliasing")
  set_source_files_properties(${HLE_SOURCES} PROPERTIES COMPILE_OPTIONS "/w;/O2;-fno-strict-aliasing")
  set_source_files_properties(${HOST_SOURCES} PROPERTIES COMPILE_OPTIONS "/W3;/O2")
else()
  set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "-w;${GEN_OPT};-fno-strict-aliasing")
  set_source_files_properties(${HLE_SOURCES} PROPERTIES COMPILE_OPTIONS "-w;-O2;-fno-strict-aliasing")
  set_source_files_properties(${HOST_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-O2")
endif()
if(MTM_XBOX)                             # after the options above, which would replace it
  set_property(SOURCE ${HLE_SOURCES} src/portable/mtm_hle.c src/portable/pad_sdl.c src/xbox/xbox_main.c
               src/xbox/no_wsock.c APPEND PROPERTY COMPILE_OPTIONS "/FI${MTM_ROOT}/src/xbox/compat/xbox_compat.h")
endif()

if(MTM_TRACE)
  target_compile_definitions(mtm PRIVATE RECOMP_TRACE=1)
endif()

else()
  message(FATAL_ERROR "MTM_HOST is native32 or win32hle, not ${MTM_HOST}")
endif()

# The game folder a run uses without --game, and the window's title.
target_compile_definitions(mtm PRIVATE "MTM_GAME_DIR=\"${MTM_GAME_DIR}\"" "MTM_TITLE=\"${MTM_TITLE}\"")
