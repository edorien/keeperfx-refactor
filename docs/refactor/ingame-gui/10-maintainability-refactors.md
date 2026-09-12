# Phase 9 — maintainability refactors (post first-pass)

Status: **§2–§8 landed 2026-09-08, §6/§7 landed 2026-09-11** (build-verified: game compiles clean
on linux + windows + ftest, layering clean, `gui_packet_parity` + `gui_seam_ingame` + full ftest
sweep pass). The in-game GUI → ImGui migration (Phases 0–8) is
functionally complete and has been through ~19 visual retest passes. This doc collects the
structural cleanup worth doing now that the shape has settled — no behaviour change, no new
features, just making the next round of work cheaper.

Ordered by the friction it actually caused during Phase 8's retest cycle. The user picked the
first four (`§3`, `§5`, `§2`, `§4`) to do now; the rest are logged for later.

---

## Doing now

### §3 — one deferred-action primitive

**Problem.** "Don't run heavy state transitions (`turn_on/off_menu`, `load_game`, recursive
packet sends) from inside an open ImGui window — enqueue and drain at the next frame's top" is
reimplemented **four times**, each slightly different:

| File | Shape |
|---|---|
| `frontgui_ingame.cpp` | `void(*s_pending)(void)` single slot + `request_deferred` / `apply_deferred` |
| `frontgui_ingame_battle.cpp` | `void(*s_pending_battle)(void)` single slot, hand-inlined drain |
| `frontgui_screens.cpp` | `void(*s_pending_action)(void)` + separate typed slots (`s_pending_state`, `s_pending_load_slot`, …) |
| `frontgui_ingame_panel.cpp` | `struct PendingBtn {fn(GuiButton*), btype_value, content_lval}` — reconstructs a stack `GuiButton` |

The single-slot ones also have a latent bug: two clicks in one frame → the first is silently
dropped.

**Fix.** A reusable `FeDeferredQueue` (small fixed-capacity `void(*)(void)[]` + `push` / `drain`)
in a new `frontgui_deferred.h`. Each module owns an *instance* instead of a hand-rolled slot
(drain timing differs per frame-owner, so not one global). The three `void()` cases
(`ingame` / `battle` / `screens`'s `s_pending_action`) adopt it; the per-file free functions
(`request_deferred` / `apply_deferred` etc.) stay as one-line wrappers so call sites don't move,
but the bug-prone part (drain semantics, capacity, lost-action-on-second-push) is now in one
place. `panel.cpp`'s typed `PendingBtn` (reconstructs a stack `GuiButton`) and `screens.cpp`'s
parameterised slots (`s_pending_state`, `s_pending_load_slot`) are left as-is — different payloads,
self-contained.

### §5 — named layout instead of `grid_pt(x, <magic y>)`

**Problem.** `frontgui_ingame_tabcontent.cpp` positions every sub-element with
`grid_pt(x, <hardcoded virtual y>)` — **46 such calls**, y-coords like `190`, `232`, `236`,
`254`, `258`, `278` scattered inline. Every "nudge the possession portrait" / "shift a row"
request in Phase 8 meant hand-editing a dozen of these and eyeballing for collisions.

**Fix.** A `frontgui_ingame_layout.h` with the tab-content sub-layout as named `constexpr`
constants in the shared 140×400 virtual space, grouped by panel body:

```cpp
namespace tcl {                       // tab-content layout (virtual 140x400)
    constexpr float GRID_X0 = 4,  GRID_X1 = 136;
    constexpr float INFO_Y0 = 196, INFO_Y1 = 240;   // room/spell/trap info strip
    constexpr float GRID_Y0 = 242, GRID_Y1 = 396;   // the icon grid
    namespace q {                     // creature-query / possession
        constexpr float HEADER_Y0 = 190, HEADER_Y1 = 243;
        constexpr float HEALTH_Y0 = 248, HEALTH_Y1 = 266;
        constexpr float DETAIL_TABS_Y = 270;
        constexpr float ABIL_ORG_Y   = 291;
        // 10:60:10:10:5:10:5 header split (§ retest 5) lives here too
    }
}
```

This is the **within-region** layout; `frontgui_hud_layout` (§ "Later" note below) stays the
**between-region** layout for the forthcoming horizontal / minimal variants. They compose: a
future layout hands `tabcontent` its region rect, `tcl::` places elements inside it.

### §2 — one value-noise implementation

**Problem.** The value-noise fbm behind the marble surface pass is copy-pasted: `m_hash` /
`m_vnoise` / `m_fbm` in `frontgui_ingame_relief.cpp`, `sb_hash` / `sb_vnoise` / `sb_fbm` in
`frontgui_widgets.cpp` (the main-menu list background).

**Fix.** A tiny `fe_noise.h` (header-only, `inline`): `fe_value_noise(x, y)` + `fe_fbm(x, y)`.
Both call sites keep their own marble *parameters* (frequency / amp / colours), just share the
noise.

### §4 — one HUD colour source

**Problem.** A reskin currently touches ~4 places: `relief::Tones`, the `COL_*` consts at the top
of `frontgui_ingame_tabcontent.cpp`, **~50 inline `IM_COL32(...)` literals** across
`panel` / `tabcontent` / `relief`, and `frontgui_style.cpp::apply_colours()`. The Phase 8 retest
loop was mostly colour tweaks, each spread across files.

**Fix.** Fold the HUD literals into `relief::Tones` (extend it with the handful of accent colours
— selection gold, hover red, have-one green, disabled grey, the bar fills) and expose named
accessors. `tabcontent.cpp`'s `COL_*` become aliases into it. `apply_colours()` (the ImGui
`style.Colors[]` for the frontend widgets) stays separate — different consumer — but can pull its
bronze/parchment values from the same ramp. Goal: the next reskin is one file.

