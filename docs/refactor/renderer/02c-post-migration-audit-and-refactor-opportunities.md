# Stage 2c — post-migration audit and refactor opportunities

Status: **audit complete; every §2 item implemented except §2.1.4, deliberately deferred (see its
entry).** Written after the Stage 2 TbPixel migration was verified against the actual game (GOG
install, real data files) and every bug reported from that in-game testing pass was fixed — see the
"Bugs found via live testing" section of [02b-legacy-bugs-found.md](02b-legacy-bugs-found.md) for
the incident log (dotted sprites, black selection-box wireframe, minimap corruption, the intro-movie
crash and the mouse-cursor heap corruption that followed it). This document has two purposes:

1. **§1** — a systematic sweep for other instances of the *same* bug classes that testing happened
   to surface, so we're not just fixing what got reported and hoping the rest is clean.
2. **§2** — refactor opportunities noticed while doing that sweep and while fixing the reported
   bugs: code duplication, performance, and robustness (including raw-pointer lifetime management
   that C++ has safer tools for). Implemented in full on request, one commit per numbered item
   (git log for this stage); each entry below is marked with what landed and where.

---

## 1. Audit: searching for the same bug classes elsewhere

Every bug found via live testing this stage traced back to one of three root causes. Each was
searched for exhaustively across `src/kfx_platform/`, `src/kfx_render/`, and `src/kfx_frontend/`'s
rendering-adjacent files. Result: **no further live instances found** beyond what's already fixed;
one stale comment updated for accuracy (§1.4).

### 1.1 Byte pitch fed into pixel-stride pointer arithmetic

**The pattern**: `TbPixel` used to be one byte, so an SDL `->pitch` field (always byte-denominated)
and a "pixels per scanline" stride were numerically identical. Once `TbPixel` widened to 4 bytes,
any code that took a byte pitch and used it directly as a `TbPixel*` stride silently under-advances
by 4x, corrupting whatever memory follows the intended write region. Three confirmed instances,
all fixed this stage:

- `RendererLockFramebuffer()` (`RendererManager.cpp`) and `bflib_video.c`'s mode-setup both set
  `lbDisplay.GraphicsScreenWidth` straight from an SDL surface's byte pitch.
- `bflib_mspointer.cpp`'s cursor backup surface did the same via `SSurface::pitch()`, corrupting
  the heap on every cursor sprite change — the abort at `set_pointer_graphic: Setting to 0`.
- `LensManager::CopyBuffer()` had the identical shape (found and fixed earlier, before the
  `lbDrawSurface` flip made it observable) — see [02a-pixel-format-design.md](02a-pixel-format-design.md).

**Swept for elsewhere**: every `->pitch`/`.pitch` reference in `src/kfx_platform/src/` and
`src/kfx_render/src/`. The only remaining ones are `LuaLensEffect.cpp`'s `LuaBufferInfo::pitch`
(sourced from `LensRenderContext::srcpitch`/`dstpitch`, which are already pixel counts derived from
`lbDisplay.GraphicsScreenWidth` post-fix) and `bflib_sndlib.cpp`'s audio "pitch" (unrelated — sound
playback speed, not a stride). Confirmed clean.

**Refactor note**: this bug class is exactly the kind a type system should catch and didn't — see
§2.3.1's strong-typedef proposal.

### 1.2 `TbPixel[]` arrays initialized with bare scalar literals

**The pattern**: `struct { TbPixel arr[16]; ... }` initialized as `{ 0x02, 0x03, 0x04, ... }` (16
bare integers) instead of 16 `{r,g,b,a}` tuples. This compiled and ran under the old 1-byte
`TbPixel` typedef; as a 4-byte struct, C++ brace elision consumes 4 scalars per `TbPixel`, so a
16-scalar initializer only fills the first ~4 array slots and zero-initializes (transparent black)
the rest. Found and fixed in `engine_render_data.cpp`'s `colored_stripey_lines[]` tables (the
black-instead-of-green dig-cube wireframe) and, earlier in the stage, `front_landview_multiplayer.c`'s
`net_player_colours[]` (caught at compile time there — `-Werror=missing-braces` fired for that flat
top-level array; it did *not* fire for the nested-inside-a-struct-initializer form in
`engine_render_data.cpp`, which is why that one shipped and had to be caught by inspection instead
of the compiler).

