# KeeperFX — Test & Coverage Harness

This document is the current, authoritative description of KeeperFX's
**unit-test harness** — the Catch2-based `KFX_BUILD_TESTS` machinery — and
its coverage tooling. Like [`architecture.md`](architecture.md), it
describes what's true *now*; `docs/refactor/testing/` is the historical
narrative of how this was built, stage by stage, and shouldn't be trusted
to track current code (see that directory's own framing, and
`architecture.md` §14's document map).

Two independent test mechanisms exist in this codebase; this document
covers only the second:

- **`src/ftests/`** — in-game functional tests, exercised against a real
  running game via `-ftests`. See `architecture.md` §9 and
  [`src/ftests/README.md`](../../src/ftests/README.md).
- **The unit-test harness** (`KFX_BUILD_TESTS`) — this document.

---

## 1. Shape

One Catch2 v3.7.1 test binary per `src/kfx_*/` library:
`kfx_platform_utest`, `kfx_config_utest`, `kfx_pathfinding_utest`,
`kfx_sim_utest`, `kfx_render_utest`, `kfx_net_utest`, `kfx_game_utest`,
`kfx_frontend_utest`, `kfx_script_utest`, `kfx_apploop_utest` — all ten
`kfx_*` libraries have one; none are still missing. Test sources live in
each library's own `src/kfx_<name>/tests/` directory, globbed by that
directory's own `CMakeLists.txt` (same glob-per-library convention the
main build uses — see `architecture.md` §7.1). Registered with CTest via
`catch_discover_tests(..., TEST_PREFIX "kfx_<name>.")`, so `ctest -R
kfx_sim` matches every test in that binary and every test's full name is
namespaced (e.g. `kfx_sim.player_has_lost is false for an invalid
player`).

Gated by two off-by-default CMake options, native Linux only (no Windows
CI runner/Wine step exists to execute a mingw-cross-compiled test
binary):

```cmake
option(KFX_BUILD_TESTS "Build Catch2 unit tests for each kfx_* library (native Linux only)" OFF)
option(KFX_TEST_COVERAGE "Instrument kfx_* libraries for gcov/lcov coverage (requires -DKFX_BUILD_TESTS=ON)" OFF)
```

A contributor who never passes `-DKFX_BUILD_TESTS=ON` sees no new
targets, no Catch2 `FetchContent`, no new required tools — the default
build is unaffected.

## 2. Building and running

```bash
cmake -S . -B out/linux -G Ninja -DKFX_OS=linux -DKFX_BUILD_TESTS=ON
cmake --build out/linux --target \
  kfx_platform_utest kfx_config_utest kfx_pathfinding_utest kfx_sim_utest \
  kfx_render_utest kfx_net_utest kfx_game_utest kfx_frontend_utest \
  kfx_script_utest kfx_apploop_utest -j"$(nproc)"

ctest --test-dir out/linux --output-on-failure       # everything
ctest --test-dir out/linux -R kfx_sim                # one library
./out/linux/src/kfx_sim/tests/kfx_sim_utest -s        # a binary directly, verbose
./out/linux/src/kfx_sim/tests/kfx_sim_utest "[room_data]"  # a Catch2 tag filter
```

Each `*_utest` binary is also directly runnable with any Catch2 CLI flag
(`--order rand`, `-s` for successful-assertion output, a free-text
substring to filter `TEST_CASE` names, a `[tag]` filter matching the
`"[kfx_<name>][<file>]"` tags every `TEST_CASE` in this codebase carries).

## 3. Per-library link shape

Every `*_utest` target links the **real** `kfx_*` OBJECT library it
tests, plus every real library below it on the dependency ladder
(`architecture.md` §1) — not a mocked-out version. This was a deliberate
early finding (`docs/refactor/testing/stage-01-framework-and-scaffold.md`):
once `check_layering_symbols.py`'s accepted residuals were fixed
(`docs/refactor/todo/check-layering-symbol-level-blind-spot.md`), linking
the whole real dependency chain "just works" for most libraries, and only
a handful of genuine residual gaps need a stub.

| Target | Links (besides itself + everything below it) | Stub libraries needed |
|---|---|---|
| `kfx_platform_utest` | — | — |
| `kfx_config_utest` | `centitoml` (explicit — see §5) | — |
| `kfx_pathfinding_utest` | `centitoml` | — |
| `kfx_sim_utest` | `centitoml` | `kfx_packet_test_stubs`, `kfx_sim_test_stubs.cpp` (compiled in directly) |
| `kfx_render_utest` | `centitoml` | `kfx_packet_test_stubs` |
| `kfx_net_utest` | `centitoml` | `kfx_game_state_test_stubs`, `kfx_frontend_state_test_stub` |
| `kfx_game_utest` | `centitoml` | `kfx_frontend_state_test_stub` only (not `kfx_game_state_test_stubs` — `kfx_game` itself provides the real symbols) |
| `kfx_frontend_utest` | `centitoml` | none (linking `kfx_net`+`kfx_game`+`kfx_frontend` for real resolves everything) |
| `kfx_script_utest` | `centitoml`, `kfx_luajit` | none |
| `kfx_apploop_utest` | `centitoml`, `kfx_luajit` | none |

