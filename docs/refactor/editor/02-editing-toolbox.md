# Phase 2 — the editing toolbox

Status: **first slice implemented** (`src/kfx_editor/src/editor_toolbox.cpp` +
`editor_toolbox.h`), covering §5 items 1-4 only: the toolbox panel itself
(tool strip + picker + bottom bar, shown whenever `editor_is_active()`),
terrain/room paint with a palette generated from `kfx_config_state.conf.slab_conf`,
the player (P0-3/Neutral) + experience (1-10) bottom bar, and creature/hero/digger
placement with a model-grid picker from `crtr_conf`. Confirms the doc's own §1 claim
in practice -- every tool here is *only* `set_players_packet_action()` calls onto
already-live `PckA_SetPlyrState`/`PckA_CheatSwitchTerrain`/`PckA_CheatSwitchPlayer`/
`PckA_CheatSwitchCreature`/`PckA_CheatSwitchHero`/`PckA_CheatSwitchExperience` handlers
(`packets_cheats.c`) -- all of which turned out to be **direct setters** keyed off
`pckt->actn_par1`, not the cycle-only inputs §1 assumed; the classic cheat menu just
happens to only ever call them with a precomputed next/prev value. No new packet verbs,
no new `kfx_sim`/`kfx_net` code needed for this slice.

