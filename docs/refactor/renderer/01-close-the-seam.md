# Stage 1 — Make the renderer seam load-bearing

Status: **complete (2026-09-03).** All four parts implemented, build/test-verified, and manually
verified in-game — including the crash Part A introduced and then fixed (caught only by manual
testing; see its revision note below for what that changed about this stage's verification bar).
Part D required investigation and documentation rather than a code change (see its section for
why). See [00-overview.md](00-overview.md) for context — this is the prerequisite for stages 2–4.

> **Revision note (2026-09-03):** the original pass through this stage's scope was grep-only and
> undercounted the bypass surface materially — a Tier-3 codebase-graph audit found roughly double
> the raw-`WScreen` call sites within `kfx_frontend` alone (14 across 7 files, not 7/5), plus
> several bypass patterns and files the first pass never looked at: an *unlocked* cursor blit, a
> pointer-alias pattern no substring grep would catch, a cross-layer framebuffer swap inside
> `kfx_sim`, `engine_redraw.c` writing pixels directly (previously flagged only for its layering
> violations, not this), and a second, independent surface-lock path for the mouse cursor. Parts
> A–D below reflect the corrected scope; see [00-overview.md](00-overview.md)'s "What's still
> genuinely 8-bit, and where" section for the full citation list and a note on the graph
> coverage gap that made the first pass's grep necessary to re-verify by hand.

## Goal

Today `IRenderer`/`IUIRenderer`/`ITextRenderer` are a real interface boundary with exactly one
implementation, and that implementation is the pre-seam code unchanged. Three problems follow
from that, and this stage fixes all of them before any format or backend work starts:

1. **The seam doesn't cover the pixels that matter.** `engine_render.c`'s bucketed
   polygon/sprite rasterizer, `engine_textures.c`, and `engine_redraw.c` write directly into the
   global `vec_screen`/`lbDisplay.WScreen` pointers, never touching `IUIRenderer`. This is the 3D
   dungeon view — the majority of on-screen pixels most frames. A second `IRenderer` backend
   (32-bit software, then GPU) can't intercept this traffic because there's no virtual call to
   override; it would have to keep reaching into the same raw globals, defeating the point of a
   backend abstraction.
2. **Considerably more of the GUI bypasses the seam than a first pass found** — 14 raw
   `lbDisplay.WScreen[]` pokes across 7 `kfx_frontend` files, not 7 across 5 — plus bypasses in
   `kfx_apploop`, `kfx_sim`, and three more `kfx_platform` files (movie playback, the mouse
   cursor, and text-clipping) that were never in scope at all. Each is a place a future backend
   would silently produce wrong (or, for the unlocked cursor blit, potentially unsafe) output
   with no compiler error to catch it.
3. **A second surface-lock discipline exists outside the seam's own lock/unlock pair** — the
   mouse cursor's backup/restore surfaces go through `LbScreenSurfaceLock`
   (`bflib_vidsurface.c:160`) directly, not `RendererManager`'s `LockFramebuffer`/
   `UnlockFramebuffer`. A backend swap needs to know about this path or cursor rendering breaks.

## Part A — Extend the seam over the rasterizer

Status: **done (2026-09-03)** for the pieces below; see the deferred item at the end.

The rasterizer doesn't need per-triangle virtual dispatch (that would tank software-path
performance for no benefit — stage 2 is still CPU-rasterized). What it needs is for its **output
target** to be an interface member instead of a raw global, so a backend controls where and in
what format those bytes land.

