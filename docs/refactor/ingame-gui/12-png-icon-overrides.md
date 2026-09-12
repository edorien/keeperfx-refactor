# Phase 12 — PNG icon overrides (user icon packs)

Status: **first landing, 2026-09-11** — let a user drop a folder of plain PNGs into
`fxdata/gui/<pack-name>/` and have them replace the built-in room/power/trap/creature/ability icons
and the static job/tendency/bar/battle/confirm/launcher icons the ImGui HUD draws, plus the
procedural marble panel background, selected live from a new `GUI_ICON_PACK` GUI-tab setting.
Filenames follow a fixed naming convention per the user's spec (§2) rather than the legacy sprite
atlas's own inconsistent naming, so a pack author never needs to know a `GPS_*` constant or a
sprite-sheet index.

**Landed as designed, §2–§7 in full:** `GUI_ICON_PACK` (`config_keeperfx.h`/`.c`,
`config_settingschema.c`) mirrors `UI_FONT`'s `fxdata/font/` scan verbatim, against `fxdata/gui/`
instead -- `"NONE"` plus one entry per sub-directory, `SApply_Live` (not `frontend_only` like
`UI_FONT` -- clearing this feature's texture cache mid-session is cheap, unlike a font-atlas
rebuild). The new `frontgui_ingame_icon_overrides.{h,cpp}` module (kfx_frontend) holds the PNG
decode (plain RGBA8 via `spng`, *not* `custom_sprites.c`'s palette-quantizing decoder), the
friendly-name cache, and four entry points: `FeIconOverrideTexture()` (raw name -- backgrounds and
the static table's internal lookups), `FeIconOverrideActiveInactive()` /`FeIconOverrideSingle()`
(the data-driven categories, §3.2, with the `_inactive`-falls-back-to-dimmed-`_active` contract from
§3.3), and `FeIconOverrideForStaticIndex()` (the static table, §3.1, checked from
`FeSpriteTexture()`/`FeGuiPanelTexture()` themselves so every *other* caller of those static
sprites needed zero changes). `blit_fit()` (`frontgui_ingame_cells.cpp`) was split to share its
aspect-fit math with a new `blit_fit_tex()` taking a resolved texture handle directly, used by every
data-driven call site's override-or-legacy branch.

Wired at every call site listed in §2/§5: `room_grid()`/`spell_grid()`/`trap_grid()`
(`frontgui_ingame_grids.cpp`, doors resolved via `tngclass == TCls_Door` picking
`door_code_name()` over `trap_code_name()`, both still under the `trap_` prefix), `creature_list()`
/`creature_list_horizontal()`/`creature_query_panel()`/`creature_query_panel_horizontal()`
/`instance_cell()` (`frontgui_ingame_creature.cpp`), `battler_cell()`
(`frontgui_ingame_battle.cpp`), and the background fills in `draw_background()`/
`draw_background_horizontal()` (`frontgui_ingame_panel.cpp`) and `ingame_tabcontent_draw()`
(`frontgui_ingame_tabcontent.cpp`) via a shared `draw_face_or_override()` helper. The static table
(job/tendency/bar/battle/confirm/launcher) covers every row from §2 except the colorized
`tab_room`/`tab_creature` icons and the ~20 `stat_*` rows -- both deliberately deferred (documented
in the static table's own comment), not silently dropped: colorized sprites resolve to a different
actual index per player colour (`get_player_colored_icon_idx()`), which the static index-keyed table
can't represent without a separate decision about what happens to the tint; `stat_*` is lower value
(the vertical layout's own Stats page only) and was cut to keep the first landing's table small.
`message_<kind>`/`event_<kind>` as *standalone* categories were dropped from the plan entirely after
implementation-time investigation: `event_style()`'s non-glyph cases and most of
`message_icon_spridx()`'s cases already resolve to sprites the static table or a data-driven category
covers by construction (the aliasing behaviour §2 already documented) -- adding separate rows for
them would have been redundant table entries, not new coverage.

Verified: build (linux/ftest/windows -- each build tree's CMake glob needed an explicit
`cmake .` re-configure to pick up the two new files, `ninja` alone did not), `check_layering.py
--strict` clean, `gui_packet_parity` + `gui_seam_ingame` + full `-ftests` sweep all passing
headless. Also smoke-tested end-to-end with a throwaway 3-file pack (`bar_research`/`bar_workshop`
-- the tab-header row, drawn every frame regardless of active tab -- plus `room_treasure_active`)
dropped into `fxdata/gui/testpack/` and `GUI_ICON_PACK=TESTPACK` in `keeperfx.cfg`: decoded and
applied with no warnings/errors logged, reverted after. Not visually verified in a real (non-headless)
session -- worth a live look before calling this done.

**Two real bugs and a naming mismatch found live-testing against the user's own `dk1`/`dk2` packs
(pre-built asset sets already dropped into `fxdata/gui/`, not throwaway fixtures), same day:**

1. **`power_*` doubled its own prefix.** `PowerConfigStats::code_name` is `"POWER_LIGHTNING"`, not
   `"LIGHTNING"` -- unlike every other data-driven category, it already carries the category name.
   `FeIconOverrideActiveInactive()`/`FeIconOverrideSingle()`'s naive `category + "_" + code_name`
   concatenation asked for `power_power_lightning_active.png`, which no pack author would ever
   write. New shared `build_base_name()` detects and skips the redundant prefix.
2. **Case-sensitive path building on a case-insensitive setting.** `GUI_ICON_PACK`'s own enum
   matching is case-insensitive (`strcasecmp`, matching `UI_FONT`'s own family-name matching), but
   the PNG path was built straight from `keeperfx_ui_config.gui_icon_pack`'s literal spelling --
   silently resolving nothing on Linux/macOS the moment the config value's case didn't match the
   folder's own casing (traced directly to "not seeing them change": the user's own config had
   `GUI_ICON_PACK=DK1`, folder is `dk1`). Fixed by resolving the *actual* on-disk directory name via
   `PlatformManager_ListSubdirectories()` once per pack switch (`resolve_pack_dir_casing()`) and
   building every PNG path from that instead of the setting's own spelling.
3. **`_std`/`_dis` accepted alongside `_active`/`_inactive`.** Both existing packs were built
   against the legacy DK "_std"/"_dis" suffix convention throughout, not this doc's `_active`/
   `_inactive` -- a real, foreseeable convention clash (it's the convention the existing mod/
   `custom_sprites.c` icon system already uses), not a one-off typo. `FeIconOverrideActiveInactive()`
   now tries `_active`/`_inactive` first, then `_std`/`_dis`, before falling back to the
   dimmed-`_active` treatment -- so a pack author can use whichever convention they already know.
4. **Two new log lines** (previously silent per doc §6.5's "stay quiet" default, which covered
   normal cache misses but not diagnosability of a totally inactive pack): one `JUSTLOG` per pack
   switch naming the resolved on-disk directory, and one `WARNLOG` when a matched PNG exists but
   fails to decode (a real gap versus §6.2's own proposed default, which this now actually
   implements).
5. **The packs' own filenames were renamed in place** (not a code workaround) once the actual
   mismatch was diagnosed with a temporary instrumented build: base-name mismatches against each
   category's real `code_name` (display names vs internal codes -- `room_hatchery` renamed to
   `room_garden` since the Hatchery room's code is `GARDEN`; abbreviations expanded --
   `spell_lightng` to `power_lightning`; DK2-specific creature renames -- `creature_icon_reaper` to
   `creature_icon_horny`, the Horned Reaper's actual code name). ~80 files renamed across both
   packs; positively confirmed via a temporary hit-logging build that `dk1`'s renamed room icons
   are being decoded and applied. A handful of files in each pack still don't map to anything (no
   matching room/trap/creature, or a genuinely ambiguous guess) and were left alone rather than
   force-renamed -- flagged to the user directly rather than in this doc, since they're pack-content
   questions, not implementation ones.

---

## 0. Why this is simpler than it first looked

Two facts found while investigating make this a small, well-contained feature rather than a new
subsystem:

1. **Every icon this project draws already funnels through one of two functions.**
   `FeSpriteTexture(short sprite_idx, ...)` / `FeGuiPanelTexture(short sprite_idx, ...)`
   (`frontgui_sprite_tex.cpp`) are the *only* way any ImGui-drawn HUD code turns a sprite index
   into a GPU texture — room/power/trap grid icons, creature hand-icons/portraits, ability icons,
   stat icons, tendency toggles, bar icons, tab headers, all of it. Each keeps a
   `std::map<short, CachedSprite>` (`s_button_cache` / `s_panel_cache`) and, on a cache miss, calls
   `resolve(idx)` (`get_button_sprite()` / `get_panel_sprite()`, `custom_sprites.{h,c}`, `kfx_render`)
   to get the legacy paletted `TbSprite*`, then `render_sprite()` rasterises it into a `TbPixel`
   (already plain RGBA8 — `struct TbPixel { uint8_t r,g,b,a; }`, `bflib_video.h`) buffer and uploads
   it via `RendererCreateDynamicTexture`/`UpdateDynamicTexture`. **One interception point** — the
   top of `lookup()` in `frontgui_sprite_tex.cpp` — sees every icon this project has ever drawn.
2. **The "scan a folder, offer what's found as a live settings-tab choice" pattern already
   exists and is proven**, for `UI_FONT` (`config_settingschema.c`): `ensure_ui_font_enum()` calls
   `PlatformManager_ListSubdirectories(prepare_file_path(FGrp_FxData, "font"), ...)` to list
   `fxdata/font/`'s sub-directories, builds a `NamedCommand` enum from them plus a couple of
   built-ins (`AUTO`, `CINZEL`), and `get_ui_font()`/`set_ui_font()` bridge the schema's enum-index
   UI to a plain string field (`keeperfx_ui_config.ui_font`) that's the literal folder name. This
   is *exactly* what "a new option on the GUI tab, enumerating folder names in `fxdata/gui/`"
   needs — no new UI code, no new config-value-kind, just a second copy of this pattern with a
   different scan directory and field name. The schema row itself needs no bespoke ImGui code
   either (same generic enum-cycle-button renderer `GUI_POSITION`/`UI_FONT` already use).

So the feature is really three small, independent pieces: (a) a naming/lookup table mapping
friendly names to what each icon category actually needs at runtime, (b) one interception point in
`frontgui_sprite_tex.cpp` that checks the current pack before falling back to the legacy path, and
(c) one more `ensure_*_enum()`-shaped settings row. Nothing here needs a new cross-`kfx_*` include —
`check_layering.py --strict` stays green the same way every prior phase kept it green.

---

## 1. Where the override folder sits, and how it's picked

`fxdata/gui/<pack-name>/*.png` — sibling to `fxdata/font/<family>/` (already the pattern for
`UI_FONT`). A new `GUI_ICON_PACK` setting (`config_keeperfx.c`/`.h`, `config_settingschema.c`),
built the same way `ui_font_enum`/`ensure_ui_font_enum()` are:

- `"NONE"` (vanilla — the legacy sprite atlas, no override lookups at all) is always entry 0,
  mirroring `UI_FONT`'s `"AUTO"`.
- One entry per sub-directory of `fxdata/gui/` (`PlatformManager_ListSubdirectories`), so e.g. an
  installed `fxdata/gui/dk1/` shows up as a `DK1` choice with zero extra wiring.
- Stored as a plain string (the folder name) in `keeperfx_ui_config`, same as `ui_font`.
- **Proposed: `SApply_NeedsRestart` for the first landing**, matching `UI_FONT`'s own comment
  ("swapping the live font atlas mid-session is a separate piece of work"). Live hot-swap would
  mean clearing `s_button_cache`/`s_panel_cache` (`frontgui_sprite_tex.cpp`) and the new override
  cache (§3) on setting change — not hard, but a second-pass refinement, not needed to ship this.

No new file-format, no manifest/JSON — a pack is *just* a folder of PNGs named per §2. A PNG that
doesn't match a known name is silently ignored (forwards-compatible: a future category's expected
name just won't resolve to anything on an older KeeperFX build).

---

## 2. Naming scheme

Per the user's spec, friendly names are always `<category>_<code_name>_active` /
`<category>_<code_name>_inactive`, where `<code_name>` is the icon's own stable, ASCII identifier —
**not** the (possibly-localised) display name, and not a `GPS_*` constant. Every category below
already carries exactly that identifier somewhere in `kfx_config`, used today for script/Lua
commands, so this needs no new data:

| Category | Source of `<code_name>` | Where it's drawn today |
| --- | --- | --- |
| `room_<code_name>_{active,inactive}` | `room_code_name(rkind)` (`config_terrain.h`) | `room_grid()`, `rs->medsym_sprite_idx` (`frontgui_ingame_grids.cpp`) |
| `power_<code_name>_{active,inactive}` | `PowerConfigStats::code_name` (`config_magic.h`) | `spell_grid()`, `ps->medsym_sprite_idx` |
| `trap_<code_name>_{active,inactive}` | `trap_code_name(tngmodel)` / `door_code_name(tngmodel)` (`config_trapdoor.h`) | `trap_grid()`, `md->medsym_sprite_idx` |
| `creature_icon_<code_name>` | `creature_code_name(crmodel)` (`config_creature.h`) | `get_creature_model_graphics(crmodel, CGI_HandSymbol)` -- the small per-row hand icon, `creature_list()`, and the battle-participant cells (`battler_cell()`, `frontgui_ingame_battle.cpp`) |
| `creature_portrait_<code_name>` | `creature_code_name(crmodel)` | `get_creature_model_graphics(crmodel, CGI_QuerySymbol)` -- the bigger query/possession portrait, `creature_query_panel()`, `creature_cell_horizontal()` |

`creature_icon_*` / `creature_portrait_*` have no `_active`/`_inactive` split (creature graphics
aren't dimmed/enabled the way build-grid icons are) — a single PNG per model, per the user's own
`creature_icon_*`, `creature_portrait_*` naming (no suffix).

**Decided: doors fold under `trap_`, not a separate `door_` prefix** — they render in the same
merged tab (`trap_grid()`, one `ManufactureData` list covering both `tngmodel` kinds) and the user
has confirmed that's the intent. Implementation detail, not a design question any more: per item,
call whichever of `trap_code_name()` / `door_code_name()` actually names that `tngmodel` (a
door-or-trap check already has to exist somewhere in `get_manufacture_data()`'s neighbourhood, or
gets added as a one-line helper) and use that string under the `trap_` prefix regardless of which
function produced it.

**A full audit of every `FeSpriteTexture()` / `FeGuiPanelTexture()` call site turned up more
categories than the first pass listed** — brought into first-landing scope rather than deferred,
per the user's direction. Two kinds:

**Data-driven** (code-name-keyed, same shape as the table above, resolved at each call site):

| Category | Source of `<code_name>` | Where |
| --- | --- | --- |
| `ability_<code_name>` | `creature_instance_code_name(inst_id)` (`config_creature.h`) | `instance_cell()` (`frontgui_ingame_creature.cpp`) -- possession/query ability grid, both layouts |

**Static** (a fixed, small, compile-time set — a hand-authored `{friendly_name, sprite_idx}` table,
no runtime code_name lookup needed at all, exactly §3.1's original shape):

| Category | Legacy sprite(s) | Where |
| --- | --- | --- |
| `job_idle` / `job_work` / `job_fight` | `GPS_rpanel_tab_crtr_wandr_act` / `_work_act` / `_fight_act` | the global pick-next header, both layouts (`creature_list()`'s 3 buttons, `creature_list_horizontal()`'s vertical icon column) |
| `tendency_imprison` / `tendency_flee` | `GPS_rpanel_tendency_prisnd_act`/`_prisnu_dis`, `GPS_rpanel_tendency_fleed_act` | `query_panel()` / `query_panel_horizontal()` |
| `stat_<name>` | the ~20 `k_stat_rows` entries (`frontgui_ingame_creature.cpp`) | the vertical layout's Stats page (`creature_query_panel()`) |
| `bar_payday` / `bar_research` / `bar_workshop` | `GPS_room_treasury_std_s` / `GPS_room_research_std_s` / `GPS_room_workshop_std_s` | `bar_row()` calls in both `query_panel()` and `query_panel_horizontal()` |
| `tab_room` / `tab_spell` / `tab_trap` / `tab_creature` | `GPS_plyrsym_symbol_room_red_std_a` (colorized) / `GPS_room_research_std_s` / `GPS_room_workshop_std_s` / `GPS_plyrsym_symbol_player_red_std_a` (colorized) | the 5 tab-header icons (`draw_tabs()`/`draw_tabs_horizontal()`, `frontgui_ingame_panel.cpp`) -- `tab_query`'s icon is a vector glyph ("?"), not a sprite, so it has nothing to override |
| `battle_vs` | `GBS_guisymbols_sym_fight` | the battle-participant box's "vs" divider (`frontgui_ingame_battle.cpp`) |
| `confirm_yes` / `confirm_no` | `GBS_options_button_smd_yes` / `_smd_no` | the quit-modal Yes/No buttons (`frontgui_ingame.cpp`) |
| `launcher_load` / `launcher_save` / `launcher_options` / `launcher_quit` | `GBS_options_button_load` / `_save` / `_graphc` / `_exit` | the pause-menu 4-button launcher (`frontgui_ingame.cpp`) |
| `message_<kind>` | `message_icon_spridx(i)` (`gui_msgs.h`, `kfx_game`) | `draw_message_queue()` (`frontgui_ingame_text.cpp`) -- one entry per `GuiMessage` kind |
| `event_<kind>` | `event_style(ev->kind).icon_spr` (`frontgui_ingame_panel.cpp`) | the 13 event markers, both layouts |

**Important consequence of intercepting at the sprite-index cache (§3): a handful of these names
alias the same underlying legacy sprite on purpose.** `tab_spell` and `bar_research` both draw
`GPS_room_research_std_s`; `tab_trap` and `bar_workshop` both draw `GPS_room_workshop_std_s`. This
isn't a bug to design around — it's the legacy atlas genuinely reusing one sprite in two UI spots —
but it means a pack can't give the *spell tab icon* a different look from the *research bar icon*
without a code change (they resolve to one cache entry). Proposed: document the alias pairs
explicitly in the override lookup table's own comment (so it's discoverable, not a silent surprise)
rather than trying to fake independent identities for sprites that are, in the legacy data, the same
picture.

---

## 3. Resolution: two lookup shapes, one cache

The full category list (§2) splits into two different "how do I find the right icon" problems,
because `FeGuiPanelTexture()`/`FeSpriteTexture()` are keyed by a `short` sprite index and that
index means something different depending on the category:

### 3.1 Static categories

`job_*`, `tendency_*`, `stat_*`, `bar_*`, `tab_*`, `battle_vs`, `confirm_*`, `launcher_*`,
`message_<kind>`, `event_<kind>` — every one of these already resolves to a fixed, compile-time
`GPS_*`/`GBS_*` sprite constant (or, for `message_<kind>`/`event_<kind>`, one entry per a small
fixed enum). One hand-authored table, `{ friendly_name, sprite_idx, is_button_sheet }`, built once
at compile time — no runtime code_name lookup, no per-frame cost beyond the same map lookup every
cached sprite already pays. This is the large majority of the category list by count, and the
simplest to implement.

### 3.2 Data-driven categories

`room_*`, `power_*`, `trap_*`, `creature_icon_*`, `creature_portrait_*`, `ability_*` — the sprite
index instead comes from a config struct field (`medsym_sprite_idx`, or
`get_creature_model_graphics()`'s return) that differs per install/mod, not from a compile-time
enum. `room_grid()` / `spell_grid()` / `trap_grid()` / `creature_list()` / `creature_query_panel()`
/ `instance_cell()` already walk exactly the config tables that know each item's `code_name`
(`get_room_kind_stats()`, `get_power_model_stats()`, `get_manufacture_data()`,
`get_creature_model_graphics()`, `creature_instance_code_name()`) — they just don't currently do
anything with it besides picking `medsym_sprite_idx`/`bigsym_sprite_idx`. The override lookup needs
to run **at the same call site**, before or alongside the existing
`build_icon(sid, (short)rs->medsym_sprite_idx, ...)` call: given `(category, code_name, active)`,
check whether the current `GUI_ICON_PACK` has a matching PNG loaded; if so, use its texture instead
of calling `FeGuiPanelTexture(medsym_sprite_idx, ...)`.

Concretely, this reshapes as a **new lookup function alongside the existing one**, not a change
inside `FeGuiPanelTexture()` itself (unlike the static case, the data-driven case needs the
category + code_name the grid code already has in hand, which `FeGuiPanelTexture(short)` never
sees) — something like:

```cpp
// Returns the override texture for (category, code_name, active), or nullptr if no pack is
// selected / this item isn't overridden in it -- callers fall back to the legacy sprite path.
void *FeIconOverrideTexture(const char *category, const char *code_name, bool active,
                            int *out_w, int *out_h);
```

`build_icon()` (`frontgui_ingame_grids.cpp`) and the creature-cell drawers become the callers:
try `FeIconOverrideTexture(...)` first, fall back to today's `FeGuiPanelTexture(spr, ...)` /
`blit_fit()` path unchanged when it returns `nullptr`. This keeps every existing call site's legacy
behaviour completely intact when no pack is active (`GUI_ICON_PACK == "NONE"`, the default) — the
override check is a cheap map lookup that returns `nullptr` immediately in that case.

### 3.3 The `_inactive` fallback contract

A pack author supplying only `<name>_active.png` (no `_inactive` file) shouldn't be forced to draw
a second frame for every item. **Proposed default:** if `_inactive` is missing, synthesize it by
alpha-dimming the `_active` texture at draw time — the exact tint `build_icon()`'s `o.dim` already
applies to the legacy sprite path (`FeHudCellOpts::dim`, `frontgui_ingame_cells.cpp`). Partial packs
(active-only) still look reasonable; a full pack can still supply a hand-drawn disabled frame if it
wants one (e.g. a greyscale variant rather than a flat alpha tint).

---

## 4. Decode + cache

PNG → texture needs to be much simpler than the codebase's existing PNG decoder
(`decode_png_to_sprite()`, `kfx_render/src/custom_sprites.c`) — that one quantizes into the legacy
**paletted** sprite format (`conversion_table`-matched) for the classic renderer + the existing mod
system's zip/JSON content-injection framework (`config_mods.c`'s `process_icon_from_list()` —
already visible in a live log as `Player_texture_pack` / `Creature_Portraits`). This feature wants
plain **RGBA8** — `spng` decoded straight into a `std::vector<TbPixel>` (already `{r,g,b,a}` byte
order) sized to the PNG's own dimensions, uploaded via the same
`RendererCreateDynamicTexture`/`UpdateDynamicTexture` pair `render_sprite()` already uses. `spng` is
already available to `kfx_frontend` with no new `CMakeLists.txt` linkage — it's pulled in via the
shared `kfx_common_opts` interface target every `kfx_*` library links, confirmed by `kfx_render`'s
own `custom_sprites.c` using `#include <spng.h>` with no per-library `target_link_libraries` line
for it.

**Deliberately not reusing the mods/`custom_sprites.c` machinery** — that system solves a different
problem (installable content packs adding *new* named sprites, zip-packaged, JSON-manifest-driven,
matched into the legacy palette) for a different audience. This feature is "skin the *existing*
ImGui HUD icons from a flat folder of same-named PNGs" — reusing the zip/JSON layer would be
solving a problem this feature doesn't have.

**Cache shape:** one `std::map<std::string, CachedSprite>` (friendly name → texture), populated
lazily on first use exactly like `s_button_cache`/`s_panel_cache` — scan-the-whole-folder-upfront
isn't necessary, only decode PNGs for icons actually drawn this session. Cleared (or simply left to
go stale, since `SApply_NeedsRestart` means the process restarts before it matters — §1) on
`GUI_ICON_PACK` change.

---

## 5. What this implies for the code (not this pass — for scoping only)

- `config_keeperfx.h`/`.c`: new `char gui_icon_pack[48]` field (or similar) alongside `ui_font`;
  `config_settingschema.c`: new `GUI_ICON_PACK` row, `ensure_gui_icon_pack_enum()` mirroring
  `ensure_ui_font_enum()` verbatim (scan `fxdata/gui/` via `PlatformManager_ListSubdirectories`
  instead of `fxdata/font/`), `get_gui_icon_pack()`/`set_gui_icon_pack()` bridging the same way.
- `frontgui_sprite_tex.{h,cpp}`: new RGBA-decode helper (spng, §4), the friendly-name cache (§4),
  and `FeIconOverrideTexture()` (§3.2) as the new lookup entry point alongside the existing
  `FeSpriteTexture()`/`FeGuiPanelTexture()`.
- `frontgui_ingame_grids.cpp`: `build_icon()`'s callers (`room_grid()`/`spell_grid()`/`trap_grid()`)
  gain the code_name lookup (`room_code_name()`/`PowerConfigStats::code_name`/
  `trap_code_name()`/`door_code_name()`) and the `FeIconOverrideTexture()`-first, legacy-fallback
  call shape.
- `frontgui_ingame_creature.cpp`: `creature_list()`'s portrait draw,
  `creature_query_panel()`/`creature_cell_horizontal()`'s portrait draws, and `instance_cell()`'s
  ability icon gain the same code_name lookup (`creature_code_name()` /
  `creature_instance_code_name()`) + override-first fallback.
- `frontgui_ingame_battle.cpp`: `battler_cell()`'s hand-icon draw and the static `battle_vs` row.
- `frontgui_ingame.cpp`: the static `confirm_yes`/`confirm_no`/`launcher_*` rows.
- `frontgui_ingame_text.cpp`: the static `message_<kind>` table.
- `frontgui_ingame_panel.cpp`: the static `job_*`/`tendency_*`/`bar_*`/`tab_*`/`event_<kind>` rows
  (`draw_tabs()`/`draw_tabs_horizontal()`, `draw_one_event_marker()`, both `query_panel*()`'s bar
  calls now live in `frontgui_ingame_creature.cpp` -- same static-table lookup, different file).
- The static table itself (§3.1) is one new small data file/array, not per-caller boilerplate --
  proposed home: a new `frontgui_ingame_icon_overrides.{h,cpp}` in `kfx_frontend`, holding both the
  static `{friendly_name, sprite_idx, sheet}` table and `FeIconOverrideTexture()` itself, so
  `frontgui_sprite_tex.cpp` stays focused on the texture cache/decode primitives it already owns
  and every call site above includes one new header instead of reaching into sprite-cache
  internals directly.
- No layering change anywhere in this list — every file above is already `kfx_frontend`;
  `config_settingschema.c` is already `kfx_config` and already calls
  `PlatformManager_ListSubdirectories` for the identical `UI_FONT` purpose.

---

## 6. Open questions (proposed default first, in each case)

1. **Filename case sensitivity.** Proposed: lower-case both the constructed lookup key and the
   on-disk filename before comparing, so `Room_Treasure_Active.PNG` and `room_treasure_active.png`
   both match — friendlier for pack authors than a strict case rule, and avoids a Windows/Linux
   filesystem case-sensitivity mismatch becoming a support question.
2. **A referenced PNG exists but fails to decode (corrupt file, unsupported PNG feature).**
   Proposed: `WARNLOG` once per bad file, fall back to the legacy icon for that one item — a bad
   file in a pack degrades gracefully instead of blocking every icon in that pack or crashing.
3. **Live hot-swap vs. restart-only.** Proposed restart-only for the first landing (§1) — matches
   `UI_FONT`'s own precedent and its stated reason (mid-session atlas swap is separate work); revisit
   if restart-only proves annoying in practice once this is live-tested.
4. **Should picking a pack validate anything at settings-render time** (e.g. show "12/47 icons
   overridden" in the tab), or stay silent like `UI_FONT` (which shows only the family name, no
   glyph-coverage feedback)? Proposed: stay silent for the first landing, matching `UI_FONT` —
   adding a coverage readout is a nice-to-have, not needed to ship the mechanism.
5. **§7's `background_*` split — one seamed image per layout-half, or one image sampled by UV
   across both fill calls?** See §7's own writeup; proposed default is the two-image seam approach.

Not answering these now — flagging them so implementation doesn't quietly bake in a default you'd
have picked differently, same as every prior phase doc in this series.

---

## 7. Background PNG override (more speculative — flagged separately)

**The ask:** let a pack replace the procedural marble-textured panel background
(`relief::face()`'s domain-warped-sine `mottle()` fill, `frontgui_ingame_relief.cpp`) with a single
static image, instead of (or as well as) the icon overrides above. This is a different *kind* of
override — a full-rect **surface** fill rather than a small **icon** — but rides the exact same
mechanism (same pack folder, same PNG→RGBA8 decode, same cache shape, same `GUI_ICON_PACK`
setting) with two new fixed friendly names and no code_name/data-driven complexity at all (there's
only one background per layout, not one per room/creature/etc.) — simpler than the icon categories
in that one specific sense, which is why it rides along here rather than needing its own phase.

**Where the fill actually happens — better-contained than it might look.** Audited every
`relief::face()` call site (the panel's "raised, mottled stone" fill — a vertical-gradient fill
plus the marble noise, `frontgui_ingame_relief.cpp:134`): there are exactly **three**, and they
compose cleanly:

- `draw_background_horizontal()` (`frontgui_ingame_panel.cpp:875`) — **one** `face()` call over the
  *entire* Bottom-layout strip rect. Simplest case: one `background_horizontal.png`, stretched to
  fill that one rect.
- `draw_background()` (`frontgui_ingame_panel.cpp:272`) — one `face()` call over the
  Vertical-Right/Left panel's *head* region only (minimap + gold + tab-strip band).
- `frontgui_ingame_tabcontent.cpp:97` — a **second**, independent `face()` call over that same
  vertical panel's *tab-content* region (the body below the head). Together these two calls cover
  the whole vertical sidebar; `relief::groove_h()` already draws an engraved divider line at
  exactly the seam between them (`draw_background()`'s own next line) — i.e. **the vertical layout
  already has a deliberate visual break at that exact spot**, so two independent images meeting
  there (rather than one image UV-sampled across both calls, which would need plumbing the *whole*
  panel height into `tabcontent.cpp`, which doesn't currently know it) reads as consistent with the
  existing design, not a compromise. Proposed: `background_vertical_head.png` +
  `background_vertical_body.png` as two separate images, stretched independently to their own
  rect — simplest to implement, and the existing groove already sells the seam.
- **Composited underneath everything else, unchanged.** `relief::edge_frame()` (the outer border)
  and every widget's own `well()`/`bevel()`/`plateau()` calls (tab strip, minimap frame, event
  marker tokens, nav-button pockets, every `fe_hud_cell()`) still draw **after** the background,
  exactly as today — a background image only replaces the flat "stone" fill underneath everything,
  never the structural chrome on top of it. The panel stays legible regardless of what the image
  looks like; a pack author supplies a flat backdrop, not a whole new panel skin.
- **Fallback:** no `background_*.png` in the active pack (including `"NONE"`) → today's
  `relief::face()` draws exactly as it does now. Purely additive, same as every icon row.
- **Scaling:** proposed stretch-to-fill only for the first landing (simplest, matches "background"
  intuition) — tiling is a plausible future option for a pack that wants a repeating pattern
  rather than one big image, not needed to ship this.
- **Quality note for pack authors** (not a code concern): a background image gets stretched across
  a potentially large rect at 4K, so a low-resolution source will look soft/blurry — worth a line
  in whatever authoring docs eventually accompany this feature, not something the code needs to
  guard against.

Flagged as its own section (not folded into §2's icon table) because it is a genuinely different
kind of override with its own open question (§6.5) about the vertical layout's two-image seam —
worth confirming that approach specifically before it's built, separately from the icon-category
work in §2–§5.
