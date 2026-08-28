# Stage 5 — Decompose the god headers (`struct Game`, `keeperfx.hpp`)

See [00-overview.md](00-overview.md) for the full plan context, especially
the revision note: **this stage was promoted from "final cleanup" to
directly after `kfx_config`, because it blocks stages 6–10 simultaneously,
not just app-target slimming.**

## Goal

`game_legacy.h` and `keeperfx.hpp` were read in full, struct by struct and
global by global, and each item assigned to the library that should own
it. `globals.h` was audited the same way and cleared — it's a legitimate
low-level vocabulary header (coordinate structs, ~55 domain-ID typedefs)
that every library can safely depend on; no work needed there beyond the
one macro-indirection note in stage 2.

`game_legacy.h` and `keeperfx.hpp` are real problems. The core of the
problem is one struct.

## `struct Game` (in `game_legacy.h`, ~176 fields) is the crux

It is the single largest struct in the codebase, and it embeds — **by
value**, not by reference — types owned by every future library at once:
a `struct GuiMessage messages[GUI_MESSAGES_COUNT]` array (from
`gui_msgs.h`, frontend), the entire `struct Configs conf` (config, see
stage 4), desync-logging structs (net), and lighting/texture state
(render), inside the same struct that also holds
`things_data[THINGS_COUNT]` and `map[...]` (sim). This is confirmed as the
direct cause of, among other things: 30 of 31 `game_*`→frontend header
includes routing through this one header (stage 9/10 finding), and
`engine_redraw.c` being unable to read `game.map_subtiles_x` without also
seeing the script VM and GUI message state (stage 7 finding).

**This cannot be fixed by deleting includes.** It requires splitting the
struct itself. Two independently-converged field-group breakdowns (from
the god-header read and the frontend/game audit) agree closely; the table
below is the merged result.

### Proposed field-group ownership

