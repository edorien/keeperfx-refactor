// kfx_sim coverage: thing_shots.c's model-flag predicate family --
// thing_is_shot and shot_is_slappable_by_player/shot_model_is_navigable/
// shot_is_boulder/shot_model_makes_flesh_explosion are plain
// ShotConfigStats::model_flags lookups indexed directly by shot model (no
// pointer-identity indirection to worry about, same shape as
// map_columns.c's CubeConfigStats lookups). The rest of this file is
// shot-flight/collision/detonation simulation (detonate_shot,
// shot_hit_wall_at, shot_hit_creature_at, shot_hit_shootable_thing_at,
// shot_hit_something_while_moving, give_gold_to_creature_or_drop_on_map_when_digging,
// apply_shot_experience(_from_hitting_creature)) -- genuinely the hard
// tail, left for a later increment.
#include <catch2/catch_test_macros.hpp>

#include "thing_shots.h"
#include "config_magic.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

namespace {
struct Thing *make_shot(ThingIndex idx, ThingModel model, PlayerNumber owner = 0)
{
    struct Thing *thing = thing_get(idx);
    thing->index = idx;
    thing->class_id = TCls_Shot;
    thing->alloc_flags = TAlF_Exists;
    thing->model = model;
    thing->owner = owner;
    return thing;
}
}

TEST_CASE_METHOD(ResetSimAndConfig, "thing_is_shot requires a valid thing of class Shot", "[kfx_sim][thing_shots]") {
    struct Thing *shot = make_shot(1, 3);
    CHECK(thing_is_shot(shot));

    struct Thing *creature = make_creature(2, 2, 0);
    CHECK_FALSE(thing_is_shot(creature));
}

TEST_CASE_METHOD(ResetSimAndConfig, "shot_is_slappable_by_player requires both ownership and the ShMF_Slappable model flag", "[kfx_sim][thing_shots]") {
    struct Thing *shot = make_shot(1, 3, 0);
    kfx_config_state.conf.magic_conf.shot_types_count = 4;
    kfx_config_state.conf.magic_conf.shot_cfgstats[3].model_flags = ShMF_Slappable;

    CHECK(shot_is_slappable_by_player(shot, 0));
    CHECK_FALSE(shot_is_slappable_by_player(shot, 1)); // wrong owner

    kfx_config_state.conf.magic_conf.shot_cfgstats[3].model_flags = 0;
    CHECK_FALSE(shot_is_slappable_by_player(shot, 0));
}

TEST_CASE_METHOD(ResetSimAndConfig, "shot_model_is_navigable/shot_is_boulder/shot_model_makes_flesh_explosion read their respective model flags", "[kfx_sim][thing_shots]") {
    kfx_config_state.conf.magic_conf.shot_types_count = 4;
    struct Thing *shot = make_shot(1, 3);

    CHECK_FALSE(shot_model_is_navigable(3));
    CHECK_FALSE(shot_is_boulder(shot));
    CHECK_FALSE(shot_model_makes_flesh_explosion(3));

    kfx_config_state.conf.magic_conf.shot_cfgstats[3].model_flags = ShMF_Navigable;
    CHECK(shot_model_is_navigable(3));
    CHECK_FALSE(shot_is_boulder(shot));

    kfx_config_state.conf.magic_conf.shot_cfgstats[3].model_flags = ShMF_Boulder;
    CHECK(shot_is_boulder(shot));
    CHECK_FALSE(shot_model_is_navigable(3));

    kfx_config_state.conf.magic_conf.shot_cfgstats[3].model_flags = ShMF_Exploding;
    CHECK(shot_model_makes_flesh_explosion(3));
}
