# Phase 8 — procedural relief / emboss pass on the ImGui sidebar

Status: **in progress (2026-09-07).** Cosmetic-only follow-up to
[05-sidebar-frame-and-minimap.md](05-sidebar-frame-and-minimap.md) and
[06-tab-content-panels.md](06-tab-content-panels.md). No gameplay, layout, input or state change —
this is entirely about how the already-working ImGui HUD *looks*. Rides the existing
`RendererImGuiEnabled()` path; `-classicmenu` is unaffected.

Goal: close the gap between the current **flat** ImGui chrome (one `AddRectFilled`, ImGui
rounded-rect borders on cells, flat text-fill nav buttons) and the DK1 sidebar's **sculpted
stone-and-bronze relief** — raised outer rim, deep circular minimap well, chamfered plate
sections divided by V-cut grooves, recessed icon wells, raised tab plinths — using only
`ImDrawList` primitives (no new textures, no `rpanel_*` chrome sprites — see
[00-overview.md](00-overview.md) §3 / [05](05-sidebar-frame-and-minimap.md) §3 "procedural, not
sprite chrome": `rpanel_full.png` is a monolithic fixed-shape 560×1600 image, not a 9-slice,
unusable for a reflowing / horizontal / minimal panel).

---

## 1. Where we are vs. the reference

**Reference** (`gfx/menufx/gui2-256/rpanel_256/rpanel_full.png`) — the DK1 vertical sidebar. Relief
vocabulary, top to bottom:

| Feature | Relief treatment in the art |
|---|---|
| Whole-panel outer edge | raised rim: light chamfer on the top + left, dark on the bottom + right; 45° cut corners at the top with an incised diamond motif |
| Minimap | deep circular **recess** — dark interior, a thick raised bezel ring that catches light on the upper-left arc and falls to shadow on the lower-right |
| Zoom / compass notches | small **engraved** ticks (V-cut) at the bezel's equator and cardinal points |
| Zone dividers (minimap / gold / tab strip / content) | horizontal **engraved grooves** — a dark line under a light line |
| Plate sections | each flat region is a slightly **raised plateau** bevelled at its edges, separated from its neighbours by a groove |
| Tab strip | 5 hexagonal / trapezoidal **raised plinths** sitting in a recessed channel, each outlined by an engraved bevel |
| Content area | a large **recessed** well with a bevelled inner lip (lit top-left) |
| Surface | heavy mottled / veined bronze-stone — warm mid-brown, darker veins, lighter highlights, reddish flecks — never a flat colour |

**Current ImGui HUD** (`frontgui_ingame_panel.cpp` / `frontgui_ingame_tabcontent.cpp`):

- `draw_background()` — one `dl->AddRectFilled(..., IM_COL32(28,20,12,205))` from the panel top to
  the tab grids. No rim, no grooves, no texture.
- `draw_minimap_and_compass()` — the minimap texture `AddImage`d as a bare diamond; no bezel, the
  out-of-diamond corners are just panel fill.
- `draw_tabs()` / `slot_icon_button()` — flat: an `AddRectFilled` wash when selected, a 1px
  `AddRect` ring on hover. No plinth.
- `draw_nav_buttons()` / `slot_text_button()` — a single flat `AddRectFilled` (rounding 3px), no
  bevel (the border was *removed* in the 2026-09-07 cosmetic pass because the old one was a hard
  1px outline that read as a sticker).
- `ingame_tabcontent_draw()` — opaque `AddRectFilled` panel bg; `build_icon()` cells are a rounded
  rect + a 1px border (gold when selected, red on hover); `info_band` / `bar_row` tracks are flat
  fills.
- `frontgui_widgets.cpp::fe_inset_bevel()` — **the one relief primitive that already exists**: a
  two-tone inset (warm light line top+left, black-ish line bottom+right). Used by `FeBeginPanel` /
  the modal windows, *not* by any in-game HUD code yet.

So the raw material is there (`fe_inset_bevel`, `fe_stipple_bg` for mottling) — it has just never
been applied to the HUD, and it only does the simplest 1px case.

---

## 2. The technique inventory (ImDrawList, procedural)

All confirmed present in the vendored ImGui 1.92.7 (`deps/imgui/imgui.h`):

