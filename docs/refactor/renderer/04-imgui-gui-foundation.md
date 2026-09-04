# Stage 4 — Dear ImGui for the main-menu frontend

Status: **planning, decision-complete, not started.** Depends on stage 2 (landed — `TbPixel` is
true-colour now). Does **not** depend on [03-gpu-renderer.md](03-gpu-renderer.md): the integration
point below works against today's `RendererSoftware` unchanged. §10 records every decision this
plan rests on; none are outstanding. See [00-overview.md](00-overview.md) for context.

> **Revision note (2026-09-04) — the recommendation in this document has been reversed.**
> The original pass recommended using Dear ImGui *only* as a batched-quad rendering backend behind
> `IUIRenderer`/`ITextRenderer`, with `kfx_frontend`'s hand-authored sprite chrome and
> `GuiButtonInit` arrays untouched, and explicitly listed "replace hand-rolled menu code with real
> ImGui widgets" under **Not recommended**. That disposition was written before stage 2 landed and
> before the GUI-side limits below were understood as structural rather than incidental. Four
> things changed; §1 records them. The new recommendation is the opposite one: **real ImGui
> widgets for the main-menu frontend, in-game GUI untouched.** The renderer-backend idea is not
> resurrected — it solved a problem (batching for the sprite path) that this plan removes instead.

---

## 1. Why the recommendation changed

### 1.1 The palette objection is gone

The original "not recommended" argument rested substantially on preserving paletted sprite chrome
exactly. Stage 2 shipped: `TbPixel` is a true-colour pixel, `lbDrawSurface` is RGBA32, and
`RendererSoftware::PresentFrame()` uploads it directly with no INDEX8 conversion
(`RendererSoftware.cpp:121-139`). Alpha compositing, gradients, rounded corners and drop shadows —
every item [gui/02-menu-v2-mockup-gap-analysis.md](../gui/02-menu-v2-mockup-gap-analysis.md) §1
called "expensive, general-purpose" — are now ordinary operations rather than palette-remap tricks.
The mitigation described in [gui/colordepth/00-notes.md](../gui/colordepth/00-notes.md) (nearest-
colour remapping land art into `front.pal`) is retired by the same change; land previews can be
composited at full fidelity.

### 1.2 "Preserve the existing chrome exactly" was never actually available

`frontend_draw_scroll_box` / `frontend_draw_scroll_box_tab` (`gui_frontbtns.c`) derive **both**
their border scale and their per-row height from `gbtn->width` alone, because a row is a fixed
sequence of six *distinct* sprites (corner, four different decorative segments, corner) uniformly
scaled to a target width. There is no repeatable tile, so width and height cannot vary
independently, and no narrower-than-~450px working instance exists anywhere in the codebase to
copy. The Land-selection screen had to abandon the control entirely and use a flat black panel
(`frontend_draw_land_selection_panel_bg`, `frontmenu_select.c`) — recorded in
[gui/colordepth/00-notes.md](../gui/colordepth/00-notes.md) and
[gui/04-phase2-landview-panel-investigation.md](../gui/04-phase2-landview-panel-investigation.md).

**The limitation is in the code, not in the art.** Checking the source PNGs those sprites are built
from (§5.1) settles this: `hugearea_thn_*` is a complete, properly-cut 9-slice set — four corners
plus `cor_ml`/`cor_mr` side pieces at 9x22, `tx1_tc`/`tx1_bc` top and bottom bands at 108x13, and
`tx1_mc` fill at 108x22, with `tx2`/`tx3`/`tx4` as decorative *variants* of the middle band rather
than four mandatory segments in sequence. The art has always supported independent width and
height; `frontend_draw_scroll_box` simply doesn't, because it scales a fixed row by `gbtn->width`.

That reframes the disposition. It is not "commission new art either way" — it is that the existing
art is already the right shape and only the drawing code stands between it and arbitrary panel
geometry. Rewriting that drawing code inside the current sprite/`GuiButtonInit` system would mean
building a 9-slice compositor, a layout pass, hit-testing and focus handling by hand; doing it
against a draw list means nine `AddImage` calls and getting layout, hit-testing, scrolling and focus
from a library.

### 1.3 The bitmap fonts do not scale

`frontend_font[0..3]` are `TbSpriteSheet*` bitmap fonts loaded from `ldata/frontft{1..4}.dat`
(`vidmode.c:150-158`), authored for 640×400/640×480. Frontend layout scales by
`units_per_pixel_menu` (`vidmode.c:781-783`, `height/30` against a 640×480 reference), so at 1440p
or 4K the glyphs are magnified bitmaps — no hinting, no true intermediate sizes, no italics, and
nothing between the four authored faces. The gap analysis already flagged typography as the
**blocking** decision for the whole menu-v2 effort ("Open questions for the user" — bitmap sheets
vs. a real font renderer). This plan answers it: real font rasterization, frontend-only, in-game
HUD text untouched.

### 1.4 There is a clean, pre-existing seam for "main menu only"

The scope the original doc treated as all-or-nothing turns out to be already separated in code:

- The frontend runs its **own loop**, `wait_at_frontend()` (`game_session_loop.cpp:648`), distinct
  from `keeper_gameplay_loop()`. Input, update, draw and present for the menus are one contained
  block (`game_session_loop.cpp:780-838`).
- `frontend_draw()` (`frontend.cpp:3423`) dispatches per `FrontendMenuState`, and one arm of its
  switch — `frontend_copy_background(); draw_gui();` — covers exactly the menu screens in scope.
- Crucially, `draw_gui()` itself is **shared** with the in-game HUD (`engine_redraw.c:555,613,635`
  and `gui_parchment.c:951,963`), so migration must be **per-menu-state**, never by replacing
  `draw_gui()`. The `frontend_draw()` switch is the discriminator, and it makes the two systems
  coexisting during migration a per-state opt-in rather than a flag day.

---

## 2. Scope

### 2.1 In scope — the 15 frontend `GuiMenu`s

