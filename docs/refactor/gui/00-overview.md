# kfx_frontend: common list/control engines + menu authoring cleanup

Status: **complete**. Scope: `src/kfx_frontend/` only. Builds directly on top of the
`docs/refactor/` stage-01..13 library split (`kfx_frontend` already exists as its own library;
see `docs/Architecture/architecture.md` for the current, authoritative library map) — this is a
follow-on cleanup within that library, not a further layering change. Phase 3's caption-table
naming turned out to need its own careful, phased treatment — see
[01-caption-table-rename.md](01-caption-table-rename.md) (also complete). Phase 1's engine was
also extended, after the fact, to `frontmenu_net.c`'s session/player/message/service lists (not
originally in scope here — found via an explicit audit request and migrated the same way).
Follow-on work outside this doc's original scope: [03-button-primitives.md](03-button-primitives.md)
(flexible-width/icon/grid button capability, complete),
[02-menu-v2-mockup-gap-analysis.md](02-menu-v2-mockup-gap-analysis.md) (gap analysis for a full
visual redesign mockup, analysis only, not started), and
[04-phase2-landview-panel-investigation.md](04-phase2-landview-panel-investigation.md) (scoping
what it actually takes to embed the interactive landview in a merged-screen panel, analysis
only, not started).

## Context

Today, adding a button to any `kfx_frontend` menu is fiddly for two independent reasons:

1. **Authoring friction**: every menu (main menu, campaign/level/settings/multiplayer select,
   in-game HUD panels) is declared as a flat, fully **positional** `struct GuiButtonInit` array
   literal — 20 comma-separated fields with no names, e.g. (`frontend.cpp:121`):
   ```c
   { LbBtnT_NormalBtn, BID_DEFAULT, 0, 0, frontend_ldcampaign_change_state,NULL, frontend_over_button,18,999,368,999,368,371,46, frontend_draw_large_menu_button, 0, GUIStr_Empty, 0, {104}, 0, frontend_main_menu_highscores_maintain },
   ```
   Inserting a button means counting commas to find the right field, manually renumbering every
   Y-coordinate below the insertion point, and — for the "classic frontend flow" menus — hunting
   for a free numeric slot in a shared magic-index caption table.

2. **Duplicated list logic**: the campaign, level, mappack, and multiplayer-mappack select
   screens (`frontmenu_select.c`/`.h`/`frontmenu_select_data.cpp`) are four near-identical
   copies of the same scrollable-list mechanics (~15 functions each: scroll up/down, drag-scroll,
   up/down-enabled maintain, per-row enabled maintain, mouse-wheel/keyboard update, scroll-tab
   draw) differing only in which global list they read and what happens on select. Three of the
   four (campaign/mappack/mp-mappack) even share the exact same underlying data type
   (`struct CampaignsList` / `struct GameCampaign`, `src/kfx_config/include/config_campaigns.h`).

3. **Duplicated settings-control logic**: the settings menu's volume sliders
   (`gui_set_sound_volume`/`gui_set_music_volume`/`gui_set_mentor_volume`) and its one checkbox
   (`frontend_invert_mouse`) are each a hand-written copy of "read the widget's value, write one
   `settings.<field>`, call `save_settings()`, maybe fire a side effect" — same duplication shape
   as (2), one level down at the widget instead of the list.

This refactor addresses all three, and — per the user — the scrollable-list engine from (2) and
the settings-control engine from (3) are being built now specifically because the **next stage**
of work will surface the many `keeperfx.cfg` options that currently require manual file edits in
this menu, as a scrollable list of bound controls, and that needs both pieces of generic
machinery already in place.

Out of scope (unchanged): `struct GuiButtonInit`/`struct GuiMenu` field layout (`kfx_platform`,
shared game-wide) and the `GMnu_*` enum / `menu_list[]` menu-registration machinery — both are
for a different concern (registering a whole new *menu*) than either of the two problems above.

## Phase 1 — Common scrollable-list engine

### New files: `src/kfx_frontend/include/frontmenu_selectlist.h`, `src/kfx_frontend/src/frontmenu_selectlist.c`

A descriptor struct plus generic pagination functions, decoupled from what a row *means* (select
vs. future edit-in-place for settings) — only the scrolling/bounds/enabled-state arithmetic is
shared:

