# Stage 5 — Code coverage tooling

See [00-overview.md](00-overview.md) for why this is worth doing before
stages 6–8 add much more test volume. This stage answers: which tool,
how it's wired into the existing `KFX_BUILD_TESTS` CMake surface, what a
report looks like, and where it fits in CI — deliberately not "what
coverage percentage should we require," which is a separate, later
decision (§5).

## 1. Tool choice

The build toolchain is GCC (`gcc`/`g++`, confirmed throughout
`docs/refactor/testing/stage-01-framework-and-scaffold.md`'s verified
builds — CI's `unit-tests` job installs `build-essential`, not clang), so
`gcov` (GCC's own coverage instrumentation, driven by `--coverage` /
`-fprofile-arcs -ftest-coverage`) is the natural instrumentation layer —
no new compiler needed, and it already ships with the GCC install this
repo's CI already has, the same way it's part of any C/C++ toolchain
install. Two standard front ends turn raw `.gcda`/`.gcno` files into a
readable report; per this repo's own dependency philosophy (`Dependencies.cmake`
fetches/builds its own third-party deps rather than assuming they're
already on the system — see `CLAUDE.md`'s Build section), **neither
should be an `apt install`**:

| Tool | Verdict |
|---|---|
| **gcovr** | A single Python tool, both collects and renders. Rejected for the fetch-not-system-package requirement specifically: it's a pip-distributed package with a real dependency chain (Jinja2 and friends) that isn't a plain source tarball the way `kfx_fetch()` handles every other dependency in this codebase — "fetching" it cleanly would mean either vendoring a `pip install --target` tree (fragile, no precedent in this repo) or accepting a `pip`/system-package install after all, defeating the point. |
| **lcov** + **genhtml**, **2.x** (what `apt install lcov` currently gives on Ubuntu 24.04) | Also rejected. Verified directly: `genhtml` `use`s `DateTime`, `Date::Parse`, and `geninfo` `use`s `Capture::Tiny` — none are core Perl modules, so even a fetched *lcov* source tree would need CPAN-installed (or system-package) Perl modules alongside it, the same "clean fetch" failure as `gcovr`. |
| **lcov** + **genhtml**, **1.16** | **Chosen.** Verified directly (`grep '^use '` across `bin/lcov`/`bin/genhtml`/`bin/geninfo` in the fetched `v1.16` release tarball): every module used — `File::Basename`, `File::Temp`, `File::Copy`, `Getopt::Long`, `Digest::MD5`, `Cwd`, `IO::Uncompress::Gunzip`, `Module::Load(::Conditional)` — is a **core Perl module**, bundled with any Perl 5.14+ install (this environment: 5.40.1), nothing extra needed. `geninfo` optionally prefers `JSON::XS`/`Cpanel::JSON::XS` for speed but falls back to `JSON::PP`, core since 5.14 — confirmed by actually running it (below), not just reading the fallback logic. `lcov`/`genhtml` are plain, uncompiled Perl scripts, so "fetching" them is exactly `kfx_fetch()`'s existing download-and-extract-a-tarball pattern — no build step, no package manager, no version drift between a contributor's local run and CI's. |

**Verified end-to-end in this environment** before committing to this
choice (not just read about): a trivial `--coverage`-instrumented binary,
run once, `lcov --capture` (which auto-detected gcov 15.2.0's JSON
intermediate format and correctly fell back to `JSON::PP`), then
`genhtml` — produced a correct HTML report with no system Perl module
installs beyond the base `perl` binary. The one-version-back pin (1.16,
not the latest 2.x) is a deliberate, checked trade — not a "haven't gotten
around to updating" gap.

## 2. CMake wiring

A new option, layered on top of the existing `KFX_BUILD_TESTS` surface
(`CMakeLists.txt`, `docs/refactor/testing/stage-01-framework-and-scaffold.md`
§2) rather than replacing any of it. Fetching `lcov` reuses
`Dependencies.cmake`'s existing `kfx_fetch(<dir> <url>)` helper directly
— it already does exactly "download a tarball into
`${CMAKE_BINARY_DIR}/deps/<dir>` once, skip if already there" for every
other prebuilt dependency in this codebase (`astronomy`, `centijson`,
`enet6`, `libcurl`), and nothing about it is specific to compiled
libraries — a plain Perl-script tarball extracts the same way:

```cmake
# Only meaningful alongside KFX_BUILD_TESTS=ON; instruments every kfx_*
# OBJECT library AND keeperfx/keeperfx_hvlog in the same build tree (see
# §3 for why that's fine, even though it sounds like scope creep).
option(KFX_TEST_COVERAGE "Instrument kfx_* libraries for gcov coverage (native Linux, requires -DKFX_BUILD_TESTS=ON)" OFF)
if(KFX_BUILD_TESTS AND KFX_TEST_COVERAGE)
    target_compile_options(kfx_common_opts INTERFACE --coverage -O0 -g)
    target_link_options(kfx_common_opts INTERFACE --coverage)

    # lcov 1.16, not the latest 2.x -- see §1 for why (2.x needs non-core
    # Perl modules; 1.16 needs none). kfx_fetch() is defined in
    # Dependencies.cmake, already `include()`'d above -- this is the same
    # helper every other prebuilt dependency in this file uses, just
    # pointed at a Perl-script tarball instead of a compiled static lib.
    kfx_fetch(lcov "https://github.com/linux-test-project/lcov/releases/download/v1.16/lcov-1.16.tar.gz")
    set(KFX_LCOV_BIN "${CMAKE_BINARY_DIR}/deps/lcov/lcov-1.16/bin/lcov")
    set(KFX_GENHTML_BIN "${CMAKE_BINARY_DIR}/deps/lcov/lcov-1.16/bin/genhtml")

    # Runs the whole capture+filter+render pipeline (§4) as one target,
    # so `cmake --build out/coverage --target coverage` after `ctest` is
    # the entire local workflow. Depends on every *_utest target so
    # `--target coverage` alone (no separate build step first) is enough.
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E env perl "${KFX_LCOV_BIN}"
            --capture --directory "${CMAKE_BINARY_DIR}"
            --output-file "${CMAKE_BINARY_DIR}/coverage.raw.info"
            --gcov-tool gcov --quiet
        COMMAND perl "${KFX_LCOV_BIN}"
            --extract "${CMAKE_BINARY_DIR}/coverage.raw.info"
            "${CMAKE_SOURCE_DIR}/src/kfx_*/src/*"
            --output-file "${CMAKE_BINARY_DIR}/coverage.info" --quiet
        COMMAND perl "${KFX_GENHTML_BIN}"
            "${CMAKE_BINARY_DIR}/coverage.info"
            --output-directory "${CMAKE_BINARY_DIR}/coverage-html" --quiet
        DEPENDS
            kfx_platform_utest kfx_config_utest kfx_pathfinding_utest
            kfx_sim_utest kfx_render_utest kfx_net_utest kfx_game_utest
            kfx_frontend_utest
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Capturing coverage and rendering HTML report (out/coverage/coverage-html/index.html)")
endif()
```

placed alongside the existing `KFX_BUILD_TESTS`/Catch2 `FetchContent`
block, after `kfx_common_opts` is defined and after the last `*_utest`
target exists (i.e. after every `add_subdirectory(src/kfx_*)` call, since
the `coverage` target's `DEPENDS` list names them). `-O0` is deliberate,
not incidental: `gcov` line attribution is unreliable under optimization
(GCC can merge, reorder, or eliminate lines the source still "contains"),
and a coverage run's job is an accurate report, not runtime speed — a
`-DCMAKE_BUILD_TYPE=Debug` configure (already what stage-01's verified
workflow uses) plus this override is the right combination. The `--extract
".../src/kfx_*/src/*"` pattern (lcov's own scoping mechanism, the
`lcov`-side equivalent of `gcovr`'s rejected `--filter`) is what keeps
`_deps/` (SDL3/Catch2/ffmpeg/openal/luajit/etc., all built from source in
the same tree per `Dependencies.cmake`) and the `*_utest`/stub sources
themselves out of the report — see §4 for confirming this actually works,
not just reads plausibly.

## 3. Why instrumenting `kfx_common_opts` (and therefore `keeperfx` too) is fine

`kfx_common_opts` is the one `INTERFACE` target every `kfx_*` `OBJECT`
library, every `*_utest` binary, *and* `keeperfx`/`keeperfx_hvlog`
themselves all link (`CMakeLists.txt`) — so turning on `--coverage` there
instruments the whole build tree uniformly, including the real game
executables, not just test binaries. That sounds like scope creep, but it
isn't in practice: `KFX_TEST_COVERAGE` is opt-in and off by default, so a
normal `./build-cmake.sh` run is completely unaffected. Anyone who does
turn it on is, by construction, building a dedicated coverage tree (`out/
coverage/`, say) to run `ctest` against and then discard — not a
release build — so `keeperfx`/`keeperfx_hvlog` also being instrumented in
that one tree is harmless, and avoiding it would need per-`OBJECT`-library
conditional flags that don't actually buy anything (nothing links a
`kfx_*` `OBJECT` library's *coverage-instrumented* compilation into a
*non-instrumented* executable within the same build tree — CMake compiles
each `OBJECT` library's sources once per tree, shared by every consumer in
that tree, the same fact stage-01/04 already leaned on for why the
`*_utest` targets get real production code for free).

## 4. Producing a report

```bash
cmake -S . -B out/coverage -G Ninja -DKFX_OS=linux -DCMAKE_BUILD_TYPE=Debug \
  -DKFX_BUILD_TESTS=ON -DKFX_TEST_COVERAGE=ON
cmake --build out/coverage --target \
  kfx_platform_utest kfx_config_utest kfx_pathfinding_utest kfx_sim_utest \
  kfx_render_utest kfx_net_utest kfx_game_utest kfx_frontend_utest \
  -j"$(nproc)"
ctest --test-dir out/coverage --output-on-failure
cmake --build out/coverage --target coverage
# open out/coverage/coverage-html/index.html
```

The `coverage` custom target (§2) runs the capture → extract → render
pipeline in one step, so producing a report locally is exactly the same
`cmake --build`/`ctest` workflow stage 1–4 already established, plus one
more `--target coverage` at the end — not a second, parallel tool
invocation a contributor has to learn separately.

- `lcov --extract ... "src/kfx_*/src/*"` (§2) scopes the report to
  production `kfx_*` source, excluding `_deps/` (fetched third-party
  sources compiled in-tree), `src/main.cpp`, `src/ftests/` (functional-test
  scaffolding, not unit-tested code), and every `*_utest`/`tests/`
  directory (test code and stub sources — coverage *of* test code isn't
  the interesting number, only coverage *of* the production libraries the
  tests exercise) — none of those paths match the extract pattern.
- A separate build tree (`out/coverage`, not `out/linux`) is deliberate:
  the `--coverage`-instrumented objects are slower and behave differently
  under a debugger than the normal `out/linux` tree stage 1–4 already use
  for day-to-day test iteration; keeping them apart avoids surprising a
  contributor who just wants a fast `ctest` loop.

## 5. What this stage deliberately does not decide

- **No coverage floor / CI gate.** With eight libraries at a handful of
  pilot tests each (stage-04*), any percentage number right now would be
  dominated by how much of each library's code the current pilot happens
  to *not* exercise (i.e., almost all of it) — a floor set today would
  either be trivially low (meaningless) or immediately failing (blocking
  merges for reasons unrelated to what a contributor is actually
  touching). Revisit once stage-08's per-library follow-up passes have
  landed for at least a few libraries and there's a real baseline number
  to reason about, not a first-run number.
- **No CI job wiring decided yet.** Whether coverage generation joins the
  existing `unit-tests` job (stage-03) as an extra step, or gets its own
  job/workflow (given the separate build tree in §4, and that a coverage
  build is slower than the plain one), and whether/how a report gets
  surfaced on a PR (a build artifact upload, same pattern as
  `build-prototype`'s `.7z` upload; a PR comment; a badge) are all open —
  a maintainer call once §1–4 are actually implemented and there's a real
  report to look at, not before.
- **No third-party coverage service** (Codecov, Coveralls, …) evaluated.
  Would need an account/token a maintainer would have to set up; out of
  scope for a planning document to decide unilaterally.

## Exit criterion

- `-DKFX_TEST_COVERAGE=ON` alongside `-DKFX_BUILD_TESTS=ON` configures
  and builds cleanly in a dedicated `out/coverage` tree, verified against
  a real build the same way every prior stage was (stage-01's "Progress"
  section is the template: don't just write the CMake, run it).
- `-DKFX_TEST_COVERAGE=OFF` (or omitted, the default) leaves `out/linux`
  byte-for-byte unaffected, the same non-negotiable stage-01 already
  established for `KFX_BUILD_TESTS` itself.
- `lcov`'s fetch requires no `apt install`/`pip install`/CPAN step beyond
  the base `perl` binary — confirmed by the CMake configure succeeding
  and `cmake --build --target coverage` running to completion in an
  environment with no `lcov`/`gcovr`/extra Perl modules pre-installed.
- The rendered report correctly excludes `_deps/`, `src/ftests/`, and
  every `*_utest`/stub source, and correctly attributes coverage to the
  real `src/kfx_*/src/*.c(pp)` files the pilot tests already exercise (a
  sanity check: the handful of functions stage-04* tests already cover —
  `LbSqrL`, `parameter_is_number`, `small_around_index_in_direction`, …
  — should show as covered; everything else in those files should not).

## Progress

All of the above implemented and verified against a real build, not just
planned: `-DKFX_BUILD_TESTS=ON -DKFX_TEST_COVERAGE=ON` configured a fresh
`out/coverage` tree, fetched `lcov` 1.16 via `kfx_fetch()` (confirmed:
regular `[download NN% complete]` progress messages during configure, no
`apt`/`pip`/CPAN step involved), built all eight `*_utest` binaries with
`--coverage` instrumentation, and `cmake --build out/coverage --target
coverage` ran the full capture → extract → render pipeline to completion.

Confirmed correct, not just "ran without error":

- `grep '^SF:' coverage.info` lists only real `src/kfx_*/src/*.c(pp)`
  production files — no `_deps/` (SDL3/Catch2/ffmpeg/openal/luajit/…),
  no `centitoml`, no `*_utest`/`tests/`/stub source anywhere in the list.
  The `lcov --extract ".../src/kfx_*/src/*"` pattern (§2) does exactly
  what it was designed to do.
- Per-function attribution is exact: `bflib_math.c`'s `FNDA` (function
  call counts) show `LbSqrL` called 5 times and `LbLerp` called 3 times —
  matching `bflib_math_test.cpp`'s exact assertion counts (5 `CHECK`s
  against `LbSqrL` across its two `TEST_CASE`s, 3 against `LbLerp`) —
  while `LbSinL`/`LbCosL`, declared in the same file but not called by
  any current test, correctly show 0.
- **Baseline overall**: 0.2% line coverage (271/119,761), 0.5% function
  coverage (38/7,100) across the eight instrumented libraries — a real,
  now-measured number confirming the "28 tests across ~430 combined
  source files is a scaffold, not a safety net" framing from
  [`00-overview.md` §1](00-overview.md#1-why-now-not-earlier) wasn't just
  a plausible-sounding claim. This is the number stage-08's per-library
  follow-up passes should move, and the honest starting point for
  whenever a coverage floor (§5) becomes worth setting.
- **Re-measured after stage 8's per-library passes** (262 tests across
  all ten `kfx_*` libraries, `kfx_pathfinding`/`kfx_sim` now instrumented
  alongside the original eight): regenerated with the same `out/coverage`
  pipeline (`find out/coverage -name "*.gcda" -delete` first — rebuilding
  after source changes hits the same stale-checksum `libgcov` warning
  noted in the Errors/Fixes history, harmless but needs clearing), **1.4%
  line coverage (1,723/124,517), 3.1% function coverage (232/7,447)** —
  a real 7x/6x jump off the 0.2%/0.5% baseline, confirmed via `lcov
  --summary` on the fresh `coverage.info`, not just the HTML report's
  headline numbers.
- **Re-measured again** after the layering-residual-focused pass (302
  tests): **1.7% line coverage (2,164/124,517), 3.8% function coverage
  (281/7,447)**.
