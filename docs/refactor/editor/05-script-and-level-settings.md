# Phase 5 — level settings, lights & FX, action points, and script

Status: **not started.** Depends on phases 1–3. This is the "properties" phase — everything
that needs a panel of fields rather than a click-to-place gesture.

Goal: make maps *playable and authored*, not just built — level metadata, editable lights/FX,
numbered action points & hero gates, and enough script assistance that a mapmaker doesn't have
to alt-tab to a text editor for the common cases.

---

## 1. Level settings panel (Editor menu → Level → Settings)

Backed by map info + `.lof`/`.lif` + script setup commands. Fields:
- **Name**, author, description (→ `.lof`).
- **Map dimensions** — chosen at New Map ([`07`](07-investigation-findings.md) F12, any size);
  read-only after creation for v1 (resizing a populated map = things/APs outside bounds +
  `around_slab` churn — defer).
- **Base texture set** — dropdown from `texture_pack_desc` (STANDARD, ANCIENT, WINTER, …,
  LATERITE_CAVERN) **plus any custom `tmap?%03d.dat` present** in the campaign config dir
  ([`07`](07-investigation-findings.md) F15); original's `t` cycle. Writes the one-byte
  `map%05d.inf`.
- **Per-slab texture paint** (multi-tileset maps) — a "texture paint" tool that writes
  `kfx_config_state.slab_ext_data[slab]` (a texture-pack id per slab); persisted as
  `map%05d.slx`. Advanced mode; fast-follow if time-boxed.
- **Players** — how many keeper slots (0–3) the map supports; which have hearts (derived from
  placed hearts, shown for sanity); hero player always implicit.
- **Ambient light level**.
- **Start gold per player**, **max creatures per player**, **generation speed** — these are
  script setup commands (`START_MONEY`, `MAX_CREATURES`, `SET_GENERATE_SPEED`). The panel
  edits them as fields and **writes them into the managed region of the script** (§4), rather
  than duplicating a config surface.
- **Creature pool** — `ADD_CREATURE_TO_POOL(kind, n)` rows; also `map.pool` console command
  exists for live testing.

## 2. Lights & effect emitters

Static lights live in the sim and serialize to `.lgtfx` (phase 3). This phase adds the
place / select / edit / delete UX (toolbox stubs from phase 2 §2.8).

- **Place light** tool: click to drop an ambient static light. New `PSt_EditorPlaceLight` +
  `PckA_EditorPlaceLight(x, y)`; the light is created in the sim's static-light list.
- **Select** (Query tool clicks a light marker) → **Light properties** panel:
  - Intensity, Radius/size, Height (original: keypad `.`/`0`, `6`/`4`, `8`/`2` before placing —
    we make them live sliders after placing), colour if the engine supports coloured static
    lights, flags (flicker etc.).
  - Live preview in the 3D view (light updates immediately).
- **Delete**: RMB on a light marker with the light tool (original: right-click).
- **Effect generators** (Lava / Drip / Portal-Ice / Dry-Ice, and any KFX effect-generator
  kinds): placed as things (they already exist as `TCls_EffectGen` / room-effect things).
  - **Place FX** tool with a kind picker; LMB-drag sets the **radius** (original behaviour).
  - Properties panel: kind, radius, spawn rate/amount if configurable.
  - These render live in the editor (particles visible) — one of the nicer WYSIWYG wins.

**Budget warnings** (from the original manual): the engine has a finite dynamic-light list and
FX cost. Show a light count / FX count with a soft cap warning in the panel and in
`verify_map()`.

## 3. Action points & hero gates

Query/zoom/reset already exist (`actionpoint.pos`, `actionpoint.reset`, `zoomto.actionpoint`,
`zoomto.herogate`). This phase adds placement + editing.

- **Add Action Point** tool: LMB-drag from centre outward sets the trigger **radius**
  (original). New `PSt_EditorPlaceActionPoint` + `PckA_EditorPlaceActionPoint(x, y, range)`.
  The AP gets the next free number; the number is shown in the overlay (phase 4) and the
  inspector — the mapmaker needs it for the script.
- **Add Hero Gate** tool: LMB places a numbered hero gate. A hero gate is a **`TCls_Object`**
  (`object_is_hero_gate`), *not* an action point ([`07`](07-investigation-findings.md) F3) —
  so it's placed via the object path (`PckA_EditorPlaceThing` with the hero-gate model) and
  the editor sets its `hero_gate.number`. Heroes/parties enter here.
- **Select** → inspector shows number, position, range (APs only); range editable; **renumber**
  allowed (warn if the script references the old number — cross-check §4).
