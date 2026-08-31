# Stage 5 — Code coverage tooling

See [00-overview.md](00-overview.md) for why this is worth doing before
stages 6–8 add much more test volume. This stage answers: which tool,
how it's wired into the existing `KFX_BUILD_TESTS` CMake surface, what a
report looks like, and where it fits in CI — deliberately not "what
coverage percentage should we require," which is a separate, later
decision (§5).

## 1. Tool choice

The build toolchain is GCC (`gcc`/`g++`, confirmed throughout
`docs/refactor/testing/stage-01-framework-and-scaffold.md`'s verified
builds — CI's `unit-tests` job installs `build-essential`, not clang), so
`gcov` (GCC's own coverage instrumentation, driven by `--coverage` /
`-fprofile-arcs -ftest-coverage`) is the natural instrumentation layer —
no new compiler needed, and it already ships with the GCC install this
repo's CI already has, the same way it's part of any C/C++ toolchain
install. Two standard front ends turn raw `.gcda`/`.gcno` files into a
readable report; per this repo's own dependency philosophy (`Dependencies.cmake`
fetches/builds its own third-party deps rather than assuming they're
already on the system — see `CLAUDE.md`'s Build section), **neither
should be an `apt install`**:

| Tool | Verdict |
|---|---|
| **gcovr** | A single Python tool, both collects and renders. Rejected for the fetch-not-system-package requirement specifically: it's a pip-distributed package with a real dependency chain (Jinja2 and friends) that isn't a plain source tarball the way `kfx_fetch()` handles every other dependency in this codebase — "fetching" it cleanly would mean either vendoring a `pip install --target` tree (fragile, no precedent in this repo) or accepting a `pip`/system-package install after all, defeating the point. |
| **lcov** + **genhtml**, **2.x** (what `apt install lcov` currently gives on Ubuntu 24.04) | Also rejected. Verified directly: `genhtml` `use`s `DateTime`, `Date::Parse`, and `geninfo` `use`s `Capture::Tiny` — none are core Perl modules, so even a fetched *lcov* source tree would need CPAN-installed (or system-package) Perl modules alongside it, the same "clean fetch" failure as `gcovr`. |
| **lcov** + **genhtml**, **1.16** | **Chosen.** Verified directly (`grep '^use '` across `bin/lcov`/`bin/genhtml`/`bin/geninfo` in the fetched `v1.16` release tarball): every module used — `File::Basename`, `File::Temp`, `File::Copy`, `Getopt::Long`, `Digest::MD5`, `Cwd`, `IO::Uncompress::Gunzip`, `Module::Load(::Conditional)` — is a **core Perl module**, bundled with any Perl 5.14+ install (this environment: 5.40.1), nothing extra needed. `geninfo` optionally prefers `JSON::XS`/`Cpanel::JSON::XS` for speed but falls back to `JSON::PP`, core since 5.14 — confirmed by actually running it (below), not just reading the fallback logic. `lcov`/`genhtml` are plain, uncompiled Perl scripts, so "fetching" them is exactly `kfx_fetch()`'s existing download-and-extract-a-tarball pattern — no build step, no package manager, no version drift between a contributor's local run and CI's. |

**Verified end-to-end in this environment** before committing to this
choice (not just read about): a trivial `--coverage`-instrumented binary,
run once, `lcov --capture` (which auto-detected gcov 15.2.0's JSON
intermediate format and correctly fell back to `JSON::PP`), then
`genhtml` — produced a correct HTML report with no system Perl module
installs beyond the base `perl` binary. The one-version-back pin (1.16,
not the latest 2.x) is a deliberate, checked trade — not a "haven't gotten
around to updating" gap.

## 2. CMake wiring

A new option, layered on top of the existing `KFX_BUILD_TESTS` surface
(`CMakeLists.txt`, `docs/refactor/testing/stage-01-framework-and-scaffold.md`
§2) rather than replacing any of it. Fetching `lcov` reuses
`Dependencies.cmake`'s existing `kfx_fetch(<dir> <url>)` helper directly
— it already does exactly "download a tarball into
`${CMAKE_BINARY_DIR}/deps/<dir>` once, skip if already there" for every
other prebuilt dependency in this codebase (`astronomy`, `centijson`,
`enet6`, `libcurl`), and nothing about it is specific to compiled
libraries — a plain Perl-script tarball extracts the same way:

```cmake
# Only meaningful alongside KFX_BUILD_TESTS=ON; instruments every kfx_*
# OBJECT library AND keeperfx/keeperfx_hvlog in the same build tree (see
# §3 for why that's fine, even though it sounds like scope creep).
option(KFX_TEST_COVERAGE "Instrument kfx_* libraries for gcov coverage (native Linux, requires -DKFX_BUILD_TESTS=ON)" OFF)
if(KFX_BUILD_TESTS AND KFX_TEST_COVERAGE)
    target_compile_options(kfx_common_opts INTERFACE --coverage -O0 -g)
    target_link_options(kfx_common_opts INTERFACE --coverage)

    # lcov 1.16, not the latest 2.x -- see §1 for why (2.x needs non-core
    # Perl modules; 1.16 needs none). kfx_fetch() is defined in
    # Dependencies.cmake, already `include()`'d above -- this is the same
    # helper every other prebuilt dependency in this file uses, just
    # pointed at a Perl-script tarball instead of a compiled static lib.
    kfx_fetch(lcov "https://github.com/linux-test-project/lcov/releases/download/v1.16/lcov-1.16.tar.gz")
    set(KFX_LCOV_BIN "${CMAKE_BINARY_DIR}/deps/lcov/lcov-1.16/bin/lcov")
    set(KFX_GENHTML_BIN "${CMAKE_BINARY_DIR}/deps/lcov/lcov-1.16/bin/genhtml")

    # Runs the whole capture+filter+render pipeline (§4) as one target,
    # so `cmake --build out/coverage --target coverage` after `ctest` is
    # the entire local workflow. Depends on every *_utest target so
    # `--target coverage` alone (no separate build step first) is enough.
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E env perl "${KFX_LCOV_BIN}"
            --capture --directory "${CMAKE_BINARY_DIR}"
            --output-file "${CMAKE_BINARY_DIR}/coverage.raw.info"
            --gcov-tool gcov --quiet
        COMMAND perl "${KFX_LCOV_BIN}"
            --extract "${CMAKE_BINARY_DIR}/coverage.raw.info"
            "${CMAKE_SOURCE_DIR}/src/kfx_*/src/*"
            --output-file "${CMAKE_BINARY_DIR}/coverage.info" --quiet
        COMMAND perl "${KFX_GENHTML_BIN}"
            "${CMAKE_BINARY_DIR}/coverage.info"
            --output-directory "${CMAKE_BINARY_DIR}/coverage-html" --quiet
        DEPENDS
            kfx_platform_utest kfx_config_utest kfx_pathfinding_utest
            kfx_sim_utest kfx_render_utest kfx_net_utest kfx_game_utest
            kfx_frontend_utest
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Capturing coverage and rendering HTML report (out/coverage/coverage-html/index.html)")
endif()
```

placed alongside the existing `KFX_BUILD_TESTS`/Catch2 `FetchContent`
block, after `kfx_common_opts` is defined and after the last `*_utest`
target exists (i.e. after every `add_subdirectory(src/kfx_*)` call, since
the `coverage` target's `DEPENDS` list names them). `-O0` is deliberate,
not incidental: `gcov` line attribution is unreliable under optimization
(GCC can merge, reorder, or eliminate lines the source still "contains"),
and a coverage run's job is an accurate report, not runtime speed — a
`-DCMAKE_BUILD_TYPE=Debug` configure (already what stage-01's verified
workflow uses) plus this override is the right combination. The `--extract
".../src/kfx_*/src/*"` pattern (lcov's own scoping mechanism, the
`lcov`-side equivalent of `gcovr`'s rejected `--filter`) is what keeps
`_deps/` (SDL3/Catch2/ffmpeg/openal/luajit/etc., all built from source in
the same tree per `Dependencies.cmake`) and the `*_utest`/stub sources
themselves out of the report — see §4 for confirming this actually works,
not just reads plausibly.

## 3. Why instrumenting `kfx_common_opts` (and therefore `keeperfx` too) is fine

`kfx_common_opts` is the one `INTERFACE` target every `kfx_*` `OBJECT`
library, every `*_utest` binary, *and* `keeperfx`/`keeperfx_hvlog`
themselves all link (`CMakeLists.txt`) — so turning on `--coverage` there
instruments the whole build tree uniformly, including the real game
executables, not just test binaries. That sounds like scope creep, but it
isn't in practice: `KFX_TEST_COVERAGE` is opt-in and off by default, so a
normal `./build-cmake.sh` run is completely unaffected. Anyone who does
turn it on is, by construction, building a dedicated coverage tree (`out/
coverage/`, say) to run `ctest` against and then discard — not a
release build — so `keeperfx`/`keeperfx_hvlog` also being instrumented in
that one tree is harmless, and avoiding it would need per-`OBJECT`-library
conditional flags that don't actually buy anything (nothing links a
`kfx_*` `OBJECT` library's *coverage-instrumented* compilation into a
*non-instrumented* executable within the same build tree — CMake compiles
each `OBJECT` library's sources once per tree, shared by every consumer in
that tree, the same fact stage-01/04 already leaned on for why the
`*_utest` targets get real production code for free).

## 4. Producing a report

```bash
cmake -S . -B out/coverage -G Ninja -DKFX_OS=linux -DCMAKE_BUILD_TYPE=Debug \
  -DKFX_BUILD_TESTS=ON -DKFX_TEST_COVERAGE=ON
cmake --build out/coverage --target \
  kfx_platform_utest kfx_config_utest kfx_pathfinding_utest kfx_sim_utest \
  kfx_render_utest kfx_net_utest kfx_game_utest kfx_frontend_utest \
  -j"$(nproc)"
ctest --test-dir out/coverage --output-on-failure
cmake --build out/coverage --target coverage
# open out/coverage/coverage-html/index.html
```

The `coverage` custom target (§2) runs the capture → extract → render
pipeline in one step, so producing a report locally is exactly the same
`cmake --build`/`ctest` workflow stage 1–4 already established, plus one
more `--target coverage` at the end — not a second, parallel tool
invocation a contributor has to learn separately.

- `lcov --extract ... "src/kfx_*/src/*"` (§2) scopes the report to
  production `kfx_*` source, excluding `_deps/` (fetched third-party
  sources compiled in-tree), `src/main.cpp`, `src/ftests/` (functional-test
  scaffolding, not unit-tested code), and every `*_utest`/`tests/`
  directory (test code and stub sources — coverage *of* test code isn't
  the interesting number, only coverage *of* the production libraries the
  tests exercise) — none of those paths match the extract pattern.
- A separate build tree (`out/coverage`, not `out/linux`) is deliberate:
  the `--coverage`-instrumented objects are slower and behave differently
  under a debugger than the normal `out/linux` tree stage 1–4 already use
  for day-to-day test iteration; keeping them apart avoids surprising a
  contributor who just wants a fast `ctest` loop.

## 5. What this stage deliberately does not decide

- **No coverage floor / CI gate.** With eight libraries at a handful of
  pilot tests each (stage-04*), any percentage number right now would be
  dominated by how much of each library's code the current pilot happens
  to *not* exercise (i.e., almost all of it) — a floor set today would
  either be trivially low (meaningless) or immediately failing (blocking
  merges for reasons unrelated to what a contributor is actually
  touching). Revisit once stage-08's per-library follow-up passes have
  landed for at least a few libraries and there's a real baseline number
  to reason about, not a first-run number.
- **No CI job wiring decided yet.** Whether coverage generation joins the
  existing `unit-tests` job (stage-03) as an extra step, or gets its own
  job/workflow (given the separate build tree in §4, and that a coverage
  build is slower than the plain one), and whether/how a report gets
  surfaced on a PR (a build artifact upload, same pattern as
  `build-prototype`'s `.7z` upload; a PR comment; a badge) are all open —
  a maintainer call once §1–4 are actually implemented and there's a real
  report to look at, not before.
- **No third-party coverage service** (Codecov, Coveralls, …) evaluated.
  Would need an account/token a maintainer would have to set up; out of
  scope for a planning document to decide unilaterally.

## Exit criterion

- `-DKFX_TEST_COVERAGE=ON` alongside `-DKFX_BUILD_TESTS=ON` configures
  and builds cleanly in a dedicated `out/coverage` tree, verified against
  a real build the same way every prior stage was (stage-01's "Progress"
  section is the template: don't just write the CMake, run it).
- `-DKFX_TEST_COVERAGE=OFF` (or omitted, the default) leaves `out/linux`
  byte-for-byte unaffected, the same non-negotiable stage-01 already
  established for `KFX_BUILD_TESTS` itself.
- `lcov`'s fetch requires no `apt install`/`pip install`/CPAN step beyond
  the base `perl` binary — confirmed by the CMake configure succeeding
  and `cmake --build --target coverage` running to completion in an
  environment with no `lcov`/`gcovr`/extra Perl modules pre-installed.
- The rendered report correctly excludes `_deps/`, `src/ftests/`, and
  every `*_utest`/stub source, and correctly attributes coverage to the
  real `src/kfx_*/src/*.c(pp)` files the pilot tests already exercise (a
  sanity check: the handful of functions stage-04* tests already cover —
  `LbSqrL`, `parameter_is_number`, `small_around_index_in_direction`, …
  — should show as covered; everything else in those files should not).

## Progress

All of the above implemented and verified against a real build, not just
planned: `-DKFX_BUILD_TESTS=ON -DKFX_TEST_COVERAGE=ON` configured a fresh
`out/coverage` tree, fetched `lcov` 1.16 via `kfx_fetch()` (confirmed:
regular `[download NN% complete]` progress messages during configure, no
`apt`/`pip`/CPAN step involved), built all eight `*_utest` binaries with
`--coverage` instrumentation, and `cmake --build out/coverage --target
coverage` ran the full capture → extract → render pipeline to completion.

Confirmed correct, not just "ran without error":

- `grep '^SF:' coverage.info` lists only real `src/kfx_*/src/*.c(pp)`
  production files — no `_deps/` (SDL3/Catch2/ffmpeg/openal/luajit/…),
  no `centitoml`, no `*_utest`/`tests/`/stub source anywhere in the list.
  The `lcov --extract ".../src/kfx_*/src/*"` pattern (§2) does exactly
  what it was designed to do.
- Per-function attribution is exact: `bflib_math.c`'s `FNDA` (function
  call counts) show `LbSqrL` called 5 times and `LbLerp` called 3 times —
  matching `bflib_math_test.cpp`'s exact assertion counts (5 `CHECK`s
  against `LbSqrL` across its two `TEST_CASE`s, 3 against `LbLerp`) —
  while `LbSinL`/`LbCosL`, declared in the same file but not called by
  any current test, correctly show 0.
- **Baseline overall**: 0.2% line coverage (271/119,761), 0.5% function
  coverage (38/7,100) across the eight instrumented libraries — a real,
  now-measured number confirming the "28 tests across ~430 combined
  source files is a scaffold, not a safety net" framing from
  [`00-overview.md` §1](00-overview.md#1-why-now-not-earlier) wasn't just
  a plausible-sounding claim. This is the number stage-08's per-library
  follow-up passes should move, and the honest starting point for
  whenever a coverage floor (§5) becomes worth setting.
- **Re-measured after stage 8's per-library passes** (262 tests across
  all ten `kfx_*` libraries, `kfx_pathfinding`/`kfx_sim` now instrumented
  alongside the original eight): regenerated with the same `out/coverage`
  pipeline (`find out/coverage -name "*.gcda" -delete` first — rebuilding
  after source changes hits the same stale-checksum `libgcov` warning
  noted in the Errors/Fixes history, harmless but needs clearing), **1.4%
  line coverage (1,723/124,517), 3.1% function coverage (232/7,447)** —
  a real 7x/6x jump off the 0.2%/0.5% baseline, confirmed via `lcov
  --summary` on the fresh `coverage.info`, not just the HTML report's
  headline numbers.
- **Re-measured again** after the layering-residual-focused pass (302
  tests): **1.7% line coverage (2,164/124,517), 3.8% function coverage
  (281/7,447)**.
- **Re-measured again** after adding `kfx_net`'s `packets_misc.c`/
  `packets_input.c` accessor coverage and three more
  `creature_states_*.c` files (`_mood`/`_gardn`/`_rsrch`, 357 tests):
  **1.86% line coverage (2,329/124,942), 4.05% function coverage
  (303/7,473)**.
- **Re-measured again** after a dedicated `kfx_platform` (lowest-ranked
  library) push (491 tests): **2.28% line coverage (2,853/124,942),
  4.96% function coverage (371/7,473)**. `kfx_platform` itself went from
  41 to 175 tests, 2.3%→6.1% line / 5.2%→12.7% function coverage
  (`kfx_platform/src/index.html`) — every remaining zero-coverage file in
  that library needs a live SDL/OpenAL/ENet/CPUID dependency or is
  actively unsafe to exercise (signal handlers), see
  `docs/Architecture/testing-harness.md` §10.
- **Re-measured again** after moving one rung up the ladder to `kfx_config`
  (550 tests): **2.69% line coverage (3,360/124,942), 8.28% function
  coverage (619/7,473)**. `kfx_config` itself went from 45 to 104 tests,
  3.3%→8.7% line / 5.0%→32.0% function coverage
  (`kfx_config/src/index.html`) — the big win was exhaustively exercising
  seven `*Callbacks`-registration files' default no-op tables (~220 tiny
  static functions, only reachable through their table's function-pointer
  fields), plus `config_campaigns.c`'s struct-management functions and a
  worked per-loader TOML-fixture example (`config_textures.c`). What's
  left is mostly the large per-`config_*.c` TOML loaders, a volume
  problem (each needs its own fixture) rather than a capability gap like
  `kfx_platform`'s remaining SDL/OpenAL/ENet-bound files.
- **Re-measured again** after a third rung, `kfx_pathfinding` (567
  tests): **2.80% line coverage (3,500/124,942), 8.48% function coverage
  (634/7,473)**. `kfx_pathfinding` itself went from 41 to 58 tests,
  6.9%→9.7% line / 21.3%→28.7% function coverage
  (`kfx_pathfinding/src/index.html`) — this library started already
  reasonably well covered, so the gains were smaller and mostly closed
  gaps *within* already-tested `ariadne_*.c` files (findcache, tringls,
  navitree, naviheap, points, regions) rather than landing a whole new
  file. The "big three" (`ariadne.c`/`ariadne_update.c`/
  `ariadne_wallhug.c`, ~4,000 lines combined) remain untouched and are
  this library's one genuine "significant harness change" item — a
  51-entry `PathfindingWorldCallbacks` fake driving real pathfinding
  logic (the interface's own default-table registration is already
  covered, in `kfx_config`).
- **Re-measured again** after a fourth rung, `kfx_sim` (589 tests):
  **2.86% line coverage (3,571/124,942), 8.61% function coverage
  (643/7,473)**. `kfx_sim` itself went from 138 to 160 tests, 1.31%→1.45%
  line / 3.18%→3.50% function coverage (`kfx_sim/src/index.html`) — by
  far the smallest percentage move of the four libraries pushed so far,
  because `kfx_sim` is by far the largest (49,302 lines across ~90
  files, more than the other three combined) and the bulk of it needs a
  fuller `Thing`+`CreatureControl`+`Room`+`Dungeon` fixture this pass
  didn't build. Landed the pure/near-pure functions reachable without
  that: `map_locations.c`'s `TbMapLocation` bit-packing accessors,
  `room_jobs.c`'s `worker_needed_in_dungeons_room_role` (research
  branch), `map_ceiling.c`'s `ceiling_set_info`, `power_process.c`'s
  `players_disease_can_infect_target_players_creatures`. Closing a
  meaningful fraction of `kfx_sim` — starting with the 13 untested
  `creature_states_*.c` files and the ~4,800-line `player_comp*.c` AI
  cluster (`player_computer.c`/`player_comptask.c`/`player_compchecks.c`/
  `player_compprocs.c`/`player_compevents.c`, all still 0%) — is
  realistically a multi-session effort, not a single follow-up pass.