Every target also links `kfx_bfdebug_std` (the `BFDEBUG_LEVEL=0` variant
— test binaries are never built `_hvlog`) and `kfx_test_main` (below).
`centitoml` needs an explicit link on every target that transitively
reaches `kfx_config` (i.e. all of them): it's an OBJECT library wrapped
two `INTERFACE`-library hops deep inside `kfx_common_opts`, which doesn't
reliably survive into a final link — the same reason root
`CMakeLists.txt` links it directly onto `keeperfx`/`keeperfx_hvlog`
rather than trusting the `INTERFACE` chain alone.

## 4. Shared test-support code

**`kfx_test_main`** (`src/kfx_platform/tests/kfx_test_main.cpp`, defined
first so every later `tests/CMakeLists.txt` can reference it by name — a
CMake target name is global to the whole build) forwards `kfxmain()` —
the entry point `kfx_platform`'s `PlatformLinux.cpp` really owns as the
process `main()` — into `Catch::Session().run(argc, argv)`. Every
`*_utest` target links plain `Catch2::Catch2` (via this library's
`PUBLIC` dependency on it), never `Catch2::Catch2WithMain`, which would
define its own `main` that a linker already holding `kfx_platform`'s
object files would silently drop rather than conflict on.

Four stub libraries paper over `check_layering_symbols.py`'s accepted
residuals (`architecture.md` §8.2) — cases where a *type* is declared in
a lower-ranked library by design, but the functions operating on it are
really implemented in a higher-ranked one, so a test binary that doesn't
happen to link that higher library needs a stand-in:

- **`kfx_packet_test_stubs`** (`src/kfx_sim/tests/packet_test_stubs.cpp`)
  — fakes `get_packet`/`get_packet_direct`/`set_packet_action`/
  `set_players_packet_action` (really implemented in `kfx_net`'s
  `packets.c`). Shared: any `*_utest` that transitively links `kfx_sim`
  but not `kfx_net` needs it (`kfx_sim_utest`, `kfx_render_utest`).
- **`kfx_game_state_test_stubs`** / **`kfx_frontend_state_test_stub`**
  (`src/kfx_net/tests/`) — fake the `game`/`kfx_game_state` and
  `kfx_frontend_state` globals `net_resync.cpp`'s raw-blob resync
  (`architecture.md` §6.2) reaches into. Two separate libraries, not one,
  because each stops being needed on a different schedule as more
  libraries get linked for real (`kfx_game_utest` still needs the
  frontend one but not the game one).
- **`kfx_sim_test_stubs.cpp`** (`src/kfx_sim/tests/`, *not* a shared
  library — compiled directly into `kfx_sim_utest` only) — fakes
  `creature_table_add[]` (really populated by `kfx_render`'s
  `custom_sprites.c`). Not shared like the others: the moment a test
  target also links `kfx_render` for real, this stub would become a
  duplicate-definition error.

**Fixture-file path baking**: `kfx_config_utest` and `kfx_script_utest`
each `configure_file()` a `*_test_paths.h.in` → generated header (the
same `@ONLY`-substitution pattern the project's own `ver_defs.h.in`
uses), baking an absolute, `CMAKE_SOURCE_DIR`/`CMAKE_CURRENT_SOURCE_DIR`-
derived path into a macro at configure time — `KFX_CONFIG_TEST_FIXTURES_DIR`
(this test directory's `fixtures/` subdir, for `value_util_test.cpp`'s
`load_toml_file()` tests) and `KFX_REPO_CONFIG_DIR` (the repo's real
`config/` dir, for `lua_base_test.cpp`'s `open_lua_script()` fixture) —
so tests are runnable from any working directory, not just the build
tree.

## 5. Testability patterns

Three shapes cover essentially every test in this codebase (see
`docs/refactor/testing/stage-02-testability-and-fakes.md` for the
original design rationale):

- **Pattern A — reset a state struct.** Most world state lives in a
  per-library `extern` struct (`kfx_sim_state`, `kfx_config_state`,
  `kfx_net_state`, …, `architecture.md` §6.1). A test fixture
  `std::memset`s the relevant struct to zero (or calls the module's own
  reset function, where one exists — `light_initialise()`,
  `triangulation_init_triangles()`, `edge_points_clean()`), sets the
  handful of fields the function under test reads, and asserts.
- **Pattern B — fake a callback provider.** Where production code calls
  through one of `kfx_config`'s callback structs (`architecture.md` §5),
  a test registers a fake implementation (`set_game_callbacks(&fake)`,
  `set_config_reload_callbacks(&fake)`, …) and restores the default
  no-op table (`set_*_callbacks(nullptr)`) in the fixture's destructor.
  Necessary whenever the *default* no-op stub would make the interesting
  branch unreachable (e.g. `get_thing_model()`'s default always returns
  model 0, so a test of pay-based eligibility needs a real fake to
  resolve a nonzero model).
- **Fixture files** — a few tests need real file content, not just
  in-memory state (`value_util_test.cpp`'s `tests/fixtures/sample.toml`;
  `lua_base_test.cpp`/`lua_classes_test.cpp` against the real
  `config/fxdata/lua/` tree, since it's KeeperFX's own scripting layer
  and fully in-repo).

A recurring, minor testability gap found repeatedly while extending this
harness: a function used only within its own translation unit sometimes
has **no header declaration at all** (only a same-file forward
declaration, or none). Since a test lives in a different translation
unit, it can't call such a function without one. The fix applied every
time this came up — never a same-file `extern` in the test, always a
real header addition — was to add the missing prototype to that library's
own header (e.g. `lvl_script_conditions.h`, `dungeon_stats.h`,
`magic_powers.h`, `room_data.h`, `light_data.h`, `ariadne_navitree.h`),
matching the file's existing declaration style. This is a legitimate,
minimal production change (the function already had external linkage; it
was just never advertised), not a test-only workaround.

## 6. Coverage tooling

Layered on top of `KFX_BUILD_TESTS`, gated by `KFX_TEST_COVERAGE`:
`gcov` (bundled with GCC) plus `lcov` **1.16** — deliberately not the
latest 2.x, which needs non-core Perl modules (`DateTime`,
`Capture::Tiny`); 1.16 needs only core Perl, verified by grepping the
fetched tarball's `use` statements. Not `gcovr` either — it needs a pip
dependency chain (Jinja2, …) with no clean fetch equivalent. `lcov` is
fetched via `Dependencies.cmake`'s existing `kfx_fetch()` helper (the
same download-a-tarball-once mechanism every other prebuilt dependency in
this project uses) — **no `apt install lcov` / `pip install gcovr`
required**, consistent with this project's "the CMake build fetches its
own dependencies" convention (see root `CLAUDE.md`).

