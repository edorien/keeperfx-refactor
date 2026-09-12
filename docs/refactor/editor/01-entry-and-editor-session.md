# Phase 1 — the `kfx_editor` library, menu entry, and the editor session

Status: **not started.** Prereq for every other phase. Depends on nothing except the ImGui
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
