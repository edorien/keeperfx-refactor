# Stage 4 — Extract `kfx_config`

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

`config_*` (48 files, 21,532 LOC) is a data-file parsing layer that should
depend only on `kfx_platform` (file I/O, string/TOML parsing). The audit
found one structural issue that dominates everything else, plus a large
number of essentially-free cleanups.

## The central issue: config's own data lives inside `struct Game`

`game_legacy.h` defines `struct Configs` — an aggregate embedding
`SlabsConfig`, `PowerHandConfig`, `MagicConfig`, `CubesConfig`,
`TrapDoorConfig`, `EffectsConfig`, `CreatureConfig`, `ObjectsConfig`,
`RulesConfig[PLAYERS_COUNT]`, `PlayerStateConfig`, `ColumnConfig`,
`LuaFuncsConf` — as a member `game.conf` of the global `struct Game game;`.
`game_legacy.h` in turn `#include`s nearly every `config_*.h` to declare
`struct Configs`, creating a **reverse dependency** (config → game_legacy →
config) that is the single biggest reason config files look coupled to
everything.

Consequence, confirmed by usage counts: **18 of 48 config files**
(`config_crtrstates`, `config_powerhands`, `config_cubes`,
`config_slabsets`, `config_mods`, `config_objects`, `config_players`,
`config_effects`, `config_spritecolors`, `config_creature`, `config_magic`,
`config_terrain`, `config_sounds`, `config_trapdoor`, `config_rules`,
`config_keeperfx`, `config_textures`, `config_crtrmodel`) include
`game_legacy.h` purely to reach `game.conf.*` (e.g. `config_creature.c` has
135 uses of `game.`, `config_magic.c` 61, `config_trapdoor.c` 51,
`config_terrain.c` 25). This one include drags the entire simulation/
render/net/lua header graph into every config parser's translation unit.

**This is the same problem stage 5 solves for the rest of the codebase,
applied to config specifically** — moving `struct Configs` (and its 12
sub-structs) to be owned and declared by the config module itself (with
`game.conf` becoming a pointer/reference into config-owned storage, or the
top-level `struct Game` composing it by reference) removes the reverse
dependency in one move. **Recommend doing this as part of stage 5, not
duplicating the design work here** — but it's called out early because it
blocks a clean `kfx_config` regardless of when it's actually done.

## Free cleanup: ~25 dead includes

Checking real symbol usage (not just `#include` presence) found a large
share of "config coupling" is unused includes, deletable with zero
interface work:

- `config_creature.c`: `thing_doors.h`, `creature_jobs.h`,
  `engine_arrays.h`, `creature_graphics.h`, `creature_states_combt.h`.
- `config_creature.h`: `creature_control.h`, `thing_creature.h`,
  `creature_graphics.h` (all three — `Thing` is only forward-declared here).
- `config_crtrmodel.c`: `thing_doors.h`, `creature_control.h`, `player_data.h`.
- `config_campaigns.c`: `map_data.h`, `game_merge.h`.
- `config_effects.c`: `thing_effects.h`.
- `config_magic.c`: `thing_effects.h`, `thing_physics.h`, `thing_shots.h`.
- `config_lenses.c`: `thing_doors.h`.
- `config_settings.c`: `frontmenu_options.h` (false-positive field-name
  collision with its own `struct GameSettings`, not the same-named
  `frontmenu_options.h` externs), `game_merge.h`.
- `config_sounds.c`/`.h`: `gui_soundmsgs.h` (`g_speech_queue_limit` is
  redundantly re-declared in `config_sounds.h` itself; `output_message()`
  appears only in a comment).
- `config_spritecolors.c`: `player_instances.h`.
- `config_strings.c`: `game_merge.h` (false-positive match on an unrelated
  `mod_item->state` field).
- `config_trapdoor.c`: `player_instances.h`; `config_trapdoor.h`:
  `engine_camera.h`.
- `config_objects.h`: `thing_objects.h` (only hit is inside a doc comment).
- `config_cubes.h`: `player_data.h`.
- `config.c` (the generic parsing engine): `front_simple.h`, `scrcapt.h`,
  `vidmode.h`, `custom_sprites.h`, `lvl_script_lib.h`,
  `config_translation.h`, `config_keeperfx.h`, `config_campaigns.h` — none
  of these are referenced anywhere in the file.

Land this as its own PR first — it's risk-free and makes the remaining
real edges (below) easier to see.

## Real coupling that needs a design decision, not just deletion

**A. Config-reload → live-world propagation, currently a direct call
instead of an event.** Several config files call straight into sim/GUI
code after reloading a value, rather than emitting a "config changed"
event the sim/GUI would subscribe to:

