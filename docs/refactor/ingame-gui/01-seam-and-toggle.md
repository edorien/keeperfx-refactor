# Phase 0 — the seam, the toggle, the submission path, the cursor

Status: **landed 2026-09-06** (builds: linux std/hvlog, mingw; `check_layering --strict`; 1556
Catch2; full ftest sweep + new `gui_seam_ingame` ftest). Prereq for every other phase. No visible
change except the one proof menu (`quit_menu`), which now renders as an ImGui modal.

**What landed:** `src/kfx_frontend/{src,include}/frontgui_ingame.{cpp,h}` — the per-`GMnu_*` seam:
`ingame_imgui_menu_active(MenuID)` / `ingame_imgui_modal_active()` predicates, `ingame_imgui_frame()`
(the submission arm, called from `FrontendImGuiFrame()`), a deferred-action slot, the once-per-
game-turn view-model cache scaffold, and the `GMnu_QUIT` proof modal. Draw suppression in
`draw_active_menus_buttons()` (`frontend.cpp`); input suppression in `get_gui_inputs()`
(`front_input.c`). `quit_menu` migrated: Esc → (classic) options → Quit → ImGui "Really quit?"
modal → Yes sends `PckA_QuitToMainMenu` (verified identical to legacy `gui_quit_game`), No closes it.

**Cursor: no changes needed for Phase 0** — see §3. The proof menu is an ImGui *modal popup*, which
sets `io.WantCaptureMouse` globally while open, so the existing cursor path (legacy cursor
suppressed on `WantCaptureMouse`, ImGui draws its own) already yields one consistent cursor. The
`PointerDraw` unification for *non-modal* HUD panels moved to Phase 4 (it can't be built or tested
without one).

**Post-landing fixes (from live testing):**
1. **White wash-out over the whole screen when the modal opened.** `ImGuiCol_ModalWindowDimBg` was
   never set in `frontgui_style.cpp`, so it used ImGui's dark-theme default — a *light* grey
   `(0.80,0.80,0.80,0.35)` that dims by lightening. Glaring over the live 3D scene. Set to
   `(0,0,0,0.55)` (matching `NavWindowingDimBg`); also corrects the frontend modals' backdrop.
2. **"Are you sure?" wrapped onto multiple lines, window too narrow.** `FeBeginModal`'s
   `AlwaysAutoResize` + `FeBodyText`'s wrap-at-content-edge form a feedback loop that collapses a
   short prompt to minimum width. Fixed: `quitmenu_frame()` sets
   `SetNextWindowSizeConstraints(min_w = DisplaySize.x * 0.18, ...)` before `FeBeginModal`, and
   uses `FeSubheading` (non-wrapping) for the prompt.
3. **Title bar read "IngameQuit".** The modal id is now `"<localized Quit>###IngameQuit"` —
   `get_string(GUIStr_MnuQuit)` shown, `IngameQuit` as the stable popup key.

Goal (original): stand up the machinery that lets a single `GMnu_*` render as ImGui while every
other in-game menu keeps its sprite path, with input, the `PCtr_Gui` contract, the per-game-turn
content cache, and cursor consistency all correct — proven end to end on the smallest possible menu.

---

## 1. What has to exist after this phase

1. `TbBool ingame_imgui_menu_active(MenuID menu_id)` — "ImGui GUI enabled (`RendererImGuiEnabled()`)
   **and** this `GMnu_*` has an ImGui implementation registered". Mirrors
   `frontend_imgui_screen_active(state)` (`frontgui_screens.cpp`).
2. An in-game dispatch arm inside `FrontendImGuiFrame()` (`frontgui_screens.cpp`) — decided in
   [00-overview.md](00-overview.md) §4.3 to fold into the frontend submission site rather than add
   a `render_overlay` slot. When a level is loaded and `ingame_imgui_menu_active()` is true for
   any active menu, submit those menus' ImGui windows.
3. Draw suppression in `draw_active_menus_buttons()` (`frontend.cpp:3480`) — for a migrated,
   active, turned-on menu, skip `gmnu->draw_cb(gmnu)` + `draw_menu_buttons(gmnu)`.
4. Input suppression + `PCtr_Gui` preservation in `get_gui_inputs()` (`front_input.c:2936`) —
   skip the `active_buttons[]` sweep for buttons whose `gmenu_idx` maps to a migrated menu, but
   still set `busy_doing_gui` / feed `PCtr_Gui` when the pointer is over a migrated panel (ImGui
   `io.WantCaptureMouse`, or the panel rect).
5. A per-game-turn view-model cache hook ([00-overview.md](00-overview.md) §3.1) — a place the
   HUD's derived numbers are recomputed once per game-turn and read per-frame by the ImGui
   submission. For Phase 0 this can be a stub (`quit_menu` has no dynamic content); the structure
   needs to be there so Phase 1+ slot into it.

