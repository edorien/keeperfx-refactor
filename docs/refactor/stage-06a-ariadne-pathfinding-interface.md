# Stage 6 appendix — Ariadne pathfinding: bundle vs. standalone library

See [stage-06-kfx-sim.md](stage-06-kfx-sim.md). Stage 6 originally landed
with `ariadne*` bundled inside `kfx_sim` (10 `.c` + 10 `.h` files, ~8,850
LOC in the `.c` files alone). This appendix records the original
bundle-vs-standalone investigation (§1–3, updated), a first-pass extraction
design (§4–5), and — after actually counting call sites rather than call
*kinds* — a much more realistic picture of the dominant cost (§6). **All
three tracks (§10) and the physical library split (§11) are now done** —
`ariadne*` (9 `.c` + 9 `.h` files) plus the new `kfx_pathfinding_state.h/.c`
physically live under `src/kfx_pathfinding/{src,include}/`, as their own
CMake OBJECT library and `Makefile` source directory, ranked between
`kfx_config` and `kfx_sim` in `scripts/check_layering.py`'s
`LIBRARY_ORDER`. See §14 for the full as-built account, including a
significant transitive-include dependency the move itself surfaced (§14
"Physical split specifics").

## 0. Implementation status

- **Track 1 (§4/§9, `navigation_map` ownership cleanup): done.**
  `ariadne_reset_navigation_map()`/`ariadne_set_navigation_map_size()`/
  `ariadne_is_map_dirty_for_navigation()`/
  `ariadne_clear_map_dirty_for_navigation()`/
  `ariadne_mark_map_dirty_for_navigation()` added to `ariadne_update.h`/`.c`;
  all five external touch points (`map_data.c` ×2, `thing_list.c`,
  `room_data.c`, `kfx_apploop/game_session_loop.cpp`) converted.
- **Track 2 (§8.2/§10, the small `PathfindingWorldCallbacks` interface):
  done**, with two adjustments made during implementation (see §14 for the
  full account): the door-owner/lock fields (`doortng->owner`/
  `doortng->door.is_locked`) turned out to be cheap enough to fold in here
  rather than leave for Track 3, and a `get_map_size_x/y` pair plus a
  `struct SlabMap`/`struct Map` opaque-pointer accessor design (not quite
  what §8.2's first draft sketched) were needed once actual call sites were
  converted. `pathfinding_world.h`/`.c` added to `kfx_config`; `main.cpp`
  wires the implementation; all ~40 real call sites in `ariadne.c`/
  `ariadne_wallhug.c`/`ariadne_update.c` converted.
- **Track 3 (§6, the `struct Thing` position/angle field-access conversion):
  done.** Added four more `PathfindingWorldCallbacks` entries
  (`thing_get_position`/`thing_set_position`/`thing_get_move_angle`/
  `thing_set_move_angle`, plus `thing_get_index`/`thing_get_clipbox_size` —
  29 → 35 total entries) using the value get/set design from §6.3 Option 1,
  and converted all ~450 real touch points across `ariadne.c` (24 functions)
  and `ariadne_wallhug.c` (17 functions), file-by-file with a compile check
  after each, exactly as §6.3 recommended. The trickiest pieces were the
  functions that temporarily reposition a creature in-place to trial a
  collision check then restore it
  (`creature_cannot_move_directly_to_with_collide`,
  `check_forward_for_prospective_hugs`,
  `get_starting_angle_and_side_of_hug_sub2`) — converted using a local
  `struct Coord3d`/`short` kept synced with the live thing via an immediate
  `thing_set_position`/`thing_set_move_angle` call after every write,
  since sibling functions in the same call chain read the *live* thing
  state back out through the same accessors. One real finding along the
  way: a genuinely missed kind-A function, `subtile_is_door` (map_data.c),
  turned up during the conversion and was added to the callback table.
  Zero remaining direct `struct Thing` field access in any of the three
  ariadne files — verified by grep, not just by the type of edits made.
- **Physical library split (§11): done.** `ariadne*` (9 `.c` + 9 `.h`) moved
  via `git mv` into `src/kfx_pathfinding/{src,include}/`, alongside a new
  `struct KfxPathfindingState` (§9 — see the updated decision there, which
  landed differently than either option this section originally sketched).
  New `src/kfx_pathfinding/CMakeLists.txt` (mirrors `kfx_config`'s shape),
  wired into the top-level `CMakeLists.txt` (include dir +
  `add_subdirectory`), the `Makefile` (`INCFLAGS` + pattern rules +
  `kfx_pathfinding_state.o` added to `OBJS`), and
  `scripts/check_layering.py`'s `LIBRARY_ORDER` (inserted between
  `kfx_config` and `kfx_sim`). `check_layering.py --strict` passes with zero
  new `ACCEPTED_VIOLATIONS` entries. The move surfaced a real, previously
  invisible dependency surface — `ariadne.c`/`ariadne_update.c` had been
  reaching a large umbrella of kfx_sim headers (macros, enums, two shared
  globals, several functions) *transitively* through `kfx_sim_state.h`,
  which the Track 1 state-struct migration had already swapped out; this
  only surfaced once the file physically moved and lost the rest of that
  umbrella too. Full account in §14.
- **Build: verified on both targets.** `./build-cmake.sh` (default mingw-w64
  i686 cross-compile — the one CI and release workflows actually use) built
  `keeperfx`/`keeperfx_hvlog` clean, zero warnings, on every touched file.
  `KFX_OS=linux ./build-cmake.sh` (native ELF) initially hard-failed at
  configure time — `Dependencies.cmake`'s non-Windows branch turned out to
  only have a real "system pkg-config, else build from source" fallback for
  SDL3; `openal`/`luajit`/`spng`/`minizip` were hard `REQUIRED` pkg-config
  lookups with no fallback, and `miniupnpc`/`natpmp` had no lookup at all
  (bare linker names, silently assuming the system already had them) —
  despite the stage-13-era commit that deleted `linux.mk` claiming this file
  "already covers native Linux builds itself." **Fixed**: all six now have
  a FetchContent-from-source fallback matching SDL3's pattern (OpenAL-soft
  and libspng have normal CMake builds; miniupnpc needed a `SOURCE_SUBDIR`
  + an include-path shim since `<miniupnpc/miniupnpc.h>` doesn't match its
  generated flat include dir; libnatpmp has its own small CMakeLists.txt
  now, pinned to a commit since it has no tagged releases; minizip is
  compiled directly from zlib's `contrib/minizip` sources since it has no
  build system of its own; LuaJIT has no CMake at all, so it's built via
  `ExternalProject_Add` shelling out to its own Makefile). One real source
  fix fell out of this: `net_portforward.cpp`'s `#define
  MINIUPNP_STATICLIB`/`NATPMP_STATICLIB` collided with the same macros
  arriving as a `PUBLIC` compile definition from the fetched libraries —
  guarded both with `#ifndef`. With that, `KFX_OS=linux ./build-cmake.sh`
  now builds both targets clean from an empty `out/`, fully statically
  linked against every previously-missing dependency (confirmed via `ldd`),
  and the resulting binary **runs**: initializes SDL/audio/CPU detection
  correctly and fails only at `keeperfx.cfg`/game-data loading — the
  expected wall, since the original game's data files aren't in this repo
  per CLAUDE.md, not a build defect. This is a separate, standalone fix
  (`CMakeLists.txt`, `build/cmake/modules/Dependencies.cmake`,
  `net_portforward.cpp`) — real value on its own, independent of the
  ariadne work, but it's what makes the rest of this bullet possible.
  Compiling and starting up cleanly is still not the same as route-testing:
  no `ftests`, no in-game wall-hug/door/save-load/resync scenarios have
  been run — those need the FUNCTESTING build define (currently wired into
  the Makefile only, not CMake) *and* real game-data, neither available
  here.

