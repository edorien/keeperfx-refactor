// kfx_sim "creature" cluster, per docs/refactor/testing/comprehensive/
// stage-08-comprehensive-library-passes.md §3: the largest cluster by
// file count, flagged as the strongest fan-in/risk argument for going
// first. Starting with creature_control.c's own accessors -- the same
// "index 0 reserved sentinel" pattern as thing_data.c's
// thing_is_invalid/thing_exists/thing_get (tested in thing_data_test.cpp)
// -- before anything in the creature_states_*.c state-machine files,
// which need a fuller Thing+CreatureControl pair set up first.
//
// Worth noting explicitly (found by reading the body, not assumed from
// the name): creature_control_invalid() only checks the *lower* bound
// (`cctrl <= &kfx_sim_state.cctrl_data[0]`) -- unlike thing_is_invalid(),
// it has no upper-bound check against CREATURES_COUNT. Tested as the
// function's actual behavior below, not "fixed" as part of writing a
// test for it -- the same "test what's there, not what you'd expect"
// discipline stage-04-kfx-config.md's `parameter_is_number` quirk
// followed.
#include <catch2/catch_test_macros.hpp>

#include "creature_control.h"
#include "thing_data.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct ResetSimState {
    ResetSimState() { std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state)); }
};
}

TEST_CASE_METHOD(ResetSimState, "creature_control_invalid rejects null and the reserved index-0 sentinel", "[kfx_sim][creature_control]") {
    CHECK(creature_control_invalid(nullptr));
    CHECK(creature_control_invalid(creature_control_get(0)));
}

TEST_CASE_METHOD(ResetSimState, "creature_control_invalid accepts an in-range slot", "[kfx_sim][creature_control]") {
    CHECK_FALSE(creature_control_invalid(creature_control_get(1)));
}

TEST_CASE_METHOD(ResetSimState, "creature_control_invalid has no upper-bound check", "[kfx_sim][creature_control]") {
    // Documented actual behavior, not "correct" behavior: unlike
    // thing_is_invalid(), an index at or past CREATURES_COUNT is still
    // reported valid, since the function only compares against the
    // lower bound.
    CHECK_FALSE(creature_control_invalid(&kfx_sim_state.cctrl_data[CREATURES_COUNT]));
}

TEST_CASE_METHOD(ResetSimState, "creature_control_exists is false until CCFlg_Exists is set", "[kfx_sim][creature_control]") {
    struct CreatureControl *cctrl = creature_control_get(1);
    CHECK_FALSE(creature_control_exists(cctrl));

    cctrl->creature_control_flags |= CCFlg_Exists;
    CHECK(creature_control_exists(cctrl));
}

TEST_CASE_METHOD(ResetSimState, "creature_control_get_from_thing resolves via the thing's ccontrol_idx", "[kfx_sim][creature_control]") {
    struct Thing *thing = thing_get(1);
    thing->ccontrol_idx = 5;
    CHECK(creature_control_get_from_thing(thing) == creature_control_get(5));
}

TEST_CASE_METHOD(ResetSimState, "creature_control_get_from_thing returns the sentinel for ccontrol_idx 0", "[kfx_sim][creature_control]") {
    struct Thing *thing = thing_get(1);
    thing->ccontrol_idx = 0;
    CHECK(creature_control_invalid(creature_control_get_from_thing(thing)));
}
