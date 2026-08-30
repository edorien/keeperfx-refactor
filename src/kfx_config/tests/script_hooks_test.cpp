// kfx_config: script_hooks.c -- same callback-registration shape as
// net_callbacks_test.cpp. Every noop stub ignores its pointer args
// (verified by reading each one), so nullptr is safe throughout.
#include <catch2/catch_test_macros.hpp>

#include "script_hooks.h"

TEST_CASE("the default script_hooks table's every stub is a safe no-op returning its documented default", "[kfx_config][script_hooks]") {
    REQUIRE(script_hooks != nullptr);

    script_hooks->lua_on_power_cast(0, 0, 0, 0, 0, nullptr);
    script_hooks->lua_on_special_box_activate(0, nullptr);
    script_hooks->lua_on_creature_death(nullptr);
    script_hooks->lua_on_creature_rebirth(nullptr);
    script_hooks->lua_on_trap_placed(nullptr);
    script_hooks->lua_on_object_destroyed(nullptr);
    script_hooks->lua_on_apply_damage_to_thing(nullptr, 0, 0);
    script_hooks->lua_on_level_up(nullptr);
    script_hooks->lua_on_pick_up(nullptr, 0);
    script_hooks->lua_on_slap(nullptr, 0);
    script_hooks->lua_on_slab_kind_change(0, 0, 0);
    script_hooks->lua_on_slab_owner_change(0, 0, 0);
    script_hooks->lua_on_room_owner_change(nullptr, 0);
    script_hooks->lua_on_shot_hit(nullptr, nullptr, nullptr, 0, 0, false);
    script_hooks->lua_on_dungeon_destroyed(0);

    CHECK(script_hooks->luafunc_crstate_func(0, nullptr) == -1);
    CHECK(script_hooks->luafunc_thing_update_func(0, nullptr) == -1);
    CHECK(script_hooks->luafunc_shot_hit_thing_func(0, nullptr, nullptr, nullptr, 0, 0) == -1);
    CHECK(script_hooks->luafunc_magic_use_power(0, 0, 0, 0, 0, 0, nullptr, 0) == -1);
    CHECK(script_hooks->luafunc_trap_activation_func(0, nullptr, nullptr) == -1);

    script_hooks->api_event("test");
    script_hooks->api_event_with_data("test", nullptr, 0);

    script_hooks->lua_on_game_start();
    CHECK_FALSE(script_hooks->open_lua_script(1));
    CHECK_FALSE(script_hooks->execute_lua_code_from_console("x"));
    CHECK_FALSE(script_hooks->execute_lua_code_from_script("x"));
    script_hooks->generate_lua_types_file();
    size_t len = 999;
    CHECK(script_hooks->lua_get_serialised_data(&len) == nullptr);
    CHECK(len == 0);
    CHECK_FALSE(script_hooks->lua_set_serialised_data("x", 1));
    script_hooks->cleanup_serialized_data();
}

TEST_CASE("set_script_hook_callbacks installs a custom table and falls back to the default once cleared", "[kfx_config][script_hooks]") {
    struct ScriptHookCallbacks fake = *script_hooks;
    static bool called = false;
    called = false;
    fake.lua_on_game_start = []() { called = true; };

    set_script_hook_callbacks(&fake);
    script_hooks->lua_on_game_start();
    CHECK(called);

    set_script_hook_callbacks(nullptr);
    called = false;
    script_hooks->lua_on_game_start();
    CHECK_FALSE(called);
}
