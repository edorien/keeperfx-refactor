# Stage 2 — Testability patterns and rollout order

See [00-overview.md](00-overview.md) for the why, and
[stage-01](stage-01-framework-and-scaffold.md) for the CMake/Catch2
mechanics this stage assumes already work. Stage 1 proves the harness runs
one trivially pure function. This stage is about everything else: most
functions in this codebase are not pure — they read or write a per-library
`extern` state struct, they call through a callback-struct interface, or
both. "Write a `TEST_CASE`" isn't sufficient guidance on its own; this
document is the pattern reference the per-library rollout (stage 4) is
meant to follow instead of each library re-deriving it independently.

## 1. Why this is a real problem here specifically

Two things about this codebase's shape make naive unit testing harder than
in a codebase built test-first:

- **World state lives in per-library `extern` globals**, not parameters —
  `kfx_sim_state`, `kfx_config_state`, `kfx_render_state`, etc. (see
  [architecture.md §4.3](../../Architecture/architecture.md#43-where-state-lives)).
  A function like `thing_is_invalid` or `get_gameturn` doesn't take a
  `Game*`; it reaches out to a single global. That's a deliberate,
  documented design (raw-blob save/resync serialization depends on it,
  architecture.md §6.2) — not something to refactor away for testability.
  It does mean a test can't construct an isolated `Game` instance and pass
  it in; it has to reset the *actual* global to a known state before each
  `TEST_CASE` and restore or re-zero it after.
- **Cross-layer calls go through callback structs**, not direct calls
  (architecture.md §5). A `kfx_sim` function that needs to tell the
  frontend to flash a message calls through `SimFeedbackCallbacks`, wired
  in production by `main.cpp::setup_game()`. A `kfx_sim_utest` binary never
  links `main.cpp` or `kfx_frontend` — so without registering *something*,
  that function call goes to whatever default no-op implementation each
  callback header ships (architecture.md §5.1: "Each has a matching
  `set_*_callbacks()` and a no-op default implementation in its `.c`").

The second point is actually good news: the callback-struct pattern is
*exactly* a dependency-injection seam, already present throughout the
codebase for architectural reasons unrelated to testing. Production code
registers `main.cpp`'s real implementations; a test registers its own
lightweight fake implementations of just the entries it needs to observe or
control. No new abstraction has to be invented — the refactor already built
the seam this plan needs to use.

## 2. Pattern A — resetting global state around a test

For a function whose only dependency is its own library's state struct
(the common case in `kfx_sim`, `kfx_config`), the test fixture's job is
narrow: put the global into a known state before the test, and leave it
clean afterward so test order never matters (Catch2 runs `TEST_CASE`s in an
unspecified order by default — order-dependent tests are a bug in the test,
not something to work around by pinning order).

```cpp
// src/kfx_sim/tests/thing_data_test.cpp
#include <catch2/catch_test_macros.hpp>
#include "kfx_sim_state.h"
#include "thing_data.h"

namespace {
struct ResetSimState {
    ResetSimState() { std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state)); }
};
}

TEST_CASE_METHOD(ResetSimState, "thing_is_invalid rejects the null thing", "[kfx_sim][thing_data]") {
    REQUIRE(thing_is_invalid(nullptr));
}

TEST_CASE_METHOD(ResetSimState, "thing_is_invalid rejects thing index 0", "[kfx_sim][thing_data]") {
    struct Thing *zero_thing = &kfx_sim_state.things_data[0];
    REQUIRE(thing_is_invalid(zero_thing));
}
```

`ResetSimState`'s constructor runs before every `TEST_CASE_METHOD` using it
(Catch2 instantiates the fixture fresh per test case) — the same
"zero the whole struct" approach `clear_complete_game()` already uses
in production between levels, not a new convention invented for tests.
Where a bare `memset` isn't enough (a state struct with a non-trivial
initialization a raw zero-fill would violate — none identified yet, but
worth checking per struct before assuming `memset` always suffices),
the fixture calls whatever narrow init function the library already
exposes instead of reimplementing one for tests.

## 3. Pattern B — faking a callback-struct dependency

For a function that calls through a `*Callbacks` table, the test registers
its own instance of that table — real code, real `set_*_callbacks()` call,
just with test-double function pointers instead of `main.cpp`'s production
wrappers:

```cpp
// src/kfx_sim/tests/room_util_test.cpp
#include <catch2/catch_test_macros.hpp>
#include "sim_feedback.h"
#include "room_util.h"

namespace {
int g_last_error_stat = -1;
void fake_report_error_stat(int stat_id) { g_last_error_stat = stat_id; }

struct FakeSimFeedback {
    FakeSimFeedback() {
        g_last_error_stat = -1;
        SimFeedbackCallbacks cb{};        // zero-init: every entry defaults
                                           // to nullptr / the header's own
                                           // no-op, per architecture.md §5.1
        cb.report_error_stat = fake_report_error_stat;
        set_sim_feedback_callbacks(&cb);
    }
};
}

TEST_CASE_METHOD(FakeSimFeedback, "claiming an already-claimed room reports the right error stat", "[kfx_sim][room_util]") {
    // ... call the function under test ...
    REQUIRE(g_last_error_stat == SOME_EXPECTED_STAT_ID);
}
```

This only works because every callback struct's own `.c` file already
ships a no-op default (architecture.md §5.1) — a test that only cares about
one entry doesn't have to stub every other one to avoid a null-pointer
call. Where a callback struct doesn't yet have that no-op-by-default
property, that's a latent production bug independent of testing (a table
registered without every entry set would already crash in `main.cpp`
today) and worth flagging separately, not routing around in test code.

## 3a. A discovered wrinkle — raw cross-layer symbol references (resolved)

Stage 1's pilot build surfaced a fourth complication, distinct from
patterns A–C: some `kfx_platform` code called functions or read globals
that were only *defined* in a higher-ranked library, via a raw `extern`
declaration instead of an `#include` — invisible to `check_layering.py`,
which only tracks `#include` edges. Full account, with every instance
found (96 across the whole codebase, not just `kfx_platform`) and how each
was fixed: [`../todo/check-layering-symbol-level-blind-spot.md`](../todo/check-layering-symbol-level-blind-spot.md).

Fixed at the source 2026-08-29 (commit `a5f5c295d`) — each instance was
either relocated to the library that actually implements it, or routed
through the existing callback-struct pattern with a safe no-op default.
`kfx_platform_utest` now links `kfx_platform` as a whole `OBJECT` library
again, no per-symbol workaround needed. The fix also added
`scripts/check_layering_symbols.py`, a post-build audit (`nm` over
compiled objects, not `#include` text) that's the right tool to check
*before* assuming a library is link-self-contained, rather than
discovering it the hard way the way stage 1 first did — run it as
`python3 scripts/check_layering_symbols.py --build-dir out/linux --strict`
against each library as stage 4 gets to it.

## 4. Pattern C — things that stay out of scope for now

Not every function is reasonably unit-testable yet, and stage 2 isn't
proposing to make them so:

- **Direct SDL/platform calls** (`bflib_video`, `bflib_sound`,
  `bflib_inputctrl`, …) — no display, no audio device, no input device in
  CI. These need a fake/headless backend to test meaningfully, which is a
  real chunk of work (`kfx_platform`'s own C++ `PlatformManager`/
  `RendererManager` seam, per architecture.md §2.1, is the natural
  extension point for one, since it already exists to make the backend
  swappable) — deliberately deferred, not attempted in this stage.
- **File-I/O-driven config loading** (`config_*.c` parsing real `.cfg`/
  `.toml` files) — testable today by pointing loaders at small fixture
  files checked into `tests/`, which is a reasonable pattern, but is a
  stage 4 (`kfx_config`) concern once that library's turn comes, not
  something to design abstractly here.
- **`kfx_script` (Lua)** and **`kfx_apploop`** — per
  [00-overview.md §7.3](00-overview.md#7-open-questions-need-a-maintainer-call-before-stage-3), likely need a fuller
  in-process fixture (a LuaJIT state, a minimal fake of "everything below"
  for `kfx_apploop`'s per-frame orchestration) than patterns A/B alone
  provide. Last in the rollout order (§5), not blocking it.

## 5. Rollout order

Bottom-up along the existing dependency ladder, for two compounding
reasons: each library's tests only need patterns A/B against *that*
library's own state/callbacks (nothing higher exists yet to accidentally
reach for), and each successive library's test binary gets to link the
previous ones' real, already-tested code instead of faking it — a
`kfx_sim_utest` calls straight into real `kfx_config`/`kfx_platform` code,
per [00-overview.md §5](00-overview.md#5-target-shape).

| Order | Library | Why here |
|---|---|---|
| 1 | `kfx_platform` — **done** | Depends on nothing project-internal; `bflib_math`/`bflib_planar` in particular are close to pure functions today. Also stage 1's pilot. |
| 2 | `kfx_config` — **done** ([stage-04-kfx-config.md](stage-04-kfx-config.md)) | One project dependency (`kfx_platform`, already tested). Config-parsing functions are naturally unit-testable against small fixture files (§4) — the first pass covered `config.c`'s pure lookup/parsing helpers; fixture-file-backed tests for the actual `.cfg`/`.toml` loaders are a follow-up. First library where pattern B (its own callback-struct *homes*, architecture.md §5.1) becomes relevant, though `kfx_config` is a caller of those, not their only owner. |
| 3 | `kfx_pathfinding` — **done** ([stage-04b-kfx-pathfinding.md](stage-04b-kfx-pathfinding.md)) | Two dependencies, both already tested by this point. Heavily geometric (triangulated nav mesh, wall-hugging) — a good pattern-A candidate once `PathfindingWorldCallbacks` (51-entry, architecture.md §2.2a) fakes are worth writing; possibly split into its own sub-stage given that interface's size. First pass covered `ariadne_points.c`'s self-contained point pool only, using its own existing reset function rather than a memset; nothing needed the callback interface yet. |
| 4 | `kfx_sim` — **first pass done** ([stage-04c-kfx-sim.md](stage-04c-kfx-sim.md)) | The hub — 83 source files, the largest library, both patterns A and B in heavy use. Confirmed the longest stage-4 sub-effort by a wide margin, as predicted; first pass covered one pure `map_utils.c` function only and needs its own per-cluster follow-up (map/thing/creature/room/player) the way the library-split plan itself broke `kfx_sim` into stage 6 + three appendices. Also the first library needing a small accepted-symbol-residual stub (`get_packet`/`creature_table_add`/etc. — real, intentional interface splits with `kfx_net`/`kfx_render`, not the fixed blind-spot problem) rather than linking cleanly on its own. |
| 5 | `kfx_render` — **done** ([stage-04d-kfx-render.md](stage-04d-kfx-render.md)) | Starts touching pattern-C territory (video mode, textures) alongside plenty of pure-ish math (lighting, lens effects). First pass stayed in the pure-ish-math half: `light_data.c`'s simplest accessors, using its own `light_initialise()` reset. Confirmed the "does the newly-linked library resolve a previously-accepted residual for free" theory directly: linking `kfx_render` resolved `creature_table_add[]` without any stub, since `custom_sprites.c` (the real implementation) is physically part of this library. |
| 6 | `kfx_net` — **done** ([stage-04e-kfx-net.md](stage-04e-kfx-net.md)) | Packet (de)serialization and checksum logic is a strong pattern-A fit; the transport/session layer (`net_main`, `net_lan`, socket code) is closer to pattern C. First pass covered `get_thing_checksum` only, property-based rather than asserting a specific checksum value. First library whose accepted residual (`net_resync.cpp`'s raw-blob resync) was already in *both* audits' accepted lists, `#include`-level and symbol-level. |
| 7 | `kfx_game` — **done** ([stage-04f-kfx-game.md](stage-04f-kfx-game.md)) | Orchestration-heavy; `get_gameturn()`'s 329 fan-in (architecture.md §11.2) makes it a high-value, low-risk target — a pure state read, widely relied on. First pass covered `game_legacy_get_gameturn()` only (deliberately thin — it really is a one-line getter). Confirmed stage-04e's prediction that `kfx_net`'s resync stub would need splitting once a consumer resolved part of it for free. |
| 8 | `kfx_frontend` — **done** ([stage-04g-kfx-frontend.md](stage-04g-kfx-frontend.md)) | Almost entirely UI/input — the most pattern-C-heavy library by file count (43 sources). Lowest priority of the "normal" libraries. First pass covered `erstat_inc()` only; genuinely harder to find a pure candidate here than in any earlier library, as predicted. First `*_utest` needing none of the accepted-residual stub libraries at all — linking `kfx_net`+`kfx_game`+`kfx_frontend` together resolves every symbol they exist to fake. |
| 9 | `kfx_script`, `kfx_apploop` | Per §4, deferred to last; needs its own design pass, not just "next in the ladder." |

This order is a recommendation to revisit per-library as stage 4 actually
starts each one — the same way the library-split plan treated stages 6+ as
"a real scoping conversation, not just execution of a document"
([`../00-overview.md` §6](../00-overview.md#6-stage-index)).

## 6. Naming and file organization convention

- One test file per source file under test:
  `src/kfx_platform/src/bflib_math.c` → `src/kfx_platform/tests/bflib_math_test.cpp`.
  Keeps the mapping obvious and keeps individual test files from growing
  into a second `kfx_sim`-sized monolith.
- `TEST_CASE` names are full sentences describing the behavior
  (`"thing_is_invalid rejects the null thing"`), not the function name
  alone (`"thing_is_invalid"`) — a failing-test list should read like a
  spec, matching Catch2's own idiom.
- Tag every `TEST_CASE` with its library and source-file (`"[kfx_sim]
  [thing_data]"`) so `ctest -R kfx_sim` and Catch2's own `[tag]` filtering
  both work at either granularity.
- `SECTION`s within one `TEST_CASE` for closely related variations of one
  behavior (edge cases of the same function), a new `TEST_CASE` for a
  genuinely different behavior — standard Catch2 style, called out here
  only so stage 4 doesn't have nine libraries independently reinventing
  the split.

## 7. Exit criterion

A short (2–3 function) real `kfx_platform` test suite, using pattern A at
least once even though `kfx_platform` has minimal state, landed as the
first non-pilot content — proving the conventions in §2/§3/§6 in practice,
not just on paper — before stage 4 begins the full rollout.
