# Stage 3 — GPU acceleration (single source)

Status: **Phase A landed** (with stage 2). **Phase B: the in-game HUD (B3) has already landed**,
via the separate [in-game-GUI-as-ImGui project](../../ingame-gui/00-overview.md) run ahead of this
stage by explicit agreement (see "What the in-game-GUI project already built," below) — what's
left of Phase B is B1 (frontend backdrop cost) and B2 (capture path), both not started, plus
cursor unification (partially open, see R7). **Phase C now scoped in detail** (below), informed by
a companion review of an external attempt at the same problem; not started.

This document is the **single source of truth for the whole GPU-acceleration stage**, superseding
[`../03-gpu-renderer.md`](../03-gpu-renderer.md) (that file now carries a pointer back here — see
its own header). It merges that document's still-current Phase A/B material, risk list, and
verification baseline with a full Phase C design that the original deliberately left unscoped
("do not scope this in detail until Phases A/B have shipped"). Phase C is scoped here anyway,
ahead of that recommendation, because a concrete external data point became available: a
full-size, single-author attempt at exactly this problem
(`origin/feature/opengl-renderer`, 31 commits, 189 files, ~21k lines), assessed in detail by
[`docs/merge-checks/opengl-renderer-review.md`](../../../merge-checks/opengl-renderer-review.md).
That branch is **not being merged** — its architecture doesn't fit this fork (see Phase C's "What
we are explicitly not taking" below) — but it's real, working evidence about what a GPU
world-view renderer for this specific codebase has to solve, and several of its design choices are
worth adopting even though its implementation isn't.

Depends on [02-32bit-software-renderer.md](../02-32bit-software-renderer.md) (**landed** — `TbPixel`
is true-colour RGBA, `lbDrawSurface` is `SDL_PIXELFORMAT_RGBA32`). See [00-overview.md](../00-overview.md)
for the broader four-stage context.

> **Revision note (2026-09-06).** This doc was originally written before stage 4
> ([04-imgui-gui-foundation.md](../04-imgui-gui-foundation.md)) landed. Stage 4 did far more than
> "evaluate ImGui as a rendering backend behind `IUIRenderer`": it reversed its own recommendation
> and migrated the **entire main-menu frontend** (all 15 `GuiMenu`s + the backdrop-plus-text
> screens, Phases A–G) to real Dear ImGui widgets, rendered through `imgui_impl_sdlrenderer3` into
> the *same* `SDL_Renderer` this stage's present path already owns. Phase B's original task list —
> "give `IUIRenderer`/`ITextRenderer` a second, GPU implementation; evaluate ImGui as that
> implementation" — is now the wrong shape: for the frontend it is **already done**, by a different
> and better mechanism than a hand-rolled quad submitter; for the in-game GUI it is deferred by
> stage 4's own §2.3 ("in-game GUI is a separate, longer-term project, revisited after the GPU
> work"). "What stage 4 already built" records the new starting point; Phases B and C are written
> against it.

> **Revision note (2026-09-13a).** Phase C, scoped: a review of `origin/feature/opengl-renderer`
> (a since-abandoned upstream attempt at a full OpenGL 3D renderer) surfaced enough concrete
> design evidence — both what worked and what didn't — to write Phase C in detail rather than wait
> for Phase B to ship first. The two previously-separate documents (this stage's original doc, and
> a standalone Phase C write-up) are merged into this single file so there is one place to read the
> whole stage's status, not two documents that can drift out of sync.

> **Revision note (2026-09-13b), correction.** The Phase B3 section below still described the
> in-game HUD as "still 100% CPU sprite/box/text writes" and framed its ImGui migration as a
> future choice between two routes. That was stale carry-over from this doc's 2026-09-06 revision
> and is wrong as of this correction: the
> [in-game-GUI-as-ImGui project](../../ingame-gui/00-overview.md) shipped in the interim (landing
> incrementally 2026-09-06 through 2026-09-12) and took exactly the route this doc already called
> the recommended default (route 1) — the sidebar, all four build/power/trap/creature grids, query
> panels, save/load, options, event/battle/text boxes, the parchment map, and the first-person/
> possession HUD are now ImGui by default. What was *not* anticipated: the project's legacy sprite
> HUD wasn't deleted — it was kept, by deliberate request, as a permanent, opt-in,
> **deliberately-CPU-only** style (`GUI_ICON_PACK=CLASSIC`, gated by
> `ingame_gui_use_classic_hud()`), not a stepping stone to be migrated later. That has real
> implications for Phase C's compositing model (see the new note in Phase C's "Architecture"
> section and R13) that the original Phase B3 write-up had no reason to consider. Every section
> below touching the in-game HUD has been corrected accordingly.

## Goal

Move drawing work onto the GPU without a single risky rewrite. Phased, each phase independently
shippable and each smaller than the last major stage.

---

## What stage 4 already built (the new starting point)

Everything in this list is live in the shipped build today, on every presented frame, frontend
**and** in-game — the session-global `RendererImGuiEnabled()` gate this originally ran behind has
itself since been retired (per the in-game-GUI-as-ImGui project): `RendererSoftware::PresentFrame()`'s
ImGui pipeline is unconditional now, not a per-state or per-session toggle:

- **One graphics context, already GPU-accelerated.** `RendererSoftware` owns an `SDL_Renderer`
  (`m_renderer`) and an `SDL_Texture` (`m_texture`, `SDL_PIXELFORMAT_RGBA32`).
  `PresentFrame()` (`RendererSoftware.cpp`) does: `SDL_UpdateTexture(m_texture, …, lbDrawSurface)`
  → `SDL_RenderClear` → `SDL_RenderTexture` (the CPU framebuffer, full-screen) → **ImGui draw data
  composited on top** (`ImGuiContextNewFrame` / `RendererRunImGuiFrameCallback` / `ImGuiContextRender`)
  → `SDL_RenderPresent`. SDL3's renderer is hardware-accelerated (Direct3D/Vulkan on Windows,
  GL/Vulkan on Linux).