```bash
cmake -S . -B out/coverage -G Ninja -DKFX_OS=linux \
  -DKFX_BUILD_TESTS=ON -DKFX_TEST_COVERAGE=ON
cmake --build out/coverage --target \
  kfx_platform_utest kfx_config_utest kfx_pathfinding_utest kfx_sim_utest \
  kfx_render_utest kfx_net_utest kfx_game_utest kfx_frontend_utest \
  kfx_script_utest kfx_apploop_utest -j"$(nproc)"

ctest --test-dir out/coverage --output-on-failure   # must run first --
                                                     # `coverage` builds the
                                                     # binaries, ctest is
                                                     # what actually produces
                                                     # .gcda profiling data
cmake --build out/coverage --target coverage        # capture -> extract -> render
# report: out/coverage/coverage-html/index.html
```

The `coverage` custom target (root `CMakeLists.txt`) chains three `lcov`/
`genhtml` calls: `--capture` (whole build tree) → `--extract
".../src/kfx_*/src/*"` (drops every `_deps/` third-party file, `centitoml`,
and the test binaries themselves — confirmed via `grep '^SF:'
coverage.info` listing only real `src/kfx_*/src/*.c(pp)` production
files) → `genhtml` (renders `coverage-html/`). Its `DEPENDS` list
covers all ten `*_utest` targets, so `cmake --build out/coverage --target
coverage` alone (after `ctest` has produced `.gcda` data) builds
everything it needs — no need to list all ten targets by hand first.

**Rebuilding after a source change**: gcov can throw `libgcov profiling
error: overwriting an existing profile data with a different checksum`
against stale `.gcda` files left from a prior instrumented build of the
same source. Harmless, but clear them before re-running: `find
out/coverage -name "*.gcda" -delete`. CI never hits this (`coverage`
always starts from a fresh `out/coverage` tree).

**gcov's line-coverage denominator excludes comments, blank lines, and
any line that emits no executable code** (declarations, closing braces,
…) — it's not counting raw file line count. Confirmed directly: `bflib_math.c`
is 1,361 lines total but only 172 appear as `DA:` (instrumented) entries
in `coverage.info`.

**Current numbers** (regenerated 2026-08-29, 604 tests): **2.96% line
coverage** (3,698 / 124,942), **9.02% function coverage** (674 / 7,473).
No coverage floor is enforced — deliberately deferred
(`docs/refactor/testing/comprehensive/stage-05-coverage-tooling.md` §5)
until there's a more mature baseline to set one against. The report is
informational, uploaded as a CI artifact on every PR, not merge-blocking.

## 7. CI integration

`.github/workflows/build-prototype.yml`, three relevant jobs (all
`ubuntu-24.04`, all reuse the same apt package list `build-prototype-linux`
needs, so `-DKFX_BUILD_TESTS=ON` doesn't fall back to slow
`FetchContent`-from-source builds of SDL3/openal/luajit/etc. in CI —
`kfx_common_opts` needs the full dependency chain regardless of which
single library is under test):

- **`check-layering`** — `python3 scripts/check_layering.py --strict`.
  Merge-blocking. No C/C++ toolchain needed.
- **`unit-tests`** — configures `-DKFX_BUILD_TESTS=ON`, builds all ten
  `*_utest` targets, runs `ctest --output-on-failure`. Merge-blocking.
- **`coverage`** — separate build tree (`out/coverage`, not `out` —
  coverage-instrumented objects behave differently under fast-iteration
  tooling than plain ones), `-DKFX_TEST_COVERAGE=ON` on top, runs
  `ctest`, then `cmake --build out/coverage --target coverage`, uploads
  `coverage-html/**` via `actions/upload-artifact@v4`. Informational
  only, does not gate merging.

`check_layering_symbols.py --strict` (the `nm`-based post-build audit
catching raw-`extern` forward-declarations `check_layering.py`'s
`#include`-only check misses — `architecture.md` §8.2, §8.1) is not
currently a separate CI job; it's run manually alongside every test
addition as a verification step (see `docs/refactor/testing/`'s stage
docs for the convention).

## 8. Current test inventory

