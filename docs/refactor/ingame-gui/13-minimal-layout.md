# Phase 13 — minimal layout (free-floating buttons + pop-up panels)

Status: **first landing, 2026-09-12** — the third `HudPanelLayout` (`Minimal`, `GUI_POSITION` value
4, `hud_position_type[]`). No persistent panel at all -- a small floating minimap+gold+event cluster
in the user-chosen upper corner (`MINIMAP_CORNER`), and a compact cluster of free-floating icon
buttons (Room / Spell / Trap / Creature / Query) in the diagonally opposite corner that each open a
pop-up panel instead of swapping a fixed tab-content area. The pop-up's *content* is the existing
vertical layout's own grid/panel functions, unchanged -- this phase was almost entirely about the
*container* (floating buttons + a persistent, non-click-away-dismissable popup) and the *chrome
placement* (corner-configurable minimap), not new game-facing behaviour.

**Landed as decided, §1–§5 in full:** `hud_layout_build()`/`hud_layout_frame()`
(`frontgui_hud_layout.{h,cpp}`) gained a `HudMinimalCorner corner` parameter (mirroring how
`b_width_mode` was added for Bottom) and `build_minimal()` -- a corner-pinned minimap/gold/events
column (`HudRegion_Minimap`/`Gold`/`Events`) and a diagonally-opposite button-cluster anchor +
pop-up rect (`HudRegion_TabStrip`/`TabContent`, the pop-up sized via a new shared
`vertical_panel_width()` helper extracted from `build_vertical_right()`, per the "reuse vertical
width/height" decision). `config_keeperfx.h`/`.c` gained `minimap_corner` + `MINIMAP_CORNER`
(`minimap_corner_type[]`, 1=upper-left/2=upper-right, default 1); `config_settingschema.c` added the
schema row (`is_enabled` gates it to `GUI_POSITION=MINIMAL` only). `frontgui_ingame_panel.cpp` gained
`draw_panel_minimal()` (aliases `s_menu_rect` to the minimap's corner rect for `render_minimap()`/
`draw_minimap_and_compass()`/`draw_nav_buttons()`, same trick `draw_panel_horizontal()` already
uses), `draw_event_markers_minimal()` (vertical stack below the minimap, same edge -- decided),
`minimal_cluster_button()` + `draw_button_cluster_minimal()` (icons only, no plinth chrome, in their
own auto-resize window pivoted off the opposite corner) with the toggle logic from §2.2
(`s_minimal_popup_open`, exposed read-only via `ingame_minimal_popup_is_open()`). `frontgui_ingame_
tabcontent.cpp` gained a third `ingame_tabcontent_draw()` branch: gated on
`ingame_minimal_popup_is_open() || query_priority` (never on a world click -- decided), drawing
`body()` inside its own plain-`WindowBg` window (no `relief::face()`/background-override call --
decided) sized off `HudRegion_TabContent`.

Verified: build (linux/ftest/windows, each needing the usual `cmake .` re-configure for the changed
header signatures to propagate), `check_layering.py --strict` clean, `gui_packet_parity` +
`gui_seam_ingame` individually with `GUI_POSITION=MINIMAL` set (both still pass -- packet-routed
click handlers are untouched, confirming §2.2's "purely a presentation toggle" claim), full
`-ftests` sweep with `MINIMAP_CORNER=UPPER_RIGHT` also set.

**Four fixes from the first live (non-headless) look, same day:**

1. **The pop-up silently ate clicks meant for the button row underneath, making it impossible to
   switch panels once one was open.** Root cause: `ImGuiWindowFlags_AlwaysAutoResize` on the button
   cluster's window means its *true* height is content size plus ImGui's own `WindowPadding` (not
   just the 40px button size `build_minimal()` assumed when reserving a flat 44px gap above it) --
   the pop-up's bottom edge ended up overlapping the cluster's real (taller-than-assumed) top edge,
   and since the pop-up is the more-recently-focused window wherever they touch, it captured the
   click instead of the button underneath. Fixed on both sides: `draw_button_cluster_minimal()`
   (`frontgui_ingame_panel.cpp`) now forces `ImGuiStyleVar_WindowPadding` to `(0,0)` so its window's
   real size exactly matches its content (`cluster_sz`, a literal both functions now comment as
   needing to stay in sync); `build_minimal()` computes the pop-up's bottom edge from that same
   `cluster_sz` plus a full `pad` of clearance, not a guessed constant.
2. **Event markers weren't pinned to the screen edge when the minimap was upper-right.**
   `draw_event_markers_minimal()` shares the minimap's own (much wider) column rect, and always
   drew each (much narrower) marker token starting at that column's *left* edge -- correct when the
   minimap is upper-left (left edge of the column = the actual screen edge), wrong when upper-right
   (there the column's *right* edge is the screen edge). The function now takes a `right` flag and
   aligns to `r.x1 - mw` instead of `r.x0` when the minimap's corner is on that side.
3. **The triangular nav-button pockets (M / + / − / cpu) near the minimap were removed for
   Minimal specifically** -- they read as too busy against the layout's own no-chrome aesthetic.
   Left/Right/Bottom are unaffected; this is a Minimal-only omission (`draw_nav_buttons()` is simply
   not called from `draw_panel_minimal()` any more).
4. **Gold moved below the minimap, and the minimap moved up to the very top of its corner** --
   the original gold-above-minimap ordering mirrored the vertical layout's own convention, but read
   as backwards once tried against Minimal's "the minimap is the anchor" framing.
   `build_minimal()`'s region order is now minimap first (starting at the corner's own `pad`), gold
   directly below it, events below that.

Re-verified after all four fixes: build (all three trees) clean, `check_layering.py --strict`
clean, `gui_seam_ingame` with `GUI_POSITION=MINIMAL`/`MINIMAP_CORNER=UPPER_RIGHT`, full `-ftests`
sweep with the same config, and a second full sweep back on the original Bottom/icon-pack config
confirming no regression to the other layouts.

**Three more fixes from a second live look (the first four's own screenshot), same day:**

1. **The minimap disappeared entirely** -- gold and the one active event marker still drew, but
   the minimap itself showed nothing. Root cause: swapping the vertical order (minimap now above
   gold, fix 4 above) moved `mm_r`'s own `y0` above `gold_r.y0`, but `draw_panel_minimal()`'s
   `whole` bounding rect (the window every element in this cluster draws into) was still computed
   from `gold_r.y0`, not `mm_r.y0` -- so the minimap's own draw calls landed entirely *above* the
   window's top edge, silently clipped by ImGui's automatic per-window scissor rect. Gold and the
   event marker, both below that edge, were unaffected, which is exactly what made this easy to
   miss. `whole` now starts at `mm_r.y0`.
2. **The pop-up still overlapped the button row slightly, even after the first fix.**
   `ImGuiWindowFlags_AlwaysAutoResize` sizes a window from the *previous* frame's content (a
   documented one-frame lag) -- forcing `WindowPadding` to zero (the first fix) made the
   steady-state size exact, but the button cluster's window could still be transiently a pixel or
   two off from what `build_minimal()`'s `cluster_sz` assumed. Replaced auto-resize with an
   explicit `SetNextWindowSize()` computed from the exact same `sz`/`gap`/`count` -- deterministic,
   no lag, nothing left to drift.
3. **Too much blank space between the pop-up's top edge and its actual content.** Every body
   function the pop-up calls (`room_grid()`, `query_panel()`, `creature_query_panel()`, ...) was
   authored against the vertical layout's full 400-unit virtual space, where content starts around
   `tcl::BODY_Y0`/`q::HEADER_Y0` (190) -- the space above that is minimap/gold/tabstrip chrome in
   the *real* vertical sidebar, which the pop-up doesn't have. Feeding the pop-up's real rect
   straight into `fe_hud_set_panel_rect()` reproduced that same "blank chrome zone" as empty space,
   since nothing draws into it here. Fixed in two parts: `build_minimal()` now sizes the pop-up to
   only the *content* fraction of the reference height (`(400-tcl::BODY_Y0)/400 ≈ 0.525`, so the
   window itself is shorter, addressing "could the popup panel be made shorter" directly), and
   `ingame_tabcontent_draw()`'s Minimal branch feeds `fe_hud_set_panel_rect()` a remapped *virtual*
   rect (scaled/shifted by that same fraction) so virtual-y=`BODY_Y0` lands exactly at the pop-up's
   real top edge instead of roughly half-way down it.

Re-verified after these three fixes too: build (all three trees) clean, `check_layering.py
--strict` clean, `gui_seam_ingame`/`gui_packet_parity` individually with `GUI_POSITION=MINIMAL` at
both `MINIMAP_CORNER` values, full `-ftests` sweep with `MINIMAP_CORNER=UPPER_RIGHT`, and a further
full sweep back on the original config confirming no regression. Still not visually confirmed after
*this* round specifically -- worth another look. Possession/query's own interface (§2.3) remains
flagged from the start as needing its own follow-up pass beyond reusing `creature_query_panel()`.

---

## 0. Why this is smaller than it looks

Two facts, found while checking what's already reusable, cut this down from "a new interaction
model built from scratch" to "a new container around existing content":

1. **The five tab bodies are already just functions taking a rect.** `room_grid()` / `spell_grid()`
   / `trap_grid()` / `creature_list()` / `query_panel()` (and `creature_query_panel()` for
   possession/query) all read their layout entirely from `fe_hud_set_panel_rect()`'s current rect
   via `grid_pt()`/`grid_sz()`/`grid_begin()` -- none of them know or care whether that rect belongs
   to a fixed sidebar panel or a floating popup. `ingame_tabcontent_draw()` (`frontgui_ingame_tabcontent.cpp`)
   already dispatches to the right one purely from `menu_is_active(GMnu_*)` state. **Minimal needs a
   third rendering mode in that same dispatcher** (alongside today's vertical/Bottom branches) that
   wraps `body()` in a floating auto-sized ImGui window instead of drawing into a fixed sub-rect --
   the content itself is a value-for-free reuse, exactly like Bottom's own grids were.
2. **The floating-icon-button-cluster idiom already exists, verbatim.** The pause-menu launcher
   (`options_launcher_frame()`, `frontgui_ingame.cpp:462`) is *already* a small auto-resize,
   no-decoration ImGui window, positioned with `ImGui::SetNextWindowPos()`, containing four
   `FeSpriteButton()` icon+label rows with no containing panel at all. That is the *exact* shape of
   Minimal's button cluster -- the same window flags, the same button primitive, just five buttons
   instead of four and a different fixed screen position instead of screen-centre.
3. **The minimap/nav-button code is already rect-driven via an aliasing trick, not layout-specific.**
   `render_minimap()` / `draw_minimap_and_compass()` / `draw_nav_buttons()` (`frontgui_ingame_panel.cpp`)
   all read the file-scope `s_menu_rect`, and `draw_panel_horizontal()` already proves the pattern
   Minimal needs: alias `s_menu_rect` to whatever rect you want the minimap drawn against for one
   frame (there, region A's rect; for Minimal, a corner-derived square), call the same three
   functions unchanged, done. No new minimap code, just a new rect to alias it to.

So the real new work is: (a) where the minimap/gold/buttons/event-markers sit on screen (§1), (b)
the pop-up's own container, sizing, background and open/close interaction (§2), and (c) the
settings-tab plumbing for the corner choice (§3). Everything else is calling existing functions
against a new rect.

---

## 1. Composition

No panel silhouette at all -- the 3D view is the whole screen (`viewport_inset = 0.0f`, same as
Bottom already does). Four independent floating elements:

```
+---------------------------------------------------+
|  [Gold]                              [message      |
|  [Minimap]                            queue, same  |
|  (user-chosen corner)                 spot as       |
|  event markers                        today's       |
|  (stacked with the minimap,           Left/Right]   |
|  same edge -- §1.3)            [Room][Spell][Trap]  |
|                                 [Creature][Query]    |
|                                (diagonally opposite  |
|                                 corner -- decided)   |
|                    -- 3D view --                    |
+---------------------------------------------------+
```

### 1.1 Minimap + gold -- corner-configurable

Reuses `render_minimap()`/`draw_minimap_and_compass()` exactly as Bottom's `draw_panel_horizontal()`
already does: alias `s_menu_rect` to a small square sized off screen height (same `mm_side`-style
clamp `build_horizontal_bottom()` already uses), positioned at whichever of the 2 upper corners
`MINIMAP_CORNER` (§5) picks, then call the two functions unchanged. **Revised after the first live
look:** `draw_nav_buttons()` (the M/+/−/cpu triangular corner pockets) is *not* called for Minimal --
too busy against the layout's own no-chrome aesthetic, Minimal-only omission. Gold sits directly
*below* the minimap (not above it, unlike every other layout's own convention) -- the minimap itself
moved to the very top of its corner; "gold above minimap" read as backwards once tried against
Minimal's own "the minimap is the anchor" framing. No new gold-drawing code either way, just a new
rect (`draw_gold_horizontal()`, already generic).

### 1.2 The button cluster -- free-floating, reusing tab iconography

Five buttons (Room/Spell/Trap/Creature/Query), same icons/colorize/glyph `TabSpec` table
`draw_tabs()`/`draw_tabs_horizontal()` already build (`frontgui_ingame_panel.cpp`), drawn via
`FeSpriteButton()`-style icon buttons in their own small auto-resize window (§0.2's launcher-window
precedent) rather than a channel-and-plinth tab strip -- "minimal" reads as *icons only*, no
recessed chrome around them, matching the layout's own name. **Decided: the button cluster sits in
the corner diagonally opposite the minimap** (one setting drives both -- not two independently
configurable corners), so the two clusters never compete for the same screen quadrant.

### 1.3 Event markers + message queue

`draw_message_queue()` (`frontgui_ingame_text.cpp`) already floats independently of panel
*position* -- confirmed true across Left/Right already, and it computes its offset from
`status_panel_width` (0 for Minimal, same as Bottom) rather than any per-layout rect, so it needs
**no change at all**. **Decided: for the first pass, leave it at its existing Left/Right position
unchanged** rather than trying to relocate it relative to the new corners -- simplest starting point,
flagged as likely to change once this is live-tested (the user's own framing: "might change
significantly on 2nd pass").

**Decided: event markers stack alongside the minimap, same edge, rather than an independent fixed
edge.** `draw_one_event_marker()` (shared between the vertical stacked and Bottom horizontal-row
shapes) needs a *third* placement shape for Minimal: a vertical stack running down the same
screen edge the minimap's corner sits on (above or below it depending on which vertical half of
the screen that corner is in), so the two elements read as one cluster rather than two unrelated
floating groups. Exact spacing/ordering is an implementation-time detail, not a design question --
this is also flagged as a first-pass placement likely to be revisited.

---

## 2. The pop-up panel

### 2.1 Container

A new floating-window shape in `ingame_tabcontent_draw()` (`frontgui_ingame_tabcontent.cpp`),
alongside its existing `if (bottom) {...} else {...}` branches: for Minimal, `body()` draws inside
its own `ImGui::Begin("##IngameMinimalPopup", nullptr, ImGuiWindowFlags_NoDecoration |
ImGuiWindowFlags_NoSavedSettings)` (the exact flag set the options launcher and event box already
use), `ImGui::SetNextWindowPos()`-anchored just off the button cluster (below it if the cluster is
in a top corner, above it if bottom -- mirroring how a real popup menu opens away from the screen
edge it's anchored to).

**Decided: size the popup by reusing the vertical layout's own panel width/height math** --
`build_vertical_right()`'s `panel_w = h * 0.26` (clamped `[200, 360]`, further capped at `w * 0.32`)
and its own tab-content height fraction, rather than inventing a new fixed pixel size or a
screen-fraction that would grow awkwardly large on 4K. The popup ends up the same shape/size a
Left/Right sidebar's tab-content area already is -- just floating instead of docked to a screen
edge.

**Decided: background is the message-box's plain plate, not the procedural marble.** `textinfo_frame()`
/`battlemenu_frame()` (`frontgui_ingame.cpp`) get their background "for free" -- they don't call
`relief::face()`/`draw_face_or_override()` at all, don't pass `ImGuiWindowFlags_NoBackground`, and
so simply show ImGui's own styled `WindowBg` fill (`frontgui_style.cpp`'s bronze/parchment palette).
The Minimal popup does the same: no custom relief drawing, just the window's native background --
reads as a lightweight floating plate rather than a chunk of stone panel architecture, matching
"minimal" as an aesthetic as well as a layout. (The icon-pack background-override feature, Phase 12
§7, is specifically about the *procedural* fill this popup is deliberately **not** using -- not
applicable here.)

### 2.2 Open/close interaction

**Decided: a popup-visibility flag independent of the legacy tab-selection state**, not a change to
`menu_is_active()`/`gui_set_menu_mode()`'s existing radio-group behaviour (§4.2 of the overview doc
-- "the menu-stack machinery... does not get replaced"). Concretely: clicking a button still calls
the same `gui_set_menu_mode`-routed action every tab click already does (so `menu_is_active(GMnu_*)`
stays the single source of truth for *which* tab is logically selected, unchanged from the other two
layouts, and possession/query's priority dispatch keeps working untouched) -- but Minimal *also*
tracks its own `bool s_popup_open`, set `true` on any button click and flipped to `false` when the
*already-active* button is clicked again (clicking a *different* button leaves it `true` and swaps
which body draws inside it -- "opens the new panel in its place"). The popup only draws while
`s_popup_open` is true; the button row itself (and the underlying selection) doesn't need it to be
open at all. This keeps every existing packet-routed click handler and the packet-parity ftest
untouched -- purely a presentation toggle layered on top.

**Decided: no click-away-to-dismiss.** Clicking the 3D view while a popup is open must *not* close
it -- room/spell/creature selection routinely needs several panel→3D→panel→3D clicks in a row (pick
a room type, click a tile, check the panel again, click another tile), and dismissing the panel on
the first world click would break that workflow. The only ways to close a popup are the two in
§2.2's first paragraph: re-click its own button, or click a different button (which swaps it rather
than closing then reopening).

### 2.3 Possession / query

Unchanged from both existing layouts: `in_possession || menu_is_active(GMnu_CREATURE_QUERY*)` still
takes dispatch priority over the button row's own selection (`ingame_tabcontent_draw()`'s existing
priority check, verbatim), and the popup opens automatically the same way the vertical layout's
tab-content area already does when possession starts (no user click needed) -- `s_popup_open` forces
`true` whenever that priority branch is the one drawing.

**Decided for the first pass: reuse `creature_query_panel()` (the vertical layout's version --
Abilities/Stats toggle intact) verbatim**, since the popup's proportions (§2.1) are the vertical
layout's own tall-narrow shape, not Bottom's wide-short one. **Flagged directly by the user as
likely needing its own follow-up pass** -- first-person/possession in a layout with no persistent
panel at all is a bigger interface question than "which existing panel function to call," and isn't
being fully worked through here.

---

## 3. What this implies for the code (not this pass -- for scoping only)

- `frontgui_hud_layout.{h,cpp}`: `build_minimal()` replaces `build_stub()` for `HudLayout_Minimal`.
  Needs at minimum a `HudRegion_Minimap` rect (the corner square) and a `HudRegion_TabStrip`-shaped
  rect for the button cluster's own position (reusing that enum slot's *name* loosely -- it's not a
  strip any more, but region-per-cluster is still the right shape); `HudRegion_TabContent` is
  probably unused (the popup's rect is computed fresh at open time from the button cluster's rect,
  not read from a persistent region) -- worth confirming once the popup's exact anchoring math is
  written.
- `config_keeperfx.h`/`.c` + `config_settingschema.c`: `hud_position_type[]` gains `{"MINIMAL", 4}`
  (the slot the existing comment already reserves); a new `MINIMAP_CORNER` setting (2-entry
  compile-time enum -- upper-left/upper-right, default upper-left, no runtime folder-scan needed
  this time, unlike `GUI_ICON_PACK`/`UI_FONT`), `SApply_Live`.
- `frontgui_ingame_panel.cpp`: new `draw_panel_minimal()` alongside `draw_panel_vertical()`/
  `draw_panel_horizontal()` -- floating gold + minimap (§1.1's `s_menu_rect`-alias trick), event
  markers stacked with the minimap (§1.3), the button-cluster window (§1.2). `draw_message_queue()`
  itself is untouched (§1.3 -- stays at its existing Left/Right position for this pass).
- `frontgui_ingame_tabcontent.cpp`: `ingame_tabcontent_draw()` gains a third branch (Minimal) next
  to today's `if (bottom) {...} else {...}`, drawing `body()` inside the floating popup window
  (§2.1 -- vertical-layout-sized, plain `WindowBg`, no `relief::face()` call at all) gated on
  `s_popup_open` (§2.2) instead of always-on.
- No layering change anywhere in this list -- everything above is already `kfx_frontend` /
  `kfx_config`, same as every prior phase in this series.

---

## 4. Decided (2026-09-12)

- Button cluster sits diagonally opposite the minimap -- one setting drives both corners, not two
  independent ones (§1.2).
- Event markers stack alongside the minimap on the same edge, not an independent fixed edge (§1.3)
  -- flagged by the user as a first pass, likely to change significantly once tried live.
- Message queue stays at its existing Left/Right screen position unchanged, for now (§1.3) -- same
  "first pass, may change" caveat.
- No click-away-to-dismiss, ever -- re-click or click-a-different-button are the only close paths,
  because room/spell/creature selection needs repeated panel↔3D clicks (§2.2).
- Popup size reuses the vertical layout's own panel width/height math, not a bespoke size (§2.1).
- Popup background is the message-box's plain `WindowBg` plate, not the procedural marble fill --
  no `relief::face()` call for this container at all (§2.1).
- Possession/query reuses `creature_query_panel()` (vertical) verbatim for the first pass, with
  first-person/possession under a no-persistent-panel layout explicitly flagged as needing its own
  follow-up thought later (§2.3).

## 5. Decided (2026-09-12, second round) -- nothing left open

- **`MINIMAP_CORNER` is a 2-value choice, not 4**: upper-left / upper-right only (no bottom
  options). **Default: upper-left.** The button cluster stays diagonally opposite (§4) -- so
  upper-left minimap → lower-right buttons, upper-right minimap → lower-left buttons.

Every open question from this doc's first draft is now resolved. Proceeding to implementation.

## 6. Post-landing fixes (2026-09-12, live-tested)

Two bugs found trying the layout live, both fixed in `frontgui_ingame_tabcontent.cpp`'s Minimal
branch and `front_input.c`:

- **Mouse-wheel over the pop-up also zoomed the 3D view underneath it.** Root cause was the general
  ImGui-migration bug class this series has hit before (`point_is_over_gui_menu()`,
  `mouse_is_over_side_panel_bottom()`): `get_dungeon_control_nonaction_inputs()`
  (`front_input.c`) sets `PCtr_MapCoordsValid` from `screen_to_map()` alone, with no idea an ImGui
  panel is drawn on top -- `get_isometric_or_front_view_mouse_inputs()` then reads that flag to
  drive wheel-zoom. Fixed by gating the whole position-setting block on `!game_is_busy_doing_gui()`,
  which `get_gui_inputs()` (called earlier in the same `get_inputs()` frame) already keeps correct
  via its own `ingame_imgui_wants_mouse()` check -- no second query needed.
- **No border around the pop-up**, unlike the message box. `ingame_imgui_frame()`
  (`frontgui_ingame.cpp`) wraps the whole HUD in a `WindowBorderSize=0` push (composite-over-gameplay
  windows read a stray gold line as a bug) -- right for the minimap/button-cluster's transparent
  chrome, wrong for this pop-up's opaque plate. Fixed by pushing the style's own 1.5f default back
  just around this one window's `Begin()`.

A third, general (not Minimal-specific) bug surfaced in the same session and is covered by the
in-game-GUI fixes below rather than this doc: clicking an objective event marker with no map
location jumped the camera to the map origin -- see `frontgui_ingame.cpp`'s
`event_box_button_column()`.
