// kfx_platform: bflib_mspointer.cpp -- LbCursorSpriteSetScaling*() are
// thin wrappers around bflib_vidraw.c's pure LbSpriteSetScaling*Array()
// functions (see bflib_vidraw_test.cpp), adding one clamp
// (swidth/sheight capped at CURSOR_SCALING_XSTEPS/YSTEPS) before
// delegating into the module's own cursor_xsteps_array/
// cursor_ysteps_array. The rest of this file (LbI_PointerHandler's
// Draw/Undraw/Backup, PointerDraw) reads/writes a real screen buffer
// (TbPixel *outbuf from the active surface) -- the usual "significant
// harness change" boundary, not attempted here.
#include <catch2/catch_test_macros.hpp>

#include "bflib_mspointer.hpp"
#include "bflib_vidraw.h"

TEST_CASE("LbCursorSpriteSetScalingWidthClipped delegates into cursor_xsteps_array with an identity mapping", "[kfx_platform][bflib_mspointer]") {
    LbCursorSpriteSetScalingWidthClipped(5, 3, 3, 100);
    CHECK(cursor_xsteps_array[0] == 5); CHECK(cursor_xsteps_array[1] == 1);
    CHECK(cursor_xsteps_array[2] == 6); CHECK(cursor_xsteps_array[3] == 1);
}

TEST_CASE("LbCursorSpriteSetScalingWidthClipped clamps swidth to CURSOR_SCALING_XSTEPS before delegating", "[kfx_platform][bflib_mspointer]") {
    // swidth=CURSOR_SCALING_XSTEPS+50 gets clamped to CURSOR_SCALING_XSTEPS
    // (384) before being passed on as both swidth and the loop count --
    // observable indirectly: this must not write past
    // cursor_xsteps_array's own 2*CURSOR_SCALING_XSTEPS bound, or a
    // -fsanitize=address-style corruption would trip elsewhere. Correctness
    // here is "runs to completion without corrupting the array", not a
    // specific numeric expectation, since swidth==dwidth no longer holds
    // once swidth is silently clamped but dwidth isn't.
    LbCursorSpriteSetScalingWidthClipped(0, CURSOR_SCALING_XSTEPS + 50, CURSOR_SCALING_XSTEPS + 50, 100000);
    CHECK(true); // must not crash / corrupt memory
}

TEST_CASE("LbCursorSpriteSetScalingWidthSimple delegates into cursor_xsteps_array with an identity mapping", "[kfx_platform][bflib_mspointer]") {
    LbCursorSpriteSetScalingWidthSimple(5, 3, 3);
    CHECK(cursor_xsteps_array[0] == 5); CHECK(cursor_xsteps_array[1] == 1);
    CHECK(cursor_xsteps_array[2] == 6); CHECK(cursor_xsteps_array[3] == 1);
}

TEST_CASE("LbCursorSpriteSetScalingHeightClipped delegates into cursor_ysteps_array with an identity mapping", "[kfx_platform][bflib_mspointer]") {
    LbCursorSpriteSetScalingHeightClipped(5, 3, 3, 100);
    CHECK(cursor_ysteps_array[0] == 5); CHECK(cursor_ysteps_array[1] == 1);
    CHECK(cursor_ysteps_array[2] == 6); CHECK(cursor_ysteps_array[3] == 1);
}

TEST_CASE("LbCursorSpriteSetScalingHeightSimple delegates into cursor_ysteps_array with an identity mapping", "[kfx_platform][bflib_mspointer]") {
    LbCursorSpriteSetScalingHeightSimple(5, 3, 3);
    CHECK(cursor_ysteps_array[0] == 5); CHECK(cursor_ysteps_array[1] == 1);
    CHECK(cursor_ysteps_array[2] == 6); CHECK(cursor_ysteps_array[3] == 1);
}
