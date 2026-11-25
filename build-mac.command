#!/bin/bash
# Builds Kalibre Player on this Mac (needs Xcode or its Command Line Tools,
# and CMake). Double-click it in the Finder, or run it in Terminal. The first
# build also builds FFmpeg, LAME and libopus, which takes a few minutes once.
# The app lands in build/KalibrePlayer_artefacts/Release/Kalibre Player.app.
set -e
cd "$(dirname "$0")"
if ! command -v cmake >/dev/null; then
  echo "CMake is needed: brew install cmake (https://brew.sh), or download it from cmake.org."
  exit 1
fi
if ! command -v pkg-config >/dev/null; then
  echo "pkg-config is needed: brew install pkgconf"
  exit 1
fi
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --target KalibrePlayer --config Release -j "$(sysctl -n hw.ncpu)"
open build/KalibrePlayer_artefacts/Release