1. **Two-tone bevel** (`AddLine` ×4, or `AddRect` twice at ±1px offset with a light and a dark
   colour). Light top+left / dark bottom+right ⇒ *raised*; swap ⇒ *recessed*. The workhorse.
   Generalise `fe_inset_bevel` to take a **width** (1–3 px) and a **direction** (raised / sunken).
2. **Vertical gradient fill** — `AddRectFilledMultiColor(p_min, p_max, upL, upR, botR, botL)`.
   Lighter at the top, darker at the bottom = "lit from above", turns a flat fill into a domed
   plateau or, inverted, a concave well.
3. **Layered offset shapes** — draw the same rect/circle 2–3× at 1px diagonal offsets with a
   dark→mid→light colour ramp for a chunkier chiselled edge than a single bevel line.
4. **Manual drop shadow** — an `AddRectFilled` / `AddCircleFilled` offset (+2,+3) in translucent
   black *behind* an element. (ImGui has no `AddShadowRect` outside internal branches — do it by
   hand; it is one extra primitive.)
5. **Arc highlight / shadow** — `PathArcTo` + `PathStroke` for a partial ring. An outer bezel =
   a light arc over the top-left ~200° and a dark arc over the bottom-right ~200°, slightly
   overlapping, 2–4 px stroke. This is how the minimap bezel gets its "caught light".
6. **Per-vertex-colour polys** — `PrimReserve` + `PrimVtx`, or `AddConvexPolyFilled` on a path,
   for chamfered (45°-cut) corners and the hexagonal tab plinths, each face shaded by its normal.
7. **Engraved groove** — two adjacent `AddLine`s: a dark line then a light line one px below
   (horizontal groove) — the V-cut divider between plate sections.
8. **Mottle / vein** — the existing `fe_stipple_bg` approach (seeded LCG, bounded fleck grid),
   toned *way* down (alpha ~6–12) as a single pass over the panel body so the fill stops reading
   as a flat colour. Cheap and already written.

Cost: the whole panel is a few dozen extra `AddLine` / `AddRect` / short `PathStroke` calls per
frame — negligible against the minimap raster capture that already runs every frame.

---

## 3. Proposed shape — a `frontgui_ingame_relief` helper set

New file `src/kfx_frontend/src/frontgui_ingame_relief.cpp` (+ `.h` in `include/`) — a small
palette of relief primitives, called from `frontgui_ingame_panel.cpp` and
`frontgui_ingame_tabcontent.cpp`. Kept separate from `frontgui_widgets.cpp` because that file is
"reusable *frontend* ImGui primitives"; this is HUD-skin-specific and churns during visual
review. (If review settles it and the frontend wants the same look, fold it back later.)