```c
struct FrontendSelectList {
    long scroll_offset;
    long items_visible;
    long items_visible_max;
    unsigned long (*item_count)(void);   // per-list: e.g. wraps campaigns_list.items_num
};

void frontend_selectlist_scroll_up(struct FrontendSelectList *list);
void frontend_selectlist_scroll_down(struct FrontendSelectList *list);
void frontend_selectlist_scroll_to_offset(struct FrontendSelectList *list, struct GuiButton *gbtn);
void frontend_selectlist_up_maintain(struct FrontendSelectList *list, struct GuiButton *gbtn);
void frontend_selectlist_down_maintain(struct FrontendSelectList *list, struct GuiButton *gbtn);
void frontend_selectlist_row_maintain(struct FrontendSelectList *list, struct GuiButton *gbtn);
void frontend_selectlist_update(struct FrontendSelectList *list); // wheel/key handling + clamp
void frontend_selectlist_draw_scroll_tab(struct FrontendSelectList *list, struct GuiButton *gbtn);
long frontend_selectlist_row_to_item_index(struct FrontendSelectList *list, struct GuiButton *gbtn);
```

This reuses the existing generic primitives `frontend_scroll_tab_to_offset`/
`frontend_draw_scroll_tab` (`frontend.cpp:921,939`, already parameterized and already shared
across highscore/options/saves/select/net — same idiom, just one level higher). Name the
currently-unnamed `-45` row-index offset seen throughout `frontmenu_select.c` as a constant
(e.g. `FE_SELECTLIST_ROW_BASE`) in the new header, since `frontend_selectlist_row_to_item_index`
centralizes that arithmetic once instead of repeating `btn_idx - 45` in ~8 places per list.

### Refactor the 4 existing select lists to instances of the engine

In `frontmenu_select.c`, each list keeps a `static struct FrontendSelectList` plus its genuinely
distinct pieces (item count source, row draw/caption logic, on-select action — these differ in
shape between the level list, which reads `campaign.freeplay_levels[]` via `get_level_info()`,
and the campaign/mappack/mp-mappack lists, which all read `struct CampaignsList.items[]`
directly). Every duplicated scroll/maintain/update function collapses to a 1–3 line wrapper, e.g.:

```c
static struct FrontendSelectList campaign_select_list = { .item_count = campaign_select_count };

void frontend_campaign_select_up(struct GuiButton *gbtn) { frontend_selectlist_scroll_up(&campaign_select_list); }
void frontend_campaign_select_down(struct GuiButton *gbtn) { frontend_selectlist_scroll_down(&campaign_select_list); }
void frontend_campaign_select_scroll(struct GuiButton *gbtn) { frontend_selectlist_scroll_to_offset(&campaign_select_list, gbtn); }
```

Since campaign/mappack/mp-mappack share `struct CampaignsList`, their `item_count`/row-caption
logic can additionally share one small helper taking a `struct CampaignsList *` — do this as an
inner simplification once the engine is in place, not as a separate phase.

The wrapper functions keep their existing names (`frontend_campaign_select_up`, etc.) so
`frontmenu_select_data.cpp`'s `GuiButtonInit` arrays don't need their function-pointer columns
touched by this phase — Phase 3 (below) reformats those same arrays for readability separately.

