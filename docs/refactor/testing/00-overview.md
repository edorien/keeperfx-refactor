# KeeperFX: Unit Test Harness — Overview

Status: **planning, just started**. This directory is the design doc for a
new, comprehensive unit-test harness — one capable of testing individual
functions in isolation, library by library — as a follow-on to the
`src/kfx_*/` library split described in [`docs/refactor/`](../). That plan
is what makes this one possible: before it, `src/` was one flat,
uncomposable 266-file directory with no boundary preventing a "unit" test
from dragging in the whole game; after it, each `kfx_*` library is a real,
independently-linkable CMake target with a declared, acyclic set of
dependencies (see [`docs/Architecture/architecture.md`](../../Architecture/architecture.md)).
Read that document first if you haven't — this plan leans on its vocabulary
(the dependency ladder, per-library state structs, the callback-struct
interface pattern) throughout.

This is the index document, following the same convention as
[`../00-overview.md`](../00-overview.md): one file per stage, this one first
for the why, the chosen shape, and the guiding rules. Only stage 1 exists as
a fully-drafted document so far — see §6.

## 1. Why

KeeperFX has exactly two things that currently run under a test runner:

- **`src/ftests/`** — a CUnit-based *functional*-test scaffold. It launches
  the real game against real level/campaign data, drives it for some number
  of turns, and asserts on outcomes (`bug_imp_tp_attack_door__claim`,
  `bug_pathing_stair_treasury`, …). Valuable, and the right tool for
  reproducing gameplay bugs — but each test needs the original game's data
  files, a frame budget, and exercises dozens of functions at once. It
  cannot answer "does `find_line_of_sight_2d` handle a degenerate segment
  correctly" without also standing up a level.
- **`tests/`** — a second, older CUnit harness (`tst_main.cpp`,
  `001_test.cpp`, `tst_enet_client/server.cpp`) that predates the `kfx_*`
  split. It `#include`s `<SDL2/SDL.h>` (the codebase migrated to SDL3 during
  the refactor — see `docs/refactor/` and the SDL3-migration history in
  `CMakeLists.txt`), is not wired into `CMakeLists.txt` at all — no
  `add_subdirectory`, no target, nothing in `CMakeLists.txt` even mentions
  `tests/` — and per stage 0's own investigation
  ([`stage-00-safety-net.md`](../stage-00-safety-net.md), "Not yet done"),
  the `Makefile`'s `tests:` target already referenced source files that
  didn't exist in that checkout. It does not build today. It is dead code,
  not a foundation to build on (see stage 1 for what to do about it).

Neither is a **unit** test harness: something that lets a contributor write
a small, fast, no-game-data-required test for one function, run hundreds of
those in well under a second, and get a specific failure (`bflib_math.c:142
lbSin(1024) expected 0 got 3`) instead of "the game desynced on turn 40."
The `kfx_*` split's whole point — per
[`../00-overview.md` §1](../00-overview.md#1-why) — was partly to make
subsystems "testable in isolation"; this plan is collecting on that.

## 2. Goals

- A real, CMake-integrated unit-test framework, buildable and runnable
  locally (`ctest`) and in CI, with **zero dependency on the original
  game's data files** — the thing `src/ftests/` cannot offer.
- One test binary per `src/kfx_*/` library, linking only that library and
  its declared dependencies (per the existing dependency ladder) — so
  writing a `kfx_platform` test cannot accidentally reach into `kfx_sim`
  types, the same discipline `check_layering.py --strict` already enforces
  for production code.
- A rollout that starts where it's cheapest (the lowest, most nearly-pure
  layers) and gets progressively harder as it climbs the ladder, so the
  harness mechanics are proven before the hard testability problems
  (global state, SDL, callback-struct fakes) are tackled.
- A documented pattern for *how* to unit-test a function that touches a
  per-library `extern` state struct or calls through a callback-struct
  interface — most functions in this codebase do at least one of those, so
  "just write a `TEST_CASE`" isn't enough guidance on its own (stage 2).

## 3. Non-goals

- **Not a replacement for `src/ftests/`.** Functional/gameplay-behavior
  tests that need a real level running for real turns stay exactly where
  they are; this plan is additive.
- **Not 100% coverage on day one, or ever mandated.** The goal is a harness
  contributors reach for by default on new/changed code, not a retroactive
  test-everything mandate — `kfx_sim` alone is 83 source files; testing it
  exhaustively is its own multi-month effort, not something to plan away in
  one document.