- **The CPU framebuffer is the backdrop layer; ImGui is a true overlay.** This is *exactly* the
  hybrid the old Phase B described as its stopping point ("3D/software content uploaded as one
  texture, composited underneath the GPU-drawn 2D layer") — it already exists, for the menus.
- **`kfx_platform/include/gui/ImGuiContext.{h,cpp}`** owns ImGui + `ImGui_ImplSDL3_*` +
  `ImGui_ImplSDLRenderer3_*` lifecycle, window/renderer change detection (kept in step with
  `ensure_present_target`), the SDL event feed (from `LbPollInputs`), and the `NewFrame`/`Render`
  pair. Nothing above `kfx_platform` includes `imgui.h` for lifecycle.
- **`RendererManager` façades already in place** (all `extern "C"`, all documented in
  `RendererManager.h`):
  - `RendererImGuiFrameFn` / `RendererSetImGuiFrameCallback` / `RendererRunImGuiFrameCallback` —
    `PresentFrame` calls up into a registered submission function (`kfx_frontend`'s
    `FrontendImGuiFrame`) between `NewFrame` and `Render`, with no upward include.
  - `RendererSwapFramebufferTarget(target,w,h)` / `RendererRestoreFramebufferTarget(prev)` — point
    the software raster at an off-screen buffer and back. Saves/restores
    `GraphicsScreenWidth`/`Height` too (a stride bug fixed during stage 4, `318d55f2c`). Used by the
    eye-lens effect and by stage 4's off-screen captures.
  - `RendererCreateDynamicTexture(w,h)` / `UpdateDynamicTexture` / `DestroyDynamicTexture` — CPU
    RGBA → `SDL_TEXTUREACCESS_STREAMING` `SDL_Texture` → opaque handle castable to `ImTextureID`.
    Used for stage 4's land-preview panel (`land_preview_draw()` rendered off-screen, uploaded,
    `ImGui::Image()`'d into the layout every frame).
  - `RendererSetCursorImageCallback` (`ImGuiCursorImageFn`) — the frontend's real cursor sprite,
    rendered off-screen through `RendererSwapFramebufferTarget`, drawn over ImGui content via
    `GetForegroundDrawList()->AddImage()` while `WantCaptureMouse`.
  - `RendererSetMousePositionCallback` (`ImGuiMousePositionFn`) — feeds ImGui the game's own
    tracked cursor position instead of raw SDL motion (the game warp-grabs the OS cursor).
- **A reentrancy guard in `PresentFrame`** (`s_presenting_imgui_frame`): palette-fade transitions
  re-enter `PresentFrame` per animation step; nested calls do the plain SDL backdrop blit only and
  skip ImGui. Any new nested-present path this stage adds must respect the same guard.
- **Text is already GPU glyph-atlas rendering** for the frontend — ImGui 1.92's dynamic font
  system (`ImGuiBackendFlags_RendererHasTextures`) rasterises TTF glyphs on demand at the exact
  size asked for; `FeStylePushFont()` derives size from `io.DisplaySize.y`. No baked atlas, no
  `ITextRenderer` GPU implementation needed for the menus.
- **Cross-platform `SDL_Renderer` exposure is already paid down.** `imgui_impl_sdlrenderer3`
  forces the mingw-w64/Windows and native-Linux `SDL_Renderer` paths that this stage's verification
  section calls for; stage 4 has been exercising both.

### What the in-game-GUI project already built

This landed as its own project, run *before* this stage's own Phase B by explicit user agreement —
see [`docs/refactor/ingame-gui/00-overview.md`](../../ingame-gui/00-overview.md) §8's decision
record ("Do this project before stage 3's GPU Phase B... it is stage 3 §B3 route 1"). It shipped
incrementally, landing 2026-09-06 through 2026-09-12. Its result is the actual current default
behaviour of the game, not a plan:

- **`menu_is_migrated()`** (`frontgui_ingame.cpp`) marks essentially every substantive in-game
  `GMnu_*` as ImGui by default: the sidebar frame (`GMnu_MAIN`), all four build/power/trap/creature
  grids (`GMnu_ROOM`/`SPELL`/`TRAP`/`CREATURE`), the query/creature-detail panels (`GMnu_QUERY`,
  `GMnu_CREATURE_QUERY1-4`), save/load, options, quit, armageddon/hold-audience, and the
  event/battle/text-info boxes. The parchment map (`frontgui_ingame_parchment.cpp`) and the
  first-person/possession HUD are migrated too.
- **`ingame_gui_use_classic_hud()`** (`src/kfx_config/src/config_keeperfx.c`) is the single gate
  every migrated draw call checks — reading the `GUI_ICON_PACK=CLASSIC` config value. This is
  **not** the retired `-classicmenu`/`RendererImGuiEnabled()` session-wide switch (that one's gone
  outright for the frontend, per stage 4/5); it's a narrower, permanent, in-game-HUD-only style
  choice, kept by deliberate request rather than as a migration safety net to be removed later.
- **The legacy sprite HUD (`CLASSIC`) is not a Phase B/C migration target.** It was kept
  specifically so it can stay CPU-rastered indefinitely as a supported style choice, not a
  stepping-stone implementation. Phase C needs to account for what that means once the 3D view
  itself moves to the GPU — see Phase C's "Architecture" section and R13.
- **What this means for the rest of this document:** every place below that used to describe the
  in-game HUD as "still CPU, waiting on a future ImGui port" (the original Phase B3 write-up) has
  been corrected — that future already happened. What Phase B still owns for the in-game surface is
  narrower than originally scoped: making sure captures/screenshots see the ImGui-rendered HUD too
  (B2, which already covers "the ImGui overlay" generally) and the cursor (still open, R7) — not a
  HUD compositor of its own.

### What is still genuinely CPU-rastered every frame

- **The 3D dungeon view** — `engine_render.c`'s bucketed `do_a_gpoly_*` rasterizer → `vec_screen`
  → `lbDrawSurface`. Unchanged. This is Phase C.
- **The in-game GUI / HUD, only when the user opts into `GUI_ICON_PACK=CLASSIC`.** By default the
  sidebar, tab content (room/power/trap/creature grids, query), save/load, options, event/battle
  boxes, and the parchment map are already ImGui (`ingame_gui_use_classic_hud()` returns false) —
  see "What the in-game-GUI project already built," below. The legacy sprite/box/text path
  (`draw_gui()`'s classic branches, `frontmenu_ingame_*`'s non-ImGui code) still exists and still
  writes `lbDrawSurface` directly, but only when `CLASSIC` is selected, and it is meant to **stay**
  CPU-rastered permanently — see Phase C's compositing note.
- **The frontend backdrop** — `frontend_copy_background()` still draws the full-screen backdrop
  through the software path; ImGui draws over it. Migrated screens skip `draw_gui()` but still call
  `frontend_copy_background()`.
- **Non-migrated frontend paths** — `FeSt_LAND_VIEW`, `FeSt_NETLAND_VIEW`, `FeSt_TORTURE`, and
  Smacker video states are still CPU/sprite-drawn on their own merits, independent of any toggle
  (the `-classicmenu` session-wide switch this used to also list is retired — see the "RendererType
  and backend selection" section).
- **The legacy mouse cursor** — `bflib_mspointer.cpp`'s `LbI_PointerHandler` still composites the
  software cursor via `LbScreenSurfaceBlit` (`SDL_BlitSurface` against `lbDrawSurface`), a parallel
  path, still **unlocked**. Stage 4 added a *second* cursor draw (the ImGui-overlay one) rather than
  replacing this.
- **Lens effects** — `LensManager`/`LensEffect` (`MistEffect`/`FlyeyeEffect`/`OverlayEffect`/
  `DisplacementEffect`/`PaletteEffect`) as a CPU post-process over the framebuffer. Phase C.
- **Screenshot & movie capture** — `RendererSoftware::ScheduleScreenshot` still saves
  `lbDrawSurface` (the CPU buffer, backdrop only); `perform_any_screen_capturing()` runs before
  present. Neither sees the ImGui overlay. Stage 4 flagged this and did not fix it — it is now
  **blocking for this stage** (see Phase B2).

---

## Phase A — Direct texture upload — **DONE**

Landed with stage 2 (`13442110f`, "Flip lbDrawSurface to true-colour"). `PresentFrame()`'s old
lock/blit/unlock `SDL_BlitSurface` INDEX8→RGBA conversion is gone; it is now a single
`SDL_UpdateTexture(m_texture, NULL, lbDrawSurface->pixels, lbDrawSurface->pitch)` straight from the
already-RGBA CPU framebuffer. Nothing further to do here.

---

## Phase B — 2D compositing on the GPU

The original Phase B was "route `IUIRenderer`/`ITextRenderer` through the GPU, with ImGui as one
candidate implementation". That framing is retired. The frontend 2D layer is already GPU-composited
via ImGui/SDLRenderer3. What remains is a short list of concrete, mostly independent items.

### B1 — Frontend backdrop and residual CPU present cost (small)

- `frontend_copy_background()` still paints a full-screen backdrop into `lbDrawSurface` every
  frontend frame, which is then `SDL_UpdateTexture`'d wholesale. For a mostly-static backdrop this
  is a full-resolution CPU fill + full-frame upload per frame behind a UI that changes little.
  Option: upload the backdrop once as a dynamic/static ImGui texture and draw it via
  `GetBackgroundDrawList()`, skipping the software fill and the per-frame texture upload on frames
  where neither backdrop nor 3D content changed. Stage 4 §3.4 deliberately deferred this ("only
  worthwhile when a screen needs the backdrop sampled/tinted per-widget"); revisit here with real
  profiling, since this stage is where per-frame upload cost is the subject.
- Decide whether `m_texture` upload can be skipped entirely on pure-frontend frames (no 3D view,
  backdrop unchanged) — i.e. present ImGui over a cleared/again-textured target without re-uploading
  an unchanged CPU framebuffer.

### B2 — Capture path: screenshots and movie recording (now blocking)

Stage 4 left this as a footnote; Phase B makes the gap unavoidable, because from here on more and
more of what's on screen exists only in the `SDL_Renderer`, never in `lbDrawSurface`. Two separate
problems, both surfaced by stage 2/4 and neither yet addressed:

- **Screenshots miss the overlay.** `RendererSoftware::ScheduleScreenshot` saves `lbDrawSurface`
  (the CPU backdrop only). It must instead capture **after** the ImGui/GPU composite:
  `SDL_RenderReadPixels` on `m_renderer` after `ImGuiContextRender()` and *before*
  `SDL_RenderPresent` (some backends leave the backbuffer undefined after present), into an
  `SDL_Surface` that `IMG_SavePNG`/`SDL_SaveBMP` then writes — with an explicit format/row-order
  conversion, since `SDL_RenderReadPixels` returns the renderer's native format, not necessarily
  `RGBA32`. The read-back belongs in the **outer** present only (respect `s_presenting_imgui_frame`).
  There is no ImGui-disabled fallback mode left to keep the old `lbDrawSurface`-only capture path
  for (`-classicmenu` and `RendererImGuiEnabled()`'s toggle are both retired — the ImGui pipeline
  is unconditional now) — drop the old path outright rather than keeping it "for compatibility."
- **FLC movie recording is already dead, not just incomplete.** `anim_record()`
  (`bflib_fmvids.cpp:1408`) hard-fails with *"Cannot record movie in non-8bit screen mode"* because
  `LbGraphicsScreenBPP()` now returns 32 (`SDL_BITSPERPIXEL(lbDrawSurface->format)`,
  `bflib_video.c:167`), and the `.flc`/FLI codec is intrinsically 8-bit-palettized
  (`anim_make_FLI_COLOUR256` etc.). `movie_record_start` → `GSF_CaptureMovie` therefore does
  nothing useful today. This stage must **decide**: (a) formally retire FLC recording and delete
  `anim_record*`/`movie_record_*`/the `cap_palette` plumbing, or (b) re-implement capture on top of
  the same post-composite `SDL_RenderReadPixels` path — a PNG/BMP frame sequence, or frames piped to
  the ffmpeg encoder the build already links (`Dependencies.cmake` builds ffmpeg from source for
  Smacker *decode*; an encoder is a bigger ask). Option (a) is the honest default unless someone
  actually wants gameplay video capture back.
- `perform_any_screen_capturing()` (`scrcapt.c:142`) runs *before* present
  (`game_session_loop.cpp:430`, `frontend.cpp:3736`) and draws the "REC" indicator into the CPU
  buffer via `LbTextDraw`. A post-composite capture inverts that ordering — the REC indicator must
  move to an ImGui draw (or a second composited pass), or it won't appear in captured frames.
- The `take_screenshot()` / `LbScreenIsLocked()` / `RendererLockFramebuffer()` dance
  (`scrcapt.c:46-62`) is meaningless for a GPU read-back — remove it rather than leave confusing
  dead lock/unlock code.

### B3 — In-game HUD / GUI onto the GPU — **DONE, via a separate project; the remainder is a permanent non-goal**

Originally scoped as an either/or ("wait for an ImGui port" vs. "build a thin GPU 2D compositor").
Route 1 is what happened: the
[in-game-GUI-as-ImGui project](../../ingame-gui/00-overview.md) shipped ahead of this stage,
migrating the sidebar, tab content, query panels, save/load, options, event/battle boxes, the
parchment map, and the first-person HUD onto the same `imgui_impl_sdlrenderer3` layer the frontend
menus already use — see "What the in-game-GUI project already built," above. There is nothing left
for B3 to build.

What's different from the original plan: that project's own scope decision (§8 of its overview)
kept the **legacy sprite HUD** (`GUI_ICON_PACK=CLASSIC`) as a permanent, deliberately-CPU-only
style choice rather than deleting it once ImGui coverage was complete — "a deliberate live-tested
request, not a leftover." That means:

- **Route 2 (a thin GPU 2D compositor for the legacy sprite HUD) is now a non-goal, not just
  unneeded today.** Nobody is going to ask "when does `CLASSIC` get GPU-composited" — the answer is
  never, by design. Drop route 2 from this stage's scope entirely rather than carrying it as
  deferred work.
- **The sprite→GPU-texture cache below is no longer motivated by an in-game HUD compositor.** Its
  remaining justification is Phase C's world-view textures, which is sufficient on its own — see
  below.
- **Phase C inherits a real question the original B3 write-up never had to ask**: what happens to
  `CLASSIC`'s CPU sprite draws — which assume they're compositing in-place onto a `lbDrawSurface`
  that already contains the rendered 3D scene — once Phase C moves the 3D view onto its own GPU
  texture and `lbDrawSurface` stops containing it? See Phase C's "Architecture" section and R13.

### Sprite → GPU-texture cache (needed by Phase C) {#sprite-texture-cache}

`RendererCreateDynamicTexture` is per-frame streaming, not a keyed cache. Build a real cache keyed
on **sprite pointer + expansion palette + remap table** (extending stage 2 §2's per-draw expansion
cache if one was built), producing static `SDL_Texture`s drawn via `SDL_RenderTexture`. `TbSprite`
data is still 8-bit-indexed RLE on disk and in memory (stage 2 expands per-draw, not on load), so
the cache's fill step is "expand indices through the active palette/remap into RGBA, upload once,
reuse until the sprite or palette changes". Invalidation on palette change is the tricky part —
frontend fades and in-game lighting both mutate the active palette (see R10 below).

**This is also Phase C's texture source.** Phase C's world-view renderer needs the identical
RGBA-resolved-once shape for tile/wall textures as this cache already plans for sprites — see
Phase C's "Architecture" section. Build one cache with two population paths (sprite expansion,
static level-texture expansion), not two differently-shaped caches with separate invalidation
rules.

### Cursor

Stage 4 gave the problem a proven pattern (off-screen software render → dynamic texture → draw
list), currently used only for the ImGui-overlay cursor. Fold the legacy
`bflib_mspointer.cpp`/`LbScreenSurfaceBlit` path into the same mechanism so there is **one** cursor
draw, composited on the GPU at present time, at the game's tracked position — removing the
unlocked-`lbDrawSurface` bypass and the current double-cursor-draw. This is stage 4's Part-D
"give the cursor its own `IUIRenderer`-routed draw call" recommendation, now with infrastructure to
do it against.

### B — verification

- B3 is verified as part of the in-game-GUI-as-ImGui project itself, not here — nothing further to
  check for it in this stage. Frame-time comparison against the current build still applies to
  B1/B2.
- Screenshots and recorded movies must now contain the ImGui overlay — that is the B2 acceptance
  test, and it should also cover an in-game frame with the migrated HUD visible, not just the
  frontend.
- `GUI_ICON_PACK=CLASSIC` sessions unaffected by B1/B2/cursor changes (the retired `-classicmenu`
  session-wide switch no longer exists — see "What the in-game-GUI project already built").

---

## Phase C — 3D polygon rasterizer on the GPU

Porting `engine_render.c`'s bucketed gouraud/textured polygon fill (`do_a_gpoly_*` family, via
`bflib_render_gpoly.c` writing `vec_screen`) to real GPU draw calls (vertex/index buffers +
`SDL_RenderGeometry` or a raw GL/Vulkan pass + a fragment shader replicating stage 2's blend math)
is substantially larger and riskier than Phases A/B. It was originally left as an unscoped stretch
goal; it's scoped in detail below, using design evidence from a real external attempt at the same
problem (see the revision note at the top of this document).

- **Compositing is already solved**, unchanged from the original plan: a GPU 3D pass renders into
  its own `SDL_Texture`/render target, which then takes the exact place `m_texture` (the uploaded
  CPU framebuffer) occupies in `PresentFrame` today — drawn first, full-screen, with the ImGui/GUI
  layer composited on top. No new present architecture; one `SDL_RenderTexture` source swaps for
  another.
- **The shade/blend model is already solved too**, by stage 2: `pixmap`'s old
  `fade_tables`/`ghost`/`alpha_sprite_table` lookups were replaced with real per-pixel blend math
  (see [02a-pixel-format-design.md](../02a-pixel-format-design.md)) — that math is now the
  fragment-shader spec directly, a genuine simplification versus reverse-engineering 8-bit table
  semantics into a shader from scratch.
- **Lens effects** (`LensManager`/`LensEffect`) currently operate on the CPU framebuffer as a
  post-process. Each of `MistEffect`/`FlyeyeEffect`/`OverlayEffect`/`DisplacementEffect`/
  `PaletteEffect` needs either its own GPU-shader equivalent, or a documented decision to keep lens
  effects as a CPU post-pass over the read-back GPU frame (`RendererSwapFramebufferTarget` already
  gives lens code a clean off-screen seam, and the `SDL_RenderReadPixels` path from Phase B2 makes
  "GPU frame → CPU buffer → lens pass → re-upload" mechanically available). `PaletteEffect` in
  particular is trivially a shader uniform in an RGBA world. Scoped further as Phase C.3 below.

### The pixel-format decision: RGBA on the GPU, not palette-index

The reviewed branch renders the 3D view by uploading sprite/tile atlases as `GL_R8`
**palette-index** textures, sampling a `256×1 RGBA8` palette texture per fragment, and a `256×64`
fade-table texture for lighting/remap — the standard technique for putting an indexed-colour engine
on a GPU (per the review: the technique itself is executed correctly, nearest-filtered throughout,
no complaint there). **This fork does not use that technique, and Phase C should not adopt it**,
for a reason that predates this document and is worth restating plainly: stage 2
([02-32bit-software-renderer.md](../02-32bit-software-renderer.md), landed) already deliberately
moved this engine off 8-bit palette indices entirely. `TbPixel` is a real `{r,g,b,a}` struct
(`bflib_video.h:62`), `lbDrawSurface` is `SDL_PIXELFORMAT_RGBA32`, and the old
`fade_tables`/`ghost`/`alpha_sprite_table` lookups were replaced with real per-pixel blend math
(`colour * shade_factor`, `src·a + dst·(1-a)`, interpolated gouraud colour). Reintroducing a
palette-index texture format for the GPU path would mean running two incompatible colour pipelines
side by side (RGBA everywhere else, indexed-plus-LUT for the one pass that's actually on screen the
most), and would silently resurrect the one-palette-per-frame ceiling stage 2 was built
specifically to remove ([00-overview.md](../00-overview.md) §"Why now").

That's the architectural reason. There's also a performance reason, and it's the more important one
long-term: indexed-plus-LUT isn't free, and the cost model gets worse, not better, as the rest of
the renderer improves.

**Why palette-index-on-GPU doesn't scale the way "it's just one more texture fetch" suggests:**

- **Every fragment pays a dependent read.** `texture(u_sprite_atlas, uv).r` → use that result as
  the UV for `texture(u_palette, ...)`. The second sample's address isn't known until the first
  completes, so it can't be prefetched or overlapped the way two independent samples can. At DK's
  native sprite resolution this is lost in the noise; it stops being free once overdraw or fill
  rate actually matter.
- **It actively fights dynamic lighting.** Real per-pixel lighting (additional light sources,
  attenuation, coloured light, anything beyond a single baked shade factor) wants to do math in
  linear RGB space *before* any quantization back to a fixed 256-colour palette. Indexed-plus-LUT
  bakes the quantization into the sampling step itself — to light a paletted fragment "correctly"
  you either (a) resolve to RGB first and light that, which throws away the entire reason to stay
  indexed, or (b) try to keep the lighting inside palette space (a remap-table trick, which is
  exactly the `fade_tables`/`ghost` approach stage 2 already retired for being inflexible and
  visually limited — one axis of variation, no colour, no combining multiple lights). There is no
  version of "richer lighting/shadows" that composes cleanly with an indexed base layer; every
  richer lighting model this project would actually want pushes back toward RGB, which means the
  indexed step was pure overhead with no payoff once that work starts.
- **It compounds with every additional post-effect.** Bloom, shadow mapping, any tone-mapping or
  colour-grading pass — all standard GPU techniques — operate on linear RGB. Chaining them after a
  palette-resolve step means either resolving to RGB early (again: why stay indexed at all) or
  reimplementing each effect to understand a quantized palette space (nobody does this; there's no
  prior art to lean on). Every one of these is on this project's plausible future list once the
  fixed-function CPU rasterizer is gone; none of them get easier by having gone through an index
  first.
- **The sprite-refinement path this project actually has planned already produces RGBA, for free.**
  Phase B already scopes a ["sprite → GPU-texture cache"](#sprite-texture-cache) keyed on **sprite
  pointer + expansion palette + remap table**, whose fill step is "expand indices through the
  active palette/remap into RGBA, upload once, reuse until the sprite or palette changes." Phase
  C's world-view textures should be *the same cache*, not a second, differently-shaped one. This
  sidesteps the performance question entirely for sprites: the resolve-to-RGBA cost is paid once
  per sprite/palette combination, not per fragment, and the GPU never sees an index at all.

None of this means the DOS-era palette *concept* is bad — DK's actual look depends on it, and
stage 2 preserved every one of its visual effects (fades, ghosting, gouraud shading, flash-remaps)
faithfully, just recomputed as real colour math instead of table lookups. The disagreement with
the reviewed branch is narrower and more specific: **the palette should be resolved once, on the
CPU or at cache-fill time, not smuggled onto the GPU as a runtime indirection on every fragment.**
Phase C should treat "produce true-colour texture data" as a solved problem it inherits from
Phase B/stage 2, not something to re-solve with a shader-side LUT.

### What's worth taking from the reviewed branch anyway

Despite the pixel-format disagreement and the layering problems (next section), several structural
ideas in `feature/opengl-renderer` are sound independent of pixel format, and Phase C should be
designed around them rather than reinventing the same shapes from nothing:

- **An intermediate-representation (IR) command buffer as the sole crossing point.** The branch
  has `src/kfx/renderer/ir/{WorldCommands,UICommands,TextCommands}.h` plus
  `IRCommandBuffer.h`/`IRenderTaskProducer.h`/`RenderTaskProducerRegistry` — a retained scene
  description that a "producer" (game/render-layer code) fills in, and a "consumer" (a specific
  backend) draws from. This is the right shape for this fork's layering rules and should be the
  **actual mechanism**, not an optional nicety: the reviewed branch built this scaffolding but then
  didn't consistently use it (its `GLWorldViewRenderer.cpp` still reaches past it into
  `player_data.h`/`creature_graphics.h`/`engine_render.h`/`game_legacy.h` directly for most of its
  2661 lines — see the review §2). Phase C should finish the idea the reviewed branch started and
  then abandoned partway through.
- **A backend-agnostic GPU resource-mapper.** `GpuResourceDesc.h`/`GpuResourceHandle.h` +
  `GLResourceMapper.cpp` describe textures/buffers by value (dimensions, format, filter mode) and
  hand back an opaque handle, so the code that decides *what* a texture should look like doesn't
  need to know *how* a specific backend allocates one. Directly reusable shape for the new
  geometry/vertex-buffer submission façade R6 (below) calls for.
- **Tile-atlas packing as its own concern.** `TileAtlasPacker.{h,cpp}` separates "how do texture
  blocks get packed into atlas pages" from both the resource mapper and the world-view renderer.
  Worth keeping as a separate small component rather than folding packing logic into the renderer
  itself.
- **One pass per concern.** `GLWorldViewRenderer` (dungeon view), `GLUIRenderer`, `GLTextRenderer`,
  `GLCursorLayer`, `GLMapFadePass`, `GLZoomBoxTilesPass`, `GLImagePresentPass` are separate classes
  with a narrow job each, rather than one large "draw everything" object. This maps cleanly onto
  Phase C's own scope (world view + lens effects only — UI/text/cursor are Phase B's job, already
  headed toward ImGui) and is worth keeping as the internal shape even though the file contents
  don't port.
- **A config-level experimental gate.** The reviewed branch ships
  `RENDERER=SOFTWARE`/`RENDERER=OPENGL` in `keeperfx.cfg` with an explicit "experimental — opt-in
  for testing only" comment, alongside keeping the CPU path as the shipped default. This matches
  the "RendererType and backend selection" section below almost exactly — good validation that
  this is the right posture, from an independent source arriving at the same answer.

### What we are explicitly not taking

- **Palette-index-on-GPU** — previous section.
- **Direct upward `#include`s from renderer code into gameplay state.** The reviewed branch's
  `GLWorldViewRenderer.cpp` pulls in `player_data.h`, `creature_graphics.h`, `engine_buckets.h`,
  `engine_render.h`, `engine_textures.h`, `vidmode.h`, `local_camera.h`, and `game_legacy.h`
  directly (review §2) — in this fork's terms, that's a `kfx_platform → kfx_sim`/`kfx_render`/
  `kfx_game` violation on a scale `check_layering.py --strict` would reject outright, and exactly
  what the IR command-buffer boundary (above) exists to prevent. Every value the world-view pass
  needs — camera state, tile/creature draw data, lighting snapshot, palette-resolved texture
  handles — must arrive through the IR command buffer or an existing/new callback struct, never
  through a raw include.
- **The dedicated render thread.** `RenderThreadManager`/`RendererThread` run GL work on its own
  thread, synchronized with `Signal()`/`WaitForCompletion()`. This directly conflicts with R2
  below: *"`SDL_Renderer` is single-threaded; keep every present on the main thread"* — true today
  because nothing net-new here has been checked against a second GL/render thread's
  context-sharing rules, its interaction with the `PresentFrame` reentrancy guard (R1), or
  `RendererSwapFramebufferTarget` (R4, already documented as non-nesting and fragile). The
  reviewed branch's own thread synchronization is unreviewed and untested by anyone but its single
  author. Phase C should ship single-threaded first (C.0–C.3 below) and treat a render thread as a
  distinct, later, separately-justified optimization — not a day-one assumption (see new risk R12).
- **Building it as one large branch.** The reviewed branch is 31 commits from one author over
  about a week, with a visible tail of correctness/perf fixes discovered after the fact (`fix:
  worldview renders again`, `fix: camera rotation`, `perf(selector): stop killing the game with
  30x the IR quads`, a screen-width/height accessor mix-up that silently broke movie playback,
  text clipping, and map-fade buffers across four files simultaneously — see the review §5 and the
  note below). None of that is a character flaw in the approach; it's what happens when ~21k lines
  land with zero automated coverage and get shaken out live. Phase C should ship in small, tested,
  independently-reviewable slices (phased delivery, below) specifically to avoid reproducing that
  tail here.
- **A naming trap worth avoiding on sight, not after the fact.** The reviewed branch's
  `RendererScreenWidth()`/`RendererScreenHeight()` accessors were swapped at several call sites —
  width used where the stride (which equals width, not height, for a row-major buffer) was meant —
  across `bflib_fmvids.cpp`, `bflib_sprfnt.c`, and `engine_redraw.c`, all fixed in one commit
  (review §5, `6cbc5d597`). Those specific functions don't exist in this fork, so there's no bug to
  port, but the failure mode is generic: any new stride/dimension accessor Phase C introduces
  (e.g. on the geometry-submission façade, R6) should have a name that makes "stride" and "width"
  impossible to confuse — `TbBytePitch` (`bflib_video.h:123`) already exists precisely to make this
  class of mistake a compile error rather than a silent wrong-by-4x; follow that pattern rather
  than adding a bare `int`-returning accessor pair that invites the same mix-up again.

### Architecture: where the code lives, and the boundary it must not cross

Per [`docs/Architecture/architecture.md`](../../../Architecture/architecture.md) §2.1/§2.4,
`kfx_platform` (home of `IRenderer`/`RendererManager`/`RendererSoftware`) is the **bottom** of the
nine-library dependency ladder — it may depend on external libs only. `kfx_render` (home of
`engine_render.c`'s bucketed rasterizer, `engine_render_data.cpp`, lens effects, `vidmode.c`) sits
above it and already depends on `kfx_sim`+`kfx_platform`. This shape is non-negotiable and is
exactly what the reviewed branch violated (previous section). Phase C's split:

```
kfx_sim / kfx_render          →  produce a WorldFrame IR command buffer (pure data)
        │  (no upward calls; a plain struct handed down through the existing
        │   RendererManager façade, same shape as every other Renderer* entry point)
        ▼
kfx_platform (renderer seam)  →  IRenderer::SubmitWorldFrame(const WorldFrame&)
        │
        ├─ RendererSoftware   →  existing CPU path (do_a_gpoly_* / bflib_render_gpoly.c) — unchanged, kept as correctness reference and fallback
        └─ RendererGpu3D (new) →  vertex/index buffers + a fragment shader implementing stage 2's real blend math against RGBA textures from the shared sprite/texture cache (Phase B)
```

- **The `WorldFrame` IR struct** (naming aside; lives in `kfx_platform` next to `IRenderer.h`, same
  home as `DrawState.h`) is the *only* thing crossing the seam. It should carry already-resolved
  data — texture-cache handles (not raw sprite pointers or palette bytes), transformed or
  transform-ready vertex positions, per-vertex shade/colour values — not a redo of `engine_render.c`'s
  bucket structs verbatim. `engine_render_data.cpp`/`engine_arrays.c` (this fork's existing bucket
  producer) is the natural place to grow a "flatten the current frame's buckets into a `WorldFrame`"
  step, run *instead of* (not in addition to) the CPU rasterizer when `RendererGpu3D` is active.
- **Bucket order, kept — at least initially.** A real depth buffer is "closer to a from-scratch 3D
  renderer" than a port (visibility-ordering concern, below). The reviewed branch sidesteps this
  question by *not* answering it — it consumes the same `BucketKind`/`BUCKETS_COUNT` sort order the
  CPU path already produces and submits geometry in that order, rather than introducing a depth
  buffer. That pragmatic choice is worth copying for Phase C.1: reuse the existing bucket sort as
  the GPU submission order (painter's-algorithm draw calls, no depth test needed for opaque
  geometry, blending still respects submission order for transparency) and treat "replace bucket
  ordering with real depth buffering" as an explicit, separately-justified Phase C.5 item — it
  becomes worth doing once something needs per-pixel depth (dynamic shadows, arbitrary transparent
  geometry) rather than as a prerequisite to getting tiles and creatures on the GPU at all.
- **Fragment shader = stage 2's blend math, directly.** Because stage 2 already turned every
  table lookup into real per-pixel arithmetic (`02-32bit-software-renderer.md` §1's table), the
  fragment shader spec is now "translate `bflib_render_trig.c`'s per-fragment math into GLSL,"
  not "reverse-engineer 8-bit table semantics into a shader" the way the reviewed branch had to.
  Concretely: lighting is `colour * shade_factor` (a per-vertex or per-fragment float uniform/
  varying, no texture fetch), ghost/alpha is a standard `src·a + dst·(1-a)` blend (fixed-function
  GL blend state for the common case, shader math only where the ratio itself varies per-pixel),
  gouraud is native vertex-colour interpolation (free on any GPU, and explicitly called out in
  stage 2's own doc as "the one case where going to true colour *simplifies* the code").
- **Texture data = the [Phase B sprite/texture cache](#sprite-texture-cache), extended to cover
  static level geometry too.** Phase B already plans an RGBA cache keyed on sprite pointer +
  palette + remap (R10 below). Phase C needs the same mechanism for `engine_textures.c`'s
  `block_ptrs[]` (the wall/floor texture blocks) — same cache, same invalidation-on-palette-change
  design, just a second population path. Do not build a second, atlas-shaped cache with different
  invalidation rules; that duplication is exactly the kind of parallel-system risk this fork's
  merge-review culture exists to catch.
- **Visibility ordering**, restated from the original scoping: the bucket/sort structure
  (`engine_arrays.c`, `engine_render_data.cpp`) exists because software rasterization needs
  explicit ordering; a GPU path would more naturally use depth-buffering instead — a
  different-enough algorithm that reusing bucket order (above) is a pragmatic first step, not the
  final design. Full depth-buffering is Phase C.5.
- **The `CLASSIC` legacy HUD's compositing assumption breaks once the 3D view leaves `lbDrawSurface`.**
  Per "What the in-game-GUI project already built" (above), `GUI_ICON_PACK=CLASSIC` is a permanent,
  deliberately-CPU-only style — not something Phase C needs to bring to the GPU. But its draw calls
  (`draw_gui()`'s classic branches) currently assume they're painting sprite/box/text pixels
  directly on top of a `lbDrawSurface` that *already contains* the CPU-rasterized 3D scene, in the
  same buffer, in place. Once `RendererGpu3D` is active, the 3D scene is a separate GPU texture and
  `lbDrawSurface` no longer holds it — a `CLASSIC` HUD drawn with the old assumption would either
  paint over garbage/stale pixels or need `lbDrawSurface` cleared to a fully-transparent buffer
  first and composited as an overlay layer via `SDL_RenderTexture`, exactly the pattern the ImGui
  overlay already uses today (see "What stage 4 already built"). This needs an explicit decision,
  not a silent assumption — see R13.

### Phased delivery (each phase independently shippable, tested, and reviewable)

Deliberately smaller-grained than the reviewed branch's single "preliminary" PR, so each phase gets
its own build+test+review cycle instead of a 21k-line tail of live-discovered fixes.

| Phase | Scope | Exit criteria |
|---|---|---|
| **C.0 — Seam + IR skeleton** | `WorldFrame` IR struct in `kfx_platform`; `IRenderer::SubmitWorldFrame()` with only `RendererSoftware` implementing it (by translating the IR back into the existing CPU rasterizer call — proves the IR shape is sufficient before any GPU code exists); `engine_render_data.cpp` grows the bucket→IR flatten step. | `check_layering.py --strict` clean; CPU-rasterized output byte-identical to today's (the flatten+reconstruct round-trip changes nothing visible); Catch2 coverage for the flatten step in `kfx_render/tests/` (pure data transform, no GL/rendering needed to test it). |
| **C.1 — Static level geometry on the GPU** | `RendererGpu3D` (new, `kfx_platform`) renders tile/wall geometry only (no creatures/sprites yet) from the IR, using the extended texture cache and bucket-order submission (no depth buffer yet). Also where R13 gets decided: `CLASSIC`/`RendererGpu3D` mutual exclusion (option 1) is the default. | Visual parity screenshot pass (a lit dungeon room, a torture-room lens-effect screen, an unlit/abyss area) against the CPU path; frame-time comparison; `RendererType` selectable at runtime, CPU path unaffected when GPU path is off; enabling `RendererGpu3D` with `GUI_ICON_PACK=CLASSIC` fails clearly (R13) rather than rendering a broken frame. |
| **C.2 — Creatures, shadows, sprites** | Extend the IR + `RendererGpu3D` to cover `QK_JontySprite`/`QK_CreatureShadow`/status flowers/room flags — the sprite-heavy bucket kinds. Reuses Phase B's sprite cache directly. | Same visual-parity pass, now including a populated dungeon with multiple creature types and an active battle (shadow + status-flower overlap case). |
| **C.3 — Lens effects** | Either a GPU-shader equivalent per `LensEffect` subclass, or a documented CPU-post-pass-over-readback decision (`RendererSwapFramebufferTarget` + the `SDL_RenderReadPixels` path Phase B2 builds). `PaletteEffect` is the easy case (a shader uniform); `DisplacementEffect`/`FlyeyeEffect` need real design time. | Every lens effect (`Mist`/`Flyeye`/`Overlay`/`Displacement`/`Palette`) verified visually against the CPU path; decision recorded per-effect (GPU-native vs CPU-post-pass), not left implicit. |
| **C.4 — Compositing with Phase B** | Confirm the GPU 3D output slots into `PresentFrame` exactly where the CPU framebuffer texture does today ("Compositing is already solved," above) — no new present architecture, one `SDL_RenderTexture` source swaps for another, ImGui/2D layer composites on top unchanged. | A full session (frontend → load level → play → in-game menu → parchment map → exit) with `RENDERER_GPU3D` active shows no compositing regression versus the CPU path. |
| **C.5 — Stretch: depth buffering, render thread** | Only after C.1–C.4 have shipped and been profiled. Real depth buffering (replaces bucket-order submission) if transparent-geometry or dynamic-shadow work actually needs it; a dedicated render thread only if profiling shows the main-thread GPU submission cost is a real bottleneck, with its own R2/R12-aware design doc before any code. | Not scoped further here — deliberately, matching the "do not scope this until profiling data exists" posture the original Phase C write-up took. |

Every phase from C.0 onward should land with the same verification baseline
[00-overview.md](../00-overview.md) already sets for every stage in this series
(`build-cmake.sh` both variants, `check_layering.py --strict`, growing the relevant Catch2 suites,
a manual before/after screenshot pass) plus the phase-specific criteria above.

### Relationship to the reviewed branch, restated

`origin/feature/opengl-renderer` is **not being merged**, in whole or in part, for its 3D
world-view renderer. [`docs/merge-checks/opengl-renderer-review.md`](../../../merge-checks/opengl-renderer-review.md)
has the full review, including which of its incidental non-renderer bug fixes *were* worth taking
(and were — see that document and the corresponding small fixes already applied to
`local_camera.c`, `gui_parchment.c`, `bflib_math.c`, `bflib_crash.c`, `console_cmd.c`, and a
confirmed-dead bucket-kind cleanup in `engine_render.c`). This section is the record of what
survives from its *design*, for Phase C's own from-scratch implementation against this fork's
layering and pixel-format decisions.

---

## `RendererType` and backend selection

The original plan said `enum RendererType` (`IRenderer.h`) gains a `RENDERER_GPU` value and
`create_renderer()` (`RendererManager.cpp:32`) gains a matching case, keeping `RENDERER_SOFTWARE`
alive as a correctness reference.

That framing needs revising: **there is no longer a clean "software vs. GPU backend" split.**
`RendererSoftware` already owns an `SDL_Renderer`, already GPU-composites (ImGui), and already
presents through hardware. What each phase actually changes is *which draw categories flow through
the `SDL_Renderer` that class already has* — not a swap to a parallel backend class.

- **Phase B** (B1/B2/B3-route-2, cursor): evolve `RendererSoftware` in place. Consider renaming it
  (`RendererSDL`?) once "software" is no longer accurate, but that is cosmetic and can wait.
- **Phase C**: *this* is where a real second backend or a real config switch earns its place —
  "CPU 3D rasterizer" vs "GPU 3D rasterizer" is a genuine either/or with real correctness-reference
  value. Add a `RENDERER_GPU3D`-shaped `RendererType` value and the `create_renderer()` case
  *then*, scoped to the 3D path, not now — the 2D/UI/text seam Phase B owns is untouched. Keep the
  CPU 3D path selectable (via `RendererInit`) as the diff target and the fallback for driver
  combinations the GPU 3D path doesn't handle. (The reviewed branch's own
  `RENDERER=SOFTWARE`/`RENDERER=OPENGL` config switch, kept CPU-default, is independent validation
  that this is the right shape.)
- `-classicmenu` provided a "no ImGui, CPU-composite frontend" mode for bisecting 2D-layer
  regressions -- **retired 2026-09-12** (docs/refactor/ingame-gui/00-overview.md §1/§8), the
  frontend has no legacy path left to fall back to at all, so this specific bisect mode no longer
  exists. `GUI_ICON_PACK=CLASSIC` (`ingame_gui_use_classic_hud()`) still forces the *in-game HUD*
  to its legacy sprite renderer, which may or may not be a close enough substitute for whatever
  Phase B needed this for -- revisit when Phase B is actually picked up.

Update [architecture.md](../../../Architecture/architecture.md) §2.1/§2.4 if/when Phase C adds the
`RendererType` value and changes `IRenderer`'s framebuffer contract.

---

## Non-goals for this stage

- **No change to `kfx_frontend`'s screen/menu authoring.** Phase B changes what happens *behind*
  the draw calls, not how screens are authored (same principle as stage 1). Note that for the
  frontend, stage 4 already moved authoring to ImGui — this stage does not touch it further.
- **The in-game-GUI-as-ImGui port was never this stage's job, and it's done.** It shipped as its
  own project ahead of this stage (see "What the in-game-GUI project already built") — B3 route 1,
  as originally scoped. Its permanent `CLASSIC` legacy-HUD carve-out is likewise not something this
  stage migrates; Phase C only has to decide how it composites once the 3D view is on the GPU
  (R13), not whether to port it.
- **Phase C is scoped, but not committed to a start date.** Scoping it in detail (above) makes it
  ready to pick up quickly; it does not move its timeline ahead of Phase B, and the original
  guidance to profile Phase B's shipped state before investing further still stands.
- **Phase C's document is not a final API.** `WorldFrame`/`RendererGpu3D` names and shapes are
  illustrative of the boundary, not a locked interface; the real design should happen at C.0 with
  actual code in hand.
- **Phase C's document is not a UI/2D-layer change.** Phase B (already in flight conceptually,
  mostly superseded by stage 4's ImGui work) is unaffected by anything in Phase C.

---

## Risks and things that will bite

Found by reading the current code (2026-09-06), roughly worst-first. Several are pre-existing
consequences of stages 2/4 that this stage is simply the first to *have* to resolve. R12/R13 were
added 2026-09-13 alongside Phase C's detailed scoping; R9 was marked resolved-by-outcome the same
day once it became clear B3 shipped via route 1, not route 2.

### R1 — The present path is called from ~20 sites, in many libraries, and each one runs a full ImGui frame

`RendererPresentFrame()` call sites, all of which now trigger
`ImGuiContextNewFrame` → `RendererRunImGuiFrameCallback` → `ImGuiContextRender`:
`kfx_apploop/game_session_loop.cpp` (×7), `kfx_net/net_exchange_gameplay.c`,
`kfx_net/packets_misc.c` (pause/unpause resync), `kfx_frontend/{front_fmvids,front_landview,
front_network,front_simple,frontend}.cpp/.c`, `kfx_platform/{bflib_video.c,bflib_fmvids.cpp}`,
`kfx_render/vidmode.c` (×2).

- The `s_presenting_imgui_frame` reentrancy guard only *suppresses* ImGui on nested calls — a
  nested present shows a frame with **no UI overlay**. That's fine for palette-fade steps (what it
  was built for); it may not be fine for a nested present during a cutscene or a net resync.
- `RendererRunImGuiFrameCallback()` also drains `s_pending_state` / `s_pending_load_slot` /
  `s_pending_action` (`frontgui_screens.cpp`). Those now execute from **whichever present fires
  first each frame** — including a resync present deep inside `kfx_net`. Any Phase B in-game HUD
  submission added to that callback inherits this: it must be null-safe when invoked re-entrantly,
  mid-netsync, and mid-cutscene, not just from the two normal loop bodies.
- Anything Phase B/C adds to `PresentFrame` (a capture read-back, a second composited pass, an
  in-game HUD submit) must be explicitly audited against every one of these call sites, not just
  the two obvious ones.
- **Mitigation, sequenced ahead of Phase B:**
  [05-imgui-linkage-consolidation.md](../05-imgui-linkage-consolidation.md) moves all ImGui knowledge
  into `kfx_frontend` behind one `RendererOverlayCallbacks` struct, so the overlay begin/submit/
  render is a single choke point and Phase B's additions live in one library instead of straddling
  the `kfx_platform`/`kfx_frontend` seam. It does *not* reduce the call-site count itself (that's a
  separate optional cleanup, noted there).

### R2 — `SDL_Renderer` is single-threaded; keep every present on the main thread

SDL's renderer API is main-thread-only. `kfx_net` spawns `SDL_Thread`s
(`net_matchmaking.c`, `net_portforward.cpp`) but those do socket/DNS work and do **not** call
`RendererPresentFrame` — the present calls in `net_exchange_gameplay.c`/`packets_misc.c` are on the
main game-loop thread. This holds today; it is an invariant Phase B/C must not break (e.g. by
moving capture read-back or texture uploads to a worker). Worth a one-line assert or comment at the
seam. **Phase C specifically**: a dedicated render thread (as the reviewed upstream branch built,
see Phase C's "What we are explicitly not taking") is the most direct way to break this invariant —
see R12.

### R3 — Dynamic-texture lifetime across `SDL_Renderer` recreation is unmanaged

`ImGuiContextCreateTexture()` returns raw `SDL_Texture*` handles that the context does **not**
track; `shutdown_backends()` frees only `s_cursor_texture`. `ensure_present_target()` recreates
`m_renderer` (and tears down + rebuilds the ImGui context) whenever
`SDL_GetRenderWindow(m_renderer) != lbWindow` — i.e. on a genuine `SDL_Window` handle change. Any
frontend-cached dynamic-texture handle (the land-preview panel today; more in Phase B/C) then
dangles into a freed renderer → use-after-free on the next `ImGui::Image`/`UpdateDynamicTexture`.

Latent right now because `INGAME_RES` is restart-only and mid-session fullscreen/resize
(`bflib_video.c:551-605`) reuses the same `SDL_Window` (so `m_renderer` survives, only `m_texture`
is resized). Phase B (cursor unification) and Phase C (more long-lived GPU textures) raise the
stakes. Fix: a tracked-texture registry in `ImGuiContext` with invalidate-on-recreate, or a
`RendererDynamicTextureInvalidated` callback the frontend subscribes to.

### R4 — `RendererSwapFramebufferTarget` is a fragile, non-nesting shared primitive

Single-level save/restore via file-scope `s_saved_screen_width/height` (`RendererManager.cpp:159`),
no nesting support, already had one stride bug fixed in stage 4 (`318d55f2c`). Callers today: the
eye-lens effect (`thing_creature.c`), the ImGui cursor capture, the land-preview capture. Phase B's
cursor unification and Phase C's "lens as a CPU post-pass over the read-back GPU frame" both add
callers. If any two overlap in one frame (a lens effect on a frame that also rebuilds the cursor
texture) the save/restore silently corrupts screen stride. Phase B should convert it to a real
save/restore stack, or document + assert the non-overlap invariant.

### R5 — VSync and the manual frame limiter both run

`SDL_SetRenderVSync` is toggled from `vsync_enabled` (`RendererSoftware.cpp:59`), and
`game_session_loop.cpp` also has a `tick_ns_one_frame` + `LbSleepFor` software limiter. With both
active, frame pacing and input-to-photon latency can beat against each other. Not introduced here,
but Phase A already changed presentation cost and B/C change frame timing more — so verification
must measure **frame-time distribution with vsync on and off**, plus a `SDL_RenderReadPixels`
screenshot's stall cost under vsync (it blocks up to a frame → visible hitch on capture).

### R6 — Phase C breaks the "SDL lives in kfx_platform" convention

`check_layering.py` allows `kfx_render` → `kfx_platform` includes, and SDL headers are
unconstrained, so a GPU 3D path in `engine_render.c` is not a *layering* violation. But
`SDL_Renderer` usage is by convention confined to `kfx_platform`'s renderer seam. Phase C needs a
new geometry/vertex-buffer submission façade on `RendererManager` (a real widening of the seam,
with its own `KfxDrawState`-style descriptor for the shade/blend uniforms) — design that interface
before committing, rather than letting `engine_render.c` reach for SDL directly. The reviewed
branch's `GpuResourceDesc`/`GpuResourceHandle` shape (Phase C, "What's worth taking") is a
reasonable starting point for this façade's design.

### R7 — Two cursor draws still coexist

The legacy `bflib_mspointer.cpp` path still blits the software cursor into `lbDrawSurface`
**unlocked**, and the ImGui overlay draws its own cursor via `GetForegroundDrawList()->AddImage()`
while `WantCaptureMouse`. Stage 4 killed the *visible* double-cursor by syncing positions, but both
still render. A post-composite `SDL_RenderReadPixels` screenshot (B2) can therefore capture two
cursors depending on timing. Phase B's cursor unification is the real fix; until it lands, capture
tests will show this.

### R8 — FMV / cutscene frames get ImGui submission too

The ImGui pipeline is unconditional now (`RendererImGuiEnabled()`'s old session-global gate is
retired), so `bflib_fmvids.cpp:510` and `front_fmvids.c` present Smacker frames with
`FrontendImGuiFrame()` still running `FeStyleSheetFrame()` (the `-imguistyle`
debug overlay) and the deferred-action drain — if anything, more certainly than when this risk was
first written, since there's no toggle left to disable it. Low severity today (the error box is
gated behind `frontend_imgui_screen_active`), but any Phase B in-game HUD submission needs an explicit
"suppress during video/intro states" gate or it will draw over cutscenes.

### R9 — `imgui_impl_sdlrenderer3` batching may not scale to an in-game HUD — **resolved by outcome, kept for history**

Written when B3 route 2 (a hand-rolled ImGui-draw-list HUD compositor) was still a live option; it
warned that `SDL_RenderGeometryRaw`'s per-draw-command texture/clip-rect overhead might not
coalesce hundreds of small HUD quads as well as a purpose-built atlas path. Moot now: B3 route 1
shipped instead (the in-game-GUI-as-ImGui project, see "What the in-game-GUI project already
built"), and it uses the same widget-level ImGui submission the frontend already does, not a
hand-rolled draw-list compositor — so this specific batching concern was never exercised in the
form this risk anticipated. Live HUD frame-time is now a real-build measurement question, not a
speculative one; if it ever becomes a problem, it's a stage on its own, not a re-litigation of B3.

### R10 — Sprite→texture cache invalidation is the hard part

The cache key must include the active palette / remap table, and both frontend fades
(`ProperFadePalette` mutating the active palette per animation step) and in-game lighting churn it.
A naïve pointer-only key returns stale colours after any fade; a per-frame-palette key defeats the
cache. Needs a generation counter on `RendererPaletteSet` and eviction keyed on it — design this
alongside stage 2 §2a's per-draw expansion cache if that was built. **Phase C inherits this
directly**: its static-level-texture cache entries need the same generation-counter invalidation as
sprite entries, since level lighting also mutates the active palette.

### R11 — `RendererClearScreen(colour)` still takes a palette index

`RendererClearScreen` resolves an 8-bit index through the palette (`RendererSoftware.cpp:34`);
every caller passes a literal (`0`, `144`). Any GPU clear path in Phase C must preserve that
contract or update all call sites — easy to miss because the signature type doesn't change.

### R12 — A dedicated render thread is an unproven pattern for this codebase (new, Phase C)

The reviewed upstream branch runs its GL renderer on its own thread (`RenderThreadManager`,
`Signal()`/`WaitForCompletion()` synchronization with the main game loop), and its own commit
history doesn't show that pattern being stress-tested against this engine's specific present-path
complexity (R1's ~20 call sites, R3's texture-lifetime issue, R4's non-nesting shared primitive) —
it's simply untested by anyone but the branch's single author, on a codebase in a different
(pre-refactor, pre-ImGui) state than this fork's. Introducing a render thread here would need to
answer, before any code: which of R1's ~20 present call sites can tolerate the main thread not
blocking on GPU submission; whether `RendererSwapFramebufferTarget` (R4) and the dynamic-texture
registry (R3) are thread-safe to touch from two threads at once; and how `s_presenting_imgui_frame`
(R1) generalizes to "GPU work in flight on another thread" rather than just "nested call on this
thread." None of this is a reason a render thread can't work — it's a reason it needs its own
design pass with this codebase's actual present-path shape in hand, not adoption because an
external branch happened to include one. See Phase C.5.

### R13 — `GUI_ICON_PACK=CLASSIC`'s compositing assumption doesn't survive Phase C unmodified (new, Phase C)

Per the Architecture section's note (above): the legacy sprite HUD draws directly into
`lbDrawSurface`, assuming that buffer already holds the rendered 3D scene underneath it — true
today (CPU 3D rasterizer, same buffer, in-place compositing) and false once `RendererGpu3D` owns
the 3D view (its own GPU texture, `lbDrawSurface` no longer the visible scene). Two ways to resolve
this, and Phase C.1 needs to pick one explicitly rather than discover the gap live:

1. **Make `CLASSIC` and `RendererGpu3D` mutually exclusive.** Selecting the GPU 3D path forces
   `GUI_ICON_PACK` off `CLASSIC` (or refuses to enable `RendererGpu3D` while `CLASSIC` is active),
   with a clear config-time error rather than a silently broken frame. Simplest, smallest, and
   consistent with `CLASSIC` being a deliberate, narrow style choice rather than a first-class
   target every renderer combination must support.
2. **Redirect `CLASSIC`'s draws through an overlay-texture pattern**, matching what ImGui already
   does: render into an off-screen, alpha-cleared buffer, then composite it via `SDL_RenderTexture`
   after the GPU 3D pass, before the ImGui layer. More consistent (every renderer combination
   works), more work (every classic-HUD draw call's "assume opaque black background" behaviour —
   the same class of issue R11 flags for `RendererClearScreen` — needs auditing for whether it now
   needs to preserve alpha=0 instead of painting over the frame).

Recommend option 1 for Phase C.1–C.4 (cheap, unblocks everything else) and revisit option 2 only if
real user demand for "`CLASSIC` HUD + GPU 3D view" together shows up — matching this document's
general bias toward not building compositor infrastructure nobody asked for (see B3's history).

---

## Verification

- **Phase A:** done — nothing to verify beyond the stage-2 regression pass that already covered it.
- **Phase B1:** frame-time measurement (around `PresentFrame` and `frontend_copy_background`)
  before/after; screenshot-identical frontend output (B1 changes nothing visible).
- **Phase B2:** a scheduled screenshot must contain the ImGui overlay (main menu, an Options tab,
  a migrated list screen) — it does not on the current build. Movie capture: either the feature is
  gone (verify the menu entry / `-recordmovie` path reports so cleanly, no silent flag set), or the
  re-implemented capture produces a playable file. Screenshot format/row-order correct on both
  toolchains. Screenshot taken with vsync on does not corrupt or stall beyond one frame.
- **Phase B3:** verified as part of the in-game-GUI-as-ImGui project itself — nothing to add here.
- **Cursor:** exactly one cursor visible at all times, at the tracked position, in the frontend, in
  the ImGui in-game HUD, and with `GUI_ICON_PACK=CLASSIC` selected; a B2 screenshot shows one
  cursor in every case.
- **Reentrancy / unusual present sites (R1):** trigger a screenshot during a palette fade, during
  Smacker cutscene playback, and during a multiplayer pause/unpause resync — no crash, no
  half-composited capture, no double-applied pending state.
- **Renderer recreate (R3):** on platforms where a fullscreen transition recreates the `SDL_Window`,
  toggle fullscreen on a screen that owns a dynamic texture (land preview) and confirm no
  use-after-free.
- **Phase C:** see the per-phase exit criteria in the "Phased delivery" table above — each phase
  (C.0–C.4) has its own layering/build/visual-parity/frame-time gate; C.5 is intentionally
  unscoped pending real profiling data from C.1–C.4.
- `KFX_OS=linux ./build-cmake.sh` + mingw cross-compile (both variants) after each phase.
- `python3 scripts/check_layering.py --strict` — no new cross-library `#include` should be needed;
  the ImGui-adjacent façades on `RendererManager.h` (Phase B) and the new geometry-submission
  façade (Phase C, R6) are the extension points.
- Test on both the mingw/Windows and native-Linux SDL3 targets — `SDL_Renderer`'s backing API
  differs by platform. Stage 4 has been exercising both through `imgui_impl_sdlrenderer3`, so a
  platform-specific regression here is more likely to show up as a *capture* (`SDL_RenderReadPixels`
  format/flip) difference than a compositing one.

## Cross-references to update once implemented

- [04-imgui-gui-foundation.md](../04-imgui-gui-foundation.md) §8 ("Screenshots will miss the ImGui
  layer") — mark resolved once Phase B2 lands.
- [04-imgui-gui-foundation.md](../04-imgui-gui-foundation.md) §2.3 / §10 ("in-game GUI … revisited
  after the GPU work") — **done**: the in-game-GUI-as-ImGui project shipped B3 route 1 ahead of
  this stage, not after it. Mark resolved and correct the "after the GPU work" framing there too.
- [docs/refactor/ingame-gui/00-overview.md](../../ingame-gui/00-overview.md) — the project that
  shipped B3; this document's "What the in-game-GUI project already built" section summarizes it.
  If that project's own scope changes (e.g. `CLASSIC` gets deprecated after all), update both.
- [architecture.md](../../../Architecture/architecture.md) §2.1/§2.4 — revise if Phase C adds a
  `RendererType` value or changes the framebuffer contract.
- [00-overview.md](../00-overview.md) roadmap table — point stage 3's row at this document (see
  its own header note); Phase A row can be marked done.
- [02c-post-migration-audit-and-refactor-opportunities.md](../02c-post-migration-audit-and-refactor-opportunities.md)
  — the "FLC movie recording is dead post-stage-2" finding (§B2) belongs there too if it isn't
  already recorded; it is a stage-2 consequence this stage inherits, not a stage-3 change.
- [`docs/merge-checks/opengl-renderer-review.md`](../../../merge-checks/opengl-renderer-review.md)
  — the source review Phase C's design section draws on; update this document, not that one, if
  Phase C's actual implementation diverges from what's scoped here.
