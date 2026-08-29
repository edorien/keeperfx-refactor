# Stage 1 — Framework acquisition and CMake scaffold

See [00-overview.md](00-overview.md) for the why and the target shape. This
stage answers the mechanical questions: how Catch2 gets into the build, what
the new CMake surface looks like, and what a first, real test binary proves
before any other library follows.

## Goal

Land `KFX_BUILD_TESTS`, a Catch2 v3 dependency, and one working test binary
(`kfx_platform_utest`) that `ctest` can run — nothing else. No attempt yet
to test more than a couple of genuinely pure `kfx_platform` functions; stage
1's job is to prove the mechanics, not to start the rollout (that's stage
2's ordering question, executed library-by-library in stage 4).

## 1. Platform scope

Unit tests build and run on the **native Linux** CMake path
(`KFX_OS=linux ./build-cmake.sh`, or a bare
`cmake -S . -B out/linux -DKFX_BUILD_TESTS=ON && cmake --build out/linux`)
only. Reasons:

- CI has no Windows runner or Wine step today — the mingw cross-compiled
  `keeperfx.exe`/`keeperfx_hvlog.exe` are never *executed* in CI, only
  built. A test binary is only useful if something actually runs it.
- The native-Linux build was already the one used to verify the
  library-split plan itself (`docs/refactor/stage-00-safety-net.md`), for
  the same reason: it builds and links without the mingw-w64 toolchain.
- Nothing about Catch2 itself is Windows-hostile — if a Windows execution
  story is wanted later (a Wine step in CI, say), the CMake wiring below
  doesn't need to change, only `.github/workflows/*.yml` would gain a job.
  Recorded as open question 2 in the overview.

`KFX_BUILD_TESTS` therefore defaults `OFF` unconditionally, and this stage
doesn't attempt to make it work under `build/cmake/toolchains/mingw32.cmake`
— if someone points that toolchain file at `-DKFX_BUILD_TESTS=ON` and it
happens to configure, that's incidental, not a supported path yet.

## 2. Acquiring Catch2

`build/cmake/modules/Dependencies.cmake` already has two acquisition
patterns in active use (see its header comment): Windows uses `kfx_fetch()`
— download-and-extract prebuilt mingw32 static-lib tarballs from
`dkfans/kfx-deps` releases; Linux uses system `pkg-config` first, with a
`FetchContent`-from-source fallback for a few libraries. Neither fits
Catch2 well as-is:

- The Windows prebuilt-tarball path only matters if tests need to run under
  mingw, which §1 just scoped out.
- The Linux pkg-config-first pattern exists because those libraries
  (ffmpeg, SDL3, …) are large, slow to build from source, and commonly
  already packaged system-wide. Catch2 is the opposite: small, fast to
  build, and pinning an exact version matters more than reusing whatever a
  given CI runner or contributor's distro happens to have (a Catch2
  version skew between a contributor's local `ctest` run and CI's is a bad
  failure mode to introduce for the sake of avoiding one small download).

Proposed: a plain `FetchContent` of Catch2 v3 from its GitHub release tag,
gated behind `KFX_BUILD_TESTS`, independent of the existing `kfx_fetch()` /
pkg-config machinery — new, not a variant of either:

```cmake
# CMakeLists.txt, near the top, alongside the other options this file
# already declares (BUILD_NUMBER, PACKAGE_SUFFIX):
option(KFX_BUILD_TESTS "Build Catch2 unit tests for each kfx_* library (native Linux only, see docs/refactor/testing/stage-01-framework-and-scaffold.md)" OFF)

if(KFX_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1   # pin; bump deliberately, not floating
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    enable_testing()
endif()
```

placed after `include(Dependencies)` (so `kfx_common_opts` and every
`kfx_*` `OBJECT` library target already exist for test `CMakeLists.txt`
files to link against) and before the `add_subdirectory(src/kfx_platform)`
block, since that block is what stage 1 extends next.

## 3. Per-library test target

