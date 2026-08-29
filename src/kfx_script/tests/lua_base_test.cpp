// Tier 2 for kfx_script, per docs/refactor/testing/comprehensive/
// stage-06-kfx-script-luajit.md §2/§3: calls the real open_lua_script()
// production entry point directly, rather than a hand-rolled narrower
// init -- confirmed viable by tracing its file-loading calls (the
// mandatory init.lua load only needs this repo's own config/fxdata/lua/
// tree, not original game data; campaign/level/mod loads gracefully
// no-op via LbFileExists() guards when absent).
#include "lua_script_fixture.h"

TEST_CASE_METHOD(LuaScriptFixture, "open_lua_script loads the real production init.lua chain", "[kfx_script][lua_base]") {
    // init.lua's own top-level code defines Game = {} once every
    // `require`d module (core.serialisation, triggers.*, classes.*,
    // managers.*, gamelogic.ShotFunctions, utils.Debug) has loaded
    // without error -- the simplest real signal that the whole chain
    // (reg_host_functions()'s C bindings, setLuaPath(), every require)
    // actually ran, not just that open_lua_script() returned true.
    lua_getglobal(Lvl_script, "Game");
    CHECK(lua_istable(Lvl_script, -1));
    lua_pop(Lvl_script, 1);
}
