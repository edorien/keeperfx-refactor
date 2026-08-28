# KeeperFX: Multistage Plan to Split `src/` into Logical Libraries — Overview

Status: **complete / historical**. All 13 stages landed; `src/` now holds
only `main.cpp`, everything else lives under `libs/<name>/{src,include}/`,
and `scripts/check_layering.py --strict` enforces the dependency order in
CI (stage 13.4). This directory describes the *migration* — for the
steady-state architecture (which library owns what, the dependency order,
how to add an interface across a layer boundary), see
[`docs/data_structure.md`](../data_structure.md)'s "Library architecture"
section instead; that's the source of truth going forward. Keep this
directory around for the historical design rationale (why each boundary
is where it is), but don't expect it to track the code after this point.
Scope: `src/` (266 `.c`/`.cpp` files + headers, ~243k lines of `.c`/`.cpp`, ~271k including headers) *as it was when this plan started* — see `docs/data_structure.md` for current numbers.

This is the index document. Each stage has its own file in this directory
(`stage-00-*.md` … `stage-13-*.md`); read this one first for the why, the
target shape, and the guiding rules that apply to every stage.

## 1. Why

`src/` is one flat directory of 266 source files, built by a single
`file(GLOB_RECURSE src/*.c src/*.cpp)` in [`CMakeLists.txt`](../../CMakeLists.txt)
into two executable targets (`keeperfx`, `keeperfx_hvlog`). There is no
enforced boundary between subsystems: any file can `#include` any other
file's header, and a systematic audit (below) confirms many already do,
across what should be one-directional layers. This:

- makes incremental builds slow (touching a low-level header rebuilds
  unrelated high-level code),
- makes it hard to reason about or test a subsystem in isolation,
- makes onboarding harder (no map of "what depends on what"),
- blocks reuse of self-contained pieces (e.g. the platform layer or the
  pathfinder) outside of KeeperFX itself.

The goal is a small number of internal libraries with a strict, acyclic
dependency graph, reached through many small, always-buildable stages —
never a single risky rewrite.

## 2. Non-goals

- Not a rewrite. No behavior changes, no algorithmic changes, no touching
  `deps/*` (already separate and fine).
- Not a move to separate repos/submodules — this plan keeps everything in
  one repo as multiple internal library targets, matching how `deps/`
  already works. The most self-contained candidates (`kfx_platform`,
  pathfinding) could be reconsidered for extraction later, once proven
  decoupled — see stage 6's appendix.
- Not a one-shot `mkdir libs && git mv`. Physical file moves happen last,
  per library, only after that library's dependencies are proven acyclic by
  the build itself.

## 3. Current state — headline evidence

File/LOC counts by naming-convention cluster (headers + sources):

| cluster (candidate library)     | files | LOC    |
|-----------------------------------|------:|-------:|
| `front_*`/`frontmenu_*`/`gui_*`  |    66 | 30,992 |
| `bflib_*` (platform layer)       |    69 | 31,281 |
| `creature_*`                     |    48 | 27,909 |
| `thing_*`                        |    26 | 27,365 |
| `config_*`                       |    48 | 21,532 |
| (uncategorized by prefix, now classified — see §4) | 72 | 33,960 |
| `player_*`                       |    15 | 14,345 |
| `lvl_script*`                    |    14 | 13,775 |
| `engine_*`                       |    13 | 13,464 |
| `room_*`                         |    24 |  9,773 |
| `net_*`/`packets*`               |    29 |  9,738 |
| `ariadne*`                       |    20 |  9,592 |
| `lua_*`                          |    22 |  7,855 |
| `map_*`                          |    14 |  7,463 |
| `spdigger_*`                     |     2 |  3,665 |
| `power_*`                        |     6 |  3,644 |
| `roomspace*`                     |     6 |  2,899 |
| `game_*`                         |    12 |  2,346 |

**Build precedent already exists.** `deps/CMakeLists.txt` already pulls in
`enet`, `zlib`, `spng`, `astronomy`, `centijson`, `CUnit` as independent
`add_subdirectory` targets, and `src/ftests` already has a CUnit-based test
scaffold. The muscle memory for multi-target builds exists; only `src/`
itself is unstructured.

**The three "god headers" are the real blocker.** `globals.h`,
`keeperfx.hpp`, and `game_legacy.h` act as de facto "everything knows
everything" headers. The investigation found this is *not* uniformly true:

