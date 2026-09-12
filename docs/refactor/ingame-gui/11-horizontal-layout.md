# Phase 10 — horizontal HUD layout (DK2-style bottom bar)

Status: **first landing, 2026-09-11** — `GUI_POSITION` gained `Bottom`; region A (minimap+nav+gold),
region B's tab-header row + Room/Spell/Trap grids reflowed to 6 columns (the original plan's 2 left
most of region B empty, live-tested — 6 fills it), and region C (message queue + horizontal event
markers) are all live. `frontgui_hud_layout.h`'s long-stubbed `HudLayout_HorizontalBottom` is now
real geometry, selected live from the setting the same way Left/Right already were.

**Creature tab and possession/query landed too (same day):** `creature_list_horizontal()` and
`creature_query_panel_horizontal()` (`frontgui_ingame_creature.cpp`) implement the creature-cell and
possession-view proposals below — see those sections for the design, now built as described
(cropped portrait + overlaid counts; header strip with Abilities/Stats toggle cells in place of the
5 tab icons). Wired into `ingame_tabcontent_draw()`'s dispatch alongside Room/Spell/Trap.

**Region B/C reshaped from the original plan, live-tested:** region C (message queue + event
markers) is now a *fixed* width flush against the right screen edge, and region B (the grid) was
briefly the *flexible* one filling whatever's left between region A and region C — the opposite of
the original proposal below, which fixed B's width and let C balloon on wide screens while the grid
stayed a fixed, cramped size. That overcorrected ("too wide, icons take up too much space, spread
apart with wide gaps") — region B is now *capped* at a size sized for 6 comfortably-spaced columns
(`std::min(std::max(strip_h * 2.2f, 440.0f), w * 0.5f)`), not the screen; any leftover width between
regions B and C is just blank panel face. `grid_geom()` (`frontgui_ingame_cells.cpp`) also now
subtracts ImGui's own scrollbar width before dividing region B's width into 6 columns — the 6th
column was landing under/against the scrollbar otherwise.

**Creature tab redesigned again after live testing** — cropped portrait as a *background* (not the
foreground cell content Room/Spell/Trap use), idle/work/fight stacked in a column rather than
scattered at three corners, and a single row that scrolls *horizontally* rather than a 6-column
wrapping grid, matching DK2's own creature panel shape instead of a generic item grid:
`creature_cell_horizontal()` now uses `CGI_QuerySymbol` (the same portrait `creature_query_panel()`
uses, not the small `CGI_HandSymbol` icon) via `blit_cover()`, with the idle/work/fight numerals in
a narrow translucent-backed column down the cell's left edge and the owned count top-right over the
portrait. `creature_list_horizontal()` lays these out in one `BeginChild(...,
ImGuiWindowFlags_HorizontalScrollbar)` row instead of `grid_begin()`'s wrapping grid, with the mouse
wheel mapped to horizontal scroll (`SetScrollX`) the same way `creature_list()`'s own wheel-paging
already repurposes the wheel for this list specifically.

**Possession/query redesigned again too** — an ability instance has a hard cap of 10
(`creature_instances.h`), so `creature_query_panel_horizontal()`'s ability grid is now a *fixed* 5
rows × 2 columns (every ability visible at once, never scrolled) rather than a 6-column wrap. The
portrait grew to 70% of the panel's height with the anger/xp bars now *horizontal* underneath it
(same width as the portrait) instead of vertical bars beside it; health became a *vertical* bar
alongside the portrait block instead of a horizontal one across the top.

**Region B width made per-tab, possession ability grid flipped, toggle dropped (same day, next
live-test round):** capping region B at one size for every tab (previous paragraph) traded one
problem for another — the creature strip could then only ever show 5 creatures at a time, and
possession/query had a lot of unused whitespace to its right. `HudBottomWidthMode`
(`frontgui_hud_layout.h`) replaces the single cap with three: `Normal` (the existing cap, for
Room/Spell/Trap), `Wide` (fills all the way to region C's edge — the creature strip), and
`Narrow` (smaller than Normal — possession/query). `build_horizontal_bottom()`
(`frontgui_hud_layout.cpp`) switches on it per frame; `determine_bottom_width_mode()`
(`frontgui_ingame_panel.cpp`) decides which mode from the same possession/query/creature-menu
checks `ingame_tabcontent_draw()` already makes, run a second time since panel.cpp computes the
layout before tabcontent.cpp dispatches. Separately, live-testing possession's 2-column × 5-row
ability grid found the icons rendered too small (region B's height is limited, so 5 short rows
squeezed each icon down) — flipped to 5 columns × 2 rows, which reads closer to DK2's own
possession bar besides being easier to see. The Abilities/Stats toggle came out entirely rather
than kept or expanded to show both at once (the two alternatives on the table): with `Narrow`
now freeing up space and the grid seating all 10 abilities without scrolling, there was nothing
left for the toggle to switch *to* in the horizontal layout specifically — the vertical layout's
`creature_query_panel()` (Left/Right/today's default) is untouched and keeps its full
Abilities/Stats paging including the Stats page, which the horizontal view now has no equivalent
for. Worth a second look if that's missed in play.

**Query/info panel landed for Bottom too; creature strip's header and portraits
adjusted again (same day, next live-test round):** `query_panel_horizontal()`
(`frontgui_ingame_creature.cpp`) replaces the `tab_not_yet_available_horizontal()`
placeholder for `GMnu_QUERY` -- `HudBottomWidth_Normal` (no special case needed
in `determine_bottom_width_mode()`), reflowing `query_panel()`'s stacked rows
into columns across region B's wide-short rect: tendency toggles | payday/
research/workshop bars | up to 4 per-player rows | query-mode + MP page-cycle,
left to right, same widgets and packets throughout. Separately, live-testing
the creature strip found the global IDLE/WORK/FIGHT header -- 3 word buttons
across a horizontal band eating into the strip's height -- read wrong next to
DK2's icon-based equivalent; it's now a vertical column of 3 icon cells (the
same `GPS_rpanel_tab_crtr_wandr_act`/`_work_act`/`_fight_act` sprites the
vertical layout's own `BID_CRTR_NXWNDR`/`NXWRKR`/`NXFIGT` buttons use) down
the panel's left edge, freeing the whole panel height for the scroll strip
rather than losing a band off the top to the header. `creature_cell_horizontal()`'s
per-model portrait switched from `blit_cover()`'s crop-to-fill back to
`blit_fit()`'s letterbox -- `blit_cover()` itself is now dead code and was
removed. The per-cell idle/work/fight numeral column (translucent strip down
the cell's own left edge, distinct from the panel-level header column above)
is unchanged.

**Query/info panel reflowed again, and TabStrip decoupled from
`b_width_mode` (same day, next live-test round):** `query_panel_horizontal()`'s
three-column shape (tendency toggles | bars+players | query-mode) put payday
in the same one-third-height slot as research/workshop, which live-tested as
"too short" for how much spare width the bars-and-players column had going
begging. Payday is now its own full-width, thin row at the top of a merged
info column; research and workshop sit side by side on a row below it; the
per-player room/creature rows fill whatever height is left below that --
top to bottom, most-glanced-at first. Separately, live-testing surfaced that
`HudRegion_TabStrip` (the 5 tab-header icons) shared `region_b_w` with
`HudRegion_TabContent`, so switching to a tab with a different
`HudBottomWidthMode` (e.g. the creature tab's Wide) visibly resized and
reflowed the tab row itself -- the navigation control moving under the
finger that just clicked it. `build_horizontal_bottom()`
(`frontgui_hud_layout.cpp`) now pins `TabStrip` to the Normal-mode width
unconditionally; only `TabContent` (the grid/panel body below it) still
follows `b_width_mode`.

**Deliberately deferred, not silently dropped:** the top-down "?" Query tab and `spell_lost_panel`
(the post-dungeon-heart-loss screen) are still built entirely from `tcl::` constants tuned for a
tall vertical panel, which would read as a squashed mess against region B's wide-short rect —
selecting either while Bottom is active shows a plain "Not yet available in this layout"
placeholder (`frontgui_ingame_tabcontent.cpp`) instead. `info_band()` (the selected/hovered item
strip above the Room/Spell/Trap grid) is likewise skipped for Bottom for now — it would overlap the
grid rather than sit above it, since region B's grid claims the whole panel rect rather than
carving out a header slice the way the vertical layout's `tcl::INFO_Y0..Y1` does.

Below this line is the original planning writeup, kept as-is — it's still the design contract for
the deferred pieces.

**Two bugs found by interactive testing, fixed same day:**

- **World clicks blocked over the old (now-empty) vertical panel spot.** `point_is_over_gui_menu()`
  (`gui_frontmenu.c`) sets `busy_doing_gui` from every active menu's *legacy* `pos_x/pos_y/width/
  height` — never updated, so it's always GMnu_MAIN's flush-left vertical rect regardless of
  `GUI_POSITION`. For Left this happens to coincide with the real drawn position, so it went
  unnoticed; for Right and now Bottom the real position has moved, so hovering the stale rect
  wrongly set `busy_doing_gui = 1` and suppressed world interaction — the exact bug class the
  minimap hit-testing fix from `10-maintainability-refactors.md` already covers, just a different
  call site. Fixed by skipping migrated menus in `point_is_over_gui_menu()`
  (`ingame_imgui_menu_active()`, the same predicate `draw_active_menus_buttons()` and the spangle
  fix already use) — `ingame_imgui_wants_mouse()`, checked right after in `front_input.c`, already
  covers their hover correctly. `mouse_is_over_side_panel_bottom()` (`kjm_input.c`) had the same
  latent gap for Bottom specifically (no Bottom branch, so it fell through to the Left check) —
  now returns `false` for Bottom (no vertical side panel exists there to be "over").
- **The objective/event text box and the battle-participants box** (`textinfo_frame()`/
  `battlemenu_frame()`, `frontgui_ingame.cpp`/`frontgui_ingame_battle.cpp`) are both bottom-centre
  anchored by their own pivot — the same screen edge the new HUD strip now occupies, so they drew
  on top of it. First fix (nudge the pivot up above the strip) wasn't right either — live-tested as
  "isn't in the right place": a big box floating disconnected above the whole strip reads wrong
  regardless of its exact Y. Both now draw *inside* region C instead (the same rect the message
  queue uses), sized to it, rather than floating full-width. Can overlap the message queue if both
  happen to be showing at once — rare, not handled specially.

**A third bug, also from interactive testing:** the bottom of the minimap, with the `cpu`/`-`
buttons, was rendering off the bottom of the screen. `render_minimap()`/`draw_minimap_and_compass()`
size the minimap's diameter from width alone (`mm_upp` derives from `s_menu_rect.w`, not `.h`), so
region A's minimap sub-rect *must* be square — `build_horizontal_bottom()` (`frontgui_hud_layout.cpp`)
was instead giving it a width equal to the *whole* region A column and a height equal to whatever
was left over after the gold strip, which aren't the same number. The circle, sized to the (wider)
width, overflowed past the (shorter) height, pushing the nav buttons anchored to its bottom edge
past the bottom of the screen. Fixed by deriving the column's width *from* the leftover height
instead of the other way around, so the minimap sub-rect is genuinely square.

## Overall composition

Left to right along a strip at the bottom of the screen:

```
+------------+----------------------------------+----------------------------------------+
|  Region A  |             Region B              |               Region C                 |
|  minimap   |     tab headers (row, top)        |     message queue (scrolling text)     |
|  + M/+/-   |     6-column cell grid (below,     |     event markers: horizontal row      |
|  /cpu nav  |     vertical-scroll for more)      |     along the top edge                 |
+------------+----------------------------------+----------------------------------------+
```

Region A is square-ish (its side matches the strip's height, same as the minimap already
being sized off `s_menu_rect.h` today). B and C split the remaining width; C gets whatever's
left over after A and B claim their needed width, same asymmetric split
`build_vertical_right()` already does today (fixed-shape regions first, one flexible region
absorbing the remainder — there, `TabContent`; here, the message queue).

## Region A — minimap + nav buttons

Reused close to verbatim: `render_minimap()`/`draw_minimap_and_compass()`/`draw_nav_buttons()`
(`frontgui_ingame_panel.cpp`) are already only parameterised by an origin + a diagonal, not by
"top of a vertical column" — they don't need to know which layout they're in, just region A's
rect. The gold counter (`draw_gold()`) sits in the same relative spot it does today: a strip
above the minimap circle, within region A rather than spanning the panel's full width.

## Region B — tab headers + grid

**Tab headers**: a row of 5 icons across region B's top edge — `draw_tabs()`'s existing tab
button look (rounded-top plinth, recessed channel) rotates naturally into a horizontal row
here; it's already drawn as one, just currently squeezed into the panel's narrow width. No
new visual vocabulary needed, just a different rect to lay the 5 slots across.

**The grid**: today's room/spell/trap/creature grids are 4 columns × N visible rows (scroll
for more), sized off the panel's full 140-virtual-unit width. Region B is a different shape
(wider, shorter — it's sharing the screen height with the tab-header row, and sharing the
screen width with A and C), so the grid reshapes column count accordingly: landed at 6 columns
(sized directly off region B's actual width, live-tested — the original plan's 2 columns left
most of region B's width empty), same vertical-scroll-for-more behavior. `GridGeom`/
`grid_begin()`/`grid_cell_pos()`
(`frontgui_ingame_cells.cpp`) already parameterise columns count nowhere explicitly — `slot % 4`
is hardcoded in `grid_cell_pos()`. That becomes `slot % cols`, with `cols` (2 vs 4) and the
per-cell size/pitch chosen by the current `HudPanelLayout`, the same way `build_icon`'s callers
already pick a cell size from `grid_sz()`.

**Creature tab, special-cased** (per your note): the current vertical layout gives each
creature model its own *row* — a 22×22 portrait plus 3 separate `cell_button`s (idle/work/fight
counts) beside it — because the panel is wide enough for that. A 2-wide grid isn't, so it
collapses to one *cell* per model: the portrait cropped to fill the cell (rather than
letter-boxed), with the three job counts overlaid as small numerals in the corners (e.g.
top-left idle, top-right working, bottom-right fighting — exact placement bikesheddable),
each numeral independently clickable (L picks up next, mirroring `cell_button`'s L/R split) —
closer to `instance_cell`'s "icon + overlaid small numbers" vocabulary than to today's
`cell_button` row. The header row (global "pick any idle/working/fighting creature", currently
3 buttons above the rows) stays as-is above the grid — it's a distinct capability (any model,
not one), worth keeping separate from the per-cell numerals.

## Region C — message queue + horizontal event markers

The message queue (`draw_message_queue()`, `frontgui_ingame_text.cpp`) currently floats as its
own top-left ImGui window, offset by `status_panel_width` — independent of the sidebar's
*position* already (confirmed when Right landed: it needed no change). For Bottom, it stops
floating and becomes region C's content instead: same scrolling icon+text list, just laid out
within a fixed region rect sized by what's left after A and B.

Event markers (`draw_event_markers()`) currently stack vertically past whichever edge of the
panel is away from the screen ( flipped for Left/Right already). For Bottom they lay out
horizontally instead — a row of the same square tokens along region C's top edge, newest
closest to... open question, see below.

## Possession-view proposal

Today, `creature_query_panel()` (shared by top-down query and possession) fills the *entire*
tab-content area regardless of layout: portrait + anger/xp bars header, health bar, a 2-tab
(Abilities/Stats) strip, then either a 3-wide ability grid or a 2-column stat list — and the
minimap above it keeps rendering unconditionally (panel.cpp draws it every frame the sidebar's
visible; the active tab body is a separate concern). For Bottom, region A (minimap) is
unaffected by any of this — it stays exactly as in the other tabs. The proposal is for
possession to occupy region B only, same as any other tab: the portrait+bars header becomes a
compact strip above region B's grid (in place of the 5 tab-header icons, since there's nothing
to switch between during possession... except Abilities/Stats, which needs to live somewhere —
proposal: as two small toggle cells at the corners of that header strip, where the tab icons
would otherwise sit), and the ability grid reuses the *same* 2×8 vocabulary the other tabs'
grids use (each ability cell is already close to a `fe_hud_cell` composition today — see
`instance_cell` in `frontgui_ingame_creature.cpp`). The stat list keeps its own 2-column
scrollable child, sized to region B's width instead of the panel's. Region C (message queue)
is untouched — messages are still relevant while possessing a creature.

## What this implies for the code (not this pass — for scoping only)

- `frontgui_hud_layout.cpp`: `build_horizontal_bottom()` replaces `build_stub()`'s placeholder
  for `HudLayout_HorizontalBottom`, computing real region A/B/C rects (mirroring
  `build_vertical_right()`'s shape: fixed regions sized off `h`, one flexible region absorbing
  the rest of `w`).
- `frontgui_ingame_cells.cpp`: `grid_cell_pos()`'s hardcoded `slot % 4` / `slot / 4` becomes
  parameterised by column count; `GridGeom` gains a `cols` field (or a second geometry function)
  so the 2-wide/4-wide choice is explicit at each `grid_begin()` call site rather than implicit.
- `frontgui_ingame_panel.cpp` / `frontgui_ingame_tabcontent.cpp` / `_grids.cpp` / `_creature.cpp`:
  today everything derives its rect from one `s_menu_rect` (the whole panel) via
  `fe_hud_set_panel_rect()`. Multi-region layouts need each region's *own* rect fed to the
  code that draws it — e.g. `ingame_tabcontent_draw()` would call `fe_hud_set_panel_rect()` with
  region B's rect (not the whole menu rect) when `HudLayout_HorizontalBottom` is active, and
  panel.cpp's minimap/tabs/gold/event-marker code reads regions A/B/C respectively instead of
  `s_menu_rect` directly. This is the one genuinely structural change (everything else is new
  geometry math within already-existing primitives) — worth its own careful pass before any
  region-specific drawing work starts.
- A new creature-cell composition (cropped portrait + overlaid numerals) in
  `frontgui_ingame_creature.cpp`, likely as a new `fe_hud_cell`-based helper alongside
  `instance_cell`/`stat_cell` rather than a change to `fe_hud_cell` itself (badge positions
  needed — up to 3 numerals, not `fe_hud_cell`'s current single hotkey/count/have-dot slots).

## Open questions (my proposed default first, in each case)

1. **Event marker order along region C.** Proposal: newest closest to region B (so a new
   event reads left of window, near the grid you're likely already looking at), oldest
   trailing off toward the screen edge — mirroring the vertical layout's "newest at the bottom,
   closest to the tab strip" convention.
2. **Event marker overflow.** 13 fixed slots at today's ~40px token size won't fit a typical
   region C width. Proposal: shrink the token size until 13 fit at common resolutions, and
   accept that extremely narrow windows just crowd them (matching the existing vertical
   layout's own "however many fit" resignation — it doesn't scroll either).
3. **Region proportions.** Proposal, mirroring `build_vertical_right()`'s formula: strip height
   `= w * 0.22`, clamped `[180, 320]`, capped at `h * 0.35`; region A width `=` strip height
   (square); region B width `=` a fixed multiple of the 2-column cell pitch (enough for 2
   columns + margins, independent of screen width); region C takes the remainder.
4. **Possession's Abilities/Stats toggle**, replacing the tab-header row's usual spot — corner
   cells of the header strip, or its own small third row? Proposal above picks corner cells;
   open to a dedicated third row if that reads better once mocked up.
5. **Whether Bottom should even keep the minimap in region A during possession**, given a
   first-person view arguably cares less about the overhead map. Proposal: yes, keep it,
   matching current (Left/Right) behavior exactly — consistency over cleverness, and it's zero
   extra work either way since panel.cpp already draws it unconditionally.

Not answering these now — flagging them so implementation doesn't quietly bake in a default
you'd have picked differently.
