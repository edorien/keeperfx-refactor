// Every kfx_*_utest binary transitively links kfx_platform (directly, or
// via another library that depends on it), and kfx_platform/src/kfx/
// platform/PlatformLinux.cpp physically owns the process main() (by
// design -- see docs/refactor/todo/check-layering-symbol-level-blind-spot.md's
// item 3), which calls kfxmain(argc, argv) -- normally main.cpp's real
// game bootstrap. This forwards it into Catch2's session runner instead;
// every *_utest target links this (via the kfx_test_main library, below)
// and plain Catch2::Catch2, not Catch2::Catch2WithMain, which would define
// its own conflicting `main` that a linker with kfx_platform's object
// files already present would just silently drop (see stage-04's note in
// docs/refactor/testing/stage-01-framework-and-scaffold.md for how that
// manifested: not a duplicate-symbol error, an unresolved kfxmain one).
#include "platform.h"

#include <catch2/catch_session.hpp>

extern "C" int kfxmain(int argc, char *argv[])
{
    return Catch::Session().run(argc, argv);
}
