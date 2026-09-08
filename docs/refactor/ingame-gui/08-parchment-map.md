# Phase 7 — the parchment map

Status: **started 2026-09-07 — chunk 1 (seam + ImGui overlay) landed.** Depends on
[06-tab-content-panels.md](06-tab-content-panels.md) (for the dynamic-texture panel pattern).

**Chunk 1 landed:** `frontgui_ingame_parchment.{h,cpp}` — `ingame_parchment_frame()` from
`ingame_imgui_frame()`, gated on `get_my_player()->view_mode == PVM_ParchmentView` (parchment is
not a `GMnu_*` menu). Full-screen transparent `NoInputs` window drawing a crisp `FeFont_Heading`
level name + a border around `get_parchment_map_area_rect()`. `redraw_parchment_view()` still runs
the legacy raster (parchment paper + overhead map + zoom box straight to the framebuffer, all
true-colour `TbPixel` now — composited under ImGui); only `draw_map_level_name()` is gated off
under `RendererImGuiEnabled()`. `get_map_level_name()` extracted from `draw_map_level_name`,
`get_parchment_background_area_rect` exported. Input (`point_to_overhead_map` via
`engine_redraw.c` screen→map) untouched.

**Chunk 2 landed (2026-09-07):** `ingame_parchment_frame()` now redirects the whole legacy raster
(`load_parchment_file` + `draw_map_parchment` + `draw_2d_map` + `draw_zoom_box`) into a screen-sized
dynamic texture via `RendererSwapFramebufferTarget` (bracketed by `LbScreenStore/LoadGraphicsWindow`
+ a full-buffer `LbScreenSetGraphicsWindow`), then composites it as a 1:1 full-screen `ImGui::Image`
with the level name on top. `redraw_parchment_view()` skips those draws under `RendererImGuiEnabled()`
(keeps `draw_gui` / `gui_draw_all_boxes` / `draw_tooltip` — all no-ops or ImGui-migrated).
`ingame_panel_frame()` early-returns in `PVM_ParchmentView` so the sidebar doesn't draw over the
map. The image is 1:1 with the screen so `point_to_overhead_map` still works unchanged. Fixes from
chunk-1 retest: sidebar hidden, decorative border removed (it drew over the mouse-following zoom
box — the zoom box now lives inside the captured texture).

**Chunk 2 retest fix (2026-09-07):** the full-screen opaque `ImGui::Image` hid the framebuffer
cursor (the game cursor draws into the framebuffer at `LbI_PointerHandler::OnBeginSwap`, before
ImGui composites over it) — user saw only the mouse-following zoom box, which reads as "cursor
stuck at the bottom" once it clamps near the screen edge. Fixed by adding `ingame_parchment_active()`
to `main.cpp`'s `RendererSetScreenOwnedCallback` lambda: in parchment view `RendererScreenOwned()`
is now true, so `PresentFrame()` skips the (empty) framebuffer blit, the legacy cursor draw is
skipped (`OnBeginSwap` / `OnMove` guard on `!RendererScreenOwned()`), and ImGui draws its own cursor
(`want_cursor = WantCaptureMouse || ImGuiContextScreenOwned()`). Same mechanism the 15 ImGui-owned
frontend screens use.

**Dead-fade cleanup (2026-09-07):** `zoom_to_parchment_map` / `zoom_from_parchment_map` simplified
to just the no-fade branch — the `PhysicalScreenWidth <= 320` fade path (`PckA_SetViewType,
PVT_MapFadeIn/Out`) is unreachable at any current INGAME_RES. The `PVM_ParchFadeIn/Out` enum values,
their `redraw_display` cases, `map_fade_in/out`, `prepare_map_fade_buffers` and
`redraw_minimal_overhead_view` (only reachable via that fade path) are now dead but left in place —
a full cross-layer enum deletion is a separate low-value change.

**Chunk 3+ (not started, low priority):** ImGui-native parchment paper + frame (needs the zoom box
out of the captured texture, drawn as its own ImGui element — magnified terrain). The parchment is
functionally complete as an ImGui-composited view; this is pure re-skin polish.

Goal: the full-screen "parchment" overhead map (`PVT_MapScreen` / `PVT_MapFadeIn/Out`, reached via
the fullscreen-map sidebar button and the map key) — the zoomable overhead view with the
magnified "zoom box", level name, and hotspots.

---

## 1. Legacy composition (`gui_parchment.c`)

- `redraw_parchment_view()` (`:942`): `load_parchment_file()` → `draw_map_parchment()` (the
  parchment paper background, `parchment_copy_background_at`) → `draw_2d_map()` (the overhead
  terrain — `draw_overhead_map` + `draw_overhead_room_icons` + `draw_overhead_things`, each an
  8-bit style-tag → ghost-blend or flat-fill per slab, `get_overhead_mapblock_style`) →
  `draw_gui()` → `gui_draw_all_boxes()` → `draw_zoom_box()` (the magnified terrain+things inset,
  `draw_zoom_box_terrain` / `_things`) → `draw_map_level_name()` → `draw_tooltip()`.
- `redraw_minimal_overhead_view()` (`:959`): the cut-down version (no zoom box / boxes).
- Entered/left by `zoom_to_parchment_map()` / `zoom_from_parchment_map()` (`:967` / `:989`) —
  **packet-routed** view-type changes (`PckA_SaveViewType` / `PckA_SetViewType` /
  `PckA_LoadViewType`), with a `network_is_active() || screen > 320` branch for low-res/legacy.
- Reached through `render_overlay->redraw_parchment_view()` /
  `->load_and_redraw_minimal_overhead_view()` / `->point_to_overhead_map()` from `kfx_render`.

