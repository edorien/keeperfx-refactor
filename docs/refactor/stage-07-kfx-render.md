# Stage 7 — Extract `kfx_render`

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

`engine_*`, `lens_api.*`, `light_data.*`, `vidmode*`, `vidfade.*`,
`spritesheet.cpp`, `custom_sprites.*`, `sprites.h`, `scrcapt.*` (13+
files, ~13,464 LOC in the `engine_*` core alone). Should depend on
`kfx_sim` (reads camera/map/thing state to draw it) and `kfx_platform`
only. `cursor_tag.*` is a borderline sim/render hybrid (validates dig/
build/steal placement against dungeon/room/player state, then draws the
overlay) — its logic belongs closer to `kfx_sim` but its one external call
(`draw_map_volume_box()`) is render; default to placing it here since the
render call is what makes it render-adjacent, revisit if it grows more
rule-checking logic.

Several files have **zero** cross-cluster includes today: `spritesheet.cpp`,
`engine_lenses.c/.h`, `engine_arrays.h`, `engine_camera.h`,
`engine_textures.h`, `vidfade.h`, `lens_api.h`, `engine_render_data.cpp`.

## The structural blocker: `PlayerInfo` embeds `Camera` by value

This is the single most consequential finding for this stage.
`player_data.h` (line 24) and `dungeon_data.h` (line 29) both
`#include "engine_camera.h"` because `struct PlayerInfo` embeds
`struct Camera cameras[4]` **by value** (`player_data.h:175`). Since
`player_data.h` is included by nearly every file in the codebase (front,
gui, game, net, lua), this one embedding transitively exposes *all* of
`engine_camera.h`'s globals and the `Camera`/view-mode type definitions
codebase-wide, regardless of whether a file explicitly includes
`engine_camera.h`.

**No clean `kfx_render` boundary is possible while this embedding exists.**
Two fixes, either sufficient on its own:
(a) move `struct Camera`'s definition to a lower-layer header both
`player_data.h` and `engine_camera.h` depend on, or
(b) change `PlayerInfo::cameras` to an opaque/pointer member so
`player_data.h` no longer needs the full render-camera definition.
Recommend (b) — it's the smaller change and matches this plan's general
preference for accessor/opaque-handle indirection over reshuffling type
ownership.

## Render globals that need an explicit accessor API

Because net and gui/frontend code write directly to render-owned globals
today (not through function calls), `kfx_render` cannot simply make these
internal statics — each needs an explicit setter/getter before the raw
`extern` can be removed from public headers:

| Global | Declared in | Written directly by |
|---|---|---|
| `zoom_distance_setting`, `frontview_zoom_distance_setting` | `engine_camera.h` | `net_game.c` (read+write, startup-sync exchange), `packets.c` (read) |
| `box_lag_compensation_x`/`_y` | `engine_render.h` | `packets.c` (`update_box_lag_compensation()`, 4 write sites) |
| `map_volume_box` | `engine_render.h` | `packets_input.c` (`.visible = 0`) |
| `keepsprite[]`, `sprite_heap_handle[]`, `jty_file_handle` | `engine_render.h` | `game_heap.c` (reset/setup) |
| `pixmap`, `alpha_sprite_table`, `colours` | `vidmode.h`/`vidfade.h` | `gui_parchment.c`, `gui_draw.c`, `gui_boxmenu.c` |
| `fade_palette_in`, `frontend_palette` | `vidfade.h` | `front_landview.c`, `front_credits.c`, `front_fmvids.c`, `front_torture.c` |
| `poly_pool` | `engine_render.h` | `gui_parchment.c` |
| `block_mem` | `engine_textures.h` | `front_torture.c`, `front_landview.c` |
| `required_sprite_zip_checksums[]` | `custom_sprites.h` | `net_game.c` (startup sprite-zip-checksum sync) |

Proposed API shape: `render_set_zoom_distance_setting()`,
`render_get_pixmap_fade_table()`, `render_reset_lag_compensation()`,
`render_set_map_volume_box_visible()`, one per global above.

