# `check_layering.py` has a symbol-level blind spot: `kfx_platform` is not actually a leaf library

Status: **fully resolved.** Every instance the post-build symbol audit
originally found (96 total, across all 8 libraries) is now either fixed
or confirmed as an intentional, documented architectural pattern.
Originally found 2026-08-28 while building the first real unit-test
binary for `docs/refactor/testing/` (stage 1:
[`../testing/stage-01-framework-and-scaffold.md`
](../testing/stage-01-framework-and-scaffold.md)). Fixed in three
passes, all 2026-08-29: kfx_platform's own instances; the largest
remaining cluster, kfx_config -> kfx_sim (61 instances); then the last
35, scattered across every other library pair. All three passes verified
by a clean two-variant build (`keeperfx` and `keeperfx_hvlog`) and by the
`scripts/check_layering_symbols.py`
post-build audit (see "Fix" below), which now reports zero new
violations for either library.

## The claim vs. the reality

`docs/Architecture/architecture.md` §2.1 states `kfx_platform` "**Depends
on:** external libs only (SDL3, enet, zlib, …)" — the foundation of the
whole dependency ladder. `scripts/check_layering.py --strict` backs this up:
running it today reports **zero** `kfx_platform` violations.

That claim was only true at the `#include` level, which is all that
checker looks at (`classify()` + `extract_includes()`, both text/AST-free
`#include "foo.h"` regex matching). It was not true at the **link**
level: several `kfx_platform` `.c`/`.cpp` files called functions and read
globals whose only definition lived in `kfx_config`, `kfx_render`,
`kfx_sim`, or `kfx_game`, all ranked *above* `kfx_platform`, via bare
same-file `extern` forward-declarations instead of `#include`-ing the
real header — invisible to a checker that only looks at `#include` text.

## What actually turned out to be true, per instance

Investigating each claim in the original writeup (grep + reading the real
definitions, not just trusting the comments) found three genuinely
different situations, not one:

### 1. Misplaced code that could just move down (no callback needed)

- **`get_sprite`** (`src/kfx_platform/include/bflib_sprite.h`): the
  entire file it was defined in, `spritesheet.cpp`, only ever
  `#include`d other kfx_platform headers (`bflib_sprite.h`,
  `bflib_filelst.h`, `bflib_dernc.h`) and had zero real kfx_render
  dependency — `TbSpriteSheet` was a private class defined right there.
  It was in the wrong directory, nothing else. **Fix:** `git mv
  src/kfx_render/src/spritesheet.cpp src/kfx_platform/src/`.
- **7 of the 14 `units_per_pixel_*`/`aspect_ratio_factor_*` globals**:
  `units_per_pixel_landview`, `units_per_pixel_landview_frame`,
  `aspect_ratio_factor_HOR_PLUS(_AND_VERT_PLUS)`,
  `landview_frame_movement_scale_x/y`, `first_person_vertical_fov` were
  declared as tentative definitions in `kfx_render/src/vidmode.c`, but
  their *only writers* — `calculate_landview_upp()` and
  `calculate_aspect_ratio_factor()` — were already physically defined in
  `kfx_platform/src/bflib_video.c`; `vidmode.c` never read or wrote them
  directly, only called into those two functions (a legal downward
  call). **Fix:** moved the storage from `vidmode.c` into
  `bflib_video.c`. The `extern` declarations in `bflib_video.h` are
  unchanged, so every other consumer (kfx_render, kfx_frontend, kfx_sim)
  is unaffected.
- **`get_rid()`**: a pure name→id lookup over whatever `NamedCommand`
  array is passed in, zero `kfx_config_state` coupling. **Fix:** moved
  `get_rid()` and `struct NamedCommand` down to
  `kfx_platform/include/bflib_basics.h` / `src/bflib_basics.c`.
  `creature_desc[]` itself (the actual creature-name registry) stayed in
  kfx_config, as genuine state — reached via callback (below).
- **`first_person_horizontal_fov`**: declared in kfx_platform's
  `bflib_video.h` but never read by any kfx_platform `.c` file — its
  only reader was kfx_render's own `engine_camera.c`. Moved the
  declaration into kfx_render's `vidmode.h` (dead weight removed from
  kfx_platform's public surface, not a behavior change).