**Decided (user 2026-09-07): it stays a helper set, and there may be sibling sets for other
panel layouts** ([05](05-sidebar-frame-and-minimap.md) §0's `HorizontalBottom` / `Minimal`) — the
primitives here are geometry-driven (take a rect / centre), so they transfer, but a different
layout may want its own composition module that calls them differently, or a flatter set of its
own. This module is `VerticalRight`'s relief; it does not try to be universal.

Primitives (names indicative):

| Helper | Draws | Used for |
|---|---|---|
| `relief_plateau(dl, r, tint)` | gradient fill (light top) + raised two-tone bevel + faint mottle | every flat plate section of the panel body |
| `relief_well(dl, r, depth)` | inverted gradient (dark top) + sunken bevel + inner shadow line | tab-content bg, each icon cell, bar tracks |
| `relief_boss(dl, r, lit)` | raised bevel + top-highlight gradient + optional drop shadow; `lit` brightens for selected/hover | tab plinths, nav buttons, sell / query buttons |
| `relief_groove(dl, x0, x1, y)` | dark line + light line below it | zone dividers |
| `relief_ring(dl, c, r_out, r_in, lit_ang)` | outer light arc + outer dark arc + inner recessed lip | minimap bezel, compass ring |
| `relief_chamfer_corner(dl, corner, size)` | 45° cut with a shaded inner face + incised notch | panel top corners |
| `relief_edge_frame(dl, r, w)` | the whole-panel raised outer rim (calls chamfer for the top corners) | `draw_background` |

Every width / offset is an **absolute pixel constant** (1–4 px), *not* multiplied by the panel's
resolution scale — a 3px chisel at 4K must still be 3px or the panel turns into a cartoon. Clamp
where a helper is handed a tiny rect.

---

## 4. Application plan (per surface, each independently checkpointed)

Land these one at a time; screenshot-retest between each (live-desktop rule — ask first).

1. **Panel body** — `draw_background()`: replace the single `AddRectFilled` with
   `relief_edge_frame()` (raised outer rim + chamfered top corners) + `relief_plateau()` for the
   region between the minimap and the tab strip + `relief_groove()` at the gold / tab-strip
   divides. Keep the transparent holes over the minimap and tab content exactly as now.
2. **Minimap** — `draw_minimap_and_compass()`: draw `relief_well()` behind the diamond and
   `relief_ring()` as a bronze bezel just outside `s_mm_diag`, with the lit arc toward the panel's
   top-left. Compass letters move onto the bezel. The diamond image and its hit-test are
   unchanged (`mouse_is_over_panel_map()` still keys off `PanelMapX/Y` + `s_mm_diag`).
3. **Tab strip** — `slot_icon_button()` (or a new `tab_plinth_button()`): a recessed channel
   (`relief_well`) behind the row, each tab on a `relief_boss()` plinth; the active tab's plinth
   sits proud and lit, inactive ones flush. Spangle (2nd-pass ring + sparks) unchanged.
4. **Nav buttons** — `slot_text_button()`: swap the flat fill for `relief_boss()`; `cpu` toggle
   stays lit via the `lit` param. `M` / `+` / `−` get the same treatment.
5. **Tab-content cells** — `frontgui_ingame_tabcontent.cpp::build_icon()` / `unknown_cell()` /
   `sell_icon()`: each cell becomes a `relief_well()` with the medsym / glyph sitting *in* the
   recess; selection = a lit inner bevel instead of the current gold `AddRect`; hover = a warm rim
   light. `info_band` / `bar_row` / `prog_bar` tracks become `relief_well()`, the fill a
   `relief_boss()` pill.
6. **Event markers** — `draw_event_markers()`: the rounded body gets a slim `relief_boss()` edge
   so the chip reads as a raised token, not a flat sticker. (Geometry was just fixed — markers now
   stack at a fixed 41px pitch anchored to slot 0's rect, no floaty gap.)
7. **Query / possession panels** — `creature_query_panel()` portrait frame + stat cells: same
   `relief_well` / `relief_boss` vocabulary as the item grid, for consistency.

---

## 5. Risks / watch-items

- **Busy at low res.** ~~640×480 with 3px bevels everywhere reads as noise.~~ **Decided (user
  2026-09-07): no low-res fallback.** 640×480 is effectively dead — few monitors do it and Windows
  has dropped it — so the "relief detail cutoff" is cut. Absolute-px bevel widths everywhere; the
  only guard is a per-helper clamp so a tiny rect doesn't invert.
- **Contrast vs. the 3D view.** The panel sits over live gameplay; too much dark drop-shadow on
  the outer edge bleeds onto the scene. Keep outer shadow alpha low (≤ 90) and tight (≤ 3px).
- **Overdraw / batching.** `AddRectFilledMultiColor` and `PathStroke` don't break the ImGui draw
  batch (no texture change, no clip change), so this stays one draw call. Confirm with a frame
  capture anyway.
- **Palette drift.** `frontgui_style.cpp::apply_colours()` already defines the bronze / parchment
  / brown ramp as placeholder values (§5.1 "procedural treatment is an accepted stand-in"). The
  relief helpers should pull their light / mid / dark tones from **one** shared struct so a later
  "sample from `front.pal`" pass is a single edit, not a scavenger hunt.
- **Screenshot capture.** Same note as [05](05-sidebar-frame-and-minimap.md) §4 — the HUD is
  composited at present time; stage 3 B1 "capture includes the overlay" still applies, nothing
  new here.
- **Not a substitute for the art route.** If someone later wants the *actual* `rpanel_full.png`
  look, that needs the 9-slice / region-atlas question reopened ([05](05-sidebar-frame-and-minimap.md)
  §3). This pass is the "procedural chrome, done properly" branch, not a detour toward sprites.

## 6. Checklist

- [x] `frontgui_ingame_relief.{h,cpp}` (2026-09-07): `relief::` namespace — `bevel` (width +
      raised/sunken), `plateau`, `well`, `well_circle`, `boss`, `groove_h`, `ring`, `edge_frame`,
      `mottle`; `Tones` shared struct via `tones()`; absolute-px widths, per-helper clamp.
- [x] ~~Generalise `fe_inset_bevel`~~ — superseded by `relief::bevel` for HUD use; `fe_inset_bevel`
      stays for the frontend modals.
- [x] **Panel body (2026-09-07, `draw_background`):** head region = `plateau` + `mottle`, two
      `groove_h` at the zone divides, `edge_frame` rim around the whole column. Minimap /
      tab-content holes preserved (both composite/paint on top). *Chamfered top corners deferred —
      §7 open question; `edge_frame` is rim-only for now.*
- [x] **Minimap (2026-09-07, `draw_minimap_and_compass`):** `well_circle` behind the diamond +
      `ring` bezel lapping its points; compass letters moved onto the bezel (`r = d*0.5`).
      `PanelMapX/Y` + `s_mm_diag` hit-test untouched.
- [x] **Tab strip (2026-09-07):** `draw_tabs` pre-pass draws a `well` channel spanning the row;
      `slot_icon_button` → `relief::boss` plinth (`lit` 0.9 active / 0.4 hover), icon inset to
      0.82 so the bevel frames it. Spangle 2nd-pass unchanged.
- [x] **Nav buttons (2026-09-07):** `slot_text_button` → `relief::boss` (`lit` for the `cpu`
      toggle-on / hover); caption unchanged.
- [x] **Tab-content cells / bars (2026-09-07):** `build_icon` / `sell_icon` / `unknown_cell` →
      `relief::well` (selection/hover = a coloured `AddRect` on top, no base border); `info_band`
      panel + capacity bar, `prog_bar`, `cell_button` → `well` / `boss`; the whole tab-content
      region is now one `well` below the raised strip. `COL_PANEL_BG` retired.
- [x] **Event markers (2026-09-07):** 1.5px raised `relief::bevel` under the state border.
- [x] **Query / possession (2026-09-07):** `instance_cell` / `stat_cell` / `spell_lost` cell /
      creature-list portraits → `relief::well`; portrait in a `well` frame; anger/xp `vbar` +
      cooldown bars → `well` tracks. `sprite_toggle` / `glyph_toggle` still flat — pending.
- [x] **Alignment (2026-09-07):** tab-strip channel + tab-content bg both span `x0+2 … x0+w-2`.
- [x] ~~Low-res fallback threshold~~ — cut (§5, 640×480 dead).
- [ ] Frame capture: still one draw call; frame-time vs. current build.
- [x] Layering + ftest/linux/windows builds green after the panel-body + minimap chunk (2026-09-07).
- [ ] Screenshot retest each surface (ask before touching the live DISPLAY).

**Retest 3 (2026-09-07):** surfaces 3–6 in. User: (1) minimap / tab strip / content grid right
edges don't line up; (2) colour jump between the warm head and the near-black tab strip + content;
(3) possession cells should be recessed like the room cells; (4)/(5) possession portrait +
anger/xp/health + query timers need bevels.
Fixes: (1) tab-strip channel and tab-content bg both span `x0+2 … x0+w-2` now (channel dropped its
`+3` overhang; content dropped the `grid_pt(138)` short edge). (2) content bg changed from a `well`
to `plateau` + `mottle` — same raised face as the head, so the panel reads as one material with
recessed pockets; `well_top/bot` retuned to a dark *brown* pocket, not near-black. (3)
`instance_cell` / `stat_cell` / `spell_lost_panel` cell / creature-list portraits → `relief::well`.
(4)/(5) portrait in a `well` frame; `vbar` (anger/xp) and the `instance_cell` cooldown bar →
`well` tracks with an inset fill. `sprite_toggle` / `glyph_toggle` (query imprison/flee/research
toggles) still flat — pending.

**Retest 4 (2026-09-07):** (a) possession portrait well too wide for the image → now hugs the
fitted image (left-aligned), anger/xp bars widened to fill the space to its right, level number
over the xp bar. (b) ABILITIES/STATS buttons → `cell_button` rebuilt as `relief::well` + gold rim
on the selected one (matches room cells; `bg` param → `bool selected`). (c) tab bar: only the
**active** tab is a raised lit `boss`, the rest are recessed `well`s. (d) the triple-line seam
between the tab strip and the content grid is gone — new `relief::face` (gradient + mottle, no
bevel) for both the panel head and the content bg, so they're one continuous surface framed only
by the outer `edge_frame`; the groove there is removed, the tab-strip channel's own bevel is the
divider. Toggles (`sprite_toggle`/`glyph_toggle`) deferred to a later pass per user.

**Retest 5 (2026-09-07):** (a) inactive tabs still showed a left/right bevel between neighbours →
inactive tabs now draw *nothing* (the shared channel `well` is their recess); only the active tab
draws anything. (b) active tab's bottom edge cut it off from the content → `bevel` / `boss` gained
`omit_bottom`; the active tab is a `boss` with no bottom bevel + no drop shadow, extended 6px past
the channel so it merges into the content face (folder-tab look). (c) possession header
re-proportioned to the spec ratio **10:60:10:10:5:10:5** (spacer:portrait:spacer:anger:spacer:
xp:spacer) across vx 3–137; the portrait `well` hugs the fitted image left-aligned in its 60-unit
slot, the anger/xp bars take their fixed columns.

**Retest 6 (2026-09-07):**
- **Tab bar rebuilt.** `slot_icon_button` → `tab_button`: rounded keys on the channel; the active
  tab takes the panel-face colour (`relief::tab_fill(true)`) and drops 8px into the content (no
  bottom edge); inactive tabs are flat, coloured halfway between face and recess
  (`tab_fill(false)`), **no side bevels** → no lines between neighbours; hover brightens toward the
  face colour. Icons are the query-panel vocabulary, not the framed legacy tab sprites: a
  pale-green `?` glyph (info), keeper-coloured `plyrsym_symbol_room` / `plyrsym_symbol_player`
  (room / creature), `room_research` / `room_workshop` (spell / trap). New helpers
  `relief::tab_fill` + `relief::mix`.
- **Room/spell/trap info strip always present** — `info_band(0,nullptr,-1,-1,-1)` draws the empty
  recess when nothing is hovered/chosen, so the strip no longer appears and disappears.
- **Possession:** portrait box +~25% height (to vy 243), image + `well` hug it, anger/xp bars
  taller to match; health bar / ABILITIES-STATS strip / ability grid / stats child all shift down
  ~12 vy; ability cells shorter (44→37 vy, pitch 46→40) at the same width to cut the whitespace.

**Retest 7 (2026-09-07):** tab rounding 4 → 8; the whole `Tones` ramp rebuilt around the
reference base colour **rgb(60,44,12)** (sampled by the user — a warm brown, almost no blue) —
plateau / well / mottle / groove tones all re-derived from it.

**Retest 8 (2026-09-07):** tabs round on the **top corners only** now (inactive tabs were
rounding all four), rounding 8 → 11. `mottle` replaced: the random fleck grid → **procedural
marble** — a domain-warped sine, `sin(freq·(p + amp·fbm(p)))`, sampled on a 7px grid in
rect-relative coords (cheap deterministic value-noise `fbm`, so still stable between runs, re-lays
on rect change). Thin bright/dark veins (`t⁴` falloff) instead of scattered dots.

**Retest 9 (2026-09-07):** tab rounding 11 → 15; marble stronger and denser — `freq` 0.019 →
0.030 (more veins), `amp` 36 → 44, falloff `t⁴` → `t³` (wider veins), alpha ×0.12/0.15 →
×0.18/0.22, 6px grid.

**Retest 10 (2026-09-07):** added `relief::groove` (arbitrary-angle engraved groove); nav buttons
(M/+/−/cpu) nudged ~6px in toward the minimap; compass letters pulled in — `r = d*0.5 − 21`.

**Retest 11 (2026-09-07):** minimap groove reworked into a full **octagonal frame** — 8 engraved
segments (top/bottom horizontals, left/right verticals, 4 corner diagonals) circumscribing the
bezel at `R = d*0.5 + 3`, half-side `R·tan22.5°`, each segment facing the circle centre. The
earlier 4 shaded corner-triangle cuts are gone.

**Retest 12 (2026-09-07):** the main-menu list-box background (`fe_stipple_bg`,
`frontgui_widgets.cpp`) switched from the seeded fleck grid to the same procedural marble as the
in-game panel (own copy of the value-noise fbm in that file's anon namespace), keeping its own
colours (`14,10,7` base / `156,126,88` light vein / black dark vein).

**Retest 13 (2026-09-07) — two behaviour fixes (not cosmetic):**
1. **Tab / Ctrl+Tab (`toggle_gui`) now hides the ImGui HUD.** `set_menu_visible_off` only clears
   `is_turned_on`, not `visual_state`, so `read_menu_rect()` didn't catch the toggle. Added an
   explicit `(kfx_sim_state.operation_flags & GOF_ShowGui) == 0` early-return in
   `ingame_panel_frame()`.
2. **Engine window no longer inset by the sidebar under the ImGui HUD** (needed for a horizontal
   layout — a left inset can't express one). `render_overlay_get_status_panel_width()` (main.cpp)
   returns 0 when `RendererImGuiEnabled()`; `set_gui_visible()` (frontend.cpp) skips the
   `setup_engine_window(status_panel_width, …)` inset the same way. The 3D view fills the screen
   and the opaque sidebar composites over its left edge. Classic sprite GUI keeps its inset.
   **Watch:** possession / first-person view — the FP view now fills the screen with the sidebar
   over its left edge (was inset); crosshair centring uses panel-width 0 so it re-centres on the
   full screen. Flagged by the user as a possible-complication area — verify on retest.

**Retest 14 (2026-09-07):** the four nav controls (M/+/−/cpu) re-seated in **recessed triangular
pockets** at the corners of the minimap frame — new `relief::well_tri` (filled well tone + sunken
edge bevel) + `corner_nav` (right-angle vertex at the panel corner, legs toward the interior,
hypotenuse parallel to the octagon facet), a 6px marble gap between each pocket and the panel
edge. `slot_text_button` removed.

**Retest 15 (2026-09-07):** corner nav pockets — larger (leg solved per corner so the hypotenuse
stays 12px clear of the minimap octagon), marble edge gap 6→12px, caption font Body→Subheading;
the two bottom pockets' base is now aligned with the octagon's bottom segment (`yb = ccy + octR`).

**Retest 16 (2026-09-07):** the tab-strip channel's sunken bevel peeked past the end tabs as a
stray vertical line (circled on the creature tab) — channel changed from `relief::well` to a
flat recess-tone `AddRectFilled` (no bevel; the tabs define the edges).

**Landed so far:** the relief helper set (`face`, `omit_bottom`, `tab_fill`, `mix`, marble
`mottle`, `groove`, `well_tri`) + surfaces 1–7 + tab rework + minimap facets + corner nav pockets
+ frontend list marble + GUI-toggle / engine-window-inset fixes. Builds/layering/ftests green.

**Retest 1 (2026-09-07):** first pass was near-imperceptible — every tone too faint. Contrast
cranked across the board: `hi` → bright bronze `(202,172,120)`, `plateau_top`/`plateau_bot` spread
for a visible gradient, `bevel` innermost line floors at ~0.45 instead of 0, `groove_h` → 2px dark
line, `ring` bezel widened with full-strength lit/shadow arcs + a hard inner-lip shadow,
`mottle` alpha ~3× and cell 9px. Compass letters pulled to `d*0.5-13`.

**Retest 2 (2026-09-07):** relief now clearly reads. User: minimap bezel a touch thick, and the
lit/shadow arc → base-tone transition too harsh. `ring` rebuilt as a **per-vertex-colour filled
annulus** (72-segment `PrimReserve`/`PrimWriteVtx`, needs `imgui_internal.h` for
`TexUvWhitePixel`) with a smooth `cos(angle - lit_dir)` angular gradient — brightest up-left,
darkest down-right, no seam. Hard inner/outer lip rings softened to ~0.5 alpha. Bezel span
`d*0.5-7 .. d*0.5+3` (~10px, was ~14).

## 7. Open questions

- ~~**One relief helper set, or fold into `frontgui_style` / `frontgui_widgets`?**~~ Decided
  (user 2026-09-07): its own helper set (`frontgui_ingame_relief`), sibling sets possible for other
  layouts; fold back only if the frontend adopts the same look.
- **Chamfered top corners — worth the per-vertex-colour polys, or fake with two triangles + a
  bevel line?** Probably the cheap version first; revisit if it looks flat.
- **Mottle: one static pass — decided (user 2026-09-07).** A **fixed seed constant** (not derived
  from the panel rect), so the fleck pattern is identical between runs. It re-lays when the panel
  rect changes (resolution / layout switch) because the fleck loop walks the rect — that drift is
  acceptable. One pass over the panel body, alpha ~6–12.
- **Does the horizontal (DK2-style) layout ([05](05-sidebar-frame-and-minimap.md) §0) want the
  same relief, or its own flatter treatment?** Out of scope here — the helpers are geometry-driven
  (take a rect), so they transfer, but the *composition* is that layout's call.
