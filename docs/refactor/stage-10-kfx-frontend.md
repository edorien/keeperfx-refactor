# Stage 10 — Extract `kfx_frontend`

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

`front_*`, `frontmenu_*`, `gui_*` — 66 files (57 non-`_data` +
9 `_data.cpp`), 30,992 LOC, the single largest cluster by file count. Plus
`button_snapping.*`, `kjm_input.*`, `local_camera.*` (reclassified here
from the original "uncategorized" bucket by content, not naming
convention). Depends on `kfx_game`, `kfx_render`, `kfx_config`,
`kfx_platform`.

## Internal layering: `gui_*` is a real lower sub-layer

Header-level, this is already clean: **zero** `gui_*.h` includes
`front_*.h`/`frontmenu_*.h`, and zero `front_*.h`/`frontmenu_*.h` includes
`gui_*.h` (headers only look "down" or sideways within the cluster, never
"up" into the other side).

At the `.c`/`.cpp` implementation level the expected direction dominates:
22 `front_*`/`frontmenu_*` files include a `gui_*.h` (e.g. `front_input.c`
pulls in six different `gui_*.h` headers). The reverse direction exists but
is small — 7 include lines across 7 `gui_*.c` files:

| `gui_*.c` file | Includes | Real symbol used | Verdict |
|---|---|---|---|
| `gui_frontbtns.c` | `front_input.h` | `LbBFeF_IntValueMask` | Real — move the flag constant down into a shared/`gui_*` header. |
| `gui_tooltips.c` | `front_input.h` | `LbBFeF_NoTooltip` | Same fix as above. |
| `gui_frontmenu.c` | `front_input.h`, `frontmenu_ingame_evnt.h` | `game_is_busy_doing_gui_string_input()` | Real — move this query function down. |
| `gui_draw.c` | `front_simple.h` | `copy_raw8_image_buffer()` | Real — move down or expose as a render-owned primitive. |
| `gui_parchment.c` | `front_simple.h`, `frontmenu_ingame_tabs.h` | `copy_raw8_image_buffer()`, `gui_room_type_highlighted` | Real, same two fixes as above. |
| `gui_msgs.c` | `frontmenu_ingame_evnt.h` | `bonus_timer_enabled()`, `script_timer_enabled()`, `display_variable_enabled()` | Real — move these three query functions down. |
| `gui_boxmenu.c` | `front_input.h` | none found | **Dead include — delete**, removes one of the two vestigial reverse-edges for free. |
| `gui_draw.c` | `frontmenu_ingame_tabs.h` | none found | **Dead include — delete**, the other vestigial one. |

Fix: move the ~5 shared flag constants (`LbBFeF_*`) and cross-cutting query
functions (`bonus_timer_enabled`, `script_timer_enabled`,
`display_variable_enabled`, `game_is_busy_doing_gui_string_input`,
`gui_room_type_highlighted`, `copy_raw8_image_buffer`) down into a `gui_*`
(or new shared) header, and delete the 2 dead includes. After that, the
ordering **`gui_*` (lower) → `front_*`/`frontmenu_*` (higher)** is
strictly true, not just mostly true, and can be enforced by the stage 0
dependency-graph script as an internal sub-boundary within `kfx_frontend`.

Relocating `gui_room_type_highlighted` into `gui_draw.h` here surfaces a
separate, worse edge one layer down: `kfx_sim`'s `roomspace.c` writes that
same variable directly, and (with `front_input.h`/`gui_frontmenu.h`/
`frontmenu_ingame_evnt.h`) reaches up into `kfx_frontend` from *below*
`kfx_game`/`kfx_render`/`kfx_net` in the dependency order. Out of this
stage's scope — see
[stage-06b-sim-frontend-leaks.md](stage-06b-sim-frontend-leaks.md).

## `_data.cpp` files: mostly free, three exceptions

9 files in this cluster follow a `*_data.cpp` naming pattern (plus 3
outside it in other clusters: `engine_render_data.cpp`,
`player_computer_data.cpp`, `vidmode_data.cpp`, already accounted for in
stages 7/6). Checked each for real logic vs. pure data:

**Pure data, zero logic — safe to bundle with near-zero risk:**
`frontmenu_ingame_tabs_data.cpp`, `front_torture_data.cpp`,
`frontmenu_ingame_evnt_data.cpp`, `frontmenu_options_data.cpp`,
`frontmenu_saves_data.cpp`, `frontmenu_select_data.cpp` — all just
`GuiButtonInit[]`/`GuiMenu` array literals or similar static tables.

**Not pure data — flagged for individual review:**
- `front_lvlstats_data.cpp` — mostly a `StatsData[]` table, but also
  defines `stat_return_c_slong(void *ptr)`, a real accessor function
  referenced by every table row via function pointer.
- `frontmenu_ingame_opts_data.cpp` — mostly tables, but defines
  `static void no_op(struct GuiButton*) {}`, an intentionally-non-NULL
  stub the source comments explain is needed so controller-snapping logic
  doesn't ignore the button — a real (if trivial) behavioral dependency.
- **`frontmenu_net_data.cpp` (264 lines) — the clear outlier.** Besides the
  usual tables, it defines four substantial functions with real logic and
  global-state mutation: `frontnet_draw_session_selected()`,
  `frontnet_session_select()` (mutates `net_session_index_active`/
  `net_session_index_active_id`), `frontnet_draw_session_button()`, and
  `frontnet_session_create()` (string parsing/dedup, calls
  `LbNetwork_Create`, `process_network_error`, `frontend_set_player_number`,
  `frontend_set_state`). **Do not treat this file as risk-free static
  data** — it has real network-session business logic that needs the same
  scrutiny as any other `.c` file when this cluster moves.

## Prerequisite from stage 5

Migrate the `kfx_frontend`-owned field group from `struct Game`
(`messages[]`, `quick_messages[][]`, `evntbox_*`, `flash_button_*`,
`active_panel_mnu_idx`, `box_tooltip[][]`, `fx_lines[]`/`active_fx_lines`,
`heart_lost_*`, `comp_player_*`/`creatures_tend_*`, `loaded_swipe_idx`,
`active_messages_count`, `computer_chat_flags`, `gui_cheat_box_1..4`) and
the frontend-owned globals from `keeperfx.hpp` (mouse/pointer tracking,
`continue_game_option_available`, `define_key_scroll_offset`,
`time_last_played_demo`, `drag_menu_x/y`, tooltip timers,
`top_of_breed_list`/`no_of_breeds_owned`, `level_names_data`,
`timerstarttime`/`Timer`, `TimerTime`).

## Sequencing

Given the size, split into sub-PRs the same way as stage 6: `gui_*` first
(it's the lower sub-layer and the smaller fix-set), then `front_*`, then
`frontmenu_*`, with `frontmenu_net_data.cpp` reviewed individually rather
than swept in with the other `_data.cpp` files.

## Exit criterion

`libs/kfx_frontend` exists. The `gui_*` → `front_*`/`frontmenu_*` ordering
is real (not just mostly-real) and enforced. `frontmenu_net_data.cpp`'s
logic has been reviewed with the same rigor as non-data files.