- **Delete** with a confirm + script-reference warning.
- Serialize: APs → `.aptfx` (`PointNumber`/`SubtileX`/`SubtileY`/`PointRange`); hero gates →
  `.tngfx` as objects with `HerogateNumber` (phase 3).

## 4. Script editing

Full visual scripting is a **non-goal for v1** (00-overview §8). What v1 ships:

### 4.1 Script text editor (Editor menu → Script)
- A multi-line text panel showing `map%05d.txt` (or `.lua`). Monospace, basic syntax tint
  (comments, commands, `IF`/`ENDIF` matching), line numbers.
- **Reload script** button: re-parse and (in playtest) re-apply. In edit mode the script is
  only parsed for validation + the helper panels, never executed.
- **Validate** button: runs the script checks from `verify_map()` §5 (balanced IF/ENDIF,
  condition count, AP/party/creature/room/spell name resolution against current config,
  undefined party refs, `WIN_GAME` presence) and lists errors with line numbers.
- Saved verbatim by `write` (phase 3).

### 4.2 Managed setup region
- The Level Settings panel (§1) and availability helper (§4.3) write into a delimited block:
  ```
  REM --- editor-managed setup: do not hand-edit between these markers ---
  ...generated SET_GENERATE_SPEED / START_MONEY / *_AVAILABLE / ADD_CREATURE_TO_POOL...
  REM --- end editor-managed setup ---
  ```
  Everything outside the markers is the mapmaker's own hand-written script, untouched. On load,
  the panels read their values back from the managed region.

### 4.3 Availability helper (Editor menu → Script → Availability)
A grid: rows = creatures / rooms / spells / doors / traps (from config), columns = per player
0–3 (+ ALL). Each cell: Off / Available / Researchable. Generates
`CREATURE_AVAILABLE` / `ROOM_AVAILABLE` / `MAGIC_AVAILABLE` / `DOOR_AVAILABLE` /
`TRAP_AVAILABLE` lines into the managed region. Mirrors `room.available` /
`creature.available` console commands, which are useful for live playtest tweaking.

### 4.4 Objective / message helper (optional, time-boxed)
Simple form → `QUICK_OBJECTIVE(n, "text", player)` / `QUICK_INFORMATION(n, "text")` /
`DISPLAY_OBJECTIVE`. Inserted at the cursor in the user region, not the managed region.

### 4.5 Creature stats
Original edited `creature.txt` in the game data dir. KeeperFX has two modern equivalents:
`SET_CREATURE_*` script commands, and a **per-map creature config layer** —
`map%05lu.creature.cfg` / `map%05lu.<name>.cfg` in the levels dir
([`07`](07-investigation-findings.md) F15). v1 stays **out of scope** for editing either; a
per-map stats panel that emits `SET_CREATURE_MAX_LEVEL` / `SET_CREATURE_STRENGTH` into the
user script region, or edits `map*.creature.cfg` directly, is a clean **v2** addition.
**Placing** custom creatures already works (§2.5) — only stat *editing* is deferred.

## 5. What must exist after phase 5

1. Level Settings panel (name/author/desc, texture set, players, ambient, gold, max creatures,
   gen speed, pool) reading/writing the managed script region + `.lof`/`.inf`.
2. Light place/select/edit(intensity,size,height)/delete with live preview; serialized.
3. FX generator place(with drag-radius)/select/edit/delete with live preview; serialized.
4. Light/FX budget warnings.
5. Action point (drag-radius) + hero gate (object) placement, numbering, renumber, delete with
   script-ref warnings; APs → `.aptfx`, hero gates → `.tngfx` objects.
6. Script text editor with tinting, reload, validate.
7. Managed setup region + availability helper grid.
8. (Time permitting) objective/message helper.

## 6. Tests

- ftest `editor_lights`: place a light, set intensity/radius, assert sim light params; delete.
- ftest `editor_action_points`: place AP #1 range 3, place hero gate #1, save, reload, assert
  numbers/positions/range; script `IF_ACTION_POINT(1, PLAYER0)` resolves in validate.
- ftest `editor_managed_script`: set gen speed + start gold in the panel, save, reload, assert
  panel shows same values and the `.txt` managed region contains the commands once (not
  duplicated).
- Catch2 `script_validate`: unbalanced IF, >48 conditions, unknown creature name, undefined
  party, missing AP reference — each flagged with the right line.
- ftest `editor_availability`: mark Bile Demon researchable for player 0, save, playtest,
  assert not available at t=0 and becomes available after research.