- **Not a rewrite for testability.** If a function is hard to test because
  of how it's currently written, the default answer is a test-double
  seam (fake callback table, a way to reset the relevant state struct) —
  not restructuring production code to be "more testable" as a side effect.
  Exceptions get called out explicitly when they come up, the same way the
  library-split plan flagged real coupling problems instead of routing
  around them silently.
- **Not solving Windows/mingw test execution in CI up front.** See stage 1
  §"Platform scope" — tests target the native Linux build first.

## 4. Framework choice: Catch2 (v3)

Options considered:

| Framework | Verdict |
|---|---|
| **CUnit** (already vendored in `deps/CUnit-2.1-3`) | Rejected as the default going forward. C-style suite/registry boilerplate (see `tests/tst_main.h`'s `TestRegistryWrapper` — hand-rolled specifically to paper over that boilerplate), weaker assertion/matcher vocabulary, and it's exactly what the existing dead `tests/` harness already used and bit-rotted with. `src/ftests/` keeps using it for functional tests — no change proposed there; this is only about which framework new **unit** tests reach for. |
| **doctest** | Single header, fast to compile, low ceremony — a reasonable alternative. Loses to Catch2 v3 mainly on maturity of matchers/generators for the kind of data-driven cases fixed-point math and config-parsing code benefit from, and on ecosystem familiarity (more prior art to copy from). Worth revisiting if Catch2's compile cost turns out to matter more than expected once dozens of test binaries exist. |
| **Catch2 v3** | **Chosen.** Split into a compiled library (unlike v2's single-header-everywhere model, which re-parses the whole framework per translation unit — a real cost across nine per-library test binaries), rich `TEST_CASE`/`SECTION`/`GIVEN`-`WHEN`-`THEN` vocabulary, built-in `REQUIRE`/`CHECK` matchers including floating point (relevant for the fixed-point/trig code in `kfx_platform`), and a CMake integration module (`Catch2::Catch2WithMain`, `catch_discover_tests()`) that registers every `TEST_CASE` as its own `ctest` entry with no hand-written registry — replacing exactly the boilerplate `tst_main.h` had to hand-roll for CUnit. |

Concrete acquisition mechanism, CMake wiring, and the new `KFX_BUILD_TESTS`
option are in [stage-01](stage-01-framework-and-scaffold.md).

## 5. Target shape

One test executable per `src/kfx_*/` library, physically living alongside
the library it tests — mirroring `src/ftests/`'s status as a sibling
directory of `src/` and `include/` within each library, not a parallel
top-level tree that has to be kept in sync by hand:

```
src/kfx_platform/
├── include/
├── src/
└── tests/            ← new: kfx_platform_utest, Catch2, links kfx_platform only
src/kfx_config/
├── include/
├── src/
└── tests/            ← new: kfx_config_utest, links kfx_config + kfx_platform
...
```

Each `tests/` directory is exempt from `check_layering.py`'s edge checks
already, for free — `find_project_files()`
([`scripts/check_layering.py`](../../../scripts/check_layering.py), the
same filter that exempts `src/ftests/`) drops any path with `"tests"` in
its parts before classification runs. No script change needed to make this
work; stage 1 confirms it rather than assuming it.

Each library's test binary only links that library's own `OBJECT` target
(the `std`, `BFDEBUG_LEVEL=0` variant — see
[architecture.md §12.3](../../Architecture/architecture.md#123-two-executables-differ-only-in-bfdebug_level))
plus whatever the dependency ladder says it's allowed to depend on. This
means a test for a `kfx_sim` function can call straight into `kfx_config`
and `kfx_platform` code (real code, not fakes — cheap since those are
lower-ranked and already linked), and reaches `kfx_render`/`kfx_net`/
`kfx_game`/`kfx_frontend`/`kfx_script` state or behavior only through
whatever callback-struct/accessor interface production code already uses
to cross that same boundary (stage 2).

**Caveat found while implementing stage 1, since resolved (see
[`../todo/check-layering-symbol-level-blind-spot.md`](../todo/check-layering-symbol-level-blind-spot.md)):**
"links that library's own `OBJECT` target" initially didn't work for
`kfx_platform` as-is — several of its files reached into `kfx_render`/
`kfx_config`/`kfx_sim` symbols via raw `extern` declarations that
`check_layering.py` couldn't see (it only tracks `#include` edges), so
linking the real `kfx_platform` target alone failed outright.
`kfx_platform_utest` briefly worked around this by compiling only the
specific tested source file directly. That workaround is gone: the
underlying issue was fixed at the source (2026-08-29, commit
`a5f5c295d`) — each raw reference was either relocated to the library that
actually implements it or routed through a callback struct with a safe
no-op default, the same pattern used everywhere else — and
`kfx_platform_utest` now links the real, whole `kfx_platform` `OBJECT`
library again, verified against a real build. Whether the same class of
problem exists in any other library is unknown until stage 4 actually
tries linking each of them; the new `scripts/check_layering_symbols.py`
post-build audit (added by the same fix) is the tool to check with before
assuming it doesn't.

## 6. Stage index

| # | Document | Goal | Status |
|---|----------|------|--------|
| 1 | [stage-01-framework-and-scaffold.md](stage-01-framework-and-scaffold.md) | Pick the acquisition mechanism, wire Catch2 + `KFX_BUILD_TESTS` into CMake, land a pilot `kfx_platform` test binary | done, verified against a real build |
| 2 | [stage-02-testability-and-fakes.md](stage-02-testability-and-fakes.md) | Document how to unit-test a function that touches `extern` state structs or a callback-struct interface; pick and justify the per-library rollout order | drafted |
| 3 | [stage-03-ci-integration.md](stage-03-ci-integration.md) | Wire `ctest` into `.github/workflows/build-prototype.yml` as a merge gate (mirroring how `check-layering` was added in stage 13.4 of the library-split plan) | done; coverage reporting still open (§7.1) |
| 4 | Per-library rollout ([stage-04-kfx-config.md](stage-04-kfx-config.md), [stage-04b-kfx-pathfinding.md](stage-04b-kfx-pathfinding.md), [stage-04c-kfx-sim.md](stage-04c-kfx-sim.md), [stage-04d-kfx-render.md](stage-04d-kfx-render.md), [stage-04e-kfx-net.md](stage-04e-kfx-net.md), [stage-04f-kfx-game.md](stage-04f-kfx-game.md), [stage-04g-kfx-frontend.md](stage-04g-kfx-frontend.md) so far) | One document per `kfx_*` library (or per-library first pass) as its test suite actually gets built out, in the order stage 2 settles on | `kfx_platform` + `kfx_config` + `kfx_pathfinding` + `kfx_render` + `kfx_net` + `kfx_game` + `kfx_frontend` done — the "normal" library rollout order is complete; `kfx_sim` first pass done (needs its own multi-part follow-up); `kfx_script`/`kfx_apploop` remain, deliberately deferred (need their own design pass, not a mechanical repeat) |
| 5 | Legacy `tests/` disposition | Decide: delete the dead CUnit/SDL2 `tests/` directory outright, or fold its two still-meaningful programs (`tst_enet_client`/`tst_enet_server` — manual multiplayer smoke tools, not unit tests) into a clearly-labeled `tools/` or `scripts/` home instead | not started |

Stages 1–2 landed first: they establish the mechanics (does the harness
build, does a real `TEST_CASE` run under `ctest`) and the pattern (how do
you actually write a unit test against this codebase's global-state style)
before committing to a library-by-library schedule. Stage 3 (CI gate)
followed immediately after, per the same reasoning stated here originally —
an unenforced test suite decays silently, which is exactly the failure
mode `check_layering.py --strict` was written to close for the layering
rule itself. Stage 4 (the actual per-library rollout) is next.

## 7. Open questions (need a maintainer call before stage 3+)

1. **Coverage tooling and target.** Nothing proposed yet. `gcov`/`lcov` is
   the obvious native-Linux-build option; whether to gate merges on a
   coverage floor (and for which libraries) is a separate decision from
   "does the harness exist."
2. **Windows/mingw execution.** Stage 1 scopes tests to the native Linux
   build only (matches how `linux.mk`/native-Linux CMake was already the
   build used to verify the library-split plan itself — see
   [`../stage-00-safety-net.md`](../stage-00-safety-net.md)). Whether
   Catch2 unit tests ever need to run under the mingw cross-compile (there
   is no Windows CI runner or Wine step today) is open; likely not worth it
   unless library code that's genuinely Windows-conditional starts
   accumulating untested branches.
3. **`kfx_script` (Lua) and `kfx_apploop`.** Both are special-ranked
   (allowed to depend on everything) and, per
   [architecture.md §12.2](../../Architecture/architecture.md#122-kfx_script-boundary-is-deliberately-wide),
   `kfx_script` already has a deliberately wide, undocumented-as-stable
   boundary. Unit-testing them meaningfully may need LuaJIT test fixtures
   and a fuller in-process game-state fake than the lower layers do —
   likely the last two libraries in the stage 4 rollout order, not the
   first.
