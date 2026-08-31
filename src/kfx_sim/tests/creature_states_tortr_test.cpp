// kfx_sim "creature" cluster, hard-tail depth increment: the first test
// for creature_states_tortr.c. at_kinky_torture_room/at_torture_room are
// two more "at_X_room()" entry points (kfx_sim_test_fixtures.h), each
// additionally calling add_creature_to_torture_room (room_jobs.c) on
// success. That function's update_speed_of_player_creatures_of_model call
// (triggered the first time a model is tortured) walks
// dungeon->creatr_list_start, which stays 0 in this fixture (the creature
// is never added to the dungeon's *creature* list, only its *work-room*
// list, a different linked list) -- confirmed safe by an actual run, not
// assumed, since the walk is then a genuine no-op rather than something
// that happens to not crash by luck.
#include <catch2/catch_test_macros.hpp>

#include "creature_states_tortr.h"
#include "creature_control.h"
#include "dungeon_data.h"
#include "globals.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "at_kinky_torture_room fails when it isn't standing on a kinky-torture-role room", "[kfx_sim][creature_states_tortr]") {
    struct Thing *thing = make_creature(1, 1, 0);
    configure_job(Job_KINKY_TORTURE, RoRoF_Prison, 0);

    CHECK(at_kinky_torture_room(thing) == 0);
    CHECK(creature_control_get(1)->target_room_id == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_kinky_torture_room succeeds on a valid room: joins the torture room and initializes its torture bookkeeping", "[kfx_sim][creature_states_tortr]") {
    struct Thing *thing = make_creature(1, 1, 0);
    struct Room *room = make_room_at_slab(1, 0, 0, RoK_TORTURE, 0);
    room->total_capacity = 10;
    configure_room_role(RoK_TORTURE, RoRoF_DeadStorage);
    const CrtrStateId kContinueState = 42;
    configure_job(Job_KINKY_TORTURE, RoRoF_DeadStorage, 0, kContinueState);

    CHECK(at_kinky_torture_room(thing) == 1);

    struct CreatureControl *cctrl = creature_control_get(1);
    CHECK(cctrl->work_room_id == room->index);
    CHECK(cctrl->tortured.visual_state == CTVS_TortureGoToDevice);
    CHECK(cctrl->tortured.accumulated_torture_points == 0);
    CHECK(thing->active_state == kContinueState);
    CHECK(get_dungeon(0)->lvstats.creatures_tortured == 1);
    CHECK(get_dungeon(0)->tortured_creatures[thing->model] == 1);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_torture_room fails when it isn't standing on a painful-torture-role room", "[kfx_sim][creature_states_tortr]") {
    struct Thing *thing = make_creature(1, 1, 0);
    configure_job(Job_PAINFUL_TORTURE, RoRoF_Prison, 0);

    CHECK(at_torture_room(thing) == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "at_torture_room succeeds on a valid room: joins the torture room, sets CCFlg_NoCompControl, and initializes torture bookkeeping", "[kfx_sim][creature_states_tortr]") {
    struct Thing *thing = make_creature(1, 1, 0);
    struct Room *room = make_room_at_slab(1, 0, 0, RoK_TORTURE, 0);
    room->total_capacity = 10;
    configure_room_role(RoK_TORTURE, RoRoF_DeadStorage);
    const CrtrStateId kContinueState = 42;
    configure_job(Job_PAINFUL_TORTURE, RoRoF_DeadStorage, 0, kContinueState);

    CHECK(at_torture_room(thing) == 1);

    struct CreatureControl *cctrl = creature_control_get(1);
    CHECK(cctrl->work_room_id == room->index);
    CHECK((cctrl->creature_control_flags & CCFlg_NoCompControl) != 0);
    CHECK(cctrl->tortured.visual_state == CTVS_TortureGoToDevice);
    CHECK(thing->active_state == kContinueState);
}