- **Re-measured again** after a fifth rung, `kfx_render` (604 tests):
  **2.96% line coverage (3,698/124,942), 9.02% function coverage
  (674/7,473)**. `kfx_render` itself went from 25 to 40 tests, 2.1%→3.1%
  line / 5.4%→11.1% function coverage (`kfx_render/src/index.html`) —
  function coverage more than doubled, the biggest relative jump of any
  library pushed so far, by finally landing the `LensEffect` C++ class
  hierarchy stage-08's kfx_render row had flagged since its original
  pass as "a natural fit... no fixture needed": every effect subclass
  whose `Draw()`/`Cleanup()` turned out safe to call on a never-`Setup()`
  instance (`Mist`/`Overlay`/`Displacement`/`Flyeye`/`LuaLensEffect`,
  confirmed by reading each body first) plus the `LensEffect` base
  class's own accessors. `PaletteEffect` (reaches into real player/
  palette state) and `LuaLensEffect`'s actual Lua-exposed pixel
  accessors (private, would need replicating an unexported struct
  layout) weren't attempted. The rest of this library --
  `engine_render.c` (5,618 lines), `custom_sprites.c`, `vidmode.c`,
  `cursor_tag.c`, and the `LensManager.cpp`/`lens_api.c` singleton --
  needs a live rendering surface or real map/player world state, the
  same "significant harness change" territory as `kfx_platform`'s
  `bflib_vidraw.c` family.
