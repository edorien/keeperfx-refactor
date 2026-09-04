# Stage 2 — legacy bugs found during the pixel-format migration

Status: **living document, started 2026-09-03.** Not a plan — a log. The 8-bit→32-bit rewrite
touches every pixel-write site in the software rasterizer line by line, which surfaces
pre-existing bugs in the original (pre-migration) code that nobody was looking for and automated
tests don't cover. Record each one here when found: what the old code actually did, what it should
have done, and the decision on whether to fix it as part of this migration or leave it flagged.
Cross-reference from [02a-pixel-format-design.md](02a-pixel-format-design.md) and the code comment
at the fix site, so all three points (design doc, source, this log) stay consistent.

---

## 1. `LbSpriteDrawOneColourUsingScalingDownDataTrans2RL` silently discards its `colour` parameter

**File**: `src/kfx_platform/src/renderer/software/bflib_vidraw_spr_onec.c`
**Found**: 2026-09-03, while rewriting the file's 8 `transmap`-taking functions for the RGBA
migration (see [02a-pixel-format-design.md](02a-pixel-format-design.md) §3.2's note on the `onec`
family). **Decision: fix as part of this migration**, flagged with a comment at the fix site and
recorded here — not silently perpetuated, not deferred to a separate change (per explicit
direction: the RGBA rewrite touches this exact line anyway, so fixing it here is lower-cost than a
separate follow-up would be).

### What the old code did

Every other `transmap`-taking function in this file packs a 16-bit lookup index into `render_ghost`
(`ghost[256*256]`, `output = table[colour_byte<<8 | dest_byte]` or the reverse order, depending on
whether the function is a "Trans1" or "Trans2" variant — see
[02a-pixel-format-design.md](02a-pixel-format-design.md) §2.2/§3.2 for the two formulas this
corresponds to). Seven of the eight do this correctly. The eighth,
`LbSpriteDrawOneColourUsingScalingDownDataTrans2RL`, did not:

```c
// As it was (bflib_vidraw_spr_onec.c, DownDataTrans2RL, ~line 955):
unsigned int pxmap;
pxmap = ((colour) << 8);                        // colour placed in the upper byte...
{
    pxmap = (pxmap & ~0xff00) | ((*out_end) << 8);  // ...then the upper byte is immediately
    *out_end = transmap[pxmap];                     // overwritten with (*out_end)<<8, discarding
    out_end--;                                       // `colour` entirely -- the lower byte,
}                                                     // never set, stays 0
```

`pxmap & ~0xff00` clears bits 8-15 of a value that only *had* bits 8-15 set (`colour << 8` with
nothing in the low byte) — so after the mask, `pxmap` is `0`, and the following `| ((*out_end) <<
8)` only ever sets the upper byte to the destination pixel. **The result: this function always
looked up `ghost[dest_byte << 8 | 0]`** — a blend against palette index 0, not against the actual
`colour` parameter the caller passed in — regardless of what `colour` was.

**Confirmed not a systematic pattern, not intentional**: its `LR` counterpart, three functions
later in the same file (`LbSpriteDrawOneColourUsingScalingDownDataTrans2LR`), computes the
identical logical operation correctly:

```c
// DownDataTrans2LR, ~line 1046 — correct:
unsigned int pxmap;
pxmap = (colour);                                // colour placed in the LOWER byte
{
    pxmap = (pxmap & ~0xff00) | ((*out_end) << 8);  // only the upper byte gets overwritten;
    *out_end = transmap[pxmap];                     // `colour` in the lower byte survives
    out_end++;
}
```

The `RL` variant's first line should have been `pxmap = (colour);` (matching `LR`), not
`pxmap = ((colour) << 8);` — a one-character-class copy-paste slip (an errant `<< 8`) when the two
mirrored functions were originally written, most likely, given every other RL/LR pair in the file
is otherwise symmetric.

### Real-world effect

