# Phase 4 — views, camera, and editor overlays

Status: **not started.** Depends on phase 1. Independent of 2/3 — can land in parallel.
**No renderer dependency** — every View item wires to an existing engine feature
([`07`](07-investigation-findings.md) F9); only the overlays are new.

Goal: the View menu from the original editor (Plan / Isometric / 1st-person / Lights / Map /
Low Walls) plus the editing overlays a modern editor needs (grid, coordinates, thing/light/AP
markers, verification flags).

---

## 1. What the engine already gives us

| Original View item | Engine equivalent today |
|---|---|
| Isometric | the normal 3D game view (`ViewMode` default) |
| Plan / top-down | parchment map **zoom box** (`draw_zoom_box`, `gui_parchment.c:861`) — a magnifier over the overhead map, `minimap_zoom` 128–2048, renders terrain *and* things ([`07`](07-investigation-findings.md) F9) |
| 1st Person | the **spectator ("ghost") camera** — `level_lost_go_first_person` / `PckA_GoSpectator` / `crtr_conf.spectator_breed` (`CMF_IsSpectator`) — flies free, invisible to gameplay ([`07`](07-investigation-findings.md) F21) |
| Lights toggle | `cmd_toggle_lights` / `PckA_ToggleLights` |
| Map (full screen) | parchment full-screen map |
| Wall Height toggle | **`settings.video_cluedo_mode`** ("cluedo mode" / low walls) — in-game video options, `PckA_SetCluedo`, renderer already implements (wall height 5→2, `engine_render.c:2491`) ([`07`](07-investigation-findings.md) F9) |

**Everything in the View menu is wiring existing features** — no new renderer work
([`07`](07-investigation-findings.md) F9). Only the overlays are new, and they go through the
existing `RenderOverlayCallbacks` seam.

## 2. View menu (editor menu bar → View)

- **Isometric** (default): normal camera. Editor camera should allow a higher pitch and
  further zoom-out than gameplay. Add an editor camera profile — looser zoom clamps, free
  rotate.
- **Plan / Top-down**: open the parchment map with the **zoom box magnifier** (`draw_zoom_box`)
  — it's already a true top-down projection rendering terrain + things, with `minimap_zoom`
  128–2048. The editor's Plan toggle drives that; no new ortho camera. (If in-place editing
  under a top-down 3D camera is wanted later, that's a separate renderer ask — not v1.)
- **1st Person**: spawn + possess a **spectator creature** at the cursor — the same free-fly
  "ghost" camera a player gets on defeat ([`07`](07-investigation-findings.md) F21:
  `level_lost_go_first_person` / `PckA_GoSpectator`, `CMF_IsSpectator` — excluded from AI,
  traps, combat, win/lose). A small `PckA_EditorGoSpectator` variant taking a position (vs the
  original's "random owned creature"). Exit via the normal `PckA_DirectCtrlExit`. WASD + mouse
  look. Needs `crtr_conf.spectator_breed` set (standard KFX config). **Wiring, not new work.**
- **Full Map**: existing parchment full-screen map, with editor annotations (AP numbers, hero
  gates, heart locations).
- **Lights**: `PckA_ToggleLights` — preview dynamic lighting on/off.
- **Wall Height (Low Walls)**: send **`PckA_SetCluedo`** to toggle `settings.video_cluedo_mode`
  — the engine's existing "cluedo mode" that renders walls at height 2 instead of 5 so you can
  see into rooms. Same setting as the in-game video options; the editor just surfaces the
  toggle in its View menu. No renderer work.

## 3. Overlays (editor menu bar → View → Overlays, individually toggleable)

Drawn through the same overlay path the in-game GUI / debug overlays use
(`RenderOverlayCallbacks`, `frontgui_ingame_debug.cpp`). All gated on `editor_is_active()`.

- **Slab grid**: thin lines on 3×3 slab boundaries; heavier every 5 slabs. Essential for
  precise placement.
- **Coordinate readout**: cursor slab (x,y) and subtile — `cmd_toggle_tooltip_land_coord`
  already toggles a land-coord tooltip; promote it to an always-available editor overlay.
- **Ownership tint**: translucent player-colour wash per slab (like the minimap colours) so
  you can see claimed areas at a glance in the 3D view.
- **Thing markers**: floating icons over placed creatures / objects / traps / doors with a
  tiny label (model + owner + level), so things are findable without hunting. Toggle by
  category.
- **Light markers**: a bulb icon at each static light; selected light shows its radius as a
  ring on the floor and its height as a vertical stick.
- **Action point / hero gate markers**: the AP number in a circle showing its trigger radius;
  hero gates as a distinct glyph with their number.
- **Verification flags**: after a `verify_map()` run (phase 3), pulsing markers on offending
  slabs, colour by severity; click cycles through them.
- **Brush/mark rectangle**: the current marked region and brush-capture box (reuse roomspace
  highlight rendering).

## 4. Camera controls in the editor

- Free pan (edge scroll + MMB drag + WASD), free rotate (Q/E or RMB drag), zoom (wheel) with
  editor-profile clamps.
- **Zoom-to**: from verification list, from the AP/thing inspector, from the full map —
  `PckA_ZoomToPosition` already exists.
- **Bookmarks**: `PckA_BookmarkLoad` exists for gameplay; optionally expose numbered editor
  camera bookmarks (nice-to-have, not in the original... actually the original had none —
  defer).

## 5. What must exist after phase 4

1. Editor View menu: Isometric / Plan (parchment zoom box) / **1st Person (spectator camera)** /
   Full Map / Lights / Low Walls — all wiring to existing engine features (`PckA_ToggleLights`,
   `PckA_SetCluedo`, parchment `draw_zoom_box`, `PckA_GoSpectator`).
2. Editor camera profile with looser clamps + free rotate.
3. Toggleable overlays: slab grid, coordinates, ownership tint, thing markers, light markers,
   AP/hero-gate markers, verification flags, mark/brush rectangle.
4. Overlays render only when `editor_is_active()`, via the existing `RenderOverlayCallbacks`.
5. **No new renderer code** ([`07`](07-investigation-findings.md) F9).

## 6. Tests

- ftest `editor_views`: cycle Isometric → Plan → 1st Person (spectator) → Full Map → back;
  assert no crash, spectator spawned/despawned, camera
  mode changes, `editor_is_active()` preserved.
- ftest `editor_overlay_toggles`: toggle each overlay on/off over N frames, assert stable.
- Manual/visual: the user reviews grid alignment, wall-height cut, and marker legibility on a
  real map (live-desktop review — ask first per the repo's interaction rules).
