# Stage 4 — Per-library rollout: `kfx_config`

See [00-overview.md](00-overview.md) for the why, [stage-01](stage-01-framework-and-scaffold.md)
for the harness mechanics, and [stage-02](stage-02-testability-and-fakes.md) §5
for why `kfx_config` is next after `kfx_platform` in the rollout order (one
project dependency, already tested; config-parsing functions are naturally
pure). This document — and the ones that follow it, one per library — is
what stage 2 called "a real scoping conversation, not just execution of a
document." Later libraries get their own `stage-04b-kfx-pathfinding.md`-style
documents as they land, rather than folding everything into this one file.

## What landed: `kfx_config_utest`

`src/kfx_config/tests/CMakeLists.txt` links the whole `kfx_config` `OBJECT`
library plus `kfx_platform` (its one dependency), following stage-01's
"Progress" recommendation to try the linked-library shape directly rather
than assuming it needs a workaround the way `kfx_platform_utest` initially
did:

```cmake
add_executable(kfx_config_utest ${KFX_CONFIG_TEST_SOURCES})
target_link_libraries(kfx_config_utest PRIVATE
    kfx_config
    kfx_platform
    kfx_bfdebug_std
    kfx_common_opts
    centitoml
    kfx_test_main)
```

It worked, with two link-time wrinkles — neither a repeat of
`kfx_platform`'s symbol-blind-spot problem (that's genuinely fixed; the
`check_layering_symbols.py` audit confirms zero new violations after this
stage), both mechanical:

1. **`kfxmain` unresolved.** `kfx_config` depends on `kfx_platform`, which
   physically owns the process `main()` (stage-01 §3's note). The fix is
   `kfx_test_main`, the shared library stage-01 introduced once this
   became clearly a recurring problem, not a `kfx_platform`-only one — see
   that section's updated account.
2. **`toml_parse` unresolved.** `config_translation.c` and `value_util.c`
   call it directly (real config-file parsing, not a layering dodge).
   `centitoml` is an `OBJECT` library (`Dependencies.cmake`) linked into
   `kfx_common_opts` only via `INTERFACE`, and root `CMakeLists.txt`
   already carries a comment explaining why that's not reliable: "Its
   compiled object doesn't reliably make it through two levels of
   INTERFACE-library indirection into the final link" — which is exactly
   why `keeperfx`/`keeperfx_hvlog` link `centitoml` directly rather than
   trusting `kfx_common_opts` alone. `kfx_config_utest` needed the same
   explicit `target_link_libraries(... centitoml ...)`. Worth remembering
   for every later library: **any target linking `kfx_common_opts` that
   calls into `centitoml` (TOML parsing) needs to link `centitoml`
   explicitly** — this isn't `kfx_config`-specific, it'll recur for any
   test binary (or any future non-`keeperfx` executable) that parses TOML.

## What's tested so far

`src/kfx_config/tests/config_test.cpp` — 5 `TEST_CASE`s against
`config.c`'s generic parsing helpers, chosen because they're genuinely
pure (no `kfx_config_state` read, no callback, no file I/O):

- `parameter_is_number` — numeric-string detection (including its
  documented-as-found quirk: a lone `"-"` currently parses as "is a
  number", since the sign-character check only looks at the first
  character and the digit-loop over the rest is a no-op for a
  single-character string; tested as the function's actual current
  behavior, not "fixed" as part of writing a test for it).
- `get_id` — case-insensitive name→id lookup over a `NamedCommand[]`
  array, including its NULL-argument and not-found paths.

This is deliberately small — proving the pattern and the rollout mechanics
for this library, not a coverage sweep. `config.c` alone is ~1,900 lines
with dozens of other functions (the `get_conf_line`/`get_conf_parameter_*`/
`iterate_conf_blocks` family in particular look like equally good
pattern-A-free candidates for a follow-up pass), and the other 23
`config_*.c` files haven't been touched at all yet — most of those need
pattern A (reset `kfx_config_state`, per stage-02 §2) or the fixture-file
approach stage-02 §4 flagged ("config-parsing functions are naturally
unit-testable against small fixture files... a stage 4 concern"), neither
of which this pass needed.

## CI

`.github/workflows/build-prototype.yml`'s `unit-tests` job (stage 3) now
builds both `kfx_platform_utest` and `kfx_config_utest`; `ctest` remains
unscoped (runs both binaries' tests), per stage-03's reasoning for staying
unscoped as more libraries are added.

## Exit criterion

- `kfx_config_utest` builds against the real `kfx_config` `OBJECT` library
  (no per-file workaround, unlike `kfx_platform`'s initial detour) and all
  5 `TEST_CASE`s pass under `ctest --test-dir out/linux -R kfx_config`.
- `check_layering.py --strict` and `check_layering_symbols.py --strict`
  both unaffected (same pre-existing violations/residuals as before this
  stage, zero new ones from `kfx_config_utest`'s addition).
- `kfx_test_main` (introduced here, but living in `kfx_platform/tests/`)
  is confirmed reusable by a second library's test binary, not just
  designed to be — the actual point of extracting it instead of
  duplicating stage-01's forwarding stub.

## What's next

Per stage-02 §5's rollout order: `kfx_pathfinding` (two dependencies, both
now tested; heavily geometric, likely a strong pattern-A candidate once
its `PathfindingWorldCallbacks` fakes are worth writing), then `kfx_sim`
(the big one — expect its own multi-document breakdown, the same way the
library-split plan itself broke `kfx_sim` into a main stage plus three
appendices).
