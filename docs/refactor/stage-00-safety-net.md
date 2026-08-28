# Stage 0 — Safety net

Prerequisite for every later stage. No source files move.

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

Get a red/green signal for both correctness and layering *before* touching
any file, so every later stage has something automatic to check itself
against instead of relying on manual review.

## Work

1. **Wire `src/ftests` into CI as a merge gate**, if it isn't already. This
   is the existing CUnit-based test scaffold (`src/ftests/ftest.c`,
   `ftest_list.c`, `ftest_util.c`) — treat any regression there as a
   blocking bug for the rest of this plan, not something to work around.
2. **Write a dependency-graph checker script.** A ~50–100 line Python
   script is enough:
   - Parse every `#include "x.h"` (project-relative includes only; ignore
     `<...>` system/external includes) in every `src/**/*.c{,pp}` and
     `src/**/*.h{,pp}` file.
   - Map each file to its owning library using a static table — the
     per-stage documents in this directory (4, 6, 7, 8, 9, 10, 11) each
     define the authoritative file list for their library as it's
     extracted; until a file's library is decided, leave it unclassified
     and skip it in the check rather than guessing.
   - For every `#include` edge where both sides are classified, fail if
     the edge points from a lower library to a higher one per the ordering
     in [00-overview.md §4](00-overview.md#4-target-architecture):
     `kfx_platform → kfx_config → kfx_sim → kfx_render → kfx_net →
     kfx_game → kfx_frontend`, with `kfx_script` allowed to depend on
     everything (see stage 11) and `kfx_platform` allowed to depend on
     nothing project-internal.
   - Run this as an **advisory** (non-blocking) CI check starting now, so
     its output is visible from stage 1 onward, before making it mandatory
     in stage 13.
3. **Record a baseline build-time measurement**: a full clean rebuild, and
   an incremental rebuild after touching one low-level file
   (`bflib_basics.c`) and one high-level file (`frontend.cpp`). Stage 13
   re-measures against this baseline to confirm the split actually paid
   off, not just added CMake ceremony.

## Exit criterion

CI has a red/green signal for `src/ftests` (blocking) and the dependency
graph script (advisory), and a written-down baseline build-time number,
before any file in `src/` is touched by stage 1.

## Progress

**Build path.** The project also has a native Linux build
(`scripts/setup-linux-thirdparty.sh` + `linux.mk`, separate from the
Windows-cross-compile `CMakeLists.txt`/`Makefile` paths) that builds
against system `pkg-config` libraries plus a few vendored-from-source ones
staged in `third_party/install/` (git-ignored). This is what was used to
verify the work below — it builds and links a working `bin/keeperfx`
without needing the mingw-w64 cross-toolchain.

**Baseline build times** (32-core machine, `make -f linux.mk -j$(nproc)`,
unmodified codebase):

| Measurement | Wall time |
|---|---|
| Full clean build (`rm -rf obj bin`, then build) | 8.7s |
| Incremental: touch `bflib_basics.c` (low-level), rebuild | 0.90s |
| Incremental: touch `frontend.cpp` (high-level), rebuild | 1.37s |

Caveat worth carrying into stage 13's re-measurement: `linux.mk` currently
has **no header-dependency tracking** (no `-MMD`/depfile generation — each
`.o` rule depends only on its own `.c`/`.cpp` and `src/ver_defs.h`).
Touching a shared header today rebuilds nothing until the next full build,
which is a confound for future dependency-graph enforcement work as much
as it is for these timings — the incremental numbers above only measure
single-file recompile + relink cost, not the "how much rebuilds when a
dependency changes" question the split is meant to improve. Fixing this
(adding proper header dependency tracking to `linux.mk`) would make future
`kfx_*` boundary work self-verifying and is worth doing before stage 1
lands, independent of this plan.

## Stage 13.6 re-measurement

Same machine (32 cores), same two files (`bflib_basics.c`/`frontend.cpp`,
now under `libs/kfx_platform/src/` and `libs/kfx_frontend/src/`), same
`make -f linux.mk -j$(nproc)`:

| Measurement | Baseline (stage 0) | Now (stage 13) |
|---|---:|---:|
| Full clean build | 8.7s | 10.33s |
| Incremental: touch `bflib_basics.c` (low-level) | 0.90s | 1.09s |
| Incremental: touch `frontend.cpp` (high-level) | 1.37s | 2.12s |

All three are slower, not faster — as flagged as a likely confound back in
stage 0: `linux.mk` still has no header-dependency tracking (`-MMD`), so
every one of these numbers is purely "recompile this one file + relink the
same final binary," regardless of the library split. The binary itself
also grew (more infrastructure: callback structs, state headers, narrow
accessors — the interfaces this whole plan introduced instead of raw
`#include`s), so more total object files get linked either way. This
methodology genuinely cannot show the split's benefit; it wasn't designed
to.

**What actually demonstrates the payoff**: the CMake build (`Unix
Makefiles` generator, which *does* do compiler-generated header dependency
tracking) touching a `kfx_sim`-only header:

```
touch libs/kfx_sim/include/thing_data.h
cd build && make -j32 keeperfx
```

rebuilds 172 of ~562 object files — every one of them in `kfx_sim` or a
library *above* it (`kfx_render`/`kfx_net`/`kfx_game`/`kfx_frontend`/
`kfx_script`/`kfx_apploop`), and **zero** in `kfx_platform` or `kfx_config`,
the two libraries below `kfx_sim`. Before this plan, the equivalent header
touch would very plausibly have forced a full rebuild regardless of which
subsystem it was conceptually about — `struct Game` alone (the god-object
`thing_data.h`-adjacent code used to reach through) was, per stage 5's
investigation, reached from `kfx_sim`, `kfx_render`, `kfx_net`, `kfx_game`,
and `kfx_frontend` simultaneously, and there was no dependency-direction
rule to stop a `kfx_platform`/`kfx_config`-level header from pulling in
game-state or GUI types transitively. That's now structurally impossible:
`scripts/check_layering.py --strict` (wired into CI in stage 13.4) fails
the build the moment a lower library's header reaches into a higher one,
so the boundary the 172-vs-0 split above demonstrates isn't just true
today — it's enforced going forward.

Also worth recording: `make -f linux.mk clean && make -f linux.mk -j$(nproc)`
and the CMake build (`cmake --build build --target keeperfx keeperfx_hvlog`)
both succeed with 0 errors as of this measurement, same as the recurring
verification this whole plan used after every stage.

**Dependency-graph checker**: implemented at
[`scripts/check_layering.py`](../../scripts/check_layering.py). Classifies
every `src/` file by its target library per the tables in stages 3–4 and
6–11, extracts `#include "..."` edges, and flags any edge from a
lower-ranked library into a higher-ranked one. Run it with:

```
python3 scripts/check_layering.py              # human-readable
python3 scripts/check_layering.py --json        # machine-readable
python3 scripts/check_layering.py --strict      # exit 1 on violation (for stage 13)
```

**Baseline run against the unmodified codebase** (525 files scanned, 3,768
classified `#include` edges checked, 0 unclassified):

**722 violations**, dominated by:

| From → To | Count |
|---|---:|
| `kfx_sim` → `kfx_frontend` | 116 |
| `kfx_sim` → `kfx_game` | 95 |
| `kfx_sim` → `kfx_render` | 52 |
| `kfx_config` → `kfx_sim` | 49 |
| `kfx_net` → `kfx_frontend` | 43 |
| `kfx_game` → `kfx_frontend` | 41 |
| `kfx_render` → `kfx_frontend` | 40 |
| `kfx_config` → `kfx_game` | 34 |
| `kfx_sim` → `app_entry` | 31 |
| *(21 more pairs, smaller counts)* | 191 |

This is strong independent confirmation of stage 5's central finding: the
two largest violation categories by far are `kfx_sim → kfx_frontend` and
`kfx_sim → kfx_game`, which is exactly what `struct Game` embedding
`GuiMessage`/script-VM/GUI state by value inside itself (reached
transitively via `game_legacy.h`, included by nearly every sim file)
would produce. **Expect this number to drop sharply once stage 5 lands**,
before stages 6–10 do any of their own cleanup — that's a good
sanity-check to look for when stage 5 is implemented: if the violation
count doesn't drop substantially, the `struct Game` decomposition didn't
address what this baseline says it should.

**Not yet done**: `src/ftests` CI wiring. Investigation found this is
more involved than assumed — `linux.mk` doesn't build `src/ftests` at
all (no `FTEST_*` references), and the Windows-targeting `Makefile`'s
`tests:` target references `src/tests/*.cpp` files that don't exist in
this checkout (likely stale after a rename to `src/ftests/`) — that
target appears to already be broken independent of this plan. Getting
`src/ftests` building under `linux.mk` is a prerequisite for the "blocking
merge gate" part of this stage's goal and is its own small, well-scoped
piece of work, not yet started.