- **Re-measured again** after a sixth rung, `kfx_net` (612 tests):
  **2.98% line coverage (3,722/124,942), 9.05% function coverage
  (676/7,473)**. `kfx_net` itself went from 33 to 41 tests, 3.3%→4.0%
  line / 6.4%→8.6% function coverage (`kfx_net/src/index.html`).
  `net_checksums.c`'s `checksums_different()` (host-vs-client comparison,
  found `get_host_player_id()` is hardcoded to `0` which simplified the
  fixture) and `net_input_lag.c`'s two `network_is_active()`-gated early
  returns landed; the rest of `net_input_lag.c` mixes zero-accessor
  private state with real wall-clock reads and wasn't pursued further to
  avoid Catch2-randomized-order test dependencies. Most of the rest of
  this library (`net_matchmaking.c`, `net_game.c`,
  `net_exchange_common.c`/`_gameplay.c`, `net_lobby.c`, `net_lan.c`,
  `net_portforward.cpp`, `net_holepunch.c`, `net_main.c`) is genuine
  ENet/socket networking; `packets_cheats.c` is three giant
  `Thing`/`Room`/`Dungeon`-state dispatch functions, `kfx_sim`'s AI-
  cluster-style fixture territory.
- **Re-measured again** after a seventh rung, `kfx_game` (624 tests):
  **3.03% line coverage (3,790/124,942), 9.23% function coverage
  (690/7,473)**. `kfx_game` itself went from 13 to 25 tests, 0.3%→0.8%
  line / 0.6%→2.5% function coverage (`kfx_game/src/index.html`) —
  both roughly quadrupled off a very small starting base.
  `game_saves.c`'s creature-transfer bookkeeping, `game_heap.c`'s
  `setup_heap_manager` real-failure path (the "creature.jty" data file
  genuinely doesn't exist in the unit-test environment) plus `he_alloc`,
  and `lvl_script_conditions.c`'s `pop_condition`/`get_`/
  `set_script_current_condition` all landed. The overwhelming majority
  of this library is still untouched: `lvl_script_commands.c` (3,741
  lines, the largest single file in the whole codebase), `console_cmd.c`
  (1,990), `lvl_script_commands_old.c` (842), `lvl_script.c` (673), plus
  `game_saves.c`'s actual save/load chunk I/O and `sounds.c`'s real
  OpenAL/S3D functions — all needing the same kind of world-state or
  live-subsystem fixture as `kfx_sim`'s AI cluster or `kfx_platform`'s
  audio files.
- **Re-measured again** after an eighth rung, `kfx_frontend` (643
  tests): **3.08% line coverage (3,850/124,942), 9.38% function coverage
  (701/7,473)**. `kfx_frontend` itself went from 9 to 28 tests,
  0.2%→0.6% line / 0.6%→1.7% function coverage
  (`kfx_frontend/src/index.html`) — roughly tripled off a very small
  base, more than doubling the test count in the one library stage-08
  called "genuinely the hardest ... for this exercise". Two clean whole-
  file wins: `kfx_frontend_state.c` (raw-blob save/load/reset lifecycle,
  a real file round trip reusing `kfx_platform`'s scratch-file
  discipline) and `gui_frontmenu.c`'s pure `active_menus[]` scans (a
  *complete* array type this time, unlike some other extern arrays hit
  earlier). `gui_topmsg.c`'s `erstat_check` also landed, reusing
  `kfx_platform`'s `GetGameTurnFunc` pattern-B fixture. Everything else
  in this library remains genuinely GUI-widget-array/screen-coordinate
  coupled the way stage-04g originally found -- `frontend.cpp` (2,026
  lines), `front_input.c` (1,943), `frontmenu_ingame_tabs.c` (1,856),
  and the rest don't reduce to a bare-struct test.
