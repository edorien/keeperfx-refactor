# Phase 2 — the editing toolbox

Status: **not started.** Depends on phase 1 (editor session + `editor_is_active()`).

Goal: a proper editor UI over the player-work-state machine — a tool palette, category pickers,
a "what am I placing" cursor preview, and the marking / brush / fill mechanics — so building a
map is a paint-program experience, not a cheat-menu radio list.

---

## 1. The model: tools = player work-states, driven by a better UI

Every tool below already has a `PSt_*` work-state and a `PckA_Cheat*` packet handler in
[`packets_cheats.c`](../../../src/kfx_net/src/packets_cheats.c). Phase 2 does **not** add new
world-mutation logic for the common cases — it adds:

1. A **tool palette** (ImGui panel, `frontgui_widgets` wrappers) that sets
   `player->work_state` via `PckA_SetPlyrState` (exactly what `gf_change_player_state` in
   `gui_boxmenu.c` does) and shows the active tool.
2. **Category pickers.** The **existing** `player->cheatselection` fields
   ([`player_data.h:138`](../../../src/kfx_sim/include/player_data.h) — `chosen_terrain_kind`,
   `chosen_player`, `chosen_creature_kind`, `chosen_hero_kind`, `chosen_experience_level`) are
   set as-is to drive the existing `PSt_*` cheat modes — the pickers just replace the
   `PckA_CheatSwitch*` cycle-only inputs with grids/lists. **New** tool params (object model,
   trap/door kind, light intensity/radius/height, AP radius, fill target, brush buffer) live
   in `kfx_editor`'s own state and travel in the packet params of new `PckA_Editor*` verbs —
   **not** as new `CheatSelection` fields ([`07`](07-investigation-findings.md) F17:
   `CheatSelection` is inside the `kfx_sim_state` save/resync blob).
3. A **shared bottom bar**: player selector (0–3 / Hero / Neutral) and experience level (1–10),
   mirroring the original editor's always-visible bottom icons. These feed `cheatselection`
   for whichever tool is active.
4. **Cursor feedback**: reuse `tag_cursor_blocks_place_terrain()` / roomspace highlight; add a
   ghost sprite / slab preview under the cursor for the current tool.

## 2. Tool list

### 2.1 Terrain / Room / Ownership paint
- **Tool:** `PSt_PlaceTerrain` (already paints any `SlbT_*` with an owner and deletes rooms on
  overwrite).
- **Picker:** a slab-kind palette grouped as the original panels were — *Tiles* (earth, rock,
  impenetrable, gold, dirt path, claimed floor, pretty path/wall, lava, water, gems) and
  *Rooms* (every `RoomKind` → its `assigned_slab`, incl. atomic 3×3 Portal & Heart). Source of
  truth: `kfx_config_state.conf.slab_conf` and `room_conf` — the palette is generated, so new
  modded slab/room kinds appear automatically.
