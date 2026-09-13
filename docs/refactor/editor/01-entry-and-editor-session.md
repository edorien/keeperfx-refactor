# Phase 1 — the `kfx_editor` library, menu entry, and the editor session

Status: **implemented.** `--strict` layering clean, both build variants (`keeperfx`/
`keeperfx_hvlog`) compile. All 7 "what must exist after phase 1" items (§7) are in place.
Not yet done: the ftest/Catch2 coverage §8 calls for (no automated test exercises this phase
yet -- verify live before trusting it). Three deliberate deviations from this doc's original
wording, decided during implementation:

- **`EditorCallbacks::request_open`** is called from `kfx_apploop`'s `case FeSt_START_EDITOR:`
  (`game_session_loop.cpp`), not from the frontend's main-menu button. Reasoning: the browser's
  New/Open choice has to reach `kfx_apploop`'s post-menu `switch` (which starts the sim) without
  an upward include, so it can only travel as plain frontend-owned data
  (`editor_pending_lvnum`/`editor_pending_is_new`/`editor_pending_new_map_*`, `frontend.h`) --
  mirroring `kfx_frontend_state.save_game_slot`'s role for `FeSt_LOAD_GAME`. The callback's real
  job turned out to be the one genuine lower-can't-call-higher gap: telling `kfx_editor` the sim
  has finished loading and paused, which only `kfx_apploop` (immediately after
  `coroutine_process()`) knows.
- **Open Map** lists `campaign.freeplay_levels[]` (kfx_frontend already reads `campaign`
  directly -- kfx_config ranks below it) instead of a dedicated `enumerate_maps` callback.
  Thumbnails (`land_preview_build_minimap`, F14) are still phase-3 polish.
- **New UI text is a literal English string**, not routed through `frontend_button_info[]`/
  `GUIStr_*`. Those net-new captions have no translation-table entry yet -- without a
  `pkg-languages` regen they'd render as `"untranslated <N>"` at runtime. Follow-up if/when the
  editor is player-facing rather than a dev tool.

Live-tested and fixed twice:

1. The main menu's "Tools" button opens a small modal (Editor / Back, `FeOpenModal`/
   `FeBeginModal` -- same pattern as the settings screen's confirm dialog) instead of jumping
   straight to the editor browser, and the Open Map list in `frontgui_editorbrowser_frame()`
   uses `FeBeginListBox`/`FeListRow` -- every other fixed-row select list in this screen file
   uses that inside an `AlwaysAutoResize` window; `FeBeginScrollArea` (a `BeginChild` wrapper)
   inside that same window flag was the cause of a black, unresponsive screen on first entering
   the editor browser.