- **Re-measured again** after a ninth rung, `kfx_script` (658 tests):
  **3.13% line coverage (3,914/124,942), 9.48% function coverage
  (708/7,473)**. `kfx_script` itself went from 8 to 23 tests, 4.6%→6.1%
  line / 6.4%→8.5% function coverage (`kfx_script/src/index.html`) —
  nearly tripled the test count. `lua_utils.c`'s `try_get_c_method`/
  `try_get_from_methods` (the shared `__index` metamethod helpers) got a
  real bare-`luaL_newstate()` fixture; `api.c`'s event-subscription
  bookkeeping turned out to be pure array management with zero socket
  involvement despite living in the external HTTP API server file, plus
  `get_max_flags`. Eight functions in `api.c` had no header declaration
  at all -- all added, though the var-subscription cluster (reaches into
  `kfx_game`'s `get_condition_value()`) wasn't pursued this pass. The
  rest of this library is the `lua_api_*.c` family's actual Lua-exposed
  bindings and `lua_triggers.c`/`lua_cfg_funcs.c`, needing either a real
  Thing/Room fixture behind the Lua call or the full `open_lua_script()`
  init chain already used in `lua_base_test.cpp`.
- **Re-measured again** after a tenth and final rung, `kfx_apploop` (662
  tests): **3.29% line coverage (3,932/119,387), 9.71% function coverage
  (710/7,308)**. The denominator here (119,387/7,308) is a correction to
  the previous bullet's (124,942/7,473) -- both numerators (3,914 lines,
  708 functions) are identical to the pre-this-round state, confirmed by
  remeasuring that exact commit after a clean `.gcda` wipe, so nothing
  regressed; the `coverage` target's `lcov --extract` pattern has simply
  never matched the 16 files one level below `src/kfx_platform/src/`
  (moved there by the recent platform/renderer flatten commit), and a
  stale build tree was masking that gap in the prior measurement. See
  `docs/Architecture/testing-harness.md` §10 for the full root-cause and
  the still-open tooling fix. `kfx_apploop` itself (a single file,
  `game_session_loop.cpp`) went from 4 to 8 tests, 4.5%→8.3% line /
  15%→25% function coverage (per-file, since this library has no
  `index.html` breakdown of its own beyond the one file). Landed the
  "threshold crossed" branch of `find_frame_rate()`/
  `packet_load_find_frame_rate()` (each already reached, but only their
  "still accumulating" branch, by the pre-existing tests' fixed-0 fake
  clock) by making that fake clock settable; `keeper_screen_swap()`
  (safe here -- its `RendererPresentFrame()` call no-ops on a
  default-null renderer in this test binary); one safe branch of
  `keeper_wait_for_next_turn()` that returns before touching the wall
  clock at all. The rest of this library -- `update()`, `game_loop()`,
  `wait_at_frontend()`, `keeper_gameplay_loop()` -- is the ladder's own
  composition root by design, not reducible to a unit test. This closes
  the ten-library "focus on the lowest-ranked library" push that started
  with `kfx_platform`.