- `globals.h` (465 lines) turns out to be a legitimate, appropriately
  low-level shared vocabulary header — coordinate structs (`Coord3d`,
  `Coord2d`, …) and ~55 domain-ID typedefs (`PlayerNumber`, `ThingIndex`,
  `RoomKind`, …) used identically by every future library. It is *not* a
  problem to fix, just a foundation to keep below all six libraries. Its
  one wart: the `*LOG` debug macros silently call `get_gameturn()`
  (defined in `game_legacy.c`), pulling a game-state dependency into 9
  `bflib_*` files that use those macros.
- `keeperfx.hpp` (333 lines) and, especially, **`struct Game` inside
  `game_legacy.h`** (~176 fields, the central mutable-state god-object) are
  real problems. `struct Game` embeds types owned by *every* target library
  directly by value — a `struct GuiMessage messages[]` array (from
  `gui_msgs.h`, frontend), the entire `struct Configs conf` (config), desync
  logging structs (net), lighting/texture state (render) — inside one
  struct that also holds `things_data[THINGS_COUNT]` and `map[...]` (sim).
  This is why, for example, 30 of 31 `game_*`→frontend includes route
  through this one header, and why rendering code (`engine_redraw.c`)
  cannot touch `game.map_subtiles_x` without also seeing the GUI and script
  VM state.

Because `struct Game` is reached from inside candidate `kfx_sim`,
`kfx_render`, `kfx_net`, `kfx_game`, and `kfx_frontend` code simultaneously,
decomposing it **cannot be deferred to a final cleanup stage** — see §6 and
the revision note at the bottom of this document.

## 4. Target architecture

One-directional dependency graph (arrows = "depends on"):

```
kfx_frontend  ──┐
kfx_net       ──┼──▶ kfx_game ──▶ kfx_render ──┐
kfx_script    ──┘                              ├──▶ kfx_sim ──▶ kfx_config ──▶ kfx_platform
                                                └──▶ kfx_pathfinding ──▶ kfx_config ──▶ kfx_platform
                                                     (resolved: standalone — see stage 6 appendix)
```

Target CMake layout under `libs/`:

| target            | owns (post-classification, see per-stage docs for full file lists) | depends on |
|--------------------|------------------------------------------------------|------------|
| `kfx_platform`     | `bflib_*`, `globals.h`, `thread.hpp`, `mutex.hpp`, `kfx_memory.*`, `custom_zip.*`, `cdrom.*`, `steam_api.*`, `windows.cpp`, `linux.cpp`, `moonphase.*`, `sound_manager.*`, `platform.h`, `compiler_compat.h`, `version.h` | SDL2, enet, zlib (external only) |
| `kfx_config`       | `config_*`, `lvl_filesdk1.*`, `value_util.*`, `highscores.*` | `kfx_platform` |
| `kfx_sim`          | `map_*`, `slab_data.*`, `thing_*`, `room_*`, `roomspace*`, `creature_*`, `player_*`, `dungeon_*`, `dungeon_stats.*`, `power_*`, `spdigger_*`, `magic_powers.*`, `actionpt.*`, `tasks_list.*` | `kfx_config`, `kfx_platform` |
| `kfx_pathfinding`  | `ariadne*` — resolved as its own library, not bundled (see stage 6 appendix) | `kfx_config`, `kfx_platform` |
| `kfx_render`       | `engine_*`, `lens_api.*`, `light_data.*`, `vidmode*`, `vidfade.*`, `spritesheet.cpp`, `custom_sprites.*`, `sprites.h`, `scrcapt.*`, `cursor_tag.*` (borderline sim/render — flagged) | `kfx_sim`, `kfx_platform` |
| `kfx_net`          | `net_*`, `packets*` | `kfx_sim`, `kfx_config`, `kfx_platform` |
| `kfx_game`         | `game_*`, `lvl_script*`, `main_game.c`, `console_cmd.*`, `sounds.*` | `kfx_sim`, `kfx_render`, `kfx_net`, `kfx_config` |
| `kfx_frontend`     | `front_*`, `frontmenu_*`, `gui_*`, `button_snapping.*`, `kjm_input.*`, `local_camera.*` | `kfx_game`, `kfx_render`, `kfx_config`, `kfx_platform` |
| `kfx_script`       | `lua_*`, `api.*` | wide (deliberately — see stage 11) |
| `keeperfx` (app)   | `main.cpp` (shrinks as `main_game.c`/game-loop logic moves into `kfx_game`, per stage 12) | everything above |

This reclassifies the 72 previously-"uncategorized" files by actual
`#include` content rather than guesswork; the per-file rationale is in the
relevant stage document (mostly stages 4–11).

