# Stage 2 — 32-bit true-colour software renderer

Status: **complete.** `TbPixel` is now `struct { uint8_t r,g,b,a; }` (`bflib_video.h`),
`lbDrawSurface` is `SDL_PIXELFORMAT_RGBA32`, the fade/ghost/alpha tables are real per-pixel blend
math, and sprite data is expanded to RGBA per-draw. Depends on
[01-close-the-seam.md](01-close-the-seam.md). See
[00-overview.md](00-overview.md) for context, including why this is the direct fix for
[`docs/refactor/gui/colordepth/00-notes.md`](../gui/colordepth/00-notes.md)'s one-palette-per-frame
finding.

**Implementation-ready design spec**: [02a-pixel-format-design.md](02a-pixel-format-design.md) —
the exact `TbPixel` type, blend-math formulas for every table this stage retires (derived from
the functions that generate those tables from an RGB palette, not reverse-engineered from bit
layouts), the sprite blit-time expansion mechanics, and a per-file disposition for all 34 files in
the `TbPixel` audit below. Written because stage 2 is a flag-day change with no safe intermediate
checkpoint and no way to visually verify lighting/colour correctness during design — read it
before writing any stage-2 code.

> **Revision note (2026-09-03):** a Tier-3 codebase-graph audit of this plan (done before any
> stage-2 implementation started, following the same practice that caught real gaps in stage 1)
> found the original pass had **misattributed where the bulk of item 1's work actually lives**
> (`engine_render.c` only wires up table pointers; the per-pixel lookups this item exists to
> rewrite are concentrated in `bflib_render_trig.c`, never previously mentioned in this doc),
> **missed a second, entirely separate 8-bit-specific mechanism** (a 16×16×16 RGB→palette-index
> quantization cube, `kfx_sim_state.colours`, used across 34 call sites in 9 files), **missed two
> more lookup tables** of the same shape as `pixmap`/`alpha_sprite_table`, **undercounted the
> `TbPixel` blast radius** by excluding `kfx_sim` entirely (which turns out to hold exactly the
> "hardcoded palette-index literal array" risk pattern item 4 warns about), and had a **factually
> wrong claim about custom sprite packs**. All fixed below; see each item for what changed and why.

## Goal

Replace the CPU raster path's 8-bit palette-index pixel with 32-bit RGBA, still CPU-rasterized
(GPU work is stage 3). Concretely: `TbPixel` stops meaning "index into the one active palette"
and starts meaning an actual colour, so any two differently-authored assets (UI chrome, land art,
a future higher-colour-count asset) can render correctly in the same frame with no shared-palette
ceiling and no remap step.

## Why software first, not straight to GPU

Two independent axes are tangled in "8-bit → 32-bit → GPU": *pixel format* and *where the pixels
are computed*. Doing the format change while still CPU-rasterizing keeps stage 2 a pure data-type
migration with a stable, already-understood execution model (the existing bucketed rasterizer,
whose correctness this project depends on matching 25+ years of Bullfrog-engine behavior) — no
new failure mode from also debugging a GPU pipeline at the same time. Stage 3 then has a much
smaller job: upload an already-32-bit buffer, rather than also being where the color-depth bugs
get found.

## Scope of the pixel-format change

