# Phase 5 — the tab content panels

Status: **all five tab bodies landed 2026-09-07 (`GMnu_ROOM` / `SPELL` / `TRAP` / `CREATURE` / `QUERY`).** Remaining: scroll-on-overflow, per-turn cache, packet-parity ftest rows, retire `update_*_tab_to_config` + `GMnu_*2`. Depends on
[05-sidebar-frame-and-minimap.md](05-sidebar-frame-and-minimap.md). See [00-overview.md](00-overview.md) §2.1.

**Landed — `GMnu_ROOM` (`frontgui_ingame_tabcontent.{h,cpp}`):** `ingame_tabcontent_frame()` from
`ingame_imgui_frame()` (after the panel, over its tab-content hole). `GMnu_ROOM` added to
`menu_is_migrated()`. `room_grid()` **iterates `get_room_kind_stats(k)` for `k` in
`1..slab_conf.room_types_count`**, keeps those with `panel_tab_idx > 0`, sorts by it — no
`GuiButtonInit` mutation, no 2nd page. Each `build_icon()`: portrait frame + `medsym_sprite_idx`
(greyed when `total_money_owned < cost`), `room_list_start[k] > 0` glow, `?` for
researchable-but-not-buildable, selected ring on `chosen_room_kind`. Left-click →
`activate_room_build_mode(k, tooltip_stridx)` (`PckA_SetPlyrState`/`PSt_BuildRoom` — called
directly, no menu ops); right-click → `go_to_my_next_room_of_type_and_select` + set
`chosen_room_*`; hover → `gui_room_type_highlighted = k` (one frame stale for roomspace
prediction — same as the legacy sweep by prediction time). Sell button →
`PckA_SetPlyrState`/`PSt_Sell`. Big info line = hovered/chosen room name + cost. Grid is a fixed
4-wide flow inside the region (scrolling on overflow is the next refinement). Geometry from the
scaled `GMnu_MAIN` menu rect × the 140×400 virtual grid.
**`GMnu_SPELL` + `GMnu_TRAP` landed (same file, 2026-09-06):** `spell_grid()` iterates
`get_power_model_stats(k)` `k` in `1..power_types_count`, `panel_tab_idx > 0`, sorted; enabled =
`is_power_available` with the `PwrK_ARMAGEDDON` (`armageddon_cast_turn`) / `PwrK_HOLDAUDNC`
(`player_uses_power_hold_audience`) special-cases; L-click `choose_spell(k, tooltip)`, R-click
`go_to_next_spell_of_type` + `set_chosen_power`; selected ring on `chosen_spell_type`; info line =
name + `cost[0]`. `trap_grid()` iterates `get_manufacture_data(m)` `m` in
`1..manufacture_types_count`, `panel_tab_idx > 0`; enabled = `is_trap_placeable || is_trap_built`
(`is_trap_buildable` → `?` overlay); L-click `choose_workshop_item(m, tooltip)`, R-click
`go_to_next_trap_of_type` + set `manufactr_*`; hover → `gui_trap_type_highlighted = tngmodel`; sell
button → `PSt_Sell`. `go_to_next_spell_of_type` / `go_to_next_trap_of_type` exported in
`frontmenu_ingame_tabs.h`.
**Restructure + polish (2026-09-07, retest round 1):** the tab body is now drawn **inside the
sidebar frame's own ImGui window** — `frontgui_ingame_panel.cpp` calls `ingame_tabcontent_draw(px,py,pw,ph)`
between its tab strip and `ImGui::End()`; the separate `##IngameTabContent` window is gone. One
window = no z-order/clip seam between frame and grid (the earlier split left the grid's
`InvisibleButton`s unhittable and clipped the info strip). `build_icon()` no longer blits the legacy
`GPS_rpanel_frame_portrt_*` sprites (they carried baked-in background) — it draws an ImGui rounded
rect + border (gold = selected, red = hover, brown = rest), a have-one dot, a `?` glyph for
researchable-only. Grid moved to the legacy metrics (virtual `y 242`, 4 cols, 32×38 pitch) so the
**info strip** (`info_band()`, virtual `y 196..240`) has its room back: hovered-else-selected item's
big symbol + name + `xN` count + gold cost, plus a capacity/efficiency bar for rooms
(`find_room_type_capacity_total_percentage`, now exported). Creature rows tightened to
`portrait + 2px` pitch.

