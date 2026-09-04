# Menu v2 mockups: gap analysis and phased implementation plan

Status: **analysis, no implementation started**. Source: `docs/refactor/gui/mockup/Front Menu
Mockups.dc.html` (5 screens: Main menu, Land selection, Free play levels, High score table, Load
game). Builds on [00-overview.md](00-overview.md)'s engines and
[03-button-primitives.md](03-button-primitives.md)'s flexible-width/icon button work — but the
mockups turn out to ask for far more than button layout. This doc is the honest accounting of
that gap before any of it gets built.

## What the mockups actually show

Reading the mockup source directly (not just the two screenshots), all 5 screens share one visual
language that is a **complete departure** from KeeperFX's current chrome — not a re-skin of the
existing carved-stone sprite buttons, a different UI system entirely: flat panels with rounded
corners and drop shadows, translucent gradient backdrop overlays, breadcrumb navigation
(`Main menu / Start new game`, italic active segment), proportional typography in four distinct
families (display/serif/sans/mono, italics used throughout), hover-highlighted list rows, and —
structurally, not just visually — **two of the five screens merge what are today two separate
menu screens into one**:

- **Land selection** (screen 02) merges campaign select + the land-view preview into one
  screen: a campaign list on the left, a live "current land" detail panel (name, description,
  "Enter this land" button) on the right that updates as you browse the list, plus a preview
  image above both. Today these are `FeSt_CAMPAIGN_SELECT` and the land-view cutscene state —
  two separate, sequential screens.
- **Free play levels** (screen 03) merges map-pack select + level select into one screen: two
  stacked list cards on the left (map packs, then levels within the selected pack) and a live
  level-detail-plus-preview panel on the right. Today these are `FeSt_MAPPACK_SELECT` and
  `FeSt_LEVEL_SELECT` — two separate screens, and internally two entirely separate
  `FrontendSelectList` instances with no shared-screen relationship.
- **Main menu** (01), **high score table** (04), and **load game** (05) keep today's 1:1 mapping
  to existing screens, just restyled.

## Gap analysis, by capability

### 1. Rendering: flat colour, alpha, gradients, rounded corners, shadows