**Swept for elsewhere**: every `TbPixel <name>[<N>]` declaration in every `include/*.h` and
`src/*.c`/`.cpp` across all ten `src/kfx_*` libraries (`grep -rn "TbPixel\s\+\w\+\[[0-9]"`). Only two
declarations exist: `lbSpriteRemapTable[256]` (populated entirely at runtime by
`SetupSpriteRemapGhost()`/`SetupSpriteRemapShade()`/etc., never statically initialized, not at
risk) and `stripey_line_color_array[16]` (fixed). Confirmed clean.

**Refactor note**: `-Wmissing-braces` not firing consistently for nested-aggregate cases is a real
gap — see §2.3.2.

### 1.3 Leftover 1-byte-per-pixel batching tricks

**The pattern**: hand-rolled loops that byte-align a pointer and then bulk-process 4 bytes at a
time via a `uint32_t` reinterpret — a real optimization when 4 bytes meant 4 pixels, meaningless
(and silently wrong) once a pixel *is* 4 bytes: the bulk step copies one pixel's worth of data but
advances both pointers by 4 pixels' worth of address space, dropping 3 out of every 4 pixels. Found
and fixed in `bflib_vidraw.c`'s `LbPixelBlockCopyForward()` — the root cause of "dotted" sprites,
used by every scaled sprite blit across `bflib_vidraw_spr_norm.c`/`_onec.c`/`_remp.c`.

**Swept for elsewhere**: every `(uint32_t *)`/`(unsigned long *)`/`& 3` alignment-style cast across
the software rasterizer (`src/kfx_platform/src/renderer/software/*.c`), `bflib_mspointer.cpp`, and
`engine_render.c`. Two more `(uint32_t *)` reinterprets exist
(`bflib_vidraw.c:1690,1725`) but both read a compressed-sprite-format run-length header from
`sprdata` (`const unsigned char*`, the sprite's own on-disk byte format — 4 bytes there is a
genuine `uint32_t` header field, not 4 pixels) — not a bug. `engine_render.c`'s
`draw_keepsprite_unscaled_in_buffer()` has its own byte-alignment fast-fill (`*(uint32_t*)out =
0xFFFFFFFF`), but its `outbuf`/`tmpbuf`/`out` are genuinely `unsigned char*` throughout — a separate
still-8bpp intermediate "keeper sprite" decode cache (backed by `big_scratch`, the same
multi-purpose scratch buffer `poly_pool` pattern used elsewhere), consumed later through the
already-migrated `vec_map`/`expand_indexed_pixel` path, not aliased with any `TbPixel` buffer.
Confirmed clean.

### 1.4 SDL surface-format assumptions (colour keys, palettes, `SDL_MapRGB`)

**The pattern**: code that assumes the draw surface is 8-bit indexed with its own SDL palette —
true before the `lbDrawSurface` flip to `SDL_PIXELFORMAT_RGBA32`, not after. Found and fixed:
`LbScreenSurfaceBlit()`'s colour key (`bflib_vidsurface.c`) was the literal `255`, meaningful only
as an 8bpp palette index; now mapped through `SDL_MapSurfaceRGBA()`.

**Swept for elsewhere**: every `SDL_CreateSurface`/`SDL_MapRGB`/`SDL_MapSurfaceRGB*`/
`SDL_SetSurfaceColorKey`/`SDL_SetPaletteColors`/`SDL_CreateSurfacePalette`/`PIXELFORMAT_INDEX8`
reference in `src/kfx_platform/src/`. The only surface creation left (`bflib_vidsurface.c`'s
`LbScreenSurfaceCreate()`) reads `lbDrawSurface->format` live rather than hardcoding one, so it
already tracks the flip automatically. `bflib_video.c`'s own creation call is the flip itself.
Confirmed clean; one stale comment fixed (`bflib_fmvids.cpp`'s `anim_record_frame()`, which
described the flip as future work — it isn't anymore, and `anim_record()`'s own `!= 8` guard now
makes that whole legacy-FLI-recorder code path permanently unreachable, exactly as designed).

---

## 2. Refactor opportunities

Not bugs — things noticed while working in this code that would make it harder for the *next*
version of this exact mistake to happen, or that are straightforwardly better engineering. Ordered
roughly by how much they would have helped catch this stage's actual incidents.

### 2.1 Code duplication