This is the scaled-**down**, right-to-left, "Trans2" (reverse-weighted ghost blend) variant of the
one-colour sprite draw path — used for a creature/object silhouette rendered smaller than its
source sprite (far from camera or similar), in the specific visual mode that blends the
silhouette's `colour` against the background with the "Trans2" weighting (2/3 colour, 1/3
background — see §2.2's formula). With the bug, every pixel of every such draw always resolved
`colour` as whatever `render_ghost`'s row-0 entry happens to be (a fixed value, unrelated to the
actual creature/effect colour intended) instead of the intended tint colour. Depending on what
`ghost[dest<<8 | 0]` actually renders as (likely a very dark or black-ish tone, since palette
index 0 is conventionally near-black in this engine's palettes), the practical symptom is probably
"this specific silhouette variant renders darker/duller than intended, not visibly tinted by its
actual colour parameter" — subtle enough to have gone unnoticed, especially since it only affects
one of eight closely-related draw paths and only at small/downscaled sizes.

### Fix applied

`DownDataTrans2RL`'s first line changed to `pxmap = (colour);`, matching `DownDataTrans2LR`, before
being further rewritten (along with all 8 functions) to the RGBA `TbPixel_RGB((2*colour.c +
dest.c)/3, ...)` formula per §2.2. See the code comment at the fix site
(`bflib_vidraw_spr_onec.c`, function `LbSpriteDrawOneColourUsingScalingDownDataTrans2RL`) for the
inline flag.

---

## 2. `void_black` is never populated by `compute_alpha_tables()` — corrects an earlier "confirmed dead" finding

**File**: `src/kfx_render/src/vidfade.c` (`compute_alpha_tables`, ~line 152-177);
consumed via `render_alpha` in `src/kfx_platform/src/renderer/software/bflib_vidraw_spr_norm.c`.
**Found**: 2026-09-03, while implementing the RGBA replacement for `render_alpha`'s
`transmap[texel<<8|dest]` lookup and tracing exactly what each row of the flat 256-row
`struct TbAlphaTables` (`vidmode.h:123-135`) actually contains. **Decision: preserved as-is**
(reproduce the observed behaviour — texel 0 and 65-255 resolve to a flat, always-index-0 output —
since fixing it would require guessing what `void_black` was *supposed* to contain, and no sprite
asset data is available to tell whether real gameplay ever reaches those texel values).

### What was previously stated, and why it was incomplete

[02a-pixel-format-design.md](02a-pixel-format-design.md) §2.3 originally reported `void_black` as
"confirmed dead" alongside `flat_colours_tl/tr/br/bl`, based on a whole-codebase grep finding no
explicit `void_black` symbol reference outside its own declaration. That grep was correct as far
as it went — but it missed that `render_alpha` (`bflib_vidraw_spr_norm.c`'s `Trans1RL`/`Trans1LR`,
both Up and Down variants) accesses `void_black` **implicitly**, by raw byte offset: `struct
TbAlphaTables` is cast to a flat `unsigned char*` and indexed as `render_alpha[texel<<8 | dest]`,
where `void_black[256]` occupies rows byte-offset 0-255 — i.e. `texel == 0` selects it — with no
symbol name appearing anywhere near that call site. A pure symbol-reference grep cannot catch an
access pattern like this; it required tracing the actual runtime indexing arithmetic against the
struct's field layout to find.

### What actually happens

`compute_alpha_tables()`'s only reference to anything resembling `void_black` is this loop:

```c
// vidfade.c, compute_alpha_tables() — as it is:
{
    for (int n = 0; n < 256; n++) {
        alphtbls->black[n] = 144;   // <-- targets `black`, not `void_black`
    }
}
compute_alpha_table(alphtbls->white,  spal, dpal, 4, 4, 4);
...
compute_alpha_table(alphtbls->black,  spal, dpal, -2, -2, -2);  // overwrites the loop above anyway
...
```

The flat-144 initialization loop writes into `black[0..255]` (only the first of `black`'s 8 rows),
not `void_black` — almost certainly a copy-paste slip when this function was originally written,
given `void_black` and `black` are adjacent, similarly-named fields. Whatever the original intent,
`void_black` is **never written by any `compute_*` call**, so it stays at its BSS-zero-init value
in every freshly-generated table, and — since `compute_alpha_tables()` only runs at all when the
`alpha.col` cache is missing or wrong-sized, then immediately caches its output via `LbFileSaveAt`
— the shipped/cached `alpha.col` perpetuates that same all-zero content indefinitely once
generated once. `unused[191*256]` (rows 65-255) is in the same position: no `compute_*` call
targets it either, so it's equally always-zero.

### Real-world effect

Any sprite drawn via `EngineSpriteDrawUsingAlpha` (the alpha-blend keeper-sprite path,
`engine_render.c:7892`, used for water/lava/shimmer-style effects) whose pixel data contains a
literal palette-index byte of `0` or anything in `65..255` resolves through `void_black`/`unused`
— always byte value `0` (palette index 0) regardless of what colour that pixel "should" be. In
practice this is likely rarely or never hit: RLE-encoded sprite data conventionally reserves the
value `0` for "end of run" at the run-length level (see the `pxlen == 0` break condition throughout
every sprite blit primitive in this codebase), so a literal `0` texel *within* a drawn run — and
therefore reaching this table lookup at all — may not occur in well-formed sprite assets. Texel
values 65-255 are more plausible to hit if any alpha-drawn sprite's palette range extends past 64.
Without the actual (proprietary, not available in this repo) sprite asset data, it isn't possible
to confirm whether this ever visibly affects real gameplay.

### Fix applied

None — behaviour preserved exactly. `render_alpha_blend()` (`bflib_render.h`) returns a flat
`TbPixel_RGBA(0, 0, 0, dest.a)` for `texel < 1 || texel > 64`, matching the always-zero-byte
result the original table lookup produced for the same inputs.

---

## 3. `LbSpriteDrawRemapUsingScalingDownDataTrans2RL` silently discards its remapped colour

**File**: `src/kfx_platform/src/renderer/software/bflib_vidraw_spr_remp.c`
**Found**: 2026-09-03, while rewriting the file's `cmap`-taking Trans1/Trans2 functions for the
RGBA migration. **Decision: fix as part of this migration**, flagged with a comment at the fix site
and recorded here — same policy as bug #1, and in fact the exact same bug shape, in the sibling
"remap" family instead of the "one colour" family.

### What the old code did

Every other `cmap`-taking Trans2 function in this file (`UpDataTrans2RL`, `UpDataTrans2LR`,
`DownDataTrans2LR`) packs its 16-bit `render_ghost` lookup index as `pxmap = cmap[*sprdata];` (no
shift — the remapped colour goes in the *low* byte, since the final combine puts the destination
pixel in the high byte: `pxmap = (pxmap & ~0xff00) | (dest << 8)`). `DownDataTrans2RL` alone had an
extra `<< 8` on the first line:

```c
// As it was (bflib_vidraw_spr_remp.c, DownDataTrans2RL, ~line 938):
unsigned int pxmap;
pxmap = ((cmap[*sprdata]) << 8);                    // colour placed in the upper byte...
{
    pxmap = (pxmap & ~0xff00) | ((*out_end) << 8);  // ...then the upper byte is immediately
    *out_end = transmap[pxmap];                     // overwritten with dest<<8, discarding
    out_end--;                                       // the remapped colour entirely -- the
}                                                     // lower byte, never set, stays 0
```

Identical shape to bug #1 (`bflib_vidraw_spr_onec.c`'s `DownDataTrans2RL`): `pxmap & ~0xff00`
clears the only bits that were ever set, so `pxmap` collapses to `dest << 8`, and the lookup
becomes `ghost[dest_byte << 8 | 0]` — a blend against palette index 0, never against
`cmap[*sprdata]`.

### Real-world effect

Same code path shape as bug #1, one level up: this is the scaled-down, right-to-left,
`Lb_SPRITE_TRANSPAR8` remap-colour draw (`LbSpriteDrawRemapUsingScalingData`, used for
player-colour-recoloured creature sprites — e.g. the `Lb_SPRITE_REMAP` path is a separate solid
draw, but the `TRANSPAR8` ghost-blended ones share this same table). Affected draws would render
darkened-toward-palette-index-0 instead of showing the player's remapped colour, for this one of
four RL/LR × Up/Down Trans2 variants, only when scaled down.

### Fix applied

Not literally reproduced when rewriting to RGBA (the RGBA rewrite derives each function's formula
from its structural position — RL/LR, Trans1/Trans2, Up/Down — against the already-established
§2.2 ghost formulas, not by re-deriving the old bit-packing per function), so this one was fixed
implicitly by that derivation and then flagged explicitly here and at the fix site once noticed.
See the code comment at `bflib_vidraw_spr_remp.c`, function
`LbSpriteDrawRemapUsingScalingDownDataTrans2RL`.

---

## 4. Minimap panel colours built once per process, not once per level — reported via live testing

**Found**: 2026-09-03, reported by the user after the Stage 2 in-game verification pass (once the
`lbDrawSurface` format flip and the bugs it exposed — dotted sprites, black selection wireframe,
minimap overflow corruption — were all fixed): a level's minimap looks correct on first load, but
loading a *different* level after returning to the main menu shows the same visual corruption again.
**Decision: fix, not merely flag** — pre-existing control flow (not introduced this migration), but
its consequences only became visually obvious once minimap colours started being derived from an
actual captured screen pixel + the active palette (this stage's work) rather than a raw index byte
that was "wrong but still some valid colour" either way.

### What the old code did

`frontmenu_ingame_map.c`'s `auto_gen_tables()` only rebuilds the minimap's background-colour capture
(`setup_background()`) and derived colour table (`setup_panel_colors()`) when `PrevPixelSize` (a
function-static, tracking UI scale) differs from the current call's `units_per_px`. Once a level has
been played, `PrevPixelSize` is set and — for the ordinary case of one screen resolution/zoom per
session — never differs again, so this lazy rebuild silently never re-runs for any subsequent level.

Separately, `main_game.c`'s level-(re)start sequence calls `config_reload_callbacks->
setup_panel_colors()` unconditionally, early — before `init_map_size()`/`load_map_file()`, i.e.
before this level has rendered a single frame of its own UI. On the *first* level of a process this
is harmless (`MapBackColours[]`/`NumBackColours` are still at their zero-initialized defaults, so
the loop body never executes; the real, correct build happens later via the lazy path once the game
is actually rendering, with the correct active palette). On the *second and later* levels, though,
`MapBackColours[]`/`NumBackColours` are no longer empty — they're frozen from the *first* level's
lazy rebuild — so this early call now actually runs its full body, computing `PanelColours[]` from
that stale background-colour data using whatever palette happens to be active at this early,
pre-render point (not this level's real one). Because the lazy path's own gate is already frozen,
that lazy path never runs again to correct it, leaving the early call's result as the permanent
(wrong) minimap for the rest of the level.

### Real-world effect

Under the old palette-indexed renderer, "wrong palette at an early loading point" still produced
*some* valid-looking (if slightly off) colour, since every byte value maps to a real palette entry
regardless of which palette's currently loaded — easy to miss. Under true-colour rendering, the same
control flow can feed `resolve_indexed_pixel()` a genuinely mismatched palette snapshot, which reads
as visibly "corrupted" rather than just "slightly wrong" — this is what made the pre-existing gap
suddenly visible and worth fixing now rather than carrying forward silently.

### Fix applied

Added a new `ConfigReloadCallbacks` field, `reset_panel_map_background_cache` (implemented in
`frontmenu_ingame_map.c` as `reset_panel_map_background_cache()`, setting `PrevPixelSize = -1`, an
impossible value for `256 * units_per_px / 16` to ever equal), called from `main_game.c` right after
the existing early `setup_panel_colors()` call. This doesn't remove that early call — its purpose
beyond "harmless first-level no-op" wasn't investigated further — but ensures `auto_gen_tables()`'s
lazy path unconditionally rebuilds (with the level's real, current palette and a real captured
background) the next time the minimap is actually drawn in-game, on every level, not just the first.

---

*(Add further entries above this line as more are found during the remaining files.)*
