# Stage 3 — CI integration

See [00-overview.md](00-overview.md) for the why, and
[stage-01](stage-01-framework-and-scaffold.md)/
[stage-02](stage-02-testability-and-fakes.md) for the harness this stage
wires into CI. Landed immediately after stage 1 stabilized (see stage-01's
"Progress" section) rather than waiting for the full library rollout,
following [00-overview.md §6](00-overview.md#6-stage-index)'s own
recommendation: "an unenforced test suite decays silently" — exactly the
failure mode `check_layering.py --strict` was written to close for the
layering rule, and the reason `check_layering.py` itself landed in stage 0
of the library-split plan before any file moved.

## What landed

A new `unit-tests` job in
[`.github/workflows/build-prototype.yml`](../../../.github/workflows/build-prototype.yml),
alongside the existing `check-layering` job:

```yaml
unit-tests:
  name: "Unit Tests"
  runs-on: ubuntu-24.04
  steps:
  - uses: actions/checkout@v4
    with: { fetch-depth: 0 }
  - name: Update system
    run: |
      sudo apt update
      sudo apt install -y build-essential cmake ninja-build pkg-config curl \
        libavcodec-dev libavformat-dev libavutil-dev libopenal-dev \
        libluajit-5.1-dev libminizip-dev libnatpmp-dev libspng-dev \
        libswresample-dev libwayland-dev wayland-protocols libdecor-0-dev \
        libxkbcommon-dev libgl1-mesa-dev libegl1-mesa-dev libdrm-dev \
        libgbm-dev libasound2-dev libpulse-dev libdbus-1-dev libudev-dev \
        libpng-dev libjpeg-dev libogg-dev libvorbis-dev libflac-dev \
        libmpg123-dev libopusfile-dev libminiupnpc-dev libssl-dev \
        libzstd-dev zlib1g-dev
  - name: Configure and build
    run: |
      cmake -S . -B out -G Ninja -DCMAKE_BUILD_TYPE=Debug -DKFX_BUILD_TESTS=ON
      cmake --build out --target kfx_platform_utest kfx_config_utest kfx_pathfinding_utest kfx_sim_utest kfx_render_utest kfx_net_utest kfx_game_utest kfx_frontend_utest -j"$(nproc)"
  - name: Run tests
    run: ctest --test-dir out --output-on-failure
```

Three decisions worth calling out:

1. **The apt package list is copy-pasted verbatim from the existing
   `build-prototype-linux` job**, not reinvented. `kfx_common_opts` (linked
   into every `kfx_*` `OBJECT` library, per `CMakeLists.txt`) always pulls
   in the full dependency chain — openal/luajit/spng/minizip/miniupnpc/
   natpmp — regardless of which single library's test binary is being
   built, so there's no smaller subset that would actually build faster.
   SDL3 itself still isn't in Ubuntu 24.04's apt repos (same comment
   `build-prototype-linux` already carries), so `Dependencies.cmake`
   builds it from source using the wayland/GL/audio/codec `-dev` headers
   this list provides — ffmpeg is *always* built from source regardless
   (`CLAUDE.md`'s Build section explains why). This is exactly the same
   mechanism verified locally in stage 1's "Progress" section, just with
   apt-installed system libraries taking the pkg-config fast path instead
   of `FetchContent`-from-source for the libraries that have one.
2. **Test binaries are built explicitly by name** (`kfx_platform_utest
   kfx_config_utest kfx_pathfinding_utest kfx_sim_utest kfx_render_utest
   kfx_net_utest kfx_game_utest kfx_frontend_utest`, growing further as
   the [stage-04](stage-04-kfx-config.md) rollout continues) rather than a
   blanket `--target all` or a dedicated `kfx_tests` meta-target that
   depends on every `*_utest` binary. Revisit once the explicit list gets
   unwieldy — not yet, at eight entries.
3. **`ctest --test-dir out` runs every registered test**, not just
   `kfx_platform`'s — there's currently only one test binary, so this is
   equivalent to a scoped `-R kfx_platform` today, but deliberately left
   unscoped since running everything is the actual end state, not a
   temporary convenience.

## What this does *not* do

- **No caching.** Every CI run rebuilds SDL3 (and, unconditionally,
  ffmpeg) from source — the same cost paid by `build-prototype-linux`
  already, now paid twice per workflow run. A `FetchContent`/
  `ExternalProject` build-tree cache (keyed on `Dependencies.cmake`'s
  hash, say) would cut this significantly, for this job and
  `build-prototype-linux` both — worth doing, but a general CI-speed
  improvement outside this plan's scope, not a unit-testing concern
  specifically.
- **Not marked as a required status check.** Whether `unit-tests` (like
  `check-layering`) blocks merging is a GitHub branch-protection setting,
  not something expressible in the workflow file itself — needs a
  maintainer with repo admin access to configure, same as presumably
  already true for `check-layering`.
- **No coverage reporting.** Still an open question — see
  [00-overview.md §7.1](00-overview.md#7-open-questions-need-a-maintainer-call-before-stage-3).

## Exit criterion

- `.github/workflows/build-prototype.yml` parses as valid YAML (checked
  with `python3 -c "import yaml; yaml.safe_load(open(...))"` locally,
  since there's no way to trigger an actual Actions run from this
  environment) and mirrors `build-prototype-linux`'s already-CI-proven
  dependency list, rather than an untested new one.
- The exact `cmake`/`ctest` command sequence in the new job is the same
  sequence stage 1 already verified end-to-end locally against `out/linux`
  (see that stage's "Progress" section) — only the build directory name
  differs (`out` vs. `out/linux`), which CMake treats identically.
- Left for a maintainer to confirm once this actually runs on GitHub: the
  job succeeds on a real Actions runner (environment differences —
  runner image package versions, network egress rules — aren't fully
  reproducible locally) and, if desired, gets marked required in branch
  protection.
