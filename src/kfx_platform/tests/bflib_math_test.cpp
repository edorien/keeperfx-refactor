// Pilot test for docs/refactor/testing/stage-01-framework-and-scaffold.md:
// proves the KFX_BUILD_TESTS/Catch2 scaffold actually builds and runs,
// against genuinely pure kfx_platform functions (no extern state, no SDL --
// see stage-02-testability-and-fakes.md for the patterns needed once tests
// stop being this simple).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "bflib_math.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("LbSqrL returns the integer square root", "[kfx_platform][bflib_math]") {
    CHECK(LbSqrL(4) == 2);
    CHECK(LbSqrL(16) == 4);
    CHECK(LbSqrL(0) == 0);
}

TEST_CASE("LbSqrL treats non-positive input as zero", "[kfx_platform][bflib_math]") {
    CHECK(LbSqrL(-1) == 0);
    CHECK(LbSqrL(0) == 0);
}

TEST_CASE("LbLerp interpolates linearly between two values", "[kfx_platform][bflib_math]") {
    CHECK_THAT(LbLerp(0.0f, 10.0f, 0.5f), WithinAbs(5.0f, 0.0001f));
    CHECK_THAT(LbLerp(2.0f, 4.0f, 0.0f), WithinAbs(2.0f, 0.0001f));
    CHECK_THAT(LbLerp(2.0f, 4.0f, 1.0f), WithinAbs(4.0f, 0.0001f));
}

// LbMathOperation is kfx_game's lvl_script_conditions.c::get_condition_status's
// entire implementation ("return LbMathOperation(opkind, left, right) != 0"),
// making this the real target behind every level script CONDITION command's
// comparison -- tested here in kfx_platform, where the function actually
// lives, rather than duplicated as a kfx_game test of a one-line wrapper.
TEST_CASE("LbMathOperation evaluates the comparison opkinds", "[kfx_platform][bflib_math]") {
    CHECK(LbMathOperation(MOp_EQUAL, 5, 5) == 1);
    CHECK(LbMathOperation(MOp_EQUAL, 5, 6) == 0);
    CHECK(LbMathOperation(MOp_NOT_EQUAL, 5, 6) == 1);
    CHECK(LbMathOperation(MOp_SMALLER, 5, 6) == 1);
    CHECK(LbMathOperation(MOp_SMALLER, 6, 5) == 0);
    CHECK(LbMathOperation(MOp_GREATER, 6, 5) == 1);
    CHECK(LbMathOperation(MOp_SMALLER_EQ, 5, 5) == 1);
    CHECK(LbMathOperation(MOp_GREATER_EQ, 5, 5) == 1);
}

TEST_CASE("LbMathOperation evaluates the logic opkinds as booleans, not raw values", "[kfx_platform][bflib_math]") {
    CHECK(LbMathOperation(MOp_LOGIC_AND, 2, 3) == 1); // both non-zero -> 1, not 2 or 3
    CHECK(LbMathOperation(MOp_LOGIC_AND, 0, 3) == 0);
    CHECK(LbMathOperation(MOp_LOGIC_OR, 0, 3) == 1);
    CHECK(LbMathOperation(MOp_LOGIC_OR, 0, 0) == 0);
    CHECK(LbMathOperation(MOp_LOGIC_XOR, 2, 0) == 1);
    CHECK(LbMathOperation(MOp_LOGIC_XOR, 2, 3) == 0);
}

TEST_CASE("LbMathOperation evaluates the bitwise and arithmetic opkinds", "[kfx_platform][bflib_math]") {
    CHECK(LbMathOperation(MOp_BITWS_AND, 6, 3) == 2);
    CHECK(LbMathOperation(MOp_BITWS_OR, 6, 1) == 7);
    CHECK(LbMathOperation(MOp_BITWS_XOR, 6, 3) == 5);
    CHECK(LbMathOperation(MOp_SUM, 5, 3) == 8);
    CHECK(LbMathOperation(MOp_SUBTRACT, 5, 3) == 2);
    CHECK(LbMathOperation(MOp_MULTIPLY, 5, 3) == 15);
    CHECK(LbMathOperation(MOp_DIVIDE, 9, 3) == 3);
    CHECK(LbMathOperation(MOp_MODULO, 10, 3) == 1);
}

TEST_CASE("LbMathOperation falls back to first_operand for an unrecognized opkind", "[kfx_platform][bflib_math]") {
    CHECK(LbMathOperation(MOp_UNDEFINED, 42, 7) == 42);
}