**Correction**: stage 1's original *plan* called for `SwDrawTarget` pixel-write helpers
(`SwTargetWritePixel`/`SwTargetWritePixelShaded`) that this stage would widen from one byte to
four. **As actually implemented, stage 1 built read-side accessors only** —
`SwTargetVecScreen()`/`SwTargetPolyScreen()`/`SwTargetVecMap()`/`SwTargetVecScreenWidth()`/
`SwTargetVecWindowWidth()`/`SwTargetVecWindowHeight()` (`01-close-the-seam.md` Part A) — which
give callers the *target pointer* through the seam instead of a raw global, but the actual pixel
*write* expressions (`pixel_dst[i] = fade_table[...]`, `*d = block[...]`, etc.) are still direct
array writes at each call site, not routed through any write-helper function. That's a deliberate,
documented choice (Part A: "Kept as plain functions, not virtual methods... this is a hot inner
loop"), not an oversight — but it means **this stage inherits a different starting point than its
own first-pass plan assumed**: there is no existing `SwTargetWritePixel` to widen. Adding one now
(or deciding not to, and instead changing each write site's element type directly, matching how
item 1's per-pixel blend-math rewrite already has to touch every one of those sites anyway) is
this stage's own design decision, not a mechanical follow-through on stage 1. Given item 1 already
requires touching every per-pixel write site in `bflib_render_trig.c` (30 sites) and
`bflib_render_gpoly.c` (1 site) to replace the table lookup with blend math, doing the pixel-width
change *at the same sites, in the same pass* is likely simpler than introducing a new indirection
layer first and then immediately rewriting what's behind it — but this is worth a deliberate
choice when this stage is actually scoped in detail, not an assumption carried over from a
superseded plan.

What still needs direct work, because it's not just "write a wider pixel":

### 1. Retire the 8-bit shade/fade/ghost/alpha remap tables

`struct TbColorTables pixmap` (`fade_tables[64*256]`, `ghost[256*256]`, four
`flat_colours_*[2*256]`, `map_abyss[256]`) and `alpha_sprite_table` (`vidmode.h:113-135`) encode
`output = table[(shade << 8) | palette_index]` — a lookup with no direct RGB equivalent, because
the table's second axis *is* an 8-bit palette index. Replace each with real per-pixel math against
the sample's actual RGBA value:

| Old table | What it did | RGB equivalent |
|---|---|---|
| `fade_tables[64*256]` | per-shade-level darkening lookup (lighting) | `colour * shade_factor` (linear multiply), computed from the same shade value that used to index the table |
| `ghost[256*256]` | ghost/invisible-creature blend | alpha blend at a fixed ratio against the destination pixel already in the framebuffer |
| `flat_colours_tl/tr/br/bl[2*256]` | 4-corner gouraud shading ramps | already-interpolated per-vertex RGB, no table needed — this is the one case where going to true colour *simplifies* the code, since gouraud shading is naturally a colour interpolation, and the 8-bit version only needed a table because it had to stay within palette indices |
| `alpha_sprite_table` (8 ramps × 256, plus a `void_black[256]` member and 191×256 of unused padding not previously mentioned) | fixed alpha-blend ramps per named colour | standard `src*a + dst*(1-a)` blend, `a` taken from whichever ramp the call site used to select |
| `white_pal[256]`/`red_pal[256]` (`vidmode.h:152-153`, missed by the original pass) | flash-blend remap for creature possession/damage flash, cached as `whitepal.col`/`redpal.col` (`vidmode.c:568-592`) | same palette-index-remap shape as `ghost`, same fix: real blend math against the flash colour |
| `map_fade_ghost_table` (`engine_redraw.c:77`, populated at `:341-361`, missed by the original pass) | a separate, dynamically-computed 256×256 additive-blend table (reuses `poly_pool` as scratch space, cached as `data/mapfadeg.dat`) for the map fade-in/out transition | same fix again — it's not part of `struct TbColorTables pixmap` at all, a fully independent table with the identical problem |

**Where the actual per-pixel lookups happen — corrected from the original pass, which
misattributed this.** `engine_render.c` does not do per-pixel table lookups; it only *wires up*
the table pointers (`render_fade_tables = pixmap.fade_tables; render_ghost = pixmap.ghost;
render_alpha = (unsigned char *)&alpha_sprite_table;` at `engine_render.c:6815-6817` and again at
`:7250-7252` — the original pass's citation of these lines dropped the `render_alpha` line each
time) and *selects a slice* for `lbSpriteReMapPtr` (`:5198,5207,5233,5238,8172,8180,8265,8277,
8287,8297` — the `white_pal`/`red_pal` selection sites at `:8265,8277,8287,8297` weren't counted
by the original pass at all). **The actual `table[(shade<<8)|index]`-shaped per-pixel reads live
in `bflib_render_trig.c`** (`src/kfx_platform/src/renderer/software/`, ~4631 lines): 19 of its 25
`trig_render_md*` rasterizer functions (md04 through md26, excluding the unshaded md00-md03)
capture `render_fade_tables`/`render_ghost` into local pointers and perform **30 distinct
per-pixel lookup expressions** across them. `bflib_render_gpoly.c` has exactly one such site
(`draw_gpoly_line`'s `fade_table[texel | shade]`, line 694 — the site stage 1's Part A already
routed the *pointer acquisition* for, deliberately leaving the lookup itself for this stage).
`render_alpha` has exactly one consumer, `bflib_vidraw_spr_norm.c`'s `DrawAlphaSpriteUsingScalingData`.

**This changes where this stage's work actually lands**: budget the bulk of item 1 as a
`bflib_render_trig.c` rewrite (19 functions, 30 lookup sites — by far the largest single piece of
work in this stage), not an `engine_render.c` one. This also directly fulfills the deferral stage
1's Part A made explicitly for this reason (`docs/refactor/renderer/01-close-the-seam.md`, "do
this conversion as part of stage 2's own pass over the file when the write side changes too") —
that file's `vec_screen`/`vec_map`/`vec_window_*` *read*-side conversion to `SwTargetVecScreen()`
etc. and this item's shading-math rewrite should happen together, in the same pass over
`bflib_render_trig.c`, not as two separate visits to the same ~4600-line file. Do this as its own
sub-phase with its own before/after screenshot pass (lighting is the single most visible thing a
player will notice changing).

### 2. Sprite/texture data: expand at draw time, don't re-author assets

`TbSprite`/`TbSpriteData` stay exactly as they are on disk and in memory — 1-byte-per-pixel,
RLE-coded, palette-indexed (`bflib_sprite.h:35,37-46`; confirmed this holds identically under both
the `SPRITE_FORMAT_V2` and non-V2 build configuration — that `#ifdef` only changes the
`SWidth`/`SHeight` field width, never the pixel data format). Re-authoring the entire asset set as
RGBA is out of scope (huge effort, no asset-pipeline benefit). Instead, expand a sprite's
palette-indexed bytes to RGBA **at blit time**, sampling whatever palette that specific draw call
is using:

**Correction to the original pass's custom-sprite-pack claim**: it stated custom sprite packs
load ".dat sprite sheets in the same 8-bit format," used as a reason re-authoring assets would
break mod compatibility. This is factually wrong and worth getting right, because the real
situation is better for this stage, not worse. `custom_sprites.c`'s actual load path
(`add_custom_sprite` → `decode_png_to_sprite`, line 860) decodes **PNG files** from a level's
`mapNNNNN.zip` — full RGB source images — then **lossy-quantizes them down** to palette-indexed
RLE via `create_rgb_to_pal_table()` (`:1159`) and `compress_raw()` (`:1201`) before storing them
as a `TbSprite`. `MapZipCallbacks` (`custom_zip.h`/`.cpp`) is unrelated to sprite decoding — it's
just the path-resolution callback for locating files inside the zip. So mod sprite sources are
**already RGB** today; the engine is the one throwing colour fidelity away at load time. Once
`TbPixel` is real RGBA, this load path can simply stop quantizing — a quality improvement for
custom sprite packs, not a new compatibility surface to design for.

- The blit primitives already resolve a source byte through a palette or remap table per pixel
  (`bflib_vidraw_spr_{norm,remp,onec}.c`) — change what they resolve *to* (an RGBA quad written
  via `SwDrawTarget`) without changing what they resolve *through* (still the caller-supplied
  palette/cmap). This is a mechanical change to the blit inner loops, not a new pipeline.
- This is what actually removes the one-palette-per-frame ceiling: because expansion happens per
  draw call against that call's own palette, the land-art panel and the UI chrome panel can each
  use their own palette in the same frame with no shared-palette compromise and no
  `land_preview_remap_screen_to_shared_palette` nearest-color remap step. Once this lands,
  `land_preview_load` (`frontmenu_landpreview.c`) should drop the backup/restore/remap dance
  entirely — update [`colordepth/00-notes.md`](../gui/colordepth/00-notes.md) at that point,
  per 00-overview.md's cross-reference note.
- Consider a small per-frame cache (source sprite pointer + palette generation → expanded RGBA
  buffer) if profiling shows repeated re-expansion of the same sprite/palette pair costing real
  time — don't build this speculatively; measure the naive per-draw-call expansion first.

### 3. Retire the RGB→palette-index quantization cube (new, missed entirely by the original pass)

A second, completely independent 8-bit-specific mechanism from item 1's shading tables:
`kfx_sim_state.colours[16][16][16]` (`kfx_sim_state.h:379`, typed `TbRGBColorTable` =
`unsigned char[16][16][16]`, `vidfade.h:40`) quantizes an RGB triple down to its nearest colour in
the *current* 256-entry palette — 16 levels per channel, `colours[r>>4][g>>4][b>>4]` → a palette
index. Populated once by `compute_rgb2idx_table()` (`vidfade.c:179-195`) against `engine_palette`
(the live active palette), called via `init_rgb2idx_table()` (`vidmode.c:555-566`) →
`init_colours()` (`vidmode.c:594`) → `src/main.cpp:212` at startup, cached to disk as
`colours.col` and only recomputed if that cache is missing.

**34 call sites across 9 files** — `kfx_game`, `kfx_sim`, `kfx_render`, `kfx_frontend`, and
`main.cpp` — read `kfx_sim_state.colours[...]` to pick a UI/overlay draw colour by approximate RGB
(box borders, bar fills, status colours), always feeding the result straight into
`RendererSetDrawColour()`/`LbDrawBox()` as a palette index. Every call site is UI-chrome colour
selection, not gameplay-critical or intentionally palette-locked art — there's no reason found for
this quantization beyond "the draw call needs a palette index and this is how you get one from an
approximate RGB colour when the framebuffer is paletted." Once `TbPixel` is real RGBA, this
entire mechanism becomes unnecessary: every call site can construct its colour directly from three
0-255 components instead of quantizing into a 16-level cube and looking up an index. This is the
same category of simplification as item 1's gouraud-shading row — going to true colour removes a
whole subsystem, it doesn't just widen it. Budget: find and convert 34 call sites (mechanical,
each one becomes a direct RGB triple instead of a `colours[r][g][b]` lookup) plus retire
`compute_rgb2idx_table`/`init_rgb2idx_table`/the `colours.col` cache file entirely.

### 4. `TbPixel` typedef and its blast radius

`bflib_video.h:54`'s `typedef unsigned char TbPixel;` becomes a 4-byte RGBA type (a small
`struct`/`uint32_t` with named channel accessors, not a bare `uint32_t` with manual shifts
scattered at call sites).