- **Done**: `SwDrawTarget.h`/`.c` gained six new getters mirroring the existing
  `SwTargetWScreen()`/`SwTargetScanline()` pattern — `SwTargetVecScreen()`,
  `SwTargetPolyScreen()`, `SwTargetVecMap()`, `SwTargetVecScreenWidth()`,
  `SwTargetVecWindowWidth()`, `SwTargetVecWindowHeight()` — wrapping `bflib_vidraw.c`'s
  `vec_screen`/`poly_screen`/`vec_map`/`vec_screen_width`/`vec_window_width`/`vec_window_height`
  (the state `setup_vecs()`, `bflib_vidraw.c:1614`, configures — these are the rasterizer's own
  configurable render target, distinct from and more dynamic than the base
  `lbDisplay.WScreen`/`GraphicsWindow*` `SwTargetWScreen()` already covered, since `setup_vecs()`
  can point them at a sub-region or different scale, e.g. `gui_parchment.c`'s downscaled
  overlay). Plain non-virtual functions, per the original plan — each call site captures the
  pointer/width **once per polygon or per texture-block draw**, not per pixel, so this added zero
  measurable call overhead to the hot loop.
- **Done**: `engine_textures.c`'s `scale_tmap2` — the one genuine cross-boundary consumer
  (`kfx_render` reaching into `kfx_platform`'s internal rasterizer-target state) — now captures
  `SwTargetVecScreen()`/`SwTargetVecScreenWidth()` once at function entry instead of reading
  `vec_screen`/`vec_screen_width` as raw externs (was `engine_textures.c:382,404,421,441,463,480`).
- **Done**: `bflib_render_gpoly.c`'s setup/per-scanline reads of `vec_map`, `vec_screen_width`,
  `vec_window_width`, `vec_screen`, `vec_window_height` (`draw_gpoly_line`, `next_line`,
  `draw_gpoly_clipped_half`, `draw_gpoly_clipped`, `draw_gpoly_whole` — was
  `bflib_render_gpoly.c:683,706,719,759,760,778,793,794`) now go through the same accessors.
  `bflib_render_trig.c`'s ~60 equivalent `vec_*`-reading sites across ~40 triangle-rasterizer
  functions were **deliberately left as direct externs** — unlike
  `bflib_render_gpoly.c`/`engine_textures.c`, every one of those sites is already fully internal
  to `kfx_platform`'s software backend (no cross-boundary reach), and stage 2 is going to rewrite
  this file's pixel-write expressions wholesale anyway (per
  [02-32bit-software-renderer.md](02-32bit-software-renderer.md)), so converting the read side
  now just to have stage 2 immediately touch the same lines again isn't worth the churn/review
  cost on a ~4600-line hot-path file. Do this conversion as part of stage 2's own pass over the
  file instead, when the write side changes too. **Confirmed by stage 2's own follow-up audit**:
  this is genuinely the same file stage 2's item 1 (retiring the shade/fade/ghost tables) has to
  rewrite anyway — 19 of the file's 25 rasterizer functions and 30 separate
  `fade_table[...]`/`ghost[...]`-shaped lookup sites, a distinct count from this `vec_*` figure
  (some functions touch both kinds of site, most don't overlap 1:1) — so the two conversions
  really do belong in one pass over the file, not two.
- **Done**: `engine_redraw.c`'s six direct `lbDisplay.WScreen` sites (`:384,395,413,429,558,613`
  — fade-blend snapshotting in `prepare_map_fade_buffers`/`map_fade_in`/`map_fade_out` and
  screen-smoothing in `redraw_creature_view`/`redraw_isometric_view`) now read `SwTargetWScreen()`
  instead — this file wasn't in the original scope but the expanded audit found it doing the same
  category of write the rasterizer does.
