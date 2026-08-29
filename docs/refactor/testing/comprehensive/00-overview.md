# KeeperFX unit-test harness: the comprehensive pass — Overview

Status: **planning, just started**. This directory is the design doc for
the work that follows `docs/refactor/testing/`'s stages 1–4: that plan
proved the harness mechanics and put a first, deliberately thin pilot
suite behind all eight non-deferred `kfx_*` libraries (28 tests total, see
[`../00-overview.md` §6](../00-overview.md#6-stage-index) for the current
tally). This directory is where that gets taken from "proves the pattern
works" to something a contributor can actually rely on — three threads,
tracked as separate stage documents because they're separate problems
with separate open questions, not one undifferentiated "write more tests"
backlog:

1. **Depth on the eight already-piloted libraries**, plus `kfx_sim`'s
   still-open per-cluster breakdown (flagged but not started in
   [`../stage-04c-kfx-sim.md`](../stage-04c-kfx-sim.md)).
2. **`kfx_script`** — deferred by every prior stage-02 revision because it
   needs a LuaJIT test fixture, not a linked-library repeat of the
   existing pattern.
3. **`kfx_apploop`** — deferred for the same reason, needing "a fuller
   in-process fixture" than patterns A/B alone provide
   ([`../stage-02-testability-and-fakes.md` §4](../stage-02-testability-and-fakes.md#4-pattern-c--things-that-stay-out-of-scope-for-now)).

Alongside those, this directory also plans **code coverage tooling** —
the one item [`../00-overview.md` §7.1](../00-overview.md#7-open-questions-need-a-maintainer-call-before-stage-3)
flagged as still open back at stage 1 and never revisited, because there
was nothing worth measuring coverage *of* until libraries actually had
more than a handful of pilot tests each.

## 1. Why now, not earlier

Stage 1–4 deliberately kept each library's first pass thin — "prove the
scaffold, not the start of real coverage" (stage-01 §6), one or two
self-contained functions per library chosen specifically to avoid the
hard testability problems (state-heavy functions, callback fakes, Lua,
the per-frame dispatcher). That was the right call for proving the
mechanics cheaply, but it means the actual production risk this harness
is meant to catch — a regression in `kfx_sim`'s 83 files of creature/room/
thing logic, in the Lua-facing API surface `kfx_script` exposes to every
mod, in the per-turn dispatcher every single game tick runs through — is
almost entirely untested today. 28 tests across ~430 combined source
files is a scaffold, not a safety net.

## 2. Non-goals (same spirit as the original plan, restated for this phase)

- **Not a coverage-percentage mandate.** This plan does not set a target
  like "80% line coverage by library X" — see stage-05 for why a floor is
  deferred even after the tooling lands.
- **Not a rewrite for testability**, still. Where `kfx_script`/
  `kfx_apploop` are hard to test as currently structured, the default
  answer is a better fixture, not restructuring production code — unless
  a specific difficulty turns out to be a real design smell worth its own
  `docs/refactor/todo/` writeup (as
  [`check-layering-symbol-level-blind-spot.md`](../../todo/check-layering-symbol-level-blind-spot.md)
  was).
- **Not attempting `src/ftests/` replacement anywhere.** Functional/
  gameplay tests needing a running level stay exactly where they are,
  including for `kfx_apploop`'s per-turn orchestration — see stage-07 for
  where this plan draws that line explicitly, since `update()` is the
  closest this codebase gets to a case where the boundary is genuinely
  ambiguous.

## 3. Stage index

| # | Document | Goal | Status |
|---|----------|------|--------|
| 5 | [stage-05-coverage-tooling.md](stage-05-coverage-tooling.md) | Pick a coverage tool, wire it into CMake/CTest behind a new opt-in flag, produce a report locally | done — `KFX_TEST_COVERAGE` + fetched `lcov` 1.16, verified end-to-end; baseline 0.2% line / 0.5% function coverage recorded; CI wiring and a coverage floor both still open (§5) |
| 6 | [stage-06-kfx-script-luajit.md](stage-06-kfx-script-luajit.md) | Design a LuaJIT test fixture for `kfx_script`; land a first real test | Tier 1 done (4 tests); Tier 2 done (init chain runs end-to-end, CWD-independent); a real scripted-behavior test done too, via `classes/Pos3d.lua` — the "world-state-free binding" target turned out to live in Lua, not the C API surface; a *C*-binding test specifically still open |
| 7 | [stage-07-kfx-apploop-game-process.md](stage-07-kfx-apploop-game-process.md) | Design the in-process fixture question for `kfx_apploop`; draw the line against `src/ftests/` explicitly | pilot done (4 tests, first library to combine pattern A + a fake `LbTimerClock` + pattern B together); `update()` itself deliberately left untested per §2's recommendation |
| 8 | [stage-08-comprehensive-library-passes.md](stage-08-comprehensive-library-passes.md), [stage-08b-kfx-sim-clusters.md](stage-08b-kfx-sim-clusters.md) | Per-library follow-up plan for the eight already-piloted libraries, plus `kfx_sim`'s per-cluster breakdown | `kfx_platform` pattern-B done; `kfx_config`'s fixture-file pattern established (`load_toml_file`, also fixed a real missing-`extern "C"` bug in `value_util.h`); `kfx_pathfinding`'s small self-contained files (`ariadne_points`/`_edge`/`_naviheap`/`_tringls`, 20 tests) now done; `kfx_net`'s `packets.c` field accessors done (6 tests); `kfx_render`'s `engine_camera.c` zoom math done for the isometric/parchment view_mode branches (9 tests); `kfx_platform`'s `LbMathOperation` and `kfx_game`'s `condition_inactive` done together (4+4 tests — `LbMathOperation` is also the entire implementation behind `kfx_game`'s `get_condition_status()`, tested once where it lives rather than duplicated); `kfx_sim`'s five clusters have a foundational-accessor pass plus depth into creature transitions, the first `get_packet`-stub-exercising room test, `player_utils.c`'s status functions, `thing_stats.c`'s pure computation functions, and (outside the five clusters) `dungeon_stats.c`'s scoring family (77 `kfx_sim` tests); `kfx_platform`'s `bflib_string.c` fully covered (11 tests, all pure UTF-8-aware string ops) and `bflib_planar.c`'s table-free functions covered (9 tests); `kfx_config`'s old-style line-oriented config tokenizer (11 tests), campaign level-membership predicates (9 tests), and campaign level-navigation functions (16 tests, including kfx_config's own first pattern-B test) covered; `kfx_frontend`'s `gui_vscroll.c` covered (6 tests, building the `save_game_catalogue[]` fixture stage-08 had flagged as a prerequisite); `kfx_sim`'s `magic_powers.c` power-price functions covered (5 tests, `Cost_Default` formula only); `kfx_game`'s `game_merge.c` `get_extra_level_kind_visibility` covered (7 tests, resolving stage-04f's deferred moon-phase-state open question); `kfx_render`'s `light_data.c` allocator functions covered (9 new tests, plus a real cross-test-pollution bug found and fixed in the fixture along the way); `kfx_sim`'s `room_data.c` capacity-computation family covered (7 tests); `kfx_pathfinding`'s `ariadne_navitree.c` tag/route bookkeeping covered (9 tests); `kfx_platform`'s `bflib_video.c` `VideoScaleCallbacks` consumer covered (10 tests — the third and last known callback consumer for that library, now fully landed); `kfx_pathfinding`'s `ariadne_regions.c` (10 tests) and `kfx_sim`'s `power_specials.c` (6 tests) covered, targeted at this library's two currently-*unaccepted* `check_layering.py --strict` violations — both root-caused to a wrong-header choice with a trivial fix, written up in [`docs/refactor/todo/two-remaining-layering-violations.md`](../../todo/two-remaining-layering-violations.md) (which also records a third finding: one of the five *accepted* residuals, `console_cmd.c`'s `game_session_loop.h` include, is now fully dead code); `kfx_net`'s `net_resync.cpp` covered around its own accepted residual (4 tests); `kfx_sim`'s `roomspace.c` `box_placement_mode`/`drag_placement_mode` branches covered (3 new tests, closing the "packet field contents" gap stage-08b had flagged around the `get_packet` accepted residual); `kfx_render`'s `engine_redraw.c` `update_mouse_light()` covered (3 tests, the `kfx_render -> kfx_net` side of the same `get_packet_direct` accepted residual); `kfx_sim`'s `creature_graphics.c` `keepersprite_*` dispatch functions covered (4 tests, the `creature_table_add` accepted residual — see [`docs/Architecture/testing-harness.md`](../../../Architecture/testing-harness.md) §9 for the precise, checked-not-assumed tally of which of the 9 accepted symbol residuals now have coverage); 16 of 17 `creature_states_*.c` files and most of the per-library table still open — **302 tests total across all ten `kfx_*` libraries** |

No fixed order across 5–8 is prescribed — stage 5 (coverage) is worth
landing first regardless, since it's the instrument that tells stages 6–8
whether they're making real progress rather than just adding test count.
Stages 6 and 7 are independent of each other and of stage 8; a maintainer
can pick whichever is highest-value to start executing.

## 4. What "comprehensive" means here, concretely

Not "test everything" — `kfx_sim` alone has 83 source files and testing
every function would be its own multi-month project independent of any
plan. Concretely, each stage document below defines its own scope
boundary and exit criterion the same way stages 1–4 did, and stage-08 in
particular proposes a lighter-weight per-library "what would make this
library's suite actually useful, not just non-empty" checklist rather
than a percentage target — see that document for the reasoning.