## 1. Background

`ariadne*` (A*, wall-hugging, navmesh triangulation) is algorithmically
self-contained but currently reads live `creature_control`/`thing_*`/`map_*`
state directly. A full trace of every struct field access and function call
it makes into the outside world found the coupling concentrated in four
kinds of query, one genuinely circular ownership issue, one piece of
ariadne's own state stranded in the wrong struct — and (found only once
call *sites*, not just call *kinds*, were counted — see §6) one very large
pocket of direct-field-access coupling that dwarfs everything else combined.

Most of the small files (`ariadne_edge/findcache/naviheap/navitree/points/
regions/tringls.c`) are self-contained navmesh internals (triangles, edges, a
binary heap, a search tree, region flood-fill). Real coupling is concentrated
in three files: `ariadne.c` (3,285 lines), `ariadne_wallhug.c` (2,145 lines),
`ariadne_update.c` (1,738 lines). `thing_stats.h`, `thing_data.h`,
`config_creature.h`, `config_terrain.h` were included but entirely unused —
those includes are gone now (checked: none of the three files include them
any more).

## 2. What's changed since this doc was first written

Two of the four coupling kinds identified originally have already been
resolved as side effects of other stage-6 work, independent of whether
ariadne ever gets its own library:

- **Diagnostic telemetry (was kind D) is already solved.** `ariadne_naviheap.c`,
  `ariadne_navitree.c`, `ariadne_points.c`, `ariadne_tringls.c`, and `ariadne.c`
  itself now call `sim_feedback->report_error_stat(ESE_*)` — the
  `SimFeedbackCallbacks` struct from `kfx_config/include/sim_feedback.h` that
  stage 6b introduced for exactly this kind of "kfx_sim needs to poke
  `gui_topmsg.h`" problem. This means: **if `kfx_pathfinding` ends up ranked
  below `kfx_sim` but above `kfx_config`** (see §7), these five files need
  *zero* changes for telemetry — they already only reach down into
  `kfx_config`, which would still be below them. Nothing to design here.

- **The hypothesized `engine_camera.h` coupling never existed.** The
  original investigation flagged `get_angle_xy_to` as a possible
  camera/zoom-state leak. It isn't: `get_angle_xy_to` is a pure-geometry
  function declared in `kfx_platform/include/bflib_math.h` and none of the
  three ariadne files with real coupling include `engine_camera.h` at all
  any more. `bflib_math.h` is the bottom of the entire ladder — this
  function is free to call from anywhere, including a future
  `kfx_pathfinding`. One less thing to build an interface for.

What's left, unchanged in kind but re-verified against current line numbers,
is kinds A/B/C below, a new finding about `navigation_map`'s storage
location (§4), and — the real substance of this update — kind C turns out to
be an order of magnitude bigger than "a per-creature slot plus a few
accessors" once you count call sites (§6).

## 3. What ariadne actually needs, grouped by kind

**A. Is this map cell solid / what kind of cell is it**
(`ariadne_update.c`'s `get_navigation_colour()`/`init_navigation_map()`, the
single most important entry point): per-subtile blocking/door flags
(`get_map_block_at`, `src/kfx_sim/src/map_data.c`), floor height
(`get_floor_filled_subtiles_at`, `map_columns.c`), hazard flag
(`subtile_is_unsafe`, `map_columns.c`), slab kind/owner
(`get_slabmap_block`/`slabmap_owner`, `slab_data.c`), the existing
well-shaped `is_valid_hug_subtile()` (`slab_data.c`), `thing_in_wall_at()`
(`thing_physics.c`), and the map extents (`kfx_sim_state.map_subtiles_x/y`).
This kind is genuinely small: ~8 call sites total (verified by count, not
just distinct functions), all bit-flag predicates or single-value lookups.

**B. Who owns/locks/hides this door** (`get_navigation_colour_for_door()`
and door checks in `ariadne_wallhug.c`): `get_door_for_position()`
(`thing_list.c`, 2 sites), `doortng->owner`, `doortng->door.is_locked`,
`door_is_hidden_to_player()`/`door_will_open_for_thing()` (`thing_doors.c`,
1 site each), `players_are_mutual_allies()` (`player_data.c`, 1 site). Also
genuinely small.

**C. Creature/thing position, speed, model state, and the per-creature
pathfinding "slot"** — split into two very differently-sized pieces once
actually counted:

- The per-creature pathfinding slot (`cctrl->navi`/`cctrl->arid`) and a
  handful of scalar reads (`max_speed`) plus two flag writes: **small,
  contained**, see §5.
- Direct field access on `struct Thing` itself (`mappos`, `move_angle_xy`,
  `index`, `owner`, `clipbox_size_xy`) plus a handful of `kfx_sim` function
  calls taking a `struct Thing *` (`get_thing_height_at` and friends):
  **~490 call sites across the two largest files, the actual bulk of this
  extraction's cost.** See §6 — this is the finding that changes the
  recommendation.

**D. Diagnostic telemetry** — resolved, see §2.

## 4. New finding: ariadne's own state is stranded in `kfx_sim_state`

Not covered by the original investigation (it predates stage 6's field
migration): `ariadne_update.c` owns and mutates three fields that live
inside `struct KfxSimState` (`src/kfx_sim/include/kfx_sim_state.h`):

```c
int32_t navigation_map_size_x;
int32_t navigation_map_size_y;
NavColour navigation_map[MAX_SUBTILES_X*MAX_SUBTILES_Y];
```