- **Done**: `RendererManager` gained `RendererSwapFramebufferTarget(target, width, height)` /
  `RendererRestoreFramebufferTarget(previous)` (`RendererManager.h`/`.cpp`), and
  `src/kfx_sim/src/thing_creature.c:4350-4378`'s `draw_creature_view` — which swaps
  `lbDisplay.WScreen` to point at the eye-lens effect's render-target buffer and back — now calls
  these instead of assigning the raw global directly. `kfx_sim` depending on `kfx_platform`
  directly is allowed by the layering ladder (no violation, before or after); the point was
  giving this one raw-pointer-swap pattern a named entry point instead of a field assignment.
  Deliberately preserves the pre-existing behavior exactly, including that
  `GraphicsScreenWidth`/`GraphicsScreenHeight` are **not** restored by the "restore" half (matches
  the original code, which only restored `WScreen` itself and relied on
  `LbScreenLoadGraphicsWindow`/the next frame's lock for the rest) — investigating whether that's
  a latent bug is out of scope for this pure refactor.
- **Bug found and fixed during manual verification**: the first version of this change declared
  `SwTargetVecScreenWidth()`/`SwTargetVecWindowWidth()`/`SwTargetVecWindowHeight()` as
  `uint32_t`/`int32_t`/`int32_t` (matching the *style* of the pre-existing `SwTarget*` functions),
  but the underlying globals are `unsigned long`/`long`/`long` (64-bit on this platform) — not a
  narrower type. That mismatch wasn't just a precision detail: C's usual arithmetic conversions
  mean the width of an operand decides the width the whole expression computes in, so
  `bflib_render_gpoly.c`'s `vec_screen_width * state.y` (a signed row index) silently went from
  64-bit to 32-bit arithmetic. This crashed with a segfault in `draw_gpoly_line` (a bad write
  address) the first time the eye-lens creature-possession render path ran — caught by the user
  running an actual hvlog build in-game, not by compile/test verification, since nothing here is
  a type error and the unit tests don't exercise the real rasterizer. Fixed by declaring the three
  accessors' return types to match the underlying globals exactly (`unsigned long`/`long`/`long`,
  see the comment on `SwTargetVecScreenWidth()` in `SwDrawTarget.h`) rather than "the type the
  neighboring functions happen to use." **Confirmed fixed by the user's manual in-game test**
  (2026-09-03) — the eye-lens/creature-possession path (the exact crash trigger) now works.
  Lesson for the rest of this stage's remaining parts: when an accessor wraps an existing global,
  match its type exactly by default — treat any deliberate narrowing as a decision to justify, not
  a stylistic default, since arithmetic-width changes are invisible at both compile time and in
  unit tests that don't exercise the real numeric ranges.
- Verified: `KFX_OS=linux ./build-cmake.sh` (both variants), the mingw cross-compile,
  `check_layering.py --strict`, the `kfx_platform`/`kfx_render`/`kfx_sim`/`kfx_frontend` Catch2
  suites (`KFX_BUILD_TESTS=ON`, 669+82+1864+84 assertions, zero failures) all pass, **and** a
  manual in-game test confirmed the crash this stage introduced (and then fixed) is gone. Part A
  is now verified end-to-end, not just compile/test-clean.

## Part B — Eliminate the remaining raw `WScreen[]` pokes in `kfx_frontend`

Status: **done (2026-09-03).**

**14 call sites across 7 files**, not the 7/5 originally scoped: `gui_draw.c:265,952`,
`gui_parchment.c:192,306,782`, `front_landview.c:112,808,887`,
`frontmenu_ingame_map.c:124,973,1268`, `frontend.cpp` (1 site), plus two files a literal
`lbDisplay.WScreen[` substring grep missed because `WScreen` was passed as a bare pointer
argument rather than indexed directly — check for this pattern specifically when re-auditing,
don't just repeat the same substring search: `front_torture.c:194`, `front_simple.c:173`.