| Menu | State | Defined in |
| --- | --- | --- |
| `frontend_main_menu` | `FeSt_MAIN_MENU` | `frontend.cpp:137,182` |
| `frontend_option_menu` | `FeSt_FEOPTIONS` | `frontmenu_options_data.cpp:68,94` |
| `frontend_define_keys_menu` | `FeSt_FEDEFINE_KEYS` | `frontmenu_options_data.cpp:48,92` |
| `frontend_load_menu` | `FeSt_FELOAD_GAME` | `frontmenu_saves_data.cpp` |
| `frontend_high_score_table_menu` | `FeSt_HIGH_SCORES` | `frontend.cpp:163,186` |
| `frontend_statistics_menu` | `FeSt_LEVEL_STATS` | `frontend.cpp:151,184` |
| `frontend_error_box` | (overlay) | `frontend.cpp:175,188` |
| `frontend_select_campaign_menu` | `FeSt_CAMPAIGN_SELECT` | `frontmenu_select_data.cpp` |
| `frontend_select_mappack_menu` | `FeSt_MAPPACK_SELECT` | `frontmenu_select_data.cpp` |
| `frontend_select_level_menu` | `FeSt_LEVEL_SELECT` | `frontmenu_select_data.cpp` |
| `frontend_select_mp_mappack_menu` | `FeSt_MP_MAPPACK_SELECT` | `frontmenu_select_data.cpp` |
| `frontend_net_service_menu` | `FeSt_NET_SERVICE` | `frontmenu_net_data.cpp` |
| `frontend_net_session_menu` | `FeSt_NET_SESSION` | `frontmenu_net_data.cpp` |
| `frontend_net_start_menu` | `FeSt_NET_START` | `frontmenu_net_data.cpp` |
| `frontend_add_session_box` | (overlay) | `frontmenu_net_data.cpp` |

### 2.2 Also in scope — the "backdrop plus text" screens

Several `FeSt_*` states are not `GuiMenu`s at all but are structurally trivial: they call
`frontend_copy_background()` and then draw text over it. They belong in this migration — they are
the cheapest possible proving ground for the font stack, and leaving them on the bitmap path would
mean the same screen shows crisp text in one state and magnified bitmap text in the next.

| State | Function | What it actually draws |
| --- | --- | --- |
| `FeSt_STORY_POEM` | `frontstory_draw` (`front_credits.c:81`) | backdrop + one centred string in a 70px-inset text window, `frontstory_font` |
| `FeSt_STORY_BIRTHDAY` | `frontbirthday_draw` (`front_easter.c:97`) | backdrop + two centred lines |
| `FeSt_CREDITS` | `frontcredits_draw` (`front_credits.c:96`) | backdrop + a vertically scrolling list built from `campaign.credits[]`, switching between `frontend_font[]` faces per item |

The same treatment covers `draw_defining_a_key_box()` (`frontend.cpp:3177`), which is
`draw_text_box(get_string(GUIStr_PressAKey))` — a modal text box over whatever is behind it. It
migrates with `FeSt_FEDEFINE_KEYS` in Phase D.

`FeSt_CREDITS` is the most interesting of the three: it is a scrolling text region with mixed
faces, which is precisely what an ImGui scroll region plus the §5 wrappers does well, and it
exercises the multi-face font stack before any settings screen depends on it.

### 2.3 Explicitly out of scope

- **All in-game GUI**: `main_menu`, `room_menu`, `spell_menu`, `spell_lost_menu`, `trap_menu`,
  `creature_menu`, `query_menu`, `event_menu`, `options_menu`, `instance_menu`, `quit_menu`,
  `error_box`, `autopilot_menu`, `video_menu`, `sound_menu`, `message_box`, `load_menu`,
  `save_menu`, `text_info_menu`, `battle_menu` (`frontmenu_ingame_*_data.cpp`), plus
  `gui_boxmenu.c`, `gui_parchment.c`, `gui_tooltips.c`, `frontmenu_ingame_tabs.c`. These keep the
  sprite path, `draw_gui()`, and `get_gui_inputs()` exactly as they are.

  A known and accepted consequence: the in-game pause options menu (`GMnu_OPTIONS`) will look and
  behave differently from the new frontend settings screen — two visually unrelated settings UIs in
  one game. Accepted for now. The in-game GUI is a **separate, longer-term project** needing more
  careful thought than a port: it is gameplay-critical, latency-sensitive, sits over live 3D
  content, and its panel/tab chrome is far more entangled with game state than any frontend screen.
  It should be revisited **after the GPU work** ([03-gpu-renderer.md](03-gpu-renderer.md)) lands,
  not before, and it is explicitly not scoped by this document.
- **Video playback and animated set-pieces**: `FeSt_INTRO`, `FeSt_DEMO`, `FeSt_OUTRO`,
  `FeSt_DRAG`, `FeSt_CAMPAIGN_INTRO` (Smacker playback), and `FeSt_TORTURE` (an interactive
  animated screen, not a menu).
