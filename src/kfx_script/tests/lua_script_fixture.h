// Shared Tier 2 fixture (docs/refactor/testing/comprehensive/
// stage-06-kfx-script-luajit.md §3) for any kfx_script test that needs
// the real, fully-loaded production Lua environment -- not shared as a
// separate library the way kfx_game_state_test_stubs is (no second
// *_utest target needs it), just a header both of this target's
// own test files include, to avoid duplicating the same ~10 lines twice
// within one binary.
#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "kfx_script_test_paths.h" // KFX_REPO_CONFIG_DIR
#include "lua_base.h"
#include "config_keeperfx.h"
#include "kfx_sim_state.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>

struct LuaScriptFixture {
    LuaScriptFixture() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::strncpy(keeper_runtime_directory, KFX_REPO_CONFIG_DIR,
                     sizeof(keeper_runtime_directory) - 1);
        keeper_runtime_directory[sizeof(keeper_runtime_directory) - 1] = '\0';
        REQUIRE(open_lua_script(0));
    }
    ~LuaScriptFixture() { close_lua_script(); }
};
