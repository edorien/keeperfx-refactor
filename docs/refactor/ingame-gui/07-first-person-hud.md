# Phase 6 — the first-person / possession HUD

Status: **started 2026-09-07 — chunk 1 (shared creature-query panel) landed, see §4.** Depends on
[06-tab-content-panels.md](06-tab-content-panels.md) (shares its wrappers and the `creature_query` pages). See [00-overview.md](00-overview.md) §5 —
**in scope** because the interface must be stylistically consistent with the top-down HUD.

Goal: the HUD shown while possessing a creature (`PVT_CreatureContrl` / `PVT_CreaturePasngr`) —
minimap + creature portrait + life bar + the 2×5 instance icon-and-timer table, plus the query
detail pages (page-down button, numeric stat readouts + icons).

---

## 1. What it is

`toggle_first_person_menu()` (`frontend.cpp:2589`) is the switch — called from `set_gui_visible()`
for the `PVT_CreatureContrl` / `PVT_CreaturePasngr` view types (`frontend.cpp:2646`). It shows one
of `GMnu_CREATURE_QUERY1..4` via `set_menu_visible_on/off`, remembering which page was up
(`creature_query_on` static).

So the first-person HUD is **the same `creature_query_menu1..4` menus** as the top-down
"query a creature" panels (`creature_query_buttons1..4`, `frontmenu_ingame_tabs_data.cpp:383-441`),
shown in possession instead of over the dungeon-top view:

- **Page 1** (`creature_query_buttons1`): anger smiley bar, experience bar, name/health line,
  6 instance buttons (`gui_area_instance_button` — the creature's spells/abilities with cooldown
  timers), next-page.
- **Page 2** (`creature_query_buttons2`): same header, instances 4..9.
- **Page 3** (`creature_query_buttons3`): name/health, 11 stat readouts (`gui_area_stat_button` —
  health, strength, armour, defence, luck, dexterity, wage, gold held, age, kills).
- **Page 4** (`creature_query_buttons4`): 11 more stats (speed, loyalty, research/manufacture/
  training/scavenge skill, training/scavenge cost, weight, blood type).

Backgrounds: `gui_creature_query_background1` / `_background2`.

The "2×5 table of icon + times" in the brief = the instance buttons on pages 1–2
(`gui_area_instance_button`, `maintain_instance` — draws the ability icon + its cooldown countdown).

---

## 2. Approach

- **Reuse Phase 5's wrappers** — `FeStatReadout` (the ~40 stat rows), `FeProgressBar` (anger, xp,
  health), `FeIconButton` with a countdown overlay (the instance buttons), `FeBeginPanel`.
- **Portrait + life bar** — the possessed creature's head sprite (dynamic-texture thumbnail, same
  as `draw_battle_head` / Phase 3) + a health `FeProgressBar`.
- **Minimap** — the same `FeMinimap` from Phase 4 (`set_gui_visible`'s `PVT_MapScreen` path
  already shows a minimap variant; possession shows the panel minimap).
- **Pages — collapse the 4 to 2** (decided, user 2026-09-06): one **"Abilities"** section
  (anger + xp bars, name/health line, all instance buttons with cooldown overlays) and one
  **"Stats"** section (all ~22 `FeStatReadout` rows in an `FeBeginScrollArea`). A 2-tab strip
  replaces the next-page button. `creature_query_on` (the remembered page, `frontend.cpp:2589`)
  collapses to a remembered tab (0/1). `creature_query_buttons1..4` and their `parent_menu`
  next-page chain go away.
- **Instance activation** — clicking an instance button in possession *casts the ability*
  (`instant_instance_selected`, `frontmenu_ingame_tabs.c:3081`) — **packet-routed** (it is a
  creature control action). Keep that path exactly.

---

## 3. Entanglements / risks

