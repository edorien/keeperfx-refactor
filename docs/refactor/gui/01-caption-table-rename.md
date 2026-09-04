# frontend_button_info[]: phased plan for a full symbolic rename

Status: **complete**. All phases (A-E below) executed; see the `FrontEndBtnStrIdx` enum in
`frontend.h` and the `FEBtn_*` names used across `frontend.cpp`, `frontmenu_select_data.cpp`,
`frontmenu_options_data.cpp`, `frontmenu_net_data.cpp`, and `frontmenu_saves_data.cpp`. Kept below
as the record of *why* the rename is scoped the way it is — re-read this before adding a new
captioned button or extending the enum, since the overloading it describes is still real. Follows
on from
[00-overview.md](00-overview.md) Phase 3's designated-initializer sweep, which deliberately left
`frontend_button_info[]`'s numeric slots as bare integers (`.content = { 45 }`) pending this plan.

## Context

The original Phase 3 plan assumed `content.lval` values in the "classic frontend flow" menus were
straightforwardly `frontend_button_info[]` caption-table indices, and that giving each of the
table's 115 slots a name would be a pure, low-risk rename. Investigating it for real found that
assumption wrong: **the same numeric literal is overloaded for up to three unrelated purposes**,
selected by which function actually consumes `gbtn->content.lval` at runtime — and that's decided
per-row by the button's `draw_call`/`click_event`/`maintain_call`, not by the number itself:

1. **Genuine caption-table index** — the button's draw function calls
   `frontend_button_caption_text()`/`frontend_button_caption_font()` (`frontend.cpp:1239,1251`),
   which read `frontend_button_info[gbtn->content.lval]`. This is the only case a rename is
   actually describing reality.
2. **Select-list row marker** — consumed via the `FE_SELECTLIST_ROW_BASE` (45) convention
   (`frontmenu_selectlist.h`) by row-button draw/click/maintain functions
   (`frontend_draw_level_select_button`, `frontnet_draw_service_button`, `frontend_draw_load_game_button`,
   etc.). Confirmed via `grep -rn "content\.lval\s*[-+]\s*[0-9]\+"` — every such row uses base 45.
   Some of these *also* happen to call `frontend_button_caption_font()` for a default/hover font
   (since slots 45-51 are all `{GUIStr_Empty, 1}` anyway), making them accidentally-dual-purpose;
   others (the select-screen row buttons) don't touch the caption table at all for the same
   literal numbers.
3. **Unrelated hardcoded/grid marker** — e.g. `frontnet_draw_alliance_button`/
   `frontnet_maintain_alliance`/`frontnet_select_alliance` (`frontmenu_net.c`) use `content.lval - 74`
   as an alliance-grid row offset with **no connection to `frontend_button_info[74]` at all**;
   `frontend_draw_slider_button` (`gui_frontbtns.c:890`) hardcodes `== 17`/`== 36`/`== 38` to pick a
   scroll-arrow sprite, never touching the table; `frontmenu_options_data.cpp`'s define-key rows use
   *negative* content values (`{-1}`..`{-10}`) as row markers, which can't be table indices at all.

A blanket rename would give confident-sounding names (derived from the table's GUIStr at that
position) to numbers whose real meaning at a given call site is "list row 3" or "alliance grid
column 2" — actively misleading, worse than the current bare numbers. Getting this right requires
verifying, per row, which of the three categories it actually falls into.

## Phase A — Build a verified per-row consumer map

For every `.content = { N }` (or negative marker) in the 5 in-scope files
(`frontend.cpp`'s `frontend_main_menu_buttons`/`frontend_statistics_buttons`/`frontend_high_score_score_buttons`,
all 4 arrays in `frontmenu_select_data.cpp`, both arrays in `frontmenu_options_data.cpp`, all 4
arrays in `frontmenu_net_data.cpp`, and `frontend_load_menu_buttons` in `frontmenu_saves_data.cpp`):

1. Read the row's `draw_call` function body. Confirm whether it calls
   `frontend_button_caption_text`/`frontend_button_caption_font` directly, or reaches them only
   through a shared helper (`frontend_draw_button`, `gui_frontbtns.c:810`) that always does.
2. Separately check the row's `click_event`/`maintain_call` bodies for `content.lval ± K`
   arithmetic (row-marker/grid-offset usage) — a row can be *both* category 1 and 2/3 at once
   (see the slots-45-51 case above), which is fine: record all applicable categories per row, not
   just one.
3. Watch for **runtime-reassigned content** — `frontend_draw_variable_mappack_exit_button`
   (`frontmenu_select.c:497`) overwrites `gbtn->content.lval` at draw time with a value chosen
   dynamically (111 or 6), so the array literal's `{111}` isn't even the value actually read by
   the caption lookup in some states. Flag any row whose `draw_call`/`click_event` writes
   `content.lval` before reading it — these need the *runtime* values traced, not just the
   initializer's.
4. Record results as a table: `slot N -> [file:line row] -> {pure-caption | row-marker | grid-marker | runtime-reassigned} -> verified?`.

This phase is the bulk of the effort — roughly 50 populated slots (per the earlier grep of every
`.content = { N }` site, positions 1-16,19,21,24-51,74-77,83-114 are populated; many are unused
`{GUIStr_Empty,*}` filler never referenced by any row) each need at least one, sometimes several,
function bodies read and cross-checked. Budget this as the majority of the work — Phases B-D are
comparatively mechanical once the map exists.

## Phase B — Classify every populated slot into three buckets

Using Phase A's map:

- **Bucket 1, pure caption**: every consuming row is category 1 only (main menu items 1-9, net
  menu titles/section-labels, options page titles and static field labels, statistics/highscore
  titles, campaign/mappack/level titles — expected to land around 40-50 slots). These get real
  symbolic names.
