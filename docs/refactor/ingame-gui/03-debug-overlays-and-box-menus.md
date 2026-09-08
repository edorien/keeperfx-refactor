# Phase 2 — debug overlays and box menus

Status: **landed 2026-09-06 — debug overlays + box menus.** Depends on
[01-seam-and-toggle.md](01-seam-and-toggle.md). See [00-overview.md](00-overview.md) §2.4.

**Box menus — what landed:** new module `frontgui_ingame_boxmenu.{h,cpp}`. The `GuiBox` /
`GuiBoxOption` list, `gui_create_box`, `kfx_frontend_state.gui_cheat_box_*`, `first_box`/`last_box`,
the toggle functions — all **unchanged**; only drawing + hit-testing move to ImGui.
- `ingame_boxmenu_frame()` (from `ingame_imgui_frame()`) walks `gui_get_lowest_priority_box()` →
  `gui_get_next_highest_priority_box()` and renders each box as an `AlwaysAutoResize` ImGui window
  (`Cheats###ingame_box_<box_index>`), one `ImGui::Selectable` per option, `is_enabled == 2` /
  empty label → `ImGui::Separator`, `active_cb` result → `BeginDisabled`, `goptn->active` →
  selected highlight. Left-click and right-click both call the option `callback` the same way
  `gui_process_option_inputs()` does (`is_enabled == 1` gate, btn 1/2). The window pos/size is
  written back to `gbox->pos_x/y/width/height` each frame so `cheat_menu_is_active()` and the next
  spawn stay coherent; dragging is ImGui's.
- `gui_draw_all_boxes()` early-returns under `RendererImGuiEnabled()`.
- `gui_process_inputs()` under ImGui: if `ingame_boxmenu_consumes_mouse()` (a cheat box has
  `io.WantCaptureMouse`), clear `left/right_button_clicked/released` so the world underneath
  doesn't also act on the click — the legacy body did the equivalent via its own hit-test. Its
  return value is ignored by the sole caller (`front_input.c:2821`) so control flow is unchanged.
- Every `gf_*` action was already `set_players_packet_action(...)` (§4) — no determinism change.
- `cheat_menu_is_active()` still gates the camera-pan suppression (`front_input.c:2553`) — works
  unchanged since the boxes are still registered.

**Debug overlays — what landed:** new module `frontgui_ingame_debug.{h,cpp}` with one entry
`ingame_debug_overlays_frame()`, called from `ingame_imgui_frame()` (before the menu dispatch).
`main.cpp::render_overlay_draw_debug_overlays()` early-returns when `RendererImGuiEnabled()`, so the
legacy sprite path and the ImGui path never both draw. All 8 overlays reproduced, each behind its
existing `*_enabled()` predicate (reused from `frontmenu_ingame_evnt.h`, not re-derived):
- **Top-right stack** (`##ingame_script_readouts`, heading font, 62% bg): bonus timer *or* script
  timer (bonus wins), script variable, `-timer` stopwatch. Value logic mirrors the legacy
  `draw_*` funcs incl. the script-timer self-hide (`flags_gui &= ~GGUI_ScriptTimer` when a
  `SET_TIMER` expires).
- **Bottom-right**: `GameTurn N` (`DFlg_ShowGameTurns`).
- **Right-middle**: frametime — an `ImGui::BeginTable` (2 cols, or 4 when `debug_display_frametime == 2`).
- **Top-left**: network stats (`debug_display_network_stats`) — 14 `ImGui::Text` lines.
- **Top half**: console log — borderless `NoInputs` window, last 21 `consoleLogArray` lines.
All overlay windows are `NoInputs | NoNav | NoFocusOnAppearing` so they never touch the cursor or
`WantCaptureMouse`. Anchored to screen edges (the armageddon-cast repositioning and the Phase 4
rebase onto the panel rect are both deferred — noted in §1).

Goal: migrate the lowest-stakes, best-ImGui-fit overlays first — the developer/debug HUD and the
cheat/service box menus — as the first real exercise of ImGui compositing over live 3D and the
input path, without touching a gameplay-critical panel.

