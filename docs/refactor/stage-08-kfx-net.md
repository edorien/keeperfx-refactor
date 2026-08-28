# Stage 8 — Extract `kfx_net`

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

`net_*`, `packets*` (29 files, 9,738 LOC). Should depend on `kfx_sim`
(serializing/applying simulation state), `kfx_config`, `kfx_platform`.
Expected and confirmed-fine: the many `config_*.h`/`thing_*.h`/`room_*.h`/
`map_*.h`/`player_*.h`/`creature_*.h` includes across `net_checksums.c`,
`packets.c`, `packets_input.c`, `packets_cheats.c` — this is net code
doing exactly its job (serializing/validating/applying sim state).

## Resolves the bflib ⇄ net cycle from stage 2

Stage 2 found `bflib_enet.cpp` depends on `net_portforward.h`/
`net_holepunch.h`/`net_matchmaking.h`, and this stage's investigation
confirmed the reverse: `net_portforward.cpp`/`net_holepunch.c`/
`net_matchmaking.c` themselves `#include bflib_basics.h`/
`bflib_netsession.h`. Not a literal `#include` cycle (neither bflib header
references anything net-related back), but a real architectural one.
Stage 2's fix (a "connectivity services" callback interface bflib calls
into) resolves bflib's side. This stage's job is making sure
`net_portforward.cpp`/`net_holepunch.c`/`net_matchmaking.c` only need
`bflib_basics.h`/`bflib_netsession.h` for genuinely low-level types — that
part was already confirmed clean (no game-domain reach from the net side
of the cycle).

## Boundary violations (net reaching upward) and fixes

| File | Reaches into | Symbols | Fix |
|---|---|---|---|
| `net_game.h` (public header!) | `front_network.h` | `enum FrontendNetService`, used in `setup_network_service()`'s public signature | Move `FrontendNetService` into a net-owned header (e.g. `net_main.h`); have `front_network.h` include *that*, inverting the dependency. This is the most important single fix — it's a public-header-level dependency, not just an implementation detail. |
| `net_game.c` | `frontend.h` | `frontend_set_state(FeSt_NET_SESSION)` | Net should raise an event/callback the frontend subscribes to and reacts to with its own state change, not drive frontend menu state directly. |
| `net_game.c` | `front_network.h` | `fe_network_active`, `net_service_index_selected`, `net_session[]`, `net_session_index_active`, `net_player[]`, `net_player_name`, `process_network_error()`, `display_attempting_to_join_message()`, `reset_attempting_to_join_cancel()` | Heaviest single violation: net both reads *and writes* UI-owned session globals directly. These should live in a net-owned "session state" struct with accessors; `front_network.h`'s UI depends on *that*. |
| `net_game.c` | `gui_msgs.h` | `message_add()`/`message_add_fmt()` (5 sites — desync/AI-takeover/disconnect notices) | Net emits an event/log record; gui (already listening to net) converts it to a chat message. |
| `net_game.c` | `custom_sprites.h` | `required_sprite_zip_checksums[]` (read+write) | Go through an accessor exposed by `custom_sprites` (render). |
| `net_lobby.c`, `net_exchange_common.c` | `front_landview.h` | `net_screen_packet` (a `struct ScreenPacket` global) | `net_screen_packet` is actually network wire data, not landview UI state — move its declaration to a net-owned header. |
| `net_exchange_common.c` | `frontend.h` | `frontend_menu_state`, `FeSt_START_MPLEVEL` (branches on this to gate exchange behavior) | Replace with a net-local "is game starting" flag the frontend sets explicitly once, rather than net reading frontend UI state. |
| `net_exchange_gameplay.c` | `engine_redraw.h` | `keeper_screen_redraw()` | Set a "needs redraw" flag the render loop polls, don't call into render synchronously from net. |
| `net_exchange_gameplay.c` | `gui_msgs.h` | `message_add()` | Same "emit event, let gui render" fix as `net_game.c` above. |
| `net_exchange_gameplay.c` | `lua_triggers.h` | `lua_on_chatmsg()` | Legitimate integration conceptually, but means net must link lua. Route through a game-layer event dispatcher instead so net doesn't need `lua_triggers.h` at all. |
| `net_resync.cpp` | `lens_api.h` | `reset_eye_lenses()` | Expose as a "post-resync render reset" hook render registers, not a direct call. |
| `net_resync.cpp` | `lua_base.h` | `lua_get_serialised_data()`, `lua_set_serialised_data()`, `lua_set_random_seed()` | Full-resync packets embed serialized Lua VM state, so this coupling may be unavoidable — if kept, isolate behind a narrow `lua_resync_export()`/`import()` pair rather than three separate `lua_base` entry points. |
| `packets.c` | `engine_camera.h` | `view_set_camera_y_inertia()`, `view_set_camera_x_inertia()`, `view_set_camera_rotation_inertia_around()`, `view_set_camera_tilt()` (6+ call sites) | Expected in principle (packets *are* player input applied to camera), but consolidate behind one `camera_apply_input(cam, packet)` render-owned function instead of scattered individual calls. |
| `packets.c` | `engine_redraw.h` | `set_engine_view()` (2 sites) | Same consolidation. |
| `packets.c` | `gui_topmsg.h`, `gui_parchment.h`/`frontmenu_ingame_map.h` | `show_onscreen_msg()`, `panel_map_update()` | "Emit event, let gui render" fix, same pattern throughout this stage. |
| `packets_misc.c` | `front_landview.h`, `frontend.h`, `gui_topmsg.h` | `erstat_inc()`, `is_onscreen_msg_visible()`, `show_onscreen_msg()` | Same fix. |

Also found (redundant/dead, delete as a free first pass): `packets.c`
redeclares `extern int frontend_menu_state;` inline (line ~1642) despite
already including `frontend.h`, which declares it — remove the redundant
inline extern. Several includes across `net_game.c`, `net_lobby.c`,
`packets.c`, `packets_input.c` show no confirmed symbol usage
(`front_landview.h` in `net_game.c`; `gui_frontmenu.h`/`gui_soundmsgs.h` in
`packets.c`; `lua_triggers.h` in `packets.c`; `front_input.h`/
`engine_render.h` in `packets_input.c`) — verify and delete.

## Prerequisite from stage 5

Migrate the `kfx_net`-owned field group from `struct Game` (`packets[]`,
`packet_save_*`/`packet_load_*`, `turns_stored`, `turns_fastforward`,
`turns_packetoff`, `local_plyr_idx`, `input_lag_turns`,
`active_players_count`, `host_checksums`, `log_snapshot`,
`neutral_player_num`).

## Exit criterion

`libs/kfx_net` exists. `net_game.h` (the public header) no longer depends
on `front_network.h`. Net no longer reads/writes frontend-owned globals
directly — all interactions go through the accessor/event patterns above.