- **Splash/loading raw bitmaps** (`front_simple.c`'s `bitmaps_*` tables).
- **In-engine HUD/bitmap text** — `bflib_sprfnt` and the `font_sprites`/`winfont` path stay.

### 2.4 Land view — low priority, deferred past Phase F

`FeSt_LAND_VIEW` / `FeSt_NETLAND_VIEW` (`frontmap_draw`, `front_landview.c`) are a full-screen map
image with hotspot buttons — frontend screens, but almost none of their pixels are chrome.

**`FeSt_LAND_VIEW` is now mostly dead** following the merged campaign-selection screen work
(`docs/refactor/gui/04-phase2-landview-panel-investigation.md`, commits through `1c11001ca`): the
campaign list, land preview and description it used to be the only home for now live on the merged
select screen. It remains reachable — campaign intro redirects into it (`front_fmvids.c:138,143`)
and returning from a singleplayer level lands there (`frontend.cpp:3734`) — but it is no longer
where the interesting content is, so it is **explicitly low priority**: schedule it after Phase F,
or leave it on the sprite path indefinitely if nothing forces the issue.

**`FeSt_NETLAND_VIEW` is planned to receive the same merged-screen treatment** its singleplayer
counterpart already got. That is a separate piece of frontend work, not part of this stage; when it
happens, the resulting screen should be authored directly in ImGui rather than built on the sprite
path and migrated twice. Sequence the two so that work lands after Phase B (wrappers available) —
otherwise it will be built against chrome this stage is retiring.

---

## 3. Integration architecture

### 3.1 Where the code lives (layering)

`scripts/check_layering.py` governs `src/kfx_*` inter-library includes only; third-party headers
are unconstrained. The split that respects the ladder:

- **`kfx_platform`** owns the ImGui *context*: creating/destroying it, `ImGui_ImplSDL3_*` and
  `ImGui_ImplSDLRenderer3_*` lifecycle, feeding SDL events, new-frame/render. It already owns the
  `SDL_Window` (`WindowSystemSDL`) and the `SDL_Renderer` (`RendererSoftware::m_renderer`), which
  are exactly what both backends need. Expose a narrow `extern "C"`-friendly service alongside
  `RendererManager` — e.g. `src/kfx_platform/{include,src}/gui/ImGuiContext.{h,cpp}` — so nothing
  above has to `#include <imgui.h>` for lifecycle.
- **`kfx_frontend`** owns the *screens* and the §5 wrapper layer: it includes `imgui.h` and submits
  widgets. This is upward-legal (frontend ranks above platform) and keeps menu authoring in the
  library that already owns menu behaviour.
- **`kfx_apploop`** (`wait_at_frontend`) drives the per-frame ordering.

### 3.2 Vendoring

Follow the existing in-tree source-dep convention (`deps/centitoml`, `deps/CUnit-2.1-3` are
git-tracked; downloaded binary deps are gitignored). Add `deps/imgui/` with:

`imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `imgui.h`,
`imgui_internal.h`, `imconfig.h`, the three `imstb_*.h`, plus `backends/imgui_impl_sdl3.{cpp,h}`
and `backends/imgui_impl_sdlrenderer3.{cpp,h}`. MIT licensed, consistent with existing third-party
posture. `imgui_demo.cpp` is worth vendoring too — it costs nothing in a release build if not
referenced, and is the fastest way to validate the backend wiring in Phase A.

**Vendor from upstream `ocornut/imgui`, not from the `pthom/imgui` fork carried by the local
`imgui_bundle` checkout.** This was checked, not assumed: that fork's `imgui.h` carries
`#include "imgui_stacklayout.h"` at line 4758 and ships `imgui_stacklayout.{cpp,h,_internal.h}`
alongside the core files — a real fork patch, not a clean mirror. The rest of the original doc's
findings on `imgui_bundle` stand and still apply: do not use `imgui_bundle`'s CMake target,
`hello_imgui` or `immapp` (its runner layer is SDL2-only — there is no `runner_sdl3.cpp` — while
KeeperFX is SDL3 throughout).

Choose `imgui_impl_sdlrenderer3` over `imgui_impl_opengl3` for the same reason as before: it
targets the same `SDL_Renderer` the software backend already presents through, so there is one
graphics context, one present call, and no second API to stand up. If stage 3 later moves game
content onto the GPU, this backend composites into the same renderer with no rework.

The local checkout is Dear ImGui **1.92.7 WIP** (`IMGUI_VERSION_NUM 19263`) and defines
`IMGUI_HAS_TEXTURES` — the dynamic font/texture system introduced at `19198`. Pin at or above that;
§4 depends on it.

### 3.3 Per-frame wiring

Three touch points, all small:

1. **Events** — `LbPollInputs()` (`bflib_inputctrl.cpp:526-534`) drains `SDL_PollEvent` into
   `process_event()`. Add `ImGui_ImplSDL3_ProcessEvent(&ev)` in that loop, gated on the ImGui
   context existing. While an ImGui-owned frontend state is active, `io.WantCaptureMouse` /
   `io.WantCaptureKeyboard` gate the legacy `get_gui_inputs(0)` path so the two systems never both
   claim a click.
2. **Submission** — in `frontend_draw()`, the migrated states call
   `ImGuiFrontendNewFrame()` + the screen's submit function instead of
   `frontend_copy_background(); draw_gui();`. Un-migrated states are byte-for-byte unchanged.
3. **Present** — in `RendererSoftware::PresentFrame()`, between `SDL_RenderTexture(...)` and
   `SDL_RenderPresent(...)` (`RendererSoftware.cpp:136-137`), render the ImGui draw data. The
   software framebuffer therefore remains the backdrop layer and ImGui is a true overlay — which is
   why this stage needs nothing from stage 3.

Set `io.MouseDrawCursor = false`: the game already draws its own pointer sprite around the swap
(`LbMouseOnBeginSwap`/`LbMouseOnEndSwap`, `RendererSoftware.cpp:125,138`) and that behaviour is
kept.

### 3.4 Backdrop

Simplest correct option for Phase A–C: keep `frontend_copy_background()` drawing the existing
full-screen backdrop through the software path (already true-colour) and let ImGui draw on top.
Uploading backdrops as ImGui textures becomes worthwhile only when a screen needs the backdrop
sampled/tinted per-widget; defer it.

### 3.5 Runtime toggle — both paths ship, ImGui on by default

**The legacy sprite path is not deleted while this stage is in progress.** Every migrated screen
keeps its existing `GuiButtonInit` array and draw callbacks intact, and a runtime switch chooses
which one runs. This is what makes the migration safe to ship incrementally: any screen that turns
out to have a regression can be worked around by the user with one flag while it is fixed, and any
bug report can be bisected to "does it also happen on the classic menu?" in one run.

- **Flag**: a new command-line parameter parsed in `process_command_line()`
  (`main.cpp:1669-1940`), following the existing `strcasecmp(parstr, ...)` pattern — `-classicmenu`
  (with `-noimgui` as an accepted alias) forces the legacy path. Default is ImGui **on**.
- **Runtime, not compile-time.** Both paths must be present in one binary. Do not put this behind
  `#ifdef` — the point is that a user with a shipped release can flip it, and that both paths stay
  compiled and therefore stay compiling.
- **Also a config key**, so it survives without editing a shortcut: add it to `conf_commands[]`
  (`config_keeperfx.c:146-194`) and use the established precedence mechanism — command line beats
  config, tracked via `start_params.overrides[Clo_*]` (`config_keeperfx.h:44-49`,
  `config_keeperfx.c:923,961,1205`). This adds one entry to `enum CmdLineOverrides` and requires
  bumping `CMDLINE_OVERRIDES` (currently 4, `config_keeperfx.h:41`).
- **Resolution of the flag is per-screen, not global.** The effective decision is
  *"ImGui enabled AND this state has been migrated"*. Un-migrated states ignore the flag entirely,
  which means the flag is meaningful from Phase C onward and harmless before it.
- **The toggle is temporary.** It exists while issues are worked out. Retiring it — and with it
  the legacy frontend menu code — is a deliberate later decision, made once every screen in §2 has
  shipped and stabilised, not an automatic consequence of Phase F. Record that decision here when
  it is taken.

---

## 4. Typography

### 4.1 Fonts: Exocet installed into `fxdata/`, Cinzel bundled as fallback

The named file is `EXL_____.TTF` in the DK2 Windows theme. Reading its `name` table directly:

> `Copyright (C) 1992 Emigre Graphics, Designed by Jonathan Barnbrook` — `Exocet Light`

Its sibling `EXH_____.TTF` is `Exocet Heavy`, same copyright. **Exocet is a commercial Emigre
retail typeface.** KeeperFX is GPL v2 and its releases are redistributed as packages, so the font
cannot be bundled in `dist/` or the CPack package, and it must not enter this repository — that
constraint is absolute regardless of anything else.

It does not, however, prevent *using* it. KeeperFX already requires the user to supply the original
Dungeon Keeper data files, which are equally proprietary and equally not shipped; the fonts join
that same list of user-supplied assets, copied out of the user's own DK/DK2 installation. So:

- **Install-time copy into `fxdata/`.** The installer/launcher copies `EXL_____.TTF` and
  `EXH_____.TTF` into the game's `fxdata/` directory alongside `gamecontrollerdb.txt` and the
  `.cfg`/`.toml` data, exactly as it already copies the DK data files into place. The engine then
  resolves them through the existing path helper — `prepare_file_path(FGrp_FxData, "EXL_____.TTF")`
  (`globals.h:362-384`'s `TbFileGroups`, the same mechanism `custom_sprites.c` and `lua_base.c`
  already use) — which means no new search logic, no install-path probing, and mod/override
  directories work for free via `prepare_file_path_mod`.
- **Fallback, bundled: Cinzel** (SIL OFL, redistributable under GPL packaging) — shipped into the
  same `fxdata/` directory and used whenever the Exocet files are absent. Chosen for its carved
  Roman-capital character and, decisively, for having a weight range: the design needs a display and
  a heading weight, which is what Exocet Light/Heavy provides. The engine's behaviour is a simple
  present/absent check at font-load time, with the result logged.

Two consequences to handle:

1. **The fallback is likely the common case.** These TTFs ship in *"Dungeon Keeper Extras/theme/
   DK2 Theme"* — a DK2-era extras bundle, **not** the base DK install KeeperFX already requires. A
   user with a working setup does not necessarily have them, and the launcher's copy step will
   often find nothing. So the bundled face is a first-class design target, not a degraded mode, and
   §5's style must look deliberate with either. Metrics differ between the two, so the wrappers'
   padding and line-height must derive from the loaded font's metrics rather than hardcoded pixels.
2. **The packaging globs need a `.ttf` rule.** `build/make/package.mk:31-35` stages `config/fxdata/`
   by extension (`*.cfg`, `*.toml`, `*.txt`, `lua/*.lua`) — the bundled fallback font would be
   silently omitted today. One rule to add, in `package.mk` and correspondingly in
   `Packaging.cmake`'s `gamedata` component.

### 4.2 CJK and Cyrillic coverage is a solved problem here

`lang/` carries `chi`, `cht`, `jpn`, `kor`, `rus`, `ukr` among others, and `bflib_sprfnt.c` already
maps codepoints through a `codepage_map` (`bflib_sprfnt.c:1414-1471`). Neither Exocet nor a Latin
display face covers any of that. Fortunately the answer already exists in-tree:
`tools/fxfontmaker/` (`make_fonts.bat`) builds today's CJK bitmap fonts from **GNU Unifont** and
**WenQuanYi** — both freely licensed and both available as TTF/OTF. Merge them as ImGui fallback
fonts behind the display/body faces (`ImFontConfig::MergeMode`), so the aesthetic face handles
Latin and the fallback handles everything else. No new licensing question, and it is the same
provenance the current fonts already have.

ImGui is UTF-8 native throughout, which matches what `get_string()` already returns.

### 4.3 Sizing

1.92's dynamic font system (`IMGUI_HAS_TEXTURES`) rasterizes glyphs on demand at the requested
size rather than requiring a pre-baked atlas per size. Two consequences worth designing around:

- Derive the frontend's base text size from actual window height (the same input
  `units_per_pixel_menu` uses, `vidmode.c:781`) and re-derive on resolution change, instead of
  magnifying a 640×480-authored bitmap. This is the direct fix for "the bitmapped fonts don't scale
  well to higher resolutions."
