// kfx_sim "creature" cluster, hard-tail depth increment: the first test
// for creature_states_guard.c (the second of the two files -- alongside
// creature_states_barck.c -- stage-08b's original pass explicitly
// flagged as needing a fuller Room+CreatureControl+job context). Uses
// kfx_sim_test_fixtures.h, the same shared setup creature_states_barck_test.cpp
// introduced.
//
// Unlike at_barrack_room, at_guard_post_room's success path additionally
// calls person_get_somewhere_adjacent_in_room(), which reaches into real
// slab/room geometry (kfx_sim_state.small_around_slab[], zeroed by the
// fixture, degrades to "keep re-checking the creature's own slab" rather
// than crashing) and set_position_at_slab_for_thing()'s deeper terrain-
// safety checks. Its outcome isn't asserted on directly here -- only that
// at_guard_post_room completes and returns success/failure correctly --
// confirmed safe by an actual run, not assumed.
#include <catch2/catch_test_macros.hpp>

#include "creature_states_guard.h"
#include "creature_control.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "at_guard_post_room fails and resets the creature's state when it isn't standing on a guard-role room", "[kfx_sim][creature_states_guard]") {
    struct Thing *thing = make_creature(1, 1, 0);
    configure_job(Job_GUARD, RoRoF_Prison, 0);

    CHECK(at_guard_post_room(thing) == 0);
    CHECK(creature_control_get(1)->target_room_id == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_guard_post_room succeeds on a valid guard-post room: joins the work room and sets the job's continue state", "[kfx_sim][creature_states_guard]") {
    struct Thing *thing = make_creature(1, 1, 0);
    struct Room *room = make_room_at_slab(1, 0, 0, RoK_GUARDPOST, 0);
    room->total_capacity = 10;
    configure_room_role(RoK_GUARDPOST, RoRoF_DeadStorage); // arbitrary non-capacity-special role
    const CrtrStateId kContinueState = 42;
    configure_job(Job_GUARD, RoRoF_DeadStorage, 0, kContinueState);

    CHECK(at_guard_post_room(thing) == 1);

    struct CreatureControl *cctrl = creature_control_get(1);
    CHECK(cctrl->work_room_id == room->index);
    CHECK((cctrl->creature_control_flags & CCFlg_IsInRoomList) != 0);
    CHECK(thing->active_state == kContinueState);
}
