// kfx_sim "creature" cluster, depth increment: creature_states.c's
// can_change_from_state_to() -- the actual state-transition permission
// logic stage-08's fan-in argument was about, not just the accessor
// layer beneath it (creature_control_test.cpp). Driven entirely by
// kfx_config_state.conf.crtr_conf.states[] (pattern A) and a handful of
// struct Thing fields -- no live simulation tick, no CreatureControl
// needed, since this function never dereferences one.
//
// Also covers get_thing_state_info_num()/state_info_invalid(), the same
// "index sentinel" family as every other cluster's accessors
// (docs/refactor/testing/comprehensive/stage-08b-kfx-sim-clusters.md),
// here over kfx_config_state.conf.crtr_conf.states[] instead of a
// kfx_sim_state array.
#include <catch2/catch_test_macros.hpp>

#include "creature_states.h"
#include "config_creature.h"
#include "kfx_config_state.h"
#include "thing_data.h"
#include "globals.h"

#include <cstring>

namespace {
struct ResetConfigState {
    ResetConfigState() { std::memset(&kfx_config_state, 0, sizeof(kfx_config_state)); }
};

struct CreatureStateFixture : ResetConfigState {
    struct Thing thing{};
};
}

TEST_CASE_METHOD(ResetConfigState, "state_info_invalid rejects the reserved index-0 sentinel", "[kfx_sim][creature_states]") {
    CHECK(state_info_invalid(get_thing_state_info_num(0)));
}

TEST_CASE_METHOD(ResetConfigState, "state_info_invalid accepts an in-range state id", "[kfx_sim][creature_states]") {
    CHECK_FALSE(state_info_invalid(get_thing_state_info_num(1)));
}

TEST_CASE_METHOD(ResetConfigState, "get_thing_state_info_num falls back to slot 0 for an out-of-range id", "[kfx_sim][creature_states]") {
    CHECK(get_thing_state_info_num(CREATURE_STATES_COUNT) == get_thing_state_info_num(0));
}

TEST_CASE_METHOD(CreatureStateFixture, "can_change_from_state_to: a controlled creature can only move to an idle-type state", "[kfx_sim][creature_states]") {
    thing.alloc_flags |= TAlF_IsControlled;
    kfx_config_state.conf.crtr_conf.states[2].state_type = CrStTyp_Sleep;
    CHECK_FALSE(can_change_from_state_to(&thing, 1, 2));

    kfx_config_state.conf.crtr_conf.states[2].state_type = CrStTyp_Idle;
    CHECK(can_change_from_state_to(&thing, 1, 2));
}

TEST_CASE_METHOD(CreatureStateFixture, "can_change_from_state_to: a transition-flagged current state blocks the change unless overridden", "[kfx_sim][creature_states]") {
    kfx_config_state.conf.crtr_conf.states[1].transition = true;
    CHECK_FALSE(can_change_from_state_to(&thing, 1, 2));

    kfx_config_state.conf.crtr_conf.states[2].override_transition = true;
    CHECK(can_change_from_state_to(&thing, 1, 2));
}

TEST_CASE_METHOD(CreatureStateFixture, "can_change_from_state_to: a captive current state blocks the change unless overridden", "[kfx_sim][creature_states]") {
    kfx_config_state.conf.crtr_conf.states[1].captive = true;
    CHECK_FALSE(can_change_from_state_to(&thing, 1, 2));

    kfx_config_state.conf.crtr_conf.states[2].override_captive = true;
    CHECK(can_change_from_state_to(&thing, 1, 2));
}

TEST_CASE_METHOD(CreatureStateFixture, "can_change_from_state_to: the current state type's matching override gates the change", "[kfx_sim][creature_states]") {
    kfx_config_state.conf.crtr_conf.states[1].state_type = CrStTyp_Sleep;
    CHECK_FALSE(can_change_from_state_to(&thing, 1, 2)); // override_sleep defaults to false

    kfx_config_state.conf.crtr_conf.states[2].override_sleep = true;
    CHECK(can_change_from_state_to(&thing, 1, 2));
}

TEST_CASE_METHOD(CreatureStateFixture, "can_change_from_state_to: an unmatched current state type defaults to allowing the change", "[kfx_sim][creature_states]") {
    // CrStTyp_Idle (the zero value, unset default) has no case in the
    // switch -- falls through to `default: return true;`.
    CHECK(can_change_from_state_to(&thing, 1, 2));
}
