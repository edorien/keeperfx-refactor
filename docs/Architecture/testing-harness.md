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

**Current numbers** (regenerated 2026-08-29, 302 tests): **1.7% line
coverage** (2,164 / 124,517), **3.8% function coverage** (281 / 7,447).
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

As of 2026-08-29, **302 tests** across all ten libraries (`ctest -N`
count; each `*_utest` binary's own Catch2 summary matches):

| Library | Tests | Notable coverage |
|---|--:|---|
| `kfx_sim` | 102 | Five natural clusters (map/thing/creature/room/player, `docs/refactor/testing/comprehensive/stage-08b-kfx-sim-clusters.md`) plus `dungeon_stats.c`/`magic_powers.c`/`power_specials.c` outside those clusters; `roomspace.c`'s `box_placement_mode`/`drag_placement_mode` (the latter driven by the shared `get_packet` stub's `control_flags` field, not just its call path); `creature_graphics.c`'s `keepersprite_*` dispatch functions (the `creature_table_add` accepted residual) |
| `kfx_config` | 45 | Old-style config tokenizer, campaign level membership/navigation (incl. `kfx_config`'s own first pattern-B test), the TOML fixture-file loader |
| `kfx_platform` | 41 | `bflib_math.c`, `bflib_basics.c`, `bflib_video.c` (all three known `VideoScaleCallbacks`/callback consumers tested), `bflib_string.c` (full), `bflib_planar.c` (table-free functions) |
| `kfx_pathfinding` | 41 | Every small self-contained `ariadne_*.c` file's accessors/allocators/tag-bookkeeping/region-connectivity; the "big three" (`ariadne.c`/`_update.c`/`_wallhug.c`) still need a `PathfindingWorldCallbacks` fake |
| `kfx_render` | 25 | `light_data.c` accessors + allocators, `engine_camera.c` zoom math, `engine_redraw.c`'s `update_mouse_light()` (the `kfx_render -> kfx_net` side of the `get_packet_direct` accepted residual) |
| `kfx_game` | 13 | `game_legacy.c`, `lvl_script_conditions.c`, `game_merge.c`'s moon-phase visibility |
| `kfx_net` | 14 | `net_checksums.c`, `packets.c` field accessors, `net_resync.cpp`'s `boing` round-trip and `animate_resync_progress_bar` |
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
| `ariadne_regions.c` → `player_data.h` | unaccepted, root-caused, trivial fix identified | `ariadne_regions_test.cpp` (10 tests) |
| `power_specials.c` → `api.h` | unaccepted, root-caused, trivial fix identified | `power_specials_test.cpp` (6 tests) |
| `console_cmd.c` → `game_session_loop.h` | accepted, but found **fully dead** (verified by removing it and rebuilding) | none needed — no live code exercises it |
| `net_resync.cpp` → `kfx_frontend_state.h`/`kfx_game_state.h`/`game_legacy.h` | accepted, by design (raw-blob resync wire format) | `net_resync_test.cpp` (4 tests) — covers other functions *in the same file*, not the blob-resync functions themselves |
| `bflib_enet.cpp` → `net_main.h` | accepted, by design (`struct NetSP` ABI sharing) | none — real ENet networking, no meaningful unit-test surface found |

All three findings (root causes, the dead-code discovery) are written up
in
[`docs/refactor/todo/two-remaining-layering-violations.md`](../refactor/todo/two-remaining-layering-violations.md).

**Symbol-level (`check_layering_symbols.py`)** — 9 distinct symbol names
across 15 accepted call sites:

| Symbol | Call sites | Test coverage |
|---|---|---|
| `get_packet_direct` | `engine_redraw.c`, `roomspace.c`, `roomspace_prediction.c` | `engine_redraw_test.cpp`, `roomspace_test.cpp` — `roomspace_prediction.c`'s own call site untested |
| `creature_table_add` | `creature_graphics.c` | `creature_graphics_test.cpp` |
| `kfxmain` | `PlatformLinux.cpp` | exercised by every single test run (`kfx_test_main.cpp` forwards it into Catch2), but never asserted on directly |
| `get_packet` | `roomspace.c`, `roomspace_prediction.c` | none — a different function from `get_packet_direct` above, same accepted-residual family |
| `set_packet_action` | `roomspace.c`, `roomspace_prediction.c` | none |
| `set_players_packet_action` | `creature_instances.c`, `roomspace.c`, `thing_creature.c` | none |
| `game` / `kfx_game_state` / `kfx_frontend_state` | `net_resync.cpp` (the raw-blob resync itself) | none — see the `#include`-level row above |

## 10. Known gaps (as of this writing)

- 16 of 17 `creature_states_*.c` files (`kfx_sim`) — only the smallest,
  `creature_states_tresr.c`, has a test; the rest need a fuller
  `Thing`+`CreatureControl`+room/job context.
- `kfx_pathfinding`'s "big three" (`ariadne.c`, `ariadne_update.c`,
  `ariadne_wallhug.c`, 3,313/1,773/2,178 lines) — need a
  `PathfindingWorldCallbacks` fake (51-entry interface,
  `architecture.md` §2.2a), a dedicated sub-effort of its own.
- `kfx_net`'s `packets_input.c`/`packets_cheats.c` — process_*-shaped
  dispatch, not the field-packing shape `packets.c` turned out to be.
- The two `check_layering.py --strict` violations themselves are still
  unfixed (only root-caused and test-covered) — see
  [`two-remaining-layering-violations.md`](../refactor/todo/two-remaining-layering-violations.md)
  for the exact one-line `#include` swap each needs.
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