As of 2026-08-29, **604 tests** across all ten libraries (`ctest -N`
count; each `*_utest` binary's own Catch2 summary matches):

| Library | Tests | Notable coverage |
|---|--:|---|
| `kfx_sim` | 160 | Five natural clusters (map/thing/creature/room/player, `docs/refactor/testing/comprehensive/stage-08b-kfx-sim-clusters.md`) plus `dungeon_stats.c`/`magic_powers.c`/`power_specials.c` outside those clusters; `roomspace.c`'s `box_placement_mode`/`drag_placement_mode` (the latter driven by the shared `get_packet` stub's `control_flags` field, not just its call path); `creature_graphics.c`'s `keepersprite_*` dispatch functions (the `creature_table_add` accepted residual); four of 17 `creature_states_*.c` files now covered (`_tresr`, `_mood`'s anger/mood bookkeeping, `_gardn`'s hunger accessors, `_rsrch`'s pure `struct Dungeon`-only research-queue functions); `map_locations.c`'s `TbMapLocation` bit-packing accessors, `room_jobs.c`'s `worker_needed_in_dungeons_room_role` (`RoRoF_Research` branch), `map_ceiling.c`'s `ceiling_set_info`, `power_process.c`'s `players_disease_can_infect_target_players_creatures` |
| `kfx_config` | 104 | Old-style config tokenizer, campaign level membership/navigation (incl. `kfx_config`'s own first pattern-B test), the TOML fixture-file loader; `config_campaigns.c`'s struct-management functions (level/campaign array append, grow, clear, swap, sort, lookup); every `*Callbacks`-registration file's default no-op table exhaustively exercised (`net_callbacks.c`, `game_callbacks.c`, `sprite_lookup.c`, `dungeon_availability.c`, `render_overlay.c`, `script_hooks.c`, `pathfinding_world.c` — the interface's own registration/defaults, not the "big three" ariadne consumers of it); `config_textures.c`/`config_players.c` (a worked per-loader-schema TOML fixture example, plus pure accessors); `config_strings.c`'s pure buffer/lookup functions |
| `kfx_platform` | 175 | `bflib_math.c`, `bflib_basics.c` (incl. its pure string/number utils — `get_rid`/`llong`/`lword`/`saturate_set_signed`/`str_append`/`str_appendf`/`make_lowercase`/`make_uppercase`/`natoi`), `bflib_video.c` (all three known `VideoScaleCallbacks`/callback consumers tested), `bflib_string.c` (full), `bflib_planar.c` (table-free functions), `kfx_memory.c` (full — the release-build allocator wrappers plus the scratch/arena allocator), `bflib_guibtns.c`, `bflib_keybrd.c`'s `keyboardControl` state machine, `bflib_datetm.cpp`'s pure `LbDateTimeDecode`/`get_trigger_time_measurement_fps`, `bflib_coroutine.c` (full, 100%), `bflib_fileio.c`'s real-file open/read/write/seek/eof/length/delete lifecycle, `bflib_dernc.c`'s `rnc_crc`/`UnpackM1` early-return/file-checksum round trip, `custom_zip.c`'s validation early-returns, `bflib_filelst.c`'s `LbDataFree`/`LbDataLoad` wildcard branch, `spritesheet.cpp` (full, incl. a real index+data file round trip through `load_spritesheet`), `bflib_text.c`'s UTF-8/codepage conversion (full non-CJK path), `bflib_render.c`'s `polyscans[]` lifecycle |
| `kfx_pathfinding` | 58 | Every small self-contained `ariadne_*.c` file's accessors/allocators/tag-bookkeeping/region-connectivity, now including `ariadne_findcache.c`'s cache-hit lookup, `ariadne_tringls.c`'s `get_triangle_point`/`triangle_tip_equals`/`edgelen_set`/`triangle_find_first_used`, `ariadne_navitree.c`'s `navitree_add`/`delaunay_init`, `ariadne_naviheap.c`'s real min-heap priority ordering (not just capacity/empty-state), `ariadne_points.c`'s `point_dispose`, and `ariadne_regions.c`'s `region_set`/`region_unset`/`region_unlock`; the "big three" (`ariadne.c`/`_update.c`/`_wallhug.c`) still need a `PathfindingWorldCallbacks` fake |
| `kfx_render` | 40 | `light_data.c` accessors + allocators, `engine_camera.c` zoom math, `engine_redraw.c`'s `update_mouse_light()` (the `kfx_render -> kfx_net` side of the `get_packet_direct` accepted residual); the `LensEffect` C++ class hierarchy — base-class accessors (`GetType`/`GetName`/`SetEnabled`/`IsEnabled`) plus `Mist`/`Overlay`/`Displacement`/`Flyeye`/`LuaLensEffect`'s constructor/`Draw`/`Cleanup` lifecycle on a never-`Setup()` instance, direct instantiation with no fixture as `stage-08-comprehensive-library-passes.md` had flagged |
| `kfx_net` | 33 | `net_checksums.c`, `packets.c` field accessors, `net_resync.cpp`'s `boing` round-trip and `animate_resync_progress_bar`, `packets_misc.c`'s full player-packet accessor set (`get_packet`/`get_packet_direct`/`set_players_packet_action`/`get_players_packet_action`/`set_players_packet_control`/`unset_players_packet_control`/`set_players_packet_position`), `packets_input.c`'s `is_mouse_on_map`/`remember_cursor_subtile` |
| `kfx_game` | 14 | `game_legacy.c`, `lvl_script_conditions.c`, `game_merge.c`'s moon-phase visibility |
| `kfx_frontend` | 9 | `gui_topmsg.c`, `gui_vscroll.c` (built the `save_game_catalogue[]` fixture) — still the thinnest library by design (stage-04g found most files GUI-widget/save-catalogue coupled) |
| `kfx_script` | 8 | Bare-`lua_State` helpers, the real `open_lua_script()` init chain, `classes/Pos3d.lua` |
| `kfx_apploop` | 4 | `display_should_be_updated_this_turn` (pattern A + a fake `LbTimerClock` + pattern B combined) |