**2.1.1 — `LbPixelBlockCopyForward`'s prototype is copy-pasted into three files.**
`src/kfx_platform/src/renderer/software/bflib_vidraw_spr_norm.c:42`,
`bflib_vidraw_spr_onec.c:42`, and `bflib_vidraw_spr_remp.c:42` each carry an identical `extern`
declaration (`void LbPixelBlockCopyForward(TbPixel * dst, const TbPixel * src, long len);`) instead
of a single shared one in `bflib_vidraw.h`. Harmless today (the definition in `bflib_vidraw.c`
matches all three), but three independent copies of a signature is exactly the kind of thing that
drifts silently if one is ever "fixed" without the others being noticed. Move the declaration to
`bflib_vidraw.h` once; delete the three local copies.
*Done.*

**2.1.2 — `bflib_fmvids.cpp`'s `copy_to_screen_pxquad`/`pxdblh`/`pxdblw`/`row` are four near-identical
functions.** All four do "expand one source index byte through the movie's palette, write it N
times with a given row/column doubling pattern." They differ only in whether width and/or height
doubling is applied. A single function parameterized on `(bool double_w, bool double_h)` (or a
small template/inline dispatch) would remove ~40 lines of duplication; the compiler will still
specialize/inline each combination if performance matters (this runs once per video frame, not per
pixel-column call, so the win is maintainability more than speed).
*Done — unified into `copy_to_screen_row_ex(..., bool double_w, bool double_h)`.*

**2.1.3 — `setup_panel_colors()` and `update_panel_colors()` in `frontmenu_ingame_map.c` duplicate
most of their body.** Both iterate `NumBackColours` background classes and fill the same
`PanelColours[]` slots (`PnC_Unexplored`/`PnC_Tagged_Gold`/`PnC_Tagged_Gems`/room and path colours)
with near-identical logic — `update_panel_colors()` is essentially `setup_panel_colors()` minus the
one-time room/path/door population, plus the highlight-flash handling. A shared
`fill_common_background_colours(bkcol, frame, pal, n)` helper called from both would cut the
duplication and make the (already subtle, now-fixed-once) overflow-cap logic in §1's minimap bug
live in exactly one place instead of two near-copies that could drift out of sync.
*Done — extracted as `resolve_common_background_colours()`; each caller's differing third (gems)
colour stays local, since its destination slot and literal offset genuinely differ between the two.*

**2.1.4 — the `UsingScalingUpData`/`UsingScalingDownData` × `SolidLR`/`SolidRL` × normal/one-colour/remap
sprite-blit families.** `bflib_vidraw_spr_norm.c`, `_onec.c`, and `_remp.c` each implement four
direction/scale variants of essentially the same rasterization loop (walk scanlines, walk
run-length-encoded source pixels, write to `outbuf` via `LbPixelBlockCopyForward` for solid runs).
This is pre-existing structure, not something this migration introduced, but it's now the file set
most likely to need touching again for any future pixel-format or blend-formula work, and 12
near-duplicate rasterization loops is a lot of surface area for that. Worth a dedicated
investigation (separate from this stage) into whether the direction/remap axes can be expressed as
a strategy parameter over one shared loop rather than full duplication per combination.
*Deliberately not done here* — this is the one item left as a proposal rather than implemented
alongside the rest: it's pre-existing structure spanning 12 rasterization loops across three files,
not something introduced this stage, and genuinely warrants its own dedicated investigation rather
than a drive-by change bundled with the other (bounded, low-risk) items in this document.

### 2.2 Performance

**2.2.1 — `LbPaletteFindColour()` is an O(256) linear scan (with a second full pass for
tie-breaking), called per pixel in two hot true-colour-to-palette-index reverse lookups.**
`gui_parchment.c`'s `draw_overhead_map()` calls it (indirectly, via the ghost-blend ->
`LbPaletteFindColour` round trip added this stage) for every pixel of every "tagged/gold/gems/
wall/abyss" styled slab, every frame the parchment map is open — this is a genuine every-frame cost
now, not a one-time setup cost. `frontmenu_ingame_map.c`'s `setup_background()` also calls it per
captured pixel, but only when the minimap's pixel size changes (zoom/resize), so it's much less
hot. The codebase already has the right tool for this: `compute_rgb2idx_table()`
(`vidfade.c`)/`TbRGBColorTable` builds a quantized 16×16×16 reverse-RGB-to-palette-index lookup
(`kfx_sim_state.colours`) precisely so this kind of reverse lookup doesn't need a linear scan.
Reusing (or building an equivalent full-resolution variant of) that table for these two call sites
would turn an O(256) scan into an O(1) lookup.
*Done — new `TbRGBColorTable_Lookup()` (vidfade.h) wraps `kfx_sim_state.colours`; applied at both
call sites.*