- Large CJK ranges no longer need to be baked up front, so the memory objection to a full-coverage
  frontend font disappears.

Text *layout* should use ImGui's own metrics; do not route ImGui text through `bflib_sprfnt`'s
`units_per_px` model. The two scaling systems coexist because they never share a screen.

---

## 5. Chrome, and the widget wrapper layer

### 5.1 Chrome: reuse the existing source art, don't commission new

**Decision: responsive layout, not a scaled 4:3 virtual canvas.** Screens lay out against the real
window and use the full widescreen area, with a max content width so ultrawide displays don't
stretch a two-column screen into unreadability. Every wrapper's sizing contract in §5.2 follows
from this: sizes derive from font metrics and available space, never from 640x480-relative
constants.

The brief allows departing from both the 1997 design and the repo's current variant. It turns out
less of that permission needs spending than expected, because the chrome already exists as source
art. `FXGraphics-main/menufx/frontend-64/` (a local working copy of `dkfans/FXGraphics`, GPL v3 —
the same repo CI already clones for `make pkg-enginegfx`; gitignored like the other game-data
folders) holds the raw PNGs every frontend sprite is built from, and they map almost one-to-one
onto §5.2's wrapper list:

| Source PNGs | Wrapper |
| --- | --- |
| `hugearea_thn_*` / `hugearea_thc_*` — corners, side pieces, top/bottom bands, four fill variants | `FeBeginPanel`, `FeBeginListBox` |
| `largearea_nx1/nx2/xts_*` — `cor_l`, `cor_r`, `tx1..tx5_c` | inset/label areas |
| `hugebutton_a01..a05_{l,c,r}`, `largebutton_a01..a05_{l,r}` | `FeButton`, `FeNavButton` (5 states each, already a 3-slice) |
| `scrollbar_{top,btm}arrow_{std,act}`, `scrollbar_indicator_{std,act}`, `scrollbar_vert_ct_{short,long}` | `FeBeginScrollArea`'s scrollbar |
| `slider_horiz_{l,c,r}`, `slider_indicator_{std,act}`, `slidrect_indicator_{std,act}` | `FeSlider` |
| `specicon_{music,sound,voice}` | the settings screen's category icons |
| `front_background-{64,128,256}` | backdrop |

So the plan for chrome is **reuse and recompose**, not commission:

- **Panels and lists**: draw the existing 9-slice pieces via `ImDrawList::AddImage` at whatever
  independent width and height the layout asks for — the thing §1.2 showed the art already supports
  and only the old drawing code prevented. Use the `tx1..tx4` variants for decorative variety along
  long edges rather than treating them as a required sequence.
- **Buttons, sliders, scrollbars**: the five-state button sets and the complete slider/scrollbar
  sets transfer directly.
