# Phase 3 — messages, tooltips, the event/battle info box

Status: **landed 2026-09-06 — Paused caption, onscreen banner, message queue (incl.
`show_game_time_taken`, which just `targeted_message_add`s), `text_info_menu`, `battle_menu`,
tooltips, MP chat line. Only the spell-cost-under-cursor number is deferred (`draw_spell_cost` is a
`static` in `engine_redraw.c` consumed on draw — needs a `render_overlay` setter to hand to the
frontend; low priority).** Depends on [01-seam-and-toggle.md](01-seam-and-toggle.md); after
[03-debug-overlays-and-box-menus.md](03-debug-overlays-and-box-menus.md).

**Phase 3a — what landed:** new module `frontgui_ingame_text.{h,cpp}`,
`ingame_text_overlays_frame()` called from `ingame_imgui_frame()`.
- **"Paused" caption** — `engine_redraw.c:1011`'s block gains a leading `!RendererImGuiEnabled()`;
  the ImGui version is a centred borderless `NoInputs` window near the top of the 3D viewport
  (`player->engine_window_x` inset), heading font, same `GOF_Paused && !GOF_WorldInfluence &&
  !unpausing_in_progress` gate (`unpausing_in_progress` read straight from `packets.h`).
- **Onscreen warning banner + OUT OF SYNC / SEED OUT OF SYNC** — `gui_topmsg.c::
  draw_onscreen_direct_messages()` keeps running its `erstat_check()` poll (WARNLOG side effects)
  and `render_onscreen_msg_time` decay unchanged; its three `LbTextDrawResized` calls are guarded
  `!RendererImGuiEnabled()`, and it stashes its show/no-show verdict in a file-static that
  `onscreen_banner_visible()` (new, `gui_topmsg.h`) exposes. `onscreen_msg_text` is now `extern`.
  The ImGui overlay draws the banner top-centre + the OOS lines in blood-red.
Both windows `NoInputs | NoNav` — no cursor / `WantCaptureMouse` involvement.

**Message queue (added to 3a):** `gui_msgs.c::message_draw()` early-returns under
`RendererImGuiEnabled()`; the per-type icon switch was extracted to `message_icon_spridx(int i)`
(`gui_msgs.h`, returns the panel-sprite index colour-remapped where the type needs it, `-1` for
none) so the ImGui side reuses it verbatim. `frontgui_ingame_text.cpp::draw_message_queue()` walks
`kfx_sim_state.messages[0..active_messages_count)`, same `target_idx == my || -1` filter, one row
each: icon via the new `FeGuiPanelTexture()` (a second index-keyed cache in `frontgui_sprite_tex`,
separate from the button one since GPS/GBS index spaces overlap) + wrapped `TextUnformatted`.
Anchored at `status_panel_width + 8`, no background plate (matches the legacy text-on-3D look).
`NoInputs`. Real-time fade / expiry is unchanged — still driven by `message_update()` in the sim.

**`text_info_menu` (objective / event box, added to 3a):** `GMnu_TEXT_INFO` added to
`menu_is_migrated()` — a *non*-monopoly migrated menu, the first one. `draw_active_menus_buttons`
+ the per-button `get_gui_inputs` skip already cover it (keyed on `ingame_imgui_menu_active`).
`textinfo_frame()` (`frontgui_ingame.cpp`): bottom-centre `ImGui::Begin`, a **left column of the
original message-box icon buttons** (`GPS_message_message_btn_show_act` zoom,
`GPS_message_message_btn_accept_act` close, via the new `FeGuiPanelIconButton()`) beside a
`FeBeginScrollArea` showing `kfx_sim_state.evntbox_scroll_window.text` — the classic layout. Both
buttons deferred (`gui_go_to_event` / `gui_close_objective`, null `GuiButton*` which they ignore).
`frontgui_sprite_tex` gained `FeGuiPanelButton` / `FeGuiPanelIconButton` (panel-sprite versions of
`FeSpriteButton`, sharing a refactored `sprite_button_body`) — the same icon-button pattern is used
for `battle_menu` and any future message box. **Icon sprites: use the `GPS_*_std` frame, not
`GPS_*_act`** — `gui_area_new_normal_button()` draws `sprite_idx + 1` (the `_std` idle frame) at
rest; the `_act` frame is a bright near-white pressed highlight (found live: "the zoom/close icons
are all white").
Per §5 the text stays sim-authoritative; ImGui owns its own scroll — the legacy line-unit
`.start_y` / `.action` are *not* synced (cosmetic per-client value), and the dedicated up/down
arrow buttons are dropped in favour of the wheel + scrollbar.
**New generic seam bit:** `ingame_imgui_wants_mouse()` (`frontgui_ingame.h`) = `RendererImGuiEnabled
&& game_is_running && io.WantCaptureMouse` (one-frame lag). `get_gui_inputs()` sets
`busy_doing_gui = 1` when it's true — so a click over any non-monopoly ImGui box (event box, and
the Phase-2 cheat boxes) doesn't also dig/cast in the world behind it, without the modal
whole-screen takeover.