**Preserve, and make explicit, the cross-list scroll reset**: `frontend_mappack_list_load()`
currently resets `select_level_scroll_offset` (not the mappack list's own offset) — confirmed
intentional: selecting a map-pack swaps in a new level list, so that list must reset to the top.
Keep this behavior, but make it an explicit, commented line against the new descriptor (e.g.
`level_select_list.scroll_offset = 0; // new mappack selected: its level list must restart at
the top`) instead of an unexplained global write.

## Phase 2 — Common settings-control engine

The settings menu's controls have the same duplication shape as Phase 1's lists, just at the
*widget* level instead of the *list* level. `frontmenu_options.c` currently has
`gui_set_sound_volume`/`gui_set_music_volume`/`gui_set_mentor_volume` (near-identical: read a
slider value, optionally apply `make_audio_slider_nonlinear`, write one `settings.<field>`, call
`save_settings()`, sometimes fire a side effect like `SetSoundMasterVolume`/`do_sound_menu_click`)
and one checkbox (`frontend_invert_mouse`/`frontend_draw_invert_mouse`: toggle a bool
`settings.<field>`, draw ON/OFF text via `frontend_button_caption_font`). `frontend_init_options_menu`
(`frontend.cpp:1211`) separately hand-writes the reverse direction — reading each
`settings.<field>` into its slider's `content.lval` at menu-open time via
`get_gui_button_init(gmnu, BID_*)`. This is exactly the pattern the next stage (surfacing many
more `keeperfx.cfg` options in this menu) will otherwise keep copy-pasting.

### New files: `src/kfx_frontend/include/frontmenu_settingctrl.h`, `src/kfx_frontend/src/frontmenu_settingctrl.c`

Two small descriptor-driven control kinds, mirroring the slider/checkbox widget types already in
use (widget rendering itself is untouched — `frontend_draw_slider`/`frontend_draw_small_slider`
are already shared draw functions; only the value-binding plumbing is new):

```c
struct FrontendSliderCtrl {
    long (*get_value)(void);         // reads the bound settings.<field>
    void (*set_value)(long value);   // writes settings.<field>, calls save_settings(), any side effect
    TbBool nonlinear;                 // true: apply make_audio_slider_linear/nonlinear, as volume does
};
void frontend_sliderctrl_init(struct GuiMenu *gmnu, short bid, const struct FrontendSliderCtrl *ctrl); // replaces the hand-written block in frontend_init_options_menu
void frontend_sliderctrl_apply(struct GuiButton *gbtn, const struct FrontendSliderCtrl *ctrl);          // the click_event body

struct FrontendCheckboxCtrl {
    TbBool (*get_value)(void);
    void (*toggle_value)(void);      // flips settings.<field>, calls save_settings(), any side effect
};
void frontend_checkboxctrl_toggle(struct GuiButton *gbtn, const struct FrontendCheckboxCtrl *ctrl);
void frontend_checkboxctrl_draw(struct GuiButton *gbtn, const struct FrontendCheckboxCtrl *ctrl);        // shared ON/OFF text draw
```

Existing controls (`gui_set_sound_volume`, `gui_set_music_volume`, `gui_set_mentor_volume`,
`frontend_set_mouse_sensitivity`, `frontend_invert_mouse`/`frontend_draw_invert_mouse`) become
`static const struct FrontendSliderCtrl`/`FrontendCheckboxCtrl` instances plus 1-line wrappers,
same shape as Phase 1's list wrappers — the per-control side effects (`do_sound_menu_click`,
`SetSoundMasterVolume`, `set_music_volume`) stay exactly where they are today, inside each
control's `set_value`, since those are genuinely control-specific and shouldn't be forced into
the shared engine. Adding a new `keeperfx.cfg`-backed setting to the menu then becomes: one
get/set (or get/toggle) pair plus one `GuiButtonInit` row referencing the shared
`frontend_sliderctrl_apply`/`frontend_checkboxctrl_toggle` — not a new hand-rolled function.

## Phase 3 — Authoring cleanup: designated initializers + named caption constants

Applies to all 9 `kfx_frontend` files with `GuiButtonInit`/`GuiMenu` array literals (do this
*after* Phases 1–2, so `frontmenu_select_data.cpp` and `frontmenu_options_data.cpp` are
reformatted in their already-simplified, post-refactor state):

1. `frontmenu_specials.c` — smallest, and the only plain-C file, so it's the fastest way to
   confirm designated-init syntax compiles cleanly under the C11 toolchain before the bigger files.
2. `frontmenu_ingame_evnt_data.cpp`, `frontmenu_ingame_opts_data.cpp` — small, no caption-table involvement.
3. `frontmenu_saves_data.cpp` — first file with a caption-table array (`frontend_load_menu_buttons`).
4. `frontmenu_select_data.cpp`, `frontmenu_options_data.cpp`, `frontmenu_net_data.cpp` — bulk of
   the caption-table + row-stacking work.
5. `frontmenu_ingame_tabs_data.cpp` — largest file (480 lines, 13 arrays, includes true grids); do
   last among the data files since it has the most bespoke (non-stack) layouts to get right.
6. `frontend.cpp` last — it owns `frontend_button_info[]` and gets the new caption-name enum.

### Mechanics (per `bflib_guibtns.h` field order, confirmed safe under C11 + C++20 + MSVC/mingw):

`gbtype, id_num, unused_field, button_flags, click_event, rclick_event, ptover_event, btype_value, scr_pos_x, scr_pos_y, pos_x, pos_y, width, height, draw_call, sprite_idx, tooltip_stridx, parent_menu, content, maxval, maintain_call`