- **`PVT_CreatureContrl` view code** — the possession view is `engine_render.c`'s first-person
  camera; the HUD composites over it the same way the top-down HUD composites over the dungeon
  view. `draw_gui()` runs for possession too (`engine_redraw.c`'s per-view-type blocks). Check
  which of the 3 `redraw_display` blocks is the possession one and that the ImGui submission
  covers it.
- **`spell_lost_menu` (`GMnu_SPELL_LOST`)** — the keeper-power grid shown in possession (you can
  still cast some powers while possessing). Comes with this phase or Phase 5; it is a reduced
  `spell_menu`.
- **`set_gui_visible` view-type switch** — entering/leaving possession swaps the whole panel
  contents (`toggle_status_menu` ↔ `toggle_first_person_menu`). The ImGui submission must switch
  cleanly on the same signal, and `creature_query_on` page memory must survive the round trip.
- **Passenger vs. control** (`PVT_CreaturePasngr` — riding without controlling) shows a reduced
  HUD; check the difference.
- **First-person input** — possession input (`can_process_creature_input`,
  `process_first_person_look`) is in the packet path; the HUD must not eat mouse-look. Only
  `busy_doing_gui` when actually over a panel widget.
- **Instance cooldown timers** update per game-turn → per-game-turn cache; the countdown *display*
  can interpolate per frame if wanted.

## 4. Checklist

**Chunk 1 landed 2026-09-07 — the shared creature-query panel (top-down testable).**
`frontgui_ingame_tabcontent.cpp::creature_query_panel()`, dispatched from `ingame_tabcontent_draw`
when any `GMnu_CREATURE_QUERY1..4` is active; all four added to `menu_is_migrated()`. Reads
`player->controlled_thing_idx` (set to the queried creature by `set_selected_creature` in
`pinstfs_query_creature`, and to the possessed creature in possession — same field works for both).
Name/health header (click → `gui_query_next_creature_of_owner`, R-click → `_and_model`, both
`PckA_PlyrQueryCreature`). 2-tab strip (ABILITIES / STATS) replaces the next-page button — the
legacy `front_input.c` / `gui_frontmenu.c` page-nav still runs but every `GMnu_CREATURE_QUERY*`
maps to this one panel so it's a visual no-op (neuter it in a later chunk). ABILITIES = anger
smiley bar + xp bar + instance grid (icon + cooldown bar, `+1` disabled frame, active-instance
ring; display-only — legacy instance buttons have no click handler, activation stays on the number
keys). STATS = scrollable 20-row `creature_statistic_text` readout.

**Chunk 1 polish + page-nav neuter (2026-09-07, retest round 1):** header reworked — legacy
`CGI_QuerySymbol` portrait top-left, stacked anger/xp bars right of it, a health bar with the name
centred inside it (no numbers). Abilities: 3-wide grid, fat cooldown bars (~30% of cell), the
hotkey number (1..9/0 = the possession number key) in the top-left corner, per-cell tooltip
(`InstanceInfo::tooltip_stridx`). Stats: 2-per-row bordered `stat_cell`s in a scroll child, each
with its `GUIStr_Creature*Desc` tooltip via `ImGui::SetTooltip`. **Legacy page-nav neutered** when
`ingame_imgui_menu_active(GMnu_CREATURE_QUERY1)`: `gui_frontmenu.c::update_query_menu` early-returns,
`front_input.c` passenger + control page chains skipped (`imgui_query` guard). Next/prev-instance
keys and the number-key instance activation (`get_creature_control_action_inputs` `numkey` block)
are untouched — they still work.

**`GMnu_SPELL_LOST` landed (2026-09-07)** — `spell_lost_panel()`, dispatched like the other tab
bodies; added to `menu_is_migrated()`. It is a top-down state (you lost your dungeon heart), not a
possession panel: legacy is one usable button (the possess icon → `PckA_GoSpectator`) plus 15 empty
frames. ImGui version = the possess-icon cell + the `CpgStr_LevelLost` line. Seam ftest's
"not migrated" negative example moved `GMnu_SPELL_LOST` → `GMnu_EVENT`.

**Possession HUD needs no extra work for the frame** — `GMnu_MAIN` stays on through possession
(`turn_off_all_window_menus` / `turn_off_all_panel_menus` never touch it), and possession routes
through `redraw_creature_view()` → the same `draw_whole_status_panel` / `draw_gui`, so the ImGui
sidebar frame (minimap, gold, compass) + this query panel already composite over the first-person
view. `pinstfe_direct_control_creature` turns `GMnu_CREATURE_QUERY1` back on, which this panel owns.

**Retest round 2 (2026-09-07):** possession showed the *pre-possession* tab body instead of the
query panel — `pinstfs_passenger_control_creature` only calls `turn_off_all_window_menus` (not
`turn_off_all_panel_menus`), so `GMnu_CREATURE` (or whatever tab was up) stays `menu_is_active`
under the later `turn_on_menu(GMnu_CREATURE_QUERY1)`, and the dispatch matched the tab first. Fixed:
`ingame_tabcontent_draw` now checks the query / `SPELL_LOST` menus **before** the tab bodies, and
also keys off `player->view_type == PVT_CreatureContrl / PVT_CreaturePasngr` directly (the query
menu can lag a frame behind the view switch). Stats grid tightened (12-virtual-unit rows, 1px
gaps). Tooltips word-wrapped (`wrapped_tooltip` — `BeginTooltip` + `PushTextWrapPos`) so the long
`GUIStr_*Desc` sentences don't run off-screen.

**Still to do: instance-click-to-cast, passenger-vs-control HUD difference, interactive verification
of the possession path.**

- [ ] `toggle_first_person_menu` → ImGui submission arm; page memory preserved.
- [ ] Portrait thumbnail + health bar.
- [ ] Instance icon+timer table (`FeIconButton` + countdown); activation packet-routed unchanged.
- [ ] Stat pages via `FeStatReadout`; page selector replaces page-down button.
- [ ] `FeMinimap` in possession.
- [ ] `spell_lost_menu` reduced power grid.
- [ ] Passenger vs control HUD difference reproduced.
- [ ] Mouse-look not captured by the HUD; `busy_doing_gui` only over widgets.
- [ ] Both `-classicmenu` states; layering; both builds; Catch2. Interactive: possess, cast an
      ability, check every stat page, leave possession.

## 5. Open questions

**Decided (user 2026-09-06):** collapse the 4 query pages to 2 — "Abilities" + a scrollable
"Stats" section. See §2.

**Resolved by code investigation (2026-09-07, after Phase 5 completed):**

- **Which `Thing` the query pages read is split in the legacy code:** the name/health nav button
  (`gui_query_next_creature_of_owner` / `_and_model`) walks `player->influenced_thing_idx` and
  re-queries via `PckA_PlyrQueryCreature` (packet-routed, arg = next creature index). But
  `gui_area_smiley_anger_button`, `gui_area_experience_button`, `gui_area_instance_button` and
  `gui_area_stat_button` all read `player->controlled_thing_idx`. So in top-down query mode
  `PckA_PlyrQueryCreature`'s handler must also set `controlled_thing_idx` (or query mode does) —
  **confirm this before wiring the ImGui panel**, and read the same field the legacy draw used for
  each widget so possession and top-down both work.
- **Instance button cooldown:** `turns_required = fp_reset_time` when `TAlF_IsControlled`, else
  `reset_time`; `turns_progress = gameturn - instance_use_turn[id] + inst_action_turns -
  inst_total_turns`. Disabled (draw `symbol_spridx + 1`, greyed) when not reset, or frozen/chicken
  and the instance disallows self-cast in that state. Active instance highlighted when
  `cctrl->active_instance_id == id`. `maintain_instance` sets `gbtn->sprite_idx =
  inst_inf->symbol_spridx`, `tooltip_stridx = inst_inf->tooltip_stridx`.
- **Cast action:** `instant_instance_selected(CrInstance)` (`frontmenu_ingame_tabs.c:3099`) —
  packet-routed creature-control action; keep verbatim.
- **Anger:** `anger_get_creature_highest_anger_type_and_byte_percentage(thing, &type, &pct)` →
  smiley sprite `GPS_symbols_creatr_mood_vhappy_std + clamp(5*pct/256, 0, 4)`.
- **XP:** `points_progress = cctrl->exp_points`, `points_required = crconf->to_level[exp_level]
  << 8`; level label = `exp_level + 1`.

**Resolved by code investigation (2026-09-06):**

- **Possession is `redraw_creature_view()`** (`engine_redraw.c:535`), reached via
  `redraw_display()` → `case PVM_CreatureView`. It calls the **same**
  `render_overlay->draw_whole_status_panel()` / `->draw_gui()` / `->message_draw()` /
  `->gui_draw_all_boxes()` sequence as the top-down views — so **Phase 0's ImGui submission arm
  reaches possession with no extra hookup**. `draw_gui()` draws whatever menus are active;
  `toggle_first_person_menu()` swaps `creature_query_menu1..4` in.
- **`PVT_CreaturePasngr` uses the same HUD as `PVT_CreatureContrl`** — `set_gui_visible()`
  (`frontend.cpp:2646-2648`) has both `case`s fall through to `toggle_first_person_menu(is_visbl)`.
  No separate layout; passenger just can't act.
- **The top-down "query a creature" panel is the same `creature_query_menu1..4`** — used by both
  `PSt_CreatrQuery` (top-down) and possession. **Migrate it once, here** (Phase 5 already defers
  it to this doc); both paths get it.