**Retest round 2 (2026-09-07):** researchable-but-unavailable items now render identically on every
panel — a big unselectable `?` (75% of the cell) via `build_icon(..., researchable_only=true)`, which
swallows clicks. `spell_grid` shows the power once `magic_resrchable[k] || magic_level[k] > 0` (was
skipping every unavailable power), `?` when `!is_power_available`, dimmed icon when available but
on cooldown / in use. Sell cell is a dedicated `sell_icon()` — a big red `$`, no legacy artwork.
Efficiency bar thickened (16px). Hover→info-band gated to genuinely usable items.

**Retest round 3 (2026-09-07):** `?`/`$` glyphs now drawn with the heavy UI vector font
(`FeStylePushFont(FeFont_Heading)` — the built-in default font was bitmap-upscaled and pixelated).
Not-yet-researched items are collapsed into a **single `unknown_cell()`** per panel (big `?` + the
hidden-count in the lower-right) instead of one `?` box each; `build_icon` lost its
`researchable_only` branch (each grid now buckets unavailable items into `hidden` and renders one
cell after the real icons).

**`GMnu_CREATURE` landed (same file, 2026-09-07):** `creature_list()` renders up to 6 rows from
`breed_activities[]` / `no_of_breeds_owned` (kept current every frame by
`update_creatr_model_activities_list()` in `get_gui_inputs()` — independent of this menu's draw).
Row 0 is always `get_players_special_digger_model`. Per row: a `CGI_HandSymbol` portrait
(`FeGuiPanelTexture`) with an owned-count badge, then three job cells with the idle / working /
fighting tallies (`crmodel_state_type_count` folded through `state_type_to_gui_state`, mirroring
`gui_activity_background`). Portrait L-click → `pick_up_creature_of_model_and_gui_job(model,
CrGUIJob_Any, …)`, R-click → `go_to_next_creature_of_model_and_gui_job` (both already packet-routed
— `PckA_UsePwrHandPick` / `PckA_ZoomToPosition`). Job cell L/R-click → the same two calls with the
`CrGUIJob_Wandering/Working/Fighting` index. A header row of IDLE/WORK/FIGHT cells does the same
against `CREATURE_ANY`. Hover → `gui_creature_type_highlighted = model`. Mouse-wheel over the window
scrolls `top_of_breed_list` when `no_of_breeds_owned > 6`. `get_creature_pick_flags` exported in
`frontmenu_ingame_tabs.h`. Anger colour bars + the exact legacy sprite gauge are a later polish
pass (text counts for now).

**`GMnu_QUERY` landed (2026-09-07):** `query_panel()` — tendency toggles (imprison / flee →
`PckA_ToggleTendency`), payday / research / workshop progress bars (`pay_day_progress` ÷
`rules.gameplay.pay_day_gap`; `research_progress>>8` ÷ `ResearchVal::req_amount`;
`manufacture_progress>>8` ÷ `manufacture_points_required`), up-to-4 per-player rows (colour swatch,
`num_active_creatrs`, `total_rooms`, ally toggle → `PckA_PlyrToggleAlly` for other allocated
keepers), a QUERY-mode button (`PckA_SetPlyrState`/`PSt_CreatrQuery`) and a `P>` page-cycle in
>4-player games. `query_pos_to_player()` / keeper count reimplemented locally (legacy equivalents
are `static`). All mutations packet-routed.

**Query-panel round 2 (2026-09-07):** the panel now reuses the legacy GPS_* panel sprites instead
of text — tendency toggles (`GPS_rpanel_tendency_prisne_act` / `_fleee_act`, `_prisnu_dis` greyed
when no prison room), payday/research/workshop icons (`GPS_rpanel_rpanel_payday_counter`,
`GPS_room_research_std_s`, `GPS_room_workshop_std_s`) beside the ImGui progress bar, per-player
`GPS_plyrsym_symbol_room_*` / `_player_*` (via `get_player_colored_icon_idx`) for the room/creature
counts, `_player_*_std_b` / `_any_dis` for the ally toggle, `GPS_rpanel_rpanel_btn_crinfo_act` for
the QUERY-mode button. `sprite_toggle()` gives active toggles a pulsing gold border and greys +
disables unavailable ones. Sprite picks refined: flee = `tendency_fleed_act` (the chicken),
imprison = `tendency_prisnd_act` / `prisnu_dis`, payday icon = `GPS_room_treasury_std_s` (the
`payday_counter` sprite is a whole widget, wrong shape). `bar_row` gives the icon its own square +
a 0.4×row-height gap so it never overlaps the track.

**Scroll-on-overflow landed (2026-09-07):** the room / power / manufacture grids draw inside an
`ImGui::BeginChild` scroll region (`grid_begin` / `grid_cell_pos` / `grid_end`) — a scrollbar
appears once the item count overflows the visible area, replacing the legacy 2nd page for good.
The sell cell moved from a fixed slot to the end of the flow. `unknown_cell` (the collapsed
not-yet-researched `?`) and sell scroll with the grid.

**Packet-parity ftest extended (2026-09-07):** `ingame_tabcontent_test_fire()` (FUNCTESTING-only,
`frontgui_ingame_tabcontent.h`) runs each migrated handler's exact code path headless.
`gui_packet_parity` golden trace now has 9 rows (3 legacy + 6 ImGui: room build / room sell /
manufacture sell / query-mode / tend-imprison / tend-flee), plus an invariant check that ImGui
spell-choose emits `PckA_SetPlyrState`.

**Not yet:** `GMnu_SPELL_LOST` (possession — Phase 6); per-turn cache; retiring
`update_*_tab_to_config` + the placeholder arrays + the 2nd page (`GMnu_ROOM2` / `SPELL2` / `TRAP2`).

Goal: migrate the five sidebar tab bodies, one `GMnu_*` at a time — the room / power / manufacture
build grids, the information tab, the creatures tab. This is where the bulk of the state-extraction
work is, and where the MP-determinism ftest harness lands.

---

## 1. Menus

| `GMnu_*` | Menu | Body |
|---|---|---|
| `GMnu_QUERY` | `query_menu` | payday / research / workshop progress bars, per-player creature & room counts, ally toggles, tend-to-imprison / tend-to-flee |
| `GMnu_ROOM` (+ `GMnu_ROOM2`, dropped — §2.1) | `room_menu` | room build grid → **scrollable** `FeIconGrid`, big hovered-room info, sell-area |
| `GMnu_SPELL` (+ `GMnu_SPELL2`, dropped) / `GMnu_SPELL_LOST` | `spell_menu` / `spell_lost_menu` | keeper-power grid → scrollable, big power info. `SPELL_LOST` = the reduced grid shown in possession / when powers are disabled |
| `GMnu_TRAP` (+ `GMnu_TRAP2`, dropped) | `trap_menu` | traps + doors grid → scrollable, big info, buildable-count, sell |
| `GMnu_CREATURE` | `creature_menu` | scrollable 4×N creature-model rows, per-activity (idle/working/fighting) counts + pick buttons, anger colour bars, up/down scroll |
| `GMnu_CREATURE_QUERY1..4` | — | the 4 query/possession detail pages — **do these in [07-first-person-hud.md](07-first-person-hud.md)**, they are shared with possession |

---

## 2. The big simplification: config-driven grids, no array mutation

`update_room_tab_to_config()` (`frontmenu_ingame_tabs.c:2628`), and its `_trap_` / `_powers_`
siblings, **mutate the `GuiButtonInit` arrays at runtime** — clear all 15 slots, then for each
configured room with `panel_tab_idx >= 1` write `sprite_idx` / `tooltip_stridx` / `content.lval` /
callbacks into `room_menu.buttons[panel_tab_idx-1]` (or `room_menu2` for 17..32), then
`turn_off_menu` + `turn_on_menu` to rebuild the live buttons. Called on config load and whenever
the buildable set changes.

The ImGui version **iterates the config directly** — `for (i in 0..room_types_count) if
(roomst->panel_tab_idx) emit an FeIconButton` — and the whole clear/repopulate/menu-cycle dance,
plus the static arrays' 32 placeholder rows, disappear. This is the same win as the frontend's
`_by_index` extraction (stage 4 Phases D–F), one level up.

`get_room_kind_stats(i)` → `medsym_sprite_idx`, `tooltip_stridx`, `panel_tab_idx`; the count badge
comes from the dungeon; availability (researched? affordable?) from `maintain_room` /
`maintain_big_room`.

### 2.1 Drop the second page — decided (user, 2026-09-06)

`room_menu2` / `spell_menu2` / `trap_menu2` (`GMnu_ROOM2` / `GMnu_SPELL2` / `GMnu_TRAP2`) and the
`gui_set_page` next-page button (`maintain_*_next_page_button`) were **a hack to fit more entries
than a fixed 4×N grid holds** — `panel_tab_idx` 1..16 on page 1, 17..32 on page 2, an extra
`GMnu_*` per tab, and the spangle/flash code has to special-case which page an item is on
(`frontend.cpp:3541-3561`).

**The ImGui grid scrolls instead.** One `FeIconGrid` per tab, inside an `FeBeginScrollArea` that
only shows a scrollbar when the content overflows. `FeIconGrid` takes an **orientation** from the
active `HudLayout` ([05](05-sidebar-frame-and-minimap.md) §0): **4 columns × N rows** for the
vertical (DK1) sidebar; **N columns × 2 rows** for the horizontal (DK2) bar, where each of
room/spell/trap/creature is its own side-by-side block. Item order within the grid is
`panel_tab_idx` (§2.1). So:
- `GMnu_ROOM2` / `GMnu_SPELL2` / `GMnu_TRAP2`, `spell_menu2_buttons` etc., `gui_set_page`,
  `maintain_*_next_page_button`, and the `panel_tab_idx > 16` branch in `update_*_tab_to_config`
  all **go away**.
- `panel_tab_idx` collapses to a simple sort key / "show in this tab" flag, not a slot index into
  a paged array — check every config reader (`.cfg` parse, docs) for the 1..32 assumption.
- The flash/spangle tab-header logic (`button_designation_to_tab_designation`,
  `frontend.cpp:3528`) loses its `GMnu_ROOM2 vs GMnu_ROOM` disambiguation — simplifies.
- `BID_ROOM_TD17..32` / `BID_MNFCT_TD17..32` / `BID_POWER_TD17..32` button-designation ids: keep
  as flash targets if anything still references them, but they no longer map to a second menu.

The typical case (base game: ~15 rooms, ~10 powers, ~13 traps+doors) fits without a scrollbar at
common resolutions; the scrollbar appears only for content-heavy campaigns that add rooms/powers.

---

## 3. Per-menu notes

- **`query_menu`** — progress bars (`gui_area_progress_bar_*` → `FeProgressBar`), player tallies
  (`gui_area_player_creature_info` / `_room_info` → `FeStatReadout` grid), ally toggles
  (`gui_toggle_ally` — **sends a packet**, MP-shared alliance state; keep the packet path),
  tend-to toggles (`gui_set_tend_to` → `kfx_sim_state.creatures_tend_imprison` / `_flee` —
  check packet routing). `maintain_prison_bar` gates imprison on having a prison.
- **`room_menu` / `trap_menu` / `spell_menu`** — grids per §2. Selecting one
  (`gui_choose_room` / `gui_choose_workshop_item` / `gui_choose_spell`) sets the player work-state
  — **via packet** (`set_players_packet_action` / work-state change). Right-click
  (`gui_go_to_next_room` etc.) cycles the camera to the next instance — a local camera action,
  check if packet-routed. Hover (`gui_over_room_button`) sets `gui_room_type_highlighted` (read
  by roomspace prediction — `roomspace_prediction.c`) — that global must still be set.
- **"Big" info areas** (`gui_area_big_room_button` etc.) — the hovered/selected item's name, cost,
  description. Text + a preview sprite. `FeBeginPanel` + `FeBodyText`.
- **`creature_menu`** — scrollable list (`FeBeginListBox` replaces `gui_scroll_activity_up/down` +
  `maintain_activity_*` + the manual windowing). Rows from `update_creatr_model_activities_list()`
  (`frontmenu_ingame_tabs.c:3007`) — keep that as the data source, or fold into the per-game-turn
  cache. Pick buttons (`pick_up_next_creature` / `pick_up_creature_doing_activity`) — **packet**
  (drops a creature into the hand). Anger bars → `FeProgressBar` variant.
- **`spell_lost_menu`** — the possession-view power subset; comes with
  [07-first-person-hud.md](07-first-person-hud.md).

---

## 4. MP-determinism ftest harness — built (2026-09-06)

This phase is the first to migrate menus that change shared state on every click, so the
determinism net was built first:

- **`src/ftests/ftest_packet_capture.{h,c}`** — snapshots `sim_packets[my_player_number]` once
  per game turn from `gameplay_loop_logic()` (right after `exchange_packets()`, before
  `process_packets()` clears it — the exact point the lockstep model exchanges), keyed by turn,
  filtered to action-bearing packets. Assertions: `ftest_packet_expect_once()`,
  `ftest_packet_expect_absent()`, `ftest_packet_trace_matches()` (golden array of
  `{action, par1..4, require_gui_flag}`, compared in order, ignoring turn/checksum/pos/mouse-button
  noise).
- **`ftest_util_gui_click(bid)` / `_turn_on_menu()` / `_gui_button_content()`** — drive an in-game
  GUI button by `BID_*` via `fake_button_click()` (no cursor / hit-test → headless-safe).
- **`ftest_gui_packet_parity`** (registered) — pins the legacy golden trace for tend-to-imprison
  (`PckA_ToggleTendency` par1=1), tend-to-flee (par1=2), computer-assist (`PckA_ToggleComputer`
  = 107), and asserts minimap zoom emits no packet. **Verified passing** headless against a real
  install (`cd core_files && ../out/ftest/keeperfx -ftests gui_packet_parity -headless
  -exitonfailedtest -nointro -nosound`).

**When a menu here is migrated:** extend `ftest_gui_packet_parity` (or add a sibling) to drive the
same clicks with `RendererSetImGuiEnabled(true)` + `ingame_imgui_menu_active()` true, and assert
`ftest_packet_trace_matches()` against the *same* golden array. Add rows as menus land:
`gui_choose_room` → `PckA_SetPlyrState`/`PSt_BuildRoom`, `gui_toggle_ally` → `PckA_PlyrToggleAlly`,
`pick_up_next_creature`, etc. Headless-safe; needs the proprietary DK data files (present at
`core_files/` on the dev machine, absent from CI).

---

## 5. Checklist (per `GMnu_*`)

- [ ] Grid/list iterates config / dungeon state directly — no `GuiButtonInit` array mutation.
- [ ] Second page dropped (§2.1): `GMnu_ROOM2`/`SPELL2`/`TRAP2`, `spell_menu2_buttons` &c.,
      `gui_set_page`, `maintain_*_next_page_button` removed; grid scrolls on overflow; every
      `panel_tab_idx` 1..32 slot assumption re-checked.
- [ ] Every action that sends a packet today still sends the same packet on the same turn.
- [ ] Hover globals (`gui_room_type_highlighted` etc.) still set for roomspace prediction.
- [ ] Count badges / availability / anger / progress from the per-game-turn cache.
- [ ] `FeIconGrid` / `FeProgressBar` / `FeStatReadout` / `FeBeginListBox` wrappers built as needed.
- [ ] `update_*_tab_to_config` and the static placeholder array rows removed for that menu.
- [ ] ftest: packet-stream parity for that menu's actions.
- [ ] Both `-classicmenu` states; SP + MP; layering; both builds; Catch2.

## 6. Open questions — resolved by code investigation (2026-09-06)

- **Tab actions are almost all packet-routed already** — good news, the `_by_index` extraction
  just has to preserve the `set_players_packet_action` call:
  | Handler | Packet |
  |---|---|
  | `gui_choose_room` → `activate_room_build_mode` | `PckA_SetPlyrState` / `PSt_BuildRoom` / rkind |
  | `gui_choose_workshop_item` → `choose_workshop_item` | `PckA_SetPlyrState` / `manufctr->work_state` |
  | `gui_choose_spell` → `choose_spell` | `PckA_SetPlyrState` (power work_state) |
  | `gui_go_to_next_room` → `go_to_my_next_room_of_type_and_select` | `PckA_ZoomToRoom` |
  | `gui_go_to_next_spell` → `go_to_next_spell_of_type` | `PckA_ZoomToSpell` |
  | `pick_up_next_creature` / `pick_up_creature_doing_activity` → `pick_up_creature_of_model_and_gui_job` | `PckA_UsePwrHandPick` (`thing_creature.c:5647`) |
  | `gui_set_tend_to` | `PckA_ToggleTendency` (already ftest-pinned) |
  | `gui_toggle_ally` | `PckA_PlyrToggleAlly` |
  | `gui_remove_area_for_rooms` / `_traps` | `PckA_SetPlyrState` / `PSt_Sell` |
  | `gui_set_query` | `PckA_SetPlyrState` / `PSt_CreatrQuery` |
  **Local-only:** `gui_switch_players_visible` (`info_page`, a display page counter for the query
  tab), `gui_set_page` (removed with §2.1). Extend `ftest_gui_packet_parity` with a row per
  migrated handler.
- **`update_creatr_model_activities_list()`** — called from `front_input.c:2946` (top of
  `get_gui_inputs`, every turn) and config-reload callbacks. Mutates `breed_activities[]` +
  `kfx_frontend_state.no_of_breeds_owned` / `.top_of_breed_list` from
  `dungeon->owned_creatures_of_model[]`. Cheap (~40-model loop). **Move the call into the
  per-game-turn cache refresh**; its `breed_activities[]` output *is* the creatures-tab row list.
- **`gui_room_type_highlighted`** (and `_door_/_trap_/_creature_type_highlighted`) — set at the
  top of `get_gui_inputs` to -1 each frame, then set by the `ptover_event` handlers
  (`gui_over_room_button` etc.) during the sweep; read by roomspace prediction
  (`roomspace_prediction.c`). An ImGui hover must set these in the same frame *before* the sim
  reads them — the ImGui submission runs at present time (after input), so the hover value is
  one frame stale for prediction. Acceptable (the legacy path's hover is also a frame old by the
  time the *next* turn's prediction runs); or set them from a `front_input.c`-time hook using
  last frame's hover, like the land-preview input trick.
- **`panel_tab_idx` is an integer, not a hard slot** — `PANELTABINDEX` config field (range 0-32
  for rooms, wider for traps/powers), and campaign scripts set it directly
  (`lvl_script_commands.c:5276`). The 1-16 / 17-32 page split lives only in
  `update_room_tab_to_config`. Dropping the 2nd page makes `panel_tab_idx` a plain ordering key;
  existing campaigns using 17-32 degrade gracefully to "further down the scroll". **Note the page
  split's removal in the config docs.**
- **Grid orientation decided** (user 2026-09-06): `FeIconGrid` is 4×N for the vertical layout,
  N×2 for the horizontal layout (each of room/spell/trap/creature a side-by-side block). Driven by
  the `HudLayout` ([05](05-sidebar-frame-and-minimap.md) §0). See §2.1.