- **Procedural where no art exists**: translucent card fills, gradients, hover tints, drop shadows —
  `AddRectFilled`, `AddRectFilledMultiColor`, layered translucent rects. Source the palette from
  `front.pal` so the colour identity is inherited. This is what
  [gui/02-menu-v2-mockup-gap-analysis.md](../gui/02-menu-v2-mockup-gap-analysis.md) §1 called
  "expensive, general-purpose"; stage 2 made it free, and that doc's "bake gradients into the
  backdrop art" recommendation should be considered withdrawn.

**One honest limitation, accepted.** The chrome art exists only at the `-64` scale —
`frontend-64/` has no `-128`/`-256` sibling, unlike `gui1`/`gui2` (which go to `-256`) and unlike
the backdrop itself (`front_background-64/128/256` = 640x480 / 1280x960 / 2560x1920). So panel and
button ornament is upscaled at high resolutions, roughly 6x at 4K. This is much less damaging than
it is for text — edges, corners and flat bands upscale acceptably with linear filtering, where
glyphs do not — but it is the same resolution ceiling in a quieter place.

**Decision: ship the low-resolution chrome as-is.** Use linear filtering, and draw ornament at
modest thickness rather than scaling it proportionally with the panel. A general asset-improvement
pass is separately planned; higher-resolution chrome belongs to that effort, not to this stage. Do
not block, redraw, or commission anything here on account of the ceiling — Phase B's style-sheet
screen at 4K is worth looking at for information, not as a gate.

New art genuinely needed is therefore small: the main-menu wordmark (Phase F), and anything the
redesigned screens introduce that has no existing counterpart. Placeholder treatment (procedural
panels, text wordmark) is fine for Phases B-E.

### 5.2 Screens do not call ImGui directly

**Every panel, list, scroll region, button, slider and text style gets a KeeperFX wrapper, and
screen code calls only wrappers.** This is a hard rule, not a preference, and it is the single
most important structural decision in the plan after the scope boundary. Without it, styling
decisions leak into fifteen screens as ad-hoc `PushStyleVar`/`PushStyleColor` pairs and the result
drifts exactly the way the current `GuiButtonInit` arrays drifted — inconsistent padding, per-screen
one-offs, and no way to restyle anything centrally.

Proposed home: `src/kfx_frontend/{include,src}/frontgui_widgets.{h,cpp}` — one wrapper module,
`imgui.h` included there and in the screens, nowhere else in `kfx_frontend`.

The wrapper set, roughly:

| Wrapper | Replaces / covers |
| --- | --- |
| `FeBeginPanel` / `FeEndPanel` | bordered content panel, ornament corners, title bar; the successor to `frontend_draw_scroll_box` |
| `FeBeginListBox` / `FeEndListBox`, `FeListRow` | scrollable selection lists — the `FrontendSelectList` screens and the key-remap rows |
| `FeBeginScrollArea` / `FeEndScrollArea` | scrolling text or content with the styled scrollbar |
| `FeButton`, `FeIconButton`, `FeNavButton` | the large/small menu button families in `gui_frontbtns.c` |
| `FeSlider`, `FeCheckbox`, `FeCombo`, `FeTextInput`, `FeKeybindRow` | settings controls; the §6.3 schema renderer is built entirely from these |
| `FeHeading`, `FeSubheading`, `FeBodyText`, `FeCaption`, `FeSeparator` | the type scale, so font-role choices live in one file |
| `FeBeginTabBar` / `FeTab` | the settings screen's Game/Graphics/Sound/Input tabs (§10) |
| `FeBeginModal` / `FeEndModal` | error box, add-session box, "press a key" |

Rules the wrappers enforce, each of which is a bug class that otherwise recurs per screen:

- **Style push/pop is owned by the wrapper**, always balanced, never left to the caller.
- **Sound feedback lives here.** Menu hover and click sounds are behaviour, not decoration, and
  wiring them once inside `FeButton`/`FeListRow` is the difference between menus that feel right
  and fifteen screens that each forgot a different one.
- **Focus and navigation defaults** — nav flags, default-focused item, Esc-to-back — are set
  consistently rather than per screen. Gamepad play is **not** a target: no screen is designed
  around it, and nothing is held back waiting for it. But the wrappers still set ImGui's nav flags
  uniformly, because consistency is the point of the layer and because uniform focus handling is
  what keyboard navigation needs anyway. Whatever gamepad behaviour falls out of that is a
  by-product, not a supported feature — do not add gamepad-specific screens, prompts or affordances.
- **Sizing derives from font metrics and window scale**, not literal pixels, so §4.1's two possible
  display faces and §4.3's resolution scaling both work without per-screen adjustment.
- **No default ImGui look reaches a player-facing screen.** The wrappers are what guarantee this.

Build the wrappers in Phase B, before the first screen migrates, and treat "a screen calls
`ImGui::` directly for something a wrapper covers" as a review failure.

---

## 6. The settings menu — the case that motivates this

### 6.1 What exists today

`frontend_option_buttons[]` (`frontmenu_options_data.cpp:68-88`) is the whole settings surface:
three volume sliders (sound `BID_SOUND_VOL`, music `BID_MUSIC_VOL`, mentor/speech `BID_MENTOR_VOL`),
a mouse-sensitivity slider (`BID_MOUSE_MUL`), an invert-mouse toggle, and a button to the key-remap
screen. `frontend_define_keys_buttons[]` (`frontmenu_options_data.cpp:48-66`) is the remap screen:
**twelve hand-declared row buttons** plus up/down/scroll-tab, each row carrying its index in
`content.lval`, all rendered through the width-derived scroll box from §1.2.

`FrontendSliderCtrl` / `FrontendCheckboxCtrl` (`frontmenu_settingctrl.h`) already abstract the
value binding — get/set/nonlinear-mapping — so the *data* half of a generic settings system exists.
What doesn't exist is a generic *presentation* half: every control is still a hand-placed pixel
rectangle in a static array. In ImGui the remap screen is a `for` loop over
`num_definable_keys()` inside an `FeBeginListBox`, and the twelve row buttons plus their
`_maintain`/`_up`/`_down`/`_scroll` callbacks all disappear.

### 6.2 Launcher options to bring in-game

