# Stage 3 — GPU acceleration

Status: **planning, not started.** Depends on [02-32bit-software-renderer.md](02-32bit-software-renderer.md).
See [00-overview.md](00-overview.md) for context.

## Goal

Move drawing work onto the GPU without a single risky rewrite. Phased, each phase independently
shippable and each smaller than the last major stage:

## Phase A — Direct texture upload (small, low-risk, do first)

Once stage 2 lands, the CPU raster buffer is already RGBA. `RendererSoftware::PresentFrame`'s
current `SDL_BlitSurface` INDEX8→RGBA conversion (`RendererSoftware.cpp:134`) becomes unnecessary
— replace the lock/blit/unlock present path with a direct `SDL_UpdateTexture` (or keep the
streaming-texture lock if profiling favors it; `SDL_UpdateTexture` is simpler and likely
sufficient at KeeperFX's resolutions). This phase is mechanical and should ship as its own small
change immediately after stage 2, not bundled with anything below — it's real, measurable
GPU-upload-cost reduction with no rendering-behavior risk.

## Phase B — 2D compositing on the GPU (GUI, sprites, text)

This is where "GPU acceleration" starts being real, not just faster presentation. The target is:
`IUIRenderer`/`ITextRenderer` gain a second implementation that submits textured quads to the GPU
(via SDL3's own hardware-accelerated `SDL_Renderer`, already in use for presentation — see
[00-overview.md](00-overview.md)) instead of writing into the CPU buffer stage 2 produces.

- Sprites become GPU textures, uploaded once (with a cache keyed on sprite pointer + expansion
  palette, extending stage 2 §2's cache if one was built) and drawn via `SDL_RenderTexture`
  calls — this is a natural fit for `SDL_Renderer`'s existing batching, no raw GL/Vulkan needed
  for this phase.
- Text (`ITextRenderer::DrawTextResized`) becomes glyph-atlas + textured-quad rendering, same
  mechanism.
- Solid boxes and slab-background tiling (`IUIRenderer::SubmitSolidBox`/`SubmitSlabBackground`)
  become filled-rect/tiled-quad GPU draws.
- **The 3D dungeon view stays CPU-rasterized in this phase** — it renders into the stage-2 RGBA
  buffer as before, which is then uploaded as one texture and composited *underneath* the
  GPU-drawn 2D layer (GUI panels, HUD, cursor) in a single `SDL_Renderer` pass. This hybrid is
  deliberately the stopping point for this phase: it gets the highest-frequency, highest-count
  draws (GUI chrome, text, HUD icons — hundreds of small draws per frame, the shape GPU batching
  helps most with) off the CPU, while deferring the much harder 3D rasterizer port to Phase C.
- [04-imgui-gui-foundation.md](04-imgui-gui-foundation.md) evaluates Dear ImGui specifically as
  *this phase's* `IUIRenderer`/`ITextRenderer` implementation — read it before starting Phase B,
  since the recommendation there (build the ImGui-backed implementation as an alternative to a
  hand-rolled `SDL_Renderer`-quad one) changes this phase's actual task list.
- **The mouse cursor needs explicit handling, not an assumption it's already covered.**
  [01-close-the-seam.md](01-close-the-seam.md) Part D found that cursor rendering
  (`bflib_mspointer.cpp`'s `LbI_PointerHandler`) is *not* routed through `IUIRenderer` today — it
  composites via `LbScreenSurfaceBlit`'s direct `SDL_BlitSurface` calls against `lbDrawSurface`,
  a complete parallel path that survives stage 2 unchanged only because it's still `SDL_Surface`
  vs. `SDL_Surface` blitting. That assumption breaks the moment this phase's `IUIRenderer`
  implementation stops writing into an `SDL_Surface`-backed CPU buffer as its primary target.
  Two ways to close this, decide when this phase is actually scoped in detail: (a) give
  `bflib_mspointer.cpp` its own `IUIRenderer`-routed draw call for the first time (the cleaner
  long-term fix, consistent with everything else this phase moves to the GPU), or (b) keep a
  small CPU-side `SDL_Surface` mirror specifically for cursor compositing if routing the cursor
  through the general submission path turns out to be awkward (it updates every mouse-move frame,
  a different cadence than static sprite/text draws).

## Phase C — 3D polygon rasterizer on the GPU (stretch goal, separately scoped)

Porting `engine_render.c`'s bucketed gouraud/textured polygon fill
(`do_a_gpoly_*` family) to real GPU draw calls (vertex/index buffers + a fragment shader
replicating the stage-2 blend math) is a substantially larger and riskier undertaking than
Phases A/B:

- The renderer's bucket/sort structure (`engine_arrays.c`, `engine_render_data.cpp`) exists
  because software rasterization needs explicit visibility ordering; a GPU path would use
  depth-buffering instead, which is a different-enough algorithm that this isn't a mechanical
  port — it's closer to a from-scratch 3D renderer that has to reproduce the original's specific
  visual behavior (the lens-effect system, `LensManager`/`LensEffect` hierarchy, currently
  operates on the CPU framebuffer as a post-process; each of `MistEffect`/`FlyeyeEffect`/
  `OverlayEffect`/`DisplacementEffect`/`PaletteEffect` needs its own GPU-shader equivalent or a
  documented decision to keep lens effects as a CPU post-pass over an already-GPU-rendered frame).
- Recommend: **do not scope this in detail until Phases A/B have shipped and been played with.**
  The value of Phase B alone (2D compositing off the CPU, 3D view still correct) may be
  sufficient for KeeperFX's actual performance needs, given the game's fixed low-poly art style
  and the fact that the CPU-rasterized 3D view already runs acceptably today. Revisit Phase C's
  cost/benefit once real profiling data exists from Phase B's shipped state, rather than
  committing to a full 3D-GPU rewrite speculatively.

## `RendererType` and backend selection

`enum RendererType` (`IRenderer.h`) gains a value for the GPU-composited backend (name TBD —
`RENDERER_GPU` or similar), and `create_renderer()` (`RendererManager.cpp:29-36`) gains the
matching case. Keep `RENDERER_SOFTWARE` alive and selectable (via `RendererInit`) rather than
deleting it once the new backend exists — it's the correctness reference to diff against during
Phase B/C development, and a reasonable fallback for any platform/driver combination the GPU path
doesn't support well.

## Non-goals for this stage

- No change to `kfx_frontend`'s screen/menu authoring — Phase B changes what happens *behind*
  `IUIRenderer`, not how `kfx_frontend` calls it (same principle as stage 1).
- Phase C is explicitly not committed — see above.

## Verification

- Phase A: presentation-cost measurement (frame time around `PresentFrame`) before/after,
  screenshot-identical output (this phase changes nothing visible).
- Phase B: screenshot-diff pass across every GUI screen (main menu, in-game HUD tabs, all
  `frontmenu_*` screens) plus a frame-time comparison against the stage-2 baseline — the actual
  point of this phase is measurable, not just structural.
- `KFX_OS=linux ./build-cmake.sh` + mingw cross-compile after each phase.
- `python3 scripts/check_layering.py --strict`.
- Test on both the mingw/Windows and native-Linux SDL3 targets — `SDL_Renderer`'s backing API
  differs by platform (Direct3D/Vulkan on Windows, typically OpenGL/Vulkan on Linux), and this is
  exactly the kind of change where a platform-specific `SDL_Renderer` quirk could pass on one
  target and fail on the other.