---

## 1. Debug overlays (`frontmenu_ingame_evnt.c`)

All reached through `render_overlay->draw_debug_overlays()` (`engine_redraw.c:1009`) and the
`bonus_script_or_variable_overlay_active()` gate. Individually toggled, none load-bearing:

| Function | Trigger | Draws |
|---|---|---|
| `draw_bonus_timer()` (`:373`) | `bonus_timer_enabled()` (`:448`) — level bonus-time script | countdown clock top-right of the panel |
| `draw_timer()` (`:453`) | `-timer` style flag | real-time / gameturn stopwatch |
| `draw_gameturn_timer()` (`:521`) | debug flag | current `get_gameturn()` |
| `draw_script_timer()` (`:603`) | `script_timer_enabled()` (`:593`) — `SET_TIMER` script | named script timer |
| `draw_script_variable()` (`:664`) | `display_variable_enabled()` (`:659`) — `DISPLAY_VARIABLE` script | named variable value |
| `draw_frametime()` (`:786`) | `frametime_enabled()` | per-subsystem frame-time bars |
| `draw_network_stats()` (`:865`) | debug flag | packet/latency counters |
| `draw_consolelog()` (`:736`) | console-log flag | scrolling recent log lines |

These position themselves relative to the `GMnu_MAIN` menu rect
(`get_active_menu(menu_id_to_number(GMnu_MAIN))`, e.g. `:419`, `:645`, `:704`) — so once Phase 4
moves the panel, these offsets rebase onto the new panel rect. Until then they can anchor to
screen edges.

**Approach:** an ImGui window (or a few) drawn from the in-game submission when
`RendererImGuiEnabled()`, gated by the same `*_enabled()` predicates. `draw_frametime` /
`draw_network_stats` / `draw_consolelog` are natural `ImGui::Begin` + tables / plots — this is what
ImGui is built for; `imgui_demo`'s own widgets are a close template. The script timer / variable /
bonus timer are single-line readouts — small always-on-top windows or draw-list text.

**Note — some of these are also *script-visible* game features**, not pure debug: `SET_TIMER` /
`DISPLAY_TIMER` / `DISPLAY_VARIABLE` are documented Lua/DK-script commands a campaign author uses
to show the player a countdown or score. Those must stay visible to a normal player with ImGui on
and look deliberate, not like a debug dump. `draw_frametime` / `draw_network_stats` /
`draw_consolelog` are genuinely dev-only and can look utilitarian.

---

## 2. Box menus (`gui_boxmenu.c`)

A **separate widget system** from `GuiButton` — `struct GuiBox` / `struct GuiBoxOption`
(`bflib_guibtns.h:97-122`), a linked list (`first_box` / `last_box`, `gui_boxes[1..2]` — max 2),
each a draggable window of text options with per-option `active_cb` (is-this-option-shown) and
`callback` (do it). Drawn by `gui_draw_all_boxes()` (`:362`), input by its own path (not
`get_gui_inputs`'s `active_buttons[]` sweep — check `front_input.c` around
`cheat_menu_is_active` / `a_menu_window_is_active`).

Contents are the cheat / service menu: `gf_change_player_state`, `gf_kill_creature`,
`gf_level_up` / `gf_level_down`, `gf_make_everything_free`, `gf_explore_everywhere`,
`gf_research_*`, `gf_all_doors` / `gf_all_traps`, `gf_decide_victory`, `gf_apply_spell`, … — a flat
list of dev/cheat actions with `gfa_*` availability predicates.

**Approach:** this is the single clearest ImGui upgrade in the whole project — a hand-drawn
draggable option list becomes an `ImGui::Begin` window with `ImGui::MenuItem` / `Selectable` rows,
`BeginDisabled` from the `gfa_*` predicate, dragging for free. `GuiBoxOption`'s
`{label, is_enabled, active_cb, callback, …}` maps almost 1:1 to a loop emitting one
`ImGui::MenuItem` per entry.

