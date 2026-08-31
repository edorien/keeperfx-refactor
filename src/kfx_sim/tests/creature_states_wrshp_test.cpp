// kfx_sim "creature" cluster, hard-tail depth increment: the first test
// for creature_states_wrshp.c. at_workshop_room's success path needs
// creature_can_do_manufacturing() -> get_next_manufacture() to resolve
// true, which reaches into set_manufacture_level/
// get_doable_manufacture_with_minimal_amount_available's trap/door
// manufacturing-selection algorithm (room_workshop.c) -- a genuinely
// bigger increment than the room-role/job-config setup
// kfx_sim_test_fixtures.h already covers, left for a later increment.
// Both of at_workshop_room's failure branches are cheaply reachable
// without it, though: the room-role check runs first (before
// creature_can_do_manufacturing at all), and the manufacturing-value
// check short-circuits on crconf->manufacture_value<=0 before ever
// calling get_next_manufacture.
#include <catch2/catch_test_macros.hpp>

#include "creature_states_wrshp.h"
#include "creature_control.h"
#include "globals.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "at_workshop_room fails when it isn't standing on a manufacture-role room", "[kfx_sim][creature_states_wrshp]") {
    struct Thing *thing = make_creature(1, 1, 0);
    configure_job(Job_MANUFACTURE, RoRoF_Prison, 0);

    CHECK(at_workshop_room(thing) == 0);
    CHECK(creature_control_get(1)->target_room_id == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_workshop_room fails when the creature's model can't manufacture at all, even on a valid room", "[kfx_sim][creature_states_wrshp]") {
    struct Thing *thing = make_creature(1, 1, 0);
    struct Room *room = make_room_at_slab(1, 0, 0, RoK_WORKSHOP, 0);
    room->total_capacity = 10;
    configure_room_role(RoK_WORKSHOP, RoRoF_DeadStorage);
    configure_job(Job_MANUFACTURE, RoRoF_DeadStorage, 0);
    // manufacture_value stays 0 -- creature_can_do_manufacturing short-circuits
    // before ever reaching get_next_manufacture's config-driven selection.

    CHECK(at_workshop_room(thing) == 0);
    CHECK(creature_control_get(1)->work_room_id == 0); // never joined the room
}

// creature_can_do_manufacturing directly: confirmed by reading (and then
// running) get_doable_manufacture_with_minimal_amount_available that a
// zeroed door_types_count/trap_types_count makes both its selection loops
// no-ops, so get_next_manufacture deterministically returns false rather
// than needing a real manufacturable trap/door configured -- the "model
// can manufacture, but nothing is currently doable" case.
TEST_CASE_METHOD(ResetSimAndConfig, "creature_can_do_manufacturing requires a non-neutral owner, a positive manufacture_value, and a doable manufacture", "[kfx_sim][creature_states_wrshp]") {
    struct Thing *thing = make_creature(1, 1, 0);
    CHECK_FALSE(creature_can_do_manufacturing(thing)); // manufacture_value defaults to 0

    kfx_config_state.conf.crtr_conf.model[0].manufacture_value = 1;
    CHECK_FALSE(creature_can_do_manufacturing(thing)); // manufacturable, but nothing doable right now

    struct Thing *neutral_thing = make_creature(2, 2, PLAYER_NEUTRAL);
    CHECK_FALSE(creature_can_do_manufacturing(neutral_thing));
}