Two libraries' rows include coverage deliberately targeted at
`check_layering.py --strict`'s two currently-*unaccepted* violations
(`ariadne_regions.c`, `power_specials.c`) — both root-caused to a
wrong-header choice with a trivial, zero-behavior-change fix, written up
in
[`docs/refactor/todo/two-remaining-layering-violations.md`](../refactor/todo/two-remaining-layering-violations.md).

Per-library detail, what's tested and what's deliberately deferred (and
why), is tracked in
[`docs/refactor/testing/comprehensive/stage-08-comprehensive-library-passes.md`](../refactor/testing/comprehensive/stage-08-comprehensive-library-passes.md)
and
[`stage-08b-kfx-sim-clusters.md`](../refactor/testing/comprehensive/stage-08b-kfx-sim-clusters.md)
— kept current as tests are added, unlike the numbered `stage-0N-*.md`
narrative documents, which are historical snapshots of the day they were
written.

## 9. Layering-residual test coverage

Per an explicit request to focus coverage around
`check_layering.py`/`check_layering_symbols.py`'s violations — both to
de-risk an eventual fix where one exists, and to protect the by-design
residuals' surrounding code where it doesn't. Checked precisely (grepping
each test file for the actual symbol/function name, not assumed from
which production file a test happens to also cover) rather than claimed
in the abstract — an earlier draft of this section overclaimed full
coverage before that check.

**`#include`-level (`check_layering.py`)**:

| Violation | Status | Test coverage |
|---|---|---|
| `ariadne_regions.c` → `player_data.h` | **fixed** (2026-08-29, `07784c1f7`) — swapped for `kfx_config_state.h`'s own `PLAYERS_COUNT` copy | `ariadne_regions_test.cpp` (10 tests) |
| `power_specials.c` → `api.h` | **fixed** (2026-08-29, `07784c1f7`) — redundant include deleted, `script_hooks.h` was already included directly | `power_specials_test.cpp` (6 tests) |
| `console_cmd.c` → `game_session_loop.h` | was accepted, found **fully dead**, then **removed** (2026-08-29, `07784c1f7`) — its `ACCEPTED_VIOLATIONS` entry is gone too | none needed — no live code exercised it |
| `net_resync.cpp` → `kfx_frontend_state.h`/`kfx_game_state.h`/`game_legacy.h` | accepted, by design (raw-blob resync wire format) | `net_resync_test.cpp` (4 tests) — covers other functions *in the same file*, not the blob-resync functions themselves |
| `bflib_enet.cpp` → `net_main.h` | accepted, by design (`struct NetSP` ABI sharing) | none — real ENet networking, no meaningful unit-test surface found |

`check_layering.py --strict` now reports **zero un-accepted violations**
and 4 accepted permanent residuals (down from 5). The two fixes above
were root-caused and test-covered in one session (recorded below and in
[`docs/refactor/todo/two-remaining-layering-violations.md`](../refactor/todo/two-remaining-layering-violations.md))
but deliberately left unapplied pending review; a separate concurrent
session (`07784c1f7`, same day) picked them up, applied all three edits,
and reverified against a clean two-variant build plus both layering
checkers.

**Symbol-level (`check_layering_symbols.py`)** — 9 distinct symbol names
across 15 accepted call sites:

| Symbol | Call sites | Test coverage |
|---|---|---|
| `get_packet_direct` | `engine_redraw.c`, `roomspace.c`, `roomspace_prediction.c` | `engine_redraw_test.cpp`, `roomspace_test.cpp`, plus the accessor itself directly in `packets_misc_test.cpp` — `roomspace_prediction.c`'s own call site is still untested (its `get_packet`/`set_packet_action` calls live inside one large orchestration function with `static`, module-private state — declined, see Known gaps) |
| `creature_table_add` | `creature_graphics.c` | `creature_graphics_test.cpp` |
| `kfxmain` | `PlatformLinux.cpp` | exercised by every single test run (`kfx_test_main.cpp` forwards it into Catch2), but never asserted on directly |
| `get_packet` | `roomspace.c`, `roomspace_prediction.c` | the accessor itself now covered directly (`packets_misc_test.cpp`); neither production call site is |
| `set_packet_action` | `roomspace.c`, `roomspace_prediction.c` | the accessor itself covered (`packets_test.cpp`, pattern A on a bare `struct Packet`); neither production call site is |
| `set_players_packet_action` | `creature_instances.c`, `roomspace.c`, `thing_creature.c` | the accessor itself now covered directly (`packets_misc_test.cpp`, along with its siblings `get_players_packet_action`/`set_players_packet_control`/`unset_players_packet_control`); none of the three production call sites are |
| `game` / `kfx_game_state` / `kfx_frontend_state` | `net_resync.cpp` (the raw-blob resync itself) | none — see the `#include`-level row above |

## 10. Known gaps (as of this writing)

