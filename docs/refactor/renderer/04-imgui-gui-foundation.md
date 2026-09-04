# Stage 4 — Dear ImGui as a GUI rendering foundation

Status: **planning, not started.** Depends only on
[01-close-the-seam.md](01-close-the-seam.md) — can run in parallel with stages 2–3, though it
targets the same seam point stage 3 Phase B does (`IUIRenderer`/`ITextRenderer`), so read
[03-gpu-renderer.md](03-gpu-renderer.md) first if sequencing both. See
[00-overview.md](00-overview.md) for context.

## Two different questions, kept separate on purpose

The prompt for this investigation asked to "consider if the GUI/menu backends could be replaced
by an ImGui-driven one, while maintaining look and feel... possibly using the imgui render as a
foundation for the more general 32-bit/GPU upgrade." Those are two different-sized changes wearing
one name, and conflating them is how a good idea (accelerated rendering) gets tangled with a risky
one (visually re-skinning 25 years of hand-authored UI). This doc treats them separately and
recommends different dispositions for each.

## Recommended: ImGui as the Phase B rendering backend, `kfx_frontend` untouched

**What this means concretely**: a new class, `ImGuiUIRenderer : IUIRenderer` (and
`ImGuiTextRenderer : ITextRenderer`), becomes an alternative implementation behind the same seam
stage 3 Phase B already targets. `kfx_frontend`'s ~272 `Lb*Draw*` call sites, its screen/menu
authoring (`GuiButtonInit` arrays, the `frontmenu_*` state machines), and its visual output are
**unchanged** — only what happens after a submission reaches `RendererManager` differs: instead
of an `SDL_Renderer`-quad implementation (stage 3 Phase B's other option), submissions become
Dear ImGui draw-list commands (`ImDrawList::AddImage` for sprites, glyph rendering for text,
`AddRectFilled` for solid boxes), batched and flushed through ImGui's own backend renderer each
frame.

**Why this satisfies "maintaining look and feel" by construction, not by careful re-skinning**:
KeeperFX's actual visual authority — sprite art, panel layout, button positions, fonts — stays
entirely in `kfx_frontend` and the existing `.dat` asset sheets. ImGui here is used purely as a
**batched textured-quad renderer with a GPU backend already written and tested by someone else**,
not as a UI toolkit. No ImGui widget (`ImGui::Button`, `ImGui::Begin`, default styling/theming)
is ever invoked. This is a legitimate and common use of Dear ImGui — its draw-list API is
decoupled from its widget layer specifically to support exactly this "bring your own layout,
borrow the renderer" pattern.

**Why this is a reasonable Phase B choice over a hand-rolled `SDL_Renderer`-quad implementation**:
mainly reduced implementation risk, not necessarily raw performance — Dear ImGui's draw-list/
batching/atlas code is mature and widely deployed, so Phase B's actual new work shrinks to "write
`ImGuiUIRenderer`'s ~7 `IUIRenderer` methods as draw-list calls" rather than also writing and
debugging a batching/atlas layer from scratch. It's a reasonable default, not a requirement —
the plain `SDL_Renderer`-quad path in [03-gpu-renderer.md](03-gpu-renderer.md) Phase B remains a
valid, slightly-more-work alternative, and the choice is worth revisiting with a small spike of
both before committing.

### Integration shape (from investigating the local `imgui_bundle` checkout)

`/home/robin/projects/flows/third_party/imgui_bundle/` was surveyed as a reference for how to
plug Dear ImGui into a CMake/SDL project. Conclusion: **don't use imgui_bundle's own CMake
target or the `hello_imgui`/`immapp` wrapper layer it builds on.**

- `imgui_bundle_add_app` links the monolithic `imgui_bundle` CMake target, which by default pulls
  in `hello_imgui`, `immapp`, markdown rendering, NanoVG, a texture inspector, ImGuizmo, a file
  dialog, a node editor, ImPlot/ImPlot3D, ImAnim, and (optionally) OpenCV bindings — none of
  which KeeperFX needs; it wants exactly the draw-list renderer.
- **`hello_imgui`'s runner layer is SDL2-only.** `external/hello_imgui/hello_imgui/CMakeLists.txt`
  only defines `HELLOIMGUI_USE_SDL2`, and the only SDL runner implementation present is
  `src/hello_imgui/internal/backend_impls/runner_sdl2.cpp` — there is no `runner_sdl3.cpp` and no
  SDL3 option anywhere in that layer, despite KeeperFX already being on SDL3 throughout
  (`kfx_platform`'s `WindowSystemSDL`, `RendererSoftware`'s `SDL_Renderer`). Going through
  `hello_imgui` would mean either backporting to SDL2-specific window/event handling or fighting
  an unsupported configuration.
- **The actual reusable material is four files**, vendored directly (bare Dear ImGui, not through
  imgui_bundle's build): `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`
  (core — `imgui_widgets.cpp` is needed even though no widgets are used, since some draw-list
  helpers live there) plus their headers, and exactly two backend files:
  `imgui_impl_sdl3.cpp`/`.h` (platform: window/input glue for SDL3) and
  `imgui_impl_sdlrenderer3.cpp`/`.h` (renderer: draws ImGui draw lists via `SDL_Renderer`). Both
  are present as plain upstream files under `external/imgui/imgui/backends/` in the checkout
  (vendored from the `pthom/imgui.git` fork, branch `imgui_bundle` — a fork that tracks upstream
  Dear ImGui closely; diff against mainline `ocornut/imgui` before vendoring to confirm no
  bundle-specific patches leaked into these particular files).
- Choosing `imgui_impl_sdlrenderer3` (over `imgui_impl_opengl3`) matters: it targets the same
  `SDL_Renderer` `RendererSoftware.cpp` already owns for presentation (`ensure_present_target()`,
  `RendererSoftware.cpp:44-80`), so `ImGuiUIRenderer` can draw into the *same* renderer instance
  the 3D-view texture (stage 3 Phase B) is composited through — one `SDL_Renderer`, one present
  call, no second graphics context/API to stand up. This is the direct enabler of "possibly using
  the imgui render as a foundation for the more general 32-bit/GPU upgrade" from the original ask
  — it's not a separate rendering stack bolted on, it's the same GPU compositing path stage 3
  already needs, with Dear ImGui supplying the batching/atlas machinery for it.
- License: MIT (imgui_bundle root `LICENSE`, and upstream Dear ImGui is MIT — consistent with
  KeeperFX's existing third-party dependency posture).
- **Font/glyph atlas**: `ITextRenderer`'s current implementation defers to `bflib_sprfnt`'s
  existing bitmap-font rasterization (`LbTextDrawResizedImmediate`). An `ImGuiTextRenderer` has a
  choice: bake KeeperFX's existing bitmap fonts into an ImGui font atlas (`ImFontAtlas`, preserves
  exact glyph appearance) rather than substituting a different font-rendering technique — this is
  the same "look and feel" principle as the sprite path, and should be treated as a hard
  requirement, not a nice-to-have, for this sub-piece.

## Not recommended (for now): replacing hand-rolled menu code with real ImGui widgets

A different, larger idea: instead of `kfx_frontend` submitting its own sprite-drawn buttons/lists
through `IUIRenderer`, use actual `ImGui::Button`/`ImGui::ListBox`/etc., restyled to visually
approximate DK's UI.

**Why not, as the default plan**: Dear ImGui's styling system (colours, rounding, padding,
per-widget theming via `ImGuiStyle`) is built around vector-drawn, procedurally-styled widgets. It
does not naturally reproduce hand-painted 9-slice sprite panels, the specific scroll-box border
art `frontend_draw_scroll_box` composes from 6 distinct decorative sprite segments (documented in
[`colordepth/00-notes.md`](../gui/colordepth/00-notes.md)'s "second, independent instance of
'designed for one shape'" section), or the exact button/font sprite art the game has always used.
Getting a restyled ImGui widget to be visually indistinguishable from KeeperFX's existing chrome
would be a large, open-ended art/styling effort with a real risk of landing at "recognizably
similar" rather than "the same" — the opposite of "maintaining look and feel." It would also
discard real, working investment: `frontmenu_selectlist.h`/`frontmenu_settingctrl.h`'s generic
list/control engines (`docs/refactor/gui/00-overview.md`) already solved the duplication problems
a widget-toolkit migration would otherwise be pitched as fixing.

**Where real ImGui widgets *are* a good fit, separately**: developer/debug-facing UI that has no
existing pixel-art identity to preserve and where iteration speed matters more than visual
polish — a live `keeperfx.cfg` options/cvar inspector (the kind of surface
`docs/refactor/stage-10-kfx-frontend.md`'s Phase 2 settings-control engine anticipated needing
once more config options get exposed), in-game debug overlays, or a level/scripting inspector.
These could reasonably use genuine ImGui widgets in default or lightly-themed style, built on the
exact same `ImGuiUIRenderer` renderer-integration work above, without any expectation of matching
the game's in-fiction UI. **Recommend scoping this as a separate, later, opt-in initiative** (not
part of this renderer refactor) if/when such a debug surface is actually needed — flagging it here
so the rendering-foundation work above is understood to enable it cheaply later, not because it's
committed now.

## Non-goals for this stage

- No change to `kfx_frontend` menu authoring, layout, or asset usage.
- No adoption of ImGui's default widget/window chrome for player-facing UI.
- No dependency on `imgui_bundle`, `hello_imgui`, or `immapp` — bare Dear ImGui + two backend
  files only.

## Verification

- Screenshot-diff every GUI screen against the stage-2/3 baseline (Phase B's other candidate
  implementation, or the pre-Phase-B software output) — this stage's whole premise is "identical
  output, different renderer," so any visible difference is a bug, not an acceptable stylistic
  drift.
- Font rendering specifically: compare glyph-for-glyph against `bflib_sprfnt`'s existing output at
  several `units_per_px` scale factors (the resize path `DrawTextResized` exists for), since font
  atlas baking is the piece most likely to introduce subtle kerning/hinting differences.
- `KFX_OS=linux ./build-cmake.sh` + mingw cross-compile — confirm the vendored ImGui core +
  `imgui_impl_sdl3`/`imgui_impl_sdlrenderer3` files build cleanly under both toolchains (MSVC/
  clang-cl via vcpkg and mingw-w64), since this is new third-party C++ source entering the build
  for the first time in `kfx_platform`.
- `python3 scripts/check_layering.py --strict`.