---

### What landed (2026-09-08)

- **§3** — `frontgui_deferred.h` (`FeDeferredQueue`, 8-slot). Adopted by `frontgui_ingame.cpp`
  (`request_deferred`/`apply_deferred` → 1-line wrappers), `frontgui_ingame_battle.cpp`,
  `frontgui_screens.cpp` (`s_pending_action` → `s_deferred_action`). Fixes the "second push in a
  frame silently dropped" latent bug. `panel.cpp`'s `PendingBtn` + `screens.cpp`'s typed slots
  left as-is.
- **§5** — `frontgui_ingame_layout.h` (`namespace tcl`). The creature-query / possession panel
  vertical layout + the shared info-strip / grid / body boundaries are named constants now
  (`tcl::q::HEADER_Y0`, `tcl::INFO_Y0`, …). The room-query panel's toggle x-positions and the
  creature-list rows stay inline — stable, weren't the friction.
- **§2** — `fe_noise.h` (`fe::value_noise` / `fe::fbm`, inline). `frontgui_ingame_relief.cpp`'s
  `m_*` and `frontgui_widgets.cpp`'s `sb_*` deleted, both call it.
- **§4** — `relief::Accents` + `relief::accents()`. `tabcontent.cpp`'s `COL_*` are aliases into
  it; the inline bar-fill / hotkey / selection literals across `tabcontent.cpp` route through it
  (`bar_good` / `bar_warn` / `bar_bad` / `hotkey` / …). `panel.cpp`'s event-marker colour
  vocabulary is left for a later pass. Health red unified `200,60,50` → `220,60,50` (the "$"
  value) — the only pixel change.

**Blocked (not this work):** the concurrent merge `79b0e2096` (`origin/master`) removed
`struct TbNetworkPlayerInfo`, breaking `ftest_net_enet_loopback_{host,join}.c` — so the ftest
binary won't link and the sweep can't run. The same merge moved `minimap_zoom` from `PlayerInfo`
to `LocalInfo`; `frontgui_ingame_panel.cpp` was fixed for it in the merge but
`ftest_gui_packet_parity.c` (this project's) was missed — fixed here (`player->minimap_zoom` →
`local_info.minimap_zoom`). The net-test breakage is for whoever owns that merge.

## Later (logged, not scheduled)

- **Wire up `frontgui_hud_layout`.** *Not* dead code — it's the between-region layout descriptor
  for the horizontal (DK2-style) and minimal HUD variants ([05](05-sidebar-frame-and-minimap.md)
  §0). Currently has no consumers because Phase 4–8 shipped only `VerticalRight` against the raw
  `s_menu_rect`. Adopting it (region code calls `hud_region_rect()` instead of `s_menu_rect` +
  140-relative constants) is the prerequisite for those variants and should happen when the first
  one is built.
