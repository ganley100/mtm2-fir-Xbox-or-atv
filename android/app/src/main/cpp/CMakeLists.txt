# The native half of the APK: SDL2 (shared, it carries SDLActivity's JNI),
# SDL2_ttf (static, with its own FreeType) and the game, libmain.so, built by
# the repository's CMakeLists.txt with MTM_HOST=win32hle.
#
# SDL2 and SDL2_ttf are fetched into android/deps by android/fetch-deps.sh;
# pcrecomp sits beside this repository, as for the Windows build.
cmake_minimum_required(VERSION 3.20)
project(mtm_android C)

get_filename_component(MTM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(DEPS "${MTM_ROOT}/android/deps")
if(NOT EXISTS "${DEPS}/SDL/CMakeLists.txt" OR NOT EXISTS "${DEPS}/SDL_ttf/CMakeLists.txt")
  message(FATAL_ERROR "SDL2 and SDL2_ttf are not in ${DEPS}: run android/fetch-deps.sh")
endif()

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${DEPS}/SDL" SDL)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${DEPS}/SDL_ttf" SDL_ttf)
# SDL2_ttf's own build exposes SDL_ttf.h without the SDL2/ prefix that
# win32hle includes it by (<SDL2/SDL_ttf.h>, as an installed SDL2_ttf has it).
configure_file("${DEPS}/SDL_ttf/SDL_ttf.h" "${CMAKE_BINARY_DIR}/ttf-include/SDL2/SDL_ttf.h" COPYONLY)
target_include_directories(SDL2_ttf INTERFACE "${CMAKE_BINARY_DIR}/ttf-include")

add_subdirectory("${MTM_ROOT}" mtm)