- 13 of 17 `creature_states_*.c` files (`kfx_sim`) still have no test.
  Four now do: `_tresr` (the original, smallest), `_mood` (the anger/mood
  bookkeeping cluster — `creature_can_get_angry`, `anger_calculate_
  creature_is_angry`, `anger_free_for_anger_increase/decrease`, `anger_
  is_creature_angry/livid`, `anger_get_creature_anger_type`), `_gardn`
  (`creature_able_to_eat`, `hunger_is_creature_hungry`), and `_rsrch`
  (`get_next_research_item`, `has_new_rooms_to_research` — both take a
  bare `struct Dungeon *`, no `Thing`/`CreatureControl` needed at all).
  In every case, only the pure/near-pure accessor functions were
  covered — the actual `at_*_room()`/`*ing()` state-machine entry points
  in all 17 files (including these four) still need a fuller
  `Thing`+`CreatureControl`+room/job context, not attempted anywhere yet.
- `kfx_pathfinding`'s "big three" (`ariadne.c`, `ariadne_update.c`,
  `ariadne_wallhug.c`, 3,313/1,773/2,178 lines) — need a
  `PathfindingWorldCallbacks` fake (51-entry interface,
  `architecture.md` §2.2a), a dedicated sub-effort of its own.
- `kfx_net`'s `packets_input.c`/`packets_cheats.c` — mostly process_*-shaped
  dispatch, not the field-packing shape `packets.c` turned out to be.
  Two of its non-dispatch helpers are now covered directly
  (`is_mouse_on_map`, `remember_cursor_subtile` — `packets_misc_test.cpp`,
  both needed a "add the missing header declaration" fix, added to
  `packets.h`); the `process_dungeon_control_packet_*`/`packets_process_cheats`
  dispatch functions themselves remain untested.
- `roomspace_prediction.c`'s `get_packet`/`set_packet_action` call sites
  (inside `update_local_dig_tag_prediction`) were investigated and
  declined: unlike every other "missing declaration" fix in this plan
  (always targeting already-externally-linked symbols), the function's
  own state (`local_dig_tag_prediction`, `local_dig_roomspace_prediction`,
  `local_dig_render_roomspace_active`) is `static` — module-private —
  so testing it would mean exposing genuinely private internals, a
  different and more invasive kind of change than the rest of this plan.
- The two `check_layering.py --strict` violations are now **fixed**
  (2026-08-29, `07784c1f7`, a separate concurrent session applying the
  fix this plan had root-caused and verified but deliberately left
  unapplied) — see
  [`two-remaining-layering-violations.md`](../refactor/todo/two-remaining-layering-violations.md)
  for the full before/after record.
- **`kfx_platform`** (the lowest-ranked library) was pushed hard in a
  dedicated pass (2026-08-29): 41 → 175 tests, 2.3% → 6.1% line coverage,
  5.2% → 12.7% function coverage (per-library, `kfx_platform/src/index.html`).
  Every file with real pure/near-pure logic reachable without a new fake-
  provider mechanism now has direct coverage (`kfx_memory.c`,
  `bflib_coroutine.c`, and `spritesheet.cpp` all at 100%/85%+). What's
  left in this library is uniformly gated behind a *live* external
  dependency, not just "needs a fixture": `bflib_render_trig.c`/
  `bflib_vidraw*.c`/`bflib_render_gpoly.c`/`bflib_sprfnt.c` (real
  rendering-surface + clip-window state machine — `setup_vecs()` does
  accept a plain malloc'd pixel buffer with no SDL involved, so this
  *could* be pattern-A tested, but understanding the clip-window globals
  well enough to do it right is a dedicated sub-effort of its own, same
  category as the pathfinding "big three" above); `bflib_sndlib.cpp`/
  `bflib_sound.c`/`sound_manager.cpp` (real OpenAL device);
  `bflib_enet.cpp`/`bflib_netsp.cpp` (real sockets); `bflib_inputctrl.cpp`/
  `bflib_mouse.cpp`/`bflib_input_joyst.cpp` (real SDL event pump);
  `bflib_fmvids.cpp` (real Smacker video decode); `bflib_crash.c` (signal
  handlers — actively unsafe to exercise in a unit test); `bflib_cpu.c`
  (real CPUID); `moonphase.c` (wraps the real `astronomy` library through
  a `static` variable with no setter — would need a new accessor, not
  just a test). This is the point flagged as "no further functions/routes
  reachable without significant harness changes" for this library.
- Two real production quirks were found while writing these tests
  (documented here, not fixed, same restraint as the layering-violation
  finds above):
  - `bflib_fileio.c`'s `LbFileOpen(..., Lb_FILE_MODE_NEW)` is broken for
    any path starting with `/`: `create_directory_for_file()` finds the
    *leading* slash as its first `strchr` match, computes an empty
    directory-to-create string, and `mkdir("")` fails with `ENOENT` —
    aborting the whole open before `fopen()` is ever reached (confirmed
    directly with a standalone repro, not just read). Dormant in
    practice because every real `Lb_FILE_MODE_NEW` caller in this
    codebase uses relative paths.
  - `bflib_filelst.c`'s `LbDataLoadSetModifyFilenameFunction()` reads
    like a "set and return the previous hook" setter but doesn't track a
    previous value at all — it just echoes `newfunc` straight back
    (confirmed by a test that assumed the old-value contract and failed).
