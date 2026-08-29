# Stage 4d — Per-library rollout: `kfx_render`

See [stage-04-kfx-config.md](stage-04-kfx-config.md) for how this document
fits into the rollout, and [stage-02](stage-02-testability-and-fakes.md)
§5 for why `kfx_render` is next: "starts touching pattern-C territory
(video mode, textures) alongside plenty of pure-ish math (lighting, lens
effects)." This first pass stays in the pure-ish-math half.

## What landed: `kfx_render_utest`

Links the whole `kfx_render` `OBJECT` library plus its real dependency
ladder (`kfx_sim`, `kfx_pathfinding`, `kfx_config`, `kfx_platform`) — same
shape as `kfx_sim_utest`, including the explicit `centitoml` link.

## Extracting the shared packet stub

Building `kfx_render_utest` hit `kfx_sim`'s accepted `get_packet`/
`get_packet_direct`/`set_packet_action`/`set_players_packet_action`
residual again (stage-04c) — expected, since `kfx_render_utest`
transitively links `kfx_sim`. Two things changed from a straight copy of
`kfx_sim_utest`'s stub, though:

1. **`creature_table_add[]` did *not* need stubbing here.** `kfx_render`
   physically contains `custom_sprites.c`, which is where that array is
   *really* populated (per `creature_graphics.h`'s own comment). Linking
   the real `kfx_render` library resolves it for free — confirming the
   dependency-ladder theory in the most direct way possible: the accepted
   residual stops being a residual once the library that actually
   implements it is in scope.
2. **The `get_packet` family stub had to become shared**, not duplicated.
   `src/kfx_sim/tests/packet_test_stubs.cpp` is now a small `STATIC`
   library (`kfx_packet_test_stubs`), defined in `kfx_sim/tests/`
   (processed first) and linked by both `kfx_sim_utest` and
   `kfx_render_utest` — the same "extract once two consumers need it"
   call `kfx_test_main` (stage-01) already established for `kfxmain()`.
   `kfx_sim_test_stubs.cpp` (just the `creature_table_add[]` part) stayed
   `kfx_sim_utest`-only, compiled directly into that target rather than
   shared, precisely *because* it must NOT be linked into any target that
   also links `kfx_render` — doing so would be a duplicate-definition
   error against the real array.

One mechanical wrinkle in the extraction: the new `kfx_packet_test_stubs`
library needed `target_link_libraries(... kfx_common_opts ...)`, not just
a couple of `target_include_directories()` entries — `packet_data.h`
transitively pulls in `version.h` → `ver_defs.h`, which is only
reachable via `kfx_common_opts`'s `INTERFACE_INCLUDE_DIRECTORIES` (it's
`configure_file()`'d to the repo root, not a `src/` tree at all).

**Pattern for later stages, sharpened from stage-04c's version**: before
copying a stub file for a newly-transitively-linked library, check
whether the *new* library being linked actually implements one or more of
the symbols the stub currently fakes. If so, drop that part of the stub
(or extract a smaller shared piece) rather than carrying dead — and
dangerous, duplicate-definition-risking — stub code forward.

## What's tested so far

`src/kfx_render/tests/light_data_test.cpp` — 4 `TEST_CASE`s against
`light_data.c`'s simplest accessors (`light_is_invalid`,
`light_get_light_radius`/`light_set_light_radius`,
`light_is_light_allocated`), using `light_initialise()` as the pattern-A
reset (the module's own production reset function, same idiom as
`kfx_pathfinding`'s `triangulation_initxy_points()`) plus direct
manipulation of the public `lish.lights[]` array's `flags` field to probe
an "allocated" light without going through the fuller
`light_create_light()` allocation path (shadow-cache allocation, dynamic-
light bookkeeping — deliberately out of scope for this first pass, the
same call stage-04c made for `kfx_sim`).

`light_set_light_intensity`/`light_get_light_intensity` were considered
and skipped for this pass: the setter's non-dynamic-light branch
recomputes a screen-space update region from `kfx_sim_state`'s map
dimensions and calls `light_signal_stat_light_update_in_area()`, another
layer of behavior a first pass doesn't need to pull in to prove the
pattern.

## CI

`.github/workflows/build-prototype.yml`'s `unit-tests` job now builds all
five test binaries; `ctest` stays unscoped. 19 tests total across the five
libraries.

## Exit criterion

- `kfx_render_utest` builds against the real `kfx_render` `OBJECT`
  library and all 4 `TEST_CASE`s pass (19/19 across all five libraries).
- `check_layering.py --strict` and `check_layering_symbols.py --strict`
  both unaffected — same 2 pre-existing violations, same 15 accepted
  symbol residuals, zero new ones.
- The shared/non-shared stub split is confirmed correct by the build
  itself succeeding without a duplicate-symbol error, not just reasoned
  about — the sharpest possible test of "did we split this right."

## What's next

Per stage-02 §5: `kfx_net` (packet (de)serialization and checksums are a
strong pattern-A fit; the transport/session layer is closer to pattern
C), then `kfx_game`, `kfx_frontend`. `kfx_sim`'s own deferred per-cluster
follow-up (map/thing/creature/room/player, stage-04c) is still open and
can land independently of continuing along the library-rollout order.
