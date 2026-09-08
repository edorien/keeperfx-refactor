# Phase 1 — the pause menu and the unified Options window

Status: **Phase 1a landed 2026-09-06. Phase 1b landed 2026-09-06 — save/load slot lists +
video/autopilot fold. Phase 1 complete.** Depends on [01-seam-and-toggle.md](01-seam-and-toggle.md).

**Phase 1b — video / autopilot fold (landed):** the legacy in-game `video_menu` and `autopilot_menu`
sprite sub-menus were only ever reachable from the old 6-icon `options_menu`, which the ImGui
launcher replaced — so nothing had to be deleted; they just aren't opened on the ImGui path
(`-classicmenu` still gets them). Their controls now live in the shared Options window
(`frontgui_screens.cpp`), drawn only when `in_game`:
- **Graphics tab** (`draw_ingame_video_controls()`): View mode combo (iso / iso-level / front →
  `gui_video_rotate_mode`), Full-height walls checkbox (`gui_video_cluedo_mode` → `PckA_SetCluedo`),
  Gamma slider 0..4 (`PckA_SetGammaLevel`). Shadows + view distance were already real schema rows.
- **Game tab** (`draw_ingame_autopilot_controls()`): a collapsed `CollapsingHeader` with the 4
  computer-assist kinds as radio buttons — sets one `kfx_net_state.comp_player_*` flag locally then
  `gui_set_autopilot(nullptr)` → `PckA_SetComputerKind` (identical to the classic radio group).
Each reuses the exact legacy click handler (all of which ignore their `GuiButton*`), so the packet
+ `save_settings()` path is unchanged. `sound_menu` needed nothing — its 3 volume sliders were
already in the Sound tab since Phase 1a.

**Phase 1b — save/load (landed):** `GMnu_LOAD` / `GMnu_SAVE` migrated to ImGui windows in
`frontgui_ingame.cpp` (`loadmenu_frame()` / `savemenu_frame()`). Both are monopoly menus, so
`ingame_imgui_modal_active()`'s topmost-monopoly rule already hands them input once stacked on the
launcher. `turn_on_menu()` still runs the classic `init_{load,save}_menu` create_cb — catalogue
refresh (`load_game_save_catalogue`) + `PckA_UpdatePause(1,1)` — so the list is fresh and the game
paused by the first frame. `load` = `FeBeginListBox` of in-use `save_game_catalogue[]` entries,
row click → deferred `load_game(i)`. `save` = list of every catalogue slot (in-use → overwrite,
the one trailing free slot → "unused slot"), pick a row → editable name `FeTextInput` + Save
button → deferred `fill_game_catalogue_slot` + `save_game` + `output_message(SMsg_GameSaved)`.
Close: Cancel → `turn_off_menu` + `PckA_UpdatePause` back to the pre-open state; after-save →
`PckA_UpdatePause(player->paused_state_restore)` (mirrors `gui_save_game`); Esc → the classic
`turn_off_all_window_menus` path (`set_packet_pause_toggle`) still works. Save failure raises the
still-legacy `GMnu_ERROR_BOX` (not migrated — the topmost-monopoly rule lets it own input). MP:
all pause changes go through `PckA_UpdatePause` exactly as before; `load_game`/`save_game`
unchanged. `-classicmenu` still gets the sprite `load_menu`/`save_menu`.

**Phase 1a — what landed:** `GMnu_OPTIONS` migrated to an ImGui 4-button launcher (Load / Save /
Options / Quit) in `frontgui_ingame.cpp::optionsmenu_frame()`. Buttons are **one option per row,
icon then label** (user's chosen layout, 2026-09-06) — icons are the classic `options_menu`
button sprites (`GBS_options_button_load` / `_save` / `_graphc` / `_exit`) rendered to GPU
textures by the new `frontgui_sprite_tex.{h,cpp}` module and drawn by `FeSpriteButton()`. The
`GMnu_QUIT` confirm modal's Yes/No are **icons only** (`GBS_options_button_smd_yes` / `_no`), no
text captions. "Options" expands the shared
`frontgui_options_frame()` — now `frontgui_options_frame(bool in_game)`, exposed as
`frontgui_options_frame_ingame()` (`frontgui_screens.h`) — in its in-game form:
- `apply_class == SApply_NeedsRestart` rows rendered `BeginDisabled()` with a "change from the
  main menu" tooltip; the restart-note caption changes to match (`s_options_in_game` in
  `frontgui_screens.cpp`).
- "Define Keys" `BeginDisabled()` in-game (no in-game rebind path — §7).
- "Return to Main" → **"Back"** → `ingame_options_back_to_launcher()` collapses to the launcher.