**2.2.2 — `RendererGetActivePalette()` is re-fetched inside per-pixel loops in a few places** (e.g.
`resolve_indexed_pixel(pixmap.ghost[...], RendererGetActivePalette())` called fresh for each of the
9-or-so `PanelColours[]` writes per background class in `setup_panel_colors()`). It's a cheap getter
(`LbPaletteGetReadonly()`, no computation), so this is a minor style nit rather than a real
bottleneck — hoisting it to a single local `pal` at the top of each function (already done in most
of the sites fixed this stage) is free and consistent with the rest of the codebase's convention.
*Not a separate change* — already the pattern at every site touched this stage; nothing further to do.

### 2.3 Robustness, including safer C++ ownership

**2.3.1 — Introduce a strong type distinguishing byte-pitch from pixel-pitch.** Every bug in §1.1
was the same mistake: a `long`/`int` holding a byte count got used somewhere expecting a pixel
count, and back, with nothing in the type system to stop it. A minimal wrapper —
```cpp
struct PixelStride { long value; };   // count of TbPixel elements
struct ByteStride   { long value; };  // count of bytes (SDL's own convention)
```
(or even just a `long` alias pair via a lightweight tagged type, not full `std::chrono`-style
arithmetic) at the handful of boundary points (`RendererLockFramebuffer()`,
`LbScreenSurfaceLock()`/`SSurface::pitch()`, `LensRenderContext::srcpitch`/`dstpitch`) would make
"used a byte count where a pixel count was expected" a compile error instead of a silent 4x
under-advance. This is the single highest-leverage change on this list relative to what it would
have caught this stage.
*Done — `TbBytePitch` (bflib_video.h), applied at `IRenderer::LockFramebuffer()`/
`RendererSoftware::LockFramebuffer()` and `struct SSurface::pitch`. `LensRenderContext::srcpitch`/
`dstpitch` were already pixel counts (fixed earlier this stage), so left as plain `long` — no byte
pitch ever flows through them, nothing to distinguish.*

**2.3.2 — `-Wmissing-braces` doesn't reliably catch nested-aggregate scalar-list initializers.**
§1.2's `colored_stripey_lines[]` bug shipped past the same compiler flag that caught the flatter
`net_player_colours[]` case. Worth a small investigation into whether `-Wmissing-braces` can be
coaxed to fire for `struct { TbPixel arr[16]; ... } x = { {16 bare scalars}, tag };` (possibly a
GCC/Clang difference, or a threshold on nesting depth) — and if not reliably, whether a
`static_assert`-based helper or a small custom Catch2 test asserting each `stripey_line`'s array
isn't all-`{0,0,0,0}` past index 0 is a cheap tripwire against a regression here.
*Done (the test, not the compiler investigation)* — `engine_render_data_test.cpp` asserts every
slot of every `colored_stripey_lines[]` entry is fully opaque (`alpha == 255`); a regression back to
bare-scalar initialization would leave the tail slots at `alpha == 0` and fail immediately. The
`-Wmissing-braces` behavior itself wasn't investigated further — the test is the cheaper, reliable
guard the entry already named as the fallback.

**2.3.3 — `LensManager`'s `m_lens_memory`/`m_spare_screen_memory` are raw `calloc`/`free` pairs with
manual bookkeeping.** `AllocateBuffers()`/`FreeBuffers()` (`LensManager.cpp`) exist solely to pair
a `calloc` with a `free` and null out the pointer + three related globals on teardown — the kind of
lifetime management `std::vector<uint32_t>`/`std::vector<TbPixel>` (or `std::unique_ptr<TbPixel[]>`
if a raw pointer must still be exposed to the C API layer via `.data()`) handles automatically,
including on the early-return failure paths `AllocateBuffers()` currently has to unwind by hand.
*Done — both are `std::vector` now; `eye_lens_memory`/`eye_lens_spare_screen_memory` point at
`.data()`. `vector::assign()` throws on allocation failure rather than returning null, so
`AllocateBuffers()` wraps it in try/catch to keep its original log-and-fail-gracefully contract.*

