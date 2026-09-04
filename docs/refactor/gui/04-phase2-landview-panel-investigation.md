# Phase 2 investigation: embedding the interactive landview in a panel

Status: **complete — the merged Land selection screen is wired up and builds/tests clean.**
Requested after the user flagged that Phase 2 (Land selection merge) "requires interaction with
the scrollable landview" and needs "more investigation" before scoping — this doc started as that
investigation (reading `front_landview.c`/`.h` in full rather than the earlier, too-shallow
assumption that only `frontmap_draw`, `check_mouse_scroll`, and `update_velocity` were
screen-size-coupled), then became the record of what got built once the user confirmed the
interaction spec. See "What was built" below for the panel module, and "The merged screen" for
how it's wired into `frontend_select_campaign_menu`. Not yet possible to visually verify in this
environment (no game data files) — worth a smoke test in a real build before considering this
fully done.

## The coupling is deeper than initially scoped

`grep -c "PhysicalScreenWidth\|PhysicalScreenHeight" front_landview.c` finds **31 occurrences
across 11 functions**, not 3: `is_ensign_in_screen_rect`, `frontmap_load`, `check_mouse_scroll`,
`compressed_window_draw`, `frontmap_draw`, `frontmap_zoom_out_init`, `frontzoom_to_point`,
`set_map_info_screen_shift[_raw]`, `set_map_info_visible_hotspot[_raw]`, `update_velocity`. Nearly
every piece of viewport math in this ~1400-line file assumes "viewport == physical screen,
origin (0,0)." A literal "add a rect parameter" pass would mean threading a viewport rect through
all eleven, converting every `GetMouseX()/GetMouseY()` read (`check_mouse_scroll`,
`frontmap_input`) to panel-relative coordinates, and adding a bounds check that doesn't exist
today (the file has never needed to ask "is the mouse even inside my viewport" because it always
filled the whole screen).

## A deeper blocker: the background blit itself doesn't support a sub-rect

`draw_map_screen()` (`front_landview.c:104`) — the function that actually draws the pannable land
art — calls:

```c
copy_raw8_image_buffer(lbDisplay.WScreen, LbGraphicsScreenWidth(), LbGraphicsScreenHeight(),
    scale_value_landview(LANDVIEW_MAP_WIDTH), scale_value_landview(LANDVIEW_MAP_HEIGHT),
    -scale_value_landview(map_info.screen_shift_x), -scale_value_landview(map_info.screen_shift_y),
    map_screen, LANDVIEW_MAP_WIDTH, LANDVIEW_MAP_HEIGHT);
```

Reading `copy_raw8_image_buffer` itself (`gui_draw.c:119`) shows *why* panning works today: the
full (oversized) scaled land image is drawn at a negative destination offset
(`-screen_shift_x/y`), and the function's own edge-of-buffer clamping is what crops it down to
the visible viewport. But that clamp is against `scanline`/`nlines`, which `draw_map_screen`
always passes as `LbGraphicsScreenWidth()/Height()` — **the whole framebuffer, not a rect**. Worse:
the function actively **clears every pixel of every scanline outside the drawn image**
(`memset(dst, 0, dwstart)` / `memset(dst+dwstart, 0, scanline-dwstart)`, plus full clear passes
above/below). That's fine when land-view owns the whole screen (nothing else is on screen to
clear by accident) — it would be actively destructive inside a merged screen, blanking the
campaign list and detail panel sitting beside the preview panel every frame.

This is a different, and more fundamental, gap than a missing rect parameter: `copy_raw8_image_buffer`
needs a genuinely new sibling — a rect-clipped variant that only touches pixels inside its own
rect — before *any* panel-scoped panning can work without corrupting neighboring UI. The existing
rect-aware helper next to it, `frontmenu_copy_background_at`/`get_frontmenu_background_area_rect`
(`gui_draw.c:942,957`, already used for the shared 640×480 backdrop), doesn't help here: it works
by scaling the *whole* source image down to exactly fit the target rect with no panning, so its
`dst_width/height` never exceeds the rect and the existing clamp is never exercised as anything
but a safety net. The land view's whole point is a source image larger than the viewport — the
clip-vs-clear behavior is load-bearing there in a way it isn't for a static backdrop.

## The decorative window-frame overlay is also full-screen, and shouldn't be reused