For each: replace the direct `lbDisplay.WScreen[y * pitch + x] = colour` (or equivalent, or the
bare-pointer-argument form) with the matching `IUIRenderer` submission
(`SubmitSolidBox`/`SubmitRawSprite*`) if one already fits the shape of the write, or — if
genuinely bespoke (e.g. `gui_parchment.c`'s per-pixel parchment texture blending) — a **new**
narrow `IUIRenderer` method rather than leaving it as a raw pointer poke. `gui_draw.c:237`
(`RendererDrawSlabBackground`, already routed) is the existing pattern to match.

Audit each site's actual pixel operation first (some may be reads, e.g. colour-picking under the
cursor, not writes — those don't need a submission API, just a `RendererManager` accessor if one
doesn't exist yet) before assuming a 1:1 `IUIRenderer` mapping.

**As built**: none of the 14 sites turned out to be a clean 1:1 `IUIRenderer` submission —
every one is either a base-pointer argument to a bulk/bespoke copy routine
(`copy_raw8_image_buffer`, `setup_vecs`, `LbHugeSpriteDraw`) or a hand-rolled per-pixel/per-block
fill loop that's itself the "immediate" drawing implementation for a specific screen (the minimap,
the parchment overhead map, `draw_slab64k_background_immediate`) — the same category as the
software rasterizer's own internals, not generic GUI drawing. Introducing a new `IUIRenderer`
method for each would have meant restructuring working loops for no seam benefit, since the
per-pixel-format-dependent logic still has to live *somewhere* and stage 2 will rewrite it
regardless. Instead, added one new `RendererManager` accessor —
`unsigned char* RendererGetFramebuffer(void)` (`RendererManager.h`/`.cpp`), a pure read of
`lbDisplay.WScreen` with no lock side effects — as the public, backend-agnostic entry point for
`kfx_frontend` (deliberately *not* `SwDrawTarget.h`'s `renderer/software/` accessors, which are
kept scoped to `kfx_platform` and the tightly-coupled `kfx_render` rasterizer files from Part A;
`kfx_frontend` is a much higher layer and should only ever reach the public seam). All 14 sites
now call `RendererGetFramebuffer()` instead of reading `lbDisplay.WScreen` directly; where the
call sits inside a loop (`frontend.cpp`'s debug font-test screen), the accessor is captured once
before the loop, matching the same "once per draw, not once per pixel" discipline Part A
established. `front_torture.c` needed a new `#include "renderer/RendererManager.h"` (the other six
files already had it via existing seam usage); the other six needed no include changes.

Verified: `KFX_OS=linux ./build-cmake.sh` (both variants), the mingw cross-compile,
`check_layering.py --strict`, and the four Catch2 suites all pass clean (same counts as Part A —
none of this touched tested logic), plus a manual in-game pass confirming the affected screens
(main menu, in-game HUD/minimap, land selection, torture screen) render correctly.

## Part B2 — Bypasses outside `kfx_frontend`/`kfx_render` (new)

Status: **done (2026-09-03)** for the three fixed items below; `game_session_loop.cpp` remains
deliberately deferred, per the original scoping.

Found by widening the search past the two libraries the original investigation scoped. Each
needed its own judgment call, not a blanket "route through `IUIRenderer`" — they sit in different
layers with different constraints:

- **`src/kfx_apploop/src/game_session_loop.cpp:260-261`** — a screen-scroll `memcpy`/`memset`
  directly on `lbDisplay.WScreen`. `kfx_apploop` is top-ranked (allowed to depend on anything
  below), so this isn't a layering violation, but it's still a raw-format assumption a backend
  swap would break. **Left as-is, per the original scoping decision** (see Non-goals) — lowest
  priority of this group, revisit once stage 2's format is decided, since a bulk memcpy-based
  scroll may not even be the right technique once the pixel format changes (a GPU backend would
  want this as a texture-coordinate offset, not a CPU copy).
- **Done: `src/kfx_platform/src/bflib_fmvids.cpp:104,154`** — FMV/movie playback blit. Already
  included `renderer/RendererManager.h`; both sites now call `RendererGetFramebuffer()` (the same
  public accessor Part B introduced), not a dedicated `SwDrawTarget`/`IUIRenderer` method — this
  file is video playback, not rasterizer-internal, so it belongs with Part B's `kfx_frontend`-style
  fix, not Part A's `kfx_platform`-internal one.
- **Done: `src/kfx_platform/src/bflib_mspointer.cpp:328`** (`PointerDraw`, the mouse-cursor blit)
  — investigated the "unlocked" flag before fixing anything, per this section's own instruction.
  **Finding: the flag was about a different lock than the framebuffer one.** The call is already
  correctly bracketed by `RendererLockFramebuffer()`/`RendererUnlockFramebuffer()`
  (`bflib_mspointer.cpp:325-330`) — properly locked at the seam level, so the original "unlocked"
  read was either imprecise or referred to something else. What actually *is* asymmetric: four
  sibling methods (`Initialise`, `Release`, `OnMove`, `OnEndSwap`) all take
  `std::lock_guard<std::mutex> guard(lock);` at entry to protect `surf1`/`surf2`/`sprite`/
  `position` from concurrent access, but `OnBeginSwap()` — the one containing this `PointerDraw`
  call — does not. **This is a separate, pre-existing possible thread-safety gap, left
  untouched**: adding a mutex guard here would be a real behavior change (different locking, not a
  pure refactor), and touching synchronization on a render-path method without dedicated testing
  is exactly the kind of change this stage's "preserve behavior exactly" rule exists to prevent.
  Flagging it here for whoever next touches `bflib_mspointer.cpp` to investigate on its own
  merits. The pixel-format bypass itself is fixed: the call now passes `RendererGetFramebuffer()`
  instead of `lbDisplay.WScreen`.
- **Done: `src/kfx_platform/src/bflib_sprfnt.c:993`** (`LbTextSetWindow`) — aliases a raw
  `WScreen` pointer into `lbTextJustifyWindow.ptr`. Investigated where `.ptr` is actually read
  (per this section's own "confirm whether this is a latent bug" instruction) and found it's
  **never read anywhere in the codebase** — only `lbTextJustifyWindow.x`/`.y`/`.width` are used
  for text-justification math; `.ptr` is write-only, vestigial state left over from before text
  drawing went through the seam. Fixed the same way regardless (`RendererGetFramebuffer()`) since
  the assignment is still a bypass even if its result is currently unused, and stage 2 shouldn't
  have to rediscover this file when it changes what `lbDisplay.WScreen` means. This was the
  pointer-alias pattern Part B's fix approach needed to generalize to: when auditing, grep for
  `= lbDisplay.WScreen` and `= lbDisplay.GraphicsWindowPtr` (assignment, not indexing) in addition
  to the indexing pattern, since an alias assigned once and indexed many times elsewhere is
  invisible to a search anchored on the indexing site. Note: the same file's
  `lbTextJustifyWindow_window_ptr`/`lbTextClipWindow_window_ptr` (`:1245,1293`) are dead code
  explicitly marked "DON'T USE...in KeeperFX" in-source — confirmed genuinely unused, not a
  second live bypass; don't spend time routing dead code.

**Found during implementation, not the original audit** — a final whole-codebase
`grep -rn "lbDisplay\.WScreen\b" src/` sweep after fixing the items above turned up two more
sites the Tier-3 audit missed entirely:

- **Done: `src/kfx_render/src/scrcapt.c:132`** (`anim_record_frame(lbDisplay.WScreen, ...)`,
  screen-capture/movie-recording) — same bare-pointer-argument shape as everything else in this
  part, fixed the same way (`RendererGetFramebuffer()`). Already included
  `renderer/RendererManager.h`.
- **Done: `src/kfx_sim/src/thing_creature.c:4378`** — the *read* half of `draw_creature_view`
  (Part A fixed the *write* half, the lens-buffer swap). `sim_feedback->draw_lens_effect(lbDisplay.WScreen
  + dst_offset, ...)` reads the framebuffer *after* `RendererRestoreFramebufferTarget()` has
  already put it back — not itself flagged as a distinct item by the original audit since it's
  part of the same function Part A already touched, but fixed here for consistency with every
  other "read `WScreen` as a draw-call argument" site in this stage.

This is worth noting plainly: **the audit, even at Tier 3, was not exhaustive.** Both misses were
caught only because implementation included its own verification sweep after each part, not
because the planning was perfect. Apply the same discipline to Parts C and D below — re-grep after
implementing, don't trust the plan's file list as final.

## Part C — Confirm no other backend-shaped globals leaked through

Status: **done (2026-09-03).**

Re-checked `docs/refactor/stage-07-kfx-render.md`'s "Render globals that need an explicit
accessor API" table against current code, plus the `vidmode_data.cpp` globals this stage's own
investigation had left untraced. Findings split into four buckets — most of the table turned out
not to need Part A/B/B2-style fixing at all, for reasons worth recording so a future pass doesn't
re-litigate the same ground:

**Already resolved by earlier work** (no action): `zoom_distance_setting`/
`frontview_zoom_distance_setting` — moved to `kfx_config_state.h` (stage 13.3,
`engine_camera.h:41-46`'s own comment confirms it, `kfx_net`'s `net_game.c`/`packets.c` now read
`kfx_config_state.zoom_distance_setting`, not a `kfx_render` global). `colours` — moved to
`kfx_sim_state.h` (stage 13.3, `vidfade.h:46`); every remaining reference in `gui_boxmenu.c`/
`gui_parchment.c` is a **read** of `kfx_sim_state.colours[...]` for picking a draw colour, not a
write to a `kfx_render` global.

**Fixed now** — the one genuine case of `kfx_frontend` duplicating `kfx_render`'s own
encapsulated logic instead of calling it: `gui_parchment.c:641-646`'s `draw_2d_map()` hand-rolled
the exact same lazy-init `render_fade_tables = pixmap.fade_tables; render_ghost = pixmap.ghost;
render_alpha = ...` sequence that `kfx_render`'s own `vidmode.c::sync_render_globals()`
(`vidmode.h:207`, already declared and includable — `gui_parchment.c` already `#include`s
`vidmode.h`) exists specifically to do, and that `engine_render.c` already calls in the normal 3D
path. Replaced the duplicate with a call to `sync_render_globals()`. This was a real instance of
the pattern Part C exists to catch: not a raw-pointer bypass like Parts A/B/B2, but a
higher layer re-deriving a lower layer's internal state instead of asking for it.

**Investigated, found to be deliberate scratch-buffer reuse, not a bypass** — `poly_pool`
(`gui_parchment.c`), `block_mem` (`front_torture.c`, `front_landview.c`), `scratch`
(`vidmode_data.cpp` — literally registered as `"*SCRATCH"` in its own load-file table), and
`hires_parchment` (`gui_parchment.c`) are all large pre-allocated buffers that `kfx_frontend`
screens reuse via direct `LbFileLoadAt`/`memcpy` when the buffer's primary owner (the 3D texture
system, the polygon renderer) isn't active — `front_torture.c:140`'s own comment states the
design explicitly: *"Texture blocks memory isn't used here, so reuse it instead of allocating."*
`scratch` in particular is reused for three unrelated purposes across different frontend files
(palette storage, multiplayer vote counting, level-stats struct copying) — that's not an
accident, it's what a buffer named `*SCRATCH` is *for*. There's no accessor API to add here
without fighting the actual design; forcing one would replace a working memory-reuse pattern with
false structure. `gui_slab` is a plain read of a loaded asset (the slab-background tile sprite),
not a write, so it was never actually a bypass. `red_palette`/`dog_palette`/`vampire_palette`
turned out not to be referenced outside `kfx_render` at all — a false lead from the initial broad
grep matching other identifiers in the same files.

**Confirmed intentional and already documented — explicitly stage 2's job, not stage 1's**:
`fade_palette_in`/`frontend_palette`/`frontend_backup_palette` are the single-active-palette
mechanism [`docs/refactor/gui/colordepth/00-notes.md`](../gui/colordepth/00-notes.md) already
documents at length as a known, load-bearing constraint that
[02-32bit-software-renderer.md](02-32bit-software-renderer.md) removes by construction (per-sprite
palette expansion). Adding an accessor API around it now would be building scaffolding for a
pattern about to be deleted — noted here only to confirm it wasn't missed, not to act on it.

**Still open per the original stage-07 finding, re-confirmed against current code, but judged
out of scope for this stage** — none of these are pixel/colour/palette data, so they don't block
stage 2 the way this stage's actual goal cares about; fixing them is a legitimate but separate
general-encapsulation cleanup:
- `box_lag_compensation_x`/`_y` (`packets.c` → `engine_render.h`) — network lag-compensation
  camera offsets.
- `map_volume_box` (`packets_input.c` → `engine_render.h`) — a debug-overlay visibility flag.
- `keepsprite[]`/`sprite_heap_handle[]`/`jty_file_handle` (`game_heap.c` → `engine_render.h`) —
  sprite-heap slot/file-handle bookkeeping.
- `required_sprite_zip_checksums[]` (`net_game.c` → `custom_sprites.h`) — multiplayer zip-sync
  checksums.

Verified: `KFX_OS=linux ./build-cmake.sh` (both variants), the mingw cross-compile,
`check_layering.py --strict`, and the `kfx_render`/`kfx_frontend` Catch2 suites all pass clean.

## Part D — The mouse cursor's independent surface-lock path

Status: **done (2026-09-03) — investigated and documented; option (b), no code change.**

`bflib_mspointer.cpp:202` locks the cursor backup/restore surfaces via `LbScreenSurfaceLock`
(`bflib_vidsurface.c:154-168`, a generic `SSurface` lock), entirely separate from
`RendererSoftware`'s own `LockFramebuffer`/`UnlockFramebuffer`/`SDL_LockSurface` path
(`RendererSoftware.cpp:93,127`, correctly excluded from this concern — that's the seam's own
path). Investigated whether this is a real backend-compatibility risk before picking between the
doc's two options (extend `RendererManager` with a second lock entry point, vs. confirm
independence and document). **Finding: option (b) — the risk is real but different in shape than
originally framed, and already mostly self-mitigated by existing code.**

