// kfx_game: lvl_script_conditions.c, per docs/refactor/testing/
// stage-08-comprehensive-library-passes.md's kfx_game row.
// condition_inactive() is a bounds-checked bitflag read over
// kfx_game_state.script.conditions[] -- pattern A, same shape as
// game_legacy_test.cpp's kfx_game_state fixture.
//
// get_condition_status() is deliberately NOT retested here: its entire
// body is "return LbMathOperation(opkind, left, right) != 0", so the
// real comparison logic is tested directly in kfx_platform's
// bflib_math_test.cpp, where LbMathOperation actually lives, rather than
// duplicated as a test of a one-line wrapper.
#include <catch2/catch_test_macros.hpp>

#include "lvl_script.h"
#include "lvl_script_conditions.h"
#include "kfx_game_state.h"

#include <cstring>

namespace {
struct ResetGameState {
    ResetGameState() { std::memset(&kfx_game_state, 0, sizeof(kfx_game_state)); }
};
}

TEST_CASE_METHOD(ResetGameState, "condition_inactive is true when the 0x01 status bit is unset", "[kfx_game][lvl_script_conditions]") {
    kfx_game_state.script.conditions[0].status = 0x00;
    CHECK(condition_inactive(0));
}

TEST_CASE_METHOD(ResetGameState, "condition_inactive is true when the 0x04 status bit is set, even alongside 0x01", "[kfx_game][lvl_script_conditions]") {
    kfx_game_state.script.conditions[0].status = 0x01 | 0x04;
    CHECK(condition_inactive(0));
}

TEST_CASE_METHOD(ResetGameState, "condition_inactive is false when 0x01 is set and 0x04 is clear", "[kfx_game][lvl_script_conditions]") {
    kfx_game_state.script.conditions[0].status = 0x01;
    CHECK_FALSE(condition_inactive(0));
}

TEST_CASE("condition_inactive returns false (not inactive) for an out-of-range index", "[kfx_game][lvl_script_conditions]") {
    // Counterintuitive on a first read -- an out-of-bounds index reports
    // "not inactive" rather than "inactive", the opposite of what a
    // fail-safe bounds check would usually return. Tested as the
    // function's actual behavior, not silently corrected, same "test
    // what's there" call stage-04-kfx-config.md made for
    // parameter_is_number's lone-"-" quirk.
    CHECK_FALSE(condition_inactive(-1));
    CHECK_FALSE(condition_inactive(CONDITIONS_COUNT));
}
