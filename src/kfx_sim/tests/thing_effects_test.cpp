// kfx_sim coverage: a first pass over thing_effects.c's class-check and
// hit-target predicates. effect_can_affect_thing/area_effect_can_affect_thing
// chain into thing_is_shootable (thing_list.c) via hit_type_to_hit_targets --
// tractable now that thing_is_shootable's creature branch is covered
// directly in thing_list_test.cpp.
//
// effect_can_affect_thing had only a same-file forward declaration --
// added to thing_effects.h alongside its already-declared sibling
// area_effect_can_affect_thing.
//
// Deliberately deferred: everything else in this 2200-line file
// (create_effect_element/create_effect_generator/effect_generate_effect_elements,
// the explosion_affecting_*/poison_cloud_affecting_* map-block sweeps,
// update_effect_element/update_effect, draw_flame_breath/draw_lightning)
// -- thing creation, map-block traversal, and rendering heavy.
#include <catch2/catch_test_macros.hpp>

#include "thing_effects.h"
#include "thing_list.h"
#include "config_effects.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "thing_is_effect requires a valid thing of class Effect", "[kfx_sim][thing_effects]") {
    struct Thing *effect = thing_get(1);
    effect->index = 1;
    effect->alloc_flags = TAlF_Exists;
    effect->class_id = TCls_Effect;
    CHECK(thing_is_effect(effect));

    struct Thing *creature = make_creature(2, 2, 0);
    CHECK_FALSE(thing_is_effect(creature));
}

TEST_CASE_METHOD(ResetSimAndConfig, "get_effect_element_model_stats falls back to model 0 once tngmodel reaches EFFECTSELLEMENTS_TYPES_MAX", "[kfx_sim][thing_effects]") {
    kfx_config_state.conf.effects_conf.effectelement_cfgstats[3].lifespan = 42;
    CHECK(get_effect_element_model_stats(3)->lifespan == 42);
    CHECK(get_effect_element_model_stats(EFFECTSELLEMENTS_TYPES_MAX)->lifespan == 0); // out of range: model 0's default
}

TEST_CASE_METHOD(ResetSimAndConfig, "effect_can_affect_thing refuses to target the effect itself or its own maker", "[kfx_sim][thing_effects]") {
    struct Thing *effect = thing_get(1);
    effect->index = 1;
    effect->alloc_flags = TAlF_Exists;
    effect->class_id = TCls_Effect;
    effect->shot_effect.hit_type = THit_All;
    effect->owner = 0;
    effect->parent_idx = 2;

    CHECK_FALSE(effect_can_affect_thing(effect, effect)); // targeting itself

    struct Thing *maker = make_creature(2, 2, 0);
    CHECK_FALSE(effect_can_affect_thing(effect, maker)); // targeting its own maker
}

TEST_CASE_METHOD(ResetSimAndConfig, "effect_can_affect_thing/area_effect_can_affect_thing resolve through thing_is_shootable via hit_type_to_hit_targets", "[kfx_sim][thing_effects]") {
    make_player_active(0);
    make_player_active(1);
    struct Thing *effect = thing_get(1);
    effect->index = 1;
    effect->alloc_flags = TAlF_Exists;
    effect->class_id = TCls_Effect;
    effect->shot_effect.hit_type = THit_All; // every HitTF_*Creatures bit set
    effect->owner = 0;

    struct Thing *enemy_creature = make_creature(3, 3, 1);
    CHECK(effect_can_affect_thing(effect, enemy_creature));
    CHECK(area_effect_can_affect_thing(enemy_creature, hit_type_to_hit_targets(THit_All), 0));

    CHECK_FALSE(area_effect_can_affect_thing(NULL, hit_type_to_hit_targets(THit_All), 0)); // invalid thing guard
}