**What exists**: the whole render pipeline is still genuinely paletted — `TbPixel` is a plain
`unsigned char` (`bflib_video.h:54`), and `RendererManager` (the SDL3-backed abstraction
`gui_frontbtns.c` and friends already route through) explicitly deals in "the currently-active
6-bit VGA palette" and palette-index draw calls (`RendererDrawBox(x, y, w, h, unsigned char
colour)`, `RendererGetActivePalette()`). `bflib_vidraw.h`'s primitives are `LbDrawBox` (flat
rectangle, one palette index, no rounding), `LbDrawCircle`, `LbDrawPixel`, and the `LbSpriteDraw*`
family (including scaled/tiled variants — `LbSpriteDrawScaled`, `LbTiledSpriteDraw`). There is no
`gradient`/`blend`/`alpha` primitive anywhere in that header or its implementation. Translucency
that does exist (`Lb_SPRITE_TRANSPAR4`-style draw flags, `lbSpriteReMapPtr` remap tables) is
palette-remap tricks authored per-sprite, not general-purpose alpha compositing.

**What the mockup wants**: `linear-gradient(...)` backdrop darkening, translucent card
backgrounds (`rgba(14,11,10,0.35)`), `box-shadow`, `border-radius`. None of this maps onto a
palette-index draw call.

**Gap**: real. Two ways to close it, very different cost:
- **Cheap, in-keeping with the existing engine**: bake the gradient/vignette directly into the
  backdrop art itself (the same `.raw` full-screen image format `front_landview.c` already loads
  for campaign land-view backgrounds — `Land Map background "%s.raw"`, confirmed at
  `front_landview.c:942`) — an art-asset decision, not a rendering-engine one. Rounded-corner
  panels become a new sprite chrome set (corner + edge + fill pieces), the exact same
  3-slice/9-slice composition pattern `frontend_draw_button_chrome_flexible`
  (`gui_frontbtns.c`) and the existing `frontend_draw_scroll_box_tab`/`frontnet_draw_alliance_box_tab`
  family already use for the game's current ornate borders — reuse the pattern, commission new
  corner/edge art in the new visual style. No shadow primitive exists, but a shadow baked into
  the panel's own sprite art (a soft dark edge drawn into the asset) reads the same at these
  panel sizes.
- **Expensive, general-purpose**: add real alpha-blended fill/gradient primitives to
  `RendererManager`. Feasible in principle (SDL3 underneath does this natively), but it's new
  surface across the renderer abstraction (`RendererDrawBox` and friends would need an alpha
  variant, plumbed through `IRenderer` and whatever concrete backend implements it), and every
  paletted-drawing assumption elsewhere in the engine stays exactly as-is — this buys generality
  the mockups don't strictly need if the "bake it into art" route is acceptable.

**Recommendation**: bake gradients/shadows into new art assets, add rounded-panel sprite chrome
following the existing 3-slice pattern. Don't build general alpha-blending unless a *future*
screen genuinely needs a gradient that can't be pre-baked (e.g. one that has to respond to live
game state).

### 2. Typography

**What exists**: `frontend_font[]` (`vidmode.h:235`) is an array of `struct TbSpriteSheet*` —
fixed-size bitmap sprite fonts, `FRONTEND_FONTS_COUNT` of them. No FreeType/SDL_ttf anywhere in
the codebase (confirmed by search — zero hits). No italics, no serif/sans/mono distinction beyond
however many bitmap sheets exist as game assets, no arbitrary point sizes (bitmap fonts scale by
pixel-doubling/`units_per_px`, not true font hinting).

**What the mockup wants**: four font families (`--font-display`, `--font-serif`, `--font-sans`,
`--font-mono`), italics, a range of sizes from `--text-2xs` to `--text-2xl`.

**Gap**: the largest one here, and the one most likely to be underestimated. Two routes:
- **In-keeping**: commission a handful of new bitmap font sheets in the new style (a "display"
  headline face, a body face, maybe an italic variant) at the specific sizes the new screens use,
  the same way the game's existing fonts were produced. Bounded, but each new size/weight/style
  is a new hand-authored asset — no italics-on-demand, no arbitrary point sizes.
- **General-purpose**: integrate SDL_ttf (or similar) as a second, parallel text renderer for
  frontend-only screens, rendering real vector/TTF fonts at arbitrary sizes with italics for
  free. This is a genuinely large addition — new third-party dependency (wire into
  `Dependencies.cmake` the same way SDL3/openal/etc. already are), a new font-loading/caching
  layer, and a decision about whether it's frontend-only or eventually replaces the bitmap
  system everywhere (in-game HUD text stays bitmap either way — that's a much bigger, likely
  unwanted, undertaking for gameplay-critical UI).

**Recommendation**: this is worth a dedicated decision before any of the other work starts, since
it changes the shape of everything downstream (how much can be reused across the 5 screens vs.
custom per-screen art). Don't default into "just add more bitmap fonts" without deciding this
explicitly — see Open Questions below.

### 3. Master-detail merged screens (Land selection, Free play levels)

**What exists**: every current frontend menu is one-shot navigation — `frontend_set_state()`
replaces the whole screen. `FrontendSelectList` (the Phase 1 engine from
[00-overview.md](00-overview.md)) already models "a scrollable list of rows, one of which is
selected," but nothing today keeps a **live preview** of the highlighted-but-not-yet-committed
row updating on a sibling panel within the same screen — clicking a row currently *is* the
commit-and-navigate action.

**What the mockup wants**: hovering/selecting a campaign or level updates a detail pane in place,
without leaving the screen; committing (a distinct "Enter this land"/"Play this level" button) is
a second, separate action.

**Gap**: real, but it's **game-logic/menu-composition work, not a rendering gap** — squarely
buildable on what already exists. Concretely: `FrontendSelectList`'s row click_event currently
calls straight into e.g. `frontend_campaign_select()`, which commits immediately
(`frontend_start_new_campaign()` + `frontend_set_state(FeSt_CAMPAIGN_INTRO)`). A merged screen
needs to split that into two: a lighter "highlight" action (updates which item the detail panel
describes, no navigation) fired on click/focus, and a separate "commit" action (the new
"Enter this land" button) that does what `frontend_campaign_select()` does today. The list engine
itself doesn't need to change; a new GuiMenu combining the existing list buttons with new
"detail panel" buttons (text fields sourced from the highlighted item, a preview image slot, a
commit button) is new menu-authoring work following exactly the patterns already established.

**Recommendation**: fully achievable now, no engine gap. This is the part of the mockups closest
to "just build it" once the visual-chrome questions above are settled.

### 4. Data table (High score) and simple list (Load game)

**What exists**: `frontend_draw_high_score_table` (`front_highscore.c`) already renders a
rank/score/turns/name table for the existing high-score screen — same data, same column shape as
the mockup's table. `frontmenu_saves.c`'s load-menu already lists saved games with name/turn/date
metadata (`gui_load_game_maintain` family). Structurally these two screens are the closest to a
pure re-skin — mostly steps 1-2's chrome/typography, minimal new logic.

**Gap**: cosmetic only (rounded panel + new typography), not structural.

## Recommended phasing

1. **Decide the typography question first** (see Open Questions) — it changes the cost of every
   other phase.
2. **New chrome sprite set**: rounded panel (corner+edge+fill, matching the existing 3-slice
   composition pattern), breadcrumb text run, hover-highlighted list row — built once, shared
   across all 5 screens' cards/panels/lists. This is the direct extension of
   [03-button-primitives.md](03-button-primitives.md)'s flexible-width work, same technique.
3. **High score + Load game re-skins** — lowest-risk proof of the new chrome, no new game logic.
4. **Land selection + Free play merged screens** — the master-detail split (highlight vs. commit)
   described in gap 3, once the chrome exists to build them in.
5. **Main menu** last, since it's the one screen whose current layout (full-width stacked
   buttons) is furthest from the mockup (narrow list + separate icon-row + big logo) — most
   layout work, plus needs the new logo/wordmark art asset.

## Open questions for the user

- **Typography route**: new bitmap font sheets in the existing style (bounded scope, no italics
  or arbitrary sizing) vs. integrating a real font renderer (SDL_ttf) for frontend screens only
  (bigger one-time cost, much more visual fidelity to the mockup, reusable for any future
  frontend redesign). This blocks a real cost estimate for everything else.
- **Art production**: backdrop art (per-screen, ideally with the darkening gradient pre-baked),
  land/level preview thumbnails, and the new logo/wordmark are all genuinely new assets someone
  has to produce — none of this is code. Is art production already planned/underway, or should
  the phased plan above assume placeholder art (flat colour panels, no backdrop image) for an
  interim "structurally correct, visually rough" version of each screen?
- **Scope check**: given the typography and rendering gaps above, is a *lower-fidelity* version
  of these 5 screens (existing bitmap fonts, flat-colour rounded panels, no backdrop gradient)
  an acceptable v1 target, with the full mockup fidelity as a later pass once art/font decisions
  are made? That would let phases 2-5 above start now instead of blocking on the typography
  decision.
