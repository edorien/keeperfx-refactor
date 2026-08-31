// kfx_sim "creature" cluster, hard-tail depth increment: the first test
// for creature_states_scavn.c. Most of the file is scavenge-target
// selection/pool dispatch needing a full Room+thing-list fixture, but
// creature_can_do_scavenging (pattern B, default model-0 provider) and
// at_scavenger_room (the same "at_X_room()" shape as
// creature_states_barck.c/_guard.c, via kfx_sim_test_fixtures.h) are
// tractable now. at_scavenger_room additionally gates on affordability
// through calculate_correct_creature_scavenging_cost, itself pattern A/B
// on kfx_config_state.conf.crtr_conf/kfx_sim_state.dungeon[].modifier --
// covered directly here rather than re-derived, since it decides which
// of at_scavenger_room's two failure branches (bad room vs. can't afford)
// a given fixture actually reaches.
#include <catch2/catch_test_macros.hpp>

#include "creature_states_scavn.h"
#include "creature_control.h"
#include "dungeon_data.h"
#include "globals.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "creature_can_do_scavenging requires a non-neutral creature whose model has a positive scavenge_value", "[kfx_sim][creature_states_scavn]") {
    struct Thing *thing = make_creature(1, 1, 0);
    CHECK_FALSE(creature_can_do_scavenging(thing)); // scavenge_value defaults to 0

    kfx_config_state.conf.crtr_conf.model[0].scavenge_value = 5;
    CHECK(creature_can_do_scavenging(thing));

    thing->owner = PLAYER_NEUTRAL;
    CHECK_FALSE(creature_can_do_scavenging(thing));
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_scavenger_room fails and resets the creature's state when it isn't standing on a scavenger-role room", "[kfx_sim][creature_states_scavn]") {
    struct Thing *thing = make_creature(1, 1, 0);
    configure_job(Job_SCAVENGE, RoRoF_Prison, 0);

    CHECK(at_scavenger_room(thing) == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_scavenger_room fails when the player can't afford the scavenging cost", "[kfx_sim][creature_states_scavn]") {
    struct Thing *thing = make_creature(1, 1, 0);
    struct Room *room = make_room_at_slab(1, 0, 0, RoK_SCAVENGER, 0);
    room->total_capacity = 10;
    configure_room_role(RoK_SCAVENGER, RoRoF_DeadStorage);
    configure_job(Job_SCAVENGE, RoRoF_DeadStorage, 0);

    kfx_config_state.conf.crtr_conf.model[0].scavenger_cost = 100;
    get_dungeon(0)->modifier.scavenging_cost = 100; // full-price modifier
    get_dungeon(0)->total_money_owned = 50; // less than the 100-gold cost

    CHECK(at_scavenger_room(thing) == 0);
    CHECK(creature_control_get(1)->work_room_id == 0); // never joined
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_scavenger_room succeeds once the player can afford it: joins the work room and sets the job's continue state", "[kfx_sim][creature_states_scavn]") {
    struct Thing *thing = make_creature(1, 1, 0);
    struct Room *room = make_room_at_slab(1, 0, 0, RoK_SCAVENGER, 0);
    room->total_capacity = 10;
    configure_room_role(RoK_SCAVENGER, RoRoF_DeadStorage);
    const CrtrStateId kContinueState = 42;
    configure_job(Job_SCAVENGE, RoRoF_DeadStorage, 0, kContinueState);

    kfx_config_state.conf.crtr_conf.model[0].scavenger_cost = 100;
    get_dungeon(0)->modifier.scavenging_cost = 100;
    get_dungeon(0)->total_money_owned = 1000;

    struct CreatureControl *cctrl = creature_control_get(1);
    cctrl->turns_at_job = 99;

    CHECK(at_scavenger_room(thing) == 1);
    CHECK(cctrl->work_room_id == room->index);
    CHECK(cctrl->turns_at_job == 0);
    CHECK(thing->active_state == kContinueState);
}