`ingame_imgui_modal_active()` now walks the menu stack top-down and returns the verdict of the
**topmost** shown monopoly menu — so a still-legacy child menu (`GMnu_SAVE`/`GMnu_LOAD`) opened
from the launcher keeps its sprite buttons working while stacked above the ImGui launcher.
Save / Load buttons `turn_on_menu(GMnu_SAVE/LOAD)` (still legacy) for now.

`ModalWindowDimBg` fix (Phase 0 post-landing) and `frontgui_style.cpp` unchanged since.
Builds linux std + mingw + ftest, `check_layering --strict`, Catch2, full ftest sweep +
`gui_seam_ingame` (extended with a `GMnu_OPTIONS` migrated+modal assertion).

**`frontgui_sprite_tex.{h,cpp}` (new, 2026-09-06):** lazily renders a classic GUI button sprite
(`get_button_sprite()`) into a cached `RendererCreateDynamicTexture` handle keyed by sprite
index — same off-screen-render-then-composite path as the ImGui cursor
(`frontgui_style.cpp::build_cursor_pixels`) and land-preview panel, minus the palette override
(these sprites are only ever shown while gameplay runs, so the active engine palette is already
right). `FeSpriteButton(str_id, sprite_idx, label, icon_h)` is the widget: transparent hit box +
`AddImage` icon + optional label, blood-red hover/nav highlight, `do_sound_menu_click()` on
release, global-alpha aware; falls back to a plain `FeButton` until the texture is ready (or
permanently, if the sprite can't be found). `FeSpriteButtonWidth(...)` returns the same layout
width for a caller that centres a row itself (the quit modal's Yes/No). Not exercised by ftests
(headless has no ImGui frame) — visual verification is manual.

Live-tuning after the first icon pass: launcher window dropped its percent-of-screen min-width
(left dead space right of the labels) — now pure `AlwaysAutoResize`; row icon height `2.0 *
font`; quit-modal min-width constraint removed (`FeSubheading` is unwrapped, so auto-resize hugs
it) and the two icon buttons centred via `FeSpriteButtonWidth`.

**Phase 1b (not started):** fold `video_menu` (rotate / wall-height / gamma) + `autopilot_menu`
(4 computer-assist options, Game-tab `CollapsingHeader`) into `frontgui_options_frame(in_game)`
as hand-written in-game-context controls calling the same packet-routed handlers; migrate the
`GMnu_SAVE`/`GMnu_LOAD` slot lists to `FeBeginListBox` (shared `save_game_catalogue`); then
retire `video_menu`/`sound_menu`/`autopilot_menu`/`load_menu`/`save_menu`.

Goal (original): replace the in-game options/video/sound/autopilot/quit sprite menus with a
**4-button launcher — Save / Load / Options / Quit** — where "Options" opens the *same*
`frontgui_options_frame()` window the frontend already uses, gated so settings that can't change
mid-level are visibly disabled.

**Not "modal, game paused".** [00-overview.md](00-overview.md) §2.3 correction: opening this menu
does not pause the game (`front_input.c:409` just `turn_on_menu(GMnu_OPTIONS)`), and in MP the sim
keeps running. It still composites over live 3D — just centred, occasional, and not
frame-latency-critical.

---

## 1. Legacy menus this replaces

| `GMnu_*` | Menu | Today |
|---|---|---|
| `GMnu_OPTIONS` | `options_menu` | 6 icons → sub-menus (`frontmenu_ingame_opts_data.cpp:59`) |
| `GMnu_VIDEO` | `video_menu` | shadows / view distance / rotate / cluedo / gamma — cycle buttons, `init_video_menu` mirrors `settings.*`→`video_*` globals on open (`frontmenu_options.c:404`) |
| `GMnu_SOUND` | `sound_menu` | 3 volume sliders, `init_audio_menu` (`frontmenu_options.c:417`) |
| `GMnu_AUTOPILOT` | `autopilot_menu` | 4 computer-assist radio buttons → `kfx_net_state.comp_player_*` |
| `GMnu_QUIT` | `quit_menu` | yes/no → `gui_quit_game` (`frontend.cpp:1092`) |
| `GMnu_LOAD` / `GMnu_SAVE` | `load_menu` / `save_menu` | slot lists, `frontmenu_saves*` — **shared with the frontend** |
| `GMnu_ERROR_BOX` / `GMnu_MSG_BOX` | `error_box` / `message_box` | modal text + OK |
| `GMnu_DUNGEON_SPECIAL`, `GMnu_RESURRECT_CREATURE`, `GMnu_TRANSFER_CREATURE`, `GMnu_ARMAGEDDON`, `GMnu_HOLD_AUDIENCE` | dungeon-special dialogs | **`GMnu_HOLD_AUDIENCE` / `GMnu_ARMAGEDDON` migrated 2026-09-06** — they are yes/no confirms structurally identical to the quit modal, so they share `confirm_modal_frame()` (`frontgui_ingame.cpp`: title + centred smd_no/smd_yes, both close, Yes fires `choose_hold_audience` / `choose_armageddon`). `RESURRECT` / `TRANSFER` are list pickers (defer with save/load-style list work); `DUNGEON_SPECIAL` has an empty button list (dead). |

`instance_menu` (`GMnu_INSTANCE`) has an empty button list — dead, skip.
`pause_buttons` (the "Paused" caption) is not a `menu_list[]` menu — it is drawn elsewhere; see
Phase 3 / check `frontmenu_ingame_opts_data.cpp:88`.

---

## 2. The four buttons

`options_menu` becomes an ImGui window: title + Save / Load / Options / Quit. Reuse the existing
sprites for the icons — `GBS_options_button_save` / `_load` / `_exit` and an options/gear icon —
imported into the wrapper texture atlas, and the `gui_pretty_background` corner ornaments as
ImGui-composited chrome (the frontend's `FeBeginPanel` ornament path, pointed at these sprites).

- **Save** → the save-slot list (`save_menu`). Migrate `frontmenu_saves.c`'s slot list to
  `FeBeginListBox` — it is already close to the frontend load/save screens stage 4 Phase D did
  (`FeSt_FELOAD_GAME`), and `frontmenu_saves*` is shared code, so this may fall out of that work.
- **Load** → `load_menu`, same.
- **Options** → §3.
- **Quit** → the `quit_menu` yes/no (already the Phase 0 proof menu) as an `FeBeginModal`.

`maintain_loadsave` (`frontend.cpp:679`) currently enables/disables Save mid-mission (no saving
during e.g. a cutscene) — carry that as `ImGui::BeginDisabled()`.

---

## 3. Options — reuse `frontgui_options_frame()`

`frontgui_options_frame()` (`frontgui_screens.cpp:378`) is already a self-contained `##FeOptions`
ImGui window: `SetNextWindowSize` fixed, `FeHeading`, a `FeBeginTabBar` of Game / Graphics / **GUI**
/ Sound / Input, each tab calling `draw_setting_options_for_category(SCat_*)` (the generic schema
render) plus, for Sound/Input, the hand-written binary-`GameSettings` controls (volumes, mouse
sensitivity/invert) that predate the schema.

**GUI tab (2026-09-07):** added `SCat_GUI` to `config_settingschema.h`'s `SettingCategory` enum
for KeeperFX-only HUD/interface options, separate from the engine's Graphics tab. `UI_FONT_SCALE`
moved there. Nothing else references the enum, so a new category is enum value + one `FeTab` block.

**UI Font picker (2026-09-07):** a new `UI_FONT` row on the GUI tab — an `INGAME_RES`-style
dynamic enum (`ensure_ui_font_enum` in `config_settingschema.c`): `AUTO` / `CINZEL` / `EXOCET`
(the last only when the DK2 files are in `fxdata/`) plus one entry per family sub-directory under
`fxdata/font/`. Stored in `keeperfx.cfg` as `UI_FONT = <token>` (case 53, `config_keeperfx.c`;
`char ui_font[64]` in `KeeperFxUiConfig`). **`SApply_Live` + `frontend_only`** — the row is a live
swap but disabled while the settings screen is the in-game pause menu (new `SettingOption.frontend_only`
flag, same "greyed, shows a tooltip" treatment `SApply_NeedsRestart` rows get in-game).
`frontgui_style.cpp::refresh_fonts_if_changed()` (called from `FeStyleEnsureInit` → every
`FeStylePushFont`) drops the old faces (`ImFontAtlas::RemoveFont`) and reloads when `ui_font`
changes — safe mid-frame because `imgui_impl_sdlrenderer3` sets
`ImGuiBackendFlags_RendererHasTextures`, so the atlas is never `Locked` during a frame.
`frontgui_style.cpp::load_fonts()` resolves the token: `AUTO`/`EXOCET`→Exocet-then-Cinzel,
`CINZEL`→bundled Cinzel, else `resolve_family_fonts()` scans `fxdata/font/<name>/` (+ `static/`)
for TTF/OTF, picking heavy/light by weight keyword (single-weight → one file both roles), Cinzel
fallback on miss. New `PlatformManager_ListSubdirectories()` (SDL3 `SDL_EnumerateDirectory` —
`LbFileFindFirst` drops directories on every platform). New `SettingOption.label_literal` /
`help_literal` so KeeperFX-only rows added after the `gtext_eng.pot` guitext numbering froze skip
`get_string()`. Bundled Cinzel moved `config/fxdata/Cinzel/` → `config/fxdata/font/Cinzel/`;
`build/make/package.mk` now recursively stages `config/fxdata/font/`.

The only `FrontendMenuState` coupling is in the **dispatcher** (`frontgui_screens.cpp:1818` —
`case FeSt_FEOPTIONS: frontgui_options_frame();`) and the input gating, **not the body**. So:

1. **Call it from the in-game submission** when `ingame_imgui_menu_active(GMnu_OPTIONS)` and the
   in-game Options button was clicked. It draws the same window over the game.
2. **"Define Keys" button** — in the frontend it transitions to `FeSt_FEDEFINE_KEYS`. In-game
   there is no such state; route it to the in-game key-rebind path if one exists, or
   `ImGui::BeginDisabled()` it for a first pass and leave key rebinding to the frontend.
3. **Context-availability gate — the schema addition.** Decided (user 2026-09-06): **the rule is
   simply `apply_class == SApply_NeedsRestart` → disabled in-game** — if an option only takes
   effect after a restart, it isn't editable mid-game. No new schema field needed; the generic
   renderer wraps the row in `ImGui::BeginDisabled()` (with a "change this from the main menu"
   tooltip) whenever it's drawn in the in-game context and `apply_class == SApply_NeedsRestart`.
   `struct SettingOption` (`config_settingschema.h:74`) already has `apply_class`. (If a specific
   restart-class option later turns out to be genuinely useful to *queue* mid-game, that's a
   per-option exception then, not a reason to build the general mechanism now.)
4. **Binary-`GameSettings` controls apply live in-game.** The volume sliders and video toggles
   already work live in the legacy in-game menus (`gui_set_sound_volume` etc. apply immediately;
   `init_video_menu` mirrors on open and applies on change). The frontend
   `frontgui_options_frame()` binds the same `FrontendSliderCtrl` instances stage 4 Phase C
   exposed — confirm those apply live and call `save_settings()` (they should; `frontmenu_options.c`).
   The `video_*` cluedo/shadows/view-distance toggles need their `settings.*` write + live engine
   effect (`init_video_menu`'s inverse) wired as schema-adjacent hand-written controls in the
   Graphics tab, same treatment Sound/Input's volumes already get.
5. **Autopilot / computer-assist** → schema rows on the **Game** tab behind a collapsed
   `ImGui::CollapsingHeader` ([00-overview.md](00-overview.md) §8). Four bools today
   (`kfx_net_state.comp_player_aggressive` / `_defensive` / `_construct` / `_creatrsonly`), radio
   in the legacy menu (`autopilot_menu_buttons`, `gui_set_autopilot`). **MP note:** these are
   `kfx_net_state` — changing them mid-MP-game needs to go through the packet system if it does
   today (check `gui_set_autopilot`); a schema row that writes `kfx_net_state` directly would
   desync.

---

## 4. Extra `keeperfx.cfg` options

Stage 4 Phase G shipped the schema with "a representative slice" and later "the full 34-row
schema". [`../renderer/04-imgui-gui-foundation.md`](../renderer/04-imgui-gui-foundation.md) §6.2
lists the target set. Check what is actually in `setting_options[]` (`config_settingschema.c`)
now; add whatever §6.2 rows are still missing as part of this phase, each with its context mask.
This is shared work with the launcher (§6.2: "coordinate… on which side owns what").

---

## 5. Entanglements / risks

- **`active_menu_functions_while_paused()`** (`front_input.c:2771`) hard-codes
  `GMnu_QUIT|OPTIONS|LOAD|SAVE|VIDEO|SOUND|ERROR_BOX|AUTOPILOT`. If `VIDEO`/`SOUND`/`AUTOPILOT`
  cease to be separate menus, this list changes — and the ImGui Options window must itself keep
  input working while the game is paused (it is a monopoly-style modal).
- **Esc handling** (`front_input.c:396`) — Esc with a window menu up calls
  `turn_off_all_window_menus()`; Esc with none up opens `GMnu_OPTIONS`. The ImGui Options window
  must integrate: Esc closes it (deferred `turn_off_menu`), Esc from the game opens it.
- **`GMnu_OPTIONS` is a monopoly menu** (`is_monopoly_menu = 1`, `frontmenu_ingame_opts_data.cpp:136`).
  `first_monopoly_menu()` must still report it so un-migrated code suppresses correctly — Phase 0's
  `point_is_over_gui_menu` / `first_monopoly_menu` ImGui-rect work must cover it.
- **Save/load mid-game** writes real files and (load) tears down and rebuilds the level — this is
  the "don't call heavy transitions from inside `Begin()`/`End()`" case; use the deferred-action
  trampoline (`s_pending_*`).
- **`gui_quit_game`** ends the level and returns to the frontend — same deferral.
- **MP: quit** — in a multiplayer game "quit" means leaving the session; confirm the packet /
  `LbNetwork_Stop` path and that a deferred call is safe.

## 6. Checklist

- [x] `options_menu` → 4-button ImGui launcher, reusing the classic button-sprite icons
      (`frontgui_sprite_tex` / `FeSpriteButton`). `gui_pretty_background` corner ornaments still TODO.
- [ ] `frontgui_options_frame()` callable from the in-game submission.
- [ ] `SettingOption` gains a context field; generic renderer disables out-of-context rows.
- [x] `video_menu` / `sound_menu` controls present as Graphics/Sound tab rows (live apply,
      `save_settings()`). The `GuiMenu`s aren't removed — they're simply unreachable on the ImGui
      path (the launcher replaced the 6-icon `options_menu` that opened them); `-classicmenu`
      still uses them. `draw_ingame_video_controls()` / `draw_ingame_autopilot_controls()`,
      `in_game`-gated (2026-09-06).
- [x] Autopilot behind a collapsed `CollapsingHeader` on the Game tab; writes one
      `comp_player_*` flag then `gui_set_autopilot()` → `PckA_SetComputerKind` (classic path).
- [x] Save/Load slot lists on `FeBeginListBox` — `frontgui_ingame.cpp` `loadmenu_frame()` /
      `savemenu_frame()`, reading `save_game_catalogue[]` directly (2026-09-06).
- [ ] `active_menu_functions_while_paused()` updated; Esc open/close integrated.
- [ ] Deferred-action for save / load / quit.
- [ ] `-classicmenu` still shows the full old menu set; both paths build.
- [ ] SP + MP: change a live setting, change a restart-class setting, save, load, quit.

## 7. Open questions — resolved by code investigation (2026-09-06)

- **No in-game key-rebind menu exists** — `frontmenu_ingame_opts_data.cpp` has none; only the
  frontend `GMnu_FEDEFINE_KEYS`. Options for the "Define Keys" button in-game: (a)
  `ImGui::BeginDisabled()` it for Phase 1 (rebinding stays frontend-only), or (b) open
  `frontgui_definekeys_frame()` (stage 4 Phase D, a self-contained ImGui window) as a nested
  in-game modal — key capture (`define_key_input()`, `lbInkey`) is global state with no
  `GuiButton`, reusable. Lean (a) now, (b) as a follow-up.
- **Volume sliders already apply live *and* persist.** `sound_volume_set()`
  (`frontmenu_options.c:292`) does `settings.sound_volume = value; save_settings();
  SetSoundMasterVolume(value);`. `gui_set_sound_volume` → `frontend_sliderctrl_apply` →
  `ctrl->set_value` → that. Reusing the frontend `sound_volume_ctrl` / `music_volume_ctrl` /
  `mentor_volume_ctrl` instances in-game is free.
- **Autopilot / computer-assist: must send a packet — model as `SOptT_Enum`.** `gui_set_autopilot`
  (`frontend.cpp:3760`) reads which `kfx_net_state.comp_player_*` flag the radio toggled, then
  sends `PckA_SetComputerKind, ntype` → `set_autopilot_type(plyr_idx, ntype)` (`packets.c:860`).
  `kfx_net_state.comp_player_*` is in the resync blob (`net_resync.cpp:44`) — a schema row poking
  it directly would desync. So the schema row's `set_enum` mirrors the flags **and** sends
  `PckA_SetComputerKind`. (The on/off `BID_ASSIST` toggle → `PckA_ToggleComputer` is already
  packet-clean.)
- **Dungeon-special dialogs (`GMnu_DUNGEON_SPECIAL` etc.): not this phase** — gameplay-triggered
  (special box picked up), not pause-menu. Defer to a later gameplay-dialog sub-phase (or fold
  into [04-messages-tooltips-infobox.md](04-messages-tooltips-infobox.md)'s modal-box work).

**Decided (user 2026-09-06):** restart-class options (`apply_class == SApply_NeedsRestart`) are
**disabled in-game** — see §3 point 3. Clean blanket rule, no per-option decision, no new schema
field.