- Convert every `GuiButtonInit`/`GuiMenu` literal to designated-initializer form in that field
  order (satisfies C99's order-flexible rule and C++20's order-required rule simultaneously).
  Omit fields that are 0/NULL (`unused_field` is genuinely unused; `BID_DEFAULT` is `#define`d
  `0`) — most rows shrink substantially. `union GuiVariant content` supports nested designated
  init: `.content = { .lval = FEBtn_Foo }`. Terminator rows (`{-1, BID_DEFAULT, 0, ...}`) become
  `{ .gbtype = -1 }`.
- **Row-stacking helper** `#define FE_ROW_Y(base, step, n) ((base) + (step) * (n))` in
  `frontend.h`, applied only to arrays confirmed to be genuine fixed-spacing stacks — main menu
  body (below the title), `frontmenu_select_data.cpp`'s 4 arrays (+22px), `frontend_define_keys_buttons`
  (+22px), net's session/service lists (+26px), the in-game event-message stack (-30px), the
  `autopilot_menu_buttons`/`video_menu_buttons` horizontal row (+48px), and specials.c's
  resurrect/transfer stacks (+28px). Leave every hand-tuned grid/dialog (room/spell/trap/creature
  panels, the alliance-matrix, tiny fixed dialogs, `frontend_option_buttons`,
  `frontend_net_start_buttons`) as raw coordinates — forcing a stack abstraction onto a 2D layout
  would fight the layout, not help it.
- **Named caption-table constants**: `frontend_button_info[]` (`frontend.cpp:221`, currently 115
  entries) and its `content.lval`-as-index pattern (`febtn_idx`, `frontend.cpp:1237-1253`) is used
  *only* by `frontend.cpp`'s own 3 captioned arrays, `frontmenu_select_data.cpp`,
  `frontmenu_options_data.cpp`, `frontmenu_net_data.cpp`, and `frontend_load_menu_buttons` in
  `frontmenu_saves_data.cpp` — confirmed via grep, nowhere else. Add a named enum in `frontend.h`
  near `FRONTEND_BUTTON_INFO_COUNT` mirroring today's numeric slots exactly (pure rename, zero
  behavior change), switch the table itself to `[FEBtn_Foo] = {...}` designated array-element
  init, and use `FEBtn_Foo` instead of a bare int in every button literal that references it.
  Appending a new captioned button becomes: add one enum value at the end (auto-numbered), add
  its table row, reference it — no manual index hunting. Bump `FRONTEND_BUTTON_INFO_COUNT` to
  match the enum's size when it grows (keep a comment noting the two must stay in lockstep, since
  the header/source split means it can't self-derive).

The in-game HUD files (`ingame_tabs`/`ingame_opts`/`ingame_evnt`, and `specials.c`) already
caption buttons more directly (a `GUIStr_*`/`CpgStr_*` constant straight in `tooltip_stridx`, a
custom `draw_call`, or a raw string pointer) — only the designated-init/row-stack parts of Phase 3
apply to them, not the caption-table renaming.

## Verification

- After each file in every phase: `KFX_OS=linux ./build-cmake.sh` — first compile error pinpoints
  exactly which literal or wrapper has a mistake.
- After Phase 1: exercise the select-list engine specifically — the existing
  `src/kfx_frontend/tests/` Catch2 suite is the right place for new unit tests of
  `frontend_selectlist_*` (pure functions over a `FrontendSelectList` + fake `item_count`, no
  global game state needed, matching the style already in `gui_frontmenu_test.cpp`).
- After Phase 2: similarly, unit-test `frontend_sliderctrl_apply`/`frontend_checkboxctrl_toggle`
  against fake get/set pairs (no real `settings`/audio state needed).
- `python3 scripts/check_layering.py --strict` once at the end (no `#include` boundaries are
  expected to change, but it's a cheap final check).
- Manual smoke test in a running build (game data required): main menu → campaign select → level
  select → settings → multiplayer map-pack select, confirming scroll/select/enabled-state still
  behave identically (especially the mappack→level scroll-reset quirk and that the volume
  sliders/mouse-invert checkbox still read, write, and side-effect exactly as before), plus a
  couple of in-game HUD tabs (room, spell, options) to confirm Phase 3's reformatting didn't drop
  a button or callback.
