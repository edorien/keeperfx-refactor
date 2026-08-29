// kfx_platform: bflib_planar.c, per docs/refactor/testing/comprehensive/
// stage-08-comprehensive-library-passes.md's kfx_platform row -- pure
// planar-geometry candidates in the LbSqrL/bflib_string.c vein.
//
// distance_with_angle_to_coord_x/y, move_coord_with_angle_x/y,
// distance3d_with_angles_to_coord_x/y, and get_distance_xy are
// deliberately NOT covered here: their exact output depends on
// LbSinL/LbCosL/LbDiagonalLength's fixed-point lookup tables, so a hard-
// coded expected value would just be a second, easier-to-get-wrong copy
// of the table data -- the same "don't assert what you'd have to
// reimplement to check" call net_checksums_test.cpp made for
// get_thing_checksum. get_angle_difference/get_angle_sign and the
// chessboard-distance pair below are pure integer arithmetic with no
// table lookup, so their exact outputs are safe to assert directly.
#include <catch2/catch_test_macros.hpp>

#include "bflib_planar.h"
#include "globals.h"

TEST_CASE("LbSetRect assigns all four fields", "[kfx_platform][bflib_planar]") {
    struct TbRect rect;
    LbSetRect(&rect, 1, 2, 3, 4);
    CHECK(rect.left == 1);
    CHECK(rect.top == 2);
    CHECK(rect.right == 3);
    CHECK(rect.bottom == 4);
}

TEST_CASE("LbSetRect does nothing for a NULL rect", "[kfx_platform][bflib_planar]") {
    LbSetRect(NULL, 1, 2, 3, 4); // must not crash
}

TEST_CASE("get_angle_difference returns the short-way unsigned difference", "[kfx_platform][bflib_planar]") {
    CHECK(get_angle_difference(0, 0) == 0);
    CHECK(get_angle_difference(0, DEGREES_90) == DEGREES_90);
    CHECK(get_angle_difference(DEGREES_90, 0) == DEGREES_90); // symmetric
    // 45 deg and 315 deg are 90 deg apart going the short way around,
    // not the 270 deg raw difference.
    CHECK(get_angle_difference(DEGREES_45, DEGREES_315) == DEGREES_90);
}

TEST_CASE("get_angle_difference masks angles past a full turn", "[kfx_platform][bflib_planar]") {
    CHECK(get_angle_difference(DEGREES_360, 0) == 0);
    CHECK(get_angle_difference(DEGREES_360 + DEGREES_90, 0) == DEGREES_90);
}

TEST_CASE("get_angle_sign returns 0 for equal angles, including after masking", "[kfx_platform][bflib_planar]") {
    CHECK(get_angle_sign(DEGREES_90, DEGREES_90) == 0);
    CHECK(get_angle_sign(0, DEGREES_360) == 0);
}

TEST_CASE("get_angle_sign is positive when b is ahead of a the short way", "[kfx_platform][bflib_planar]") {
    CHECK(get_angle_sign(0, DEGREES_90) == 1);
    CHECK(get_angle_sign(DEGREES_90, 0) == -1); // and negative the other way
}

TEST_CASE("get_angle_sign picks the short way around the wrap point", "[kfx_platform][bflib_planar]") {
    // Going from 45 to 315 the raw difference is +270, but the short way
    // around (through 0) is -90 -- get_angle_sign must report that,
    // not the raw direction.
    CHECK(get_angle_sign(DEGREES_45, DEGREES_315) == -1);
    CHECK(get_angle_sign(DEGREES_315, DEGREES_45) == 1);
}

TEST_CASE("get_chessboard_distance is the max of the axis-aligned deltas", "[kfx_platform][bflib_planar]") {
    struct Coord3d pos1{};
    struct Coord3d pos2{};
    pos1.x.val = 0;
    pos1.y.val = 0;
    pos2.x.val = 3;
    pos2.y.val = 7;
    CHECK(get_chessboard_distance(&pos1, &pos2) == 7);
}

TEST_CASE("get_chessboard_3d_distance also considers the z delta", "[kfx_platform][bflib_planar]") {
    struct Coord3d pos1{};
    struct Coord3d pos2{};
    pos1.x.val = 0;
    pos1.y.val = 0;
    pos1.z.val = 0;
    pos2.x.val = 3;
    pos2.y.val = 7;
    pos2.z.val = 20;
    CHECK(get_chessboard_3d_distance(&pos1, &pos2) == 20);
}