- (§6/§7/§8 assessed 2026-09-08 — see the next section.)
- **Schema dynamic-enum boilerplate** — `INGAME_RES` + `UI_FONT` each hand-roll `ensure_*_enum` +
  static tables + index↔`.num` translation. A `SettingEnumBuilder` helper.
- **Centralise per-`GMnu_*` migration gating** — adding a migrated menu edits `menu_is_migrated()`,
  `front_input.c` blocks, and `gui_frontmenu.c`. One registration table.
- **Retire dead legacy paths** — `update_*_tab_to_config` + `GMnu_ROOM2/SPELL2/TRAP2` (superseded
  by scroll); eventually the whole `frontmenu_ingame_*` sprite draw once `-classicmenu` is dropped.

---

## §6 / §7 / §8 — the bigger three (assessed 2026-09-08)

**Order: §8 → §6 → §7.** All three **landed** (§8 2026-09-08, §6/§7 2026-09-11). §8 was
independent and low-risk and closed a bug class. §6 *reduced* the code that §7 then split — doing
§7 first would have meant splitting duplicated code and then §6 touching three files instead of
one.

### §8 — scoped offscreen-capture RAII — **landed 2026-09-08**

`frontgui_offscreen.{h,cpp}` — `FeOffscreenTarget(buf, w, h, palette=nullptr)`: on construction,
optionally forces `palette`, then `LbScreenStoreGraphicsWindow` + `RendererSwapFramebufferTarget`
+ `LbScreenSetGraphicsWindow(0,0,w,h)`; the destructor restores all three in reverse. All **6
call sites** converted to `{ FeOffscreenTarget cap(...); legacy_draws(); }` + their own
`RendererUpdateDynamicTexture` after. `render_minimap` gains the graphics-window clip it was
missing (harmless — it now matches the buffer). `sprite_tex` / the two cursors keep their
`RendererSetDrawFlags`/`SetDrawColour` save-restore as 2 lines outside the scope (orthogonal
draw-state, not every site touches it); `sprite_tex`'s `engine_palette == nullptr` early-bail is
preserved. Builds (linux/windows/ftest) + layering + `gui_packet_parity`/`gui_seam_ingame` green.
Needs a screenshot check of minimap / parchment / cursor / land-preview.

**Was.** The "redirect the legacy raster at a local RGBA buffer, draw, restore" bracket appeared
at **6 call sites in 5 files**, each ~10 hand-written lines:

| Site | Palette forced |
|---|---|
| `frontgui_ingame_panel.cpp` `render_minimap` | `engine_palette` |
| `frontgui_ingame_parchment.cpp` `ingame_parchment_frame` | none |
| `frontgui_sprite_tex.cpp` (panel/button sprite → texture) | `engine_palette` |
| `frontgui_style.cpp` frontend cursor | `frontend_palette` |
| `frontgui_style.cpp` in-game cursor | none |
| `frontgui_screens.cpp` `draw_land_preview_panel` | none |

The bracket is: `LbScreenStoreGraphicsWindow` → (opt) `RendererPaletteGet`/`Set(pal)` →
`RendererSwapFramebufferTarget` → `LbScreenSetGraphicsWindow(0,0,w,h)` → *legacy draws* →
`RendererRestoreFramebufferTarget` → (opt) `RendererPaletteSet(prev)` → `LbScreenLoadGraphicsWindow`.
The palette-force half is exactly the step that caused the minimap-red bug (§ retest 2 of
[09](09-relief-and-emboss-pass.md)) — easy to omit, easy to forget the restore.

**Fix.** `frontgui_offscreen.{h,cpp}` (kfx_frontend):

```cpp
class FeOffscreenTarget {
public:
    // Redirect the legacy raster target at `buf` (w*h RGBA), set the
    // graphics window to (0,0,w,h). `palette != nullptr` forces it for the
    // scope. Everything restored on destruction, in reverse order.
    FeOffscreenTarget(TbPixel *buf, int w, int h, unsigned char *palette = nullptr);
    ~FeOffscreenTarget();
    FeOffscreenTarget(const FeOffscreenTarget &) = delete;
};
```