- `config_campaigns.c`, `config_terrain.c` → `update_room_tab_to_config()`/
  `update_trap_tab_to_config()`/`update_powers_tab_to_config()`
  (`frontmenu_ingame_tabs.h`, **frontend**).
- `config_trapdoor.c` → same GUI functions, plus `update_all_door_stats()`
  (`thing_doors.h`) and `update_trap_draw()` (`thing_traps.h`) — pushes new
  stats onto live `Thing`s.
- `config_crtrmodel.c` → `do_to_all_things_of_class_and_model`,
  `do_to_players_all_creatures_of_model`,
  `update_speed_of_player_creatures_of_model` (`thing_list.h`),
  `update_creature_health_to_max`/`update_relative_creature_health`
  (`thing_stats.h`), `set_creature_model_graphics` (`creature_graphics.h`),
  `process_job_stress_and_going_postal` (`creature_states_mood.h`).
- `config_rules.c` → `add_research_to_all_players`/
  `clear_research_for_all_players` (`room_library.h`),
  `panel_map_update()` (`frontmenu_ingame_map.h`, GUI redraw).

Recommendation: introduce a `config_reload_event` (or per-domain variant)
that `kfx_sim`/`kfx_frontend` subscribe to, so `kfx_config` never needs to
`#include` sim/frontend headers to apply a just-loaded value.

**B. `config_keeperfx.c`/`config_settings.c` writing engine/frontend
globals directly** — not disguised as data storage, a real leak:
`config_keeperfx.c` assigns to `creature_status_size`, `line_box_size`
(`engine_render.h`), `default_tag_mode`, `right_click_tag_mode_toggle`
(`frontend.h`), `rotate_around_mouse_option`, `rotate_follow_mouse_option`,
`zoom_to_mouse_option` (`front_input.h`), `gui_blink_rate`,
`neutral_flash_rate` (`gui_draw.h`), and calls `network_is_active()`
(`game_legacy.h`). `config_settings.c` reads `game_key_settings[]`
(`front_input.h`) to seed default keybindings and `FRONTVIEW_CAMERA_ZOOM_MAX`
(`engine_camera.h`) as a clamp bound. Fix: config should populate its own
struct; render/frontend/input read *from* config at startup, not the
other way around.

**C. Config structs embedding non-config types by value** — a
structural leak baked into the data model itself, not just usage:
- `config_slabsets.h`: `struct ColumnConfig` embeds `struct Column
  cols[COLUMNS_COUNT]` from `map_columns.h` (a map-rendering type).
- `config_terrain.h`: `struct RoomConfigStats` embeds three `SpeechRef`
  fields from `gui_soundmsgs.h` (GUI speech-queue layer), plus
  `Room_Update_Func` (a callback typedef from `room_data.h` — legitimate
  callback-table pattern, lower priority than the `SpeechRef` embed).
- `config_magic.h`: embeds `SpeechRef speech` (`gui_soundmsgs.h`) and
  `struct InstanceInfo instance_info[...]` (`creature_instances.h`)
  directly by value.

Fix: replace the embedded concrete types with opaque IDs/handles that
`kfx_sim`/`kfx_frontend` resolve, the same pattern used throughout stage 5.

**Stage 4 status: assessed, deferred to stage 5 (not implemented here).**
Confirmed real consumers exist for all three embeds (`struct Column
cols[]` is written by `config_slabsets.c`; `msg_needed`/`msg_too_small`/
`msg_no_route` are read by both `config_terrain.c` and
`lvl_script_commands.c`; `instance_info[]` is read by `config_creature.c`,
`creature_instances.c` and `player_instances.c`), so this isn't a small
mechanical fix -- it needs the opaque-ID/handle pattern this doc itself
says stage 5 should establish canonically. Doing a one-off version here
risks inventing a shape stage 5 then has to redo. Left for stage 5 to
pick up alongside `struct Configs`'s own extraction out of `struct Game`.

**D. `config_magic.c` contains runtime gameplay logic, not just
parsing.** `add_power_to_player()`/`remove_power_from_player()` are the
live "grant/revoke a keeper power" operations, called at runtime whenever a
player researches or loses a power — not config parsing. Its `switch`
calls `turn_off_power_obey()`, `turn_off_power_sight_of_evil()`,
`turn_off_power_call_to_arms()`, `prepare_to_controlled_creature_death()`
(`thing_creature.h`). Fix: split `config_magic.c` — parsing stays in
`kfx_config`; `add_power_to_player`/`remove_power_from_player` move to
`kfx_sim`, ideally alongside `magic_powers.c` which already owns the
runtime implementation of every other power's effect.

## Exit criterion

`libs/kfx_config` exists, depends only on `kfx_platform`. `struct Configs`
no longer requires config files to include `game_legacy.h` (coordinate
timing with stage 5). `config_magic.c`'s runtime power-grant/revoke logic
has moved to `kfx_sim`.