- **Bucket 2, row-marker (with or without incidental caption reuse)**: slots 45-51 (list rows,
  `FE_SELECTLIST_ROW_BASE`-relative) and the scroll-widget slots 17/18/36-40 consumed by
  `frontend_draw_slider_button`'s hardcoded sprite selection. **Do not rename these via the
  caption enum** — they're already correctly self-documenting through
  `FE_SELECTLIST_ROW_BASE`/`frontend_selectlist_row_to_item_index()` (Phase 1's engine). If the
  scroll-widget role numbers (17/18/36/38 = up-arrow, others = down-arrow) are worth naming at
  all, that's a separate, smaller follow-up — symbolic *widget-role* constants
  (`FE_SCROLL_ROLE_UP`?) in `gui_frontbtns.h`, not `frontend_button_info` slot names.
- **Bucket 3, unrelated marker**: slot 74-77 (alliance grid offset) and the negative define-key
  markers. Leave as bare numbers with a one-line comment pointing at the real convention (`- 74`
  alliance-grid base, matching `frontnet_maintain_alliance`; negative = define-key row per
  `frontend_define_key_maintain`). These are not `frontend_button_info` indices in any row that
  uses them and must never enter the enum.

Any slot Phase A couldn't fully verify (ambiguous or mixed in a way not covered above) stays a
bare number rather than getting a guessed name — a wrong name is worse than no name.

## Phase C — Build the enum for Bucket 1 only

Add `enum FrontEndBtnStrIdx` to `frontend.h` near `FRONTEND_BUTTON_INFO_COUNT`, with explicit
values pinned to match today's positions exactly (pure rename, zero behavior change — verified by
Phase A, not assumed). Switch `frontend_button_info[]`'s own initializer
(`frontend.cpp:228`) to designated array-element form: `[FEBtn_Foo] = { GUIStr_Foo, font },`.
Bucket 2/3 slots keep numeric array-position comments (`// [45]`) as today, since they're not
getting a name.

## Phase D — Rewire Bucket 1 consumers

For every row Phase B placed in Bucket 1, replace `.content = { N }` with
`.content = { FEBtn_Foo }` in place, across the 5 in-scope files. Bucket 2/3 rows are untouched
(stay bare numbers, or gain the short explanatory comment from Phase B). Convert file by file with
a build checkpoint after each, same discipline as Phase 3's designated-init sweep.

## Outcome (as executed)

63 of the 115 slots turned out to have a non-`GUIStr_Empty` GUIStr and got a name (`FEBtn_Foo =
N`, values pinned exactly to their old position). A few GUIStr values legitimately recur at a
second slot with a different `font_index` (e.g. `GUIStr_MnuOptions` at both 96 and 97,
`GUIStr_MnuHighScoreTable` at 85 and 104) — those got an `_<N>` suffix (`FEBtn_MnuOptions_97`)
rather than an invented distinct name. 60 `.content = { N }` sites across the 5 files were rewired
to the new names; the rest of the 63 are declared but not currently referenced in these files
(e.g. `FEBtn_MnuPlayIntro`), kept for completeness since they're still real, valid table slots.

Phase A's read-every-consumer pass also turned up two more Bucket 2/3-shaped families beyond what
this doc anticipated, both confirmed by reading their draw functions directly and left as bare
numbers exactly as Bucket 2/3 prescribes:
- **Dead slots**: `frontend_draw_scroll_box_tab`/`frontnet_draw_alliance_box_tab`/
  `frontnet_draw_text_bar` (pure sprite chrome) and every `*_scroll_tab` wrapper (they all resolve
  to `frontend_draw_scroll_tab(gbtn, offset, first, last)`, `frontend.cpp:928`, which never reads
  `gbtn->content` at all) never read their row's `.content` value for anything — slots 28 and 40
  are populated, `{GUIStr_Empty, 1}`, and completely inert everywhere they appear.
- **Scroll-box size selector**: `frontend_draw_scroll_box` (`gui_frontbtns.c:848`) switches on
  `gbtn->content.lval` between literal cases `24/25/26/89/90/91/94` to pick a height/scrollbar
  combination — real, load-bearing, but a box-size code, not a caption index. Worth its own named
  constants (`FE_SCROLLBOX_2LINE`, etc.) as a future follow-up, same as the scroll-widget role
  numbers Bucket 2 already flagged — neither belongs in `FrontEndBtnStrIdx`.

`frontend_draw_variable_mappack_exit_button` (`frontmenu_select.c:497`) was the one confirmed
runtime-reassignment case: it overwrites `gbtn->content.lval` at draw time before reading it, so
its array literal's initial `{111}` was renamed to `{ FEBtn_MnuReturnToFreePlay }` for a correct
starting state, and the function's own local magic numbers (`long str_idx = 111` / `= 6`) were
also switched to `FEBtn_MnuReturnToFreePlay`/`FEBtn_MnuReturnToMain` for the same reason.

## Verification

- Build (`KFX_OS=linux ./build-cmake.sh`) after each file in Phase D.
- `python3 scripts/check_layering.py --strict` once at the end.
- Full `kfx_frontend_utest` run (no behavior is expected to change, so no new tests are required
  for this phase specifically, but the existing suite must stay green).
- Manual smoke test covering every menu with a Bucket-1 rename: main menu, statistics, high score
  table, campaign/level/mappack/mp-mappack select (title bars + the "type" label row only — their
  list rows are Bucket 2, untouched), options + define-keys, net service/session/start screens,
  load game — confirm every title, label, and static caption still reads correctly (a
  transposition mistake in Phase A/C would show up as the wrong text on a button, not a build
  failure, since the enum values are hand-verified rather than compiler-checked against the
  original numbers).