Goal: move all in-engine *message and info text* onto the ImGui font engine — the top-of-screen
messages, the objective/event box, the battle box, tooltips, and the loose text drawn straight
from `engine_redraw.c` ("Paused", the MP chat input line, spell-cost-under-cursor). This is the
first place [00-overview.md](00-overview.md) §5's text-boundary note on "in-engine text via message boxes → ImGui
fonts" lands. Positioning/text only; no sidebar.

---

## 1. What's in scope

| Source | Function(s) | Notes |
|---|---|---|
| `gui_msgs.c` | `message_draw()` (`:43`), `message_add*` / `message_update` / `delete_message` | the player-message queue (creature taunts, event text, `DISPLAY_MESSAGE`), scrolls up the top-left. `type` + `plyr_idx` + `timeout`. |
| `gui_topmsg.c` | `draw_onscreen_direct_messages()` (`:122`), `show_onscreen_msg()`, `erstat_check()` | the transient centred banner (`onscreen_msg_text`, `render_onscreen_msg_time`), plus error-stat spam warnings. |
| `gui_msgs.c` | `show_game_time_taken()` (`:311`), `targeted_message_add()` | end-of-level time; targeted (per-player) MP messages. |
| `frontmenu_ingame_evnt_data.cpp` | `text_info_menu` (`GMnu_TEXT_INFO`, `gui_round_glass_background`) | the objective / event detail box, bottom-centre — the dark rounded panel in the user's screenshot. Buttons: zoom-to-event, close, scroll up/down. `kfx_sim_state.evntbox_scroll_window`. |
| `frontmenu_ingame_evnt_data.cpp` | `battle_menu` (`GMnu_BATTLE`, `gui_round_glass_background`) | battle participants box — friendly/enemy battler rows (`gui_area_friendly_battlers` / `_enemy_battlers`), prev/next battle, close. Portraits via `draw_battle_head`. |
| `gui_tooltips.c` | `draw_tooltip()` (`:636`), `draw_tooltip_at`, `draw_tooltip_slab64k`, `setup_*_tooltips`, `gui_button_tooltip_update`, `input_gameplay_tooltips` | context tooltips — over panel buttons *and* over world things/traps/objects. Scrolling long tooltips (`render_tooltip_scroll_offset`). |
| `engine_redraw.c` (`kfx_render`) | inline: `GUIStr_PausedMsg` (`:1014`), MP chat input `>%s_` (`:977-990`), spell-cost / creature-level under cursor (`:992-1008`) | drawn directly with `LbTextDrawResized`; not menus. |

---

## 2. Approach

- **Message queue / banner / time-taken** — ImGui draw-list text (`GetForegroundDrawList()->AddText`)
  or a borderless `ImGui::Begin` with `ImGuiWindowFlags_NoInputs`, positioned as today, fed the
  same `game.messages[]` / `onscreen_msg_text`. Real-time fade using `kfx_render_state.delta_time`
  (already the pattern — `draw_onscreen_direct_messages` decrements `render_onscreen_msg_time` by
  `delta_time`). Font is the ImGui body face; CJK via the merged fallback fonts (stage 4 §4.2).
- **`text_info_menu` / `battle_menu`** — real ImGui windows: `FeBeginPanel` (rounded-glass style —
  add a translucent-dark variant to `frontgui_style.cpp` matching `gui_round_glass_background`),
  `FeBeginScrollArea` replacing `evntbox_scroll_window` + the up/down buttons, the zoom/close
  buttons as `FeButton`. Battle rows are a small table with `draw_battle_head` portraits →
  dynamic-texture thumbnails (same technique as the creature-query portraits, Phase 5).