**Determinism:** several of these actions mutate shared game state (`gf_kill_creature`,
`gf_level_up`, `gf_make_everything_free`, `gf_decide_victory`). Check whether they currently go
through packets — cheats in MP usually do, or are host-only. A migrated menu must keep whatever
path they use. Most sessions that open this menu are single-player anyway.

**Cheat-menu gating:** `cheat_menu_is_active()` / `a_menu_window_is_active()`
(`front_input.c:2555,2560`) suppress world input while the box is open — the ImGui window needs to
feed `busy_doing_gui` / `PCtr_Gui` the same way (Phase 0 §2.4).

---

## 3. Checklist

- [x] `draw_debug_overlays()` → ImGui windows, same `*_enabled()` gates
      (`frontgui_ingame_debug.cpp`, 2026-09-06). `main.cpp` legacy path early-returns under ImGui.
- [x] Script-visible timers/variables in a heading-font top-right plate; dev overlays utilitarian.
- [x] `dev`-only overlays: frametime as an `ImGui::BeginTable`, netstats/consolelog as text windows.
- [x] `gui_boxmenu.c` → ImGui windows; `GuiBoxOption` list → `Selectable` loop; dragging via ImGui
      (`frontgui_ingame_boxmenu.cpp`, 2026-09-06).
- [x] Cheat-box click swallowed from the world via `ingame_boxmenu_consumes_mouse()` +
      `gui_process_inputs()`; `cheat_menu_is_active()` camera gate unchanged.
- [x] Cheat actions keep their `set_players_packet_action` path (untouched — ImGui only calls them).
- [x] Overlays anchor to screen edges for now; Phase 4 rebase onto the panel rect noted (§1).
- [x] Both `-classicmenu` states build; `check_layering.py --strict` clean; 3 trees; 69 frontend
      Catch2; full ftest sweep; `gui_seam_ingame` + `gui_packet_parity`.

## 4. Open questions — resolved by code investigation (2026-09-06)

- **Cheat-box actions are all packet-routed.** `gf_change_player_state` → `PckA_SetPlyrState`,
  `gf_kill_creature` → `PckA_CheatKillCreature`, `gf_level_up` → `PckA_CheatLevelUp`,
  `gf_make_everything_free` → `PckA_CheatAllFree`, etc. — every `gf_*` in `gui_boxmenu.c` calls
  `set_players_packet_action()`. So a migrated ImGui box menu just calls the same, no new
  determinism concern; `goptn->active` is per-client radio visual state, keep it local.
- **`gui_boxmenu.c` input is fully separate from `get_gui_inputs`.** `gui_process_inputs()`
  (`gui_boxmenu.c:882`, called from `front_input.c:2820` *before* `get_gui_inputs(1)`) uses
  `GetMouseX/Y()`, the `left_button_*`/`right_button_*` globals, `dragging_box`, and its own
  `gui_get_box_*` hit-tests — no `active_buttons[]`. It returns a bool that gates the rest of
  `get_players_input`. `gui_process_option_inputs(hpbox, goptn)` fires the option callback on
  button *release*. Migration: `gui_process_inputs()` returns false for a migrated box (nothing to
  hit-test), the ImGui window calls `gui_process_option_inputs()` itself, `point_is_over_gui_box()`
  consults the ImGui window rect, dragging is free.
- **Player-facing vs dev-only:**
  - *Player-facing* (campaign/script features — style deliberately): `bonus_timer`
    (`GGUI_CountdownTimer`), `script_timer` (`GGUI_ScriptTimer`), `draw_script_variable`,
    `draw_timer` (`kfx_sim_state.TimerGame` / `-timer` / `SET_GAME_TIMER` — a speedrun stopwatch).
  - *Dev-only* (utilitarian styling fine): `draw_gameturn_timer` (`DFlg_ShowGameTurns`),
    `draw_frametime` (`debug_display_frametime`), `draw_network_stats`, `draw_consolelog`
    (`debug_display_consolelog`).
- **`draw_consolelog` → a proper ImGui log window** (`Begin` + filter + `TextUnformatted` +
  autoscroll — the standard `imgui_demo` log pattern), not a straight port. It's dev-only so the
  extra affordances cost nothing and help.