**Already built (2026-09-06):** the packet-capture determinism harness
(`src/ftests/ftest_packet_capture.{h,c}`, `ftest_util_gui_click()`, the `ftest_gui_packet_parity`
test) — see [06-tab-content-panels.md](06-tab-content-panels.md) §4. Phase 0's proof-menu step
(`quit_menu`) should add a golden-trace row and run it both toggle states.
6. Cursor: one draw, world and panel identical ([00-overview.md](00-overview.md) §3.9).

---

## 2. Investigation — the legacy dispatch, in detail

### 2.1 Draw path

`redraw_display()` (`engine_redraw.c:928`) has **three** near-identical blocks (one per view type,
`:553` / `:611` / `:635`) each calling, via `render_overlay`:
`draw_whole_status_panel()` → `draw_gui()` → `message_draw()` → `gui_draw_all_boxes()` →
`draw_tooltip()` (order varies slightly; power-hand interleaves).

`draw_gui()` (`frontend.cpp:3587`) → `update_fade_active_menus()` → `draw_active_menus_buttons()`
(`:3480`). That function walks `menu_stack[0..no_of_active_menus)`, and for each turned-on menu
with `visual_state != 0` calls `gmnu->draw_cb(gmnu)` then, if `visual_state == 2`,
`draw_menu_buttons(gmnu)` (`:3433` — two passes: unpressed buttons, then pressed).

**So the opt-in point is precise:** in `draw_active_menus_buttons()`'s loop body, `if
(ingame_imgui_menu_active(menu_stack[k])) continue;` (after the ImGui submission has drawn it) —
exactly the shape `frontend.cpp`'s draw switch got for `FeSt_FEOPTIONS`.

**`draw_whole_status_panel()` is separate** — it draws the `GMnu_MAIN` background (panel sprite,
gold, minimap) *outside* `draw_active_menus_buttons()`. It is called directly from
`engine_redraw.c`. Migrating `GMnu_MAIN` (Phase 4) means gating `render_overlay->draw_whole_status_panel()`
too, not just the `draw_gui()` arm. Phase 0 does not touch it.

### 2.2 Menu-fade

`update_menu_fade_level()` / `gmnu->fade_time` / `visual_state` (0 = off, 1 = fading, 2 = shown)
animate menus in/out. Migrated menus won't use this — ImGui does its own appearance. But
`kill_menu()` / `remove_from_menu_stack()` fire off `update_fade_active_menus()` when a menu fades
out, and `turn_off_menu()` sets `visual_state` toward 0. The ImGui side must treat "menu is in the
stack and `is_turned_on`" as the visibility signal and ignore `fade_time`, while leaving the
fade-state machine running so `menu_is_active()` / `first_monopoly_menu()` stay correct for
un-migrated code.

### 2.3 Input path

`get_gui_inputs(gameplay_on=1)` (`front_input.c:2936`), called from `get_players_input()` /
`get_inputs()` when `!GOF_Paused || active_menu_functions_while_paused() || GOF_WorldInfluence`
(`front_input.c:2843`). It:
- resets `battle_creature_over`, `gui_*_type_highlighted`;
- calls `update_busy_doing_gui_on_menu()` (`gui_frontmenu.c:127` — sets `busy_doing_gui` from
  `point_is_over_gui_menu()`);
- sweeps `active_buttons[]`: per button runs `maintain_call`, then if the mouse is over it and its
  menu is the front monopoly menu, sets `LbBtnF_MouseOver`, `busy_doing_gui = 1`, runs
  `ptover_event`, and (for sliders/hotspots) tracks drag;
- click/hold/release handling further down;
- special-case: `if (mouse_is_over_panel_map(...)) continue;` — the minimap is carved out of
  button hit-testing by an explicit check (there is a `// TODO GUI Introduce circular buttons`
  note there).

At the very end of `get_player_gui_clicks()`: `if (game_is_busy_doing_gui()) set_players_packet_control(player, PCtr_Gui);`
(`front_input.c:2765`). `PCtr_Gui` on the packet tells `kfx_net`/`kfx_sim`
(`packets.c:800,999`, `packets_input.c:711`, `roomspace_prediction.c:73`) to not treat the
click as a world action **this turn** — and this is replayed from packets in MP and demos, so it
must be produced identically.

**Phase 0's input change:** for a migrated menu, its `active_buttons[]` entries should either not
be created at all (cleanest — `create_menu()` skips button instantiation for migrated menus) or be
skipped in the sweep. `busy_doing_gui` / `PCtr_Gui` must still be set whenever ImGui wants the
mouse over a migrated panel *or* `point_is_over_gui_menu()`-style geometry says the pointer is over
it — so the sim layer's view-suppression is unchanged. `quit_menu` is a good probe precisely
because it *does* send a packet (`gui_quit_game` → quit), so "same packet, same turn" is testable.

