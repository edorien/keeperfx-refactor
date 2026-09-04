# Flexible-width, icon+text, and grid button primitives

Status: **complete**. Implemented in the "Add flexible-width, icon+text menu button primitives to
kfx_frontend" commit. Kept here (matching [00-overview.md](00-overview.md)/
[01-caption-table-rename.md](01-caption-table-rename.md)'s convention) as the record of why these
three specific primitives were added, ahead of any actual menu redesign using them — see
[02-menu-v2-mockup-gap-analysis.md](02-menu-v2-mockup-gap-analysis.md) for what else a *visual*
redesign (as opposed to a layout-capability gap) turned out to need.

## Context

Before any menu could be reshaped into a narrower, grouped, multi-column layout with icons, three
authoring gaps needed closing: buttons whose rendered width isn't locked to a few fixed presets,
a button style that draws an icon and a text caption together, and a convenient way to lay
buttons out in a grid, not just a single column/row.

## What was actually true before this work (verified by reading the draw code, not assumed)

- **Height already scaled freely.** Every frontend button's `units_per_px` comes from
  `simple_frontend_sprite_height_units_per_px(gbtn, spridx, 100)` (`gui_draw.c:532`) — a uniform
  scale factor derived purely from `gbtn->height` vs. the sprite's native height. Any `.height`
  value already worked.
- **Width was *not* independently adjustable for the ornate "menu button" style** —
  `frontend_draw_button` (`gui_frontbtns.c:730`, backing `frontend_draw_large_menu_button`/
  `_vlarge_menu_button`/`_small_menu_button`) drew a fixed sequence of pieces from the
  `GFS_hugebutton_a0*l` animation-frame sprite set: an opening piece (`spridx`), a *middle* piece
  (`spridx+1`) repeated a **hardcoded** 0/1/2 times selected by the `btntype` argument (0 = small,
  1 = large, 2 = vlarge), then a closing piece (`spridx+2`). `.width` in the `GuiButtonInit` row
  only clipped the text window afterward — it didn't affect how many chrome pieces got drawn.
  This was already a real (if crude) left-cap/middle-tile/right-cap 3-slice design; the middle-tile
  repeat count just needed to become computed-from-width instead of a 3-way preset.
- **No existing draw function combined an icon and a text caption.** `frontend_draw_icon`
  (`frontend.cpp:1092`) drew a single sprite, nothing else. The in-game panel family
  (`gui_area_new_normal_button`, `gui_area_no_anim_button`, `gui_area_normal_button` —
  `gui_frontbtns.c:508,664,696`) also drew one sprite each, no caption text — icon-only, with a
  hover tooltip rather than a persistent label.
- **Grid layout needed no new macro** — `FE_ROW_Y(base, step, n)` (`frontend.h`, from
  [00-overview.md](00-overview.md) Phase 3) was already axis-agnostic; it had already been used
  for both `.scr_pos_x`/`.pos_x` and `.scr_pos_y`/`.pos_y`. A 2-column grid is just `FE_ROW_Y`
  called once per axis on the same row — no engine gap, just a discoverability one.

## What was built

1. **`frontend_button_chrome_repeat_count(width, left_w, right_w, mid_w)`** (`gui_frontbtns.c`) —
   pure arithmetic generalizing `frontend_draw_button`'s hardcoded 0/1/2 repeat presets to any
   target width. Unit-tested directly (`gui_frontbtns_chrome_test.cpp`), no rendering dependency.
2. **`frontend_draw_button_chrome_flexible(gbtn, spridx, units_per_px)`** — draws the same
   `GFS_hugebutton_a0*l` chrome pieces `frontend_draw_button` uses, tiling the middle piece the
   computed number of times. Returns the x position after the chrome so a caller can lay out
   content relative to it.
3. **`frontend_draw_button_icon(gbtn)`** — a new `draw_call` combining the flexible chrome above
   with an icon (from `.sprite_idx`) and a left-aligned caption (from `.content.lval`, same
   caption-table mechanism every other frontend button uses). No `GuiButtonInit`/`GuiButton`
   struct changes needed — `.sprite_idx` and `.content.lval` were already independent fields,
   just never used together on one row before.
4. **`FE_ROW_Y`'s doc comment** (`frontend.h`) gained a worked 2-column grid example — no code
   change, since the macro already supported it.

## Deliberate scope limit

`frontend_draw_button` itself and its three existing callers
(`frontend_draw_large_menu_button`/`_vlarge_menu_button`/`_small_menu_button`) were left
**completely untouched**. This environment has no game data files to visually verify rendering
against, so there was no way to confirm the new width-driven tiling reproduces the *exact* pixel
width of today's three presets before risking every existing menu button on it. The new
capability ships as an additive, opt-in path (new functions, new draw_call) rather than a
modification to already-working, unverifiable-here rendering code. Before wiring an existing
menu's `draw_call` over to the flexible-width path, visually confirm in a real build that a test
button at a previously-unused width tiles with no seam gap/overlap.