- **Fixed the coverage-extract-pattern gap** the previous bullet
  flagged as still-open. Root cause was a missing `VERBATIM` on the
  `coverage` custom target (`CMakeLists.txt`), not the `--extract`
  pattern's shape: without it, CMake emitted the pattern unquoted on the
  generated Ninja command line, so the shell glob-expanded
  `src/kfx_*/src/*` against the real filesystem -- one path segment per
  `*` -- before lcov's own `*` -> `.*` regex matcher (already correct,
  already able to cross `/`) ever saw a literal `*`. Confirmed by
  reading the generated `build.ninja` directly (no quotes at all around
  the argument), then confirmed fixed the same way after adding
  `VERBATIM` (the argument now appears quoted). The 16 previously-
  invisible files under `src/kfx_platform/src/{platform,renderer,
  renderer/software}/` now appear in `coverage.info` (262 -> 277 `SF:`
  entries). Corrected overall totals, still 662 tests: **3.1% line
  coverage (3,949/125,589), 9.4% function coverage (715/7,629)** -- both
  the numerator and denominator moved up from the previous bullet's
  figures, since these files carry real coverage from earlier rounds on
  top of previously-uncounted total lines. See
  `docs/Architecture/testing-harness.md` §10 for the full record.
- **Deep pass with fixtures, restarting from the bottom of the ladder**
  (2026-08-30, per an explicit request to keep going past the earlier
  low-hanging-fruit-only passes, building fixtures/fake callbacks as
  needed this time). `kfx_platform` revisited first: 662 -> 735 tests,
  **3.6% line coverage (4,468/125,589), 10.5% function coverage
  (803/7,629)**. `kfx_platform` itself: 175 -> 248 tests, 6.1%->9.5%
  line / 12.7%->19.5% function coverage. The headline finding: the
  original pass's "real OpenAL device" verdict on `bflib_sound.c` was
  too coarse -- re-reading every function body found it's almost
  entirely pure 3D-audio bookkeeping (distance/volume/pan/pitch math,
  emitter/sample array management), with real OpenAL calls concentrated
  in a handful of specific branches (`stop_sample`/`play_sample`/
  `SetSample*`/`GetCurrentSoundMasterVolume`, still declined). Un-static'd
  `MaxNoSounds`/`SampleList` (extern in `bflib_sound.h`) to make "sample
  already playing" scenarios constructible via direct field writes
  instead of the real playback path -- the same pattern `kfx_config`'s
  `campaign` global already established, applied here for the first
  time in this library; landed ~30 functions this way. Also landed:
  `renderer/RendererManager.cpp` (not part of the original pass at all --
  its `s_active_renderer==nullptr`-guarded surface turned out to cover
  almost the entire file, plus a first pattern-B fake for its
  `RendererDrawCallbacks` seam), `bflib_vidraw.c`'s pure
  `LbSpriteSetScaling*Array`/`LbSpriteClearScaling*Array` functions (no
  screen buffer touched -- first coverage this 2,117-line file has ever
  had) and `bflib_mspointer.cpp`'s thin wrappers around them,
  `bflib_cpu.c`'s bit-field decoders, `bflib_netsession.c`,
  `moonphase.c`'s process-start-zero default. Twelve functions had real
  external linkage but no header declaration; all added. Full detail:
  `docs/Architecture/testing-harness.md` §8/§10.