`point_to_overhead_map()` (`gui_parchment.c:152`) maps a screen point to a map coord — the hit
test for clicking the parchment to recentre / issue orders.

---

## 2. Why land-preview reuse works

`frontmenu_landpreview.c` already reimplements a *static* version of this: `land_preview_build_minimap()`
(`:310`) walks slab data into a colour buffer via `land_preview_minimap_slab_color()` (`:365`) —
structurally parallel to `draw_overhead_map`'s `get_overhead_mapblock_style`. It renders that to an
off-screen buffer, uploads a dynamic texture, and draws it with `ImGui::Image` inside an ornate
frame with drag-to-pan and ensign hit-testing (`land_preview_maintain`, `land_preview_ensign_at`).

The parchment map is the same panel with:
- the *live* dungeon map instead of a campaign level's static preview (invalidate the buffer on
  slab/room/reveal change, or rebuild per game-turn);
- full-screen instead of a right-column panel;
- the zoom box (a second, magnified `ImGui::Image` of a sub-region + things);
- click-to-order / click-to-recentre via `point_to_overhead_map` rebased on the image rect.

---

## 3. Approach

- An ImGui full-screen (or near) window: parchment-paper background (existing sprite as a
  texture), the overhead map as a dynamic-texture `ImGui::Image` built from a slab→colour pass
  (port `get_overhead_mapblock_style` to fill an RGBA buffer — it is already a clean
  style-tag → colour function now that `TbPixel` is true-colour), the zoom box as a second
  `ImGui::Image` sampling a magnified sub-rect, level name as `FeHeading`, tooltips via Phase 3's
  world-tooltip draw.
- Keep `zoom_to`/`zoom_from_parchment_map` packet-routed view changes exactly as-is — the ImGui
  side just renders when `player->view_mode == PVM_ParchmentView` (or the view-type equivalent).
- `point_to_overhead_map` → screen point via the image rect + zoom/pan → map coord. Orders issued
  on the parchment (`front_input.c` parchment-view input, e.g. `front_input.c:2500`) stay
  packet-routed.
- `gui_draw_all_boxes()` over the parchment — comes from Phase 2's `gui_boxmenu` migration.

---

## 4. Entanglements / risks

- **View-type state machine** — `PVT_MapScreen` / `PVT_MapFadeIn` / `PVT_MapFadeOut` and the
  `GOF_ShowPanel` toggle dance in `zoom_to_parchment_map`. The fades are legacy screen transitions
  (stage 4 removed the *frontend* fades; these are separate). Decide: keep the fade, or drop it
  like the frontend did.
- **`draw_overhead_map` ghost-blends against on-screen pixels** for tagged/gold/gems/wall styles
  — that "blend against whatever's underneath" needs to become a real per-pixel blend into the
  RGBA buffer (stage 2's blend math), not a table lookup.
- **`draw_2d_map` is also used by `redraw_minimal_overhead_view`** — the minimal path (used during
  some cutscenes / net waits) must keep working; migrate both or gate carefully.
- **Low-res / `screen <= 320` branch** — probably dead in practice (INGAME_RES minimum), but
  confirm before deleting.
- **Reached from `kfx_render`** via `render_overlay` — keep `kfx_render` ImGui-free; the
  `redraw_parchment_view` callback becomes "submit the ImGui parchment window".

## 5. Checklist

- [ ] Slab→colour overhead pass filling an RGBA buffer (port `get_overhead_mapblock_style`,
      real blend for ghost styles).
- [ ] Full-screen ImGui parchment window: paper bg + map image (shared map-sized buffer, §6) +
      zoom box (UV sub-rect of the same buffer) + level name.
- [ ] `point_to_overhead_map` rebased on the image rect; orders/recentre still packet-routed.
- [ ] `zoom_to`/`zoom_from` view-type packets unchanged; `PVT_MapFadeIn/Out` + `screen<=320`
      branch deleted (§6).
- [ ] `redraw_minimal_overhead_view` path still works.
- [ ] `gui_draw_all_boxes` over the parchment (from Phase 2).
- [ ] Both `-classicmenu` states; layering; both builds; Catch2. Interactive: open map, pan, zoom
      box, click to recentre, issue an order from the map, SP + MP.

## 6. Open questions

**Decided (user 2026-09-06, following the Phase 4 minimap decision — same mechanism):**

- **Map-sized buffer, invalidate-on-change** (dig / build / reveal / door), not a per-turn
  rebuild — same as the Phase 4 minimap ([05](05-sidebar-frame-and-minimap.md) §2). The overhead
  map and the panel minimap can even share the rasterised buffer (different zoom, same source).
- **Zoom box = a UV sub-rect of the same map buffer** at a magnified scale — no separate texture,
  no separate rasterise. It's just `ImGui::Image` of a small region of the buffer with a large
  on-screen size.

**Resolved by code investigation (2026-09-06):**

- **Drop the map fade — it's already dead.** `zoom_to_parchment_map()` (`gui_parchment.c:967`)
  branches: `network_is_active() || lbDisplay.PhysicalScreenWidth > 320` →
  `PckA_SaveViewType, PVT_MapScreen` (no fade); *else* → `PckA_SetViewType, PVT_MapFadeIn`. The
  fade path only runs single-player at ≤320px wide, which no current `INGAME_RES` allows. So
  `PVT_MapFadeIn`/`PVT_MapFadeOut` / `PVM_ParchFadeIn` and the `screen <= 320` branch are
  **unreachable in practice — delete both**, matching the frontend's fade removal.