Each library's `tests/` directory gets its own small `CMakeLists.txt`,
added conditionally from the library's existing `CMakeLists.txt` — keeping
"new file → just the correct directory" true for tests the same way it's
true for library sources (`CLAUDE.md`'s Conventions section):

```cmake
# src/kfx_platform/CMakeLists.txt, appended after the existing
# kfx_platform/kfx_platform_hvlog OBJECT library declarations:
if(KFX_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

```cmake
# src/kfx_platform/tests/CMakeLists.txt (as actually landed, post-fix — see
# the note below for the detour this took and why Catch2::Catch2WithMain
# still isn't used):
add_library(kfx_test_main STATIC kfx_test_main.cpp)
target_link_libraries(kfx_test_main PUBLIC Catch2::Catch2)
target_include_directories(kfx_test_main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")

file(GLOB KFX_PLATFORM_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM KFX_PLATFORM_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/kfx_test_main.cpp")

add_executable(kfx_platform_utest ${KFX_PLATFORM_TEST_SOURCES})
target_link_libraries(kfx_platform_utest PRIVATE
    kfx_platform            # the library under test — std variant, see §4
    kfx_bfdebug_std
    kfx_common_opts
    kfx_test_main)

include(CTest)
include(Catch)
catch_discover_tests(kfx_platform_utest TEST_PREFIX "kfx_platform.")
```

`catch_discover_tests()` (from Catch2's own `extras/Catch.cmake`, on
`CMAKE_MODULE_PATH` per §2) runs the built binary at configure time to
enumerate its `TEST_CASE`s and registers each as its own `ctest` entry —
so `ctest -R kfx_platform` lists individual test names, not one
opaque "did the binary exit 0" blob. This is the direct replacement for
what `tst_main.h`'s `TestRegistryWrapper` had to hand-roll for CUnit (see
[00-overview.md §4](00-overview.md#4-framework-choice-catch2-v3)). Note the
`TEST_PREFIX` argument — without it, ctest entry names are the bare Catch2
`TEST_CASE` description with no per-library namespacing at all, and
`ctest -R kfx_platform` matches nothing.

**What actually happened (a detour, since resolved):** the first attempt
at building the real `kfx_platform` target into a test binary — this
section's original two-block `target_link_libraries(... kfx_platform
...)` shape — failed outright. Several `kfx_platform` files called
functions/read globals only defined in `kfx_render`/`kfx_config`/`kfx_sim`
via raw `extern` declarations that `check_layering.py` couldn't see (full
account:
[`../todo/check-layering-symbol-level-blind-spot.md`](../todo/check-layering-symbol-level-blind-spot.md)).
That wasn't something a testing-scaffold stage should paper over with a
large stub file, so `kfx_platform_utest` briefly compiled only the
specific tested source file directly instead of linking the library — and
the underlying problem was flagged as its own separate finding rather than
silently worked around forever.

It's since been fixed at the source (2026-08-29, commit `a5f5c295d`):
every raw cross-layer reference across the whole codebase (96 instances,
not just `kfx_platform`'s) was either relocated to the library that
actually owns it or routed through a callback struct with a safe no-op
default. `kfx_platform_utest` now links the real, whole `kfx_platform`
target again — the code block above is what it currently looks like, not
a stale plan. One thing the fix *didn't* touch, because it's by design and
not a violation: `kfx_platform/src/kfx/platform/PlatformLinux.cpp`
physically owns the process `main()` (the OS-callable-entry-point
pattern), which still collides with `Catch2::Catch2WithMain`'s own `main`.

Since every other `kfx_*` library depends on `kfx_platform` (directly or
transitively), every future `*_utest` target hits this exact same
collision — confirmed when stage 4's `kfx_config_utest` (see
[stage-04-kfx-config.md](stage-04-kfx-config.md)) tried the naive
`Catch2::Catch2WithMain` shape and got a genuinely confusing failure mode:
not a duplicate-`main` link error (the linker never gets that far — object
files, unlike static-library members, are included unconditionally, so
`PlatformLinux.cpp.o`'s `main` wins and `Catch2Main`'s copy is simply never
pulled from its archive), but an *unresolved* `kfxmain` — nothing was left
to call `Catch::Session().run()` at all. One instance of that fix was
enough to justify sharing it rather than duplicating a forwarding stub in
every library's `tests/` directory: `kfx_test_main`, a small `STATIC`
library defined once in `kfx_platform/tests/CMakeLists.txt` (processed
first, so its target name is available to every later `add_subdirectory`)
that every `*_utest` target links instead of `Catch2::Catch2WithMain`.

Every other `kfx_*` library's test target should attempt the "link the
whole `OBJECT` library" shape directly when stage 4 gets to it — the
`check_layering_symbols.py` post-build audit
(`python3 scripts/check_layering_symbols.py --build-dir out/linux --strict`)
now exists specifically to confirm whether a given library is actually
link-self-contained before assuming so.

## 4. Which `BFDEBUG_LEVEL` variant

Every `kfx_*` library builds twice — `kfx_platform` and `kfx_platform_hvlog`
— differing only in `BFDEBUG_LEVEL` (0 vs 10), which per
[architecture.md §12.3](../../Architecture/architecture.md#123-two-executables-differ-only-in-bfdebug_level)
is a log-verbosity knob, not a behavioral one, in every library today. Test
binaries therefore link the `std` (`BFDEBUG_LEVEL=0`) variant only — one
test binary per library, not two. If a future change ever makes some
library's behavior depend on `BFDEBUG_LEVEL` (architecture.md flags this as
something that would need revisiting), that library's tests would need to
follow the same std/hvlog split the library itself gains at that point;
not a concern today.

## 5. Confirming the layering-check exemption

[00-overview.md §5](00-overview.md#5-target-shape) claims `tests/`
directories are already exempt from `scripts/check_layering.py --strict`
for free. Concretely: `find_project_files()` runs `git ls-files -- src`,
then drops any path where `"tests"` appears as a path component before
building the `#include` graph or the stem index used to resolve edges.
Once `src/kfx_platform/tests/*.cpp` exists and is `git add`-ed, this should
be verified directly rather than just cited:

```bash
python3 scripts/check_layering.py --strict
```

run once with a stub test file present, expecting the same output as
before that file existed (i.e. it neither appears in the report nor
changes the violation count either direction). If it *does* show up, the
filter's assumption about `.parts` matching a directory named exactly
`tests` needs re-checking against the actual path
(`src/kfx_platform/tests/foo.cpp` → parts include `"tests"`, so it should
match) before proceeding — worth a two-minute sanity check, not worth
skipping given how load-bearing the assumption is for every later stage.

## 6. What the pilot test binary should contain

Deliberately minimal — enough to prove the scaffold, not the start of real
`kfx_platform` coverage (that's stage 2 §3's job, once the rollout order is
settled):

- One `TEST_CASE` for something genuinely pure in `bflib_math.c` (a
  fixed-point trig or square-root table lookup — no `extern` state, no
  SDL, no file I/O) as the "does the harness actually work" smoke test.
- A second `TEST_CASE` with a deliberately failing `REQUIRE`, run once
  manually to confirm `ctest` reports it as a failure with a useful
  message (line number, expression, actual vs. expected) — then deleted
  before landing. This is the same "prove the red path, not just the
  green one" discipline stage 0 of the library-split plan applied to the
  layering checker itself.

## 7. Local developer workflow

```bash
cmake -S . -B out/linux -DKFX_OS=linux -DKFX_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build out/linux --target kfx_platform_utest -j"$(nproc)"
ctest --test-dir out/linux -R kfx_platform --output-on-failure
```

`build-cmake.sh` itself is not modified in this stage — it's the
"produce a runnable `keeperfx`/`keeperfx_hvlog`" script, and
`-DKFX_BUILD_TESTS=ON` is an orthogonal, opt-in configure flag, not
something that belongs in its default invocation. Whether to add a
`KFX_BUILD_TESTS=1 ./build-cmake.sh` passthrough for convenience (mirroring
how it already reads `KFX_OS`/`BUILD_DIR` from the environment) is a small
follow-up, not blocking for stage 1.

## 8. Exit criterion

- `-DKFX_BUILD_TESTS=ON` configures cleanly on native Linux (system Catch2
  not required — `FetchContent` builds it from source once and caches it
  under the build tree, same lifetime as every other `FetchContent`
  dependency).
- `kfx_platform_utest` builds and `ctest --test-dir out/linux` reports its
  `TEST_CASE`s individually, all green.
- `check_layering.py --strict` is unaffected by the new `tests/` directory
  (§5), confirmed by actually running it, not just cited from reading the
  script.
- `-DKFX_BUILD_TESTS=OFF` (or omitted — it's the default) leaves the
  `keeperfx`/`keeperfx_hvlog` build byte-for-byte unaffected: no new
  targets, no new `FetchContent`, no new required tools. Contributors who
  never touch this flag see no change at all.

## Progress

All four exit-criterion points above have been verified against a real
build, not just planned: `KFX_OS=linux ./build-cmake.sh`-equivalent
configure with `-DKFX_BUILD_TESTS=ON` succeeds (SDL3/openal/spng/minizip/
luajit/miniupnpc/natpmp all correctly fell back to `FetchContent`-from-
source, ffmpeg's `ExternalProject_Add` triggered as expected — see
`CLAUDE.md`'s Build section for why none of that needs any manual `-dev`
package install first); `kfx_platform_utest` builds and
`ctest --test-dir out/linux -R kfx_platform --output-on-failure` reports
all three pilot `TEST_CASE`s individually and green; staging
`src/kfx_platform/tests/` and re-running `check_layering.py --strict`
produced byte-identical output to before that directory existed; and a
plain reconfigure with `KFX_BUILD_TESTS` left at its default `OFF` adds no
Catch2 `FetchContent` and no `*_utest` target.

The "just link the whole `OBJECT` library" shape §3 originally planned
didn't survive first contact with a real build — see that section's "What
actually happened" note and
[`../todo/check-layering-symbol-level-blind-spot.md`](../todo/check-layering-symbol-level-blind-spot.md)
for the full account — but it didn't stay broken either: the underlying
symbol-level layering gap was fixed at the source (2026-08-29,
`a5f5c295d`), and `kfx_platform_utest` now links the real `kfx_platform`
target as originally designed, reverified end-to-end
(`cmake --build out/linux --target kfx_platform_utest` +
`ctest --test-dir out/linux -R kfx_platform --output-on-failure`, all 3
`TEST_CASE`s green) and against `check_layering_symbols.py --strict`
(zero new violations). Later libraries in stage 4's rollout should try the
linked-library shape directly and use that script to confirm it, rather
than assuming either outcome up front.