## 5. Guiding principles for every stage

1. **Never break the build.** Every stage ends with `keeperfx` and
   `keeperfx_hvlog` compiling, linking, and `src/ftests` passing.
2. **Logical split before physical split.** Prove a library's dependencies
   acyclic with a CMake `OBJECT` library *in place* (no file moves) before
   moving files into `libs/`.
3. **Cut edges by adding interfaces, not by deleting functionality.** Where
   a lower layer calls into a higher one, introduce a callback/function
   pointer, an accessor function, or an event the lower layer owns and the
   higher layer registers into — never just `#include` less and hope.
4. **One direction only**, enforced by CI (stage 13), not just convention.
5. **Delete dead includes first — they're free.** The audit found a
   meaningful fraction of "coupling" is simply unused `#include` lines
   (headers included, zero symbols from them referenced). Each stage lists
   these explicitly; removing them costs nothing and shrinks the real
   problem before any interface work starts.
6. **Land incrementally.** Each stage is independently reviewable, not one
   20k-line PR. Large stages (kfx_sim, kfx_frontend) are explicitly
   sub-divided by prefix cluster in their own documents.

## 6. Stage index

| # | Document | Goal | Size |
|---|----------|------|------|
| 0 | [stage-00-safety-net.md](stage-00-safety-net.md) | CI safety net + dependency-graph script | S |
| 1 | [stage-01-object-libraries.md](stage-01-object-libraries.md) | In-place CMake `OBJECT` library boundaries, no file moves | S |
| 2 | [stage-02-decouple-bflib.md](stage-02-decouple-bflib.md) | Cut `bflib_*`'s upward includes | M |
| 3 | [stage-03-extract-kfx-platform.md](stage-03-extract-kfx-platform.md) | Physically move `kfx_platform` into `libs/` | S |
| 4 | [stage-04-kfx-config.md](stage-04-kfx-config.md) | Extract `kfx_config`; fix its reverse-dependency on `game_legacy.h` | M |
| 5 | [stage-05-god-headers.md](stage-05-god-headers.md) | Decompose `struct Game`/`keeperfx.hpp` field-by-field into per-library ownership — **prerequisite for stages 6–10, not a final step** | L |
| 6 | [stage-06-kfx-sim.md](stage-06-kfx-sim.md) (+ [appendix: ariadne interface](stage-06a-ariadne-pathfinding-interface.md), [appendix: sim→frontend leaks](stage-06b-sim-frontend-leaks.md), [appendix: roomspace_prediction.c tangle](stage-06c-roomspace-prediction-tangle.md)) | Extract the simulation core | XL |
| 7 | [stage-07-kfx-render.md](stage-07-kfx-render.md) | Extract rendering; fix the `PlayerInfo`-embeds-`Camera` leak | M |
| 8 | [stage-08-kfx-net.md](stage-08-kfx-net.md) | Extract networking; fix the `bflib_enet.cpp` ⇄ net cycle | M |
| 9 | [stage-09-kfx-game.md](stage-09-kfx-game.md) | Extract game-loop orchestration | S |
| 10 | [stage-10-kfx-frontend.md](stage-10-kfx-frontend.md) | Extract the UI (largest file count) | L |
| 11 | [stage-11-kfx-script.md](stage-11-kfx-script.md) | Extract Lua bindings; define the scripting API surface | M |
| 12 | [stage-12-slim-app-target.md](stage-12-slim-app-target.md) | Reduce `src/` to just `main.cpp`; move game-loop code out of it | M |
| 13 | [stage-13-enforce-and-document.md](stage-13-enforce-and-document.md) | Make the dependency-graph check mandatory in CI; update docs | S |

Stages 0–4 are worth committing to now; low risk, establish the tooling.
Stage 5 (god-header decomposition) is the pivot the rest of the plan hinges
on and deserves a dedicated design review before starting. Stages 6+
onward should be revisited one at a time — each is a real scoping
conversation, not just execution of a document.

## 7. Open decisions (need a maintainer call before stage 5+)

