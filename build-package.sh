#!/usr/bin/env bash
#
# Full local build + package assembly: compiles keeperfx and keeperfx_hvlog,
# runs the make asset/data pipeline, and installs everything (binary +
# runtime libs + game data) into dist/<platform>/. Generalizes what CI's
# release workflows do (.github/workflows/build-*.yml) for local iteration,
# reusing build-cmake.sh's same out/<KFX_OS>/ build tree.
#
#   KFX_OS=windows (default)  32-bit MinGW-w64 (i686) cross-compile
#   KFX_OS=linux               native x86_64 Linux
#
# Network access is needed: pkg-enginegfx clones dkfans/FXGraphics, and a
# first run fetches every third-party dependency (see build-cmake.sh).
#
# Usage:
#   ./build-package.sh                              # Windows, full package
#   KFX_OS=linux ./build-package.sh                 # Linux, full package
#   BUILD_NUMBER=1234 PACKAGE_SUFFIX=Alpha ./build-package.sh
#   BUILD_DIR=out/foo ./build-package.sh             # override the build tree
#
# Requirements: same as build-cmake.sh, plus a real `make` (mingw32-make's
# actual usual name; the asset pipeline's own Makefile is platform-agnostic).
#
set -euo pipefail

KFX_OS="${KFX_OS:-windows}"
BUILD_DIR="${BUILD_DIR:-out/$KFX_OS}"
DIST_DIR="dist/$KFX_OS"
BUILD_NUMBER="${BUILD_NUMBER:-$(git rev-list --count HEAD)}"
PACKAGE_SUFFIX="${PACKAGE_SUFFIX:-}"

echo "==> Building engine graphics (make pkg-enginegfx)"
make BUILD_NUMBER="$BUILD_NUMBER" PACKAGE_SUFFIX="$PACKAGE_SUFFIX" pkg-enginegfx

echo "==> Configuring ($KFX_OS)"
if [ "$KFX_OS" = "linux" ]; then
    cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DBUILD_NUMBER="$BUILD_NUMBER" -DPACKAGE_SUFFIX="$PACKAGE_SUFFIX"
else
    if ! command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
        echo "error: i686-w64-mingw32-gcc not found on PATH." >&2
        echo "       Install a MinGW-w64 i686 toolchain (Ubuntu: 'sudo apt install g++-mingw-w64-i686')," >&2
        echo "       or KFX_OS=linux for a native Linux build." >&2
        exit 1
    fi
    cmake -S . -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=build/cmake/toolchains/mingw32.cmake \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DBUILD_NUMBER="$BUILD_NUMBER" -DPACKAGE_SUFFIX="$PACKAGE_SUFFIX"
fi

echo "==> Building keeperfx + keeperfx_hvlog"
cmake --build "$BUILD_DIR" --target keeperfx keeperfx_hvlog -j"$(nproc 2>/dev/null || echo 4)"

echo "==> Assembling game data (make pkg-assemble)"
make BUILD_NUMBER="$BUILD_NUMBER" PACKAGE_SUFFIX="$PACKAGE_SUFFIX" pkg-assemble

# Two separate --component calls, not a plain `cmake --install`: the latter
# would also run every install() rule the fetched SDL3 subprojects register
# for themselves (headers, cmake config, docs, ...) -- see Packaging.cmake's
# COMPONENT runtime/gamedata comments.
echo "==> Installing to $DIST_DIR"
cmake --install "$BUILD_DIR" --prefix "$DIST_DIR" --component runtime >/dev/null
cmake --install "$BUILD_DIR" --prefix "$DIST_DIR" --component gamedata >/dev/null

echo
echo "Packaged: $DIST_DIR/"