- **Tooltips** — ImGui's own tooltip (`ImGui::BeginTooltip` / `SetTooltip`) for panel-button
  tooltips (Phase 5's wrappers can carry the `tooltip_stridx` and emit it). **World tooltips**
  (hovering a trap / object / room in the 3D view) are separate — `setup_trap_tooltips` /
  `setup_object_tooltips` run in `input_gameplay_tooltips()` during input, keyed on
  `cursor_moved_to_new_subtile`. Those still compute in the input path; only the *draw* moves to
  an ImGui draw-list tooltip following the cursor.
- **`engine_redraw.c` inline text** — `kfx_render` can't call ImGui (layering). Route these
  through the `render_overlay` seam: either new callback entries
  (`render_overlay->draw_paused_caption()` etc.) that the frontend implements as ImGui text, or
  fold the state (`draw_spell_cost`, the MP-message flag) into the per-frame view-model the
  frontend submission already reads. The "Paused" caption in particular
  (`GOF_Paused && !WorldInfluence && !unpausing`) is a clean single callback.

---

## 3. Entanglements / risks

- **`message_draw` reads `kfx_sim_state` / `game.messages`** — the message list is sim state,
  replayed deterministically; only rendering moves. Don't touch `message_add` / `message_update`.
- **`text_info_menu` scroll state is `kfx_sim_state.evntbox_scroll_window`** — sim-owned (it is
  `memcpy`'d in saves/resync). An ImGui scroll area can drive it but must keep it as the source of
  truth, or move it out of `kfx_sim_state` with a migration story (architecture.md §6.2). Simplest:
  keep the field, sync ImGui's scroll position to it.
- **`battle_menu` / `text_info_menu` are `POS_SCRBTM` bottom-anchored** and overlap the event/
  message box area — the ImGui layout has to reproduce the stacking (`active_messages_count`
  offsets, `engine_redraw.c:987`).
- **`toggle_tooltips()`** (`gui_tooltips.c:483`) — the tooltips-on/off setting; honour it.
- **Tooltip text can be long and scrolls** (`draw_tooltip_slab64k` viswidth clipping,
  `render_tooltip_scroll_offset`) — ImGui tooltips don't scroll; either wrap to a max width or
  keep the scroll behaviour in a custom draw.
- **`GMnu_TEXT_INFO` / `GMnu_BATTLE` are not monopoly menus** and don't block world input — the
  ImGui windows must be `NoNavInputs`-ish / not steal clicks meant for the world behind them,
  except on their own buttons. `busy_doing_gui` only when actually over the box.

## 4. Checklist

- [~] Message queue + **banner** + time-taken as ImGui text — message queue + banner + OOS done
      (3a); `show_game_time_taken` (end-of-level time) still to do.
- [x] "Paused" caption → ImGui (3a); legacy `engine_redraw.c` block guarded.
- [x] `text_info_menu` → ImGui window (3a). Text from `evntbox_scroll_window.text` (sim-owned);
      ImGui owns its own scroll, `.start_y` not synced.
- [x] `battle_menu` → ImGui window (`frontgui_ingame_battle.cpp`, 2026-09-06). 3 `visible_battles`
      rows, each friendly | fight-symbol | enemy; per battler = square creature hand-symbol icon
      (`FeGuiPanelTexture`, sized equal to the "vs" symbol) + slim ImGui health bar. The "vs" is
      centred on each row (`SameLine(cx - half_vs)`); friendly cells end before it, enemy start
      after. Hover sets `battle_creature_over` (reset per frame), L/R-click run
      `gui_get_creature_in_battle` / `gui_go_to_person_in_battle`. Left column: a single close
      icon; prev / next battle is the **mouse wheel** over the scroll area (→ deferred
      `gui_previous_battle` / `gui_next_battle`), no arrow buttons. Icon buttons use the `*_std`
      (idle) sprite frame — the `*_act` frame is a near-white "pressed" highlight (found live).
      Exp-level flower and the world-space hover highlight are deferred (minor).
- [ ] Rounded-glass translucent-dark panel style added to `frontgui_style.cpp`.
- [x] Tooltips (3a): `gui_tooltips.c::draw_tooltip()` early-returns under `RendererImGuiEnabled()`;
      `ingame_tooltip_frame()` (`frontgui_ingame_text.cpp`, called last so it sits over the menus)
      renders `tool_tip_box.text` as a cursor-following `NoInputs` window, wrapped to 28% width
      instead of the legacy vertical scroll. `setup_*_tooltips()` / `gui_button_tooltip_update()`
      still run in the input path and fill `tool_tip_box` (the `help_tip_time` hover delay, the
      `toggle_tooltips()` on/off — all inherited via the `TTip_Visible` flag). Covers both world
      tooltips and legacy panel-button tooltips (the sidebar is still legacy until Phase 4/5).
      The trailing control legend baked into the strings (" LMB pick up creature. RMB zoom." etc.)
      is stripped in the render — `tooltip_without_control_hint()` cuts from the first
      `LMB`/`RMB`/`MMB` word (kept verbatim across translations) to the end (user's call).
- [x] `engine_redraw.c` inline text: Paused caption + MP chat line guarded `!RendererImGuiEnabled()`
      and redrawn from `frontgui_ingame_text.cpp` (reading `kfx_sim_state` / `player` directly, no
      new `render_overlay` entry). Spell-cost number deferred (`static` + consume-on-draw).
- [ ] Bottom-anchored stacking of message box / info box / battle box reproduced.
- [ ] `toggle_tooltips()` honoured; boxes don't steal world clicks.
- [ ] Both `-classicmenu` states; layering; both builds; Catch2.

## 5. Open questions — resolved by code investigation (2026-09-06)

- **`evntbox_scroll_window`: keep the field, sync ImGui scroll to it — don't move it.** It is
  `struct TextScrollWindow evntbox_scroll_window` in `kfx_sim_state` (`:354`), deliberately moved
  there from `struct Game` (`game_legacy.h:223`). `.text` is written by `map_events.c` (`kfx_sim`)
  from the script-set objective — sim-authoritative, deterministic. `.start_y` is the local
  scroll position. Moving it out is a save/wire layout migration for a cosmetic field; not worth
  it. The ImGui scroll area reads/writes `.start_y`, `.text` stays sim-owned. (Also: check the
  `text_info_menu` scroll buttons are packet-free — scroll position should be per-client; if
  they're local, no change needed.)
- **World tooltips: compute and draw-prep are intertwined, but only on frontend-local state.**
  `setup_trap_tooltips()` etc. call `update_gui_tooltip_target(thing)` (resets scroll, sets
  `tool_tip_box.target`), bump `kfx_frontend_state.help_tip_time` (hover delay), and
  `set_gui_tooltip_box_fmt()` (fills `tool_tip_box.text`) — all `kfx_frontend_state` /
  `tool_tip_box`, no sim. So keep `setup_*_tooltips` running in the input path (they populate
  `tool_tip_box`); replace only the final render of `tool_tip_box.text` with an ImGui draw-list
  tooltip. `help_tip_time` delay logic stays.
- **MP chat input: keep the existing mechanism, migrate display only.** `get_players_message_inputs()`
  (`front_input.c:353`) already does SDL text input (`LbStartTextInput`), builds
  `player->mp_message_text`, `KC_RETURN` → `PckA_PlyrMsgEnd` + `send_network_chat_message`,
  `KC_ESCAPE` → `PckA_PlyrMsgClear`, `KC_TAB` → `cmd_auto_completion` (a `cmd_char`-prefixed
  command autocomplete). An `ImGui::InputText` swap would mean re-plumbing autocomplete and the
  packet sends. Not worth it — ImGui just renders `mp_message_text` + a cursor as draw-list text.
- **Inline `engine_redraw.c` texts: individual `render_overlay` callbacks**, matching the file's
  existing granularity (`message_draw`, `draw_tooltip` are already individual). `draw_paused_caption()`
  is a clean one (`GOF_Paused && !WorldInfluence && !get_unpausing_in_progress()`). The MP-message
  line and spell-cost need one small callback each (spell-cost's `draw_spell_cost` is a
  `kfx_render`-owned global — expose it or pass it).