## Boundary violations (render reaching upward) and fixes

`engine_redraw.c` is the concentration point — it drives the whole-screen
redraw and, in doing so, reaches directly into GUI, frontend, power-hand,
and even net/packet state:

| Reached header | Symbols | Fix |
|---|---|---|
| `gui_parchment.h`, `gui_draw.h`, `gui_boxmenu.h`, `gui_msgs.h`, `gui_tooltips.h` | `redraw_parchment_view()`, `draw_slab64k()`, `gui_draw_all_boxes()`, `message_draw()`, `draw_tooltip()`, etc. | Register these as render-frame debug/overlay callbacks that gui registers into, instead of `engine_redraw.c` calling up. |
| `power_hand.h` | `power_hand_is_empty()`, `draw_power_hand()` | Query hand state via a thin `kfx_sim` accessor; keep the actual icon draw inside render, driven by data not a `power_*` call. |
| `frontmenu_ingame_tabs.h`, `frontmenu_ingame_evnt.h` | `draw_whole_status_panel()`, plus **16 distinct debug-overlay symbols** (`draw_bonus_timer`, `draw_timer`, `draw_frametime`, `draw_gameturn_timer`, `draw_consolelog`, `draw_network_stats`, `debug_display_network_stats` (a **net**-layer global!), script/lua-adjacent debug drawing) | The heaviest single violation in the codebase's render code. Register all of these as render-layer debug-draw callbacks instead. |
| `front_easter.h` | `draw_eastegg()` | Frontend-registered overlay. |
| `frontend.h` | `winfont` global (5 uses) | Pass in / set via a render-state-setter API. |
| `packets.h` | `unpausing_in_progress` global | Replace with a render-facing `is_unpausing()` accessor owned by game/sim state, not a direct read of a net-layer flag. |

Elsewhere in the cluster: `vidmode.c`/`vidmode_data.cpp`/`custom_sprites.c`
populate frontend sprite-sheet globals (`font_sprites`, `frontend_font`,
`button_sprites`, `winfont`, `gui_panel_sprites`, `frontend_sprite`,
`hires_parchment`) directly — invert to a
`frontend_register_sprite_sheets()` call the frontend makes into render.
`custom_sprites.c` also depends on `net_checksums.h` for
`calculate_file_checksum()` — this is a general file-hashing utility
misfiled under the net cluster; move it to a `kfx_platform` utility header.
`engine_textures.c`/`vidmode.c`/`lens_api.c`/`engine_render.c` all pull in
`game_legacy.h` for `game.texture_animation[]`/`game.lish`/`game.mode_flags`
etc. — resolved by stage 5's render field-group migration, not separately
here.

Several includes across this cluster (`engine_camera.c`'s
`frontmenu_ingame_map.h`, `engine_render.c`'s `front_simple.h`/`frontend.h`,
`engine_arrays.c`'s and `engine_textures.c`'s `front_simple.h`, `vidmode.c`'s
`game_heap.h`) show **no confirmed symbol usage** — dead includes, delete
as a free first pass, same pattern as stages 2 and 4.

## Prerequisite from stage 5

Migrate the `kfx_render`-owned field group from `struct Game` (`lish`,
`texture_animation[]`, `texture_id`, `top_cube[]`, `active_lens_type`/
`applied_lens_type`, `small_map_state`, `mouse_light_pos`,
`ceiling_height*`, `fps_limit_*`) and the render-owned globals from
`keeperfx.hpp` (the palette set, `optimised_lights`/`total_lights`/
`do_lights`).

## Exit criterion

`libs/kfx_render` exists, depends only on `kfx_sim` + `kfx_platform`.
`PlayerInfo::cameras` no longer forces every `player_data.h` includer to
see `engine_camera.h`. Every global in the accessor-API table above is
reached through a function, not a raw `extern`, from outside the library.