### How the cursor surface system actually relates to the seam

`struct SSurface` (`bflib_vidsurface.h:32-36`) wraps its own independent `SDL_Surface*`
(`surf_data`), created fresh by `LbScreenSurfaceCreate` (`bflib_vidsurface.c:47-68`) —
**not** a view into the main framebuffer. Two things tie it to the seam's state, both already
handled correctly:

1. **Format**: `LbScreenSurfaceCreate` reads `lbDrawSurface->format` (`:51-56`) — the *live*
   format of the seam's own backing surface — at creation time, not a hardcoded constant. Since
   `LbI_PointerHandler::Initialise()` (`bflib_mspointer.cpp:195-225`, via the RAII
   `ScopedScreenSurface::Create()`) re-creates `surf1`/`surf2` every time the cursor sprite
   changes (frequent — different cursor icons per game mode), a stage-2 format change to
   `lbDrawSurface` propagates to the cursor surfaces automatically, with **zero code changes
   needed here**.
2. **Compositing**: `LbScreenSurfaceBlit` (`bflib_vidsurface.c:82-152`) calls `SDL_BlitSurface`
   directly against `lbDrawSurface` (`:134,138`) — this is a **complete parallel rendering path**
   for the cursor's draw/backup/undraw operations, not just a second *lock*. It never goes through
   `IRenderer`/`IUIRenderer`/`RendererManager` at all. This is broader than the doc's original
   framing ("a second lock path") — it's a second, self-contained *composite* path, hardcoded to
   `SDL_Surface`/`SDL_BlitSurface` semantics.

