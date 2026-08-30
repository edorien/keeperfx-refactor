// kfx_sim "creature" cluster: creature_states_gardn.c (Hatchery/garden
// job), the third individual creature_states_*.c file to get direct
// coverage. creature_able_to_eat/hunger_is_creature_hungry are both
// Pattern A/B, same ConfigReloadCallbacks fake as creature_states_tresr_
// test.cpp/creature_states_mood_test.cpp -- no Room/thing-list fixture
// needed, unlike the actual garden-room state-machine entry points
// (creature_to_garden/creature_eating_at_garden/person_eat_food/...) in
// the rest of this file, deliberately not attempted here (see
// docs/Architecture/testing-harness.md §10).
#include <catch2/catch_test_macros.hpp>

#include "creature_states_gardn.h"
#include "creature_control.h"
#include "thing_data.h"
#include "config_creature.h"
#include "kfx_config_state.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
ThingModel g_fake_model = 0;
ThingModel fake_get_thing_model(const struct Thing *) { return g_fake_model; }

struct GardnFixture {
    struct ConfigReloadCallbacks callbacks{};
    struct Thing* thing;
    struct CreatureControl* cctrl;

    GardnFixture() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::memset(&kfx_config_state, 0, sizeof(kfx_config_state));
        kfx_config_state.conf.crtr_conf.model_count = CREATURE_TYPES_MAX;
        g_fake_model = 1;
        callbacks.get_thing_model = fake_get_thing_model;
        set_config_reload_callbacks(&callbacks);

        thing = thing_get(1);
        thing->ccontrol_idx = 1;
        cctrl = creature_control_get(1);
    }
    ~GardnFixture() { set_config_reload_callbacks(nullptr); }
};
}

TEST_CASE_METHOD(GardnFixture, "creature_able_to_eat is true when the model has a nonzero hunger_rate", "[kfx_sim][creature_states_gardn]") {
    kfx_config_state.conf.crtr_conf.model[1].hunger_rate = 5;
    kfx_config_state.conf.crtr_conf.model[1].hunger_fill = 0;
    CHECK(creature_able_to_eat(thing));
}

TEST_CASE_METHOD(GardnFixture, "creature_able_to_eat is true when the model has a nonzero hunger_fill, even with zero hunger_rate", "[kfx_sim][creature_states_gardn]") {
    kfx_config_state.conf.crtr_conf.model[1].hunger_rate = 0;
    kfx_config_state.conf.crtr_conf.model[1].hunger_fill = 3;
    CHECK(creature_able_to_eat(thing));
}

TEST_CASE_METHOD(GardnFixture, "creature_able_to_eat is false when both hunger_rate and hunger_fill are zero", "[kfx_sim][creature_states_gardn]") {
    kfx_config_state.conf.crtr_conf.model[1].hunger_rate = 0;
    kfx_config_state.conf.crtr_conf.model[1].hunger_fill = 0;
    CHECK_FALSE(creature_able_to_eat(thing));
}

TEST_CASE_METHOD(GardnFixture, "creature_able_to_eat is false for the reserved model-0 sentinel", "[kfx_sim][creature_states_gardn]") {
    g_fake_model = 0;
    kfx_config_state.conf.crtr_conf.model[0].hunger_rate = 5; // would look eligible if read directly
    CHECK_FALSE(creature_able_to_eat(thing));
}

TEST_CASE_METHOD(GardnFixture, "hunger_is_creature_hungry is true once hunger_level exceeds the model's hunger_rate", "[kfx_sim][creature_states_gardn]") {
    kfx_config_state.conf.crtr_conf.model[1].hunger_rate = 100;
    cctrl->hunger_level = 101;
    CHECK(hunger_is_creature_hungry(thing));
}

TEST_CASE_METHOD(GardnFixture, "hunger_is_creature_hungry is false while hunger_level is at or below the model's hunger_rate", "[kfx_sim][creature_states_gardn]") {
    kfx_config_state.conf.crtr_conf.model[1].hunger_rate = 100;
    cctrl->hunger_level = 100;
    CHECK_FALSE(hunger_is_creature_hungry(thing));
}

TEST_CASE_METHOD(GardnFixture, "hunger_is_creature_hungry is false when the model's hunger_rate is zero, regardless of hunger_level", "[kfx_sim][creature_states_gardn]") {
    kfx_config_state.conf.crtr_conf.model[1].hunger_rate = 0;
    cctrl->hunger_level = 999999;
    CHECK_FALSE(hunger_is_creature_hungry(thing));
}

TEST_CASE_METHOD(GardnFixture, "hunger_is_creature_hungry is false for an invalid creature control", "[kfx_sim][creature_states_gardn]") {
    thing->ccontrol_idx = 0; // resolves to the reserved index-0 sentinel
    kfx_config_state.conf.crtr_conf.model[1].hunger_rate = 1;
    CHECK_FALSE(hunger_is_creature_hungry(thing));
}