- **`kfx_config`** (next up the ladder after `kfx_platform`, same "focus
  on the lowest-ranked library" request) went 45 → 104 tests, 3.3% →
  8.7% line coverage, 5.0% → 32.0% function coverage (per-library,
  `kfx_config/src/index.html`). The single biggest lever turned out to
  be this library's seven still-untested `*Callbacks`-registration files
  (`net_callbacks.c` 52 stub functions, `game_callbacks.c` 46,
  `pathfinding_world.c` 54, `script_hooks.c` 25, `render_overlay.c` 28,
  `dungeon_availability.c` 10, `sprite_lookup.c` 7 — ~220 tiny functions
  total): every one is `static`, only reachable through its default
  table's function-pointer fields, and every noop body ignores its
  pointer arguments, so one exhaustive "call every field, assert the
  documented no-op default" test per file drove several of them straight
  to 100%/100% line+function coverage at once. `config_campaigns.c`
  (0/804 lines beforehand, despite this table's earlier text implying
  campaign coverage existed — that coverage was actually `is_bonus_level`
  and friends in `config.c`, a different file) got its struct-management
  functions covered (level/campaign array append/grow/clear/swap/sort/
  lookup); `config_textures.c` got a worked example of the "per-loader
  TOML-fixture" pattern the general gap below describes.
  What remains in `kfx_config` is overwhelmingly the large per-`config_*.c`
  TOML loaders (`config_crtrmodel.c` 1,635 lines, `config_creature.c`
  1,320, `config_magic.c` 563, `config_sounds.c` 477, `config_trapdoor.c`
  271, `config_rules.c` 266, `config_settings.c` 260, `config_terrain.c`
  227, `config_effects.c` 167, `config_compp.c` 130, `config_slabsets.c`
  96, `highscores.c` 94, `config_objects.c` 86, `config_crtrstates.c` 66,
  `config_spritecolors.c` 64, `config_lenses.c` 55, `config_cubes.c` 40)
  — each needs its own TOML fixture file and real schema understanding,
  a dedicated per-loader sub-effort each rather than a shared mechanism,
  same category as the pathfinding "big three"/kfx_platform's live-
  external-dependency files above. Not a hard "significant harness
  change" blocker like those, though — `value_util.c`'s `load_toml_file()`
  and the fixture-file convention (`kfx_config_test_paths.h.in`) already
  make each one tractable; it's a volume problem, not a capability gap.
- **`kfx_pathfinding`** (third rung up the ladder, same approach again)
  went 41 → 58 tests, 6.9% → 9.7% line coverage, 21.3% → 28.7% function
  coverage (per-library, `kfx_pathfinding/src/index.html`). Unlike
  `kfx_platform`/`kfx_config`, this library was already reasonably well
  covered on the small self-contained `ariadne_*.c` files going in — this
  pass mostly closed remaining gaps *within* those same files rather than
  landing a new one wholesale: `ariadne_findcache.c`'s `triangulation_init_cache`/
  `triangle_brute_find8_near` (the latter had no header declaration,
  added); `ariadne_tringls.c`'s `get_triangle_point`/`triangle_tip_equals`/
  `edgelen_set`/`triangle_find_first_used` (the last one reusing
  `triangulation_init_triangles` as a deterministic fixture, the same
  dual production-API/test-setup role `ariadne_edge_test.cpp`'s
  `edge_points_clean()` plays); `ariadne_navitree.c`'s `navitree_add`/
  `delaunay_init` (`ix_delaunay` had no header declaration, added — and
  a prior test-file comment claiming `delaunay_init` needed a real
  triangulated fixture like its Delaunay-walk siblings turned out to be
  wrong on inspection, corrected); `ariadne_naviheap.c`'s actual min-heap
  priority ordering (`naviheap_top`/`naviheap_remove` popping in
  ascending `tree_val` order — previously only capacity/empty-state was
  tested, deliberately deferred pending `ariadne_navitree.c`'s own
  fixture, which now exists); `ariadne_points.c`'s `point_dispose`;
  `ariadne_regions.c`'s `region_set`/`region_unset`/`region_unlock`
  (their own `Regions[]` bookkeeping stays unobservable — no accessor —
  but `region_set`'s write to the *triangle's own* region id, via the
  already-covered `set_triangle_region_id`, is). `triangle_find8`/
  `point_find` (the actual point-location triangulation walk, via
  `triangle_divide_areas_s8differ`'s barycentric-sign math) were assessed
  and declined — reproducing `ariadne_regions_test.cpp`'s "hand-verified
  minimal fixture" discipline for point-in-triangle geometry instead of
  region-BFS connectivity is a bigger, separate undertaking. The "big
  three" (`ariadne.c` 1,773 lines/`ariadne_update.c` 936/`ariadne_wallhug.c`
  1,375) remain the one real "significant harness change" item in this
  library: a `PathfindingWorldCallbacks` fake (51 entries,
  `architecture.md` §2.2a) driving real pathfinding logic, not just the
  interface's own default-table registration (`kfx_config`'s
  `pathfinding_world_test.cpp` already covers that side).