| Destination | Representative fields | Notes |
|---|---|---|
| **`kfx_sim`** (by far the largest group) | `players[]`, `columns_data`, `slabset*`/`slabobjs[]`, `cctrl_data[]`, `things_data[]`, `navigation_map[]`, `map[]`, `computer_task[]`/`computer[]`, `slabmap[]`, `rooms[]`, `dungeon[]`, `thing_lists[]`, `gold_lookup[]`, `battles[]`, `creature_scores[]`, `pool` (`CreaturePool`), `chosen_room/spell/manufactr_*`, `hand_over_subtile_*`, `around_*`, `block_health[]`, `entrance_room_id`, `entrances_count`, `map_subtiles_x/y`, `map_tiles_x/y`, random seeds (`action_random_seed`, `ai_random_seed`, `player_random_seed`, `unsync_random_seed`, `sound_random_seed`) | Random seeds are simulation-determinism primitives — keep with sim (or a shared RNG utility in `kfx_platform`), not scattered across the layer that happens to consume them. |
| **`kfx_game`** | `script` (`LevelScript`) + all `script_*` fields, `event[]`, `action_points[]`, `game_kind`, `play_gameturn`, `pckt_gameturn`, `selected_level_number`, `continue_level_number`, `loaded_level_number`, `campaign_fname`, `last_level`, `frame_skip/step`, `paused_at_gameturn`, `mode_flags`, `operation_flags`, `easter_egg*`, `heart_lost_*`, `triggered_object_location`, `level_name` (from `keeperfx.hpp`), `default_loc_player`, `TimerGame`/`TimerNoReset`/`TimerFreeze`, `turns_per_second`, `exit_keeper`/`quit_game` | Session/rules/campaign bookkeeping and game-loop control flags. |
| **`kfx_net`** | `packets[]`, `packet_save_*`/`packet_load_*`, `packet_fname`, `packet_fopened`, `turns_stored`, `turns_fastforward`, `turns_packetoff`, `local_plyr_idx`, `input_lag_turns`, `active_players_count`, `host_checksums` (`DesyncChecksums`), `log_snapshot` (`LogDetailedSnapshot`, built from `LogThingDesyncInfo`/`LogPlayerDesyncInfo`/`LogRoomDesyncInfo`), `neutral_player_num` | The four `Log*DesyncInfo`/`DesyncChecksums` structs are purpose-built for multiplayer desync detection — unambiguous net ownership. |
| **`kfx_render`** | `lish` (lighting/shadows), `texture_animation[]`, `texture_id`, `top_cube[]`, `active_lens_type`/`applied_lens_type`, `small_map_state`, `mouse_light_pos`, `ceiling_height*`, `fps_limit_current/main/secondary`, palettes from `keeperfx.hpp` (`blue_palette`, `red_palette`, `dog_palette`, `vampire_palette`, `engine_palette`, `frontend_backup_palette`, `zoom_to_heart_palette`, `temp_pal`, `lightning_palette`), `optimised_lights`/`total_lights`/`do_lights` | `thing_pointed_at`/`me_pointed_at` (results of the engine's picking pass) are a genuine render/sim boundary case — classify with picking/render since that's where they're produced, but flag as needing a maintainer call if sim code ever needs to read them without a render dependency. |
| **`kfx_frontend`** | `messages[]` (`GuiMessage`), `quick_messages[][]`, `evntbox_text_objective`, `evntbox_text_buffer`, `evntbox_scroll_window`, `flash_button_index/time`, `active_panel_mnu_idx`, `box_tooltip[][]`, `fx_lines[]`/`active_fx_lines`, `heart_lost_display_message/quick_message/message_id/message_target`, `comp_player_aggressive/defensive/construct/creatrsonly`, `creatures_tend_imprison/flee` (explicitly commented in source as "GUI only"), `loaded_swipe_idx`, `active_messages_count`, `computer_chat_flags`, `gui_cheat_box_1..4`, plus from `keeperfx.hpp`: `last_mouse_x/y`, `my_mouse_x/y`, `pointer_x/y`, `top_pointed_at_*`, `continue_game_option_available`, `define_key_scroll_offset`, `time_last_played_demo`, `drag_menu_x/y`, `tool_tip_time`/`help_tip_time`, `top_of_breed_list`, `no_of_breeds_owned`, `level_names_data`/`end_level_names_data`, `timerstarttime`/`Timer`, `TimerTime` struct | This is the group that most clearly shouldn't be inside `struct Game` at all — it's HUD/menu presentation state with no simulation meaning. |
| **`kfx_config`** | `conf` (`struct Configs`) | See [stage-04-kfx-config.md](stage-04-kfx-config.md) — this is the same struct discussed there; decide the ownership/pointer strategy once, apply it to both `game.conf` and any other config embed found later. |
| **`kfx_platform`** | `is_running_under_wine`, `FatalError` (from `keeperfx.hpp`) | OS/environment/crash state, not game-domain. |

### `StartupParameters`/`GameTime` (in `keeperfx.hpp`, not `struct Game`)

- `struct StartupParameters` (23–26 fields, command-line/config-derived
  boot settings) → `kfx_config`.
- `struct GameTime` (`get_game_time(turns, fps)`, converts turn count to
  elapsed time) → `kfx_game` (it's turn-clock logic, independent of
  display).
- `struct TimerTime` (breaks `Timer` into H/M/S for the HUD) → `kfx_frontend`.

`keeperfx.hpp` also declares ~90 free function prototypes spanning
game-loop orchestration (`setup_game`, `game_loop`, `update`,
`check_players_won/lost`, `process_payday`, `engine()`, `lose_level`/
`resign_level`/`complete_level`, …) — essentially the public API of the
whole game loop. These belong with `kfx_game` (stage 9), not scattered;
`keeperfx.hpp` itself should shrink to whatever remains genuinely tied to
`main.cpp` (see [stage-12-slim-app-target.md](stage-12-slim-app-target.md)).

### Related, but a separate struct: `PlayerInfo` embeds `Camera` by value

Not part of `struct Game`, but the same disease: `struct PlayerInfo`
(`player_data.h`) embeds `struct Camera cameras[4]` by value. Since
`player_data.h` is included almost everywhere, this alone transitively
leaks `engine_camera.h`'s render globals (`zoom_distance_setting`, etc.)
into net and frontend code with no explicit include needed. This is
significant enough to affect stage 7's scoping — see
[stage-07-kfx-render.md](stage-07-kfx-render.md) for the detail and fix
options; it's called out here only so it isn't missed when `player_data.h`
itself is touched.

## Decomposition strategy — decided

Two viable approaches were weighed, not mutually exclusive:

1. **Composed top-level object.** `struct Game` becomes a thin struct of
   pointers/references to per-library owned sub-structs (`sim_state*`,
   `net_state*`, `render_state*`, `game_state*`, `frontend_state*`,
   allocated/owned by their respective library). Code that currently does
   `game.dungeon[i]` becomes `sim_state->dungeon[i]` or a thin accessor.
   Pro: clean ownership, easy to reason about. Con: touches every call
   site that reads `game.*` (thousands, codebase-wide) — needs a mechanical
   rename pass, ideally scripted.
2. **Incremental field migration.** Move one field group at a time into
   its destination library's own struct, leaving a forwarding
   reference/macro in `struct Game` until all call sites in that group are
   migrated, then remove the forwarding shim. Pro: safely incremental,
   fits this plan's "never break the build" principle. Con: `struct Game`
   stays partially god-object-shaped for longer, and forwarding shims are
   themselves a coupling point that needs cleanup tracking.

**Decided: incremental (option 2), field-group by field-group, timed to
match stages 6–10** — migrate the `kfx_sim`-owned fields when stage 6
lands, the `kfx_render`-owned fields when stage 7 lands, and so on, rather
than doing all of struct Game in one pass. This is why this stage is
positioned *before* stage 6 in the index but its actual field-moves are
expected to complete alongside stages 6–10, not entirely before them —
treat this document as the design reference those stages point back to.

## Exit criterion

Field-group ownership table above is agreed by a maintainer (including the
decomposition strategy decision) — **done**, confirmed 2026-08-02. No code
needs to move yet — stage 6 onward each claim and migrate their field
group as they land, referencing this document.