Scope: the launcher's **Game, Graphics, Sound and Input** tabs
(`ui/settingsdialog.ui`, `src/settingsdialog.cpp::saveSettings`), **excluding** packet save
(`GAME_PARAM_PACKET_SAVE_ENABLED`, `GAME_PARAM_PACKET_SAVE_FILE_NAME`), exit-on-Lua-error
(`EXIT_ON_LUA_ERROR`), extra launch options (`EXTRA_GAME_LAUNCH_OPTIONS`), command character
(`COMMAND_CHAR`), and enable-sound (`GAME_PARAM_NO_SOUND`). The Multiplayer and Launcher tabs stay
in the launcher entirely.

That leaves, by tab:

**Game** — `LANGUAGE`, `CENSORSHIP`, `SCREENSHOT`, `DELTA_TIME`, `FREEZE_GAME_ON_FOCUS_LOST`,
`FLEE_BUTTON_DEFAULT`, `IMPRISON_BUTTON_DEFAULT`, `STARTUP` (the splash-screens + intro list; the
legacy `DISABLE_SPLASH_SCREENS` + `-nointro` pair on older builds), plus cheats
(`GAME_PARAM_ALEX`) and gameturns (`GAME_PARAM_FPS`).

**Graphics** — `DISPLAY_NUMBER`, `RESIZE_MOVIES`, `INGAME_RES`, `FRONTEND_RES`,
`CREATURE_STATUS_SIZE`, `LINE_BOX_SIZE`, `HAND_SIZE`, `FRAMES_PER_SECOND` (including its `AUTO`
prefix form), `GUI_BLINK_RATE`, `NEUTRAL_FLASH_RATE`, plus smooth video (`GAME_PARAM_VID_SMOOTH`).

**Sound** — `PAUSE_MUSIC_WHEN_GAME_PAUSED`, `MUTE_AUDIO_ON_FOCUS_LOST`, `ATMOSPHERIC_SOUNDS`,
`ATMOS_FREQUENCY`, `ATMOS_VOLUME`, plus CD music (`GAME_PARAM_USE_CD_MUSIC`).

**Input** — `POINTER_SENSITIVITY`, `UNLOCK_CURSOR_WHEN_GAME_PAUSED`, `LOCK_CURSOR_IN_POSSESSION`,
`CURSOR_EDGE_CAMERA_PANNING`, `ZOOM_TO_MOUSE`, `ROTATE_AROUND_MOUSE`, `TAG_MODE_TOGGLING`,
`DEFAULT_TAG_MODE`, plus alt input (`GAME_PARAM_ALT_INPUT`).

Four findings from reading both sides, each of which is real work the schema alone won't absorb:

1. **`keeperfx.cfg` is read-only to the engine.** `config_keeperfx.c` contains no writer — no
   `fopen`/`fprintf`/save path of any kind. `save_settings()` (`config_settings.h:92`) persists the
   binary `struct GameSettings` (`config_settings.h:64-86`), a *different, much smaller* set: video
   detail, shadows, view distance, volumes, keybindings, mouse invert/sensitivity, zoom levels.
   Nearly every option above lives in the file the engine cannot write. **A comment- and
   order-preserving `keeperfx.cfg` writer is a prerequisite**, independent of ImGui, and could be
   built first. Launcher and game never run write operations concurrently, so no locking or
   conflict-resolution scheme is needed — but both write the same file at different times, so the
   writer must round-trip faithfully: preserve comments, key order, and any key it does not
   recognise (the launcher already relies on this for `STARTUP`'s hidden tokens).
2. **Five of the wanted options have no config key at all.** The `GAME_PARAM_*` entries are the
   launcher's own settings, converted to command-line flags at launch — `-alex`
   (`main.cpp:1869`, `start_params.easter_egg`), `-vidsmooth` (`main.cpp:1775`, `smooth_on`),
   `-altinput` (`main.cpp:1792`, `lbMouseGrab`), CD music (`Clo_CDMusic`, `main.cpp:1735`), and
   `-fps` (`main.cpp:1757`, which *does* already have a `TURNS_PER_SECOND` config key and uses the
   override mechanism). Owning the rest in-game means **adding config keys** to `conf_commands[]`
   and giving them the same command-line-beats-config precedence via
   `start_params.overrides[Clo_*]` — the same mechanism §3.5's own toggle needs, so build it once.
3. **Some controls are not 1:1 with keys.** `POINTER_SENSITIVITY` is one key driving *two* widgets
   — the raw-input checkbox writes `0`, otherwise the slider's value is written. `STARTUP` is a
   space-separated token list assembled from two checkboxes plus tokens the launcher preserves but
   does not display. `FRAMES_PER_SECOND` carries an optional `AUTO ` prefix. And the resolution
   keys are worse than they look — see §6.4.
4. **Some controls gate others.** Alt input enables the possession-cursor-lock checkbox and
   disables the unlock-when-paused one, and vice versa. The schema needs an enable-condition
   field, not just a type.

Apply-class, which the UI must show honestly: *live* (volumes, sensitivity, `ZOOM_TO_MOUSE`,
`ROTATE_AROUND_MOUSE`, `TAG_MODE_TOGGLING`, `CURSOR_EDGE_CAMERA_PANNING`, `GUI_BLINK_RATE`,
`DELTA_TIME`) versus *needs restart* (`INGAME_RES`/`FRONTEND_RES`, `DISPLAY_NUMBER`, `LANGUAGE`,
`STARTUP`, and everything in finding 2 that stays a launch parameter).

### 6.3 The shape that makes this scale

Hand-authoring ~35 controls is exactly the work that produced the current unmaintainable arrays.
Instead: a **declarative option schema** in `kfx_config` — one row per option carrying key name,
type (bool/enum/int/float/string/keybind/composite), range or enum table, localized label and help
string id, category (tab), apply-class, enable-condition, and get/set accessors reusing the
`FrontendSliderCtrl`/`FrontendCheckboxCtrl` pattern. The frontend then renders *the schema*, not
the options: one generic renderer per type, built from §5's wrappers, and adding an option becomes
one table row plus a `.pot` string. Findings 3 and 4 above are why the schema needs composite
types and enable-conditions from the start rather than bolted on later.

**Localization: English first.** Each option contributes a label and a help string, and ~35 options
across 17 languages is a translation burden that should not gate the feature. Ship labels and help
text in English, added to `lang/gtext_eng.pot` as usual, and let translations land incrementally
afterwards — the schema already routes every string through `get_string()`, so a translated entry
starts working the moment it exists with no code change.

This is the single largest payoff in the whole plan, and it is the reason to prefer real widgets
over a re-skinned sprite path. It also feeds the earlier idea (kept from the original doc) of a
debug/cvar inspector — same schema, developer-facing view, effectively free once the schema exists.

### 6.4 Resolution settings collapse to one

Today's resolution configuration is four screen-specific settings wearing two config keys, and it
is the single messiest thing the settings screen would otherwise have to expose:

- **`FRONTEND_RES`** is three slots feeding three *different* targets — failsafe mode, movie
  playback mode, and frontend mode (`config_keeperfx.c:505-513`, dispatching to
  `set_failsafe_vidmode`/`set_movies_vidmode`/`set_frontend_vidmode`).
- **`INGAME_RES`** is up to `CONFIG_MAX_GAME_VIDMODE_COUNT` slots feeding `switching_vidmodes[]`
  (`vidmode.c:55,218-225`) — the list Alt+R cycles through, with the current position stored in
  `settings.switching_vidmodes_index`.

**The intended direction is to collapse all of this to a single resolution option.** The four
distinct modes are historical: separate failsafe, movie and frontend modes made sense when the
frontend was a fixed 640×480 paletted surface and Smacker playback needed its own mode. §5.1's
responsive layout removes the reason the frontend needs a mode of its own, `RESIZE_MOVIES`
(`SMK_FullscreenFit` and friends) already handles movie scaling independently, and a true-colour
renderer at native resolution removes most of what failsafe was insuring against.

This is engine work, not settings-screen work, and it should be treated as a prerequisite rather
than something the schema papers over — exposing four resolution controls in a new settings screen
only to delete three of them later is worse than sequencing it properly. Concretely it touches
`vidmode.c`'s mode getters/setters, `reenter_video_mode()`, the Alt+R cycling path
(`vidmode.c:966-1021`), and both config keys. One migration note: `switching_vidmodes_index` is a
field in `struct GameSettings` (`config_settings.h:75`), which is one of the structs `memcpy`'d
wholesale for saves and network resync — changing or removing it needs the layout-migration story
`docs/Architecture/architecture.md` §6.2 describes, not just a field deletion.

**Decision: this lands before Phase G.** The settings screen is therefore authored against a single
resolution option from the start and never ships the four-control interim. That makes §6.4 a
prerequisite of Phase G rather than part of it — see §7.

---

## 7. Phasing

Each phase is independently shippable; the frontend never has a broken intermediate state, because
un-migrated `FrontendMenuState`s keep their existing draw path and §3.5's flag can force the whole
frontend back to it.

**Phase A — backend up, toggle in place.** Vendor ImGui + the two backends; `kfx_platform` context
service; event feed in `LbPollInputs`; render hook in `PresentFrame`; the `-classicmenu` flag and
its config key, including the `CMDLINE_OVERRIDES` bump. Prove it with `imgui_demo` behind a debug
launch flag, over a live main menu. *Exit:* demo window renders and takes input at several
resolutions on both toolchains, the legacy menu still fully functional underneath, and the toggle
demonstrably switches paths at runtime.

**Phase B — typography, style, and the wrapper layer.** Font loading from `fxdata/` (Exocet when
the installer has copied it in, bundled Cinzel otherwise, Unifont/WenQuanYi merge for CJK/Cyrillic),
the `.ttf` packaging rule, size derived from window height and re-derived on resolution change, the
KeeperFX `ImGuiStyle`, importing the §5.1 chrome PNGs into an ImGui texture atlas, and **all of
§5.2's wrappers** built to the responsive sizing contract. *Exit:* a style-sheet test screen
exercising every wrapper and every shipped language's sample text, legible at 640×480 through 4K,
with both display faces, and a first read on how the `-64` chrome art holds up upscaled.

**Phase C — text screens, then the Options screen.** `FeSt_STORY_POEM`, `FeSt_STORY_BIRTHDAY` and
`FeSt_CREDITS` first — they are backdrop-plus-text (§2.2), so they validate the font stack and the
text wrappers with almost no logic at risk. Then `FeSt_FEOPTIONS`: small, self-contained, and the
first exercise of sliders, checkboxes, navigation and sound feedback. *Exit:* settings change and
persist identically to before; keyboard and gamepad navigation work; credits still scroll at the
same rate; the sprite path is untouched and still used by every other state.

**Phase D — list-shaped screens.** `FeSt_FEDEFINE_KEYS` (deletes the twelve-row pattern, and brings
`draw_defining_a_key_box` with it), `FeSt_HIGH_SCORES`, `FeSt_FELOAD_GAME`. Text entry (high-score
names, network chat) is **Latin-only** — ImGui's SDL3 backend text input is enough; no IME
composition work. Displaying CJK is unaffected and still works via §4.2's fallback fonts; only
typing it is out of scope. *Exit:* scrolling lists of arbitrary length, key capture still works,
high-score name entry still works.

**Phase E — master-detail select screens.** `FeSt_CAMPAIGN_SELECT`, `FeSt_MAPPACK_SELECT`,
`FeSt_LEVEL_SELECT`, `FeSt_MP_MAPPACK_SELECT`, and the land preview at full colour — the highlight-
vs-commit split described in the gap analysis §3, plus retiring
`land_preview_remap_screen_to_shared_palette`. Decide the `FeSt_LAND_VIEW` treatment here.

**Phase F — main menu and network screens.** `FeSt_MAIN_MENU` (needs the wordmark asset),
`FeSt_NET_SERVICE`/`SESSION`/`START`, `FeSt_LEVEL_STATS`, error/add-session overlays. *Exit:* every
screen in §2.1 and §2.2 is ImGui and the flag switches the entire frontend between two complete
implementations. The legacy code is **not** deleted at this point — see §3.5.

**Phase G — settings expansion.** *Prerequisite, landing before this phase:* the resolution
collapse (§6.4), so the settings screen is authored against one resolution option and never ships
the four-control interim. Then: the `keeperfx.cfg` writer, the new config keys and overrides from
§6.2 finding 2, the option schema, the generic schema renderer, and the four tabs' worth of options,
with labels and help text English-first (§6.3). Coordinate with the launcher so the two agree on
which side owns what.

**Later, separately — retire the toggle.** Once §2's screens have shipped and stabilised, decide to
remove `-classicmenu` and the legacy frontend menu code. Deliberate, and not part of this plan's
completion criteria.

---

## 8. Risks and things that will bite

- **Screenshots will miss the ImGui layer.** `RendererSoftware::ScheduleScreenshot`
  (`RendererSoftware.cpp:105-119`) saves `lbDrawSurface` — the CPU framebuffer — and
  `perform_any_screen_capturing()` (`scrcapt.c:142`) is called inside `frontend_draw()` *before*
  present. Neither sees an overlay composited at present time. Fix in Phase A: for ImGui-composited
  frames, capture with `SDL_RenderReadPixels` after the ImGui render pass instead. Movie recording
  (`movie_record_frame`) has the same problem.
- **Two input systems in one process.** Mitigated by `WantCaptureMouse`/`WantCaptureKeyboard`
  gating and by the fact that a given `FrontendMenuState` belongs entirely to one system. Watch
  `frontend_mouse_over_button` (a single global, reset each frame in `wait_at_frontend`) and
  `snap_to_direction` (`button_snapping.c`) — ImGui's own nav replaces the latter on migrated
  screens, and the two must not both run.
- **Two live code paths is a maintenance cost, deliberately accepted.** §3.5's toggle means every
  frontend behaviour change during this stage has to be made twice or consciously made
  ImGui-only. Keep the window short; that is the argument for retiring the toggle promptly once
  Phase F is stable.
- **`gui_frontbtns.c` is shared.** It holds both frontend-only drawing and helpers used by in-game
  code. Whenever legacy frontend chrome is eventually deleted, it needs a per-function check, not
  a file-level one.
- **Menu sound feedback is behaviour, not decoration.** Centralised in the §5.2 wrappers rather
  than left to each screen — that is what the wrapper layer is for.
- **Functional tests.** Any `src/ftests/` test that drives frontend menus by simulated clicks at
  fixed coordinates will need updating as screens migrate. The `-classicmenu` flag gives such tests
  a stable path in the interim, which is a second reason for it to exist.
- **Build surface.** This is the first substantial third-party **C++** source compiled into the
  game. Verify under mingw-w64 cross-compile, MSVC/clang-cl via vcpkg, and native Linux —
  `imgui_impl_sdlrenderer3` in particular must match the vendored SDL3 version
  (`Dependencies.cmake` pins 3.4.12 for the Windows path).
- **Aesthetic drift is the real risk, not a technical one.** The permission to modernize makes it
  easy to land somewhere generic. Phase B's style test screen exists to make that visible early;
  treat it as a gate, not a formality.

## 9. Verification

- **Not** screenshot-diff against the pre-migration baseline — this stage deliberately changes
  appearance, so the original doc's "any visible difference is a bug" criterion no longer applies.
  Verify *behaviour* parity instead: every navigation path, every setting written and re-read,
  every list scrolled to its end, per migrated screen.
- **Every phase must be tested both ways**, with the flag on and off. The legacy path staying
  correct is a shipped feature until the toggle is retired, not a courtesy.
- Legibility check per phase at 640×480, 1920×1080 and 3840×2160, for a Latin, a Cyrillic and a
  CJK language, and with both the Exocet and the bundled fallback face.
- `KFX_OS=linux ./build-cmake.sh` plus the mingw cross-compile, both variants.
- `python3 scripts/check_layering.py --strict`.
- Confirm un-migrated states are pixel-identical after each phase — that *is* a valid screenshot
  diff, and it's the guard that the two systems stay independent.

---

## 10. Decisions on record

**No open questions remain.** Every decision this plan depends on was settled on 2026-09-04 and is
recorded here so the reasoning isn't relitigated; each links to the section that acts on it. The
plan is decision-complete — the only thing sequenced ahead of it is §6.4's resolution collapse,
which is engine work in its own right.

### Scope and approach

- **Real ImGui widgets for the main-menu frontend; in-game GUI untouched.** The reversal of this
  document's original recommendation — see the revision note and §1.
- **In-game GUI is a separate, longer-term project, revisited after the GPU work.** See §2.3. Two
  visually unrelated settings UIs (frontend vs. `GMnu_OPTIONS`) is an accepted interim consequence.
  It needs more careful thought than a port: gameplay-critical, latency-sensitive, over live 3D
  content, chrome entangled with game state.
- **`FeSt_LAND_VIEW` is low priority**, mostly superseded by the merged campaign screen; deferred
  past Phase F, possibly indefinitely. `FeSt_NETLAND_VIEW` gets the same merged-screen treatment as
  separate work — sequence it after Phase B so it is authored in ImGui rather than built on chrome
  this stage retires. See §2.4.
- **Both paths ship behind a runtime toggle**, ImGui on by default, `-classicmenu` to opt out. See
  §3.5. Retiring the toggle and the legacy code is a later, deliberate decision.

### Presentation

- **Bundled fallback face — Cinzel.** See §4.1. The weight range decides it: the design needs
  display and heading weights, matching what Exocet Light/Heavy gives us. Exocet itself is copied
  into `fxdata/` at install time and used when present.
- **Responsive layout, not a scaled 4:3 virtual canvas**, with a max content width so ultrawide
  doesn't stretch two-column screens. This is the wrappers' sizing contract. See §5.1.
- **Reuse `FXGraphics-main/menufx/frontend-64/` for chrome.** The source PNGs are already correctly
  cut for 9-slice panels, five-state buttons, and complete slider and scrollbar sets. Only the
  main-menu wordmark is genuine new art; placeholders carry Phases B–E. See §5.1.
- **Ship the low-resolution chrome as-is.** Linear filtering, ornament drawn at modest thickness
  rather than scaled proportionally. A general asset-improvement pass is separately planned and owns
  any higher-resolution chrome; nothing here blocks on it. See §5.1.
- **Gamepad is not a target** — nothing designed around it, nothing blocked on it — but the wrappers
  set nav flags uniformly, because that is what keyboard navigation needs anyway. No
  gamepad-specific affordances. See §5.2.
- **Text entry is Latin-only.** Displaying CJK is unaffected (§4.2's fallback fonts); typing it is
  out of scope. See Phase D.

### Settings

- **Tabs, mirroring the launcher's Game/Graphics/Sound/Input grouping.** Too many options for one
  scrolling list, and matching the launcher's grouping makes parity review straightforward. The
  schema's category field maps directly to a tab. See §6.2, §6.3.
- **`FRONTEND_RES` and `INGAME_RES` collapse to one resolution option**, replacing the four
  screen-specific modes (failsafe, movies, frontend, in-game cycle list). Engine work including a
  `struct GameSettings` layout migration, **landing before Phase G** so the settings screen never
  ships four resolution controls it will then lose three of. See §6.4.
- **No launcher/game write-conflict handling needed** — they never write concurrently. The writer
  must still round-trip faithfully (comments, key order, unrecognised keys), because both edit the
  same file at different times. See §6.2 finding 1.
- **English-first strings.** Labels and help text ship in English; translations land incrementally
  afterwards with no code change, since every string already routes through `get_string()`. See
  §6.3.