- **`kfx_sim`** (fourth rung up the ladder) went 138 → 160 tests, 1.31%
  → 1.45% line coverage, 3.18% → 3.50% function coverage (per-library,
  `kfx_sim/src/index.html`). The smallest percentage move of any library
  pushed so far, and expectedly so: at 49,302 lines across ~90 files,
  `kfx_sim` is by far the largest library (more than 3× `kfx_platform`,
  5× `kfx_config`, 10× `kfx_pathfinding` combined) and the bulk of it is
  genuine `Thing`+`CreatureControl`+`Room`+`Dungeon` simulation logic —
  the same "needs a fuller world-state fixture" territory the 13
  untested `creature_states_*.c` files and the AI `player_comp*.c`
  files (`player_computer.c`/`player_comptask.c`/`player_compchecks.c`/
  `player_compprocs.c`/`player_compevents.c`, ~4,800 lines combined, all
  still at 0%) already represent. This pass found and landed the pure/
  near-pure functions reachable without that fixture work:
  `map_locations.c`'s `TbMapLocation` bit-packing accessors (pure
  bitfield math, `get_coord_encoded_location`/`get_map_location_type`/
  `_longval`/`_plyrval`, plus `get_map_location_code_name`'s
  `MLoc_HEROGATE`/`MLoc_PLAYERSHEART` branches — the latter through
  `kfx_config`'s already-populated `player_desc[]` table, no fixture
  needed); `room_jobs.c`'s `worker_needed_in_dungeons_room_role`
  (`RoRoF_Research` branch only — the sibling branches reach into
  `player_computer.c`'s `get_dungeon_money_less_cost`, which itself calls
  a `kfx_config` power-cost lookup, not attempted); `map_ceiling.c`'s
  `ceiling_set_info` (found, by testing rather than assuming, that a
  very negative `height_min` reaches the "distance too large" rejection
  branch and still leaves `kfx_sim_state.ceiling_dist` written before
  the early return — an earlier draft of this test wrongly assumed that
  branch was unreachable through the public validation path); `power_
  process.c`'s `players_disease_can_infect_target_players_creatures`
  (found, again by testing, that its `allies_share_disease` branch is
  only reachable when the one-directional alliance flag it re-checks is
  already known to be set, so that branch always resolves to "can't
  infect" regardless of the flag's value — not a bug fixed here, just
  the function's actual behavior, pinned down). Given the library's
  scale, closing a meaningful fraction of it — starting with the
  `creature_states_*.c`/`player_comp*.c` fixture work — is realistically
  a multi-session effort of its own, not a single follow-up pass like
  the other three libraries got.
- **`kfx_render`** (fifth rung up the ladder) went 25 → 40 tests, 2.1% →
  3.1% line coverage, 5.4% → 11.1% function coverage (per-library,
  `kfx_render/src/index.html`) — function coverage more than doubled,
  the biggest relative jump of any library pushed so far, by landing the
  `LensEffect` C++ class hierarchy stage-08's kfx_render row had already
  flagged as "a natural fit... no fixture needed": `MistEffect`/
  `OverlayEffect`/`DisplacementEffect`/`FlyeyeEffect`'s constructors,
  `Draw()`, and `Cleanup()` are all safe to exercise on a never-`Setup()`
  instance (confirmed by reading every one of their bodies first — each
  guards its teardown logic behind `m_current_lens >= 0` or an
  internally nullptr-checked helper, and each `Draw()` takes the same
  early-return path before ever touching its `LensRenderContext*`
  argument, so `nullptr` is safe to pass); `LensEffect` base-class
  accessors (`GetType`/`GetName`/`SetEnabled`/`IsEnabled`) and
  `LoadAssetWithFallback`'s validation early-returns; `LuaLensEffect`'s
  full lifecycle with a `NULL` `lua_State` (skips `RegisterBufferFunctions`
  at construction and every other Lua-touching branch) plus its pure
  `SetConfig`/`SetParameter`/`GetParameter` config surface.
  `LuaLensEffect`'s actual Lua-exposed pixel accessors (`LuaGetPixel`/
  `LuaSetPixel`/`LuaCopyPixel`) are `private`, only reachable by
  registering them into a real `lua_State` and running a script that
  passes a userdata block shaped like the `.cpp` file's own unexported
  `LuaBufferInfo` struct — replicating that layout in a test would be a
  fragile shortcut, not a real fixture, so not attempted. `PaletteEffect`
  (the sixth effect subclass)'s `Setup()`/`Cleanup()` reach into real
  player/palette state (`get_my_player()`, `lenses_conf`,
  `PaletteSetPlayerPalette`) and weren't attempted either. The rest of
  this library — `engine_render.c` (5,618 lines, the 3D rendering
  engine), `custom_sprites.c`, `engine_arrays.c`, `vidmode.c`,
  `cursor_tag.c` (real map/subtile state), and the remaining
  `LensManager.cpp`/`lens_api.c` singleton-backed render-target
  plumbing — all need a live rendering surface or real map/player world
  state, `kfx_platform`'s `bflib_vidraw.c`-style "significant harness
  change" territory rather than a quick follow-up.
- No coverage floor / CI gate — deliberately deferred, §6 above.
- No branch-coverage data (`lcov`'s `--summary` reports "no data found"
  for branches; line/function coverage only).

## 11. Document map

| Question | Where to look |
|---|---|
| What's true about the harness right now | this document |
| How the harness was designed, stage by stage (historical) | [`docs/refactor/testing/`](../refactor/testing/) |
| Per-library test status, what's tested vs. deferred | [`docs/refactor/testing/comprehensive/stage-08-comprehensive-library-passes.md`](../refactor/testing/comprehensive/stage-08-comprehensive-library-passes.md), [`stage-08b-kfx-sim-clusters.md`](../refactor/testing/comprehensive/stage-08b-kfx-sim-clusters.md) |
| Library dependency ladder, callback-struct pattern, layering enforcement | [`architecture.md`](architecture.md) |
| How to write a functional (`src/ftests/`) test | [`src/ftests/README.md`](../../src/ftests/README.md) |