Each site becomes `{ FeOffscreenTarget cap(buf, w, h, engine_palette); legacy_draws(); }` +
its own `RendererUpdateDynamicTexture` after. **Effort:** ~1h. **Risk:** low — pure mechanical,
and the RAII *guarantees* the restore. **Verification:** minimap / parchment / cursor / land
preview all render (screenshot).

### §6 — one interactive-cell + one bar primitive — **landed 2026-09-11**

`frontgui_ingame_cells.{h,cpp}` gained `FeHudCellOpts`/`fe_hud_cell()` and `FeHudBarOpts`/
`fe_hud_bar()`, matching the design below with two differences found while wiring it up: `content`
is a `std::function<void(ImDrawList*, const ImVec2&, const ImVec2&)>` rather than the sketch's
"no content" mode, since a caller (`instance_cell`) needs its custom icon+bar drawn *between* the
well and the selected/hover ring, not before or after `fe_hud_cell` as a whole — a callback keeps
that z-order without duplicating the button/ring/tooltip skeleton. And `FeHudBarOpts` gained a
`fill_rounding` field: the pre-existing call sites disagreed on it (`prog_bar`/`vbar` rounded the
fill rect at 1.5px, `instance_cell`'s cooldown bar and `info_band`'s capacity bar left it square at
0) and unifying that as a side effect of the refactor would have been an uninvited visual change.
`spell_lost_panel`'s possess icon also uses the `content` callback rather than the plain `sprite`
field — its icon inset is 2px, not the 3px every other sprite-content cell uses, another
pre-existing difference kept intact rather than smoothed over.

All six cells now route through `fe_hud_cell` (`build_icon`/`sell_icon`/`unknown_cell` as ~5-line
wrappers in `grids.cpp`; `cell_button` similarly in `creature.cpp`; `instance_cell`/`stat_cell` use
the `content` callback / `sprite+text` mode respectively). `prog_bar`/`bar_row`/the `vbar`
lambda/`info_band`'s capacity bar/`instance_cell`'s cooldown bar all route through `fe_hud_bar`.
`sprite_toggle`/`glyph_toggle` (query panel tendency/query-mode toggles — pulsing gold border,
fill-colour state instead of a well) were deliberately left alone, as the doc originally scoped:
not part of the six-cell skeleton. Builds clean (linux/windows/ftest), layering clean,
`gui_packet_parity` + `gui_seam_ingame` pass. Screenshot retest of every tab body + possession
still needed to confirm no pixel drift in badge position / dim alpha / ring width.

**Where.** In `frontgui_ingame_tabcontent.cpp`: `build_icon`, `sell_icon`, `unknown_cell`,
`cell_button`, `instance_cell`, `stat_cell` all share the skeleton

> `SetCursorScreenPos` + `PushID` + `InvisibleButton` + (opt tooltip) + `PopID` +
> `relief::well` + *content* + (opt corner badge) + `if selected/hovered → AddRect` + return click

differing only in: rounding (2/3), content (aspect-fit sprite / big glyph / centred text /
sprite+sub-bar / sprite+value), corner badge (have-one dot / count / hotkey), selectable-or-swallow,
L-only vs L+R. And `prog_bar`, `bar_row`, the `vbar` lambda, the `info_band` capacity bar and the
`instance_cell` cooldown bar all share `relief::well` + inset fill + (opt) centred label.

**Fix.** In a new `frontgui_ingame_cells.{h,cpp}`:

```cpp
struct FeHudCellOpts {
    float rounding = 3.0f;
    bool  rclick = false;          // enable the right-click return value
    bool  swallow = false;         // unknown_cell: eat clicks, no visuals-change
    bool  selected = false;
    short sprite = 0;  bool dim = false;   // content: aspect-fit sprite ...
    const char *glyph = nullptr;           //   ... or a big vector glyph ...
    const char *text  = nullptr;           //   ... or centred text
    bool  have_dot = false;                // upper-left green dot
    int   count = -1;                      // >=0 -> lower-right badge
    const char *hotkey = nullptr;          // upper-left number
    const char *tooltip = nullptr;
};
int  fe_hud_cell(const char *id, const ImVec2 &p0, const ImVec2 &sz, const FeHudCellOpts &o); // 0/1/2

struct FeHudBarOpts { float rounding = 2.0f; unsigned int fill = 0; const char *label = nullptr; bool vertical = false; };
void fe_hud_bar(const ImVec2 &p0, const ImVec2 &p1, float frac, const FeHudBarOpts &o);
```