- **`kfx_config` revisited next in the same deep pass**: 740 tests total,
  **3.68% line coverage (4,627/125,589), 12.22% function coverage
  (932/7,629)**. `kfx_config` itself: 104 -> 109 tests, 8.8%->10.5%
  line / 32.2%->46.3% function coverage. `sim_feedback.c` was an eighth
  `*Callbacks`-registration file the original sweep of seven simply
  missed -- same shape (113 tiny static noop functions), landed with one
  exhaustive test, most of this round's function-coverage jump.
  `config_powerhands.c` got a second worked TOML-fixture-loader example.
  The ~20 remaining `config_*.c` loaders are still a volume problem, not
  attempted exhaustively. Full detail: `docs/Architecture/
  testing-harness.md` §8/§10.
- **`kfx_config`, second sub-round (2026-08-30, after an explicit
  redirect to keep pushing the config_*.c volume rather than stop at
  one or two examples)**: 771 tests total, **4.09% line coverage
  (5,142/125,589), 13.61% function coverage (1,038/7,629)**.
  `kfx_config` itself: 109 -> 138 tests, 10.5%->15.9% line /
  46.3%->57.6% function coverage. Found a ninth missed `*Callbacks`
  file -- `config.c`'s own 93-field `ConfigReloadCallbacks`, the
  largest in this library, landed in one test case (72 assertions).
  Found and confirmed via an actual failing test (not read-only
  inspection) that `config_cubes.c`'s `Name` field is spelled with the
  wrong case for `config.c`'s `set_defaults()` auto-registration check
  (needs `"NAME"`, has `"Name"`), so `cube_desc[]` never populates and
  `cube_code_name()` always returns "INVALID" -- dormant, nothing
  outside the file calls it. Landed three worked examples of a second,
  previously-zero-coverage loader family, `NamedField`/
  `parse_named_field_blocks` (9 files use it): `config_cubes.c`,
  `config_lenses.c`, `config_crtrstates.c`. Plus `config_translation.c`
  (TOML-backed alias lookup) and `highscores.c`'s
  `add_high_score_entry()` (sorted-array insertion algorithm on the
  module-level `campaign` global). ~15 `config_*.c` loaders remain, now
  with two proven fixture patterns instead of one.