### 2. Genuine cross-layer needs — fixed via the existing callback-struct pattern

Everything else really did read higher-layer *state* (not just
misplaced code), so it needed the same callback-struct pattern already
used throughout this codebase (`SoundStateCallbacks`,
`InputFocusPredicates`, etc., wired in `main.cpp::setup_game()`):

- `prepare_file_path`/`prepare_file_path_mod`/`prepare_file_path_buf`/
  `prepare_file_fmtpath` (read `kfx_config_state`'s install/mods state) —
  added as 4 new `SoundStateCallbacks` fields, used by
  `sound_manager.cpp`, `bflib_sndlib.cpp`, **and** `bflib_input_joyst.cpp`
  (a `prepare_file_path` bare-extern the original writeup had missed —
  found by the new post-build audit, see below).
- `creature_code_name(crmodel)` and `creature_desc[]` access (kfx_config
  state) — added as `SoundStateCallbacks::creature_code_name`/
  `get_creature_desc`.
- `thing_is_invalid(const struct Thing*)` (checks
  `kfx_sim_state.things_data` bounds) — added as
  `SoundStateCallbacks::thing_is_invalid`.
- `units_per_pixel_width/height/menu_height/best/menu/ui/min` — of these
  7, only `width/height/best/menu/ui` are actually read by kfx_platform
  code (`menu_height` and `min` are declared but never read there).
  Added a new `VideoScaleCallbacks` struct (`bflib_video.h`) bundling
  all 5 behind one getter (cheaper than 5 separate function-pointer
  calls in this hot-path scaling math), backed by
  `vidmode.c::get_video_scale_values()`.
- `emulate_integer_overflow(nbits)` (reads
  `kfx_config_state.conf.rules[0]...`) — `bflib_basics.c` had no
  existing callback struct to extend, so this got a single registered
  function pointer (`EmulateIntegerOverflowFunc`/
  `emulate_integer_overflow_provider`), the same shape as `get_gameturn`
  below.
- **`get_gameturn()`** — the wart already named (but not fixed) in
  architecture.md. Fixed without touching any of its call sites (used by
  `ERRORLOG`/`WARNLOG`/`SYNCLOG`/etc. everywhere): `get_gameturn()`
  itself is now a thin wrapper in `bflib_basics.c` over a registered
  `GetGameTurnFunc` provider, defaulting to a safe stub (`0`) until
  `main.cpp` wires up kfx_game's real implementation (renamed
  `game_legacy_get_gameturn()`, since the plain name is now owned by the
  kfx_platform wrapper).

### 3. Not a violation — by design

**Revised, see below.** `src/kfx_platform/src/kfx/platform/PlatformLinux.cpp`
(and `PlatformWindows.cpp`) define the process's actual `main()`, which
calls `kfxmain()` — declared in kfx_platform's own `platform.h` but
*defined* in `main.cpp` (`app_entry`). This is the OS-callable-entry-point
pattern (the platform layer legitimately needs to be what the OS calls
first, then hand control to the app) — inverse direction by design. Left
as-is, and added to the new post-build audit's accepted-residuals list (see
below) so it doesn't show up as noise.

**Later revised** (`docs/refactor/todo/remove-kfxmain-symbol-residual.md`):
this judgment turned out to be wrong. `kfx_platform` owned the *entry point
itself*, not a platform service — moving `main()`/`WinMain()` down into
`app_entry` (`src/native_entry.cpp`) removed the reverse reference
entirely, so this is no longer an accepted residual at all.

## Fix: a post-build symbol audit (`scripts/check_layering_symbols.py`)

Per this document's original option 2, now implemented: after a build,
it runs `nm` over every `kfx_*` `OBJECT` library's compiled `.o` files,
builds a symbol→defining-library map from each library's externally
visible (uppercase-type) defined symbols, and flags any `U`ndefined
symbol in one library that resolves only to a symbol defined in a
strictly higher-ranked one. This is ground truth from the linker's point
of view — it catches the bare-extern-forward-declaration pattern (and
anything else shaped like it) for free, with no declaration-parsing or
hand-maintained name index. It complements, not replaces,
`check_layering.py`: the `#include`-level check runs on raw source with
no build required (fast enough to gate every push); this one needs an
actual build tree, so it's meant for periodic/CI-post-build runs or
local investigation while working on a specific library boundary.

