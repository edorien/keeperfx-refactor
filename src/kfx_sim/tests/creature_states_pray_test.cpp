// kfx_sim "creature" cluster, hard-tail depth increment: the first test
// for creature_states_pray.c. creature_is_doing_temple_pray_activity is a
// pure state check (via get_creature_state_besides_interruptions,
// already exercised in creature_states_lair_test.cpp), and at_temple is
// another "at_X_room()" entry point via kfx_sim_test_fixtures.h -- no
// navigation calls on either its failure or success path, unlike several
// of its siblings (guard/scavenge/training). The sacrifice-recipe/
// spell-effect/summon machinery (process_sacrifice_award,
// apply_spell_effect_to_players_creatures, summon_creature, etc.) needs a
// fuller map+thing-list fixture and is left for a later increment.
#include <catch2/catch_test_macros.hpp>

#include "creature_states_pray.h"
#include "creature_states.h"
#include "creature_control.h"
#include "dungeon_data.h"
#include "globals.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "creature_is_doing_temple_pray_activity recognizes both temple-related states", "[kfx_sim][creature_states_pray]") {
    struct Thing *thing = make_creature(1, 1, 0);

    thing->active_state = CrSt_AtTemple;
    CHECK(creature_is_doing_temple_pray_activity(thing));
    thing->active_state = CrSt_PrayingInTemple;
    CHECK(creature_is_doing_temple_pray_activity(thing));
    thing->active_state = CrSt_CreatureSleep;
    CHECK_FALSE(creature_is_doing_temple_pray_activity(thing));
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_temple fails and resets the creature's state when it isn't standing on a temple-pray-role room", "[kfx_sim][creature_states_pray]") {
    struct Thing *thing = make_creature(1, 1, 0);
    configure_job(Job_TEMPLE_PRAY, RoRoF_Prison, 0);

    CHECK(at_temple(thing) == 0);
    CHECK(creature_control_get(1)->target_room_id == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_temple succeeds on a valid temple room: joins the work room and tallies the praying creature", "[kfx_sim][creature_states_pray]") {
    struct Thing *thing = make_creature(1, 1, 0);
    struct Room *room = make_room_at_slab(1, 0, 0, RoK_TEMPLE, 0);
    room->total_capacity = 10;
    configure_room_role(RoK_TEMPLE, RoRoF_DeadStorage);
    const CrtrStateId kContinueState = 42;
    configure_job(Job_TEMPLE_PRAY, RoRoF_DeadStorage, 0, kContinueState);

    struct CreatureControl *cctrl = creature_control_get(1);
    cctrl->turns_at_job = 99;

    CHECK(at_temple(thing) == 1);
    CHECK(cctrl->work_room_id == room->index);
    CHECK(cctrl->turns_at_job == 0);
    CHECK(thing->active_state == kContinueState);
    CHECK(get_dungeon(0)->creatures_praying[thing->model] == 1);
}
