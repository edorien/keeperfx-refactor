// kfx_sim "creature" cluster, hard-tail depth increment: creature_senses.c
// was at 0% coverage, but almost the whole file is line-of-sight geometry
// tracing that needs a real map fixture (genuinely the hard tail).
// get_explore_sight_distance_in_slabs is the one self-contained exception:
// a thing_exists() guard plus is_thing_some_way_controlled() (itself
// pattern A on kfx_sim_state.players[].controlled_thing_idx), no map
// involved at all.
#include <catch2/catch_test_macros.hpp>

#include "creature_senses.h"
#include "thing_data.h"
#include "player_data.h"
#include "globals.h"
#include "kfx_sim_state.h"
#include "kfx_config_state.h"

#include <cstring>

namespace {
struct ResetState {
    ResetState() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::memset(&kfx_config_state, 0, sizeof(kfx_config_state));
        // Left at its zeroed default, neutral_player_num (0) would collide
        // with owner 0 below and make is_neutral_thing() true -- caught by
        // an empirical run returning 7 (uncontrolled) instead of 10 for
        // what should have been the controlled case.
        kfx_config_state.neutral_player_num = PLAYER_NEUTRAL;
    }
};
}

TEST_CASE_METHOD(ResetState, "get_explore_sight_distance_in_slabs is 0 for a thing that doesn't exist", "[kfx_sim][creature_senses]") {
    struct Thing *thing = thing_get(1);
    CHECK(get_explore_sight_distance_in_slabs(thing) == 0);
}

TEST_CASE_METHOD(ResetState, "get_explore_sight_distance_in_slabs is 7 for an uncontrolled creature", "[kfx_sim][creature_senses]") {
    struct Thing *thing = thing_get(1);
    thing->alloc_flags = TAlF_Exists;
    thing->index = 1;
    thing->owner = 0;

    CHECK(get_explore_sight_distance_in_slabs(thing) == 7);
}

TEST_CASE_METHOD(ResetState, "get_explore_sight_distance_in_slabs is 10 once the owning player is directly controlling it", "[kfx_sim][creature_senses]") {
    struct Thing *thing = thing_get(1);
    thing->alloc_flags = TAlF_Exists;
    thing->index = 1;
    thing->owner = 0;
    get_player(0)->controlled_thing_idx = 1;

    CHECK(get_explore_sight_distance_in_slabs(thing) == 10);
}
