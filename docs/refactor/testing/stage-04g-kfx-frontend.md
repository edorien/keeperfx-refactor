# Stage 4g — Per-library rollout: `kfx_frontend`

See [stage-04-kfx-config.md](stage-04-kfx-config.md) for how this document
fits into the rollout, and [stage-02](stage-02-testability-and-fakes.md)
§5 for why `kfx_frontend` is next (and last of the "normal" libraries):
"almost entirely UI/input — the most pattern-C-heavy library by file
count (43 sources). Lowest priority."

## What landed: `kfx_frontend_utest`

Links the whole `kfx_frontend` `OBJECT` library plus its full real
dependency ladder (`kfx_game`, `kfx_net`, `kfx_render`, `kfx_sim`,
`kfx_pathfinding`, `kfx_config`, `kfx_platform`) — the largest link list
of any `*_utest` so far, and, notably, **the first one that needed none of
the accepted-residual stub libraries** (`kfx_packet_test_stubs`,
`kfx_game_state_test_stubs`, `kfx_frontend_state_test_stub`). Predictable
in hindsight, and predicted in [stage-04e](stage-04e-kfx-net.md)/
[stage-04f](stage-04f-kfx-game.md): with `kfx_net`, `kfx_game`, *and*
`kfx_frontend` all genuinely linked here, every symbol those three stubs
exist to fake is now provided for real by the library that actually owns
it. Confirmed by the build succeeding on the first attempt with no stub
libraries listed at all — not just reasoned about in advance.

## What's tested so far

`src/kfx_frontend/tests/gui_topmsg_test.cpp` — 3 `TEST_CASE`s against
`erstat_inc()`, an error-statistics counter over a small module-static
array (`erstat[]`, 10 entries), reset via the module's own
`erstats_clear()` — the same self-contained-module-with-its-own-reset
idiom as `ariadne_points.c` (stage-04b) and `light_data.c` (stage-04d).
Covered: incrementing returns the running count since the last flush,
different `stat_num`s track independently, and an out-of-range
`stat_num` returns the sentinel `1` *without* incrementing any real slot
(verified as a property — a subsequent valid call still returns `1`, not
some polluted value — not just that the out-of-range call itself doesn't
crash).

This was, as stage-02 predicted, harder to find than in any library so
far: most of `kfx_frontend`'s other files
(`button_snapping.c`/`front_landview.c`/`gui_vscroll.c`/…) are directly
coupled to GUI widget arrays, save-game catalogue scans, or menu-active
state — real pattern-B/C territory, not attempted in this first pass.

## CI

`.github/workflows/build-prototype.yml`'s `unit-tests` job now builds all
eight test binaries; `ctest` stays unscoped. 28 tests total across the
eight libraries.

## Exit criterion

- `kfx_frontend_utest` builds against the real `kfx_frontend` `OBJECT`
  library and all 3 `TEST_CASE`s pass (28/28 across all eight libraries).
- `check_layering.py --strict` and `check_layering_symbols.py --strict`
  both unaffected — same 2 pre-existing violations, same 15 accepted
  symbol residuals, zero new ones.
- No accepted-residual stub library was needed, confirmed by a successful
  build with none linked — the predicted end state for the stub-sharing
  pattern stage-04d/04e/04f tracked across the last three stages.

## What's next

This closes out the "normal" library rollout order from stage-02 §5.
Remaining, deliberately deferred per that document's §4: `kfx_script`
(Lua bindings — needs a LuaJIT test fixture, not just a linked library)
and `kfx_apploop` (the per-frame session loop — needs a fuller in-process
game-state fake than any library so far). Both need their own design pass
before a first test lands, not a mechanical repeat of this stage's
pattern. `kfx_sim`'s per-cluster follow-up (stage-04c) also remains open
and can proceed independently.
