# Stage 4e — Per-library rollout: `kfx_net`

See [stage-04-kfx-config.md](stage-04-kfx-config.md) for how this document
fits into the rollout, and [stage-02](stage-02-testability-and-fakes.md)
§5 for why `kfx_net` is next: "packet (de)serialization and checksum logic
is a strong pattern-A fit."

## What landed: `kfx_net_utest`

Links the whole `kfx_net` `OBJECT` library plus its real dependency ladder
(`kfx_render`, `kfx_sim`, `kfx_pathfinding`, `kfx_config`, `kfx_platform`)
— same shape as `kfx_render_utest`, including the explicit `centitoml`
link.

`kfx_net`'s one accepted residual (`check_layering_symbols.py`'s list —
`kfx_frontend_state`, `game`, `kfx_game_state`, all referenced from
`net_resync.cpp`, the documented raw-blob resync serialization,
architecture.md §6.2) needed a stub, the same shape as `kfx_sim`'s
`get_packet` family (stage-04c) and `kfx_render`'s reuse of it
(stage-04d). Two small shared `STATIC` libraries, defined in
`kfx_net/tests/CMakeLists.txt`: `kfx_game_state_test_stubs` (`struct
Game game;` / `struct KfxGameState kfx_game_state;`) and
`kfx_frontend_state_test_stub` (`struct KfxFrontendState
kfx_frontend_state;`) — split into two, not one, because they turned out
to stop being needed on different schedules once `kfx_game_utest` came
along (stage-04f): `kfx_game_utest` transitively links `kfx_net` too, but
by then `kfx_game` itself provides the real `game`/`kfx_game_state`, so
only `kfx_frontend_state_test_stub` is still needed there. Their real
definitions live in `kfx_game`/`kfx_frontend`, and linking either whole
library just for these three globals would cascade into their own
dependency graphs for no benefit to testing `kfx_net`.

Two things worth noting about this one, compared to the `get_packet`
family:

- It's the **same accepted violation already listed in both audits** —
  `check_layering.py`'s `ACCEPTED_VIOLATIONS` (an `#include`-level entry,
  since `net_resync.cpp` really does `#include kfx_game_state.h`/
  `kfx_frontend_state.h`/`game_legacy.h` directly, not via a bare
  `extern`) *and* `check_layering_symbols.py`'s
  `ACCEPTED_SYMBOL_VIOLATIONS`. Every other stub so far
  (`kfx_test_main`'s `kfxmain`, `kfx_sim`'s `get_packet` family) was only
  ever a symbol-level residual; this is the first one where the `#include`
  itself was already accepted too, which is why the compile step (not
  just the link step) needed the real `kfx_game_state.h`/
  `kfx_frontend_state.h` headers to be reachable — they were, for free,
  since every `kfx_*/include` dir is already on `kfx_common_opts`'s
  global include path.
- **Not shared** with `kfx_packet_test_stubs` (a separate library, same
  reasoning as `kfx_test_main` vs. this one — different symbols, no
  overlap). It *did* turn out to need splitting into two pieces of its
  own one stage later, once `kfx_game_utest` came along and resolved
  `game`/`kfx_game_state` for free by linking the real `kfx_game` —
  exactly the "one of the two libraries whose state it fakes will
  actually be in scope" prediction this bullet originally made, confirmed
  in [stage-04f-kfx-game.md](stage-04f-kfx-game.md).

Also confirmed directly: `kfx_net_utest` does **not** link
`kfx_packet_test_stubs` — `kfx_net` physically contains `packets.c`/
`packets_misc.c`, the real implementations that stub fakes for libraries
that don't link `kfx_net`. Linking both would be a duplicate-definition
error, caught by trying it (not just reasoned about) before landing.

## What's tested so far

`src/kfx_net/tests/net_checksums_test.cpp` — 4 `TEST_CASE`s against
`get_thing_checksum`, a pure per-`Thing` rolling checksum. Pattern A on
`kfx_sim_state` (the memset fixture from stage-02 §2), reused from a
`kfx_net` context — legitimate, since `kfx_net` depends on `kfx_sim`.

Deliberately **property-based, not value-based**: no test asserts a
specific numeric checksum. `CHECKSUM_ADD`'s rolling-hash internals aren't
part of the function's documented contract, only "deterministic, and
sensitive to the fields it reads over" is — hard-coding an expected
checksum value would just be a second, easier-to-get-wrong copy of the
implementation. Covered: zero for a non-existent thing, zero for a
non-synchronized thing class (`TCls_EffectElem`), determinism (same state
→ same checksum, twice), and sensitivity (changing `owner` changes the
result).

## CI

`.github/workflows/build-prototype.yml`'s `unit-tests` job now builds all
six test binaries; `ctest` stays unscoped. 23 tests total across the six
libraries.

## Exit criterion

- `kfx_net_utest` builds against the real `kfx_net` `OBJECT` library and
  all 4 `TEST_CASE`s pass (23/23 across all six libraries).
- `check_layering.py --strict` and `check_layering_symbols.py --strict`
  both unaffected — same 2 pre-existing violations, same 15 accepted
  symbol residuals, zero new ones.

## What's next

Per stage-02 §5: `kfx_game` (`get_gameturn()`'s 329 fan-in makes it a
high-value, low-risk target — a pure state read, widely relied on), then
`kfx_frontend`. `kfx_script`/`kfx_apploop` stay deferred to last per
stage-02 §4. `kfx_sim`'s own per-cluster follow-up (stage-04c) remains
open independent of the library-rollout order.
