# Stage 12 — Slim the app target

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

By this point, stages 2–11 have moved everything with a clear library
home out of `src/`. What's left should be just the process entry point.

## Scope

- `main.cpp` — contains `int LbBullfrogMain(...)`, the real entry point
  called from `main()`. The investigation found this file currently
  contains much more than bootstrap: it holds game-loop logic that
  belongs in `kfx_game` (alongside `main_game.c`, see
  [stage-09-kfx-game.md](stage-09-kfx-game.md)). Split it: the process
  bootstrap (arg parsing hand-off to `kfx_config`'s `StartupParameters`,
  subsystem init calls, top-level exception/crash wiring) stays; the
  actual `game_loop()`-adjacent logic moves to `kfx_game`.
- `windows.cpp`, `linux.cpp`, `cdrom.*`, `steam_api.*` — these were
  already reclassified into `kfx_platform` in stage 3 as OS-abstraction
  implementations of `platform.h`. Confirm they haven't crept back into
  `src/` by this point.
- `keeperfx.hpp` — after stage 5 distributed its structs/globals/function
  prototypes to their owning libraries, whatever's left (genuinely
  `main.cpp`-specific bootstrap declarations, if anything) stays as a
  small header next to `main.cpp`. If nothing app-specific remains,
  delete it and have `main.cpp` include the specific per-library headers
  it actually needs.

## Work

1. Verify against stages 3 and 5's "what moved" lists that nothing
   remains in `src/` except `main.cpp` (and possibly a slimmed
   `keeperfx.hpp`).
2. Extract the game-loop portion of `main.cpp` into `kfx_game`, per the
   `main_game.c` orchestration-layer question raised in stage 9 — resolve
   that question here if it wasn't already: does the orchestrator that
   ties frontend+net+sim+render together per frame live in `kfx_game`, or
   at this app layer above `kfx_game`? Recommendation: keep it in
   `kfx_game` since `main_game.c` already sets precedent there; `main.cpp`
   just calls `kfx_game`'s top-level `run_game()`-equivalent entry point.
3. Update `CMakeLists.txt`: top-level build description becomes "build
   each `libs/*` target, link them all into `main.cpp` to produce
   `keeperfx`/`keeperfx_hvlog`."

## Exit criterion

`src/` contains only the process-entry-point file(s). `CMakeLists.txt`'s
top level reads as a short, flat list of library builds plus two thin
executable links — not a 266-file glob.