### 2.4 `busy_doing_gui` and friends — the globals to keep consistent

- `busy_doing_gui` (int, `gui_frontmenu.c`) — set each frame from geometry; read by
  `game_is_busy_doing_gui()` and thus `PCtr_Gui`.
- `first_monopoly_menu()` (`gui_frontmenu.c:72`) — first menu with `is_monopoly_menu`; a monopoly
  menu (options/quit/etc.) blocks input to menus below it. The ImGui modal in Phase 1 must respect
  the same "nothing below me gets input" semantics.
- `point_is_over_gui_menu(x,y)` (`gui_frontmenu.c:100`) — hit-tests menu rects. For a migrated
  menu its rect is now ImGui-owned; this needs to consult the ImGui window rect (cached last
  frame) or `io.WantCaptureMouse`.
- `a_menu_window_is_active()` (`frontend.cpp:494`) — any window (non-panel) menu up. Used all
  over `front_input.c` for Esc handling, right-click-to-close, etc. Stays truthful as long as
  `turn_on_menu`/`turn_off_menu` still run for migrated menus (they should — only draw/input are
  swapped).

### 2.5 The submission entry point

`FrontendImGuiFrame()` runs from `RendererSoftware::PresentFrame()` every present, gameplay
included. Today its body dispatches on `frontend_menu_state` and does nothing when a level is
loaded. Add: if `game_is_in_progress()` (or equivalent) and any active menu is migrated, run the
in-game submission. The deferred-action pattern (`s_pending_*`) already there covers "don't call
`turn_off_menu()` / state transitions from inside a `Begin()`/`End()`".

