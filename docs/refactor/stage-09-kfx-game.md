# Stage 9 — Extract `kfx_game`

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

`game_heap.*`, `game_lghtshdw.*`, `game_loop.*`, `game_merge.*`,
`game_saves.*`, `lvl_script*`, `console_cmd.*`, `sounds.*` (12 + 14 + 2
files). Depends on `kfx_sim`, `kfx_render`, `kfx_net`, `kfx_config`.
`main_game.c` (level/session lifecycle: `init_level`,
`startup_network_game`, `clear_complete_game`, `init_seeds`) belongs here
too — distinct from `main.cpp`, which is the actual process entry point
(see [stage-12-slim-app-target.md](stage-12-slim-app-target.md)).

## Boundary violations (game reaching into frontend) and fixes

25 include lines across 9 files reach from `game_*`/`lvl_script*` into
`front_*`/`frontmenu_*`/`gui_*`. Most are legitimate but currently direct
calls rather than an inverted registration:

| File | Header | Symbol(s) | Note |
|---|---|---|---|
| `game_saves.c` | `front_simple.h` | `engine_palette` | Legitimate (render-owned global, see stage 7's accessor-API table). |
| `game_saves.c` | `frontmenu_ingame_tabs.h` | `update_trap_tab_to_config()`, `update_room_tab_to_config()` | Legitimate — same config-reload-propagation pattern as stage 4. |
| `game_saves.c` | `front_highscore.h` | `high_score_entry` global | Legitimate. |
| `game_saves.c` | `front_lvlstats.h` | `frontstats_initialise()` | Legitimate. |
| `game_saves.c` | `gui_soundmsgs.h` | `output_message(SMsg_GameLoaded, 0)` | Legitimate — "emit event, gui renders" pattern from stages 7/8 applies here too. |
| `game_saves.c` | `frontmenu_ingame_map.h` | `panel_map_update()` | Legitimate. |
| `game_saves.c` | `front_landview.h`, `gui_boxmenu.h` | none found | **Dead includes — delete.** |
| `main_game.c` | `front_network.h` | `setup_alliances()` | Legitimate, and arguably fine to stay here since `main_game.c` is the top-level orchestrator tying frontend/network/simulation together each session — see note below. |
| `main_game.c` | `frontmenu_ingame_tabs.h`, `frontmenu_ingame_map.h`, `gui_topmsg.h`, `gui_soundmsgs.h` | `update_powers/room/trap_tab_to_config()`, `setup_panel_colors()`, `erstats_clear()`, `show_onscreen_msg()`, `clear_messages()` | Legitimate, same orchestrator note. |
| `main_game.c` | `frontmenu_ingame_evnt.h`, `gui_boxmenu.h` | none found | **Dead includes — delete.** |
| `lvl_script_value.c` | `frontmenu_ingame_map.h` | `panel_map_update()` | Legitimate. |
| `lvl_script_value.c` | `gui_soundmsgs.h` | none found via direct grep | Likely dead/indirect — verify and delete if confirmed. |
| `lvl_script_lib.h` (header-level!) | `frontmenu_ingame_tabs.h` | `get_button_designation()`, `gui_set_button_flashing()`, `update_powers_tab_to_config()` | Structurally significant: this header is shared by **every** `lvl_script_*.c` file, so all of them transitively pull in a frontend GUI header just to flash panel buttons/resolve tab designations from script commands. Narrow this to the 2–3 functions actually needed, exposed via a small `kfx_frontend`-owned "script hooks" header instead of the full tabs header. |
| `game_merge.h` (header-level) | `gui_msgs.h` | none found in `game_merge.h`/`.c` | **Dead include at header scope — delete**, shrinks every `game_merge.h` includer's transitive closure. |
| `game_heap.c` | `front_simple.h` | none found | **Dead include — delete.** |
| `lvl_script_commands.c` | `gui_soundmsgs.h`, `gui_frontmenu.h`, `frontmenu_ingame_map.h` | `script_play_message()`, `turn_off_menu()`/`turn_on_menu()` | Legitimate — script commands directly manipulating GUI state is part of the level-scripting feature set. |

**`main_game.c`'s frontend/net coupling is different in kind from the
rest**: it's the per-session orchestrator that ties frontend, network, and
simulation together each frame/session, so depending on all three is
arguably correct *for that one file* — consider whether it should live at
an "app/orchestration" layer above `kfx_game` rather than being held to
the same "no frontend includes" bar as the rest of the library. Revisit
once stage 12 clarifies what's left in `main.cpp`.

**Reverse direction (expected, confirms `game_legacy.h`'s god-header
status independently of stage 5's fix):** 31 frontend files include one of
the `game_*.h` headers, and 30 of those 31 go through `game_legacy.h`
specifically (`game_merge.h` next-highest at only 5). This ratio is exactly
why stage 5's `struct Game` decomposition matters — once it's done, this
30:5 imbalance should flatten out because frontend code will reach for the
specific per-library header it actually needs instead of the one header
that happens to include everything.

## Prerequisite from stage 5

Migrate the `kfx_game`-owned field group from `struct Game` (`script` +
`script_*`, `event[]`, `action_points[]`, `game_kind`, `play_gameturn`,
`pckt_gameturn`, `selected_level_number`, `continue_level_number`,
`loaded_level_number`, `campaign_fname`, `last_level`, `frame_skip/step`,
`paused_at_gameturn`, `mode_flags`, `operation_flags`, `easter_egg*`,
`heart_lost_*`, `triggered_object_location`) and the `kfx_game`-owned
globals from `keeperfx.hpp` (`level_name`, `default_loc_player`,
`TimerGame`/`TimerNoReset`/`TimerFreeze`, `turns_per_second`,
`exit_keeper`/`quit_game`), plus the ~90 game-loop function prototypes
currently declared in `keeperfx.hpp`.

## Exit criterion

`libs/kfx_game` exists. The dead includes above are removed. Every
remaining game→frontend edge in the table is either accepted as legitimate
(config-reload propagation, event-emission pattern) or narrowed (the
`lvl_script_lib.h`→`frontmenu_ingame_tabs.h` header-level dependency in
particular). `main_game.c`'s orchestration-layer status is resolved one
way or the other before stage 12.