### Why this is judged safe through stage 2, and a stage-3 concern rather than stage-1's

`SDL_BlitSurface` handles format conversion between differently-formatted `SDL_Surface`s
automatically (that's exactly the mechanism `RendererSoftware::PresentFrame` already relies on for
its own INDEX8→RGBA present-time conversion, per [00-overview.md](00-overview.md)). Since the
cursor surfaces' format tracks `lbDrawSurface`'s live format (point 1 above), and the blit targets
`lbDrawSurface` directly regardless of what format it currently holds, **this entire subsystem is
expected to keep working unchanged through stage 2's 8-bit→32-bit software change** — no accessor
API would add real value here, and Part C already established the principle of not forcing
structure onto something that doesn't need it.

Where this *does* become a real concern: **stage 3's GPU backend**, if it stops using
`lbDrawSurface`/`SDL_Surface` as its primary render target at all. At that point *every* consumer
of `lbDrawSurface` breaks, not just the cursor — `bflib_video.c:594` itself hardcodes
`lbDrawSurface = SDL_CreateSurface(mdinfo->Width, mdinfo->Height, SDL_PIXELFORMAT_INDEX8);`
(the canonical creation site, worth stage 2 knowing about directly — see
[02-32bit-software-renderer.md](02-32bit-software-renderer.md)'s cross-reference). Confirmed via
`grep -rln lbDrawSurface src/` that it's referenced *only* within `kfx_platform`
(`bflib_video.c`, `bflib_vidsurface.{c,h}`, `RendererSoftware.cpp`) — `bflib_mspointer.cpp` never
reaches the global directly, only through the proper `LbScreenSurface*` API, so there's no
additional raw-global bypass here beyond what's already documented above.

**Decision, recorded rather than implemented**: leave `LbScreenSurfaceBlit`'s direct
`SDL_BlitSurface`-against-`lbDrawSurface` design as-is for stage 1 and stage 2. Flag it explicitly
for [03-gpu-renderer.md](03-gpu-renderer.md) — Phase B's design needs to account for cursor
rendering as a distinct case (it's currently *not* routed through `IUIRenderer` at all, unlike
every other 2D draw this stage brought under the seam), since a GPU backend that drops
`SDL_Surface` as its primary target has to either keep a CPU-side `SDL_Surface` mirror
specifically for this compositing, or give the cursor its own `IUIRenderer`-routed path for the
first time.

## Non-goals for this stage

- No pixel-format change (stage 2).
- No new `IRenderer` backend (stages 2–3).
- No change to `kfx_frontend`'s call-site API (`Lb*Draw*` signatures) — only what's *behind*
  `lbDisplay.WScreen[]` pokes and `vec_screen` moves, not how frontend code asks for a draw.
- `kfx_apploop`'s screen-scroll `memcpy` (Part B2) is flagged, not fixed, in this stage — its
  right shape depends on stage 2's format decision.

## Verification

- `KFX_OS=linux ./build-cmake.sh` after each part (A, B, B2, D).
- `python3 scripts/check_layering.py --strict` (no new cross-library includes expected — Part
  A's `thing_creature.c` accessor and Part B2's cross-layer fixes stay within each file's existing
  allowed dependencies).
- Screenshot-diff pass: main menu, a lit dungeon room (exercises the fade-table path Part A
  re-routes and `engine_redraw.c`'s smoothing path), the Land selection screen (exercises
  `gui_parchment.c`), a torture-room/lens-effect screen (`front_torture.c`, now in scope),
  in-game map view (`frontmenu_ingame_map.c`), an FMV/cutscene (`bflib_fmvids.cpp`), and — since
  Part B2 flags it as a possible latent bug, not just a seam gap — explicit cursor-rendering
  verification (move the cursor over every screen type, confirm no corruption/flicker from the
  unlocked `PointerDraw` path before and after any change there). Byte-identical or visually
  indistinguishable output is the bar for everything except the cursor-lock investigation, which
  may legitimately change behavior if a real bug is found.
- Run `src/kfx_render/tests/` and `src/kfx_frontend/tests/` Catch2 suites; add coverage for any
  new `IUIRenderer` method introduced in Part B, and for the new framebuffer-swap accessor from
  Part A if `thing_creature.c`'s lens-view swap logic is unit-testable in isolation.
- Given the graph-coverage gap noted in [00-overview.md](00-overview.md), re-run
  `check_index_coverage` on `engine_render.c`, `engine_redraw.c`, `engine_textures.c`,
  `front_landview.c`, `front_torture.c`, `front_simple.c`, and `engine_lenses.c` before relying on
  any *new* graph-based claim about them during implementation — treat a stale/parse_partial
  result as a signal to fall back to direct grep/read for that file, the same way this
  investigation had to.
