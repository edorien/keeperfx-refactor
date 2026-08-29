// kfx_sim "thing" cluster depth increment, per docs/refactor/testing/
// comprehensive/stage-08b-kfx-sim-clusters.md's "still open" list:
// thing_stats.c's pure computation functions. get_radially_decaying_value/
// get_radially_growing_value/compute_controlled_speed_increase/_decrease
// are self-contained integer arithmetic, no state at all; is_neutral_thing/
// is_hero_thing and compute_creature_max_health are pattern A on
// kfx_sim_state/kfx_config_state, same fixture used throughout this plan.
#include <catch2/catch_test_macros.hpp>

#include "thing_stats.h"
#include "thing_data.h"
#include "player_data.h"
#include "kfx_sim_state.h"
#include "kfx_config_state.h"

#include <cstring>

namespace {
struct ResetSimState {
    ResetSimState() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::memset(&kfx_config_state, 0, sizeof(kfx_config_state));
    }
};
}

TEST_CASE("get_radially_decaying_value returns full magnitude before decay_start", "[kfx_sim][thing_stats]") {
    CHECK(get_radially_decaying_value(100, 10, 10, 5) == 100);
    CHECK(get_radially_decaying_value(100, 10, 10, 10) == 100); // at the boundary, still full
}

TEST_CASE("get_radially_decaying_value decays linearly through the decay region", "[kfx_sim][thing_stats]") {
    CHECK(get_radially_decaying_value(100, 10, 10, 15) == 50); // halfway through
}

TEST_CASE("get_radially_decaying_value is zero at and past decay_start + decay_length", "[kfx_sim][thing_stats]") {
    CHECK(get_radially_decaying_value(100, 10, 10, 20) == 0);
    CHECK(get_radially_decaying_value(100, 10, 10, 999) == 0);
}

TEST_CASE("get_radially_growing_value is zero past the max range", "[kfx_sim][thing_stats]") {
    CHECK(get_radially_growing_value(100, 10, 10, 20, 1) == 0);
}

TEST_CASE("get_radially_growing_value returns the decayed magnitude unchanged when the push distance doesn't overshoot", "[kfx_sim][thing_stats]") {
    // A huge friction shrinks the intended push distance (5) to no more
    // than the actual distance (5), so the function falls through to
    // returning magnitude as-is rather than clamping.
    CHECK(get_radially_growing_value(10, 1000, 100, 5, 1000000) == 10);
}

TEST_CASE("get_radially_growing_value returns a negative pull-back value when the push would overshoot the epicenter", "[kfx_sim][thing_stats]") {
    CHECK(get_radially_growing_value(100, 0, 1000, 500, 1) == -2);
}

TEST_CASE("compute_controlled_speed_increase steps by 1 below speed_limit 4, else by speed_limit/4", "[kfx_sim][thing_stats]") {
    CHECK(compute_controlled_speed_increase(0, 3) == 1);
    CHECK(compute_controlled_speed_increase(0, 20) == 5);
}

TEST_CASE("compute_controlled_speed_increase clamps into [-speed_limit, speed_limit]", "[kfx_sim][thing_stats]") {
    CHECK(compute_controlled_speed_increase(5, 3) == 3);     // 6 clamped down to 3
    CHECK(compute_controlled_speed_increase(-30, 20) == -20); // -25 clamped up to -20
}

TEST_CASE("compute_controlled_speed_decrease steps by 1 below speed_limit 4, else by speed_limit/4", "[kfx_sim][thing_stats]") {
    CHECK(compute_controlled_speed_decrease(0, 3) == -1);
    CHECK(compute_controlled_speed_decrease(0, 20) == -5);
}

TEST_CASE("compute_controlled_speed_decrease clamps into [-speed_limit, speed_limit]", "[kfx_sim][thing_stats]") {
    CHECK(compute_controlled_speed_decrease(-5, 3) == -3);  // -6 clamped up to -3
    CHECK(compute_controlled_speed_decrease(30, 20) == 20); // 25 clamped down to 20
}

TEST_CASE_METHOD(ResetSimState, "compute_creature_max_health applies the configured per-level percentage", "[kfx_sim][thing_stats]") {
    kfx_config_state.conf.crtr_conf.exp.health_increase_on_exp = 10; // 10% per level
    CHECK(compute_creature_max_health(100, 0) == 100);
    CHECK(compute_creature_max_health(100, 5) == 150);
}

TEST_CASE_METHOD(ResetSimState, "compute_creature_max_health clamps exp_level to CREATURE_MAX_LEVEL - 1", "[kfx_sim][thing_stats]") {
    kfx_config_state.conf.crtr_conf.exp.health_increase_on_exp = 10;
    CHECK(compute_creature_max_health(100, CREATURE_MAX_LEVEL) == compute_creature_max_health(100, CREATURE_MAX_LEVEL - 1));
    CHECK(compute_creature_max_health(100, CREATURE_MAX_LEVEL + 5) == compute_creature_max_health(100, CREATURE_MAX_LEVEL - 1));
}

TEST_CASE_METHOD(ResetSimState, "is_neutral_thing compares owner against the configured neutral player number", "[kfx_sim][thing_stats]") {
    kfx_config_state.neutral_player_num = 5;
    struct Thing *thing = thing_get(1);
    thing->owner = 5;
    CHECK(is_neutral_thing(thing));
    thing->owner = 2;
    CHECK_FALSE(is_neutral_thing(thing));
}

TEST_CASE_METHOD(ResetSimState, "is_hero_thing reads the owning player's roaming player_type", "[kfx_sim][thing_stats]") {
    struct Thing *thing = thing_get(1);
    thing->owner = 3;
    struct PlayerInfo *owner = get_player(3);

    owner->player_type = PT_Keeper;
    CHECK_FALSE(is_hero_thing(thing));

    owner->player_type = PT_Roaming;
    CHECK(is_hero_thing(thing));
}
