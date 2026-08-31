// kfx_sim "room" cluster depth increment, per docs/refactor/testing/
// comprehensive/stage-08b-kfx-sim-clusters.md's "still open" list:
// room_garden.c's remove_food_from_food_room_if_possible(), the one
// self-contained function in this file. It reaches through
// get_room_thing_is_on() -> subtile_room_get() -> get_slabmap_for_subtile(),
// which is the first test in this plan to build the small
// map_subtiles/map_tiles + slabmap[].room_index + rooms[] fixture that
// "a thing standing in a room" needs -- this same shape will apply to
// every other creature/room function that calls get_room_thing_is_on
// (at_barrack_room, guarding, etc., stage-08b's "bigger increment" list).
#include <catch2/catch_test_macros.hpp>

#include "room_garden.h"
#include "room_data.h"
#include "thing_data.h"
#include "slab_data.h"
#include "kfx_config_state.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct RoomFixture {
    struct Thing *thing;
    struct Room *room;

    RoomFixture() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::memset(&kfx_config_state, 0, sizeof(kfx_config_state));

        kfx_sim_state.map_subtiles_x = 10;
        kfx_sim_state.map_subtiles_y = 10;
        kfx_sim_state.map_tiles_x = 4;
        kfx_sim_state.map_tiles_y = 4;
        kfx_config_state.neutral_player_num = PLAYER_NEUTRAL;

        thing = thing_get(1);
        thing->mappos.x.val = subtile_coord_center(4);
        thing->mappos.y.val = subtile_coord_center(4);
        thing->owner = 0;
        thing->food.life_remaining = -1;

        // Slab (1,1) (containing subtile (4,4)) is room index 1.
        struct SlabMap *slb = get_slabmap_for_subtile(4, 4);
        slb->room_index = 1;
        room = room_get(1);
        room->kind = RoK_GARDEN;
        room->owner = 0;
        room->used_capacity = 5;

        kfx_config_state.conf.slab_conf.room_types_count = RoK_GARDEN + 1;
        kfx_config_state.conf.slab_conf.room_cfgstats[RoK_GARDEN].roles = RoRoF_FoodStorage;
        kfx_config_state.conf.rules[0].gameplay.food_life_out_of_hatchery = 300;
    }
};
}

TEST_CASE_METHOD(RoomFixture, "remove_food_from_food_room_if_possible refuses a thing owned by the neutral player", "[kfx_sim][room_garden]") {
    thing->owner = PLAYER_NEUTRAL;
    room->owner = PLAYER_NEUTRAL;

    CHECK_FALSE(remove_food_from_food_room_if_possible(thing));
    CHECK(room->used_capacity == 5);
}

TEST_CASE_METHOD(RoomFixture, "remove_food_from_food_room_if_possible refuses a thing that already carries food", "[kfx_sim][room_garden]") {
    thing->food.life_remaining = 50; // already assigned -- not eligible to draw from the room

    CHECK_FALSE(remove_food_from_food_room_if_possible(thing));
    CHECK(room->used_capacity == 5);
}

TEST_CASE_METHOD(RoomFixture, "remove_food_from_food_room_if_possible refuses when the thing isn't standing in any room", "[kfx_sim][room_garden]") {
    get_slabmap_for_subtile(4, 4)->room_index = 0; // no room at this subtile

    CHECK_FALSE(remove_food_from_food_room_if_possible(thing));
}

TEST_CASE_METHOD(RoomFixture, "remove_food_from_food_room_if_possible refuses a room whose kind doesn't hold food", "[kfx_sim][room_garden]") {
    kfx_config_state.conf.slab_conf.room_cfgstats[RoK_GARDEN].roles = 0; // no RoRoF_FoodStorage

    CHECK_FALSE(remove_food_from_food_room_if_possible(thing));
    CHECK(room->used_capacity == 5);
}

TEST_CASE_METHOD(RoomFixture, "remove_food_from_food_room_if_possible refuses a room owned by a different player", "[kfx_sim][room_garden]") {
    room->owner = 3; // thing->owner is 0

    CHECK_FALSE(remove_food_from_food_room_if_possible(thing));
    CHECK(room->used_capacity == 5);
}

TEST_CASE_METHOD(RoomFixture, "remove_food_from_food_room_if_possible succeeds, decrements capacity, and starts the configured food timer", "[kfx_sim][room_garden]") {
    CHECK(remove_food_from_food_room_if_possible(thing));
    CHECK(room->used_capacity == 4);
    CHECK(thing->food.life_remaining == 300);
    CHECK(thing->parent_idx == -1);
}

TEST_CASE_METHOD(RoomFixture, "remove_food_from_food_room_if_possible still succeeds but doesn't underflow an already-zero used_capacity", "[kfx_sim][room_garden]") {
    room->used_capacity = 0;

    CHECK(remove_food_from_food_room_if_possible(thing));
    CHECK(room->used_capacity == 0);
    CHECK(thing->food.life_remaining == 300);
}