**Ordering trap** (from stage 4 Phase E's land-preview input bug): `FrontendImGuiFrame()` runs at
present time, *after* `get_players_input()` and `exchange_packets()` in the same frame. So a
migrated-menu click's deferred packet (`s_pending` applied at the *next* present) reaches
`exchange_packets()` two turns after the click, vs. the classic path's same-turn send from inside
`get_gui_inputs()`. **For `quit_menu` this is invisible** (quit-to-menu isn't turn-sensitive) and
determinism still holds (the defer→apply is local-input-driven and consistent). But **Phase 4+
non-modal panels whose clicks are gameplay-relevant** (room/spell select, pickup) should run their
input at `get_gui_inputs()` time via a `front_input.c` hook using last frame's ImGui rect —
exactly like `FrontendImGuiLandPreviewInput()` — not the present-time deferred path. Modals
(`quit_menu`, Phase 1's options launcher) are fine on the deferred path.

---

## 3. Cursor — deferred to Phase 4 (revised 2026-09-06)

Today, in-game: `bflib_mspointer.cpp`'s `PointerDraw()` blits the cursor sprite straight into
`lbDrawSurface` (unlocked) around the frame swap. The ImGui overlay cursor (stage 4) is a separate
draw via `GetForegroundDrawList()->AddImage()` gated on `io.WantCaptureMouse`, fed the game's
tracked `GetMouseX/Y()` and the `FeStyleGetCursorImage()` texture. `OnBeginSwap()` / `OnMove()`
already *skip* the legacy blit whenever `ImGuiContextWantCaptureMouse()` is true.

**Phase 0 needed no cursor change.** The proof menu (`quit_menu`) is an ImGui **modal popup**
(`ImGui::BeginPopupModal` via `FeBeginModal`), which sets `io.WantCaptureMouse` **globally** while
open — the dim-background layer covers the whole screen. So while the modal is up: legacy cursor
suppressed everywhere, ImGui draws `FeStyleGetCursorImage()` (`GFS_cursor_horny`) everywhere → one
consistent cursor, no straddle-the-edge case. When the modal closes, the legacy cursor returns.
This holds for every migrated *monopoly* menu (Phase 1's options launcher too).

**The full `PointerDraw()` unification is now a Phase 4 item.** It only matters for a *non-modal*
migrated panel (the sidebar), where the world cursor must stay the work-state sprite (dig / cast /
possess) while the ImGui panel shows a pointer over itself — which needs `PointerDraw()` folded
into the off-screen-render → dynamic-texture → draw-list mechanism, keyed on the current cursor
sprite id, so both are drawn by one code path at present time. That can't be built or tested
without a non-modal panel, so it moves to [05-sidebar-frame-and-minimap.md](05-sidebar-frame-and-minimap.md)
(it is also stage 3 §B-cursor's item — coordinate).

**Acceptance (Phase 0):** the quit modal shows one cursor, over the modal and over the world
behind it. **Acceptance (Phase 4):** screenshot with the pointer straddling a non-modal panel
edge — one cursor, continuous, right sprite either side.

---

## 4. Checklist

- [x] `ingame_imgui_menu_active(MenuID)` + `menu_is_migrated()` switch registry
      (`frontgui_ingame.cpp`).
- [x] `draw_active_menus_buttons()` skips migrated menus (`frontend.cpp`, one `continue`).
- [x] `FrontendImGuiFrame()` in-game dispatch arm — `ingame_imgui_frame()`.
- [x] `get_gui_inputs()`: modal → early `return true` with `busy_doing_gui = 1`; per-button skip
      for any migrated menu (`front_input.c`).
- [~] `point_is_over_gui_menu()` / `first_monopoly_menu()` — **not needed for Phase 0**: the modal
      early-return in `get_gui_inputs` covers it. Deferred to Phase 4 (non-modal panels), where
      `point_is_over_gui_menu` must consult the ImGui panel rect.
- [x] Per-game-turn view-model cache scaffold — `refresh_cache_if_stale()` keyed on
      `get_gameturn()`, empty `rebuild_cache()` body (`quit_menu` has no dynamic content).
- [x] Cursor — no change needed for Phase 0 (modal → global `WantCaptureMouse`). Full
      `PointerDraw` unification deferred to Phase 4 (§3).
- [x] `quit_menu` migrated: Yes → `PckA_QuitToMainMenu` (identical to `gui_quit_game`), No closes.
      Verified by `gui_seam_ingame` ftest. **Manual still owed:** the full interactive matrix
      (Esc→options→quit by hand, `-classicmenu` on/off, SP + MP, cursor screenshot) — not
      headless-doable.
- [x] `check_layering.py --strict`; linux std/hvlog + mingw builds; 1556 Catch2; full ftest
      sweep (18/18) + new `gui_seam_ingame`.
- [~] Un-migrated menus pixel-identical — not screenshot-verified headless; the ftest sweep
      (creature/pathing tests exercise the live in-game GUI) passing unchanged is the evidence.

## 5. Open questions — resolved by code investigation (2026-09-06)

- **Migrated menus: keep their `active_buttons[]` entries, gate only draw + input.**
  `turn_on_menu()` → `create_menu()` (`frontend.cpp:2343`) instantiates every button
  unconditionally via `create_button()`. `menu_is_active()` / `first_monopoly_menu()` /
  `point_is_over_gui_menu()` walk `active_menus[]`, not buttons — unaffected. The other
  `active_buttons[]` readers are `button_snapping.c` (nav), `gui_vscroll.c`, `gui_tooltips.c:406`,
  `gui_frontbtns.c` (`fake_button_click`), `front_input.c` (the sweep), `frontend.cpp`
  (`draw_menu_buttons`, spangle). Skipping button creation would mean auditing all of those for
  null-safety. Instead: **leave buttons instantiated** (harmless — nothing draws or hovers them
  once `get_gui_inputs` skips migrated menus), and add a `ingame_imgui_menu_active(gbtn's menu)`
  guard to the snapping + tooltip walks (or let the ImGui path own nav/tooltips for its widgets so
  the legacy walks naturally never reach a migrated menu's buttons — snap-to-direction only runs
  when ImGui nav is off).
- **"Level running" predicate: `kfx_sim_state.game_kind == GKind_LocalGame || == GKind_MultiGame`.**
  `game_kind` is `GKind_Unset` (0) outside a level, set to `GKind_LocalGame`/`GKind_MultiGame`
  when one starts (`main_game.c`, `net_game.c:516`), reset to `GKind_Unset` at
  `keeper_gameplay_loop` exit (`game_session_loop.cpp:650`). `GKind_NonInteractiveState` is
  cutscenes. No dedicated "game in progress" helper exists — check `game_kind` directly (add a
  one-line inline). This is only a coarse guard; the real gate is per-menu
  `ingame_imgui_menu_active()`, and `FrontendImGuiFrame()` runs from `PresentFrame` in both the
  frontend and gameplay loops so the guard keeps the in-game arm dormant during menus.
- **Per-game-turn cache: lazy rebuild keyed on `get_gameturn()`, checked at the top of the in-game
  ImGui submission.** No new call site — `if (s_cached_turn != get_gameturn()) { rebuild;
  s_cached_turn = get_gameturn(); }`. `gameplay_loop_logic()` (`game_session_loop.cpp:448`) runs
  once per game turn (verified: `ftest_packet_capture_tick()` sits there and fires once/turn), so
  `get_gameturn()` advances once per turn and the lazy check self-heals across pauses / frame
  skips. `kfx_game_state.play_gameturn` is the frame-skip-aware counter — not what a "recompute
  when displayed numbers changed" cache wants; `get_gameturn()` is right.
