// Tier 3 for kfx_script, per docs/refactor/testing/comprehensive/
// stage-06-kfx-script-luajit.md §4's next action: a test against actual
// scripted behavior, not just the init chain succeeding (lua_base_test.cpp).
// config/fxdata/lua/classes/Pos3d.lua turned out to be exactly the
// "world-state-free binding" stage-06's original plan went looking for
// among the C API surface and didn't find (lua_api_camera.c's metatable
// objects needed a constructed userdata first; lua_api.c's global_methods
// were almost entirely world-state-coupled) -- it's realized in Lua, not
// C, and is pure arithmetic over the coordinates passed to Pos3d.new().
// Reused via require("classes.Pos3d") -- Lua caches require() results in
// package.loaded, so this doesn't reload the file, just re-fetches what
// init.lua already loaded via its own `require "classes.Pos3d"`.
#include "lua_script_fixture.h"

TEST_CASE_METHOD(LuaScriptFixture, "Pos3d.new stores the raw coordinates passed to it", "[kfx_script][lua_classes]") {
    REQUIRE(luaL_dostring(Lvl_script,
        "local Pos3d = require('classes.Pos3d')\n"
        "local p = Pos3d.new(512, 768, 256)\n"
        "return p.val_x, p.val_y, p.val_z") == LUA_OK);

    CHECK(lua_tointeger(Lvl_script, -3) == 512);
    CHECK(lua_tointeger(Lvl_script, -2) == 768);
    CHECK(lua_tointeger(Lvl_script, -1) == 256);
    lua_pop(Lvl_script, 3);
}

TEST_CASE_METHOD(LuaScriptFixture, "Pos3d computes stl_x/stl_y/slb_x as pure functions of the raw coordinates", "[kfx_script][lua_classes]") {
    // COORD_PER_STL = 256, COORD_PER_SLB = 768 (Pos3d.lua's own constants).
    REQUIRE(luaL_dostring(Lvl_script,
        "local Pos3d = require('classes.Pos3d')\n"
        "local p = Pos3d.new(512, 768, 0)\n"
        "return p.stl_x, p.stl_y, p.slb_x") == LUA_OK);

    CHECK(lua_tointeger(Lvl_script, -3) == 2); // floor(512 / 256)
    CHECK(lua_tointeger(Lvl_script, -2) == 3); // floor(768 / 256)
    CHECK(lua_tointeger(Lvl_script, -1) == 0); // floor(512 / 768)
    lua_pop(Lvl_script, 3);
}

TEST_CASE_METHOD(LuaScriptFixture, "Pos3d defaults unset coordinates to zero", "[kfx_script][lua_classes]") {
    REQUIRE(luaL_dostring(Lvl_script,
        "local Pos3d = require('classes.Pos3d')\n"
        "local p = Pos3d.new()\n"
        "return p.val_x, p.val_y, p.val_z") == LUA_OK);

    CHECK(lua_tointeger(Lvl_script, -3) == 0);
    CHECK(lua_tointeger(Lvl_script, -2) == 0);
    CHECK(lua_tointeger(Lvl_script, -1) == 0);
    lua_pop(Lvl_script, 3);
}