plus a fourth, `map_changed_for_navigation` (a dirty flag). `NavColour`
itself is already bottom-layer (`typedef uint16_t NavColour;` in
`kfx_platform/include/globals.h`), so the type isn't the problem — the
*array and its bookkeeping* are ariadne-private data that happen to be
homed in the sim's blob, almost certainly because stage 6's "migrate
`struct Game`'s sim field group into `kfx_sim_state`" pass moved it there
along with everything else map-shaped, without noticing it's actually
pathfinding cache, not simulation state.

Four touch points reach into these fields *from outside every ariadne
file* (a fifth call site — `map_data.c::set_map_size()` also writes
`navigation_map_size_x/y` — lives in the same file as the first), all of
which would need to become calls into ariadne's public API instead of
direct field pokes once the fields move to a new library's own state
struct:

1. `map_data.c::clear_mapmap()` — zeroed `navigation_map[i]` alongside
   zeroing each `struct Map` block.
2. `map_data.c::set_map_size()` — sets `navigation_map_size_x/y` at
   map-size-known time.
3. `thing_list.c::update_things()` — clears `map_changed_for_navigation = 0`
   once per turn, after all thing lists have been updated (i.e. "no
   navigation-relevant change happened since the flag was last set — the
   map is stable this frame").
4. `room_data.c::place_room()` — sets `map_changed_for_navigation = 1`
   unconditionally on room placement (a new room can open or close
   passages, so any cached path may now be stale).
5. `kfx_apploop/src/game_session_loop.cpp::update()` — clears
   `map_changed_for_navigation = 0` on the early-out path for
   `GKind_NonInteractiveState` (a second, narrower echo of #3).

**Status: done.** These were converted to a small ariadne-owned API
(`ariadne_reset_navigation_map()`, `ariadne_set_navigation_map_size(x, y)`,
`ariadne_is_map_dirty_for_navigation()`/`ariadne_clear_map_dirty_for_navigation()`/
`ariadne_mark_map_dirty_for_navigation()`, declared in `ariadne_update.h`,
implemented in `ariadne_update.c`) — one-line call-site swaps, no behavior
change. This piece was genuinely cheap and self-contained, exactly as
predicted; see §10, Track 1.

## 5. The CreatureControl-embedded slot — small, contained

`creature_control_get_from_thing(thing)` is called 8 times (4 in `ariadne.c`,
4 in `ariadne_wallhug.c`) just to reach two fields: `cctrl->navi`
(`struct Navigation`, defined in `ariadne_wallhug.h`) and `cctrl->arid`
(`struct Ariadne`, defined in `ariadne.h`), plus 2 reads of `cctrl->max_speed`
and 2 writes (`cctrl->creature_state_flags = 0;`/`cctrl->combat_flags = 0;`,
both at the same call site in `ariadne_wallhug.c`, ~line 1633, when a
wall-hug maneuver takes over from normal creature-state processing). Total:
13 touch points. This matches the original investigation's estimate closely
— it was right about this piece.

Both `struct Navigation` and `struct Ariadne` are self-contained — verified:
they only reference `kfx_platform`-level types (`Coord3d`, `Coord2d`,
`PlayerBitFlags`, `SubtlCodedCoords`, primitives) plus forward-declared
`struct Thing`/`struct SlabMap` used only in function signatures, never
embedded. So the types themselves are perfectly safe to move to a lower
library.

The actual problem is the *access path*, not the types: both structs are
embedded **by value** inside `struct CreatureControl`
(`kfx_sim/include/creature_control.h`, lines ~359–361), which is fine in the
direction kfx_sim → ariadne (a higher library embedding a lower library's
type by value just needs the definition — a normal downward include). The
circularity is that `ariadne.c`/`ariadne_wallhug.c` currently reach the
struct instances via `creature_control_get_from_thing(thing)->navi`/`->arid`
— i.e. ariadne calls into kfx_sim and includes `creature_control.h` to get
back its *own* data. If `kfx_pathfinding` ranks below `kfx_sim` (§7), that
`#include` becomes an upward violation.

Fix: 3 opaque-handle accessor callbacks (§8.2) — `creature_get_navigation`,
`creature_get_ariadne_state`, `creature_clear_state_flags_for_wallhug_override`
— covering all 13 touch points between them. Genuinely small, matches the
scale the original doc implied.

## 6. The real cost: `struct Thing` field access throughout the two big files

This is what a "grouped by kind of query" pass misses and a call-site count
catches: kind C isn't dominated by the CreatureControl slot (§5) — it's
dominated by `ariadne.c`/`ariadne_wallhug.c` treating `struct Thing *` as a
fully transparent struct and reading *and writing* its fields directly,
throughout the hottest, most delicate movement/collision code in the
simulation.

### 6.1 The numbers

| Field / call | Occurrences | File split | Notes |
|---|---|---|---|
| `mappos` (read+write) | 388 | wallhug: majority, ariadne.c: rest | 45 whole-struct writes, 28 partial-field writes (`.z.val = ...` etc.), rest reads |
| `move_angle_xy` (read+write) | 50 | mostly wallhug | 15 writes, 35 reads |
| `index`, `owner`, `clipbox_size_xy` | ~13 | split, mostly debug-log args | cheap in isolation |
| `get_thing_height_at(...)` | 56 | both files | one function, high call frequency |
| `get_floor_height_under_thing_at(...)` | 4 | wallhug | |
| `thing_is_invalid(...)` | 2 | | |
| `creature_can_travel_over_lava(...)` | 2 | | |
| `thing_model_name(...)` | 8 | debug-log args only | lowest-stakes of the list |

Total: **~522 cross-boundary touch points**, landing on **433 distinct
source lines** (272 in `ariadne_wallhug.c`, 161 in `ariadne.c`,
**1** in `ariadne_update.c` — that file is essentially clean; all the
weight is in the other two). For scale: the entire rest of kinds A+B+§5
combined is ~29 touch points. This one item is ~18x that.

Critically, 73 of the 388 `mappos` touches (45 whole-struct + 28
partial-field) are **writes** — i.e. `ariadne_wallhug.c` is directly
repositioning creatures in-place (`creatng->mappos = next_pos;`,
`creatng->mappos.z.val = get_thing_height_at(...)`) as part of resolving a
wall-hug collision. This is not incidental state-reading; it's ariadne
performing part of the actual movement simulation by mutating `kfx_sim`'s
core entity struct directly.

### 6.2 Why this is a materially different problem than §5/kinds A/B

Kinds A/B/§5 are "read a value, occasionally write a flag" — cheap
per-callback, and there are few enough call sites that hand-converting them
is an afternoon's careful work per file. This is "hundreds of reads and
writes, many of them multi-statement position math, scattered through the
two files with the most subtle, hardest-to-regression-test logic in
`kfx_sim`" (wall-hug collision resolution and A*/waypoint routing). Getting
every conversion exactly right — not silently dropping a write, not
reading a stale copy across a sequence of partial-field updates — needs
line-by-line review, not a mechanical find-and-replace, and a bug here
manifests as creatures clipping through walls, getting stuck, or
teleporting — regressions that are easy to miss in a quick smoke test and
only show up under sustained play.

### 6.3 Two viable designs for the position/angle accessors, if this is attempted

**Option 1 — value get/set pairs** (matches this codebase's dominant
callback idiom — see `SimFeedbackCallbacks`, almost entirely scalar/by-value
entries):

```c
struct Coord3d (*thing_get_position)(const struct Thing *thing);
void           (*thing_set_position)(struct Thing *thing, const struct Coord3d *pos);
long           (*thing_get_move_angle)(const struct Thing *thing);
void           (*thing_set_move_angle)(struct Thing *thing, long angle);
```

Every one of the 433 lines needs individual rewriting: a whole-struct write
(`creatng->mappos = next_pos;`) becomes a single call
(`pathfinding_world->thing_set_position(creatng, &next_pos);`), but a
partial-field write (`creatng->mappos.z.val = h;`) becomes a three-line
read-modify-write (`struct Coord3d p = pathfinding_world->thing_get_position(creatng);
p.z.val = h; pathfinding_world->thing_set_position(creatng, &p);`). Safer
(no live pointer into `kfx_sim`'s internals escapes the boundary), more
mechanical churn.

**Option 2 — pointer accessor** (precedented once elsewhere in this
codebase: `SimFeedbackCallbacks::get_local_camera` returns a live
`struct Camera *`):

```c
struct Coord3d *(*thing_mappos_ptr)(struct Thing *thing);
long            *(*thing_move_angle_ptr)(struct Thing *thing);
```

Every occurrence becomes a uniform textual substitution (`creatng->mappos`
→ `(*pathfinding_world->thing_mappos_ptr(creatng))`), whether it's a read,
whole-struct write, or partial-field write — a scriptable, much
lower-effort conversion. Trade-off: it hands `kfx_pathfinding` a raw
mutable pointer into a `kfx_sim`-owned struct, which is a weaker boundary
than every other callback in this codebase provides (every existing
accessor returns by value or takes a narrow setter — none hand back a
pointer to let the caller poke arbitrary fields, `get_local_camera`
included, which returns a pointer *to a struct the caller already owns and
passed in*, not a pointer into render's own internal state).

**Recommendation if this is attempted: Option 1.** It's more work, but it's
the only one of the two that actually enforces an interface rather than
papering over the lack of one — and given how failure here manifests
(subtle movement bugs), the extra rigor of forcing every write through a
real setter is worth the churn. Do it file-by-file, `ariadne_update.c`
first (it's nearly untouched by this — 1 line), then `ariadne.c`, then
`ariadne_wallhug.c` last (the largest and most collision-logic-dense).

## 7. Where this library sits in the dependency ladder

`kfx_pathfinding` slots in immediately above `kfx_config`, below `kfx_sim`:

```
kfx_platform → kfx_config → kfx_pathfinding → kfx_sim → kfx_render → ...
```

Justification: every kind-A/B/C world query it needs (§3) is owned by
`kfx_sim`, so it must rank below `kfx_sim`. It needs nothing from
`kfx_render`/`kfx_net`/`kfx_game`/etc. It does want `kfx_config`'s
`sim_feedback.h` (telemetry, already free per §2) and `kfx_config`'s
`globals.h`-level types — both already satisfied by ranking above
`kfx_config`. No evidence it needs to rank any higher than immediately
above `kfx_config`.

This also means `kfx_sim` can freely `#include` `kfx_pathfinding`'s headers
(downward, always allowed) — which is exactly what's needed for
`creature_control.h` to embed `struct Navigation navi;`/`struct Ariadne
arid;` by value, and for `thing_navigate.c`/`room_util.c`/`thing_doors.c`/
`slab_data.c` to keep calling ariadne's public entry points unchanged.

## 8. Interfaces

### 8.1 Forward interface (kfx_sim/kfx_config → kfx_pathfinding) — no new design needed

This direction is already a clean header surface today, because it's
downward-shaped even in the current bundled state. It just needs to become
`kfx_pathfinding`'s public API instead of kfx_sim-internal headers. No
behavior change; the exact existing entry points move as-is:

- `ariadne.h`: `ariadne_prepare_creature_route_to_target`,
  `ariadne_initialise_creature_route_f`,
  `ariadne_count_waypoints_on_creature_route_to_target_f`,
  `ariadne_invalidate_creature_route`, `navigation_points_connected`.
- `ariadne_update.h`: `update_navigation_triangulation`, `init_navigation`
  (called from `room_util.c`, `thing_doors.c` (5 sites), `slab_data.c`,
  `main_game.c`).
- `ariadne_wallhug.h`: `creature_follow_route_to_using_gates`,
  `get_next_position_and_angle_required_to_tunnel_creature_to`,
  `initialise_wallhugging_path_from_to`, `slab_wall_hug_route`,
  `thing_nav_sizexy`, `thing_nav_block_sizexy`, `set_nav_rule_default`,
  `get_hug_side_options`, `dig_to_position`,
  `slab_good_for_computer_dig_path`, `path_init8_wide_f`, `nearest_search_f`,
  `pointed_at8`, `angle_to_quadrant`.
- New, small: the three navigation-map accessors from §4
  (`ariadne_reset_navigation_map`, `ariadne_set_navigation_map_size`,
  `ariadne_is_map_dirty_for_navigation`/`ariadne_clear_map_dirty_flag`),
  replacing `map_data.c`/`thing_list.c`/`room_data.c`'s direct
  `kfx_sim_state.navigation_map*`/`map_changed_for_navigation` field access.

### 8.2 Backward interface (kfx_pathfinding → kfx_sim) — new `PathfindingWorldCallbacks`

Declared in `kfx_config/include/pathfinding_world.h` (new file, following the
existing `sim_feedback.h`/`game_callbacks.h` pattern: forward-declared opaque
types only, implemented in `kfx_sim`, wired once in `main.cpp::setup_game()`
via `set_pathfinding_world_callbacks(&pathfinding_world_impl)`).

```c
struct Thing;
struct Coord3d;
struct SlabMap;
struct Navigation;   /* defined in kfx_pathfinding's ariadne_wallhug.h -- opaque here */
struct Ariadne;      /* defined in kfx_pathfinding's ariadne.h -- opaque here */

struct PathfindingWorldCallbacks {
    /* kind A -- map/terrain, backing get_navigation_colour()/init_navigation_map() */
    TbBool       (*get_map_block_flags)(MapSubtlCoord stl_x, MapSubtlCoord stl_y, struct Map **out_mapblk);
    long         (*get_floor_filled_subtiles_at)(MapSubtlCoord stl_x, MapSubtlCoord stl_y);
    TbBool       (*subtile_is_unsafe)(MapSubtlCoord stl_x, MapSubtlCoord stl_y);
    SlabKind     (*get_slabmap_block_kind)(MapSlabCoord slb_x, MapSlabCoord slb_y);
    PlayerNumber (*slabmap_owner)(MapSlabCoord slb_x, MapSlabCoord slb_y);
    TbBool       (*is_valid_hug_subtile)(MapSubtlCoord stl_x, MapSubtlCoord stl_y, PlayerNumber plyr_idx);
    TbBool       (*thing_in_wall_at)(const struct Thing *thing, const struct Coord3d *pos);

    /* kind B -- doors, backing get_navigation_colour_for_door() */
    struct Thing *(*get_door_for_position)(MapSubtlCoord stl_x, MapSubtlCoord stl_y);
    TbBool        (*door_is_hidden_to_player)(const struct Thing *doortng, PlayerNumber plyr_idx);
    TbBool        (*door_will_open_for_thing)(const struct Thing *doortng, const struct Thing *creatng);
    TbBool        (*players_are_mutual_allies)(PlayerNumber a, PlayerNumber b);

    /* §5 -- CreatureControl-embedded pathfinding slot */
    struct Navigation   *(*creature_get_navigation)(struct Thing *creatng);   /* &cctrl->navi */
    struct Ariadne      *(*creature_get_ariadne_state)(struct Thing *creatng); /* &cctrl->arid */
    short                (*creature_get_max_speed)(const struct Thing *creatng);
    void                 (*creature_clear_state_flags_for_wallhug_override)(struct Thing *creatng);
        /* wraps the two direct writes: cctrl->creature_state_flags = 0; cctrl->combat_flags = 0; */

    /* §6 -- struct Thing field access, the large piece. Shown here as
       Option 1 (value get/set) from §6.3 -- the recommended shape if this
       is attempted. */
    struct Coord3d (*thing_get_position)(const struct Thing *thing);
    void           (*thing_set_position)(struct Thing *thing, const struct Coord3d *pos);
    long           (*thing_get_move_angle)(const struct Thing *thing);
    void           (*thing_set_move_angle)(struct Thing *thing, long angle);
    PlayerNumber   (*thing_get_owner)(const struct Thing *thing);
    ThingIndex     (*thing_get_index)(const struct Thing *thing);
    short          (*thing_get_clipbox_size)(const struct Thing *thing);
    long           (*get_thing_height_at)(const struct Thing *thing, const struct Coord3d *pos);
    long           (*get_floor_height_under_thing_at)(const struct Thing *thing, const struct Coord3d *pos);
    TbBool         (*thing_is_invalid)(const struct Thing *thing);
    TbBool         (*creature_can_travel_over_lava)(const struct Thing *creatng);
    const char    *(*thing_model_name)(const struct Thing *thing); /* debug logging only */
};
void set_pathfinding_world_callbacks(const struct PathfindingWorldCallbacks *callbacks);
extern const struct PathfindingWorldCallbacks *pathfinding_world;
```

Every implementation is a one-line wrapper around an existing kfx_sim
function or field (`creature_control.c`, `map_data.c`, `map_columns.c`,
`slab_data.c`, `thing_physics.c`, `thing_list.c`, `thing_doors.c`,
`thing_data.c`, `thing_navigate.c`, `player_data.c`). **The struct itself is
small and cheap to design** — this table is comparable in size to several
existing `kfx_config` callback structs. The cost of this extraction is
entirely in §6.2's ~433 call-site conversions, not in designing this table.

No callback is needed for diagnostics (§2) or for `get_angle_xy_to` (§2) —
both already resolve downward through `kfx_config`/`kfx_platform` directly.

## 9. State ownership: `navigation_map` and the serialization invariant (done)

Per architecture.md §6.2, `kfx_sim_state` is one of the structs `memcpy`'d
wholesale in three places — network resync (`net_resync.cpp`), save games
(`game_saves.c`), and level reset (`main_game.c::clear_complete_game()`) —
and `navigation_map`/`navigation_map_size_x/y`/`map_changed_for_navigation`
used to ride along inside that blob. This section originally posed the
choice as Option A (keep syncing/saving it, zero behavior change) vs.
Option B (stop syncing it, recompute after load/resync, a behavior change
needing netcode sign-off). **What actually landed is a third option,
found by investigation rather than picked from the original two:**

- **Option C (shipped) — move the fields to `struct KfxPathfindingState`,
  and exclude them from all three blob chains entirely, with no behavior
  change and no new `init_navigation()` call sites needed.** This is safe
  specifically because `reinit_level_after_load()` (`main_game.c`) — called
  unconditionally on **both** the save-load path and `resync_game()`,
  immediately after the raw blob is applied and before anything else can
  read navigation state — itself calls `init_navigation()` every time.
  `navigation_map` was therefore already being fully recomputed on both
  paths regardless of what the blob carried; the four fields were dead
  weight in the save/resync payload, not authoritative state riding along
  with it. This gets Option B's actual benefit (smaller save files, smaller
  resync payload — `MAX_SUBTILES_X*MAX_SUBTILES_Y` `NavColour` entries no
  longer serialized) without Option B's risk (no new call sites, no
  netcode-timing question to resolve, because the existing unconditional
  `init_navigation()` call already covered it on both paths).
- `struct KfxPathfindingState` (`src/kfx_pathfinding/include/
  kfx_pathfinding_state.h`) is still `memset` in
  `clear_complete_game()` alongside `kfx_sim_state`, purely to match every
  sibling state struct's "cleared at complete-game-clear time" invariant —
  `init_navigation()` always rebuilds it before it's read either way, so
  this memset is belt-and-suspenders, not load-bearing.
- The four fields are gone from `kfx_sim_state.h` entirely (not just moved
  and still-referenced) — `kfx_sim_state.h` shrinks by the full
  `MAX_SUBTILES_X*MAX_SUBTILES_Y` `NavColour` array plus the three scalar
  fields.

## 10. Sequencing — three tracks with very different risk profiles

Given §6, this is **not** a single mechanical "step 1/2/3" sequence — the
three pieces have genuinely different sizes and risk, and should be
evaluated (and potentially stopped after) independently rather than as one
committed plan:

**Track 1 — `navigation_map` ownership cleanup (§4 + §9). Done.** Small,
self-contained, zero behavior change (fields still live in `kfx_sim_state`
for now — that's Option A from §9, deliberately not revisited yet — just
accessed through ariadne's own accessor functions instead of poked directly
from `map_data.c`/`thing_list.c`/`room_data.c`/`game_session_loop.cpp`).
Landed without the physical library split, proving the pattern works with
the files still inside `kfx_sim`. **Build verified** (§0) —
`./build-cmake.sh`'s default mingw target compiles and links clean.

**Track 2 — the small interface (kinds A/B + §5). Done.** ~40 call sites
once actually converted (a bit more than the ~29 first estimated — see §0 and
§14), one new callback struct (`PathfindingWorldCallbacks`, 29 entries,
minus the §6 field-access bucket), wired through `main.cpp`. Roughly matched
the original doc's "well-defined interface, afternoon of careful work"
characterization — bigger than the first pass counted, but still a different
order of magnitude from Track 3. Landed with the files still inside
`kfx_sim`, proving the interface shape before anything moves. **Build
verified** (§0) — same clean compile/link as Track 1.

**Track 3 — the `struct Thing` field-access conversion (§6). Done.** ~450
touch points converted across `ariadne.c` and `ariadne_wallhug.c`
(`ariadne_update.c` needed none — confirmed nearly untouched by this, as
predicted), file-by-file with a compile check after each, using the value
get/set design from §6.3 (Option 1) rather than the live-pointer
alternative. **Build verified** (§0) on all four variants (native Linux +
mingw, each in both `BFDEBUG_LEVEL=0`/`10`) from clean rebuilds, plus
`check_layering.py --strict`. **Not yet playtested** — the honest gap this
doc originally flagged stands: a compile check catches syntax/type errors,
not a mis-converted partial-field write or a stale read, which would show
up as creatures clipping through walls or getting stuck, not as a build
failure. That needs a real game-data install and in-game exercise of
wall-hugging/A* routing, neither available in the environment this was
implemented in.

**The physical `git mv` + build-wiring (§11) is done** — see §11 and §14.

## 11. Build-system wiring checklist (done)

Per this repo's three-independent-build-definitions rule (CLAUDE.md), all of
the following landed together:

- **New directory** `src/kfx_pathfinding/{src,include}/` with its own
  `CMakeLists.txt` (glob-based, mirrors `kfx_config/CMakeLists.txt`'s shape —
  defines `kfx_pathfinding`/`kfx_pathfinding_hvlog` OBJECT libraries and
  appends to `KFX_OBJECT_LIBS_STD`/`_HVLOG`).
- **Top-level `CMakeLists.txt`**:
  - `"${CMAKE_SOURCE_DIR}/src/kfx_pathfinding/include"` added to the
    `kfx_common_opts` `target_include_directories(... INTERFACE ...)` list,
    between `kfx_config` and `kfx_sim`.
  - `add_subdirectory(src/kfx_pathfinding)` added before
    `add_subdirectory(src/kfx_sim)`.
- **`Makefile`**:
  - `-I"src/kfx_pathfinding/include"` added to `INCFLAGS`.
  - a pattern-rule pair mirroring the existing kfx_config ones added:
    `obj/std/%.o: src/kfx_pathfinding/src/%.c ...` and the `hvlog` variant.
  - `obj/kfx_pathfinding_state.o` added to `OBJS` (the `obj/ariadne*.o`
    entries already there needed no change — the new pattern rule resolves
    them against the new source directory automatically now that the files
    live there).
- **`scripts/check_layering.py`**: `"kfx_pathfinding"` inserted into
  `LIBRARY_ORDER` between `"kfx_config"` and `"kfx_sim"`. `--strict` passes
  with zero new `ACCEPTED_VIOLATIONS` entries needed.

## 12. Exit criterion (met)

For Tracks 1–3: `creature_control.h` no longer requires ariadne to call
back into kfx_sim to reach its own per-creature state; every kind-A/B
world query, CreatureControl-slot access, and `struct Thing` field touch in
`ariadne.c`/`ariadne_wallhug.c`/`ariadne_update.c` goes through
`pathfinding_world->*` rather than a direct call — verified by grep, zero
remaining bare `thing->`/`creatng->`/`doortng->` field access on any of
`mappos`/`move_angle_xy`/`index`/`clipbox_size_xy`/`owner`.

For the physical split: `ariadne*` physically lives in
`src/kfx_pathfinding/`, not `src/kfx_sim/`. `kfx_sim_state.h` no longer
contains `navigation_map`/`navigation_map_size_x/y`/
`map_changed_for_navigation` — moved to `struct KfxPathfindingState`
(`src/kfx_pathfinding/include/kfx_pathfinding_state.h`) and, per §9's final
decision (Option C, not A), deliberately **excluded** from all three blob-
serialization call sites rather than wired into them — safe because
`reinit_level_after_load()`'s unconditional `init_navigation()` call
already rebuilds this state on both the save-load and net-resync paths.
`src/kfx_pathfinding/` exists as its own CMake OBJECT library and
`Makefile` source directory, and `scripts/check_layering.py --strict`
passes with `kfx_pathfinding` inserted into `LIBRARY_ORDER` and zero new
entries needed in `ACCEPTED_VIOLATIONS`.

## 13. Decision

**Updated: Tracks 1–3 and the physical library split are all done and
build-verified. What's left is playtesting — this section's remaining
recommendation stands unchanged.**

An earlier pass on this document, written before counting call sites,
badly undersold the `struct Thing` field-access piece (Track 3) — grouping
by *kind* of query hid that it accounted for ~490 of ~520 total
cross-boundary touch points. That finding was real, and Track 3 turned out
to be exactly as large as re-estimated (~450 touch points across two
files, in the most collision-logic-dense code in the simulation). It also
turned out to be tractable in one sitting once approached file-by-file
with a compile check after each conversion, as this document itself
recommended — the "multi-session effort" framing was a reasonable caution
given the size, not a hard floor.

What actually still gates full confidence is **not** more conversion work,
it's verification depth: everything here has been build-verified (compiles
clean on all four target/debug-level combinations, `check_layering.py`
clean) but **not playtested** — no real game-data install was available to
exercise the converted wall-hugging/A* code in motion. Compiling clean
rules out syntax and type errors; it does not rule out a subtly
mis-sequenced read-modify-write (a partial-field write applied to a stale
local, a live-state push ordered one line too late relative to a callee
that reads it back). The conversion followed a consistent discipline for
this exact hazard (§14 documents it: fetch the thing's position once,
mirror every write back to the live thing immediately via
`thing_set_position`/`thing_set_move_angle` before any callee that reads
it back through the same accessor), but discipline followed correctly
under review is not the same guarantee a real playtest gives.

**Recommendation:** before merging, run this through at least the existing
`-ftests` pathfinding regression tests
(`bug_pathing_stair_treasury`, `bug_pathing_pillar_circling`) and some
manual play focused on wall-hugging around obstacles, digging-through-wall
paths, and door navigation. The physical library split (§11, now done) did
turn out to need more than a pure `git mv` — see §14 "Physical split
specifics" for the callback additions and pure-constant relocations it
surfaced — but every one of those was build-verified and is exactly the
kind of thing `check_layering.py --strict` and a real compiler are suited
to catch; it does not change what still needs an actual playtest before
merging.

## 14. As-built notes for Tracks 1–3 (implementation adjustments)

§8.2's callback table was a design sketch written before any call site was
actually converted. Converting the real ~490 sites across `ariadne.c`/
`ariadne_wallhug.c`/`ariadne_update.c` surfaced a few things the sketch
didn't anticipate — recorded here so the table above isn't taken as
literally what shipped:

- **`struct Map *`/`struct SlabMap *` stay opaque pointers, not out-params.**
  The sketch's `get_map_block_flags(x, y, struct Map **out)` shape didn't
  match how the code actually uses these: `get_map_block_at()`'s result is
  held in a local (`mapblk`) and its `.flags` field read several times
  across a function, and `get_slabmap_block()`'s result (`slb`) is both
  read directly (`.kind`) and threaded through the `CHECK_SLAB_OWNER` macro
  to a second function (`slabmap_owner(slb)`). The shipped design keeps
  `get_map_block_at`/`get_map_block_at_pos`/`get_slabmap_block` returning
  the real pointer (opaque to ariadne — forward-declared, never
  dereferenced on ariadne's side) and adds narrow accessors for every field
  ariadne actually reads off them: `map_block_flags`, `map_block_is_invalid`
  (wraps the existing `map_block_invalid`), `slabmap_block_kind`,
  `slabmap_block_is_invalid` (wraps `slabmap_block_invalid`, found during
  conversion — not in the original trace), `slabmap_owner`. This is the
  same opaque-handle shape as every other callback in this codebase; it
  just wasn't obvious until the call sites were in front of me.
- **`get_map_size_x`/`get_map_size_y` added.** `ariadne_wallhug.c` (2 sites)
  and `ariadne_update.c` (10 sites, in `update_navigation_triangulation`'s
  own bounds math) read `kfx_sim_state.map_subtiles_x/y` directly — this is
  kind A ("map extents") from §3, correctly identified there, but the
  callback table in §8.2's first draft omitted it. Added and wired.
- **Door `owner`/`is_locked` moved from Track 3 into Track 2.** The original
  §6 inventory bucketed all `struct Thing` field access together, including
  `doortng->owner`/`doortng->door.is_locked` (2 of the ~13 "index/owner/
  clipbox_size_xy" sites). Unlike the creature position/angle churn, these
  are two isolated single-purpose reads in one function
  (`get_navigation_colour_for_door`), not part of the movement hot path —
  cheap enough to fold into Track 2 (`door_is_locked`, `thing_get_owner`
  added to the callback struct) rather than wait for Track 3. This is why
  the Track 3 residual is `owner` ×3, not ×4 (§0).
- **`creature_control_get_from_thing`/`cctrl` locals removed entirely**, not
  just their field access rewritten — every one of the 12 touch points
  (`cctrl->navi` ×4, `cctrl->arid` ×4, `cctrl->max_speed` ×2,
  `cctrl->creature_state_flags`/`cctrl->combat_flags` ×1 site) was the
  *only* use of that `cctrl` local, so the intermediate variable and the
  `creature_control_get_from_thing()` call were deleted at each site in
  favor of calling the new accessor directly
  (`pathfinding_world->creature_get_navigation(creatng)` etc.) — slightly
  fewer lines than the original code, not more.
- **`#include "creature_control.h"` was left in place** in
  `ariadne_wallhug.c` even though nothing in the file still names
  `struct CreatureControl` after the above — a correctness-neutral cleanup
  better done alongside the physical file move (removing it now buys
  nothing while the file is still inside `kfx_sim`, and the physical move
  is the point where its absence actually gets tested).
- **Two more `#include`s added**: `pathfinding_world.h` in `ariadne.c`/
  `ariadne_wallhug.c`/`ariadne_update.c` (for the `pathfinding_world`
  symbol), and `ariadne_update.h` in `map_data.c`/`thing_list.c`/
  `room_data.c` (for the Track 1 accessors) —
  `kfx_apploop/game_session_loop.cpp` already had it.
- **A fifth `navigation_map` touch point existed beyond §4's original
  three**: `kfx_apploop/src/game_session_loop.cpp`'s `update()` also clears
  `map_changed_for_navigation` on the `GKind_NonInteractiveState` early-out
  path. Missed on the first pass because that investigation only grepped
  `kfx_sim`/`kfx_game`/`kfx_frontend`/`kfx_net`, not `kfx_apploop`. Fixed
  once found; §4 above is updated to list all five.

### Track 3 specifics

- **Six more callback entries, not four.** §0's design added
  `thing_get_position`/`thing_set_position`/`thing_get_move_angle`/
  `thing_set_move_angle` (the core Option 1 design from §6.3) plus
  `thing_get_index`/`thing_get_clipbox_size` for the two smallest remaining
  fields — bringing the table from 29 to 35 entries. A seventh,
  `subtile_is_door` (`MapSubtlCoord, MapSubtlCoord) -> TbBool`, was added
  mid-conversion: `ariadne.c`'s `blocked_by_door_at()` called it directly
  and it had been missed by every earlier pass, including the original
  call-site investigation — found only because the conversion touched
  every line of the file, not by re-auditing.
- **The live-sync discipline this needed, precisely.** Several functions
  (`creature_cannot_move_directly_to_with_collide`,
  `check_forward_for_prospective_hugs`,
  `get_starting_angle_and_side_of_hug_sub2`,
  `get_starting_angle_and_side_of_hug`) temporarily reposition a creature
  in-place to trial a collision check, then restore the original position
  before returning. Because sibling functions in the same call graph
  (`get_hugging_blocked_flags`, `get_angle_of_wall_hug`,
  `creature_cannot_move_directly_to_with_collide` itself) read the
  creature's position/angle back out through
  `pathfinding_world->thing_get_position`/`thing_get_move_angle` rather
  than through a parameter, a caller that only updated its own local
  `struct Coord3d`/`short` copy without pushing it to the live thing would
  desync from what those callees actually see. The rule applied
  throughout: every local mutation that mirrors an original
  `creatng->mappos = ...`/`creatng->move_angle_xy = ...` write is followed
  immediately by `pathfinding_world->thing_set_position()`/
  `thing_set_move_angle()`, with no batching or deferral — verified
  correct by re-reading each converted function against this rule, not
  just by it compiling.
- **Not every function needed the full discipline.** Several large
  functions (`get_next_position_and_angle_required_to_tunnel_creature_to`,
  ~370 lines) turned out to only *read* `creatng->mappos`/
  `move_angle_xy` — every mutation in that function is to the separate
  `struct Navigation *navi` parameter, never to the thing itself — so a
  single fetch at the top of the function, with no write-back at all, was
  sufficient and correct. Checking this per-function (not assuming the
  worst-case pattern everywhere) kept the conversion from being more
  invasive than it needed to be.
- **`ariadne_update.c` needed zero Track 3 changes**, confirming the
  original §6.1 count (1 line) — all of Track 3's ~450 touch points were in
  `ariadne.c` (24 functions) and `ariadne_wallhug.c` (17 functions).

### Physical split specifics

The `git mv` itself was mechanical (9 `.c` + 9 `.h` files, plus the two new
`kfx_pathfinding_state` files). What wasn't mechanical was everything the
move exposed once the compiler was the judge instead of a grep audit:

- **The real surprise: a large transitive-include dependency, not a direct
  one.** `ariadne.c` and `ariadne_update.c` had, all along, been reaching a
  wide umbrella of kfx_sim symbols — pure macros (`COORD_PER_STL`,
  `subtile_coord`), enums (`SlabBlockedFlags`/`SlbBloF_*`), two shared
  globals (`owner_player_navigating`, `nav_thing_can_travel_over_lava`), a
  debug no-op macro (`TRACE_THING`), and several state-reading functions
  (`creature_cannot_move_directly_to`, `get_subtile_number`,
  `stl_num_decode_x/y`, `stl_slab_center_subtile`) — *only* because
  `kfx_sim_state.h` transitively `#include`d `map_data.h`/`slab_data.h`/
  `thing_data.h`/`creature_control.h`/etc. This had already been broken
  (silently, no build attempted at the time) when the Track 1 state-struct
  migration swapped `#include "kfx_sim_state.h"` for
  `#include "kfx_pathfinding_state.h"` in those two files — the physical
  move was just the point a real build finally exercised it. Lesson: a
  header swap that looks like a pure state-struct change can silently drop
  an entire transitive-include umbrella; only a real compile catches it,
  not a grep for the field names being migrated.
  `src/kfx_pathfinding/src/ariadne_wallhug.c` had a milder version of the
  same issue in the other direction — it never included its own header,
  `ariadne_wallhug.h` (which defines `CHECK_SLAB_OWNER`,
  `IGNORE_SLAB_OWNER_CHECK`, the `WaHSS_*` enum, and `struct Navigation`
  itself), relying on one of the kfx_sim headers it *did* include to pull
  it in transitively. Fixed by adding the missing direct
  `#include "ariadne_wallhug.h"`.
- **Pure, zero-state constants relocated to `kfx_platform/globals.h`
  instead of wrapped as callbacks.** Consistent with `struct Around`/
  `SmallAroundIndex` already living there (moved during an earlier stage
  for the same reason — needed by both kfx_config and kfx_sim): `enum
  SlabBlockedFlags` (`SlbBloF_None/WalledX/WalledY/WalledZ`, from
  `thing_navigate.h`) and the subtile/slab/coordinate conversion macros
  (`STL_PER_SLB`, `COORD_PER_STL`, `COORD_PER_SLB`, `subtile_slab`,
  `slab_subtile`, `slab_subtile_center`, `coord_subtile`, `coord_slab`,
  `subtile_coord`, `slab_coord`, `subtile_coord_center`, from `map_data.h`)
  moved down wholesale — no logic changed, both are still used bare
  throughout kfx_sim exactly as before, now reachable from
  `kfx_pathfinding` too without an upward `#include`.
- **16 new `PathfindingWorldCallbacks` entries** (35 → 51) for genuinely
  state-reading or shared-mutable-global dependencies that surfaced only
  once the compiler was asked: `get_subtile_number`, `stl_num_decode_x`,
  `stl_num_decode_y`, `stl_slab_center_subtile`, `get_slabmap_for_subtile`,
  `hug_can_move_on`, `cross_x_boundary_first`, `cross_y_boundary_first`,
  `get_small_around` + `get_small_around_length` (wraps `map_utils.c`'s
  `small_around[]`/`SMALL_AROUND_LENGTH` — kept as a real accessor rather
  than a duplicated local constant since the table itself stays kfx_sim-
  owned), `small_around_index_in_direction`, `get_map_size_z` (wraps the
  `map_subtiles_z` extern), `creature_cannot_move_directly_to`, and two
  getter/setter pairs — `get_owner_player_navigating`/
  `set_owner_player_navigating`, `get_nav_thing_can_travel_over_lava`/
  `set_nav_thing_can_travel_over_lava` — for the two shared globals, since
  those are read *and* written by both kfx_sim (`thing_navigate.c`,
  `creature_states.c`) and ariadne, not something either side could take
  exclusive ownership of the way `navigation_map` could.
- **Verification approach for this piece specifically**: manual symbol-by-
  symbol grepping across headers turned out to be unreliable at this scale
  (it missed the `small_around_index_in_direction`/`map_subtiles_z`/
  `SlbAtFlg_*`/`ariadne_wallhug.h` self-include gaps on the first pass) —
  what actually found every remaining gap was attempting a real
  `KFX_OS=linux ./build-cmake.sh` and iterating on compiler errors directly,
  the same lesson as the transitive-include finding above. Once that
  converged, the full verification matrix was re-run from clean rebuilds:
  native Linux (`KFX_OS=linux`) and the default mingw-w64 i686 cross-compile,
  each in both `BFDEBUG_LEVEL=0`/`10` (four `cmake --build` combinations,
  zero errors), plus `check_layering.py --strict` (zero new violations, the
  `kfx_pathfinding -> kfx_sim` entries that existed mid-conversion all
  resolved, no new `ACCEPTED_VIOLATIONS` needed). The raw hand-maintained
  `Makefile` (`make standard`, mingw cross target) was also attempted: it
  correctly discovered and compiled all 10 new `kfx_pathfinding` source
  files with the right include path and zero errors specific to them, but
  the *full* build couldn't complete in this sandbox because its
  third-party source tree (`deps/`, `sdl/`) was never fetched here — a
  pre-existing environment gap unrelated to this work and distinct from
  CMake's already-working `FetchContent` cache (`out/_deps/`). This is a
  gap in what could be verified here, not a known defect.