**Selection feedback added after live testing**: none of the palette/bottom-bar picks had any
visual "selected" indication, which made a working click indistinguishable from a no-op --
found live when the user reported terrain placement "not working" via the toolbox. Root-caused
via `packets_cheats.c`: painting is genuinely a single click-driven `PckA_CheatPlaceTerrain`,
sent from `front_input.c` on `PCtr_LBtnRelease`, keyed off `ustate->cheatselection.
chosen_terrain_kind` -- exactly what `PckA_CheatSwitchTerrain` (the toolbox's palette click) sets.
No bug found in that path; the far more likely explanation is that a New Map canvas starts 100%
`SlbT_ROCK`, and painting *more* rock over rock (the engine default `chosen_terrain_kind` before
ever picking something else) is correct behaviour that's simply invisible. Added: a local shadow
of the current terrain/creature/hero/owner/level selection in `editor_toolbox.cpp`, used to
highlight the active pick (`FeListRow`'s own `selected` param for the palettes;
bracketed labels, e.g. `[P0]`, for the bottom bar's plain `FeButton`s, which have no built-in
highlight state). Note for testing: a `PckA_SetPlyrState` (tool switch) and a
`PckA_CheatSwitchTerrain`/`_Creature`/`_Hero` (palette pick) can't be sent in the same click --
only one action fits in the per-turn packet slot, so sending both back-to-back would silently
clobber the first with the second. This mirrors the classic cheat menu's own one-action-per-click
shape (`gf_change_player_state`), not a workaround.

**Confirmed live**: single-slab terrain placement works (the invisible-rock-on-rock theory was
it). **Drag placement of terrain does not** -- expected, not a bug: `PSt_PlaceTerrain`'s per-
frame cursor code (`packets_cheats.c`) always calls `create_box_roomspace(player->render_roomspace,
1, 1, slb_x, slb_y)` -- a fixed 1x1 box, not one that grows with a held drag -- and the actual
paint only fires once, on `PCtr_LBtnRelease`. Multi-tile drag painting is exactly what §2.2
("Marking (rectangular select) + area ops") is for -- reusing the roomspace drag machinery this
single-tile tool doesn't -- and hasn't been built yet. User's own read on this
("likely not enabled for terrain, as was originally done for room placement") matches: room
placement's drag support is the existing machinery §2.2 plans to extend to terrain/marking
generally, not something `PSt_PlaceTerrain` itself already has.

**Drag/multi-tile terrain painting explicitly tracked as the next step (user's ask)**: §2.2
("Marking") is the right place for it -- reusing room placement's existing roomspace-drag
machinery for terrain too. Sequenced after creature placement is fixed (below), still ahead of
objects/traps/doors in this doc's own §5 ordering, since the user asked for it specifically.

**Creature placement: narrowed down, not yet root-caused.** The `keeperfx.log` from a real
attempt (BARBARIAN/BUG models, indices 19-22 and 24) shows `create_creature()` succeeding
cleanly -- `set_start_state_f` transitions each straight to `CreatureDoingNothing`, no
`ERRORLOG`/`ERRORDBG` anywhere near those lines. So the whole dispatch chain (`tag_cursor_
blocks_place_thing()` -> `PckA_CheatMakeCreature` -> `create_creature()`) genuinely works --
**the creature exists** (confirmed independently: visible on the minimap/overview immediately)
**but never renders in the 3D view.** Existing creatures already on the level (e.g. the level's
own starting Imps) render fine in the same session, so this isn't a blanket "nothing renders"
problem -- it's specific to things created *after* the editor session (and its
`simulation_suspended` freeze) is already active.

Leading theory: this is the same shape of bug as the `PI_HeartZoom` intro from
01-entry-and-editor-session.md -- something about a newly-created thing needs at least one real
`update()` pass to be fully wired up for rendering (the pre-existing Imps got that pass during
normal level startup, before the freeze ever engaged; anything created afterward never does).
Not confirmed -- `keeperfx.log` at the current `SYNCDBG` level doesn't have per-thing rendering
trace to pin this down further from logs alone.

**Added the "Preview Motion" affordance** 01-entry-and-editor-session.md §3 already planned (a
checkbox in the Esc/F10 editor menu, `editor_session.cpp`) -- while checked,
`simulation_suspended` is left off instead of being re-forced every frame, so a real turn
actually runs. **Confirmed live: toggling it on made the invisible creature appear.** But the
user immediately (and correctly) flagged this as unusable as the *only* fix: Preview Motion
unfreezes everything, including creature AI -- placed creatures belonging to different players
would start fighting each other the moment you toggle it, which is exactly the kind of thing an
editor session exists to prevent. Preview Motion stays as a real, deliberately-opt-in feature
(checking idles/lava/FX motion) -- it is not the placement fix.

**Root-caused and fixed properly**: `update_thing_interpolation()` (`thing_list.c`, called once
per thing per real game turn from inside `update_things_in_list()`) is what primes
`thing->previous_mappos` from its zeroed post-allocation state to the thing's actual position.
On a frozen sim, no turn ever runs, so anything created after the freeze engaged keeps
`previous_mappos == (0,0,0)` forever -- and the renderer, interpolating from that degenerate
baseline, never draws it, even though the thing exists in every other respect (immediately on
the minimap, no creation errors). Fix (`thing_creature.c`): `create_creature()` now primes
`previous_mappos` (and clears `TF1_Teleported`) itself, right after `mappos` is finalized --
exactly what `update_thing_interpolation()` would do on the thing's first real turn regardless
of whether the sim is frozen, so this is correct in normal (non-frozen) play too, not an
editor-only hack; it just closes a one-frame gap that's imperceptible outside a frozen session.
`create_owned_special_digger()` (which calls `create_creature()` internally, then re-corrects
`mappos`'s Z height afterward without re-syncing `previous_mappos` to match) got the same
one-line re-sync after its own correction.

**Confirmed live: Digger now places visibly; raw Creature/Hero placement (`PckA_CheatMakeCreature`)
still doesn't.** Found the actual difference between the two routes: `PckA_CheatMakeCreature`'s
handler (`packets_cheats.c`) never sets `pos.z.val` at all before calling `create_creature()` --
only `pos.x.val`/`pos.y.val`. `pos` is a shared local reused across many `case`s of the same
switch, so `z.val` came out as whatever an earlier case happened to leave on that stack slot --
undefined, and evidently not "close enough to the real floor height" to render, unlike
`create_owned_special_digger()`, which explicitly sets `z.val = 0` before its own
`create_creature()` call and then corrects it afterward via `get_thing_height_at()`. This is a
pre-existing engine bug (the classic cheat menu's "Make Creature" goes through the exact same
handler), not something the editor introduced -- just newly exposed by more deliberate/repeated
testing. **Fix**: `PckA_CheatMakeCreature`'s handler now follows the same two-step pattern
Digger already used -- explicit `pos.z.val = 0` before creation, then
`thing->mappos.z.val = get_thing_height_at(thing, &thing->mappos)` (and the matching
`previous_mappos` re-sync) right after. **Confirmed live -- both Creature and Hero placement
now work.**

**Drag/multi-tile terrain painting, first slice (§2.2)**: `PSt_PlaceTerrain`'s placement gate
in `packets_cheats.c` fired only on `PCtr_LBtnRelease` (one tile per click); now also fires on
`PCtr_LBtnHeld`, so holding the button and dragging paints every tile the cursor crosses, like
a normal paint-tool brush. Deliberately the simpler half of what §2.2 eventually wants (a
`PCtr_MapCoordsValid`-gated stream of individual placements as the cursor moves, not yet the
fuller "mark a rectangle, apply one value to the whole thing in a single batched/undoable
action" design that reuses room placement's roomspace-drag machinery) -- room placement's own
gold-cost/roomspace-cost tracking has no equivalent for raw terrain and wasn't pulled in.

**Confirmed live -- works, but user correctly distinguished it from room placement's own drag
behaviour** in the main game: room placement drags out a rectangular N×N region and commits the
whole region as one action on release (via the roomspace/`keeper_build_roomspace` machinery
above); what's implemented here paints each tile individually as the cursor crosses it during
the hold, which is a different feel (continuous brush vs. a defined rectangle). **Both are
wanted** -- tracked as a toggle to add in the toolbox once work reaches the Brush tool (§2.4)
near the end of this phase, not blocking anything now: a "Rectangle" mode reusing the roomspace-
drag rectangle math (without room placement's gold-cost logic) alongside the current continuous
"Brush" mode already built.

**Also observed**: Query (`PSt_QueryAll`) opens a classic (non-ImGui) popup. Expected, not a
new bug -- the creature-query screen hasn't been migrated to ImGui by the separate ingame-gui
project yet; the editor's Query tool just reaches an existing, still-legacy screen.

**Not started**: items 5-11 (objects/spellbooks/gold, traps/doors, marking+area-ops,
flood fill, brush grab-and-stamp, query inspector/eraser/eyedropper as real tools rather
than a bare work-state switch, the command journal/undo-redo) and the definable-keybinding
work in §3. No ftest/Catch2 coverage (§6) either. Depends on phase 1 (editor session +
`editor_is_active()`), which is done.

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

**Implemented.** `PSt_EditorFill` appended to `enum PlayerStates` (`config_players.h`) and
`PckA_EditorFloodFill` to `enum TbPacketAction` (`packet_data.h`) -- both appended after the
last existing entry, not inserted among the numbered ones, since these values are what
savegames/`.pck` replay files actually store. `packets_cheats.c`: the `PSt_EditorFill`
work-state case reuses the Terrain tool's own `chosen_terrain_kind`/`chosen_player` selection
(no new selection state needed, release-only so one fill per click) and sends
`PckA_EditorFloodFill`; its handler runs an iterative BFS (`editor_flood_fill_terrain()`, a
static helper -- not recursive, uses map-sized static queue/visited buffers so a large flood
can't stack-overflow) that floods 4-connected slabs of the *seed's own* original kind,
refusing rooms as a boundary, calling `place_slab_type_on_map()` per flooded tile (same
mutation path Terrain itself uses). Toolbox: a new "Fill" entry in the tool strip
(`editor_toolbox.cpp`) reuses `draw_terrain_picker()` verbatim. Not yet live-confirmed.

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
- **Terrain rectangle-region mode (user ask, phase 2 first-slice follow-up):** the Terrain
  tool's current LMB-drag paints continuously, tile-by-tile, as the cursor crosses each one
  (`packets_cheats.c`'s `PSt_PlaceTerrain`, `PCtr_LBtnHeld`-gated) -- distinct from, and wanted
  alongside, room placement's own drag behaviour of marking out a rectangular N×N region and
  committing the whole region as one action on release. Add a toolbox toggle here between
  "Brush" (current, continuous) and "Rectangle" (drags out a box via the same roomspace-drag
  rectangle math room placement uses, applies the chosen slab kind to the whole box on
  release) -- reuse the geometry, not room placement's gold-cost logic, which doesn't apply to
  raw terrain.

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

**First slice implemented** -- a generic model-grid picker + click-to-place, no property panel
(gold amount/spellbook power tweak, sub-square occupancy rules, treasure-room restriction) yet.
`PSt_EditorPlaceObject`/`PckA_EditorPlaceObject` appended to their enums (same "never renumber,
only append" rule as `PSt_EditorFill`/`PckA_EditorFloodFill`). **This tool's interaction shape
is genuinely different from every tool before it**, and is the template §2.7 (Traps/Doors) and
any later new-selection-state tool should follow:

- No `CheatSelection` field exists for "chosen object model" (F17), so unlike Terrain/Creature
  there's no server-side selection for `packets_cheats.c`'s per-work-state switch to read back
  when a click arrives. `PSt_EditorPlaceObject`'s case there does *nothing but* the usual
  cursor-highlight (`tag_cursor_blocks_place_thing`) -- it exists only so a stray click doesn't
  fall through to whatever tool ran before it.
- The picker (`editor_toolbox.cpp`) only ever updates a *local* `s_selected_object_model` --
  no packet sent on selection at all.
- Placement is triggered by **kfx_editor watching for the click itself**
  (`handle_object_placement_click()`, called once per frame from `editor_toolbox_frame()`
  whenever the Object tool is active): `ImGui::IsMouseClicked` gated on `!io.WantCaptureMouse`
  (so clicking the picker list itself doesn't also place an object), then sending
  `PckA_EditorPlaceObject` directly. kfx_editor can do this because it's the highest-ranked
  library and already includes `packet_data.h`/`player_data.h`; no new callback or cross-layer
  plumbing required.
- `PckA_EditorPlaceObject`'s handler (`packets_cheats.c`) calls `create_object()`
  (`thing_objects.c`) and applies the same `previous_mappos`-priming fix `create_creature()`
  already needed -- `create_object()` itself now primes it too (never did before), closing the
  same latent "invisible until a real turn runs" gap for objects.

**Confirmed live: nothing appeared** -- correctly suspected as a repeat of the creature Z-height
bug. It partly was: the handler set `pos.z.val = 0` as a placeholder before `create_object()`
(matching `create_owned_special_digger()`'s own first step) but never corrected it afterward via
`get_thing_height_at()`. Fixed. **Retested live: still nothing appeared.** Added unconditional
`JUSTMSG` diagnostics to both `handle_object_placement_click()` and the packet handler; the log
showed every click getting past the `!io.WantCaptureMouse` guard but then `MapCoordsValid=0
pos=(0,0)` on every single attempt, and the packet handler's own log line never appeared at all
-- the packet was never sent.

**Root cause:** the original design's assumption above -- that `get_local_packet()`'s
`pos_x`/`pos_y`/`PCtr_MapCoordsValid` are "already kept current every frame ... regardless of
which work state is active" -- was wrong for *when kfx_editor reads them*. Those fields are
per-turn scratch state: `get_dungeon_control_nonaction_inputs()` populates them once during
`input()`, but `input()` runs once per logic turn (gated by `use_delta_time()`/
`process_turn_time` in `game_session_loop()`), and `exchange_packets()` (called immediately
after `input()`) resets the local packet for the next turn. `handle_object_placement_click()`
runs later in the same frame, from the ImGui render phase (`editor_toolbox_frame()`, invoked
from `gameplay_loop_draw()`) -- by then `get_local_packet()` is already the *next* turn's blank
packet. Terrain/Fill/Creature never hit this because their dispatch lives *inside*
`packets_cheats.c`'s per-work-state switch, which runs from within `input()` itself, before the
packet is reset.

**Fixed:** `handle_object_placement_click()` no longer reads the packet's position fields at
all. It recomputes the world position itself, at click time, with the same `screen_to_map()`
(`engine_redraw.h`, `kfx_render`) the input path uses internally, against the current mouse
position (`GetMouseX()`/`GetMouseY()`) and the local active camera (`get_local_active_camera()`).
The resolved position now travels explicitly in the packet: `PckA_EditorPlaceObject`'s params
were reordered to `actn_par1`/`actn_par2` (`int32_t`, x/y) + `actn_par3`/`actn_par4` (`int16_t`,
model/owner) -- the position needs the full 32-bit range (a max-size map's subtile position can
exceed `int16_t`), which the model/owner values never will. Not yet live-confirmed.

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

**First slice implemented** -- unlike Objects, this turned out to be the *simple* pattern
(same shape as Terrain/Creature/Fill), not the "kfx_editor watches for its own click" one:
`struct UserState` already has `chosen_trap_kind`/`chosen_door_kind` fields (used by the classic
workshop `PSt_PlaceTrap`/`PSt_PlaceDoor`, set via `set_player_state()`), so there's no F17 gap
here at all -- the picker can set them directly via new dedicated verbs
(`PckA_CheatSwitchTrap`/`PckA_CheatSwitchDoor`, same shape as `PckA_CheatSwitchTerrain`), and
`PSt_EditorPlaceTrap`/`PSt_EditorPlaceDoor`'s own `packets_cheats.c` dispatch (running from
within `input()`, same as every other tool except Objects) reads the packet's `pos_x`/`pos_y`
directly and sends `PckA_EditorPlaceTrap`/`PckA_EditorPlaceDoor` on release.

New work states rather than reusing `PSt_PlaceTrap`/`PSt_PlaceDoor` themselves, because those
work states' *dispatch* (`packets_input.c`) is wired to the resource-checked placement path
(`player_place_trap_at`/`player_place_door_at`, which refuse when the player has no
manufactured stock -- always true on a blank editor map). The editor verbs instead call
`player_place_trap_without_check_at()`/`player_place_door_without_check_at()` with `free=true`
directly, bypassing the workshop-inventory check entirely (same "free placement" shape
`PckA_EditorFloodFill` already established for terrain).

Owner is `ustate->cheatselection.chosen_player` (the bottom bar selector), not the packet's
literal `plyr_idx` -- consistent with every other editor tool letting you place on behalf of
any player regardless of who's actually driving the session.

Doors needed one genuine validity gate before this session's other "first slice, refinements
deferred" precedent could apply: `create_door()` indexes `doorst->slbkind[orient]` with
whatever `find_door_angle()` returns, and that's `-1` (an out-of-bounds read, not just a visual
glitch) unless the target slab is `SlbT_CLAIMED` and owned by the chosen owner. `PSt_EditorPlaceDoor`
checks `find_door_angle(stl_x, stl_y, chosen_player) != -1` itself before dispatching, rather
than reusing `tag_cursor_blocks_place_door()`'s own gate, which is keyed to the packet's actual
`plyr_idx` and also drags in fog-of-war/`is_my_player_number` visual-only gating that doesn't
fit an editor placing on behalf of an arbitrary owner. Traps have no equivalent crash risk, so
(matching Objects' own "bare click-to-place, occupancy rules deferred" first slice) no
placement-time validity check was added for them yet.

Also fixed `create_trap()` (`thing_traps.c`) and `create_door()` (`thing_doors.c`) to prime
`previous_mappos` on creation -- the same latent "invisible until a real turn runs" gap
`create_creature()`/`create_object()` already needed fixing for editor-created things.
`player_place_trap_without_check_at()`'s own z-height correction re-syncs it a second time
after, same two-step pattern as `create_owned_special_digger()`; doors need no second sync since
their z is a fixed constant, never corrected afterward.

**Deferred to a later pass**: Ctrl+LMB door-lock toggle (`PckA_EditorToggleDoorLock`), trap
occupancy validation ("not on an occupied square"), door occupancy/wall-adjacency-quality
checks beyond the bare orientation gate. Not yet live-confirmed.

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
