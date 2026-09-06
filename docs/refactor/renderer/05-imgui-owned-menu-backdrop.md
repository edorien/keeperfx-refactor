# ImGui-owned menu backdrop: stop compositing over a live legacy render

Status: **Phases 0, A, B, C, D landed. Phase E (final live verification) pending.** Written at the user's request ("prepare plans
for imgui-owned menu screens") after a live cursor bug (`04-imgui-gui-foundation.md`'s "Fix
double-drawn cursor" entry) exposed the underlying architecture question: "The legacy background
cursor is still rendered, why are we overlaying the imgui boxes over that rather than just
controlling the background directly with imgui, for the menus?" Offered two options (harden the
existing `WantCaptureMouse` cursor-suppression fix further, vs. stop rendering the legacy backdrop
for pure menu screens entirely); the user chose the latter. This document is that plan. Revised
once: the user confirmed the fade-in/out screen-transition effect can be dropped outright ("It was
there to allow for loading time on 30+ year old 486/386 CPUs"), turning §5's original "needs an
ImGui-drawn replacement, deferred" item into Phase 0's straightforward removal instead — see §2.6/§5.
**Phase 0 implemented and verified** (§7.0): `fade_out()`/`fade_in()`/`ProperFadePalette()` and the
`fade_palette_in` global deleted outright (`vidfade.c`/`.h`), every call/write site removed
(`frontend.cpp` ×4, `game_session_loop.cpp`, `front_landview.c`) rather than left dead,
`RendererSoftware::PresentFrame()`'s reentrancy guard comment updated to record that its only known
trigger is now gone (guard itself left in place, per §7.0's own note, pending live confirmation).
**Phases A and B implemented** (§7): `FeStyleGetMenuBackdropTexture()` (`frontgui_style.cpp`) decodes
`frontend_background` through `frontend_palette` once via `resolve_indexed_pixel()` and uploads it to
a texture via `RendererCreateDynamicTexture()`/`RendererUpdateDynamicTexture()` (the land-preview
panel's own idiom, just built once and never re-uploaded); `draw_menu_backdrop()`
(`frontgui_screens.cpp`) draws it via `ImGui::GetBackgroundDrawList()->AddImage()`, reusing
`get_frontmenu_background_area_rect()`'s own aspect-fit math (newly declared in `gui_draw.h`, which
had the function's definition but no header declaration at all until now), called once per frame
from `FrontendImGuiFrame()` right after the `frontend_imgui_screen_active()` gate. **The legacy blit
is still running underneath at this point, deliberately** (§6.3's recommended landing order) — this
should currently look like the exact same backdrop being drawn twice, a harmless no-op if the
aspect-fit port is correct and a visible bug (misaligned/stretched image) if it isn't. All four build
configs, `check_layering.py --strict`, and the full test suite pass; **not yet confirmed live** --
this is the point to verify before Phase C touches the actual composite order.

**Live-testing result**: backdrop confirmed correct on every migrated screen (the aspect-fit port
was right first time), but "no mouse cursor visible" -- a real, pre-existing bug in the cursor code
this change happened to expose, unrelated to the backdrop itself; see
`04-imgui-gui-foundation.md`'s corresponding entry for the fix. Backdrop verification itself is
complete; proceeding to Phase C.

**Phases C and D implemented** (§7). §4.3's plan was refined based on a second live finding: once the
cursor was actually visible (after the fix above), "Cursor only appears when hovering over menus. It
disappears when over background" -- caused by exactly the interaction §4.3 anticipated (`draw_menu_
backdrop()` painting over the legacy-drawn cursor for the area outside the small menu panel, since
that draw runs *after* the legacy blit every frame, and `WantCaptureMouse` alone is only true over
the panel itself). Rather than a `RendererSetSkipBackdropCallback` used for exactly one thing, added a
more general `ImGuiScreenOwnedFn`/`RendererScreenOwnedFn` query (`gui/ImGuiContext.h` owns the actual
storage and logic, `RendererManager.h` is a thin pass-through facade, matching the existing cursor-
image-callback pattern) answering "is the *current* screen one of the 15 fully ImGui-owned states" --
used in three places: `RendererSoftware::PresentFrame()` skips the legacy blit (§4.3, still clears to
black first so there's no stale single-frame flash); `ImGuiContext.cpp`'s cursor draw fires whenever
this is true, not just on `WantCaptureMouse`, since there's no legacy fallback left to defer to
anywhere on these screens; and `bflib_mspointer.cpp`'s `OnBeginSwap()`/`OnMove()` skip their own
legacy draw when this is true too, avoiding pure wasted work now that nothing legacy composites there
at all. `frontend_draw()`'s switch (§4.4, `frontend.cpp`) drops `frontend_copy_background()` for every
migrated state, exactly as planned. Verified: all four build configs, `check_layering.py --strict`,
full test suite. Not yet confirmed live -- this closes out the backdrop side of the plan; only Phase E
(final live verification across every migrated screen, plus confirming `-classicmenu` is untouched)
remains.

## 1. The ask, verbatim

> The legacy background cursor is still rendered, why are we overlaying the imgui boxes over that
> rather than just controlling the background directly with imgui, for the menus?

Confirmed as a request for the bigger change (not just a harder cursor-suppression patch) when
offered the explicit choice between the two.

## 2. Current behaviour (traced, not assumed)

### 2.1 The per-frame composite is backdrop-then-ImGui, unconditionally

`RendererSoftware::PresentFrame()` (`src/kfx_platform/src/renderer/RendererSoftware.cpp:126`) runs
every frame, for every screen, and always does the same three things in order:

1. `LbMouseOnBeginSwap()` — draws the classic cursor sprite directly into `lbDrawSurface` (the
   software-rendered backdrop), unless suppressed (see §2.4).
2. Uploads `lbDrawSurface` to `m_texture` and blits it to fill the entire render target
   (`SDL_RenderTexture(m_renderer, m_texture, NULL, NULL)`) — no gating, no exceptions, whatever
   `lbDrawSurface` currently holds.
3. If ImGui is enabled, runs `ImGuiContextNewFrame()` / `RendererRunImGuiFrameCallback()` /
   `ImGuiContextRender()` — composited **on top of** the backdrop texture from step 2, then
   presents.

There is currently no code path that skips step 2. Every ImGui screen, including ones with no
gameplay content behind them at all, sits on top of a live SDL blit of whatever `lbDrawSurface` was
last drawn into.

### 2.2 What `lbDrawSurface` actually holds for menu screens: a static bitmap, not a live render

`frontend_draw()` (`src/kfx_frontend/src/frontend.cpp:3631`) is what fills `lbDrawSurface` before
`PresentFrame()` runs. Its `switch (frontend_menu_state)` groups every currently-ImGui-migrated menu
screen (`FeSt_FEOPTIONS`, `FeSt_FELOAD_GAME`, `FeSt_HIGH_SCORES`, `FeSt_MAPPACK_SELECT`,
`FeSt_CAMPAIGN_SELECT`, `FeSt_MP_MAPPACK_SELECT`, `FeSt_MAIN_MENU`, `FeSt_LEVEL_STATS`,
`FeSt_NET_SERVICE`, `FeSt_NET_SESSION`, `FeSt_NET_START`, `FeSt_FEDEFINE_KEYS`, `FeSt_STORY_POEM`,
`FeSt_CREDITS`, `FeSt_STORY_BIRTHDAY`) into calling `frontend_copy_background()` — **unconditionally**,
even when that screen's own `draw_gui()` (the legacy widget path) is skipped because ImGui already
owns it:

```c
case FeSt_FEOPTIONS: /* ... */ case FeSt_MAIN_MENU: /* ... */
    frontend_copy_background();
    if (!frontend_imgui_screen_active(frontend_menu_state))
        draw_gui();
    break;
```

`frontend_copy_background()` → `draw_frontmenu_background()` → `frontmenu_copy_background_at()`
(`src/kfx_frontend/src/gui_draw.c:1052`) does exactly one thing: `copy_raw8_image_buffer()`s a
**static 640×480 indexed-colour bitmap**, `frontend_background`, into the framebuffer, scaled to
fit. `frontend_background` is loaded **once**, at `frontend_load_data()` time
(`src/kfx_frontend/src/frontend.cpp:895`), from a single file (`front.raw`, `FGrp_LoData`) — it
never changes at runtime. It's the torch-lit dungeon-corridor image visible behind the "Dungeon
Keeper" panel in every screenshot so far. This is **not** a live/dynamic render in any of the 15
migrated states — every one of them is compositing ImGui over what amounts to a static desktop
wallpaper.

The one nuance: `frontmenu_copy_background_at()` draws through `copy_raw8_image_buffer()`, which
decodes the indexed bitmap through **whatever palette is currently active** — not a fixed RGBA image.
`fade_in()`/`fade_out()` (`src/kfx_render/src/vidfade.c:52`) scale the active palette down toward
black and back over a short animation to produce the game's screen-transition fades, so the same
static bitmap's on-screen colour genuinely does shift for ~0.3–0.5s around every state change. See
§5 for how this document proposes to handle that.

### 2.3 Two migrated screens already partially do this (land preview panels)

`FeSt_CAMPAIGN_SELECT` and `FeSt_MAPPACK_SELECT`'s land-preview panel is *already* rendered
off-screen into its own buffer and submitted as an ImGui image from inside
`FrontendImGuiFrame()`/`draw_land_preview_panel()` — not through the per-frame
`frontend_copy_background()` path at all (see that function's own comment,
`frontgui_screens.cpp`, and `frontend.cpp:3676`'s note: "Phase E's master-detail screens
additionally render their land preview panel through the software path too, but off-screen and
from inside that later ImGui submission"). Their full-screen **backdrop** is still the same static
`front.raw` image, though — only the smaller preview panel is already decoupled this way. This is
useful precedent: this document's proposal for the backdrop is structurally the same idea (render
once, hand ImGui a texture, drawn as an ImGui image) already proven to work for a harder case
(a *dynamic* land preview) inside this exact codebase.

### 2.4 The cursor entanglement this all causes

`LbI_PointerHandler::OnBeginSwap()`/`OnMove()` (`src/kfx_platform/src/bflib_mspointer.cpp`) draw the
classic cursor sprite into `lbDrawSurface` every frame, and (as of the fix earlier in
`04-imgui-gui-foundation.md`) are gated on `ImGuiContextWantCaptureMouse()` so they skip drawing
when ImGui already owns the cursor. That fix is necessary *because* step 2 in §2.1 always blits
`lbDrawSurface`, cursor and all — there was no other way to keep the two cursor-drawing systems from
fighting. If the backdrop itself is never drawn for a given screen, this whole cross-check becomes
unnecessary for that screen: there's nothing in `lbDrawSurface` for the legacy cursor draw to matter
against, because `lbDrawSurface` is never composited at all.

### 2.5 What stays exactly as it is

- **`-classicmenu`/`-noimgui` mode**: `frontend_imgui_screen_active()` returns false whenever
  `RendererImGuiEnabled()` is false, which is the case for the entire classic-menu path
  (`use_classic_menu()`, `config_keeperfx.c`). None of this document's proposal touches that code
  path at all — `frontend_copy_background()`/`draw_gui()` keep running exactly as today whenever
  ImGui isn't the thing drawing this screen.
- **Screens with genuinely live content**: `FeSt_LAND_VIEW` (`frontmap_draw()`),
  `FeSt_NETLAND_VIEW` (`frontnetmap_draw()`), and `FeSt_TORTURE` (`fronttorture_draw()`) are *not*
  in `state_is_migrated()`'s list (`frontgui_screens.cpp:100`) — they're still fully classic-rendered
  screens with no ImGui window of their own, so `frontend_draw()`'s dispatch for them is completely
  untouched by this proposal. If/when either of these is ever migrated to ImGui, it would need its
  own off-screen-render-into-texture treatment (§2.3's precedent), not the static-bitmap approach
  this document proposes for the other 15.
- **In-game overlays over a live dungeon view** (if any exist or get added later) genuinely need a
  live backdrop underneath ImGui content — this proposal only applies to states whose backdrop is
  provably static.

### 2.6 The fade-in/out mechanism, precisely — since it's now being removed (§5), not just deferred

Traced exactly where it lives: `frontend_set_state()` (`frontend.cpp:3102`) calls `fade_out()`
unconditionally on every state change (skipped only for the very first transition out of
`FeSt_INITIAL`) and sets the global `fade_palette_in = 1`. The frontend/menu main loop
(`game_session_loop.cpp:801-823`) then runs `frontend_update()` → `frontend_draw()` +
`RendererPresentFrame()` once for the *new* state (drawn while the palette is still faded to black
from `fade_out()`), and only afterward checks `fade_palette_in`, calling `fade_in()` once and
clearing the flag. Two call sites total, nothing else in the tree calls either function
(`frontmenu_landpreview.c`/`frontgui_style.cpp` only reference the mechanism in comments). Each of
`fade_out()`/`fade_in()` is itself a **blocking** multi-step animation (`ProperFadePalette()` →
`LbPaletteFade()`/`LbPaletteFadeStep()`), calling `RendererPresentFrame()` again per step to
actually show the palette dimming/brightening on screen — which is *why*
`RendererSoftware::PresentFrame()` needed the `s_presenting_imgui_frame` reentrancy guard documented
in `04-imgui-gui-foundation.md` (a fade triggered by a state change requested from inside an ImGui
frame callback would otherwise start a second ImGui frame before the first had called `Render()`).

## 3. Scope: which screens this actually covers

Every state `state_is_migrated()` currently lists, all 15 of them — confirmed above to use *only*
the static `front.raw` bitmap for their full-screen backdrop:

`FeSt_MAIN_MENU`, `FeSt_FEOPTIONS`, `FeSt_FEDEFINE_KEYS`, `FeSt_FELOAD_GAME`, `FeSt_HIGH_SCORES`,
`FeSt_LEVEL_STATS`, `FeSt_CAMPAIGN_SELECT`, `FeSt_MAPPACK_SELECT`, `FeSt_MP_MAPPACK_SELECT`,
`FeSt_NET_SERVICE`, `FeSt_NET_SESSION`, `FeSt_NET_START`, `FeSt_STORY_POEM`, `FeSt_CREDITS`,
`FeSt_STORY_BIRTHDAY`.

Any *future* screen migrated to ImGui automatically gets this treatment for free as long as it's
added to `state_is_migrated()` and its backdrop is the same static bitmap — no per-screen special
casing needed beyond that one list.

## 4. Proposed design

### 4.1 Cache `front.raw` as an RGBA texture, once

New helper in `frontgui_style.cpp` (same file, same "off-screen render, cache once, ImGui owns the
texture" idiom `FeStyleGetCursorImage()` already established for the cursor sprite) —
`FeStyleGetMenuBackdropImage(struct ImGuiCursorImage *out)`-shaped, or a small dedicated struct if a
plain RGBA blob without hotspot fields reads more cleanly. Simpler than the cursor case: `front.raw`
is a flat indexed buffer, not an RLE sprite, so decoding it is a plain per-pixel palette lookup
(`frontend_palette[3*index+{0,1,2}]` → RGB, alpha 255) rather than `LbSpriteDrawImmediate()` through
the renderer. Decode once, through `frontend_palette` (the same stable, un-faded reference the
cursor capture already uses, for the same reason — a mid-fade capture would freeze at whatever the
palette happened to be that instant). Cache the resulting `640×480` RGBA buffer for the life of the
process; `frontend_background` itself never changes after `frontend_load_data()`, so there's no
invalidation to handle.

### 4.2 Draw it from ImGui, once per frame, for migrated states only

`FrontendImGuiFrame()` (`frontgui_screens.cpp`) gets a new call at its very top, before any
per-screen `frontgui_*_frame()` dispatch: if `frontend_imgui_screen_active(frontend_menu_state)`,
draw the cached backdrop texture full-screen via `ImGui::GetBackgroundDrawList()->AddImage(...)`
(the background list draws *before* every window, so per-screen ImGui content still layers
correctly on top — mirrors `ImGuiContext.cpp`'s own use of `GetForegroundDrawList()` for the cursor,
just the opposite end of the paint order). Scaled to `io.DisplaySize`, same aspect-fit math
`get_frontmenu_background_area_rect()` already uses today (centred, letterboxed to the image's own
4:3, not stretched) — reuse that function's arithmetic rather than reinventing it, since it's
already resolution-independent and already exactly right for this image.

### 4.3 Tell `RendererSoftware::PresentFrame()` to skip its own blit for these screens

Without this, `PresentFrame()` still unconditionally blits `lbDrawSurface` (§2.1 step 2) *underneath*
the new ImGui-drawn backdrop from §4.2 — at best redundant (drawing the same image twice), at worst
showing **stale** content (whatever `lbDrawSurface` held from the *previous* frame's classic
rendering, e.g. a leftover dungeon view from just before returning to the Main Menu, if
`frontend_copy_background()` is also skipped per §4.4 without this). A new callback, matching the
established `RendererSetCursorImageCallback`/`RendererSetMousePositionCallback` shape
(`RendererManager.h`): `RendererSetSkipBackdropCallback(TbBool (*fn)(void))`, checked in
`PresentFrame()` immediately before the `SDL_RenderClear`/`SDL_RenderTexture` pair — skip both when
it returns true. Wired in `main.cpp`'s `setup_game()` to a small wrapper around
`frontend_imgui_screen_active(frontend_menu_state)`, next to where `RendererSetCursorImageCallback`
is already wired.

### 4.4 Stop drawing the legacy backdrop for these screens in `frontend_draw()`

Mirrors the exact pattern already used for `draw_gui()` in the same switch — change:

```c
frontend_copy_background();
if (!frontend_imgui_screen_active(frontend_menu_state))
    draw_gui();
```

to:

```c
if (!frontend_imgui_screen_active(frontend_menu_state))
{
    frontend_copy_background();
    draw_gui();
}
```

for every migrated-state case in the switch (the `FeSt_STORY_POEM`/`FeSt_CREDITS`/
`FeSt_STORY_BIRTHDAY` cases have their own `if (frontend_imgui_screen_active(...))` branches already
— same change, just inverted onto the existing structure). `FeSt_FEDEFINE_KEYS` keeps its
`defining_a_key` modal-box special case exactly as is, just also folded under the same guard.

### 4.5 Consequence for the cursor fix already landed

Once §4.2–4.4 land for a given screen, `lbDrawSurface` is never composited at all while that screen
is showing (per §4.3/§4.4 together) — there is structurally nothing left for the classic cursor
draw to conflict with. The `ImGuiContextWantCaptureMouse()` gate in `bflib_mspointer.cpp`
(`04-imgui-gui-foundation.md`'s "Fix double-drawn cursor" entry) stays exactly as it is — it's still
correct and still needed for any screen that keeps a live legacy backdrop (Land View, Torture,
NetLand View, or any future mixed screen), just newly redundant-but-harmless for the 15 screens this
document covers. Not proposing to remove it.

## 5. Explicitly out of scope / deferred

- **Fade in/out — dropped entirely, not replaced.** Originally scoped here as "needs an ImGui
  overlay to replicate it" (§2.6 has the exact mechanism this deferred). Per the user: "We can
  discard the fade-in/out between screens. It was there to allow for loading time on 30+ year old
  486/386 CPUs" — the effect's original purpose (masking load time on hardware roughly three decades
  old) no longer applies, so this document now proposes removing the two call sites outright
  (`fade_out()` in `frontend_set_state()`, `frontend.cpp:3102`; the `fade_palette_in`-gated
  `fade_in()` in the frontend main loop, `game_session_loop.cpp:820-823`) rather than replacing the
  effect with an ImGui-drawn equivalent. This is a genuine simplification of this document's own
  scope, not scope creep bolted on: removing `fade_out()`'s call site removes the *only* known
  trigger for `RendererSoftware::PresentFrame()`'s reentrancy scenario (§2.6's last sentence) — the
  `s_presenting_imgui_frame` guard becomes dead code as a result (harmless to leave as a defensive
  belt-and-suspenders, or removable in the same pass — see Phase A′ below). Scoped as universal (every
  frontend state, not just the 15 in §3) since the two call sites aren't state-specific and the
  historical rationale for removing them doesn't depend on which screen is showing.
- **Screenshot capture during a menu.** `perform_any_screen_capturing()` captures from
  `lbDrawSurface`, which never included ImGui's own draw calls even before this change (ImGui
  composites straight to the SDL renderer, never back into the CPU-side buffer) — a menu screenshot
  today already can't show ImGui content. This document doesn't change that limitation one way or
  the other; worth a separate look if "screenshot the options screen" ever becomes a real ask, but
  it's pre-existing, not a regression this change introduces.
- **`FeSt_LAND_VIEW`/`FeSt_NETLAND_VIEW`/`FeSt_TORTURE` migration.** Out of scope entirely — see
  §2.5.

## 6. Open questions / risks worth confirming before implementation

1. **Letterboxing at non-4:3 resolutions.** `get_frontmenu_background_area_rect()`'s aspect-fit math
   already handles this today (the static image is centred with bars, not stretched, at wide
   resolutions) — §4.2 reuses it rather than reinventing it, so behaviour here shouldn't change, but
   worth confirming visually once implemented rather than assumed identical.
2. **`RendererSetSkipBackdropCallback`'s default (noop) value** should return false (never skip) —
   matching every other `ConfigReloadCallbacks`/`Renderer*Callback` noop-stub convention in this
   codebase, so a build that hasn't wired the real callback yet (or `-classicmenu` mode, which never
   calls it either way since `frontend_imgui_screen_active()` is false there) behaves exactly as
   today.
3. **Order of landing**: §4.1 (cache the texture) and §4.2 (draw it) can land and be verified live
   completely independently of §4.3/§4.4 (skip the legacy blit) — landing them first, with the
   legacy blit still running underneath (briefly drawing the same image twice, harmless but
   wasteful), would let the backdrop's visual correctness be confirmed before touching
   `RendererSoftware::PresentFrame()`'s composite order at all. Recommend that ordering.

## 7. Phase breakdown (for whenever implementation starts)

- **Phase 0**: Remove the fade-in/out mechanism (§5) — drop the `fade_out()` call in
  `frontend_set_state()` and the `fade_palette_in`/`fade_in()` handling in the frontend main loop
  (`game_session_loop.cpp:820-823`). Independent of every other phase here (universal, not tied to
  the 15-screen scope in §3) and low-risk enough to land and verify on its own first — doing so also
  removes the only known trigger for `RendererSoftware::PresentFrame()`'s `s_presenting_imgui_frame`
  reentrancy guard before Phase C touches that same function, simplifying what Phase C has to reason
  about. Whether to also delete the now-provably-dead reentrancy guard itself, or leave it as a
  harmless defensive no-op, is a judgement call to make once this phase is confirmed live — not
  required either way.
- **Phase A**: `FeStyleGetMenuBackdropImage()` (or equivalent) in `frontgui_style.cpp` — decode
  `frontend_background` through `frontend_palette` once, cache the RGBA buffer.
- **Phase B**: Draw it from `FrontendImGuiFrame()` via `ImGui::GetBackgroundDrawList()`, gated on
  `frontend_imgui_screen_active(frontend_menu_state)`, reusing
  `get_frontmenu_background_area_rect()`'s aspect-fit math. Verify live against §6.1 before
  proceeding — the legacy blit is still running underneath at this point, so this phase is visually
  a harmless no-op if done right (same image drawn twice) and a visible bug if the aspect-fit port
  got something wrong.
- **Phase C**: `RendererSetSkipBackdropCallback()` (`RendererManager.h`/`.cpp`), wired in
  `RendererSoftware::PresentFrame()` and `main.cpp`'s `setup_game()`.
- **Phase D**: `frontend_draw()`'s switch, per §4.4, for all 15 migrated states.
- **Phase E**: Live verification — confirm the backdrop still looks correct with the legacy blit now
  actually skipped, confirm the cursor is single-drawn and correctly positioned on every migrated
  screen, confirm classic-menu mode is untouched, confirm the four-build-config +
  `check_layering.py --strict` + full-test-suite loop this project always runs before calling
  something done.