- **`kfx_pathfinding` revisited in the same deep pass**: `kfx_pathfinding`
  itself: 58 -> 60 tests, 9.7%->9.9% line / 28.7%->29.7% function
  coverage. `ariadne.c` (largest of the "big three", 3,313 lines) got
  its first-ever coverage via a minimal single-field
  `PathfindingWorldCallbacks` fake (`thing_nav_block_sizexy`/
  `thing_nav_sizexy`, each calling exactly one callback). A third
  candidate, `tag_open_closed_init()`, turned out to be dead code inside
  a `/* TODO PATHFINDING Enable when needed */` comment block -- caught
  by a link failure, not assumed from reading. The rest of the "big
  three" still needs the full 51-entry functional fake. Full detail:
  `docs/Architecture/testing-harness.md` §8/§10.
- **`kfx_config`, third sub-round**: 779 tests total, **4.14% line
  coverage (5,205/125,589), 13.72% function coverage (1,047/7,629)**.
  `kfx_config` itself: 138 -> 146 tests, 15.9%->16.6% line /
  57.6%->58.6% function coverage. `config_objects.c` -- a fourth
  `NamedField` worked example, plus room-role capacity branches and
  `crate_thing_to_workshop_item_class`/`_model` exercised directly
  against `config_reload_callbacks`'s already-verified default no-op
  stubs. `config_effects.c` surveyed and found to mix both loader
  families (raw TOML dict traversal plus a nested `NamedField`
  sub-config) in one file -- a third shape, not attempted. ~14
  `config_*.c` loaders remain, still a volume problem. Full detail:
  `docs/Architecture/testing-harness.md` §8/§10.
