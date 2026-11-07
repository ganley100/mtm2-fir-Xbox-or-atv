# The Xbox (UWP x64) app: SDL2_ttf (static, with its own FreeType) and the
# game, mtm.exe, built by the repository's CMakeLists.txt with MTM_XBOX=ON,
# all with clang-cl, against an SDL2 built for UWP with MSVC beforehand (its
# WinRT backend is C++/CX). xbox/build.ps1 runs both steps.
#
#   -DSDL2_UWP_BUILD=<SDL2's UWP build folder>  -DSDL2_SOURCE=<SDL2's source>
cmake_minimum_required(VERSION 3.20)
project(mtm_xbox C)

get_filename_component(MTM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
foreach(v SDL2_UWP_BUILD SDL2_SOURCE SDL_TTF_SOURCE)
  if(NOT ${v})
    message(FATAL_ERROR "${v} is not set (xbox/build.ps1 sets it)")
  endif()
endforeach()

# Everything here is a UWP app's code.
add_compile_definitions(WINAPI_FAMILY=WINAPI_FAMILY_APP _WIN32_WINNT=0x0A00 _CRT_SECURE_NO_WARNINGS)

# SDL2 as built for UWP: the DLL, its import library and its headers (the
# build folder has them under include/SDL2, as win32hle includes them).
find_file(SDL2_DLL SDL2.dll PATHS "${SDL2_UWP_BUILD}/Release" "${SDL2_UWP_BUILD}" NO_DEFAULT_PATH REQUIRED)
find_file(SDL2_LIB SDL2.lib PATHS "${SDL2_UWP_BUILD}/Release" "${SDL2_UWP_BUILD}" NO_DEFAULT_PATH REQUIRED)
add_library(SDL2 SHARED IMPORTED GLOBAL)
set_target_properties(SDL2 PROPERTIES
  IMPORTED_LOCATION "${SDL2_DLL}"
  IMPORTED_IMPLIB "${SDL2_LIB}"
  INTERFACE_INCLUDE_DIRECTORIES "${SDL2_UWP_BUILD}/include;${SDL2_UWP_BUILD}/include/SDL2;${SDL2_UWP_BUILD}/include-config-release/SDL2")
add_library(SDL2::SDL2 ALIAS SDL2)
add_library(SDL2::SDL2-static ALIAS SDL2)   # what SDL2_ttf built static looks for

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${SDL_TTF_SOURCE}" SDL_ttf)
configure_file("${SDL_TTF_SOURCE}/SDL_ttf.h" "${CMAKE_BINARY_DIR}/ttf-include/SDL2/SDL_ttf.h" COPYONLY)
target_include_directories(SDL2_ttf INTERFACE "${CMAKE_BINARY_DIR}/ttf-include")

set(MTM_HOST win32hle CACHE STRING "" FORCE)   # clang-cl looks like MSVC, which would pick native32
set(MTM_XBOX ON CACHE BOOL "" FORCE)
add_subdirectory("${MTM_ROOT}" mtm)
