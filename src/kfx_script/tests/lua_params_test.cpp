// First kfx_script coverage, per docs/refactor/testing/comprehensive/
// stage-06-kfx-script-luajit.md's Tier 1 survey: lua_params.c's
// argument-checking helpers are pure Lua-stack operations. They need a
// live lua_State (LuaJIT's C API, already available via kfx_common_opts'
// kfx_luajit target -- no new dependency, per the fetch-not-system
// preference this stage was scoped under: LuaJIT itself is already
// fetched/built by Dependencies.cmake for the main program, reused here
// as-is), but deliberately NOT the full open_lua_script() init chain
// (Lvl_script global, reg_host_functions(), config/fxdata/lua/init.lua
// loading, ...) -- a bare luaL_newstate() plus a value pushed directly
// onto the stack is enough, the same "start with what needs the least
// setup" discipline every prior stage-04* pilot followed.
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <catch2/catch_test_macros.hpp>

#include "lua_params.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct LuaState {
    lua_State *L;
    LuaState() { L = luaL_newstate(); }
    ~LuaState() { lua_close(L); }
};
}

TEST_CASE_METHOD(LuaState, "luaL_checkIntMinMax accepts a value inside the range", "[kfx_script][lua_params]") {
    lua_pushinteger(L, 5);
    CHECK(luaL_checkIntMinMax(L, 1, 0, 10) == 5);
}

TEST_CASE_METHOD(LuaState, "luaL_optCheckinteger returns 0 when the argument is absent", "[kfx_script][lua_params]") {
    // Nothing pushed -- index 1 is "none" on an empty stack.
    CHECK(luaL_optCheckinteger(L, 1) == 0);
}

TEST_CASE_METHOD(LuaState, "luaL_optCheckinteger returns the argument when present", "[kfx_script][lua_params]") {
    lua_pushinteger(L, 42);
    CHECK(luaL_optCheckinteger(L, 1) == 42);
}

TEST_CASE_METHOD(LuaState, "luaL_checkstl_x accepts an in-bounds subtile coordinate", "[kfx_script][lua_params]") {
    // Pattern A (docs/refactor/testing/stage-02-testability-and-fakes.md
    // §2): luaL_checkstl_x range-checks against kfx_sim_state.map_subtiles_x.
    std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
    kfx_sim_state.map_subtiles_x = 100;

    lua_pushinteger(L, 50);
    CHECK(luaL_checkstl_x(L, 1) == 50);
}