- **Fourth `kfx_config` sub-round + second `kfx_pathfinding` sub-round**
  (user pointed out more was reachable in both), 788 tests total,
  **4.22% line coverage (5,300/125,589), 13.83% function coverage
  (1,055/7,629)**. `kfx_config`: 146 -> 153 tests, 17.4% line / 58.9%
  function coverage. `config_slabsets.c` split in two:
  `load_columns_config_file()` needed no fake (writes straight into
  `kfx_config_state`); `load_slabset_config_file()`/`clear_slabsets()`
  needed a real `ConfigReloadCallbacks` fake with real backing arrays
  -- the first `*Callbacks` reach in this library where the *default*
  itself isn't safe to call (`get_slabset_array`/etc. all default to
  `NULL`). Found and fixed a dotted-TOML-header parse failure
  (`[slab0.S]` alone doesn't parse without an explicit `[slab0]`
  parent under this codebase's bundled TOML impl) by matching the real
  `config/fxdata/slabset.toml`'s own structure. `kfx_pathfinding`: 60
  -> 62 tests, 10.3% line / 32.2% function coverage. `ariadne_update.c`
  (second-largest of the "big three") got its first-ever coverage: five
  pure `kfx_pathfinding_state` bookkeeping functions, no map/callback
  dependency. `ariadne_wallhug.c`'s `set_hugging_pos_using_blocked_flags`
  was assessed and declined -- its coordinate math writes through
  `struct Coord3d`'s packed union in a way that depends on unconfirmed
  platform struct layout, not something safely hand-verifiable. Full
  detail: `docs/Architecture/testing-harness.md` §8/§10.
- **Fifth `kfx_config` sub-round** (explicit "keep going until
  significant line coverage" request), 821 tests total, **4.40% line
  coverage (5,524/125,589), 14.38% function coverage (1,097/7,629)**.
  `kfx_config`: 153 -> 186 tests, 19.8% line / 63.5% function coverage.
  A technique shift for `config_terrain.c`/`config_trapdoor.c`/
  `config_rules.c`/`config_settings.c`: direct `kfx_config_state` field
  writes (pattern A) instead of a real loader fixture, reaching a dozen
  pure slab/room-kind predicates in `config_terrain.c` alone.
  `create_manufacture_array_from_trapdoor_data()` fully covered. Found
  a real bug in `config_rules.c`, confirmed by an actual failing test:
  `sac_compare_fn` (the `qsort` comparator for `add_sacrifice_victim`'s
  victim list) returns a plain bool, never negative -- violates
  `qsort`'s contract, so the sort order is implementation-defined; the
  test was rewritten to not depend on it. `config_settings.c`'s
  `load_settings()`/`save_settings()` declined -- unlike every other
  loader here, they hardcode a real save-directory path with no `fname`
  parameter to redirect. Full detail: `docs/Architecture/
  testing-harness.md` §8/§10.
- **`kfx_pathfinding`'s full functional fake** (same "keep going until
  significant line coverage" request, continued for this library), 892
  tests total, **5.96% line coverage (7,489/125,589), 16.48% function
  coverage (1,257/7,629)**. `kfx_pathfinding`: 62 -> 87 tests, **10.3%
  -> 40.4% line coverage, 32.2% -> 77.2% function coverage**. Built the
  full 51-entry `PathfindingWorldCallbacks` fake the two prior
  sub-rounds explicitly deferred (`pathfinding_fake_world.h`) -- a
  reusable grid-backed fake with real controllable backing storage for
  map/slab/thing state, since those structs are opaque to ariadne and
  never dereferenced by production code. `ariadne_wallhug.c` (2,178
  lines, zero coverage before this round) now covered via its 5
  real-external-linkage entry points, transitively exercising every
  `static` helper in the file. `ariadne_update.c`'s
  `init_navigation()`/`update_navigation_triangulation()` now drive the
  real Delaunay triangulation algorithm end-to-end (68.4% line
  coverage), transitively exercising real triangulation machinery
  across five other files for the first time. `ariadne.c`'s
  route-lifecycle entry points driven across a genuinely-triangulated
  map took it from 2.5% to 25.3% line coverage. Caught and fixed a real
  order-dependent test bug via the routine 3x `--order rand` stability
  check: a pre-existing `ariadne_update_test.cpp` assertion compared
  `count_Triangles` against a "before" snapshot that isn't a reliable
  bound once another test triangulates the identical map shape first
  (re-triangulating an identical-shaped map is idempotent, not
  cumulative) -- fixed to assert an absolute structural bound instead.
  Full detail: `docs/Architecture/testing-harness.md` §8/§10.
