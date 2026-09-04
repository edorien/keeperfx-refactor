# Stage 2 design spec — TbPixel type, blend formulas, per-site migration plan

Status: **design complete — every formula and open question resolved (§6), nothing deferred to
implementation time. Implementation in progress.** Companion to
[02-32bit-software-renderer.md](02-32bit-software-renderer.md) (the scope/plan doc) — this is the
concrete, implementation-ready specification requested before writing any stage-2 code, since
stage 2 is a flag-day change (`TbPixel`'s meaning shifts everywhere at once, 215 occurrences
across 34 files) with no safe intermediate checkpoint and no way for the author to visually verify
lighting/colour correctness — getting the formulas wrong here means wrong-but-plausible-looking
lighting that's easy to ship and hard to notice. See
[02b-legacy-bugs-found.md](02b-legacy-bugs-found.md) for pre-existing bugs the line-by-line
implementation work has turned up — the migration surfaces them because it touches every pixel-
write site, not because it went looking.

**Method**: every blend formula below is extracted from the functions that *generate* the
existing 8-bit lookup tables from an RGB source palette (`vidfade.c`'s `compute_fade_tables`/
`compute_alpha_table`/`compute_rgb2idx_table`/`compute_shifted_palette_table`, and
`engine_redraw.c`'s `generate_map_fade_ghost_table`) — not reverse-engineered from the tables'
runtime bit-packing. Those functions already express the intended math in RGB terms (they build
an RGB→palette-index table *from* RGB inputs), so they're the authoritative source of intent, not
an approximation of it.

> **Update (2026-09-03):** two follow-up passes closed every question the first draft left open.
> §2.1's fade-table gap (whether `fade_tables`' rows 32-63 are reachable at runtime) is resolved
> by tracing the `shade_intensity` → `S` → `shade` pipeline through `engine_render.c` end-to-end —
> both formula segments are real and reachable in normal gameplay, not one live and one dead. The
> smaller remaining items (`white_pal`/`red_pal`'s exact shift values, `void_black`'s status, and
> `tables.dat`'s on-disk-layout question) are resolved too — see §2.4, §2.3, and §6 respectively.
> Nothing in this design is deferred to implementation time anymore.

> **Scope correction (2026-09-03, mid-implementation):** §4's Bucket A/B file audit undercounts
> `pixmap.ghost`/`pixmap.fade_tables`/`pixmap.map_abyss`'s live consumers. Beyond `vidfade.c` (the
> generators) and `vidmode.c`/`engine_render.c` (the `render_fade_tables`/`render_ghost`/
> `render_alpha` wiring, already retired), these raw table arrays are read directly — not through
> the now-deleted wiring pointers — in at least:
> - **`engine_render.c`'s `lbSpriteReMapPtr` sites** (`:5205`, `:5214`, `:8174`, `:8182`) — this is
>   the `remp`-family `cmap` mechanism §3.2 already flags as needing caller-by-caller tracing, and
>   these ARE those callers. `lbSpriteReMapPtr = &pixmap.ghost[256 * tint_colour]` selects the
>   ghost table's `ref=tint_colour` row (→ `render_ghost_blend(expand_indexed_pixel(tint_colour,
>   pal), dest)` per §2.2); `lbSpriteReMapPtr = &pixmap.fade_tables[shade<<8]` selects the
>   fade-table's `shade` row (→ `render_shade(expand_indexed_pixel(src, pal), shade)` per §2.1).
>   Retiring `pixmap.ghost`/`fade_tables` requires replacing `lbSpriteReMapPtr` (currently
>   `unsigned char *`, `bflib_vidraw.h:173`) with a mode+parameter pair (e.g. a `{REMAP_NONE,
>   REMAP_GHOST, REMAP_SHADE}` enum plus a `uint8_t` param) that `bflib_vidraw_spr_remp.c`'s blit
>   functions switch on and compute per-pixel, instead of indexing a precomputed table slice.
> - **`MistEffect.cpp:230-231`** — reads `pixmap.fade_tables`/`pixmap.ghost` slices directly for a
>   screen-space mist blend; not yet traced to a specific §2 formula.
> - **`gui_parchment.c:258,329,334,336,340,342`** — overhead-map colour/remap selection, mixing
>   `pixmap.ghost` slice pointers and a direct `pixmap.map_abyss` read; already flagged in §4's
>   Bucket B row but that row undersells it — this is the same `remp`-style "pointer into a table
>   row" pattern as the `engine_render.c` sites above, not a simple constant swap.
> - **`gui_draw.c:896,926`** — `LbSpriteDrawResizedRemap(..., &pixmap.fade_tables[remap*256])`,
>   same `remp` pattern, a third call site family for the shade formula.
> - **`frontmenu_ingame_map.c:1022-1135`** — minimap panel-colour computation reads `pixmap.ghost`/
>   `pixmap.map_abyss` directly as palette-index bytes (`PanelColours[...] = pixmap.ghost[...]`);
>   §4 already lists this file under Bucket B for other reasons, but these specific lines weren't
>   called out.
> - **`main.cpp:225`** — `lbDisplay.GlassMap = pixmap.ghost;` (whole-table pointer, not a slice);
>   `GlassMap`'s own consumers need checking once `pixmap.ghost` no longer exists.
>
> None of this blocks the `engine_render.c`/`engine_textures.c` work already landed (those files
> only touched the `render_fade_tables`/`render_ghost`/`render_alpha` *wiring*, which is genuinely
> dead now — `TbPixel_Unpack` and the wiring pointers themselves have zero remaining callers). But
> it means retiring `struct TbColorTables`/`struct TbAlphaTables`/`white_pal`/`red_pal` themselves
> (§2's stated end goal) is a materially bigger next step than §4 implies: at least 6 more files
> need their own caller-by-caller formula tracing before the structs' fields can actually be
> deleted, not just 1-2. Do this as its own dedicated pass, file by file, the same way the
> `onec`/`gpoly` work was done — don't attempt it as a drive-by fix alongside unrelated compile
> errors.

> **Second scope correction (2026-09-03, after `bflib_render_trig.c`):** two more findings from
> that pass:
> - **`vec_colour` is not always a colour.** Tracing `trig_render_md07`'s bit-packing
>   (`RendVec_mode07`, `VM_SolidColor`'s actual renderer) showed the original used `vec_colour` as
>   a raw 0-63 shade *level* applied to a `vec_map` texture sample — not a resolved colour, despite
>   the enum name and despite `VM_FlatColor`/`VM_QuadFlatColor` genuinely using it as one two modes
>   over. Confirmed by `trig_render_md10` (`VM_SpriteTranslucent`) using the identical bit-packing
>   shape for the same variable, and by trig()'s own `vec_colour == 0x20` dispatch check only
>   making sense for a small integer. **Fixed**: `engine_render.c`'s 13 `VM_SolidColor` call sites
>   (`case 12`-`26` of the dome/sphere LOD-fallback switch, plus `QK_PolygonSimple`) now write
>   `vec_shade` instead of resolving through `expand_indexed_pixel()`; `trig_render_md07`/`md10`
>   and the `trig()` dispatcher's `0x20` check now read `vec_shade` to match. This was a genuine
>   correction to work landed in the *previous* session's `engine_render.c` migration, caught only
>   by reading `bflib_render_trig.c`'s actual consumer — a concrete instance of why §3's "trace the
>   call site before assuming" guidance matters even for sites that look done.
> - **Only 8 of the file's 19 `trig_render_md*` functions are reachable.** `engine_render.c` is the
>   *only* writer of `vec_mode` anywhere in the codebase (confirmed by grep), and it only ever sets
>   `VM_FlatColor`/`TriangularGouraud`/`TriangularTexture`/`QuadFlatColor`/`QuadTextured`/
>   `TriangularTextured`/`SolidColor`/`SpriteTranslucent` — i.e. `RendVec_mode{00,02,03,04,05,06,
>   07,10}`. `trig_render_md{01,08,09,12..26}` (11 functions) can never execute in KeeperFX. They
>   were still migrated to `TbPixel` (the file has to compile as a whole), using the same formula
>   derivation as their live siblings, but their exact behaviour is unverified and unverifiable —
>   flagged inline at each one. Worth knowing before spending more time on this file: `md16`/`md17`
>   in particular chain `render_shade()` into `render_ghost_blend()`, `md26` has a third
>   (threshold-gated) branch, and none of it is observable in real gameplay.
> - **Fixing `bflib_vidraw.h`'s `setup_outbuf()`/`lbSpriteReMapPtr` signatures (needed for
>   `bflib_vidraw_spr_norm.c`, two sessions back) surfaced a new, large front**: a full rebuild
>   after the `bflib_render_trig.c` work shows ~100 errors across `kfx_frontend/` (`frontmenu_
>   ingame_map.c` 46, `gui_parchment.c` 19, `frontmenu_ingame_tabs.c` 7, `gui_boxmenu.c` 6,
>   `gui_draw.c` 4, and smaller counts in `frontmenu_ingame_evnt.c`/`front_simple.c`/`frontmenu_
>   specials.c`/`frontmenu_net.c`/`front_network.c`/`front_landview*.c`/`lvl_script_commands.c`) —
>   previously unreached because Ninja doesn't get far enough to compile them while earlier files
>   in the dependency graph still fail. Not part of the original 34-file audit's Bucket A/B lists at
>   all (that audit was scoped to `src/kfx_platform`/`src/kfx_render`, not `src/kfx_frontend`) —
>   this is GUI-layer sprite/panel drawing, a distinct area needing its own pass, most likely
>   following the same `expand_indexed_pixel`/literal-colour-array patterns established so far.

> **Third scope correction (2026-09-03, lens effect subsystem investigated, not migrated):**
> `lens_api.c`/`LensManager.cpp`/`thing_creature.c`'s remaining errors all come from the same
> boundary — `RendererGetFramebuffer()` now returns `TbPixel*`, but `LensManager::Draw()`,
> `LensRenderContext::{src,dst}buf`, `LensManager::CopyBuffer()`, and every `LensEffect::Draw()`
> override (`MistEffect.cpp`, `DisplacementEffect.cpp`, `OverlayEffect.cpp`, `FlyeyeEffect.cpp`,
> `LuaLensEffect.cpp`, `PaletteEffect.cpp` — ~1700 lines total) still declare `unsigned char*`.
> Investigated whether a thin `reinterpret_cast` bridge at the `LensManager::Draw()` boundary could
> defer the real migration; it cannot, for two reasons found while tracing it:
> - **`LensManager::CopyBuffer()` already has a live pitch/type mismatch.** Its `memcpy(dst, src,
>   width * sizeof(TbPixel))` was already updated for 4-byte pixels (by an earlier, incomplete pass
>   — not part of this migration), but `dst += dstpitch` / `src += srcpitch` still advance the
>   (still-`unsigned char*`) pointers by a raw pitch *count*, not the pitch's byte size. If
>   `dstpitch`/`srcpitch` are pixel counts (matching this migration's convention everywhere else —
>   see `SwTargetVecScreenWidth()`'s comment), every row after the first already lands ~4x short of
>   where it should, corrupting the copy. A `reinterpret_cast` bridge would inherit this bug
>   silently; the real fix is retyping `CopyBuffer` to `TbPixel*` throughout (both the memcpy width
>   *and* the pointer arithmetic then agree in the same unit), not casting around it.
> - **At least `MistEffect.cpp`'s `CMistFade::Render()` does real palette-index table math, not a
>   byte-agnostic copy.** Its inner loop reads `this->fade_data[(n << 8) + *src]` — `fade_data` is a
>   `pixmap.fade_tables`-shaped slice (confirmed: same `shade<<8 | texel` addressing as every other
>   `render_shade()` call site in this migration) — and writes the *looked-up palette index* to
>   `*dst`, not `*src` itself. A pointer-only retype would compile but silently corrupt the visual
>   result (a stale index reinterpreted as one channel of a `TbPixel`, not a real colour); this needs
>   the same `render_shade(expand_indexed_pixel(*src, palette), n)` treatment as everywhere else,
>   not just a type change. The other four effects have not been individually traced yet.
>
> **Net finding**: the lens effect subsystem needs its own dedicated pass — type retyping *and*
> formula work together, file by file (`LensRenderContext`/`CopyBuffer` first since everything
> routes through them, then each `LensEffect::Draw()` override) — the same way the `onec`/`gpoly`/
> `bflib_render_trig.c` work was done. Not attempted here; reverted an initial partial signature
> change on `LensManager::Draw()`/`m_spare_screen_memory` rather than leave the file in a
> half-migrated state. `lens_api.c` (1 error), `LensManager.cpp` (1), `thing_creature.c` (4) remain
> open pending that pass.

---

## 1. The `TbPixel` type

```c
// bflib_video.h
typedef struct TbPixel {
    uint8_t r, g, b, a;
} TbPixel;
```

Plain 4-byte POD struct, not a bit-packed `uint32_t` with manual shifts — matches the existing
codebase's preference for named fields over shift/mask arithmetic (see `struct SSurface`,
`struct GPolyDrawState`, etc.), and the doc's own stated intent ("a small `struct`/`uint32_t` with
named channel accessors, not a bare `uint32_t` with manual shifts scattered at call sites").

**Byte order chosen to match the existing presentation path exactly, avoiding a conversion step**:
`RendererSoftware.cpp` already creates its present-time texture as `SDL_PIXELFORMAT_RGBA32`
(`ensure_present_target()`, `RendererSoftware.cpp:68`) — SDL's `RGBA32` alias guarantees
byte-order `{R,G,B,A}` in memory regardless of host endianness (unlike the packed `RGBA8888`-style
formats, which are endianness-dependent). `struct TbPixel {r,g,b,a}` above is byte-for-byte
identical to that layout on both little- and big-endian hosts. Once stage 2 lands and
`PresentFrame` becomes a direct `SDL_UpdateTexture` (per `02-32bit-software-renderer.md` §5), the
CPU buffer can be handed to SDL with zero reinterpretation.

### Helper API (add alongside the typedef, `bflib_video.h`)

```c
static inline TbPixel TbPixel_RGB(uint8_t r, uint8_t g, uint8_t b) {
    TbPixel p = { r, g, b, 255 }; return p;
}
static inline TbPixel TbPixel_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    TbPixel p = { r, g, b, a }; return p;
}
#define TbPixel_Transparent ((TbPixel){0, 0, 0, 0})
static inline TbBool TbPixel_IsTransparent(TbPixel p) { return p.a == 0; }
static inline TbBool TbPixel_Equal(TbPixel a, TbPixel b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
```

**Why this matters for the migration**: today, "palette index 0 is the sprite transparency
marker" is a convention checked as `if (pixel == 0)` throughout the blit primitives
(`bflib_vidraw_spr_*.c`). That numeric-equality check has to become `TbPixel_IsTransparent(px)`
(alpha-based) everywhere it appears — this is exactly the class of site item 4's audit exists to
catch. `TbPixel_Equal` exists because `TbPixel == TbPixel` won't compile once it's a struct
(C has no struct equality operator) — any remaining `pixel == some_value` comparison that *isn't*
a transparency check (rare, but the audit below flags candidates) needs this instead.

### What does *not* change shape

`TbSpriteData` (`typedef unsigned char *`, `bflib_sprite.h:35`) — sprite source bytes stay
`unsigned char*`, one byte per source pixel, palette-indexed, exactly as
[02-32bit-software-renderer.md](02-32bit-software-renderer.md) §2 already specifies. `TbPixel` is
the *destination* (framebuffer) pixel type only. Don't conflate the two — a sprite byte is a
palette index (0-255) all the way until blit time; a `TbPixel` is always an actual RGBA colour.

---

## 2. Blend formulas, extracted from the table generators

Each row: the existing 8-bit table, its generator function (source of truth), the exact formula
in RGB terms, and the RGBA replacement.

### 2.1 `fade_tables[64*256]` — shading/lighting

Source: `compute_fade_tables` (`vidfade.c:63-95`), first inner loop (the part that actually
populates valid `fade_tables[0..16383]` content — see the note below on why the function's second
loop, which nominally continues past row 63, is a harmless artifact, not a second real segment).

```c
// vidfade.c:73-83 (i = shade row, 0..31; k = source palette index, 0..255)
r = spal[3*k+0]; g = spal[3*k+1]; b = spal[3*k+2];
*dst = LbPaletteFindColour(dpal, i * r >> 5, i * g >> 5, i * b >> 5);
```

**RGB formula — two segments, both confirmed reachable at runtime (resolved 2026-09-03, see
derivation below)**:

```
factor(shade) = shade <= 31  ?  shade / 32.0
                              :  (3*shade - 64) / 32.0        // shade in 32..63

output = clamp(source_colour * factor(shade), 0, 255)
```

`compute_fade_tables`'s *second* loop (`vidfade.c:85-95`, `i` from 32 to 191 step 3) continues
writing through the *same* incrementing `dst` pointer, which — because the loop's first phase
wrote exactly `32*256` elements (half of the declared `64*256`) — lands its next 32 iterations
(rows 32-63, using formula values `i = 32, 35, 38, ..., 125`) still *within* `fade_tables`' real
bounds, before the remaining ~22 iterations spill past the array into `ghost`'s memory (harmless,
because `compute_fade_tables` immediately overwrites `ghost` right after with its own correct
content, in the very next loop of the same function). **Rows 32-63 of `fade_tables` are real,
intentional content, not padding** — they follow a distinct, non-linear formula (`factor(row) =
(3*row-64)/32`) that exceeds 1.0, i.e. brightens *beyond* the source colour rather than
continuing to darken.

**Confirmed reachable at runtime, traced end-to-end**: `shade` (the value that indexes
`fade_tables`) is `vertex_a_shade = point_a->S >> 16` (`bflib_render_gpoly.c:857`), where `S` is
set from `coordinate_1_lightness << 8` in the "near, no distance fog" branch
(`engine_render.c:3050`; the fogged/far branches produce lower `S` values, capping out at the
same range from below — see next paragraph). `coordinate_1_lightness` is `EngineCoord.shade_intensity`
(`engine_render.c:3041`), which is computed per-subtile by `get_subtile_lightness()` and
explicitly clamped: `if (lightness < 0) lightness = 0; if (lightness > 16128) lightness = 16128;`
(`engine_render.c:1163-1166`). Composing the chain: `shade = (lightness << 8) >> 16 = lightness >> 8`,
so `lightness = 0` → `shade = 0`, and `lightness = 16128` (the clamp ceiling) → `shade = 16128 >> 8
= 63` **exactly** — the full 0-63 range is reachable by construction, not an edge case, and
`shade = 32` (where the second formula segment starts) corresponds to `lightness = 8192`, exactly
half the clamp ceiling — reached by any moderately well-lit subtile, not just extreme cases. The
far/fogged `S` branches (`engine_render.c:3052-3058`, lerping toward or hard-setting `0x8000`)
only ever *lower* `S` relative to the unfogged case, so they narrow the reachable range toward 0,
never introduce a value outside it.

**Why the "brightens beyond 1.0" segment doesn't look broken in the original renderer**: the old
per-pixel result was resolved through `LbPaletteFindColour(dpal, ...)` — a *nearest-match search*
against a palette whose channels only go to 63 (VGA 6-bit). Once `factor(shade)` pushes a channel
value past what any palette entry offers, the search just keeps returning the closest (brightest)
available entry — a natural saturation/highlight-rolloff, not literal unbounded brightening. The
RGBA replacement's explicit `clamp(..., 0, 255)` reproduces exactly this behaviour (values that
would exceed white just clip to it), so this is a faithful, confirmed drop-in — not an
approximation.

### 2.2 `ghost[256*256]` — ghost/invisible-creature blend

Source: `compute_fade_tables`, second table (`vidfade.c:97-113`).

```c
// vidfade.c:100-111 (i = reference/creature colour index, k = background/destination colour index)
unsigned long rr = spal[3*i+0], rg = spal[3*i+1], rb = spal[3*i+2];  // creature's own colour
r = dpal[3*k+0]; g = dpal[3*k+1]; b = dpal[3*k+2];                    // whatever's behind it
*dst = LbPaletteFindColour(dpal, (rr+2*r)/3, (rg+2*g)/3, (rb+2*b)/3);
```

**RGB formula**: `output = lerp(background_colour, creature_colour, 1/3)` — a **fixed 1/3-alpha
blend** of the creature's own colour onto whatever's already in the framebuffer at that pixel.
This is a genuine alpha-blend-against-destination operation (needs the *current* framebuffer
pixel as an input, not just the source sprite texel), matching
[02-32bit-software-renderer.md](02-32bit-software-renderer.md)'s original prediction exactly.

```c
// RGBA replacement — src is the creature's sprite-sample colour, dst is the current framebuffer pixel:
TbPixel out = {
    (uint8_t)((src.r + 2*dst.r) / 3),
    (uint8_t)((src.g + 2*dst.g) / 3),
    (uint8_t)((src.b + 2*dst.b) / 3),
    255
};
```

### 2.3 `alpha_sprite_table` — 8 named-colour alpha ramps

Source: `compute_alpha_table` (`vidfade.c:116-150`), called once per named ramp with a per-channel
delta (`compute_alpha_tables`, `vidfade.c:152-177`):

| Ramp | `(dred, dgreen, dblue)` |
|---|---|
| `white` | `(4, 4, 4)` |
| `yellow` | `(6, 4, 0)` |
| `red` | `(6, 1, 1)` |
| `blue` | `(2, 2, 6)` |
| `green` | `(2, 6, 2)` |
| `purple` | `(3, 0, 3)` |
| `black` | `(-2, -2, -2)` |
| `orange` | `(6, 3, 1)` |

```c
// vidfade.c:122-149, nrow = 0..7 (8 intensity steps)
int valR = clamp(blendR + baseCol[0], 0, 63);   // blendR = nrow * dred, accumulated per row
int valG = clamp(blendG + baseCol[1], 0, 63);
int valB = clamp(blendB + baseCol[2], 0, 63);
alphtbl[nrow*256 + n] = LbPaletteFindColour(dpal, valR, valG, valB);
```

**RGB formula**: `output = clamp(source_colour + intensity * (dred, dgreen, dblue), 0, 63)` — a
straightforward **additive tint**, not a blend against the destination (unlike `ghost` above).
8 discrete intensity steps in the original; the RGBA replacement can keep the same 8 discrete
steps (call sites already select a `nrow` 0-7) or, since this no longer needs a precomputed table,
trivially support continuous intensity if any call site would benefit — check call sites before
deciding; don't add flexibility nothing asks for.

```c
// RGBA replacement, given a named ramp's (dr, dg, db) and integer step 0-7:
TbPixel out = {
    clamp255(src.r + step * dr_scaled_to_8bit),
    clamp255(src.g + step * dg_scaled_to_8bit),
    clamp255(src.b + step * db_scaled_to_8bit),
    src.a
};
```

Note the `(dred, dgreen, dblue)` deltas above are in the source palette's native 6-bit VGA scale
(0-63 range, same scale `LbPaletteFindColour`'s inputs use) — when computing against a true 0-255
`TbPixel`, scale each delta by `255/63 ≈ 4.05` (or use `chan6_to_8()`, already implemented in
`RendererManager.cpp:83-86`, as the exact reference for how this codebase already converts VGA-6
to 8-bit-per-channel).

**Correction (2026-09-03, superseding the original "confirmed dead" claim below)**:
`void_black[256]` is **not** dead the way `flat_colours_tl/tr/br/bl` (§2.7) are — it's reachable,
just never explicitly named. `render_alpha` (`bflib_vidraw_spr_norm.c`'s alpha-blend sprite path)
casts `struct TbAlphaTables` to a flat byte array and indexes it by raw offset
(`texel<<8 | dest`), so `void_black` (byte offset 0-255) *is* read at runtime whenever a sprite's
raw palette-index byte is `0` — a symbol-reference grep can't see an access pattern like that. See
[02b-legacy-bugs-found.md](02b-legacy-bugs-found.md) #2 for the full trace: `compute_alpha_tables`'s
flat-`144`-fill loop (`vidfade.c:156-159`) targets `black`, not `void_black` (likely a copy-paste
slip, adjacent similarly-named fields) — `void_black` is never written by any generator function,
so it's always BSS-zero. `unused[191*256]` (rows 65-255) is in the same position — also reachable
via the same offset scheme, also always zero, also never explicitly named. Both are preserved in
`render_alpha_blend()` (`bflib_render.h`) as a flat always-zero fallback for `texel < 1 || texel >
64`, matching the observed original behaviour exactly rather than guessing at intended content.

### 2.4 `white_pal[256]` / `red_pal[256]` — flash-blend remap

Source: `compute_shifted_palette_table` (`vidfade.c:206-222`), called from `init_whitepal_table`/
`init_redpal_table` (`vidmode.c:568-592`) with fixed, resolved shift arguments:

| Table | `(shiftR, shiftG, shiftB)` | Effect |
|---|---|---|
| `white_pal` | `(+48, +48, +48)` (`vidmode.c:583`) | strong uniform brightness boost toward white — the possession/full-flash effect |
| `red_pal` | `(+20, -10, -10)` (`vidmode.c:570`) | boosts red, pulls back green/blue — tints toward red, the damage-flash effect |

Same formula shape as the alpha ramps above (§2.3) — a single additive shift instead of 8
graduated steps: `output = clamp(source_colour + (shiftR, shiftG, shiftB), 0, 63)` in the
original's VGA-6 scale, scaled to 0-255 via `chan6_to_8()` for the RGBA replacement, same as
§2.3's note.

```c
// vidfade.c:211-220
int valR = clamp(spal[3*i+0] + shiftR, 0, 63);
// same for G, B
ocol[i] = LbPaletteFindColour(dpal, valR, valG, valB);
```

**RGB formula**: identical shape to the alpha-ramp formula above but a single fixed shift (no
8-step gradation) — `output = clamp(source_colour + (shiftR, shiftG, shiftB), 0, 63)`.

### 2.5 `map_fade_ghost_table` — map fade-in/out transition

Source: `generate_map_fade_ghost_table` (`engine_redraw.c`, cited by the earlier audit at
`:341-361`).

```c
unsigned char r = bpal[0] + spal[0];   // note: NOT divided, a straight sum
unsigned char g = bpal[1] + spal[1];
unsigned char b = bpal[2] + spal[2];
*out = LbPaletteFindColour(palette, r, g, b);
```

**RGB formula**: `output = colour1 + colour2` (additive/"screen"-style combination of the two
frame snapshots being cross-faded), relying on `LbPaletteFindColour`'s nearest-match search to
implicitly cap the visual result since VGA channels only go to 63 — **the RGBA replacement needs
an explicit `clamp(., 0, 255)`**, since real RGBA arithmetic won't auto-clamp the way a
nearest-palette-match search incidentally did.

### 2.6 `kfx_sim_state.colours[16][16][16]` — RGB→palette-index quantization cube

Source: `compute_rgb2idx_table` (`vidfade.c:179-195`). Already covered in
[02-32bit-software-renderer.md](02-32bit-software-renderer.md) §3 — **no formula needed for the
replacement**, since every one of the 34 call sites can construct a `TbPixel` directly from its
intended 0-255 RGB triple instead of quantizing into a 16-level cube and looking up a palette
index. This item's "formula" is simply: delete the cube, replace `kfx_sim_state.colours[r>>4][g>>4][b>>4]`
call sites with `TbPixel_RGB(r, g, b)` using the same nominal RGB values the cube's caller was
already choosing (check each of the 34 sites' literal RGB inputs — they're likely already
expressed as descriptive constants like "bright red" that map straightforwardly).

### 2.7 `flat_colours_tl/tr/br/bl[2*256]` — dead code, not a conversion target

**Correction to [02-32bit-software-renderer.md](02-32bit-software-renderer.md)'s item 1 table**,
which describes these as "4-corner gouraud shading ramps" needing conversion to "already-
interpolated per-vertex RGB." A full-codebase grep (`grep -rn flat_colours src/`) finds **only
the four struct-field declarations in `vidmode.h:116-119` — no generator function, no reader, no
writer anywhere in the codebase.** These fields are dead: never populated, never consumed.
**Correct action: delete these four fields from `struct TbColorTables` entirely** (as part of
retiring the struct), not "convert" them to anything — there's no live gouraud-shading-table
mechanism to preserve. If gouraud shading exists elsewhere in the rasterizer (plausible, given
`vertex_a_shade`/`vertex_b_shade`/`vertex_c_shade` per-vertex interpolation already exists in
`bflib_render_gpoly.c`), it doesn't currently route through these fields — confirm during
implementation that removing them doesn't silently disable something reached only via
`sizeof(struct TbColorTables)`-based serialization (the struct is loaded from `tables.dat` via
`LbFileLoadAt(fname, &pixmap)` checking `sizeof(struct TbColorTables)` exactly,
`vidmode.c:509` — removing fields changes this size and invalidates the cache file format, which
is expected and fine since the whole struct is being retired anyway, just confirm nothing else
depends on the on-disk `tables.dat` layout persisting).

### 2.8 `map_abyss[256]`

Source: `vidmode.c:529-531` (inline, not a separate `compute_*` function):
```c
for (int i = 0; i < 256; i++)
    pixmap.map_abyss[i] = abyss_colours[pixmap.ghost[i] * 3 >> 8];
```
where `abyss_colours[] = {160, 1, 253}` (`vidmode.c:506`) is a **3-entry palette-index constant
array** (not a formula) — `pixmap.map_abyss[i]` picks one of exactly 3 fixed abyss colours based
on `ghost[i]`'s value. **RGBA replacement**: keep the 3-entry constant, but as 3 actual `TbPixel`
RGB constants instead of palette indices (look up what palette entries 160/1/253 actually render
as today, e.g. via `LbPaletteGetReadonly()` against the shipped `data/palette.dat`, and hardcode
the resulting RGB triples) — the selection logic (`ghost[i]*3>>8`, a 3-way bucket by the *already
RGBA* ghost blend's brightness) can stay structurally the same, just index into the 3 RGB
constants instead of the old 3 palette indices.

---

## 3. Sprite blit-time RGBA expansion — mechanics

[02-32bit-software-renderer.md](02-32bit-software-renderer.md) §2 already specifies the *what*
(expand at blit time, sample through the caller's palette, don't re-author assets). This section
is the *how*, concretely enough to implement directly.

### 3.1 Expansion function

```c
// New, e.g. in bflib_vidraw.c or a new bflib_pixel_expand.c:
static inline TbPixel expand_indexed_pixel(uint8_t index, const unsigned char *pal /* 768-byte VGA-6 RGB triples */)
{
    if (index == 0) return TbPixel_Transparent;   // preserves today's "index 0 = transparent" convention
    return TbPixel_RGB(
        chan6_to_8(pal[3*index + 0]),
        chan6_to_8(pal[3*index + 1]),
        chan6_to_8(pal[3*index + 2]));
}
```

`chan6_to_8` already exists (`RendererManager.cpp:83-86`, currently `static`/file-local — promote
it to a shared header, e.g. `bflib_video.h` or a new small palette-utilities header, since both
`RendererManager.cpp` and every blit primitive will need the identical conversion).

### 3.2 Where this plugs into the existing blit primitives

`bflib_vidraw_spr_norm.c`/`_remp.c`/`_onec.c`'s inner loops today do (approximately)
`dst[i] = src_byte` (norm), `dst[i] = cmap[src_byte]` (remp — remapped through a caller-supplied
256-entry table, e.g. a fade/ghost/alpha-ramp table), or `dst[i] = fixed_colour` (onec — "one
colour" variants, used for silhouette/outline drawing). Each becomes:

- **norm**: `dst[i] = expand_indexed_pixel(src_byte, active_palette)`.
- **remp**: the `cmap` parameter today is a 256-entry `unsigned char*` remap table (a slice of
  `fade_tables`/`ghost`/an alpha ramp). Once those tables are retired (§2 above), callers that
  used to pass a `cmap` slice instead need to pass the *equivalent blend operation* — e.g. a
  shade level + the active palette, or a source/dest blend mode enum — rather than a raw table
  pointer. This is the one part of the blit-primitive rewrite that isn't just "swap a byte read
  for `expand_indexed_pixel`" — it needs each `remp`-family caller in `bflib_render_gpoly.c`/
  `bflib_render_trig.c`/`engine_textures.c` re-examined for *which specific §2 formula* it was
  invoking via its `cmap` argument, then wired to call that formula directly instead of indexing
  a table. Do this caller-by-caller, cross-referencing against §2's table above; don't try to keep
  a generic "remap callback" abstraction alive if only 1-2 formulas actually flow through it —
  check first.
- **onec**: not uniformly the simple case the first design pass assumed — split in two.
  `bflib_vidraw_spr_onec.c`'s plain variants (no `transmap` parameter) *are* simple: `dst[i] =
  transparent, else fixed_colour` — the "fixed colour" argument just becomes a real `TbPixel`.
  But 8 of the file's functions (`LbSpriteDrawOneColourUsingScaling{Up,Down}DataTrans{1,2}{RL,LR}`)
  take an extra `const TbPixel *transmap` parameter and compute
  `pxmap = (colour << 8) | dest_byte; *out = transmap[pxmap];` — a **table lookup keyed on both
  the fixed colour *and* the current destination pixel**, not a plain byte swap. **Confirmed by
  tracing every call site** (`bflib_vidraw_spr_onec.c:1312-1365`, the only callers of these 8
  functions): `transmap` is `render_ghost` at every single one — this whole family exists to draw
  a sprite's silhouette in `colour`, ghost-blended against whatever's on screen (the
  invisible-creature rendering path). It's a direct, already-solved application of §2.2's ghost
  formula, not a new unknown: replace the lookup with
  `*out = TbPixel_RGB((colour.r + 2*dst.r)/3, (colour.g + 2*dst.g)/3, (colour.b + 2*dst.b)/3)`
  (`dst` = the current value at `*out` before overwriting). Treat every `remp`/`onec`-family
  compile error the same way once real implementation starts: **trace the call site to its actual
  table argument before assuming it needs new formula work** — most will resolve to one of §2's
  already-derived formulas the same way this one did, confirmed by a first real build attempt
  that surfaced ~90 compile errors in this file alone once `TbPixel` was widened, all following
  this same handful of idioms.

### 3.3 Caching

Per `02-32bit-software-renderer.md` §2's own guidance: don't build a cache speculatively. If one
turns out to be needed after profiling, key it on `(sprite_data_pointer, palette_generation_counter)`
→ expanded `TbPixel*` buffer, where `palette_generation_counter` is a new monotonic counter
incremented every time `RendererPaletteSet` changes the active palette (cheap to add, makes cache
invalidation exact rather than approximate).

---

## 4. `TbPixel` site-by-site audit — the full 34-file list, classified

Extends [02-32bit-software-renderer.md](02-32bit-software-renderer.md) §4's file list with a
per-file disposition, so implementation is a checklist, not a re-investigation. **Bucket A**
(sites that just become RGBA automatically, no logic change beyond the type width) vs. **Bucket B**
(sites needing an explicit code change — a literal value, a comparison, or a formula).

**Bucket A — pure framebuffer/sprite-pixel plumbing, becomes RGBA with the type change alone:**

| File | Occurrences | What it is |
|---|---|---|
| `bflib_vidraw.c` | 33 | Core box/line/pixel draw primitives — `LbDrawBox`, `LbDrawPixel`, etc. |
| `bflib_vidraw_spr_norm.c`/`_remp.c`/`_onec.c` | 81 combined | Sprite blit inner loops — see §3 above for the `remp` family's extra work |
| `bflib_vidraw.h` | 18 | Declarations mirroring the above |
| `SwDrawTarget.h`/`.c` | 11 combined | Stage-1 accessors — pointer types only, no logic |
| `vidfade.c` | 3 | `TbPixel c = LbPaletteFindColour(...)` in the table-generator functions themselves — **these generators are being deleted per §2, not migrated**, so this is moot once §2's replacements land |
| `bflib_mspointer.cpp` | 4 | `PointerDraw`'s `outbuf` parameter and friends — cursor blit, already routed through `RendererGetFramebuffer()` (stage 1) |
| `bflib_video.c`/`.h` | 4 combined | The typedef site itself + `lbDrawSurface` format interactions |
| `bflib_render.c`/`.h` | 2 combined | `render_fade_tables`/`render_ghost`/`render_alpha` extern pointers — **retired per §2**, not migrated |
| `engine_lenses.c`, `LensManager.cpp` | 2 combined | `eye_lens_spare_screen_memory` — off-screen render target pointer |
| `lens_api.h` | 2 | Same pointer's declaration |
| `thing_creature.c` | 1 | `sizeof(TbPixel)` in a `memset` — automatically correct at the new size |
| `front_simple.c`, `front_simple.h`, `gui_draw.c`, `gui_draw.h` | 5 combined | Framebuffer copy/draw call sites, already stage-1-routed |
| `front_landview_multiplayer.c` | 2 | Same category |
| `frontmenu_ingame_evnt.c` | 2 | Same category |
| `bflib_mspointer_test.cpp` | 1 | Test fixture — update alongside `bflib_mspointer.cpp` |

**Bucket B — needs an explicit code change, not just a type-width change:**

| File | Occurrences | What needs changing |
|---|---|---|
| `player_data.c`/`player_data.h` | 6+6 | **Highest priority.** `player_path_colours[]`-style literal arrays (e.g. `{131, 90, 163, ...}`) are palette-index literals — each needs replacing with the actual RGB triple that index currently renders as (look up against the shipped palette, same "resolve once, hardcode the RGB result" approach as §2.8's `map_abyss` constants). |
| `frontmenu_ingame_map.c` | 16 | Minimap drawing (`panel_map_draw_pixel`, `col = 15`-style literals, `MapBackground` buffer). Mix of Bucket-A pointer plumbing and Bucket-B literal palette-index constants (e.g. line 674's `col = 15`) — audit each of the 16 individually, don't batch-assume. |
| `gui_parchment.c` | 6 | Overhead-map colour selection (`get_overhead_mapblock_style`'s callers, `draw_overhead_map`) — mix of `pixmap.ghost[...]`/`pixmap.map_abyss[...]` reads (retired per §2/§2.8, becomes a direct RGB constant lookup) and genuine pointer plumbing. |
| `creature_graphics.c` | 1 | `tint_thing(..., TbPixel colour, ...)` — a *tint* operation, i.e. this function's job is exactly the kind of blend §2.2/§2.3 replace; confirm which formula it currently implements via its own body before assuming it's just a parameter-type change. |
| `config_lenses.h` | 1 | **Resolved, not a remap table — a genuine bug-in-waiting if left as `TbPixel`.** `TbPixel palette[PALETTE_SIZE]` (`PALETTE_SIZE` = `3*PALETTE_COLORS` = 768, `bflib_video.h:43`) is consumed by `PaletteEffect.cpp:51`'s `player->lens_palette = cfg->palette;` and compared by pointer in `vidfade.c:368` (`pal == player->lens_palette`) — this is a real 768-byte **RGB palette** a lens effect can swap in wholesale (same shape as `engine_palette`/`frontend_palette`), using `TbPixel` today only because `TbPixel` currently *is* `unsigned char` and this field really wants "raw byte buffer," not "pixel value." **Must be retyped to `unsigned char palette[PALETTE_SIZE]` explicitly, not left as `TbPixel`** — if `TbPixel` silently widens to 4 bytes here, this field balloons to a 3072-byte array of the wrong shape entirely (768 RGBA structs instead of 256 RGB triples), and every consumer's indexing breaks silently rather than failing to compile. Flag this as a **required, not optional**, part of item 4's audit — it's the one site where "just widen the type" is actively wrong, not just imprecise. |
| `vidmode.c` | 2 | `init_colours`/`init_rgb2idx_table` and the `white_pal`/`red_pal` computation calls — **deleted**, not migrated, per §2.4/§2.6. |
| `engine_render.c` | 3 | Table-pointer wiring (`render_fade_tables = pixmap.fade_tables;` etc., stage 2's item 1 citation) — **deleted** along with the tables themselves, not migrated. |
| `vidfade.h` | 1 | `TbRGBColorTable` typedef itself — deleted along with `compute_rgb2idx_table` (§2.6). |
| `engine_render.h` | 1 | **Resolved**: `TbPixel stripey_line_color_array[16]` (`engine_render.h:102`) — a 16-entry literal colour array for a debug/selection "stripey line" effect, consumed at `engine_render.c:5979` via `LbDrawPixel(x, y, colored_stripey_lines[line_color].stripey_line_color_array[color_index])`. Same treatment as `player_data.c`'s literal arrays above: 16 palette-index literals need converting to their equivalent RGB triples, not reinterpreting in place. |

---

## 5. Recommended write-order (not an independent-checkpoint order — see caveat)

Even though stage 2 can't land in independently-buildable increments the way stage 1 did, there's
still a sensible order to *write* the code in, since later pieces depend on earlier ones existing
even if nothing compiles until all of them are in place together:

1. `TbPixel` type + helper API (§1) — foundational, nothing else makes sense without it.
2. The §2 blend-math replacement functions, as free-standing functions first (they're pure
   functions of `TbPixel`/`uint8_t` inputs — genuinely unit-testable in isolation *before* wiring
   them into the rasterizer, unlike everything else in this stage). Write Catch2 tests for these
   against known input/output pairs derived from the old tables' actual generated content
   (e.g. spot-check `fade_tables[5*256 + 100]`'s current value against the new formula's output
   for shade=5, palette-index=100, converted through the same source palette) — this is the one
   piece of stage 2 that *can* get a real automated correctness check before any visual testing,
   so use it.
3. §4's Bucket B sites — the literal-value and comparison audits — since these don't depend on
   the rasterizer rewrite and can be drafted independently.
4. `bflib_render_trig.c`'s 30 lookup sites + `bflib_render_gpoly.c`'s 1 site — wire the §2
   functions in, replacing table lookups. This is where stage 1's deferred `vec_*` accessor
   conversion for `bflib_render_trig.c` lands too (same file, same pass, per
   `01-close-the-seam.md`'s note).
5. §3's sprite blit-time expansion.
6. `bflib_video.c:594`'s `lbDrawSurface` format change + `RendererSoftware`'s present-path
   simplification — last, since everything above needs to be ready for the framebuffer to
   actually start receiving real RGBA writes.

## 6. Nothing left open — every question from the first design pass resolved

The `fade_tables` rows-32-63 reachability question (§2.1, traced end-to-end through
`shade_intensity`/`apply_lighting_to_triangle_*`/`ABYSS_SHADE`), `white_pal`/`red_pal`'s exact
shift arguments (§2.4), `void_black`'s status (§2.3 — later corrected during implementation, see
its note and [02b-legacy-bugs-found.md](02b-legacy-bugs-found.md) #2: reachable via an implicit
byte-offset access, not dead), `config_lenses.h`'s palette field (§4), and `engine_render.h`'s
`stripey_line_color_array` (§4)
were all resolved in the prior revision.

**`tables.dat`'s on-disk layout — resolved (2026-09-03), no action needed.** `tables.dat` (and
its four siblings — `alpha.col`, `colours.col`, `redpal.col`, `whitepal.col`, one cache per table
this stage retires) are **not tracked in version control and not touched by any packaging
script** — confirmed via `git ls-files` (empty) and `.gitignore:82`'s `/core_files/` entry, plus a
`grep` across every `build/` packaging script finding zero references to any of the five
filenames. They live in `core_files/data/` — this repo's convention for a developer's local,
proprietary Dungeon Keeper data install (per `build/cmake/modules/StageFtestData.cmake`'s own
comment: *"the real, proprietary Dungeon Keeper data files"*), the same directory structure an
end user's real game install has. Each file is a pure runtime cache: `vidmode.c`'s
`init_fades_table`/`init_alpha_table`/`init_rgb2idx_table`/`init_whitepal_table`/`init_redpal_table`
compute it once via the §2 generator functions and save it via `LbFileSaveAt` purely to skip
recomputation on the next launch, loading it back via `LbFileLoadAt(fname, &pixmap) !=
sizeof(struct TbColorTables)`-style exact-size checks (confirmed: the shipped
`core_files/data/tables.dat` is exactly 84224 bytes, matching `sizeof(struct TbColorTables)`
computed today bit-for-bit — `64*256 + 256*256 + 4*2*256 + 256`).

Once stage 2 deletes `struct TbColorTables`/`struct TbAlphaTables`/`TbRGBColorTable` and every
`compute_*`/`init_*_table` function that reads or writes these five files, **the code path that
would even notice a stale cache no longer exists** — there's no "graceful fallback on size
mismatch" scenario to design for, because nothing calls `LbFileLoadAt` against these filenames
anymore, full stop. The only real consequence: existing local installs (this dev's `core_files/`
included) end up with ~150KB of five orphaned, never-again-read files sitting in their data
directory after upgrading. Harmless, and not worth writing cleanup code for — the engine has no
asset-directory garbage collector today, and building one solely to delete 150KB of dead cache
files would be exactly the kind of unrequested scope stage 2 should avoid. No action required.