**2.3.4 — `DisplacementEffect`/`FlyeyeEffect`'s lookup tables are raw `malloc`/`free`.**
`m_lookup_table` in both classes (`DisplacementEffect.cpp`, `FlyeyeEffect.cpp`) is manually
allocated in `BuildLookupTable()` and freed in `FreeLookupTable()`/the destructor — a
`std::vector<DisplaceLookupEntry>`/`std::vector<FlyeyeLookupEntry>` would remove the manual
free-then-null-then-realloc dance entirely and make the "table doesn't match current resolution,
rebuild it" check (`m_lookup_table == nullptr || m_table_width != ctx->width || ...`) a plain
`.size()` comparison instead of three separately-tracked fields.
*Done, with one adjustment* — both are `std::vector` now (`.empty()` replaces the null check), but
`m_table_width`/`m_table_height` stay as separate fields rather than collapsing to `.size()`: they
answer "does the table match the *current* resolution", which a raw element count can't distinguish
from a different same-area resolution, so they're still genuinely needed alongside the vector, not
redundant with it.

**2.3.5 — `OverlayEffect`'s `COverlayRenderer::m_overlay_data` tracks ownership with a bool flag.**
`m_owns_data` exists purely to distinguish "borrowed pointer into the asset registry, don't free
it" from "we malloc'd this ourselves for the file-fallback path, must free it" — exactly the
distinction `std::unique_ptr` (for the owned case) versus a raw observer pointer (for the borrowed
case), or a `std::variant`/tagged union of the two, exists to make foolproof. As written, forgetting
to check `m_owns_data` before a `free()` anywhere new gets added is a use-after-free or double-free
waiting to happen.
*Done — `m_owns_data` is gone, replaced by `m_owned_data` (`std::unique_ptr<unsigned char[]>`, null
when borrowing) plus `m_overlay_data` (the `const unsigned char*` `Render()` actually reads
through). Allocation uses `new(std::nothrow)` rather than `malloc()`+exception-handling, so failure
still reports via a null pointer with no `try`/`catch` needed.*

**2.3.6 — `frontmenu_ingame_map.c`'s `MapBackground`/`MapShapeStart`/`MapShapeEnd` are raw
`calloc`/`free` C globals**, resized by freeing and reallocating whenever `MapDiagonalLength`
changes. Since this file is C, not C++, the direct fix is `std::vector` if/when this file is ever
converted to C++ (matching the pattern already used for the newer C++ renderer classes); until
then, at minimum the resize logic could be pulled into one small `resize_if_needed(ptr, old_len,
new_len, elem_size)` helper shared by the three arrays, rather than three independent
free-check-calloc blocks.
*Done (the C-appropriate fallback, not the `std::vector` conversion)* — this file stays C, so
`std::vector` isn't applicable yet; added `resize_scratch_array(old_ptr, count, elem_size)` and
routed all three arrays through it instead of three independent free-then-calloc blocks.

**2.3.7 — `PanelColours[]`'s overflow cap (§1's minimap fix) degrades silently.** When
`num_colours` hits `PANEL_MAP_BACKGROUND_COLOURS_MAX`, the fix folds any further distinct colour
into the last bucket with no diagnostic — correct for avoiding memory corruption, but if a future
UI background genuinely needs more than 16 background colour classes, this will silently produce a
slightly-off minimap with no log line pointing at why. Worth a one-time `WARNLOG` the first time the
cap is hit per `setup_background()` call, so a future "why does the minimap look slightly wrong"
report has a paper trail.
*Done — one `WARNLOG` per `setup_background()` call, the first time the cap is hit.*

---

## Cross-references

- [02a-pixel-format-design.md](02a-pixel-format-design.md) — the design spec these bugs were found
  relative to; its "Scope correction" addenda document the `pixmap.ghost`/`fade_tables` retirement
  work still outstanding (unrelated to this document's findings).
- [02b-legacy-bugs-found.md](02b-legacy-bugs-found.md) — the log of *pre-existing* bugs the
  migration's line-by-line rewrite surfaced; this document instead covers bugs the migration itself
  introduced (caught by live testing) and forward-looking refactor proposals.
