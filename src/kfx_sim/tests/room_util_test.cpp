// kfx_sim "room" cluster depth increment, per docs/refactor/testing/
// comprehensive/stage-08b-kfx-sim-clusters.md's "still open" list:
// room_util.c's recompute_rooms_count_in_dungeons(), the one
// self-contained pattern-A function in this file -- it's a thin wrapper
// over room_list.c's count_player_rooms_of_type/room_is_counted, both
// already covered directly in room_list_test.cpp. The rest of the file
// (flame effects, slab destruction/recreation, script-driven slab
// replacement, room-role capacity events) needs a full map+thing fixture
// and is left for a later increment.
#include <catch2/catch_test_macros.hpp>

#include "room_util.h"
#include "room_data.h"
#include "dungeon_data.h"
#include "kfx_config_state.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct ResetState {
    ResetState() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::memset(&kfx_config_state, 0, sizeof(kfx_config_state));
    }
};
}

TEST_CASE_METHOD(ResetState, "recompute_rooms_count_in_dungeons sums each dungeon's counted room kinds", "[kfx_sim][room_util]") {
    kfx_config_state.conf.slab_conf.room_types_count = 2; // only rkind 1 is scanned
    kfx_config_state.conf.slab_conf.room_cfgstats[1].flags = 0; // counted

    struct Dungeon *dungeon0 = get_dungeon(0);
    struct Room *room = room_get(1);
    room->owner = 0;
    room->kind = 1;
    room->next_of_owner = 0;
    dungeon0->room_list_start[1] = 1;

    struct Dungeon *dungeon1 = get_dungeon(1); // no rooms

    recompute_rooms_count_in_dungeons();

    CHECK(dungeon0->total_rooms == 1);
    CHECK(dungeon1->total_rooms == 0);
}

TEST_CASE_METHOD(ResetState, "recompute_rooms_count_in_dungeons skips room kinds flagged RoCFlg_NotCounted", "[kfx_sim][room_util]") {
    kfx_config_state.conf.slab_conf.room_types_count = 2;
    kfx_config_state.conf.slab_conf.room_cfgstats[1].flags = RoCFlg_NotCounted;

    struct Dungeon *dungeon0 = get_dungeon(0);
    struct Room *room = room_get(1);
    room->owner = 0;
    room->kind = 1;
    room->next_of_owner = 0;
    dungeon0->room_list_start[1] = 1;

    recompute_rooms_count_in_dungeons();

    CHECK(dungeon0->total_rooms == 0);
}
