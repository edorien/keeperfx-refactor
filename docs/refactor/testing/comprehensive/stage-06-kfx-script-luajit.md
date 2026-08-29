# Stage 6 — `kfx_script`: a LuaJIT test fixture

See [00-overview.md](00-overview.md) for context. `kfx_script` is
special-ranked (allowed to depend on anything below it,
architecture.md §1) and deliberately wide — 23 `kfx_sim` headers, 13
`kfx_config` headers (architecture.md §12.2) — so `kfx_script_utest`
would already be the widest-reaching `*_utest` binary by dependency count
before this stage adds anything. That's not this stage's hard problem,
though: linking the whole ladder is exactly what every prior stage-04*
document already established works. The hard problem is genuinely new —
this library's behavior lives partly in C (`src/kfx_script/src/lua_*.c`,
`api.c`) and partly in Lua source
(`config/fxdata/lua/{init.lua,aliases.lua,bindings/,classes/,managers/,
triggers/,gamelogic/,config-api/,core/,utils/}`, confirmed present in this
repo — not part of the original game's copyrighted data files CLAUDE.md
says aren't shipped here), and no prior stage needed to run code in a
second language runtime.

No new dependency is needed to get a `lua_State*` at all: LuaJIT is
already fetched/built for the main program by `Dependencies.cmake`'s
`kfx_luajit` target (system pkg-config first, `FetchContent`+
`ExternalProject` fallback otherwise — the same pattern the fetch-not-
system-package preference behind stage-05's `lcov` choice extends from).
`kfx_script_utest` just links `kfx_luajit` like every other consumer;
nothing script-testing-specific to add on the dependency-acquisition side.

## 1. Three tiers of what "testing kfx_script" can mean

Surveying `lua_params.h`/`lua_utils.h` found the two-tier split this
section originally proposed was one tier too coarse — every single
`lua_params.c` function takes a `lua_State*` (so none are truly
Tier 1's "no VM at all"), but several need nothing beyond a bare,
freshly-`luaL_newstate()`'d state with a value pushed directly onto its
stack — no `luaL_openlibs()`, no `reg_host_functions()`, no
`config/fxdata/lua/` loading at all. That's a real, distinct, and
*cheaper* middle tier, now confirmed landed (§3):

- **Tier 1 — bare `lua_State`, no init chain.** `lua_params.c`'s argument
  checkers that only touch the Lua stack plus (for some) a `kfx_sim_state`
  read — `luaL_checkIntMinMax`, `luaL_optCheckinteger`,
  `luaL_checkstl_x`/`_y` confirmed pure by reading their bodies, then by
  landing real tests against them (§3). The natural first candidates,
  filling the same role `bflib_math.c`'s `LbSqrL`/`LbLerp` played in
  stage 1 — proving the scaffold cheaply before anything harder.
- **Tier 2 — the real `open_lua_script()` init chain**, now known to be
  viable directly (§2) rather than needing a hand-rolled narrower path.
  Boots the actual production Lua environment (`reg_host_functions()`,
  `config/fxdata/lua/init.lua`, every real binding registered) and drives
  it with `luaL_dostring()`. Not landed yet — the natural next step.
- **Tier 3 — the Lua-callable bindings' *observable behavior* against
  world state** (`lua_api_room.c`'s room queries, `lua_api_things.c`'s
  thing manipulation, …) — needs Tier 2's fixture *plus* pattern-A setup
  of whatever `kfx_sim_state`/`kfx_config_state` the binding under test
  reads. The actual "protect the mod-facing API surface" goal this whole
  stage exists for; everything above is groundwork for reaching it
  cheaply and correctly.

## 2. What production code already does — traced, not guessed

`lua_base.c::open_lua_script(LevelNumber lvnum)` (line 296) is the real
entry point:

```c
Lvl_script = luaL_newstate();
luaL_openlibs(Lvl_script);
disable_lua_functions(Lvl_script);
lua_set_random_seed(kfx_sim_state.action_random_seed);
reg_host_functions(Lvl_script);          // registers every lua_api_*.c binding
setLuaPath(Lvl_script);

char* fname = prepare_file_fmtpath(FGrp_FxData, "lua/init.lua");   // mandatory
if (!LbFileExists(fname)) { ERRORLOG(...); return false; }
CheckLua(Lvl_script, luaL_dofile(Lvl_script, fname), "global_lua_file");

fname = prepare_file_fmtpath(FGrp_CmpgConfig, "lua/init.lua");     // optional
if (LbFileExists(fname)) { CheckLua(...); }

fname = prepare_file_fmtpath(get_level_fgroup(lvnum), "map%05lu.lua", lvnum); // optional
if (LbFileExists(fname)) { CheckLua(...); }

open_lua_script_for_mod_all(Lvl_script, lvnum);
```

**The open question this section originally raised is answered: the
mandatory load only needs the already-in-repo tree, confirmed by tracing
both the file-group resolution and the actual file on disk, not by
reading the call site alone.** `FGrp_FxData` resolves
(`config.c::_resolve_file_path_internal`) to
`${keeper_runtime_directory}/fxdata`, and `keeper_runtime_directory` is a
plain `extern char[152]` (`config_keeperfx.c`), zero-initialized by
default — a test fixture can just set it to point at this repo's
`config/` directory before calling `open_lua_script()`. Confirmed the
actual file exists at exactly that resolved path:
`config/fxdata/lua/init.lua` is present and tracked in this repo. The
*other* two loads (`FGrp_CmpgConfig`'s campaign `init.lua`, and the
per-level `map%05lu.lua`) are each guarded by `LbFileExists()` first — a
test fixture with no campaign/level data present makes both silently
no-op rather than fail, exactly the "gracefully degrades without real
game data" property stage-01's founding constraint needs. **`lvnum` never
gates the mandatory load at all** — it's read.

This means a fixture calling something very close to the real
`open_lua_script()` directly (not a narrower hand-rolled init path) is
viable, once `keeper_runtime_directory` is pointed at the repo's `config/`
directory — a genuine, lower-effort option than this document originally
expected, worth building once Tier 2 work actually starts (§4).

## 3. Tier 1 fixture (landed) and Tier 2 fixture (next)

**Tier 1, as actually landed** in `src/kfx_script/tests/lua_params_test.cpp`
— no `luaL_openlibs()` even, since none of the three functions tested
need any standard library loaded:

```cpp
struct LuaState {
    lua_State *L;
    LuaState() { L = luaL_newstate(); }
    ~LuaState() { lua_close(L); }
};

TEST_CASE_METHOD(LuaState, "luaL_checkIntMinMax accepts a value inside the range", "[kfx_script][lua_params]") {
    lua_pushinteger(L, 5);
    CHECK(luaL_checkIntMinMax(L, 1, 0, 10) == 5);
}
```

Plus `luaL_optCheckinteger` (both the "argument present" and "argument
absent" branches) and `luaL_checkstl_x` (pattern A: `memset`s
`kfx_sim_state`, sets `map_subtiles_x`, confirms the range check reads
it) — 4 `TEST_CASE`s total, all only asserting the *success* path.
`luaL_argcheck`'s failure path calls `lua_error()` (a C `longjmp`, not a
C++ exception LuaJIT's default build lets Catch2 intercept safely) — left
untested here deliberately, the same "don't reach for the hard case on
the first pass" discipline every stage-04* pilot followed. Testing it
properly needs the call wrapped in `lua_pcall` so the error is caught
inside Lua rather than unwinding past the C++ test frame — a Tier 1
follow-up, not attempted in this landing.

**Tier 2, landed** in `src/kfx_script/tests/lua_base_test.cpp` — calls
the real `open_lua_script()` directly, as §2 found viable:

```cpp
struct LuaScriptFixture {
    LuaScriptFixture() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::strncpy(keeper_runtime_directory, KFX_REPO_CONFIG_DIR,
                     sizeof(keeper_runtime_directory) - 1);
        keeper_runtime_directory[sizeof(keeper_runtime_directory) - 1] = '\0';
    }
    ~LuaScriptFixture() { close_lua_script(); } // null-safe if open_lua_script never ran
};

TEST_CASE_METHOD(LuaScriptFixture, "open_lua_script loads the real production init.lua chain", "[kfx_script][lua_base]") {
    REQUIRE(open_lua_script(0));
    lua_getglobal(Lvl_script, "Game"); // init.lua's own top-level `Game = {}`
    CHECK(lua_istable(Lvl_script, -1));
    lua_pop(Lvl_script, 1);
}
```

The `<repo-root>` path problem resolved exactly the way predicted:
`kfx_script_test_paths.h.in` → `configure_file()` (mirroring
`ver_defs.h.in`'s own pattern) bakes `KFX_REPO_CONFIG_DIR` in at compile
time — confirmed CWD-independent by actually running the binary from
`/tmp`, not just reasoned about.

**This is the whole stage's thesis, verified for real, not just argued
for**: the entire production Lua initialization chain —
`reg_host_functions()` registering every `lua_api_*.c` binding,
`setLuaPath()`, and every `require` in `init.lua` (`core.serialisation`,
`triggers.Events`/`Builtins`/`TriggerSystem`, `classes.Pos3d`/`Creature`/
`Thing`/`Slab`, `managers.CreatureManager`/`ThingManager`/`RoomManager`,
`gamelogic.ShotFunctions`, `utils.Debug`) — runs successfully in a bare
unit-test binary with zero original-game-data dependency, confirmed by
`Game` (the table `init.lua`'s own top-level code assigns only after
every `require` above succeeds) actually existing afterward.

Surveying `lua_api_camera.c`/`lua_api_lens.c` for a "no world-state
dependency" *binding* to test next (§1's original plan for the first real
Tier 2 test) found their registered functions are largely OOP-style
methods on metatable-backed objects (`camera_methods`/`camera_meta`),
needing a constructed userdata object first — more setup than the
`Game`-table smoke test above, and not attempted in this pass. The
`global_methods` table (`lua_api.c`, `Global_register()`) — plain
Lua-callable globals, not object methods — was also surveyed and found to
be almost entirely world-state-coupled (`AddCreatureToLevel`,
`SetTimer`, `RoomAvailable`, …); genuinely fewer "free" candidates here
than any prior library, consistent with `kfx_script` being the mod-facing
API surface stage-02 §5 predicted would be different from everything
before it.

## 4. Concrete next actions (in order)

1. ~~Trace `open_lua_script()` and `init.lua`'s file-loading calls.~~
   **Done — §2.**
2. ~~Survey `lua_utils.c`/`lua_params.c` for Tier-1 pure candidates; land
   those first.~~ **Done — §3.**
3. ~~Solve the repo-root-path problem; build a Tier 2 fixture and land a
   real test against it.~~ **Done — §3. The `Game`-table smoke test
   isn't quite "a world-state-free binding" (the original plan's target)
   but is arguably more valuable as a first Tier 2 test: it validates the
   *whole init chain* at once rather than one binding in isolation.**
4. ~~Land a test against actual scripted behavior, not just the init
   chain succeeding.~~ **Done, via a route §3's survey didn't
   anticipate**: `config/fxdata/lua/classes/Pos3d.lua` turned out to be
   exactly the "world-state-free binding" the original plan was looking
   for among the *C* API surface (`lua_api_camera.c`, `lua_api.c`'s
   `global_methods`) and didn't find there — it's realized in *Lua*
   instead. `Pos3d.new(x, y, z)` plus its `stl_x`/`stl_y`/`slb_x`
   computed properties (`__index` metamethod, pure arithmetic over the
   constructor's arguments) are entirely self-contained: no
   `kfx_sim_state`/`kfx_config_state` read anywhere in the class. Reused
   via `require('classes.Pos3d')` inside a `luaL_dostring()` snippet —
   Lua's own `package.loaded` cache means this doesn't reload the file,
   just re-fetches what `init.lua`'s own `require` already loaded.
   `lua_script_fixture.h` extracts the `LuaScriptFixture` shared between
   `lua_base_test.cpp` and this file (`lua_classes_test.cpp`) — a plain
   shared header, not a library the way `kfx_test_main`/
   `kfx_packet_test_stubs` are, since no second `*_utest` target needs it
   (yet). 3 tests: raw-coordinate storage, the computed-property
   arithmetic (verified against the class's own `COORD_PER_STL`/
   `COORD_PER_SLB` constants), and the `Pos3d.new()`-with-no-arguments
   default-to-zero path.
5. A real *C-binding* test (`lua_api_camera.c`/`lua_api.c`'s
   `global_methods`) is still open — needs either a metatable-backed
   object constructed first or a world-state-coupled global's pattern-A/B
   setup, genuinely more work than the Lua-class route turned out to be.
   Also still open: Tier 3 proper (bindings against real
   `kfx_sim_state`/`kfx_config_state` setup, not just pure Lua-side
   computation) and any `config/fxdata/lua/*.lua` script whose logic
   *isn't* self-contained the way `Pos3d.lua` happens to be.

## What this stage is not

Not a plan to test the HTTP/TCP API server (`api.c`) — that's networked,
stateful, and a different kind of testing problem (closer to `kfx_net`'s
transport-layer pattern-C territory, stage-02 §5) than anything a Lua
fixture solves. Out of scope for this document; flag separately if it
becomes a priority.