`build_icon` → 4 lines. `instance_cell` composes `fe_hud_cell` (no content) + manual sprite +
`fe_hud_bar`. `bar_row` stays a thin `[icon][gap][fe_hud_bar]` wrapper. `panel.cpp`'s
`tab_button` / `corner_nav` / event markers are a *different* look (rounded-top keys, triangular
pockets, RoundCornersRight tokens) — leave them; `fe_hud_cell` is the grid/query vocabulary.

**Effort:** ~half a day. **Risk:** medium — behaviour-preserving but every grid/query cell moves,
and the user is exacting about cell appearance (badge position, dim alpha, ring width). Needs the
screenshot retest of every tab body + possession. **Removes:** ~150–200 lines.

### §7 — split `frontgui_ingame_tabcontent.cpp` (1275 lines) — **landed 2026-09-11**

Split exactly along the table below, done together with §6 in one pass rather than as a separate
mechanical step afterward (§6 already required moving every cell function into its new home file,
so splitting the remaining grid/creature bodies at the same time avoided writing the same code
twice). `frontgui_ingame_tabcontent.cpp` is now ~110 lines: `ingame_tabcontent_draw`'s dispatch +
background fill, and the `FUNCTESTING` hook. One header per new `.cpp`
(`frontgui_ingame_grids.h`, `frontgui_ingame_creature.h`) declaring only what `tabcontent.cpp`'s
dispatch and FUNCTESTING hook need (`room_grid`/`spell_grid`/`trap_grid`/`do_sell_rooms`/
`do_sell_traps`; `creature_list`/`query_panel`/`creature_query_panel`/`spell_lost_panel`/
`creature_pick`) — everything else (`cell_button`, `sprite_toggle`, `glyph_toggle`, `bar_row`,
`instance_cell`, `stat_cell`, `info_band`, ...) stays `static`/anonymous-namespace in whichever of
the two files uses it, since nothing outside that file calls it. `GridGeom` lost its `ImVec2 cell`
member (became `cell_w`/`cell_h` floats) so `frontgui_ingame_cells.h` doesn't need to pull in
`<imgui.h>` just to define a struct — keeping that header on the `frontgui_widgets.h`-and-screen-
code-only convention for `imgui.h`. Reconfigured cmake on linux/ftest/windows for the 3 new
`.cpp`s; all three build clean, layering clean, `gui_packet_parity` + `gui_seam_ingame` pass.

Do **after** §6 (which shrinks it). Target split:

| New file | Contents | ~lines |
|---|---|---|
| `frontgui_ingame_cells.{h,cpp}` | `s_panel` + setter, `grid_pt`/`grid_sz`, `blit_fit`, `draw_big_glyph`, `fe_hud_cell`/`fe_hud_bar` (§6), `GridGeom` + `grid_begin`/`grid_cell_pos`/`grid_end`, `wrapped_tooltip` | ~260 |
| `frontgui_ingame_grids.cpp` | `room_grid` / `spell_grid` / `trap_grid`, `info_band`, `do_sell_*` | ~330 |
| `frontgui_ingame_creature.cpp` | `creature_list`, `query_panel` (top-down), `creature_query_panel` (+ possession), `instance_cell`, `stat_cell`, `spell_lost_panel`, `k_stat_rows`, `s_query_detail_tab` | ~520 |
| `frontgui_ingame_tabcontent.cpp` | just `ingame_tabcontent_draw` (the dispatch) + the `FUNCTESTING` hook | ~150 |

**Gotchas:** `s_panel` is a file-scope global read by `grid_pt` — moves to `cells.cpp` with a
`fe_hud_set_panel_rect()` setter that `ingame_tabcontent_draw` calls. `s_grid_base` moves with
`grid_begin`. The `ITTA_*` FUNCTESTING enum + `ingame_tabcontent_test_fire()` stay in
`tabcontent.cpp` but call into the grid/creature files. 3 new `.cpp` → `cmake` reconfigure on
ftest/linux/windows.

**Effort:** ~half a day. **Risk:** low-medium — pure code motion, but easy to drop a `static` or
an include; the compiler catches most of it. **Win:** navigation, and each file becomes reviewable.
