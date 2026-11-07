#!/bin/sh
# Fetch SDL2 and SDL2_ttf (with its FreeType) into android/deps for the APK
# build. Run once, from anywhere.
set -eu
cd "$(dirname "$0")"
mkdir -p deps
if [ ! -d deps/SDL ]; then
  git clone --depth 1 --branch release-2.30.9 https://github.com/libsdl-org/SDL deps/SDL
fi
if [ ! -d deps/SDL_ttf ]; then
  git clone --depth 1 --branch release-2.22.0 https://github.com/libsdl-org/SDL_ttf deps/SDL_ttf
  (cd deps/SDL_ttf && git submodule update --init --depth 1 external/freetype)
fi
echo "deps ready: $(pwd)/deps"