`compressed_window_draw()` (`front_landview.c:880`) — called every frame in *both* branches of
`frontmap_draw` (zooming and non-zooming) — draws an ornate carved-stone "window frame" overlay
(`map_window` huge sprite) directly against `lbDisplay.WScreen`/`GraphicsScreenWidth`/
`PhysicalScreenHeight`, again full-screen. This is specifically the full-screen cutscene's visual
identity (looking at the land "through a window"); it isn't something a small merged-screen panel
would want reproduced at panel scale even if it could be rect-parameterized. For a panel, the
existing 3-slice panel chrome ([03-button-primitives.md](03-button-primitives.md)'s pattern,
already used elsewhere for list/scroll-box borders) is the right border, not this sprite.

## Ensign click/zoom/speech machinery looks skippable for the merged screen

`frontmap_input` (`front_landview.c:1276`) owns: ESC-to-leave, two debug/easter-egg level-skip
hotkeys, left-click → `frontmap_input_active_ensign`/`clicked_map_level_ensign` (the flag-icon hit
test that zooms into a level and eventually commits to it), and continuous hover tracking for the
same. All of this exists to let the player pick a level *by clicking its flag on the land art*, in
a screen where the land art is the only way to navigate. In the merged Land-selection design, level
identity is already picked via the campaign list on the left and committed via the separate
"Enter this land" button (plan gap 2's highlight/commit split) — the ensign-click path duplicates
that, isn't in the mockup's flow, and every function backing it (`is_over_ensign`,
`frontzoom_to_point`, `frontmap_zoom_out_init/in_init`, `play_description_speech` and friends) adds
more `PhysicalScreenWidth/Height`-coupled surface to fix for no clear benefit. Recommend leaving
it out of the panel entirely rather than porting it.

## Revised, narrower Phase 2 scope

What the merged screen actually needs from `front_landview.c` is much smaller than "the whole
file, rect-parameterized":

1. **A new rect-clipped sibling of `copy_raw8_image_buffer`** (e.g.
   `copy_raw8_image_buffer_rect`) that clips/clears only within a given rect instead of the whole
   scanline — the one genuinely new primitive this phase needs, reusable by anything else that
   ever wants a panel-scoped raw-image blit.
2. **A panel-scoped pan state and update, not the full `MapLevelInfo`.** Reuse the shift/velocity
   *algorithm* from `update_velocity`/`check_mouse_scroll` (accel/decel/clamp math is fine as-is),
   but drive it from a small panel-local struct (viewport width/height instead of
   `PhysicalScreenWidth/Height`, mouse position translated to panel-relative before the edge
   checks) rather than reusing `map_info` and its screen-global assumptions.
3. **No `compressed_window_draw`, no ensign click/zoom/speech.** The panel shows the panned
   backdrop art inside the merged screen's own panel chrome; level identity/commit stays owned by
   the list + "Enter this land" button, per gap 2.
4. **Loading**: `load_map_and_window`/`frontmap_load` still centers on loading the right
   `land_view`/`land_window` `.raw` per campaign (`config_campaigns.h` fields, already confirmed
   reusable in the original gap analysis) — that data-loading half of `frontmap_load` is fine to
   reuse close to as-is; it's specifically the *drawing and input* half that's screen-coupled.

This is still real, non-trivial work (new blit primitive, new panel-local pan-state struct/update
function, wiring drag-to-pan input scoped to the panel's rect) but is a small fraction of the
original file's surface, and sidesteps the two hardest, least-necessary pieces
(`copy_raw8_image_buffer`'s full-scanline clearing semantics at full scope, and the ensign
click/zoom/speech state machine) entirely rather than trying to rect-parameterize them.

## Resolved interaction spec (user confirmed)

- **Drag-to-pan the backdrop**, not click-to-zoom — confirms the scope above; `frontzoom_to_point`
  and the zoom fade state machine stay out of the panel.
- **Ensigns are clickable after all**, but repurposed for the highlight/commit split rather than
  the original zoom-and-commit: left-click an ensign → update the detail panel to that *level's*
  name + description (new field, same idea as campaign description below) instead of the
  campaign's; the separate "Play"/"Enter this land" button commits whichever is currently shown.
  Right-click → reset the detail panel back to the campaign's own description.
- **Click on an ensign also triggers that level's description speech.** `play_description_speech`
  (`front_landview.c:730`) turns out to be a clean, already-reusable function — no
  `PhysicalScreenWidth/Height` coupling at all, just `get_level_info(lvnum)` +
  `play_streamed_sample`. In the original screen this fires on *hover* via `frontmap_update`
  (`front_landview.c:1362`); for the panel, fire it once per click instead (mirrors the new
  highlight action, avoids the noisier continuous-hover trigger inside a small panel surrounded by
  other clickable UI). Reuses `play_description_speech(lvnum, 1)` (the "before"/flavour speech)
  as-is.
- **Per-level description data already has a parser slot, just unused.** `cmpgn_map_commands`
  (`config_campaigns.c:93`) already has `{"DESCRIPTION", 11}` for per-level blocks — it's parsed
  and then explicitly discarded (`config_campaigns.c:998`, `case 10/11/12: // As for now, ignore
  these`). Wiring it into a new `LevelInformation.description` field is a small change, not a new
  parser entry. The *campaign*-level description (gap 3 in the original plan) still needs a new
  `DESCRIPTION` entry added to `cmpgn_common_commands` (`config_campaigns.c:56`) — no such slot
  exists at that level yet.

Ensign hit-testing and hover tracking follow the same *shape* as `is_over_ensign`/
`frontmap_input_active_ensign`, but aren't literally reused — see "What was built" below for why.
`frontmap_zoom_in_init`/`clicked_map_level_ensign`'s `state_trigger = FeSt_START_KPRLEVEL`
commit-on-click behavior is not reused — that's now the "Play" button's job.

## What was built

New module: [`frontmenu_landpreview.h`](../../../src/kfx_frontend/include/frontmenu_landpreview.h)/
[`.c`](../../../src/kfx_frontend/src/frontmenu_landpreview.c), kept separate from
`front_landview.c` rather than adding a rect parameter throughout it — the earlier analysis's
conclusion that the two have too little in common to share code held up once written.

- **`struct LandPreviewPanel`** — a small panel-local pan/highlight state (`screen_shift_x/y`,
  its own `units_per_px`, `dragging`+drag-tracking fields, `highlighted_lvnum`), independent of
  `front_landview.c`'s screen-global `map_info`/`units_per_pixel_landview`.
- **`land_preview_compute_units_per_px`** — picks the panel's own zoom level from its rect size
  (aiming to show roughly half the map at once, matching `calculate_landview_upp`'s ~2x-zoom feel
  but computed from the panel instead of the physical screen). Confirmed by reading
  `calculate_landview_upp` (`bflib_video.c:1280`) that the existing global is derived from the
  *game window's* size specifically to make the land art ~2x the window — reusing it for a small
  panel would zoom in far too tight, so this needed its own formula, not reuse.
- **`land_preview_clamp_shift`** — same clamp shape as `update_velocity`'s, parameterized by the
  panel's own rect/scale instead of `PhysicalScreenWidth/Height`.
- **`land_preview_point_over_ensign_box`** — same asymmetric hit-box as `is_over_ensign`
  (horizontally centered on the ensign anchor, vertically only the flag banner above it), as a
  pure function taking map-space coordinates directly rather than reading `map_info`/
  `units_per_pixel_landview` off globals — that's why it's a new function and not a reuse of
  `is_over_ensign` itself, even though the underlying box math is identical.
  All four of the above are pure arithmetic and unit-tested directly
  ([`frontmenu_landpreview_test.cpp`](../../../src/kfx_frontend/tests/frontmenu_landpreview_test.cpp)).
- **`land_preview_load`/`_unload`** — thin wrappers around `load_map_and_window(SINGLEPLAYER_NOTSTARTED)`
  (the campaign's own `land_view_start`/`land_window_start`, reused verbatim) and the new
  `load_map_ensign_sprites()` (extracted out of `frontmap_load` — same behavior, just callable
  without `frontmap_load`'s continue-level/zoom-fade/music/ambient-sound orchestration, which is
  specific to the full-screen cutscene's entry sequence and not wanted for a live campaign-list
  preview). Caller is expected to have already called `change_campaign()` for the highlighted
  campaign.
- **`land_preview_maintain`** (per-frame, via a `GuiButtonInit.maintain_call`) — polls
  `left_button_clicked`/`right_button_clicked`/`lbDisplay.LeftButton` directly, the same raw-flag
  style `frontmap_input` itself uses, rather than going through the generic per-button
  `click_event`/`rclick_event` dispatch: left-click on an ensign highlights it (sets
  `highlighted_lvnum`, updates the shared `mouse_over_lvnum` so `get_ensign_sprite_for_level`'s
  existing highlight-frame animation picks it up, and calls `play_description_speech(lvnum, 1)`
  verbatim); left-click elsewhere starts a drag; holding the button pans `screen_shift_x/y` by the
  per-frame mouse delta; right-click anywhere in the panel clears `highlighted_lvnum` back to
  "show the campaign's own description."
- **`land_preview_draw`** (via `GuiButtonInit.draw_call`) — backdrop via the new
  `copy_raw8_image_buffer_rect` (see below), ensigns via `get_ensign_sprite_for_level` (reused
  as-is — no screen coupling, already handles hidden/animation/custom-ensign lookup) positioned
  relative to the panel's own rect instead of the screen, with an extra visibility check per
  ensign since (unlike the full-screen version) most of the map is normally *not* on screen at
  once.

**New shared primitive**: `copy_raw8_image_buffer_rect` (`gui_draw.c`/`.h`) — the rect-clipped
sibling of `copy_raw8_image_buffer` this phase's blocker (see above) needed: confines both drawing
and margin-clearing to an explicit rect instead of the whole scanline/buffer. Unit-tested directly
against synthetic buffers (no sprites needed) —
[`gui_draw_rect_blit_test.cpp`](../../../src/kfx_frontend/tests/gui_draw_rect_blit_test.cpp)
covers confinement, panning, margin-clearing, and edge/degenerate-rect clipping.

**Data layer**: `LevelInformation.description` (new field) wired to the per-level `DESCRIPTION`
command that was already tokenized and discarded (`config_campaigns.c` case 11); and
`GameCampaign.description` (new field) plus a new `DESCRIPTION` command added to
`cmpgn_common_commands` (case 23) — the gap 3 campaign-description work from the original plan,
implemented now since the detail panel needs both a campaign-level and a level-level description
to switch between.

## The merged screen

`frontend_select_campaign_menu`/`frontend_select_campaign_buttons`
([`frontmenu_select_data.cpp`](../../../src/kfx_frontend/src/frontmenu_select_data.cpp)) is
reworked in place — same `GuiMenu`/list engine (`campaign_select_list`), same screen identity
(`GMnu_FECAMPAIGN_SELECT`/`FeSt_CAMPAIGN_SELECT`), new layout and new rows:

- **List** (left, narrowed to `FE_LANDSEL_LIST_W`=190px, `x`=24 matching Phase 1/4's margin):
  same 7-row `campaign_select_list` engine, same scroll tab/arrows, just repositioned.
- **Preview panel** (right): one new `GuiButtonInit` row with `.draw_call = land_preview_draw`,
  `.maintain_call = land_preview_maintain`, no `.click_event` — all input (ensign click/right-
  click, drag) is polled raw inside `land_preview_maintain` itself, same style
  `frontmap_input` already uses, not the generic per-button click dispatch.
- **Detail panel** (right, below preview): new row, `.draw_call = frontend_draw_land_selection_detail`.
- **"Enter this land" / "Return to Main"** (bottom): two new/repositioned rows using
  `frontend_draw_button_icon` (the Phase 1/3 flexible-width chrome), left-anchored to match.

**Highlight vs. commit split** (gap 2), landed in
[`frontmenu_select.c`](../../../src/kfx_frontend/src/frontmenu_select.c):
- `frontend_campaign_select` (list row `click_event`, name kept as-is so `button_snapping.c`'s
  `gbtn->click_event == frontend_campaign_select` identity check keeps working) now *highlights*:
  `change_campaign()` + `land_preview_unload`/`_load` to swap the preview art, tracked via a new
  file-local `land_selection_highlighted_campaign`. It no longer commits.
- `frontend_land_selection_enter` (new, the "Enter this land" button) does what the row's
  click used to: `frontend_start_new_campaign()` + `frontend_set_state(FeSt_CAMPAIGN_INTRO)` —
  plus, if an ensign is highlighted (`land_preview.highlighted_lvnum`), `set_continue_level_number()`
  to that level first, so committing starts *that* level rather than always the campaign's first.
- **Per-entry refresh**: `frontend_campaign_list_load()` (called by `frontend_setup_state` on
  every entry into `FeSt_CAMPAIGN_SELECT`, not just the menu's first-ever creation — confirmed by
  reading `create_menu()`, whose `create_cb` only fires once) now also highlights the first
  campaign and loads its preview, so the panel isn't empty on entry. A `GuiMenu.create_cb` was
  tried first and reverted once this distinction became clear.
- **Centralized unload**: `frontend_shutdown_state()`'s `FeSt_CAMPAIGN_SELECT` case now also calls
  `land_preview_unload`, the same way `FeSt_LAND_VIEW`'s case already calls `frontmap_unload()` —
  catches every way of leaving the screen (not just the two new buttons) in one place.

**New string**: `GUIStr_MnuEnterLand` ("Enter this land", `guitext:1123`) — no existing string
fit; added the same way the main-menu title text was changed earlier this session (new `.pot`
entry, regenerated `pkg/fxdata/gtext_eng.dat`).

**Still open**: no way to visually verify any of this without game data files — positioning,
scaling, and the drag/click feel all need a real smoke test.