- **RMB** = paint earth (already the handler's behaviour).
- **Atomic rooms:** Portal / Heart place as 3×3; delete only (RMB with a room/tile tool),
  confirm dialog — same rules as the original. `place.slab` already special-cases
  `RoK_DUNGHEART`.
- **`r` — reinforce perimeter:** for the selected player, convert their dungeon's earth
  perimeter to reinforced wall. New small helper in `kfx_sim` (walk owned slabs, fortify
  bordering earth) exposed as `PckA_CheatReinforcePerimeter`, or a console-style loop. Original
  shortcut `r`.

### 2.2 Marking (rectangular select) + area ops
- Reuse the roomspace drag machinery (`PckA_SetRoomspaceDrag` / `create_box_roomspace` /
  `RoomspaceHighlightToggle`). `Ctrl+drag` (or a "Mark" toggle) draws a rectangle; then a
  single click of a terrain/room/ownership value applies it to the whole rectangle — one packet
  carrying the rect + value, handled like a batched `PckA_CheatPlaceTerrain`.
- Area ops on a marked rect: **set owner**, **fill with slab**, **clear to earth**, **delete
  things inside**.

### 2.3 Fill (flood)
- New `PSt_EditorFill` + `PckA_EditorFloodFill` carrying `pos_x/pos_y` (seed) + `actn_par1`
  (slab kind) + `actn_par2` (owner) — all fits one packet. The **handler** does the 4-connected
  flood from the seed across contiguous same-kind, same-unclaimed-ness slabs, bounded by the
  map and by rooms (original refuses to flood rooms). Area-capped (whole map max).

### 2.4 Brush (grab region → stamp)
- Right-drag a rectangle to **capture** into an in-editor clipboard: slab kinds + ownership +
  things + lights + APs within the box (client-side buffer in `kfx_editor`, not world
  state).
- LMB **stamps** the buffer at the cursor as a **burst of primitive `PckA_Editor*` packets**
  (one per slab / thing — the buffer can't fit in one packet's ~5 scalar params,
  [`07`](07-investigation-findings.md) F17). Queued over frames if large. Fallback if a big
  stamp is visibly janky: `kfx_editor` calls the sim mutations directly (single-player-local
  exception, D2). The undo journal records one "stamp" entry regardless.
- Original restrictions to keep: can't stamp over a Heart or Portal; Gems / Guard Post /
  Bridge don't survive a grab (engine-derived slabs).

### 2.5 Creatures / Heroes / Diggers
- **Tools:** `PSt_MkBadCreatr` / `PSt_MkGoodCreatr` / `PSt_MkDigger` (all live).
- **Picker:** creature-model grid — loop `[1, crtr_conf.model_count)` (`CREATURE_TYPES_MAX =
  128`), split evil / hero, icons via `SpriteLookupCallbacks`, labels from
  `creature_code_name(model)`. Sets `cheatselection.chosen_creature_kind` / `chosen_hero_kind`.
  **Custom / campaign-modded creatures appear automatically** — they're just extra `crtr_conf`
  entries ([`07`](07-investigation-findings.md) F15). Same principle for every palette in this
  phase (objects, traps, doors, rooms, slabs).
- Bottom bar's player + experience feed owner and level.
- **255-thing cap** enforced with a visible counter (original's warning).
- RMB delete (via `PSt_DestroyThing` fallthrough or tool-specific).

### 2.6 Objects / Spellbooks / Specials / Gold / Decor
- **Tool:** `PSt_MkGoldPot` for gold; a new `PSt_EditorPlaceObject` for the rest (thin wrapper
  around `create_thing` / `PckA_CheatMakeObject` — `console_cmd.c cmd_create_object` shows the
  call).
- **Picker:** object-model grid from `object_conf` (spellbooks, dungeon specials, gold pots,
  hearts-as-object, food, decor, traps-as-boxes). `s`/`x` value tweak for gold amount,
  spellbook power, special kind — property panel, not just cycle keys.
- **Sub-tile precision:** original places one object per 3×3 sub-square at the cursor's
  sub-tile. Packet already carries `stl_x/stl_y`; keep sub-tile resolution for objects (unlike
  slabs which snap to the 3×3 slab).
- Rules: one object per sub-square; not on a square occupied by creature/trap/spell/secret;
  no gold bags in a Treasure Room (original constraints — port the checks).

### 2.7 Traps / Doors
- **Tool:** new `PSt_EditorPlaceTrap` / `PSt_EditorPlaceDoor` (wrappers over trap/door thing
  creation; `thing_traps.c` / `thing_doors.c` have the constructors; `give.trap`/`give.door`
  show config lookup).
- **Picker:** trap-kind / door-kind grid from `trapdoor_conf`.
- Door rules: only on the selected player's claimed floor, between two walls (port the
  original's adjacency check). `create_door(pos, model, orient, plyr, is_locked)`
  ([`thing_doors.c:102`](../../../src/kfx_sim/src/thing_doors.c)) takes the lock state at
  creation; **Ctrl+LMB toggles it live** via `lock_door()` / `unlock_door()`
  ([`thing_doors.c:219`/`230`](../../../src/kfx_sim/src/thing_doors.c)) — needs a small
  `PckA_EditorToggleDoorLock` verb. Persisted as `DoorLocked` in `tngfx`.
- Traps: any land incl. unclaimed; not on an occupied square.

### 2.8 Lights & effect emitters
- Covered in [`05-script-and-level-settings.md`](05-script-and-level-settings.md) §2 (they need
  a property panel — intensity/size/height for lights, radius for effect generators — so they
  live with the other "properties" work). The palette entry + place/delete tool stubs are
  created here so the toolbox layout is complete.

### 2.9 Action points & hero gates
- Covered in [`05`](05-script-and-level-settings.md) §3 (numbering, radius handles). Palette
  stubs here.

### 2.10 Query / Delete / utility
- **Query** (`PSt_QueryAll`): click anything → property panel for that slab/thing/light/AP.
  This is the inspector the property panels (phase 5) render into.
- **Delete** (`PSt_DestroyThing` + slab→earth for terrain): RMB in most tools; also an
  explicit eraser tool.
- **Eyedropper:** click a slab/thing → set the active tool + picker to match it (quality-of-
  life, not in the original; cheap given `cheatselection`).

## 3. Toolbox UI layout

An ImGui dock/panel set shown only when `editor_is_active()`:

- **Left:** vertical tool strip (Terrain, Rooms, Creatures, Objects, Traps/Doors, Lights, FX,
  Action Points, Query, Brush, Fill, Mark).
- **Left, below:** the active tool's **picker** (slab grid / creature grid / …).
- **Bottom:** player selector + experience selector + thing counter + coordinate readout.
- **Right:** the **inspector** (Query results / selected-object properties).
- **Top:** the Editor menu bar (File: New/Open/Save/Save As; Edit: Undo/Redo/Clear Map;
  View: → phase 4; Playtest; Help).

Build with `FeBeginPanel` / `FeButton` / `FeBeginListBox` / `FeBeginScrollArea` etc. Icons via
the sprite-lookup callback (`SpriteLookupCallbacks`) so slab/room/creature/object art matches
the game. Follow the in-game GUI project's **deferred-action** rule for anything that transitions
player state from a click.

**Keybindings (D6).** Every tool shortcut is a **definable key** — add `Gkey_Editor*` entries
to `settings.kbkeys[]` ([`config_settings.c:50`](../../../src/kfx_config/src/config_settings.c),
`struct GameKey[GAME_KEYS_COUNT]`), each with a GUI-string label and a default from the 1998
manual (`F1`–`F9` tiles, `0`–`5` players, `f`/`b`/`z` fill/brush/paint, `t` texture,
`l` lights, `Ctrl+Z`/`Ctrl+Y` undo/redo, `Delete`, `Tab` mode, `p`/`i`/`o` views, …). They
appear in the Define Keys menu automatically (classic + ImGui `frontgui_definekeys_frame`);
the editor reads them via the normal `is_key_pressed` path. Bump `GAME_KEYS_COUNT`; these live
in the settings file, not the sim blob ([`07`](07-investigation-findings.md) D6).

## 4. Command journal (undo/redo)

Design the toolbox around a journal from the start (O4):
- Every editor packet action also appends a record to `kfx_editor`'s command journal:
  `{ action, args, inverse_args }`. For terrain: inverse = prior slab kind + owner (read before
  write). For thing place: inverse = delete that thing. For delete: inverse = recreate from a
  captured snapshot.
- **Undo** replays the inverse as a normal packet action; **Redo** replays the forward action.
- Cap the journal (e.g. 200 entries) or snapshot-and-truncate.
- Ship undo/redo in this phase if the inverse capture is straightforward for terrain + thing
  place/delete (the 90% case); defer brush/fill/area-op undo to phase 6 if fiddly.

## 5. What must exist after phase 2

1. Toolbox panel set, shown on `editor_is_active()`, hidden otherwise.
2. Terrain/room/ownership paint with a generated slab+room palette and cursor preview.
3. Player + experience bottom bar feeding `cheatselection`.
4. Creature / hero / digger placement with a model-grid picker + thing counter.
5. Object / spellbook / special / gold placement with a model grid + value property.
6. Trap / door placement with kind grid + door rules + lock toggle.
7. Marking rectangle + area ops (set owner / fill / clear / delete-things).
8. Fill (flood) tool.
9. Brush grab-and-stamp with an editor clipboard.
10. Query inspector + eraser + eyedropper.
11. Command journal with undo/redo for terrain + thing place/delete.

## 6. Tests

- ftest `editor_paint_terrain`: select rock, paint a 5×5 block via synthesized packets, assert
  slab kinds + ownership + that columns/collision updated (walkability query).
- ftest `editor_place_creature`: place 3 imps for player 0 at level 5, assert count, owner,
  experience; delete one, assert count.
- ftest `editor_fill`: enclosed earth pocket, flood with path, assert bounded by walls.
- ftest `editor_brush`: grab a 3×3 room+creatures, stamp elsewhere, assert deep-equal.
- ftest `editor_undo`: paint → undo → assert original slab; place thing → undo → assert gone.
- Catch2: palette generation from a synthetic slab/room/creature config (counts, grouping);
  journal inverse-arg capture.
