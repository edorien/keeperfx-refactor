# Stage 4f — Per-library rollout: `kfx_game`

See [stage-04-kfx-config.md](stage-04-kfx-config.md) for how this document
fits into the rollout, and [stage-02](stage-02-testability-and-fakes.md)
§5 for why `kfx_game` is next: `get_gameturn()`'s 329 fan-in
(architecture.md §11.2) makes it "a high-value, low-risk target — a pure
state read, widely relied on."

## What landed: `kfx_game_utest`

Links the whole `kfx_game` `OBJECT` library plus its real dependency
ladder (`kfx_net`, `kfx_render`, `kfx_sim`, `kfx_pathfinding`,
`kfx_config`, `kfx_platform`) — same shape as `kfx_net_utest`, including
the explicit `centitoml` link.

This is the stage that confirmed [stage-04e](stage-04e-kfx-net.md)'s
prediction about its own stub: `kfx_game_utest` transitively links
`kfx_net`, which needs `net_resync.cpp`'s accepted-residual symbols
(`game`, `kfx_game_state`, `kfx_frontend_state`) satisfied — but by the
time this target links, `kfx_game` itself is also in scope, and it
physically contains `game_legacy.c`/`kfx_game_state.c`, the *real*
definitions of `game` and `kfx_game_state`. Linking `kfx_net`'s stub for
those two (as `kfx_net_utest` does) would have been a duplicate-definition
error against the real ones. This is what forced
[stage-04e](stage-04e-kfx-net.md)'s originally-combined
`net_resync_test_stubs.cpp` to actually be split, retroactively, into two
independent shared libraries: `kfx_game_state_test_stubs` (`game`/
`kfx_game_state` — not needed here) and `kfx_frontend_state_test_stub`
(`kfx_frontend_state` — still needed, since nothing yet links the real
`kfx_frontend`). `kfx_game_utest` links only the latter.

This is the third time this pattern has repeated (stage-04d's
`creature_table_add[]`, stage-04e's own prediction, now this) — solid
enough evidence to state as a rule rather than a one-off observation:
**an accepted-residual stub should be split per-symbol (or per tightly-
coupled symbol group) from the start, anticipating that different
consumers will resolve different pieces of it for free at different
points in the rollout**, rather than waiting to discover the split is
needed via a duplicate-definition build failure.

## What's tested so far

`src/kfx_game/tests/game_legacy_test.cpp` — 2 `TEST_CASE`s against
`game_legacy_get_gameturn()`, now a one-line accessor over
`kfx_game_state.play_gameturn` (renamed from `get_gameturn()` by the
[check-layering-symbol-level-blind-spot.md](../todo/check-layering-symbol-level-blind-spot.md)
fix — the plain name is now owned by a thin wrapper in `kfx_platform`
around a registered provider function, not tested here since
`kfx_platform_utest` doesn't wire that provider up). Pattern A on
`kfx_game_state` (memset fixture). Deliberately thin — it's genuinely a
one-line getter, and stage-02's case for testing it was its fan-in and
low risk, not algorithmic complexity.

## CI

`.github/workflows/build-prototype.yml`'s `unit-tests` job now builds all
seven test binaries; `ctest` stays unscoped. 25 tests total across the
seven libraries.

## Exit criterion

- `kfx_game_utest` builds against the real `kfx_game` `OBJECT` library
  and both `TEST_CASE`s pass (25/25 across all seven libraries).
- `check_layering.py --strict` and `check_layering_symbols.py --strict`
  both unaffected — same 2 pre-existing violations, same 15 accepted
  symbol residuals, zero new ones.
- The `kfx_net`-stub split is confirmed correct by both `kfx_net_utest`
  and `kfx_game_utest` building without a duplicate-symbol error, the
  same kind of "build itself is the sharpest test" confirmation
  stage-04d relied on.

## What's next

Per stage-02 §5: `kfx_frontend` — the most pattern-C-heavy library by
file count, lowest priority of the "normal" libraries. After that,
`kfx_script`/`kfx_apploop` (deferred per stage-02 §4) close out the
library-rollout order, alongside `kfx_sim`'s still-open per-cluster
follow-up (stage-04c).