1. **~~Pathfinding (`ariadne*`) as its own library, or bundled in
   `kfx_sim`?~~ Resolved: standalone `kfx_pathfinding` library.** Stage 6
   originally landed it bundled; it has since been extracted. See
   [stage-06a-ariadne-pathfinding-interface.md](stage-06a-ariadne-pathfinding-interface.md)
   for the full account — counting actual call sites (not just kinds of
   query) found the coupling lopsided: ~40 call sites of map/door/
   CreatureControl-slot access, versus ~450 direct reads *and writes* of
   `struct Thing` fields (creature position/angle) throughout
   `ariadne.c`/`ariadne_wallhug.c`'s collision-resolution code, plus a
   further 16 dependencies the physical move itself surfaced (state-reading
   functions and shared globals `ariadne.c`/`ariadne_update.c` had been
   reaching only transitively through `kfx_sim_state.h`). All of it now goes
   through a `PathfindingWorldCallbacks` interface (51 entries) and is
   build-verified (native Linux + mingw, both `BFDEBUG_LEVEL` variants,
   `check_layering.py --strict` clean, zero new `ACCEPTED_VIOLATIONS`).
   `ariadne*` physically lives in `src/kfx_pathfinding/` now, ranked between
   `kfx_config` and `kfx_sim`. **Still not playtested** — no real game-data
   install was available to exercise the converted movement code in motion,
   only to confirm it compiles and links correctly; recommended before
   merging (see stage-06a §13).
2. **How strict should `kfx_script`'s boundary be?** The Lua layer reaches
   into 23 distinct `kfx_sim` headers and 13 `kfx_config` headers today
   (full breakdown in stage 11) — narrowing that to a stable public binding
   surface is a large, separate effort. Recommendation: pragmatic pass only
   in stage 11; revisit if the scripting API needs versioning for mod
   compatibility.
3. **`struct Game` decomposition strategy** — composed top-level object
   referencing per-library owned sub-structs by pointer, vs. fully
   distributing fields with cross-references resolved through accessor
   functions. **Decided 2026-08-02: incremental field migration**, one
   field-group at a time, timed to match stages 6–10 as each lands — see
   [stage-05-god-headers.md](stage-05-god-headers.md#decomposition-strategy--decided).
4. **Two executables (`keeperfx`, `keeperfx_hvlog`) differ only in
   `BFDEBUG_LEVEL`.** Confirm this is a compile-time define read only at
   the app layer, not inside any library — if any library reads it, that
   library needs two build variants, which affects stage 1's `OBJECT`
   library setup.

## 8. Correction found while implementing stage 0

The original plan assumed one build definition (`CMakeLists.txt`'s
`file(GLOB_RECURSE src/*.c src/*.cpp)`) that would auto-follow any file
move. **There are actually three independent build definitions**,
discovered while setting up local build verification:

| Build file | Target | How it lists sources |
|---|---|---|
| `CMakeLists.txt` | Windows (MSVC/clang-cl), via vcpkg | `file(GLOB_RECURSE ...)` — auto-follows file moves |
| `Makefile` | Windows, via mingw-w64 cross-compile (what CI's `build-prototype.yml` and the release workflows actually use) | Hand-maintained `OBJS = \` flat list |
| `linux.mk` | Native Linux, via `scripts/setup-linux-thirdparty.sh` (git-ignored `third_party/` staging, no sudo) | Hand-maintained `KFX_SOURCES = \` flat list |

Only `CMakeLists.txt` auto-follows a `git mv`. **Every physical file move
from stage 3 onward needs a matching edit to `Makefile`'s `OBJS` list and
`linux.mk`'s `KFX_SOURCES` list**, or those two build paths silently stop
compiling the moved file. This wasn't visible from static analysis alone —
worth calling out because it changes the mechanical cost of every move
stage (3, 6–11), not just the design work.

`linux.mk` was adopted as the build used to verify stage 0's work locally,
since it builds and links successfully without the mingw-w64 toolchain
(see [stage-00-safety-net.md](stage-00-safety-net.md) for the working
recipe and measured baseline). It's also missing header-dependency
tracking (`-MMD`/depfiles) — noted in that document since it affects how
meaningful future incremental-build measurements are, independent of this
plan.

## 9. Revision history

- **Initial draft**: single-document plan based on a coarse `#include`
  grep across naming-convention clusters, with `struct Game` decomposition
  folded into a final "slim the app target" stage.
- **This revision**: split into one document per stage, after five
  parallel deep-dive investigations (bflib coupling, config/uncategorized
  files/ariadne, render/net boundaries, frontend/game structure, and a full
  read of all three "god headers"). The most significant structural change:
  **god-header decomposition was promoted from the last stage to stage 5**,
  because the investigation showed `struct Game` is reached from inside
  the sim, render, net, game, and frontend clusters simultaneously — it
  blocks all of stages 6–10, not just final cleanup. `globals.h` was
  cleared as a non-issue (legitimate low-level header). The 72
  previously-uncategorized files were individually classified by actual
  include/content inspection rather than left as an "other" bucket.