```bash
KFX_OS=linux ./build-cmake.sh                      # build first
python3 scripts/check_layering_symbols.py           # human-readable report
python3 scripts/check_layering_symbols.py --json     # machine-readable
python3 scripts/check_layering_symbols.py --strict   # exit 1 on violation
```

Running it against a clean build today: **zero new `kfx_platform`
violations** (one accepted residual: `kfxmain`, case 3 above).

## The bigger discovery: this blind spot was codebase-wide, not kfx_platform-specific

Running the new audit against the whole build originally surfaced **96
more instances of the exact same pattern**, spread across every other
library in the ladder — clearly out of scope for this document's
original kfx_platform focus, but directly relevant to the
architecture's longer-term goal of the libraries being genuinely
independent, not just `#include`-acyclic. `kfx_config → kfx_sim` (61
instances, nearly two-thirds of the total) was fixed as a follow-up pass
(see below); the remaining 35 were spread thin across every other
library pair (`kfx_sim → kfx_net`/`kfx_render` 9 each, `kfx_config →
kfx_frontend`/`kfx_game` 2 each, `kfx_net → kfx_game` 2,
`kfx_sim → kfx_frontend`/`kfx_game` 2 each, `kfx_frontend → app_entry`
2, and single instances of `kfx_game → app_entry`, `kfx_net →
kfx_apploop`/`kfx_frontend`, `kfx_render → kfx_apploop`/`kfx_net`) — all
now closed (see "Final 35" below).

Running `python3 scripts/check_layering_symbols.py` on a clean build
today reports **zero new violations** for every library.

### `kfx_config → kfx_sim` fix (2026-08-29 follow-up)

All 61 instances resolved, via the same two-tier triage as
kfx_platform's:

- **Movable (no callback needed):** `terrain_room_total_capacity_func_list`/
  `terrain_room_used_capacity_func_list` (17 symbols: config_terrain.c's
  `count_*_in_room`/`count_slabs_*` family) and
  `powermodel_expand_check_func_list` (3 symbols: config_magic.c's
  `*_expand_check` functions) were data tables defined in kfx_config but
  populated entirely with kfx_sim functions and read only by kfx_sim (or,
  for the power one, kfx_render) — never by kfx_config itself. Moved
  both tables down to their real owners (`kfx_sim/room_data.c`,
  `kfx_sim/power_process.c`). 20 symbols resolved for free.
- **Genuine state, needs a callback:** the remaining 41 (plus one bonus,
  `thing_class_and_model_name`, shared by two files, found via the audit
  itself) were added to `config.h`'s existing `ConfigReloadCallbacks`
  struct, following the same pattern as kfx_platform's fix — spanning
  config_terrain.c (6), config_campaigns.c (2), config_compp.c (4),
  config_creature.c (10), config_crtrmodel.c (9), config_crtrstates.c
  (4), config_magic.c (1), config_objects.c (1), config_rules.c (1),
  config_strings.c (1), config_trapdoor.c (2).
- **A new wrinkle not seen in kfx_platform's fix:** three files
  (config_compp.c, config_crtrstates.c) embed a kfx_sim `NamedCommand`
  table pointer directly inside a `static const struct NamedField[]`
  compile-time initializer, where a runtime callback call can't
  syntactically appear. Fixed by dropping `const` from those specific
  arrays, initializing the affected slots to `NULL`, and patching them
  at runtime via each `ConfigFileData`'s existing `pre_load_func` hook
  (`resolve_compp_func_type_pointers()`,
  `resolve_crstates_func_commands_pointers()`) — confirmed safe because
  all these config files are (re)loaded together, well after
  `set_config_reload_callbacks()` runs, in kfx_sim's
  `dungeon_stats.c`'s per-level config-reload sweep, never during early
  process startup.
- Also caught **one instance the original kfx_platform pass missed**:
  `bflib_input_joyst.cpp`'s own bare-extern `prepare_file_path` call,
  found by re-running the post-build audit after the kfx_platform fix
  landed — now also routed through `SoundStateCallbacks`.

Verified: `check_layering_symbols.py` reports zero new `kfx_config`
violations; `check_layering.py --strict` still shows only the same 2
pre-existing, unrelated violations (`kfx_pathfinding → kfx_sim`,
`kfx_sim → kfx_script`) that predate all of this work.