**Scope correction**: [00-overview.md](00-overview.md#whats-still-genuinely-8-bit-and-where)'s
22-file count is accurate only for the three directories it was taken from
(`src/kfx_render`, `src/kfx_frontend`, `src/kfx_platform/src/renderer` — and even there,
`SwDrawTarget.h` was a false negative, since it lives under `include/renderer/software/`, a path
the original `grep -rl ... src/kfx_platform/src/renderer` didn't reach). Widening the search to
the whole tree finds **12 more files this stage's `TbPixel` audit needs to cover, most importantly
`kfx_sim` — never mentioned by the original pass at all**:

- `src/kfx_sim/src/thing_creature.c:4353` (`sizeof(TbPixel)` in a `memset`).
- `src/kfx_sim/src/creature_graphics.c:385` (`tint_thing(..., TbPixel colour, ...)`).
- **`src/kfx_sim/src/player_data.c:38-42,564` and `src/kfx_sim/include/player_data.h:274-278,314`
  — hardcoded literal palette-index arrays typed `TbPixel`**, e.g.
  `TbPixel player_path_colours[] = {131, 90, 163, ...};`. This is exactly the "small-integer
  palette-index semantics baked into a literal" risk pattern this item's own audit exists to
  catch — a mechanical `TbPixel` type-widen would silently reinterpret these literals as RGBA
  colours rather than palette indices. Treat as the highest-priority file in this audit, not an
  afterthought found by a wider grep.
- Also now in scope: `src/kfx_platform/src/bflib_mspointer.cpp` (`PointerDraw`'s `outbuf`
  parameter), `bflib_video.c`, `bflib_video.h` (the typedef site itself), `bflib_vidraw.h`,
  `bflib_render.h`, `src/kfx_platform/tests/bflib_mspointer_test.cpp`, and
  `src/kfx_config/include/config_lenses.h:44` (`TbPixel palette[PALETTE_SIZE]`).

Every file in the combined list needs its `TbPixel` arithmetic checked: code that did `TbPixel`
comparisons/arithmetic assuming "small integer, palette index semantics" (e.g. `if (pixel == 0)`
meaning "transparent index 0", or `player_path_colours[]`'s literal palette-index values above)
needs to keep meaning the same thing in RGBA terms (e.g. alpha channel == 0, not a magic colour
value; the path-colour literals need converting to actual RGB triples, not reinterpreting as one)
— audit each site rather than assuming a mechanical type swap is safe everywhere.

### 5. Display palette plumbing becomes vestigial, not deleted

`RendererPaletteSet`/`RendererGetActivePalette`/`SetDisplayPalette` (`RendererManager.h`/`.cpp`)
stay — they're still how the game's *authoritative* 256-colour palette is tracked and how
palette-indexed source assets get resolved to RGB at expansion time (§2 above). What goes away is
`RendererSoftware::SetDisplayPalette`'s `SDL_SetPaletteColors` call
(`RendererSoftware.cpp:19-34`) and the `lbDrawSurface`-is-an-`SDL_Surface`-with-a-palette
assumption, since the draw surface itself is no longer index-format. The actual creation site to
change is `bflib_video.c:594`'s `lbDrawSurface = SDL_CreateSurface(mdinfo->Width, mdinfo->Height,
SDL_PIXELFORMAT_INDEX8);` — the hardcoded `SDL_PIXELFORMAT_INDEX8` is the one place this stage's
target format actually gets decided; everything downstream (including the mouse cursor's backup
surfaces, see below) derives its format from this surface rather than hardcoding its own, so this
single change is expected to propagate widely with no separate fixes required at those call
sites. `PresentFrame` (`RendererSoftware.cpp:122-146`) simplifies too: no more `SDL_BlitSurface`
INDEX8→RGBA conversion (line 134) — the CPU buffer is already RGBA, so presentation becomes a
direct texture upload (`SDL_UpdateTexture`, replacing the lock/blit/unlock dance), which is also a
small performance win, not just a simplification. This is a natural seam to keep in mind for
stage 3, since "direct texture upload" is one step short of "the GPU already owns the buffer."

**Confirmed self-propagating**: [01-close-the-seam.md](01-close-the-seam.md) Part D investigated
the mouse cursor's backup/restore surfaces (`bflib_mspointer.cpp`'s `surf1`/`surf2`, backed by
`struct SSurface`) specifically to check whether they'd need their own fix here. They won't —
`LbScreenSurfaceCreate` (`bflib_vidsurface.c:47-68`) reads `lbDrawSurface->format` live at
creation time rather than hardcoding a format, and `LbI_PointerHandler::Initialise()` recreates
these surfaces every time the cursor sprite changes (frequent), so the format change above
propagates to them automatically with no code change needed in `bflib_mspointer.cpp` itself.

## Non-goals for this stage

- No GPU work (stage 3).
- No asset re-authoring — sprites stay 8-bit indexed on disk (though custom sprite packs get a
  free quality improvement, see item 2's correction above — not a goal of this stage, a side
  effect).
- No change to game logic that reads `TbPixel` values for gameplay purposes. Item 4's audit
  already found the one concrete candidate worth naming here —
  `player_data.c`/`player_data.h`'s `player_path_colours[]`-style literal arrays — and confirmed
  it's cosmetic (pathing-visualization colour), not gameplay logic, so it's in scope for
  conversion, not exempted by this non-goal. Still audit the rest of item 4's file list for
  anything that *would* be exempted; don't assume there's nothing else.

## Verification

- Lighting/shading parity is the highest-risk area — a before/after screenshot pass across every
  lens effect (`MistEffect`, `FlyeyeEffect`, `OverlayEffect`, `DisplacementEffect`,
  `PaletteEffect`), a dungeon room with varied torch lighting, the ghost/invisible-creature effect
  specifically (the `ghost[256*256]` table's replacement), the possession/damage flash effect
  (`white_pal`/`red_pal`'s replacement), and a map fade-in/out transition
  (`map_fade_ghost_table`'s replacement) — all four found during the revision above, all easy to
  miss in a screenshot pass that only thinks of "lens effects." Given item 1's corrected scope,
  the actual highest-risk *file* is `bflib_render_trig.c` (19 of 25 rasterizer functions
  rewritten) — treat a dungeon-room screenshot pass across varied lighting/texture/creature
  combinations as the primary regression net for this file specifically, not an afterthought.
- Confirm the Land selection screen's land-art panel now renders at full source fidelity (the
  user-supplied reference screenshot from the colordepth investigation — vivid teal sky, green
  grass, blue water — is the target to match, not the sepia-remapped current output).
- Confirm item 3's removal (the RGB→palette-index quantization cube) doesn't visibly shift any
  UI-chrome colour — a before/after screenshot of the 34 call sites' actual screens (status bars,
  event message boxes, in-game tabs) should be pixel-identical or better, never different, since
  removing quantization can only *increase* fidelity relative to the same nominal RGB triple.
- `KFX_OS=linux ./build-cmake.sh` + mingw cross-compile.
- `python3 scripts/check_layering.py --strict`.
- Full `src/kfx_render/tests/` + `src/kfx_frontend/tests/` Catch2 run; any test asserting on raw
  `TbPixel` byte values needs updating for the new type, which is itself a useful audit of where
  such assertions exist.
