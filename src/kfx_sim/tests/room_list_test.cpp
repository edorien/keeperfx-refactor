// kfx_sim "room" cluster depth increment, per docs/refactor/testing/
// comprehensive/stage-08b-kfx-sim-clusters.md's "still open" list:
// room_list.c's per-player room bookkeeping. clear_rooms/
// count_player_rooms_of_type/count_player_rooms_entrances are pattern A on
// kfx_sim_state.rooms[]/dungeon->room_list_start[]. The
// nearest-room-for-thing family (get_player_room_of_kind_nearest_to and
// friends) is navigation/pathfinding-heavy and left for a later increment.
#include <catch2/catch_test_macros.hpp>

#include "room_list.h"
#include "room_data.h"
#include "dungeon_data.h"
#include "player_data.h"
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

TEST_CASE_METHOD(ResetState, "clear_rooms zeroes every room slot", "[kfx_sim][room_list]") {
    kfx_sim_state.rooms[3].kind = RoK_LIBRARY;
    kfx_sim_state.rooms[3].owner = 2;

    clear_rooms();

    CHECK(kfx_sim_state.rooms[3].kind == 0);
    CHECK(kfx_sim_state.rooms[3].owner == 0);
}

TEST_CASE_METHOD(ResetState, "count_player_rooms_of_type returns 0 for an out-of-range player", "[kfx_sim][room_list]") {
    CHECK(count_player_rooms_of_type(-1, RoK_LIBRARY) == 0);
    CHECK(count_player_rooms_of_type(DUNGEONS_COUNT, RoK_LIBRARY) == 0);
}

TEST_CASE_METHOD(ResetState, "count_player_rooms_of_type walks the per-kind room list to a count", "[kfx_sim][room_list]") {
    struct Dungeon *dungeon = get_dungeon(2);
    struct Room *room1 = room_get(1);
    room1->owner = 2;
    room1->kind = RoK_LIBRARY;
    room1->next_of_owner = 4;
    struct Room *room2 = room_get(4);
    room2->owner = 2;
    room2->kind = RoK_LIBRARY;
    room2->next_of_owner = 0; // end of list
    dungeon->room_list_start[RoK_LIBRARY] = 1;

    CHECK(count_player_rooms_of_type(2, RoK_LIBRARY) == 2);
}

TEST_CASE_METHOD(ResetState, "count_player_rooms_of_type stops without counting a room whose owner doesn't match the list's player", "[kfx_sim][room_list]") {
    struct Dungeon *dungeon = get_dungeon(2);
    struct Room *room1 = room_get(1);
    room1->owner = 3; // mismatched -- the loop aborts before incrementing
    room1->kind = RoK_LIBRARY;
    room1->next_of_owner = 0;
    dungeon->room_list_start[RoK_LIBRARY] = 1;

    CHECK(count_player_rooms_of_type(2, RoK_LIBRARY) == 0);
}

TEST_CASE_METHOD(ResetState, "calculate_player_num_rooms_built counts through the player's id_number, not the plyr_idx argument", "[kfx_sim][room_list]") {
    // A real, if surprising, indirection found by reading the body: it
    // resolves get_player(plyr_idx)->id_number and looks up *that*
    // dungeon's room lists, not plyr_idx's own.
    struct PlayerInfo *player = get_player(2);
    player->id_number = 5;

    kfx_config_state.conf.slab_conf.room_types_count = 2; // only rkind 1 is scanned
    kfx_config_state.conf.slab_conf.room_cfgstats[1].flags = 0; // counted (RoCFlg_NotCounted unset)

    struct Dungeon *dungeon5 = get_dungeon(5);
    struct Room *room = room_get(1);
    room->owner = 5;
    room->kind = 1;
    room->next_of_owner = 0;
    dungeon5->room_list_start[1] = 1;

    CHECK(calculate_player_num_rooms_built(2) == 1);
}

TEST_CASE_METHOD(ResetState, "calculate_player_num_rooms_built skips room kinds flagged RoCFlg_NotCounted", "[kfx_sim][room_list]") {
    struct PlayerInfo *player = get_player(2);
    player->id_number = 2;

    kfx_config_state.conf.slab_conf.room_types_count = 2;
    kfx_config_state.conf.slab_conf.room_cfgstats[1].flags = RoCFlg_NotCounted;

    struct Dungeon *dungeon = get_dungeon(2);
    struct Room *room = room_get(1);
    room->owner = 2;
    room->kind = 1;
    room->next_of_owner = 0;
    dungeon->room_list_start[1] = 1;

    CHECK(calculate_player_num_rooms_built(2) == 0);
}

TEST_CASE_METHOD(ResetState, "count_player_rooms_entrances walks the global entrance list by next_of_kind, filtered by owner", "[kfx_sim][room_list]") {
    struct Room *room1 = room_get(1);
    room1->owner = 2;
    room1->next_of_kind = 2;
    struct Room *room2 = room_get(2);
    room2->owner = 3;
    room2->next_of_kind = 0;
    kfx_sim_state.entrance_room_id = 1;

    CHECK(count_player_rooms_entrances(2) == 1);
    CHECK(count_player_rooms_entrances(3) == 1);
    CHECK(count_player_rooms_entrances(-1) == 2); // negative plyr_idx counts every entrance
}