### The final 35: every other library pair (2026-08-29 follow-up)

The remaining instances split into two genuinely different categories —
about a third of them turned out not to be bugs at all:

**Real violations, fixed (9 total, plus 1 relocation):**

- **`game_key_settings[]`** (`kfx_config → kfx_frontend`, 1 of the 2
  `config_settings.c` instances counted above): `struct GamekeySettings`
  and its `extern const struct GamekeySettings game_key_settings[]`
  declaration already lived in kfx_config's `config_settings.h` (a
  stage-13.3 move that only moved the declaration, not the data) but the
  ~80-entry initializer itself was still physically in kfx_frontend's
  `front_input.c`. Moved the data down to `config_settings.c` to match,
  along with `enum BindingMenuVisibility` (pure name/ID vocabulary used
  only by that struct's fields, same shape as `GameKeys`/
  `GAME_KEYS_COUNT`'s own earlier move to kfx_platform).
- **`g_speech_queue_limit`** (`kfx_config → kfx_frontend`,
  config_sounds.c): a plain kfx_frontend-owned `int` (the speech message
  queue's UI-owned limit), written directly by config_sounds.c via a
  bare extern hiding in kfx_config's own `config_sounds.h`. Added
  `ConfigReloadCallbacks::set_speech_queue_limit`.
- **`script_strdup`/`script_strval`** (`kfx_config → kfx_game`,
  config.c): kfx_game's live script-string-pool interning functions,
  used by config.c's script-hook value parsing. Added to
  `ConfigReloadCallbacks`.
- **`autostart_multiplayer_campaign`/`autostart_multiplayer_level`/
  `force_player_num`** (`kfx_frontend → app_entry`,
  `kfx_game → app_entry`): three cmdline-parsed globals that had been
  left in `main.cpp` instead of kfx_config's already-established
  `struct StartupParameters` (`start_params`) home for exactly this kind
  of process-startup config. Moved all three into `start_params` —
  resolves all 3 remaining instances via pure relocation, no callback
  needed.
- **`host_packet_received`** (`kfx_net → kfx_apploop`,
  net_exchange_common.c): kfx_apploop's multiplayer clock-adjust
  timestamp. Added to the existing `NetCallbacks` struct (kfx_config's
  `net_callbacks.h`) as `set_host_packet_received` — that struct already
  existed specifically to close this exact violation shape for other
  symbols (its own comments cite a near-identical historical case).
- **`interpolate_time`** (`kfx_render → kfx_apploop`, engine_render.c):
  kfx_apploop's per-frame interpolation fraction. Added
  `RenderOverlayCallbacks::get_interpolate_time`.
- **`units_per_pixel_ui`** (`kfx_sim → kfx_render`, power_hand.c): the
  same kfx_render-owned scale value kfx_platform's fix already routed
  through `VideoScaleCallbacks`/`video_scale_callbacks` — kfx_sim (ranked
  above kfx_platform) can call that existing callback directly, no new
  plumbing needed.
- **`load_texture_map_file`** (`kfx_sim → kfx_render`, lvl_filesdk1.c):
  already properly declared in kfx_render's own `engine_textures.h`, but
  reached via bare extern instead. Added to `sim_feedback.h`'s
  `SimFeedbackCallbacks` (kfx_sim's general-purpose "reach into a higher
  layer" struct, already used pervasively in this exact file).
- **`process_dungeon_destroy`/`initialise_devastate_dungeon_from_heart`**
  (`kfx_sim → kfx_game`, player_utils.c/thing_objects.c) and
  **`light_create_light`/`light_init_dungeon_heart`/
  `light_get_light_intensity`/`light_set_light_intensity`/
  `light_set_light_never_cache`** (`kfx_sim → kfx_render`, same two
  files): all added to `SimFeedbackCallbacks`. Notably, `thing_objects.c`
  already called `sim_feedback->light_create_light()` correctly at one
  call site and the bare-extern version at another — a case of the
  callback existing but a later addition forgetting to reuse it.
- **`event_button_info[]`** (`kfx_sim → kfx_frontend`, map_events.c) and
  **`frontstats_initialise`** (`kfx_sim → kfx_frontend`, player_utils.c):
  `event_button_info` mixes UI fields (button sprite, tooltip —
  kfx_render-owned enum values) with two sim fields kfx_sim actually
  reads, so it stays kfx_frontend-owned rather than moving down; added
  `SimFeedbackCallbacks::get_event_button_info`/`frontstats_initialise`.

**Not bugs — deliberate, now-documented architectural patterns (15
symbol-occurrences, added to `check_layering_symbols.py`'s
`ACCEPTED_SYMBOL_VIOLATIONS`, none touched):**

- **`get_packet`/`get_packet_direct`/`set_packet_action`/
  `set_players_packet_action`** (`kfx_sim → kfx_net`, 9 occurrences
  across creature_instances.c/roomspace.c/roomspace_prediction.c/
  thing_creature.c, plus `kfx_render → kfx_net` in engine_redraw.c):
  kfx_sim's own `packet_data.h` *already* explains this precisely —
  `struct Packet` is split out of kfx_net's `packets.h` specifically
  because kfx_sim/kfx_render dereference its fields directly and
  pervasively, so the type (and these trivial accessor declarations)
  have to live at kfx_sim's layer, while the real implementation stays
  in kfx_net's `packets.c`/`packets_misc.c`. Quoting that file's own
  rationale: "a higher-ranked library implementing a lower-ranked
  interface is fine — only the reverse is a violation." Nothing to fix;
  this is exactly what the design intends.
- **`kfx_frontend_state`/`kfx_game_state`/`game`** (`kfx_net →
  kfx_frontend`/`kfx_game`, net_resync.cpp): the already-known,
  already-accepted (in `check_layering.py`'s own `ACCEPTED_VIOLATIONS`)
  raw-blob network resync serialization — `game`/`kfx_game_state`/
  `kfx_frontend_state` are `memcpy`'d wholesale by design. This is the
  wire format; the symbol-level audit was simply seeing the same
  intentional exception the `#include`-level checker already names.
- **`creature_table_add[]`** (`kfx_sim → kfx_render`, creature_graphics.c):
  the same shape as `packet_data.h` above, just undocumented until now —
  `struct KeeperSprite` is defined in kfx_sim's own
  `creature_graphics.h` (pervasive field dereferencing forces it down),
  but the array's real storage is kfx_render's `custom_sprites.c`. Added
  a matching rationale comment to `creature_graphics.h` alongside the
  declaration, plus the audit exemption.

Verified: `check_layering_symbols.py` reports **zero new violations
across every library**; `check_layering.py --strict` still shows only
the same 2 pre-existing, unrelated violations.

## Why this matters beyond the test harness

- **The dependency ladder was not actually acyclic at the symbol level**
  for any library, only at the `#include` level — now fixed and verified
  everywhere, not just kfx_platform.
- **It was blocking the "one test binary links the whole library"
  design** stage 1/2 of `docs/refactor/testing/` planned — see
  [`../testing/stage-01-framework-and-scaffold.md`](../testing/stage-01-framework-and-scaffold.md)'s
  `kfx_platform_utest` `CMakeLists.txt`. With kfx_platform's instances
  fixed, that workaround (compiling only the specific tested source file
  instead of linking kfx_platform as a unit) is worth revisiting.
- **It was actively maintained, not just a fossil**: the
  `sound_manager.cpp` and `bflib_basics.c` comments showed a contributor
  deliberately choosing the forward-declare workaround *to keep
  `check_layering.py --strict` green* rather than fixing the underlying
  dependency. The new post-build audit closes that loophole going
  forward — a future same-file bare-extern workaround will show up in
  `check_layering_symbols.py`'s report even though `check_layering.py`
  still can't see it.

## Options considered for the checker itself (historical)

1. **Extend `check_layering.py`** with a name→defining-library index
   built from `git grep`, cross-referenced against bare `extern`
   declarations. Cheapest to reason about but a real chunk of new regex/
   parsing logic, and prone to false positives on common names. Not
   implemented — superseded by option 2 below, which needed no
   declaration-parsing at all.
2. **Post-build symbol audit**: implemented, see above.
3. **Fix the known instances properly** via the callback-struct pattern
   or by moving misplaced code down: done for every kfx_platform
   instance (see per-instance breakdown above).
