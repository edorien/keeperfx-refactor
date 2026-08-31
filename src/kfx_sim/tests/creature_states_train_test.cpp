// kfx_sim "creature" cluster, hard-tail depth increment: the first test
// for creature_states_train.c. creature_can_be_trained/
// player_can_afford_to_train_creature are pattern A/B (default model-0
// provider, kfx_sim_state.dungeon[] fields), and at_training_room is
// another "at_X_room()" entry point via kfx_sim_test_fixtures.h -- with
// two extra gates (trainability, affordability) ahead of the usual
// room-role check.
//
// On success, at_training_room also calls setup_move_to_new_training_position,
// which calls find_random_valid_position_for_thing_in_room -- that
// returns false immediately (an ERRORLOG, not a crash) for a room with
// slabs_count==0, the fixture's default, confirmed safe by an actual run
// rather than assumed; the function then falls through to
// set_creature_instance(..., CrInst_SWING_WEAPON_SWORD, ...) instead of
// setting a training-post position, still without crashing.
#include <catch2/catch_test_macros.hpp>

#include "creature_states_train.h"
#include "creature_control.h"
#include "dungeon_data.h"
#include "globals.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "creature_can_be_trained requires a positive training_value and room to gain experience", "[kfx_sim][creature_states_train]") {
    struct Thing *thing = make_creature(1, 1, 0);
    CHECK_FALSE(creature_can_be_trained(thing)); // training_value defaults to 0

    kfx_config_state.conf.crtr_conf.model[0].training_value = 5;
    CHECK_FALSE(creature_can_be_trained(thing)); // creature_max_level[model] still 0 == exp_level

    get_dungeon(0)->creature_max_level[0] = 5;
    CHECK(creature_can_be_trained(thing));
}

TEST_CASE_METHOD(ResetSimAndConfig, "player_can_afford_to_train_creature compares total_money_owned against the configured training cost", "[kfx_sim][creature_states_train]") {
    struct Thing *thing = make_creature(1, 1, 0);
    kfx_config_state.conf.crtr_conf.model[0].training_cost = 100;
    get_dungeon(0)->modifier.training_cost = 100;

    get_dungeon(0)->total_money_owned = 50;
    CHECK_FALSE(player_can_afford_to_train_creature(thing));

    get_dungeon(0)->total_money_owned = 100; // exactly enough -- ">=" includes it
    CHECK(player_can_afford_to_train_creature(thing));
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_training_room fails early when the creature can't be trained at all", "[kfx_sim][creature_states_train]") {
    struct Thing *thing = make_creature(1, 1, 0);
    // training_value stays 0 -- creature_can_be_trained is false regardless of room/gold.

    CHECK(at_training_room(thing) == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_training_room fails when trainable but the player can't afford it", "[kfx_sim][creature_states_train]") {
    struct Thing *thing = make_creature(1, 1, 0);
    kfx_config_state.conf.crtr_conf.model[0].training_value = 5;
    get_dungeon(0)->creature_max_level[0] = 5;
    kfx_config_state.conf.crtr_conf.model[0].training_cost = 100;
    get_dungeon(0)->modifier.training_cost = 100;
    get_dungeon(0)->total_money_owned = 0;

    CHECK(at_training_room(thing) == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_training_room succeeds when trainable, affordable, and on a valid training room", "[kfx_sim][creature_states_train]") {
    struct Thing *thing = make_creature(1, 1, 0);
    kfx_config_state.conf.crtr_conf.model[0].training_value = 5;
    get_dungeon(0)->creature_max_level[0] = 5;
    get_dungeon(0)->total_money_owned = 1000; // training_cost defaults to 0 -- trivially affordable

    struct Room *room = make_room_at_slab(1, 0, 0, RoK_TRAINING, 0);
    room->total_capacity = 10;
    configure_room_role(RoK_TRAINING, RoRoF_DeadStorage);
    const CrtrStateId kContinueState = 42;
    configure_job(Job_TRAIN, RoRoF_DeadStorage, 0, kContinueState);

    struct CreatureControl *cctrl = creature_control_get(1);
    cctrl->turns_at_job = 99;

    CHECK(at_training_room(thing) == 1);
    CHECK(cctrl->work_room_id == room->index);
    CHECK(cctrl->turns_at_job == 0);
    CHECK(thing->active_state == kContinueState);
}
