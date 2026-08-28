# Stage 11 — Extract `kfx_script`

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

`lua_*` (22 files, 7,855 LOC) and `api.c`/`api.h` (a JSON-over-SDL_net
external API server, `api_init_server`/`api_event`, that exposes/subscribes
to game events and runs `lvl_script` commands for external tools —
classified with the scripting layer, not networking, since its purpose is
external automation, not multiplayer). This layer is *supposed* to reach
into everything — "no upward includes" isn't the right test here, unlike
every other stage.

## Measured API surface — where the width actually is

Every distinct non-lua, non-bflib project header reached by `lua_*`/`api.*`
files, grouped by target library:

| Target library | Distinct headers reached | Scope |
|---|---|---|
| `kfx_sim` | **23** | `thing_data.h`, `thing_navigate.h`, `thing_creature.h`, `thing_effects.h`, `thing_shots.h`, `thing_physics.h`, `thing_stats.h`, `creature_states.h`, `creature_states_pray.h`, `creature_states_mood.h`, `room_library.h`, `room_util.h`, `room_data.h`, `map_data.h`, `map_blocks.h`, `map_locations.h`, `player_utils.h`, `player_data.h`, `player_instances.h`, `dungeon_data.h`, `power_specials.h`, `slab_data.h`, `magic_powers.h` |
| `kfx_config` | **13** | `config_translation.h`, `config_campaigns.h`, `config.h`, `config_creature.h`, `config_crtrstates.h`, `config_keeperfx.h`, `config_terrain.h`, `config_magic.h`, `config_effects.h`, `config_objects.h`, `config_trapdoor.h`, `config_rules.h`, `config_mods.h` |
| `kfx_game` | **6** | `lvl_script_lib.h`, `lvl_script.h`, `lvl_script_commands.h`, `lvl_script_value.h`, `game_merge.h`, `sound_manager.h` |
| `kfx_render` | **3–4** | `lens_api.h`, `engine_textures.h`, `custom_sprites.h`, and `local_camera.h` (ambiguous, used only by `lua_api_things.c`) |
| `kfx_frontend` | **2–3** | `gui_msgs.h`, `gui_soundmsgs.h`, and `console_cmd.h` (ambiguous, used only by `api.c`) |
| `kfx_net` | **0** | No `lua_*`/`api.*` file reaches into `net_*`/`packets*` at all today. |
| Cross-cutting, not owned by a single library | 1 | `value_util.h` (generic TOML value/variant utility, used by `api.c`) |

**This defines the priority order for narrowing the scripting API surface**
if that work is ever done: `kfx_sim` (23 headers) and `kfx_config` (13
headers) are where narrowing would actually matter; `kfx_render` (3-4) and
`kfx_frontend` (2-3) are already narrow; `kfx_net` costs nothing to make
narrow since there's nothing to narrow — a future `kfx_net` public API
would be adopted here for free.

## Recommended approach: pragmatic, not full API purity

Full narrow-API discipline (bind only through purpose-built headers) is
proportional to this cluster's size (7,855 LOC) but the payoff is
speculative unless the scripting API needs to be versioned/stabilized for
mod compatibility. For this stage:

1. Do the mechanical library-boundary work (this cluster becomes
   `libs/kfx_script`, linked against `kfx_sim`, `kfx_config`, `kfx_game`,
   `kfx_render`, `kfx_frontend`).
2. Document the current reach (the table above) as "this is the de facto
   scripting API surface today" — a snapshot other stages can check against
   so they don't accidentally widen it further without noticing.
3. Do **not** attempt to narrow the 23-header `kfx_sim` surface or the
   13-header `kfx_config` surface as part of this stage. Revisit only if a
   concrete need for a stable/versioned scripting API surface emerges
   (e.g. mod compatibility guarantees).
4. Do apply the resync-specific narrowing already called out in
   [stage-08-kfx-net.md](stage-08-kfx-net.md): `net_resync.cpp`'s direct
   calls to `lua_get_serialised_data()`/`lua_set_serialised_data()`/
   `lua_set_random_seed()` should go through a narrow
   `lua_resync_export()`/`import()` pair rather than three separate
   `lua_base` entry points — this is small, concrete, and worth doing now
   since it's the one place `kfx_net` needs to reach into `kfx_script`.

## Exit criterion

`libs/kfx_script` exists and links against `kfx_game`, `kfx_sim`,
`kfx_render`, `kfx_frontend`, `kfx_config`. The measured-surface table
above is committed to the repo (e.g. as a comment block or checked-in
snapshot) so future stages can detect if they've silently made the
scripting layer's reach wider without an explicit decision to do so.
