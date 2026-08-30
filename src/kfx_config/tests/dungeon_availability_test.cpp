// kfx_config: dungeon_availability.c -- same callback-registration shape
// as net_callbacks_test.cpp. Several fields share the same underlying
// noop_* function (e.g. all four dungeon-validity checks route through
// noop_get_bool) -- each field is still called individually below since
// that documents each field's own contract, even though gcov only needs
// one call per distinct static function for full coverage.
#include <catch2/catch_test_macros.hpp>

#include "dungeon_availability.h"

TEST_CASE("the default dungeon_availability table's every stub is a safe no-op returning its documented default", "[kfx_config][dungeon_availability]") {
    REQUIRE(dungeon_availability != nullptr);

    CHECK_FALSE(dungeon_availability->player_has_valid_dungeon(0));
    CHECK_FALSE(dungeon_availability->player_has_valid_dungeon_with_heart(0));
    CHECK_FALSE(dungeon_availability->players_num_dungeon_valid(0));
    CHECK_FALSE(dungeon_availability->players_num_dungeon_valid_with_heart(0));

    dungeon_availability->set_creature_availability(0, 0, 1, 0);
    dungeon_availability->try_set_backup_heart_idx(0, 0);

    CHECK_FALSE(dungeon_availability->set_room_resrchable_and_buildable(0, 0, 1, 1));
    CHECK_FALSE(dungeon_availability->get_room_resrchable(0, 0));
    dungeon_availability->set_all_room_resrchable(0);
    CHECK_FALSE(dungeon_availability->get_room_buildable(0, 0));
    dungeon_availability->set_all_room_buildable_from_resrchable(0);

    CHECK_FALSE(dungeon_availability->get_magic_resrchable(0, 0));
    dungeon_availability->set_magic_resrchable(0, 0, true);
    dungeon_availability->set_all_magic_resrchable_unchecked(0);
    CHECK_FALSE(dungeon_availability->get_magic_level_gt0(0, 0));

    CHECK_FALSE(dungeon_availability->get_trap_placeable(0, 0));
    CHECK_FALSE(dungeon_availability->get_trap_manufacturable(0, 0));
    CHECK_FALSE(dungeon_availability->get_trap_built(0, 0));
    CHECK_FALSE(dungeon_availability->get_door_placeable(0, 0));
    CHECK_FALSE(dungeon_availability->get_door_manufacturable(0, 0));
    CHECK_FALSE(dungeon_availability->get_door_built(0, 0));
}

TEST_CASE("set_dungeon_availability_callbacks installs a custom table and falls back to the default once cleared", "[kfx_config][dungeon_availability]") {
    struct DungeonAvailabilityCallbacks fake = *dungeon_availability;
    static bool called = false;
    called = false;
    fake.player_has_valid_dungeon = [](PlayerNumber) -> TbBool { called = true; return true; };

    set_dungeon_availability_callbacks(&fake);
    CHECK(dungeon_availability->player_has_valid_dungeon(0));
    CHECK(called);

    set_dungeon_availability_callbacks(nullptr);
    called = false;
    CHECK_FALSE(dungeon_availability->player_has_valid_dungeon(0));
    CHECK_FALSE(called);
}
