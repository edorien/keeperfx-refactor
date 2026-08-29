# Stage 4c — Per-library rollout: `kfx_sim` (first pass)

See [stage-04-kfx-config.md](stage-04-kfx-config.md) for how this document
fits into the rollout, and [stage-02](stage-02-testability-and-fakes.md)
§5 for why `kfx_sim` is next: the hub, 83 source files, the largest
library. This is explicitly a **first pass**, not full coverage — see
"What's next" below. `kfx_sim`'s size is exactly why stage-02 predicted it
would need its own per-cluster breakdown, the same way the library-split
plan broke `kfx_sim` into stage 6 plus three appendices; this document is
appendix zero of that breakdown, covering only the scaffold and one
self-contained function.

## What landed: `kfx_sim_utest`

Links the whole `kfx_sim` `OBJECT` library plus its real dependency ladder
(`kfx_pathfinding`, `kfx_config`, `kfx_platform`) — same shape as
`kfx_pathfinding_utest`, including the same explicit `centitoml` link.

## A new kind of wrinkle: accepted symbol-level residuals, not a bug

Unlike `kfx_platform`/`kfx_config`/`kfx_pathfinding`, linking the whole
`kfx_sim` library did **not** come back clean. Five symbols were
unresolved:

- `get_packet`, `get_packet_direct`, `set_packet_action`,
  `set_players_packet_action` — declared in `kfx_sim`'s own
  `packet_data.h` (`struct Packet` and its accessors), but really
  *implemented* in `kfx_net/src/packets_misc.c` and friends.
- `creature_table_add[]` — declared in `kfx_sim`'s `creature_graphics.h`
  as `extern struct KeeperSprite creature_table_add[]`, but really
  *populated* in `kfx_render/src/custom_sprites.c`.

Critically, **these are not the same problem**
[`check-layering-symbol-level-blind-spot.md`](../todo/check-layering-symbol-level-blind-spot.md)
found and fixed. That document's fixes were for code hiding a real
architectural violation behind a bare `extern` to dodge
`check_layering.py`. These five are already in
`check_layering_symbols.py`'s `ACCEPTED_SYMBOL_VIOLATIONS` list, already
investigated during that fix, and already judged intentional: "a
higher-ranked library implementing a lower-ranked interface is fine —
only the reverse is a violation" (verbatim from `creature_graphics.h`'s
own comment on `creature_table_add[]`). `struct Packet`'s type has to live
at or below `kfx_sim` because `kfx_sim` code dereferences it directly and
pervasively; the functions that actually move packets over the network
have to live in `kfx_net`, which owns the transport. Same story for
`struct KeeperSprite`. Nothing here needs fixing — it's the accepted
shape.

That does mean `kfx_sim_utest` needs to satisfy these five symbols itself,
the same way `kfx_platform_utest` needs `kfx_test_main` for `kfxmain()`.
Linking the real `kfx_net`/`kfx_render` libraries to get them was
considered and rejected: `kfx_net` has its *own* accepted residual
(`net_resync.cpp` reaching into `kfx_game_state`/`kfx_frontend_state`,
architecture.md §6.2's intentionally-preserved raw-blob resync) that would
cascade the same problem one layer further up for no benefit to testing
`kfx_sim` itself. Instead, `src/kfx_sim/tests/kfx_sim_test_stubs.cpp`
provides minimal definitions: `get_packet`/`get_packet_direct` return a
local stub `struct Packet`, `set_packet_action`/`set_players_packet_action`
are no-ops, and `creature_table_add` is a size-1 placeholder array (never
indexed by anything the current test suite exercises — sized against the
real `KEEPERSPRITE_ADD_NUM`, 16383, would require pulling in a
`kfx_render` header `kfx_sim` correctly doesn't expose).

**Pattern for later stages**: when a library's accepted-residual list
(`check_layering_symbols.py --strict`'s output) is non-empty, check it
*before* assuming the library will link cleanly — it's a strictly cheaper
question than trying the link and finding out. `kfx_render`, `kfx_net`,
and `kfx_frontend` all have at least one entry in that list already (per
the audit's current output) and should expect to need the same kind of
small, targeted stub.

## What's tested so far

`src/kfx_sim/tests/map_utils_test.cpp` — 2 `TEST_CASE`s (6 assertions)
against `map_utils.c`'s `small_around_index_in_direction`: a pure
coordinate function (calls into `kfx_platform`'s already-tested
`LbArcTanAngle`, touches no `kfx_sim_state`) that resolves a source→
destination vector to one of 4 cardinal directions.

Worth calling out: the expected values weren't hand-derived from
`LbArcTanAngle`'s documented angle convention and trusted blind — they
were confirmed by actually running the function once (a throwaway
`fprintf`-based exploratory test, replaced before landing) and matching
the real output. This turned out to matter — see the next section.

## A near-miss, again: don't trust a quick grep over a real build

This is the second time in two consecutive stages that a quick
investigation got contradicted by actually building. While looking for
`kfx_pathfinding`'s pilot function
([stage-04b](stage-04b-kfx-pathfinding.md)), a grep-based "this include is
dead" call turned out to be wrong. This time it wasn't a mistake that
shipped — the empirical-verification habit that stage-04b's near-miss
argued for was applied here on purpose, and correctly caught nothing
(the hand-derived values matched), but the process is the same lesson
twice: for anything involving `LbArcTanAngle`'s rounding behavior at
axis-aligned angles (the function's own comment flags this as a case that
needs "proper rounding, up or down"), empirical confirmation is cheap
(one throwaway test run) and hand-derivation is not reliably cheap enough
to skip it.

## CI

`.github/workflows/build-prototype.yml`'s `unit-tests` job now builds all
four test binaries; `ctest` stays unscoped. 15 tests total across the
four libraries.

## Exit criterion

- `kfx_sim_utest` builds against the real `kfx_sim` `OBJECT` library (with
  the five-symbol stub file, not a per-file compile workaround) and both
  `TEST_CASE`s pass.
- `check_layering.py --strict` and `check_layering_symbols.py --strict`
  both unaffected — same 2 pre-existing `#include`-level violations, same
  15 accepted symbol-level residuals, zero *new* ones introduced by this
  stage's stub (confirming the stub's signatures/behavior match what the
  audit already expected, not a coincidentally-different set that happens
  to link).

## What's next

This is a first pass, not `kfx_sim` coverage. `kfx_sim` needs its own
per-cluster follow-up documents (map/thing, creature, room, player, in
roughly that order of increasing pattern-B involvement) rather than one
more monolithic stage — not scoped here. Per stage-02 §5, `kfx_render` is
the next *library* in the rollout order once (or instead of, depending on
how the maintainer wants to sequence it) `kfx_sim`'s own follow-up passes
land.
