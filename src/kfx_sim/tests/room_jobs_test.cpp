// kfx_sim: room_jobs.c's worker_needed_in_dungeons_room_role(), the
// RoRoF_Research branch only -- it's the one role bit fully self-
// contained against a bare struct Dungeon (current_research_idx plus
// has_new_rooms_to_research(), both already covered directly in
// creature_states_rsrch_test.cpp). The other role branches
// (RoRoF_CratesManufctr/CrTrainExp/CrScavenge/...) reach into
// player_computer.c's get_dungeon_money_less_cost(), which itself calls
// compute_power_price() -- a kfx_config power-cost lookup -- not
// attempted here.
#include <catch2/catch_test_macros.hpp>

#include "room_jobs.h"
#include "dungeon_data.h"
#include "config_terrain.h" // RoRoF_Research

#include <cstring>

namespace {
struct ZeroedDungeon {
    ZeroedDungeon() { std::memset(&dungeon, 0, sizeof(dungeon)); }
    struct Dungeon dungeon;
};
}

TEST_CASE_METHOD(ZeroedDungeon, "worker_needed_in_dungeons_room_role(RoRoF_Research) is 0 when no research is selected", "[kfx_sim][room_jobs]") {
    dungeon.current_research_idx = -1;
    CHECK(worker_needed_in_dungeons_room_role(&dungeon, RoRoF_Research) == 0);
}

TEST_CASE_METHOD(ZeroedDungeon, "worker_needed_in_dungeons_room_role(RoRoF_Research) is 2 when new rooms still need research", "[kfx_sim][room_jobs]") {
    dungeon.current_research_idx = 0;
    dungeon.research_num = 1;
    dungeon.research[0].rtyp = RsCat_Room;
    dungeon.research[0].rkind = 5;
    dungeon.room_buildable[5] = 0;
    dungeon.room_resrchable[5] = 1;
    CHECK(worker_needed_in_dungeons_room_role(&dungeon, RoRoF_Research) == 2);
}

TEST_CASE_METHOD(ZeroedDungeon, "worker_needed_in_dungeons_room_role(RoRoF_Research) is 1 when research is active but no new rooms are pending", "[kfx_sim][room_jobs]") {
    dungeon.current_research_idx = 0;
    dungeon.research_num = 0; // has_new_rooms_to_research is false with an empty queue
    CHECK(worker_needed_in_dungeons_room_role(&dungeon, RoRoF_Research) == 1);
}
