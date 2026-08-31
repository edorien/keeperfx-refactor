// kfx_sim "creature" cluster, hard-tail depth increment: the first test
// for creature_states_barck.c (one of the two files -- alongside
// creature_states_guard.c -- stage-08b's original pass explicitly flagged
// as needing "a fuller Room+CreatureControl+job context" beyond the
// accessor-only tests landed at the time). Uses the new shared
// kfx_sim_test_fixtures.h helpers (make_room_at_slab/make_creature/
// configure_job/configure_room_role), extracted for exactly this
// situation: several creature_states_*.c "at_X_room()" entry points need
// the same setup.
//
// at_barrack_room()'s failure branch calls set_start_state(), which
// (confirmed by reading set_start_state_f's body) resolves to the safe
// "player doesn't exist" branch -- initialise_thing_state(thing,
// CrSt_CreatureDormant) -- as long as the creature's owning player stays
// at its zeroed default (PlaF_Allocated unset). initialise_thing_state's
// own cleanup_current_thing_state() call is likewise safe by default:
// kfx_config_state.conf.crtr_conf.states[0].cleanup_state defaults to 0,
// which takes cleanup_current_thing_state's plain
// clear_creature_instance() branch, not the cleanup_func_list[] dispatch
// table. barracking() itself (the ongoing per-turn state, which calls
// creature_setup_adjacent_move_for_job_within_room) needs real navigation
// and is left for a later increment.
#include <catch2/catch_test_macros.hpp>

#include "creature_states_barck.h"
#include "creature_control.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "at_barrack_room fails and resets the creature's state when it isn't standing on a barrack-role room", "[kfx_sim][creature_states_barck]") {
    struct Thing *creatng = make_creature(1, 1, 0);
    // No room at all under the creature's default (0,0) position.
    configure_job(Job_BARRACK, RoRoF_Prison, 0); // wrong role, doesn't matter which room exists

    CHECK(at_barrack_room(creatng) == 0);
    CHECK(creature_control_get(1)->target_room_id == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_barrack_room fails when the room exists but is owned by someone else and enemies may not work there", "[kfx_sim][creature_states_barck]") {
    struct Thing *creatng = make_creature(1, 1, 0);
    make_room_at_slab(1, 0, 0, RoK_BARRACKS, 3); // owned by player 3, creature is player 0
    configure_room_role(RoK_BARRACKS, RoRoF_Prison); // matches the (wrong) role below
    configure_job(Job_BARRACK, RoRoF_Prison, 0);

    CHECK(at_barrack_room(creatng) == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_barrack_room succeeds on a valid barrack room: joins the work room and sets the job's continue state", "[kfx_sim][creature_states_barck]") {
    struct Thing *creatng = make_creature(1, 1, 0);
    struct Room *room = make_room_at_slab(1, 0, 0, RoK_BARRACKS, 0); // same owner as the creature
    room->total_capacity = 10;
    // An arbitrary non-LairStorage/_CrHealSleep role -- avoids
    // get_required_room_capacity_for_job's special-cased lair_size branch,
    // so required capacity resolves via the plain job_flags check (0,
    // since JoKF_NeedsCapacity isn't set below).
    configure_room_role(RoK_BARRACKS, RoRoF_DeadStorage);
    // No CrtrStateId is specifically named for barracking's continue state
    // in the headers -- 42 is an arbitrary marker value proving
    // get_continue_state_for_job's config value round-trips into
    // creatng->active_state, not a real named state.
    const CrtrStateId kContinueState = 42;
    configure_job(Job_BARRACK, RoRoF_DeadStorage, 0, kContinueState);

    CHECK(at_barrack_room(creatng) == 1);

    struct CreatureControl *cctrl = creature_control_get(1);
    CHECK(cctrl->work_room_id == room->index);
    CHECK((cctrl->creature_control_flags & CCFlg_IsInRoomList) != 0);
    CHECK(creatng->active_state == kContinueState);
}