2. New Map crashed twice more before landing (both `SIGABRT`, `keeperfx.log` backtraces):
   - First: inside `ceiling_init()` itself, called before any slab was painted -- moving the
     paint loop before that tail (mirroring `load_level_file()`'s order) didn't fully fix it.
   - Second, after that reorder: inside `ceiling_partially_recompute_heights()`, reached via
     `place_slab_type_on_map()` -> `update_blocks_around_slab()` -> `update_blocks_in_area()`
     while painting the *first* slabs. Root cause: `place_slab_type_on_map()` (the terrain
     cheat/editor brush's live-mutation entry point) is designed for editing one slab on an
     *already-finalized* map -- calling it incrementally while the map has no ceiling/
     navigation data at all yet exercises a code path the engine never otherwise hits (every
     real level already has `ceiling_init()`+navigation set up once, from disk data, before
     any live edit ever runs).
   - **Fix**: `create_blank_map()` no longer calls `place_slab_type_on_map()` (or anything else
     in that live-mutation family) at all. It now mirrors `load_map_slab_file()` +
     `load_level_file()`'s own structure exactly: pass 1 assigns every slab's kind + owner as
     plain data (`slb->kind = ...`, same as `load_map_slab_file()`'s own loop); pass 2 derives
     columns from those now-final kinds via `place_single_slab_type_on_map()` (the doc's own
     gap analysis already named this as the safe "rebuild columns from slabset config"
     primitive -- it does no navigation/ceiling work itself); then the same
     `initialise_map_collides/health/extra_slab_info` + `reinitialise_map_rooms/ceiling_init/
     init_keys` tail `load_level_file()` runs once, over a fully-populated map, exactly as a
     real load does. Static analysis could not pin the exact faulting instruction inside
     `ceiling_partially_recompute_heights()` with confidence -- this fix sidesteps the whole
     code path rather than patching that function, which is the more robust guarantee.

**Confirmed live**: New Map now reaches an interactive, paused session with the phase-2 toolbox
visible and responsive (screenshot + clean `keeperfx.log`, no abort). Per user request, New
Map's blank canvas is **entirely `SlbT_ROCK`** ("HARD" in the palette) now, not
earth-with-a-rock-border -- one uniform slab kind, matching the original editor's "nothing
pre-dug" starting state and removing the border/interior distinction from `create_blank_map()`
entirely. Map size is already effectively locked to 85x85 (`editor_pending_new_map_w/h`,
`frontgui_screens.cpp`'s New Map handler) -- there's no size-picker UI yet for it to vary.

**Root cause found for the `delete_attached_things_on_slab: Request of invalid thing` spam +
black viewport + unresponsive session** (all one bug, user correctly suspected they were
linked): `init_level()`'s early `clear_game()` clears the map/slab/column arrays
(`clear_mapmap()`/`clear_slabs()`/`clear_columns()`) sized off whatever
`kfx_sim_state.map_subtiles_x`/`map_tiles_x`/`y` held **at that point** -- the previously
loaded level's dimensions, or 0 if none has loaded yet this process (exactly the reported
repro: launch straight to Tools -> Editor -> New Map, no other level loaded first). Nothing
else in the load path re-clears for the *real* size once `create_blank_map()` calls
`set_map_size()` -- so every subtile/slab/column beyond whatever tiny rectangle
`clear_game()` actually cleared kept whatever was already sitting in that storage (session-
lifetime, not per-level). That's what the "invalid thing" spam was: stale `mapwho`/attached-
thing references outside the cleared rectangle, not random corruption -- and the same stale
state is the leading suspect for the black viewport (garbage column/ceiling data outside the
cleared area) and the reported total unresponsiveness. **Fix**: `create_blank_map()` now calls
`clear_mapmap()`/`clear_slabs()`/`clear_columns()` again itself, immediately after
`set_map_size()`, so the arrays are genuinely zeroed for the map's actual final size before
anything reads or writes them.

**Confirmed live**: this fully fixed the log spam (clean `keeperfx.log`, none of the earlier
"invalid thing"/"can't find door" errors) and the total freeze. **The 3D viewport is still
black, though** -- a separate issue. User's own theory, and it matches the evidence:
`PckA_CheatRevealMap` (fog-of-war reveal) wasn't actually applying. `editor_open()` was
sending it as a packet (`set_players_packet_action`) -- the same mechanism the cheat menu
uses, structurally sound, but with a timing dependency on `process_packets()` running before
the session's first rendered frame. Switched `editor_open()` to call `reveal_whole_map(player)`
and the other cheat effects (`make_all_creatures_free()`, `make_all_rooms_free()`,
`make_all_powers_cost_free()`, `make_available_all_researchable_rooms/powers()`,
`make_available_all_doors/traps()`) **directly** instead -- all declared in `kfx_config`
headers, freely callable from `kfx_editor` -- removing the timing dependency entirely and
matching `07-investigation-findings.md`'s own D2 "single-player direct-call exception". **Live-
tested: no change** -- reveal was not the (or not the only) cause.

Found the actual cause next: `init_player_cameras()` (`engine_camera.c`), called during the
normal `init_players_local_game()` path every session runs (editor or not), points the
isometric and front-view cameras at `get_player_soul_container(player->id_number)`'s
position -- the Dungeon Heart Thing. On a heart-less New Map that lookup returns the invalid-
thing sentinel, so both cameras end up sitting at world `(0,0,z)` -- the map's corner, not
anywhere over the map at all. Every real level has a Heart before this code path ever runs, so
nothing in the engine previously needed a fallback here. **Fix**: `editor_open()` now
re-centers `player->cameras[CamIV_Isometric]` and `[CamIV_FrontView]`'s `mappos.x/y` on the
map's own center (`subtile_coord_center(map_subtiles_x/2, map_subtiles_y/2)` -- the same
fallback position `init_player_start()` already uses for `dungeon->mappos` when there's no
heart) immediately after the reveal/free calls. Not yet live-confirmed.

Separately, `get_nearest_valid_position_for_creature_at`/`place_thing_in_mapwho` failures for a
per-player "FLOATING_SPIRIT" during player setup are confirmed linked to the missing Heart, not
a distinct bug: `init_player_start()` (`thing_list.c:1224`) has a comment stating
`dungeon->mappos` defaults to map center "needed for Floating Spirit" when there's no heart --
and since New Map's canvas is 100% solid rock, there's nowhere valid near that point to place
one. This is the intro "spirit flies to your heart" cutscene sequence; not suppressed yet
(deprioritized behind the viewport/freeze investigation per the user's explicit ordering) --
not crashing, just log noise, though it will need suppressing for a real editor session once
this is revisited.

**Diagnostic pivot, user's idea**: rather than keep chasing the black viewport through New
Map's blank-canvas construction, open an existing, known-good level (`map00001`) through the
*same* editor session path (`startup_local_game_for_editor`) to isolate whether the bug is
specific to blank-map construction or something broader in the editor session itself. Added
`campaign.single_levels[]` (not just `freeplay_levels[]`) to `frontgui_editorbrowser_frame()`'s
Open Map list so a single-player campaign level can be opened this way at all. **Result: it
rendered correctly**, including the normal spirit-to-heart intro cutscene -- confirming the
black viewport is specific to New Map's blank canvas, not the editor session/pause/rendering
plumbing in general. (Caught before it could taint that test: the camera-recenter fix above was
originally unconditional, which would have wrongly overridden a real level's already-correct
heart-centered camera too -- gated it on `thing_is_invalid(get_player_soul_container(...))` so
it only fires when there's genuinely no Heart.)

That same real-level test surfaced two more bugs, both **fixed**, unrelated to the viewport:

1. **"Each unpause allows creatures to move."** `game_session_loop.cpp`'s per-turn gate
   (`update()`) calls `process_packets()` *then* checks `GOF_Paused` in the same call --
   a queued `PckA_TogglePause` (the player's own pause key, unrelated to the editor) clears
   `GOF_Paused` before that same turn's gate check runs, letting one full turn execute before
   `editor_frame()`'s reassertion catches it again on the next rendered frame. Fixed: the gate
   now also checks `kfx_sim_state.simulation_suspended` directly
   (`!flag_is_set(...,GOF_Paused) && !kfx_sim_state.simulation_suspended`) --
   `simulation_suspended` is untouched by `PckA_TogglePause`, so it can't race the same way.
2. **Both the normal in-game pause menu and the editor's own Esc menu opened at once.**
   `front_input.c`'s `get_options_menu_inputs()` (the normal `GMnu_OPTIONS` pause launcher)
   reads raw `lbKeyOn[]`/`is_key_pressed()`, entirely independent of the ImGui key-event
   tracking `editor_frame()` used for its own Escape check -- both systems saw the same
   keypress and each opened their own menu. Fixed by moving the editor menu's toggle to **F10**
   instead of Escape (the base game has no default binding on it) -- kfx_frontend can't be
   taught to skip its own handler while the editor is active without a new callback (it's
   ranked below `kfx_editor`), so sidestepping the key conflict was the low-risk fix for now.
   Temporary: D6 (`02-editing-toolbox.md` §3) makes every editor shortcut a definable
   `Gkey_Editor*` key; F10 is a placeholder default until that lands.

**The actual unifying root cause, found after fix 1 above made things worse in a revealing
way**: re-testing `map00001` with the `simulation_suspended` gate in place made it hang
completely, never finishing its normal intro. Root cause: `game_loop()`
(`game_session_loop.cpp`, right after `wait_at_frontend()` hands control back) unconditionally
calls `set_player_instance(player, PI_HeartZoom, 0)` for every local, non-computer player when
a level starts -- the classic "camera flies to your Dungeon Heart" opening shot. That instance
needs several real turns of `update()` to complete. Once `simulation_suspended` also gated that
`update()` (fix 1), no turns ever ran again -- the session could never leave the intro. **This
also explains New Map's black viewport all along**: with no Heart, `PI_HeartZoom`'s target
lookup is degenerate on top of the turns never completing -- the camera-recenter fix from the
previous round couldn't matter because the stuck HeartZoom instance's own camera handling was
what the player actually saw, not the plain `mappos` fields read at session start.

**Fix**: skip the whole `PI_HeartZoom` sequence for editor sessions, using the same feature
flag `-skipheartzoom` already exists for (`Ft_SkipHeartZoom`, `config_keeperfx.h`) --
`editor_open()` now saves the current `get_skip_heart_zoom_feature()` value, forces it on, and
`editor_close()` restores whatever it was before. No player-camera cutscene is meaningful in an
editing context anyway, heart or no heart, so this is the right fix rather than trying to let a
few turns of the intro slip through.

**Confirmed live: this fixed both.** New Map and Open Map both render correctly now, toolbox
interactive and responsive, no freeze.

One more bug surfaced by that same test: **quitting via the normal in-game pause menu** (not
the editor's own "Exit to Main Menu") **left the toolbox rendering over the main menu
afterward.** Cause: `editor_close()` was the *only* path that ever cleared `s_editor_active` --
any other way of ending the session (the base game's own Quit option, a level lost/won, ...)
skipped it entirely, and `editor_frame()` (called unconditionally every frame from main.cpp's
wrapper, main menu included) kept submitting the toolbox forever after. Fixed with a safety net
rather than chasing every individual exit path: `keeper_gameplay_loop()`
(`game_session_loop.cpp`) already unconditionally resets `kfx_sim_state.game_kind` to
`GKind_Unset` once its loop ends, by every exit route -- `editor_frame()` now checks for
`game_kind != GKind_LocalGame` at the top and self-cleans (`editor_deactivate()`, an internal
helper `editor_close()` also now calls, minus the `PckA_QuitToMainMenu` send which would be
meaningless once the session's already over).

Both build variants compile clean, `check_layering.py --strict` passes.

**Confirmed live**: the persisting-toolbox-after-quit fix works.

**But the toolbox tools themselves turned out not to work at all** -- selecting a terrain
palette entry, a player, a creature, etc. all reach `process_packets()` fine (it has no pause
gate), but clicking in the dungeon view to actually place/build did nothing. Root cause, and
the real design mistake in this doc's original §3: `GOF_Paused` does far more than gate the
per-turn simulation update. `get_packet_control_mouse_clicks()` (`front_input.c`) hard-returns
without generating *any* `PCtr_LBtn*`/`RBtr_RBtn*` packet control while `GOF_Paused` is set --
and that control is what a world click gets turned into a `PckA_CheatPlaceTerrain`-style packet
*from*. `GOF_Paused` means "no player interaction with the world", not just "no simulation
ticks" -- exactly the opposite of what an editor session needs (frozen sim, *live* interaction).
User's diagnosis: "might need special editor flag so it stops simulation, but still allows
changes in game world" -- correct, and `simulation_suspended` already was that flag; the bug
was forcing `GOF_Paused` alongside it out of an unexamined assumption inherited from this doc's
own original wording.

**Fix**: stop setting `GOF_Paused` for editor sessions entirely. `simulation_suspended` alone
(checked directly in `game_session_loop.cpp`'s per-turn gate, per the earlier fix) already fully
freezes creature AI/economy/game-turn advance -- `editor_open()`/`editor_frame()`
(`editor_session.cpp`) and `startup_local_game_for_editor()` (`main_game.c`) no longer touch
`GOF_Paused` at all, only `kfx_sim_state.simulation_suspended`. Known cosmetic side effect: the
"Paused" banner (which reads `GOF_Paused` to decide whether to show) no longer appears during
editor sessions -- an "Editor Mode" indicator could replace it later if wanted, not a
functional concern. Not yet live-confirmed.

Prereq for every other phase. Depends on nothing except the ImGui
frontend seam already in place (`frontgui_screens.cpp`).

Goal: stand up `src/kfx_editor/` as an internal library, and from the main menu open an editor
that loads a map (existing or blank) into a **running but frozen** sim, with the local player
in a build-anything state, and a clean way back out (exit / playtest / resume editing).

---

## 0. Scaffold `src/kfx_editor/`

Per [`00-overview.md`](00-overview.md) §5 the editor is its own `OBJECT` library.

- **Directory + CMake:** `src/kfx_editor/{src,include}/` + `src/kfx_editor/CMakeLists.txt`
  copied from a sibling `kfx_*` lib (glob-based, compiled twice std/hvlog). Add it to the
  top-level `src/CMakeLists.txt` library list and to both binary link lines.
- **Layering rank ([`07`](07-investigation-findings.md) F11):** insert `"kfx_editor"` into
  `scripts/check_layering.py`'s `LIBRARY_ORDER` **immediately before `"app_entry"`** — i.e.
  *above* `kfx_apploop`. It becomes the highest-ranked internal library: `#include`s anything
  below; **nothing below `#include`s it**; the only inbound edge is `main.cpp` (app_entry,
  which already includes every layer). Run `--strict` after wiring.
- **Public surface** (`src/kfx_editor/include/kfx_editor.h`):
  `void editor_open(LevelNumber target_or_scratch, bool is_new);`
  `void editor_close(void);`
  `bool editor_is_active(void);`
  `void editor_frame(void);`  ← ImGui submission + per-frame editor logic
  `void editor_notify_playtest_end(void);`
- **Per-frame hook ([`07`](07-investigation-findings.md) F11/F16):** the in-game ImGui
  submission is already one registered callback — `main.cpp:1301`
  `RendererSetImGuiFrameCallback(&FrontendImGuiFrame)` — invoked every present, in-game
  included (`RendererSoftware.cpp:185`). `main.cpp` changes it to a wrapper:
  `static void app_imgui_frame(){ FrontendImGuiFrame(); editor_frame(); }`. `editor_frame()`
  no-ops unless `editor_is_active()`; the `GOF_Paused` re-assert, tool/cursor handling and
  packet dispatch all run there. `kfx_apploop`'s only editor line is the `case FeSt_START_EDITOR:`
  in §2 (calls a `kfx_game` fn, no `#include kfx_editor`).
- **`EditorCallbacks`** (`kfx_config/include/editor_callbacks.h`, no-op default in a `.c`,
  `set_editor_callbacks()`): the *frontend → editor* request path —
  `void (*request_open)(LevelNumber, bool is_new);` plus what the `FeSt_EDITOR` browser needs
  (enumerate writable maps). Wired in `main.cpp::setup_game()`. This is the **only** cross-layer
  plumbing.

Everything else in this doc is implemented *inside `src/kfx_editor/`* unless a file path says
otherwise.

## 1. Main menu: Tools → Editor

**Decision: the editor entry is ImGui-menu-only.** The classic sprite menu
(`-classicmenu`) is legacy; adding a `GMnu_*` sub-menu there means bumping
`MENU_LIST_ITEMS_COUNT` (`frontend.h:36`, currently 52) and hand-laying sprite buttons for a
brand-new feature. Not worth it. The classic menu gets **no Editor button** (or a disabled one
captioned "Editor — use the modern menu"); the editor requires the ImGui menu, same as it
requires the ImGui GUI toolkit anyway.

- **ImGui menu** — `frontgui_mainmenu_frame()` in
  [`frontgui_screens.cpp:1384`](../../../src/kfx_frontend/src/frontgui_screens.cpp). Add a
  **Tools** `FeButton` row (opens a small child window / submenu) containing **Editor**, using
  the same `FeCenterNextItem(btn_size.x)` pattern as its neighbours. On Editor click:
  `request_frontend_state(FeSt_EDITOR)` (deferred-transition helper — never
  `frontend_set_state()` from inside a live ImGui window).
- New caption `FEBtn_MnuEditor` / `FEBtn_MnuTools` in `frontend_button_info[]` + the caption
  string table (see `docs/refactor/gui/01-caption-table-rename.md`).

**Decision:** one item under Tools for now (Editor), but keep the Tools submenu so future tools
(map-pack manager, packet-demo browser, per-map config editor) have a home — matches the
user's "Tools → Editor" phrasing.

## 2. `FeSt_EDITOR` + `FeSt_START_EDITOR` frontend states

Two new `enum FrontendMenuStates` values before `FeSt_FONT_TEST = 255`
([`frontend.h:151`](../../../src/kfx_frontend/include/frontend.h)) — mirroring the
`FeSt_LEVEL_SELECT` → `FeSt_START_KPRLEVEL` pair ([`07`](07-investigation-findings.md) F16):

**`FeSt_EDITOR`** — the **ImGui-only** project browser:
- **New Map** → size (default 85×85, any size — F12), texture set, name, player count →
  stash `{scratch_num, is_new=true}` → `request_frontend_state(FeSt_START_EDITOR)`.
- **Open Map** → `editor_callbacks->enumerate_maps()` → list with `land_preview_build_minimap`
  thumbnails (F14) → stash `{lvnum, is_new=false}` → `FeSt_START_EDITOR`.
- **Back** → `FeSt_MAIN_MENU`.

**`FeSt_START_EDITOR`** — transient: `frontend_update()` adds it to the `*finish_menu = 1`
group ([`frontend.cpp:3927`](../../../src/kfx_frontend/src/frontend.cpp)); the post-menu
`switch (prev_state)` in [`game_session_loop.cpp:864`](../../../src/kfx_apploop/src/game_session_loop.cpp)
gets `case FeSt_START_EDITOR: startup_local_game_for_editor(&loop, target);` — **the one
editor-related line in `kfx_apploop`** (it calls a `kfx_game` function; no `#include
kfx_editor`).

Wire `FeSt_EDITOR` into (frontend glue only, no editor logic):
- the `FeSt_*` `switch` sites in `frontend.cpp` (state→name ~`:3100`, backdrop, dispatch
  ~`:2889`/`:3049`/`:3368`/`:3741`).
- `frontgui_screens.cpp`: `frontgui_editorbrowser_frame()` + register in
  `frontend_imgui_screen_active()` and the `case` list ~[`:108`](../../../src/kfx_frontend/src/frontgui_screens.cpp).
- `front_input.c` — Esc on the browser → `FeSt_MAIN_MENU`.
- No `GMnu_*` / `menu_list[]` / `MENU_LIST_ITEMS_COUNT` change — ImGui-only (§1).

The **in-session editor GUI** (toolbox, palettes, property panels) is *not* a frontend state
and *not* frontend code — a level is loaded and the game loop is running. It's an ImGui panel
set **owned by `kfx_editor`**, submitted from `editor_frame()`, built with `kfx_frontend`'s
`frontgui_widgets` wrappers, gated on `editor_is_active()`. Phase 2 owns it.

## 3. `simulation_suspended` — force-and-lock `GOF_Paused`

**Investigation ([`07`](07-investigation-findings.md) F1) resolved this: the frozen sim
already exists.** [`game_session_loop.cpp:139`](../../../src/kfx_apploop/src/game_session_loop.cpp)
guards the *entire* per-turn block —
`update_things` (creature AI, effects), `process_rooms`, `process_dungeons` (economy),
`update_research`, `update_manufacturing`, `event_process_events`, `process_level_script`,
`lua_on_game_tick`, `process_computer_players2`, `process_players`, `process_action_points`,
`play_gameturn++` — behind `!flag_is_set(kfx_sim_state.operation_flags, GOF_Paused)`.
`process_packets()` runs *before* that guard (so editor edits still apply), and rendering is
independent of `update()` (so the frozen world still draws).

So there is **no subsystem-by-subsystem audit and no scattered new guards**. Instead:

- Add `bool simulation_suspended` to `kfx_sim_state`
  ([`kfx_sim_state.h`](../../../src/kfx_sim/include/kfx_sim_state.h)) — blob-safe (survives the
  save/resync/reset `memcpy`s). Deliberately **not** `editor_mode`: it's a neutral "force the
  pause and don't let anything lift it" flag, reusable (demo scrubbing, replay inspector).
- Where it's set (in `editor_open`, cleared in `editor_close`): it just *sets* `GOF_Paused`.
- `editor_frame()` **re-asserts `GOF_Paused` every frame** while `simulation_suspended`, so the
  player's pause key, the pause menu, or a stray `PckA_TogglePause` can't un-freeze the editor.
  Don't round-trip through `set_packet_pause_toggle()` ([`packets_misc.c:347`](../../../src/kfx_net/src/packets_misc.c),
  has a cooldown + net broadcast) — set the flag directly.
- One editor-menu affordance — **"Preview motion"** — clears `GOF_Paused` while held/toggled,
  for checking creature idles, lava, particle FX.

| Subsystem | Frozen by `GOF_Paused`? | Notes |
|---|---|---|
| Game clock (`play_gameturn++`) | ✅ | |
| Creature AI / states / jobs (`update_things`) | ✅ | placed creatures stand still |
| Portal generation, creature pool | ✅ | |
| Economy: upkeep, wage, research, manufacture | ✅ | |
| Computer players (`process_computer_players2`) | ✅ | + editor assigns none (F5) |
| Win/lose + script `IF` (`process_level_script`, `lua_on_game_tick`) | ✅ | runs only in playtest |
| Action points (`process_action_points`) | ✅ | |
| Effect generators, static-light updates, animated textures | ✅ (side effect) | v1 accepts static; "Preview motion" or an `editor_frame()` re-pump (phase 5) restores them |
| `process_packets` (→ editor edits, camera) | ❌ runs | this is the point |
| Rendering, input | ❌ runs | separate from `update()` |
| Fog of war | n/a | editor reveals whole map for the editor player |

**Editor player.** The local player (`my_player_number`, typically 0). **No "enable cheats"
step** — [`07`](07-investigation-findings.md) F10: `PckA_CheatEnter` is a no-op and no
`cheat_mode` flag exists, so `PckA_Cheat*` actions just work on receipt. `editor_open()`
sends: everything-free + all rooms/magic/traps/doors available (`PckA_CheatAllFree` /
`…AllRooms` / `…AllMagic` / `…AllDoors` / `…AllTraps`), reveal map (`PckA_CheatRevealMap`).
Their `work_state` is driven by the toolbox (phase 2).

## 4. Session bootstrap

The browser (§2) transitions to `FeSt_START_EDITOR`; the post-menu `switch` in
`game_session_loop.cpp` runs `startup_local_game_for_editor(&loop, target, suspend=true,
trim=true)`. Then `editor_frame()`'s first tick: reveal the whole map for the editor player
(`PckA_CheatRevealMap`), put it in the god/build state (§3, no cheat-enable step — F10), ask
`kfx_script` to *parse* the level script (not run it) for the phase-5 panels, and start
drawing the toolbox. `editor_is_active()` is true from that point.

**The one sanctioned new `kfx_game` function ([`07`](07-investigation-findings.md) F5):**
`startup_local_game_for_editor(context, lvnum, bool suspend, bool trim_post_init)` next to the
`startup_*_game` family =
- `init_level()` ([`main_game.c:377`](../../../src/kfx_game/src/main_game.c), currently
  `static` — expose or wrap) — reused **as-is**: full map + config + script load, one player.
  For **New Map**, `editor_new_map()` (§5) runs in place of `load_map_file()` (`kfx_editor`
  populates the arrays before calling this, or passes a "blank" flag).
- `setup_zombie_players()` (not `setup_computer_players()`) — no CPU keepers.
- `post_init_level()` — **trimmed when `trim_post_init`**: keep `init_traps` /
  `init_all_creature_states` / `init_keepers_map_exploration` (correct rendering); **drop**
  `lua_on_game_start` (arbitrary user Lua), `create_transferred_creatures_on_level`,
  generation-speed setup. Full when `!trim_post_init` (the Playtest path, §6).
- set `GOF_Paused` iff `suspend`.

It holds no editor logic — just the "one player, zombie keepers, optionally-trimmed post-init,
optionally-paused" startup variant. It is coroutine-staged (`CoroutineLoop *context`) like
`startup_network_game` — the `case FeSt_START_EDITOR:` (§2) adds its steps to the same `loop`.
`init_level()` sets `FTF_LevelLoaded` under `FUNCTESTING` — editor ftests get a ready signal.

## 5. New / blank map — `editor_new_map()`

The loader has an "empty map" branch
([`lvl_filesdk1.c:1495`](../../../src/kfx_sim/src/lvl_filesdk1.c)) — an error fallback:
`init_whole_blocks()` + `load_slab_file()` + `init_columns()`, texture 0, no heart,
returns `false`. `kfx_editor`'s `editor_new_map(w, h, texture_set, name)` does the real thing
by writing directly into `kfx_sim`'s map/slab/ownership arrays (it's above `kfx_sim`):

- `set_map_size(w, h)` ([`07`](07-investigation-findings.md) F12 — any size, not just 85×85;
  it rebuilds the `around_slab` neighbour tables), then all slabs `SlbT_EARTH` with a rock
  border (ADiKtEd/DK convention: never edit the outermost ring), ownership all neutral, no
  things/lights/APs, one default ambient light level, the chosen texture set, map-info
  defaults (name from the New dialog);
- then reuse the loader's init tail (`init_whole_blocks` → `init_columns` → `ceiling_init` → …)
  — extract it into a `finalize_loaded_map()` helper in `kfx_sim` if it isn't already callable
  standalone.
- No heart is placed automatically — the editor player places it with the terrain tool
  (heart is an atomic 3×3 room, same as the original). Verification (phase 3) warns on save
  if player 0 has no heart.
- The map lives at a **scratch level number** in a writable dir until first Save assigns it a
  real slot/name (mirrors the original's "assign a slot on save"). Save target resolved
  ([`07`](07-investigation-findings.md) F4): a `.lof` into `FGrp_CmpgLvls`, default bucket an
  "Editor Maps" mappack; scratch-number allocation detail in phase 3.

## 6. Leaving the editor

The in-game editor GUI has an **Editor menu** (Esc opens it, like the pause menu):

- **Save** / **Save As** / **New** / **Open** → phase 3 dialogs (in-session, no full frontend
  round-trip where avoidable).
- **Playtest** ([`07`](07-investigation-findings.md) F19 — there is no in-memory level-reset
  path, so): `editor_save_map()` to a scratch slot → `startup_local_game_for_editor(&loop,
  scratch, /*suspend=*/false, /*trim_post_init=*/false)` (same startup fn, but runs the full
  `post_init_level` incl. script setup + `lua_on_game_start`, and does **not** set
  `GOF_Paused`). **"Return to editor"** = `editor_open()` on the same scratch slot (reloads the
  pre-playtest state). `editor_notify_playtest_end()` is the win/lose/quit-from-playtest hook.
  Consequence (D1): an unsaved edit is auto-committed to the scratch slot before playtest —
  acceptable, arguably desirable.
- **Exit to Main Menu** → prompt if unsaved (`verify_map` + dirty flag), then
  `PckA_QuitToMainMenu` and `editor_close()` (clears `simulation_suspended`).

**Dirty tracking:** `kfx_editor` owns a dirty flag set by any editor action, cleared on save.

## 7. What must exist after phase 1

1. `src/kfx_editor/` library — CMake, `LIBRARY_ORDER` slot before `app_entry`, `kfx_editor.h`,
   `EditorCallbacks`, `editor_frame()` in `main.cpp`'s ImGui-frame wrapper, `--strict` clean.
2. Tools→Editor in the **ImGui menu only** (§1); `FeSt_EDITOR` browser (New / Open / Back)
   calling `EditorCallbacks`.
3. `kfx_sim_state.simulation_suspended` + `simulation_is_suspended()`, blob-safe; it forces
   `GOF_Paused` and `editor_frame()` re-asserts it (§3 — no per-subsystem audit).
4. `startup_local_game_for_editor()` in `kfx_game` (§4): `init_level` + `setup_zombie_players`
   + trimmed post-init + `GOF_Paused`.
5. `editor_open()` loading an existing map into the frozen sim with a god-state editor player
   and full map reveal.
6. `editor_new_map()` + New Map flow producing an editable blank.
7. Esc → Editor menu (owned by `kfx_editor`) with Save (stub → phase 3) / Playtest (stub) /
   Exit, + dirty flag.

**Explicitly deferred to later phases:** any actual tool palette (2), any Save that writes
files (3), view toggles (4), light/AP/script panels (5).

## 8. Tests

- ftest `editor_session_enter`: `editor_open()` on a stock campaign map, assert
  `simulation_is_suspended()`, `editor_is_active()` and `GOF_Paused` set; run 10k frames and
  assert `play_gameturn` unchanged + a slab/thing/creature state checksum unchanged (R2);
  assert map fully revealed; toggle "Preview motion" and assert turns advance; assert
  `editor_close()` restores normal mode + clears `GOF_Paused`.
- ftest `editor_blank_map`: New 85×85 → assert slab grid is earth+rock border, no things,
  enter session, place a heart via `place.slab`/packet, exit.
- Catch2: `editor_new_map()` dimensions/border/ownership; `simulation_suspended` survives a
  `kfx_sim_state` blob copy; `check_layering.py --strict` passes with `kfx_editor` present.
