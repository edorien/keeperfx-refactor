# Land selection: palette, text, and listbox notes

Status: **known limitations, accepted for now** — recorded while building the merged Land
selection screen ([04-phase2-landview-panel-investigation.md](../04-phase2-landview-panel-investigation.md)),
kept separate because the root cause (the engine's single 8-bit palette) is a rendering-architecture
question, not something fixable from this screen alone. The user has confirmed a true 32-bit
(true-color) renderer is already on the roadmap — see "The real fix" below for how that removes
the constraint entirely.

## The core constraint: one active palette per frame

`TbPixel` is `unsigned char` — the engine is genuinely 8-bit paletted throughout
(`bflib_video.h:54`). `frontend_palette` (`vidfade.h`) is the *single* global "active palette"
buffer, pushed to the renderer once per state-transition fade (`fade_in()`, driven by
`fade_palette_in`, `frontend.cpp`'s `frontend_set_state`). Every sprite/text/image drawn in a
frame is interpreted through that one palette — there is no per-region or per-layer palette in
this pipeline.

This was invisible before Phase 2 because every existing frontend screen only ever draws assets
authored against the *same* palette (`front.pal`) as the shared backdrop and button chrome. The
merged Land selection screen was the first to put two different palette-shaped assets in one
frame: the campaign list/buttons (authored for `front.pal`) and the per-campaign land art
(authored against its own bespoke `.pal` file, loaded by `load_map_and_window`).

## What happens if you don't work around it

`load_map_and_window` backs up the current palette into `frontend_backup_palette` and overwrites
`frontend_palette` with the land's own `.pal`. Left as-is, the *next* `fade_in()` uploads that land
palette as the frame's only palette — every other sprite on screen (buttons, list text, chrome)
is now interpreted through a palette it was never designed for. Confirmed empirically: this
doesn't read as "wrong tint," it reads as full video-static noise, because a dungeon-themed
UI palette (few, saturated brown/red entries) and a land art palette (broad photographic gradient)
don't share enough structure for nearest-neighbor-style misinterpretation to stay legible.

## Current mitigation: palette remap at load time

`land_preview_load` (`frontmenu_landpreview.c`) builds a 256-entry nearest-color remap table
(brute-force RGB distance, 256×256 comparisons, once per campaign load — cheap) mapping the land
palette's colors to the closest colors in the *shared* palette, then rewrites `map_screen`'s pixel
bytes in place before restoring `frontend_palette`. This keeps the whole frame under one
consistent, correct palette — no static, no corruption of surrounding UI.

**The real cost**: nearest-color remapping is lossy, and the shared `front.pal` genuinely doesn't
contain the land art's actual hues (confirmed against a reference screenshot the user
supplied — vivid teal sky, green grass, blue water in the source art vs. a uniformly sepia/brown
result once remapped). This is not a bug to be tuned away; it's the ceiling of what one shared
8-bit palette can represent for two differently-authored asset families at once. Accepted by the
user for now (2026-09-02) rather than building a true-color bypass just for this screen.

## The real fix: true-color rendering (already on the roadmap)

The user confirmed a genuine 32-bit (true-color) renderer is already planned. Once that lands,
this whole class of problem disappears: land art (and any other asset with its own palette) can
render at full fidelity via an RGB/RGBA blit path independent of whatever 8-bit palette the rest
of the paletted UI still uses, with no remap step and no shared-palette ceiling. At that point:

- `land_preview_remap_screen_to_shared_palette` and the backup/restore dance around
  `frontend_palette` in `land_preview_load` become unnecessary — the land art can just be blitted
  through the true-color path with its own palette converted once to RGB.
- The quality ceiling described above goes away entirely; this doc's core finding stops applying.
- Worth revisiting whether other palette-constrained decisions made during Phase 2 (see below)
  are still the right tradeoff once mixed-fidelity rendering is cheap.

This doc doesn't attempt to scope that renderer work — it's a separate, larger initiative — just
records that Phase 2's color fidelity ceiling is a symptom of the same underlying constraint that
work will remove.

## Related: the listbox is a second, independent instance of "designed for one shape"

Not a palette issue, but discovered in the same investigation and worth recording alongside it:
`frontend_draw_scroll_box`/`frontend_draw_scroll_box_tab` (`gui_frontbtns.c`) derive their entire
render scale — border pieces *and* per-row height together — from `gbtn->width` alone, because
each row is a fixed 6-sprite sequence (corner, four *distinct* decorative segments, corner —
confirmed via the sprite enum, `GFS_hugearea_thn_tx1..4_*` are four different sprites, not one
repeatable tile) uniformly scaled to fit a target width. There's no way to vary width and height
independently with that control as authored, and no narrower-than-~450px working example anywhere
in the codebase to copy. Land selection's list column ended up using a plain flat background
(`frontend_draw_land_selection_panel_bg`, `frontmenu_select.c`) at a fixed, readable row height
instead, showing more rows at once rather than shrinking text to fit a narrower column. See
[04-phase2-landview-panel-investigation.md](../04-phase2-landview-panel-investigation.md) for the
full trace of how this was found (including a discarded intermediate attempt to keep
`frontend_draw_scroll_box` and dynamically sync row positions to its derived scale — technically
correct but produced unreadably small text, not what "independent width/height" turned out to
mean in practice).

`frontend_scroll_box_units_per_px`/`frontend_scroll_box_row_height` (extracted from
`gui_draw_scroll_box` during this investigation, `gui_frontbtns.c`/`.h`) remain available if a
future screen needs to stay in sync with that control's actual rendered scale — but for a screen
that wants a *readable, independently-sized* list, reaching for a flat box (or, if built later, a
proper 9-slice panel with independent width/height tiling) is the better starting point, not this
control.

## Still open / cosmetic

- The list and detail panel backgrounds are flat black (`LbDrawBox`, no border) — a deliberate,
  low-risk choice after a bordered version (`draw_round_slab64k`) turned out to depend on in-game
  panel sprites not loaded on the frontend, and a translucent version
  (`Lb_SPRITE_TRANSPAR4`) turned out to depend on a blend table with the same problem. The user
  has flagged the flat black look as not ideal but acceptable for now; no safe alternative fill
  color has been identified without further sprite-sheet investigation.
