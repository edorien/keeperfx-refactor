# Stage 13 — Enforce and document

Status: **complete**. See [00-overview.md](00-overview.md) for the full
plan context (now marked historical) and
[`docs/data_structure.md`](../data_structure.md) for the steady-state
"which library owns what" reference this stage produced.

## Goal

Turn the stage 0 dependency-graph script from advisory into a required,
merge-blocking CI check, and make sure the new structure is discoverable
without having read this whole plan.

## Work

1. **Make the dependency-graph check mandatory.** By this stage every file
   has a real `libs/*` home, so the script from
   [stage-00-safety-net.md](stage-00-safety-net.md) no longer needs the
   static classification table — it can walk actual CMake target
   membership. A back-edge (e.g. `kfx_sim` including a `kfx_render`
   header) becomes a build/CI failure, not a code-review nitpick.
2. **Update `docs/build_instructions.txt`** with the new `libs/` layout.
3. **Update `docs/data_structure.md`** (currently covers map/thing/slab
   data structure only) with a pointer to the dependency diagram in
   [00-overview.md §4](00-overview.md#4-target-architecture) and a short
   "which library owns what" summary — new contributors currently have no
   single place to learn this.
4. **Re-measure build times** against the stage 0 baseline (full rebuild,
   and incremental rebuild touching one low-level and one high-level file)
   to confirm the split actually paid off in practice, not just in theory.
5. **Retire this `docs/refactor/` plan's stage documents once executed**,
   or mark them clearly historical — they describe a migration, not the
   steady-state architecture; once the migration is done, the "which
   library owns what" summary in `docs/data_structure.md` (or a dedicated
   `docs/architecture.md`) becomes the source of truth, not this directory.

## Exit criterion

A new contributor can read one doc (`docs/data_structure.md` or a new
`docs/architecture.md`) and know which library owns what and what it's
allowed to depend on. CI rejects layering violations automatically, not
just on request. The stage 0 build-time baseline has a documented
after-number next to it.

## Completion notes

All five items above landed:

1. `scripts/check_layering.py` rewritten to classify by physical
   `libs/<name>/` directory membership (each directory maps 1:1 to a
   CMake `OBJECT` library's source set) instead of a hand-maintained
   stem table, and wired into `.github/workflows/build-prototype.yml` as
   a standalone `--strict` job. The rewrite surfaced 15 real violations
   the old table had silently never checked (`kfx_game_state` and
   `game_lifecycle.c` were never in its stem list) — all fixed. The 4
   confirmed-irreducible residuals (Tier 4 Group D: `console_cmd.c`'s
   direct game-loop call, `net_resync.cpp`'s raw-blob wire format,
   `bflib_enet.cpp`'s ABI-shared `NetSP`) are tracked in the script's
   `ACCEPTED_VIOLATIONS` allowlist so `--strict` still fails on anything
   new.
2. `docs/build_instructions.txt` gained a "Source tree layout (libs/)"
   section.
3. `docs/data_structure.md` gained a "Library architecture" section:
   dependency order, per-library ownership summary, and the
   interface-not-`#include` rule.
4. Build times re-measured — see the "Stage 13.6 re-measurement" section
   in [stage-00-safety-net.md](stage-00-safety-net.md#stage-136-re-measurement).
   `linux.mk`'s numbers are flat-to-slightly-slower (it still has no
   header-dependency tracking, so it can't show the split's benefit); the
   CMake build's header-touch test (172/562 files rebuilt, zero of them
   in a library below the touched one) is what actually demonstrates it.
5. This directory is marked historical in `00-overview.md`'s status line;
   individual stage documents were left as-is (accurate record of the
   migration) rather than edited, since `docs/data_structure.md` is now
   the steady-state reference.
