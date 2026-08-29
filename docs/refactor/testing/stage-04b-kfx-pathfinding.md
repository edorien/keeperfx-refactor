# Stage 4b — Per-library rollout: `kfx_pathfinding`

See [stage-04-kfx-config.md](stage-04-kfx-config.md) for how this document
fits into the rollout (one file per library, not one growing document),
and [stage-02](stage-02-testability-and-fakes.md) §5 for why
`kfx_pathfinding` is third: two dependencies (`kfx_config`, `kfx_platform`),
both already tested, and heavily geometric code that's a strong candidate
for pattern A once it's worth writing.

## What landed: `kfx_pathfinding_utest`

Links the whole `kfx_pathfinding` `OBJECT` library plus `kfx_config` and
`kfx_platform`, the same shape as `kfx_config_utest`
([stage-04-kfx-config.md](stage-04-kfx-config.md)) — no per-symbol
workaround needed, and the one wrinkle it does hit is a repeat of a
already-known one, not a new discovery: `kfx_pathfinding` transitively
links `kfx_config`, so it needs the same explicit `centitoml` link
`kfx_config_utest` does (that stage's document explains why
`kfx_common_opts`'s `INTERFACE` link to `centitoml` isn't reliable on its
own).

## A near-miss: don't trust a quick grep over a real build

While looking at `kfx_pathfinding` for this stage, `ariadne_regions.c`'s
`#include "player_data.h"` looked like dead weight — it's one of the two
pre-existing `check_layering.py --strict` violations from before this
whole testing-harness effort, and a keyword grep for `PlayerInfo`/
`get_player`/`players[` found nothing. Removing it and rebuilding
`kfx_pathfinding` (not just `kfx_pathfinding_utest`) surfaced a real
compile failure: `PLAYERS_COUNT` (a `#define`, invisible to a grep for
function/type names) is used at line 58 and is declared in
`player_data.h`. Reverted immediately rather than chasing a bigger fix
(routing that one constant through `kfx_config_state.h`, which also
`#define`s `PLAYERS_COUNT` and is already a legitimate dependency, would
work but pulls in a dozen more config headers for one macro — a real
decision, not something to make as a side effect of this stage). Recorded
here rather than silently dropped: **the `kfx_pathfinding → kfx_sim`
violation is real** (unlike everything `check-layering-symbol-level-blind-spot.md`
found and fixed), and a text search is not a substitute for actually
trying the change — this session got that wrong once already on the way
to writing this document.

## What's tested so far

`src/kfx_pathfinding/tests/ariadne_points_test.cpp` — 5 `TEST_CASE`s
against `ariadne_points.c`'s point pool (`ari_Points[POINTS_COUNT]` plus
module-static bookkeeping), the most self-contained piece of this library:
no `PathfindingWorldCallbacks` involvement, no `kfx_pathfinding_state`
involvement (that's the navigation-map cache, architecture.md §2.2a — a
different piece of state entirely), and — unlike `kfx_sim_state`'s
memset-based pattern A — a real reset function the production code already
provides: `triangulation_initxy_points()`. The fixture
(`ResetPointPool`) just calls it.

Covered: seeding the four corner points, `point_set`/`point_get`
round-tripping, `point_set_new_or_reuse`'s dedup-by-coordinate behavior,
and out-of-range id handling on both `point_set` and `point_get`.

Not attempted yet, deliberately: anything touching
`PathfindingWorldCallbacks` (the 51-entry interface, architecture.md
§2.2a) — stage-02 §5 already flagged this as "possibly split into its own
sub-stage given that interface's size," and nothing in this first pass
needed it. `ariadne_naviheap.c`/`ariadne_tringls.c`/`ariadne_edge.c` are
similarly self-contained-looking (small, local data structures) and are
reasonable next candidates before reaching for the callback interface at
all.

## CI

`.github/workflows/build-prototype.yml`'s `unit-tests` job now builds all
three test binaries (`kfx_platform_utest kfx_config_utest
kfx_pathfinding_utest`); `ctest` stays unscoped.

## Exit criterion

- `kfx_pathfinding_utest` builds against the real `kfx_pathfinding` `OBJECT`
  library and all 5 `TEST_CASE`s pass (13/13 across all three libraries
  under `ctest --test-dir out/linux`).
- `check_layering.py --strict` and `check_layering_symbols.py --strict`
  both unaffected — same 2 pre-existing violations (now including the
  confirmed-real `player_data.h` one, not just cited but actually
  re-verified this stage) and zero new symbol-level ones.
- The accidental near-revert above didn't make it past a rebuild before
  being caught and reverted — `src/kfx_pathfinding/src/ariadne_regions.c`
  is unchanged from before this stage.

## What's next

Per stage-02 §5: `kfx_sim`, the hub — 83 source files, the largest
library, both patterns A and B in heavy use, and expected to need its own
per-cluster breakdown rather than a single document.
