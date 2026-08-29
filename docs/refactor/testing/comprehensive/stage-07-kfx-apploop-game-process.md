# Stage 7 — `kfx_apploop`: the per-turn dispatcher and the fixture question

See [00-overview.md](00-overview.md) for context. `kfx_apploop` is one
file — `src/kfx_apploop/src/game_session_loop.cpp`, 1,027 lines
(architecture.md §2.9: "a single file... extracted from `src/main.cpp` in
stage 12.5") — special-ranked like `kfx_script`, allowed to depend on
everything below it. Unlike every prior stage-04* library, this one's
central function, `update()`, genuinely *is* the per-turn integration
point: architecture.md §3 lists its call sequence as roughly twenty
functions across `kfx_net`/`kfx_render`/`kfx_script`/`kfx_sim`/
`kfx_game`/`kfx_frontend`, in order, once per game turn. There is no
smaller "pure function" version of `update()` to extract — dispatching in
the right order *is* what it does.

## 1. What's actually testable without a bigger fixture

Not everything in this file is `update()`-shaped. Skimming
`game_session_loop.cpp`'s other exports:

- **`display_should_be_updated_this_turn()`** (line 226) — **landed as
  the pilot, but reading its actual body first corrected a wrong
  assumption this section originally made.** It's not a pure state read:
  its "not fast-forwarding" branch unconditionally calls
  `find_frame_rate()` before returning, and its fast-forward branch calls
  `packet_load_find_frame_rate(64)` — so it inherits both of those
  functions' `LbTimerClock()` dependency, not just
  `kfx_sim_state`/`kfx_net_state` reads. Caught by reading the function
  body directly rather than trusting the name-based skim that produced
  this section's first draft — the same "verify, don't guess" lesson
  [stage-04b](../stage-04b-kfx-pathfinding.md)/[stage-04c](../stage-04c-kfx-sim.md)
  already logged twice.
- **`LbTimerClock`** (`bflib_datetm.h`) turned out to already be a
  registered function-pointer seam — `extern TbClockMSec
  (*LbTimerClock)(void);`, default-`NULL` until `LbTimerInit()` assigns
  it to `LbTimerClock_chrono` — not a raw platform call. A test fixture
  just has to assign it to a fake before calling anything that reads it
  (`LbTimerInit()` itself is never called in a unit-test binary), which
  answers this section's original "needs checking" note directly: yes,
  swappable, no new production-code seam needed.
- **`find_frame_rate()`** / **`packet_load_find_frame_rate()`** (lines
  189, 205) — frame-rate accounting into `kfx_frontend_state.time_delta`,
  called *by* the pilot test above (with a fake `LbTimerClock` in place,
  so safely) but not tested *directly* in this pass. Both keep `static`
  local accumulator state (`prev_time2`/`cntr_time2`, `start_time`/
  `extra_frames`) that persists across calls within the test process with
  no reset accessor — a real wrinkle pattern A can't solve the usual way
  (there's no struct to `memset`). Testing these directly needs either a
  test that fully controls the call sequence within one `TEST_CASE`
  (accepting the static state's history) or a production-code change
  (parameterizing the state, which the non-goals lean against doing
  lightly) — deferred, not attempted in this pass.
- **`keeper_wait_for_next_turn()`**, **`keeper_screen_swap()`** (lines
  272, and others) — frame-pacing/vsync-adjacent; likely closer to
  pattern C (real timing/rendering side effects) than A. Lower priority
  than the two above; survey once those land.

## 2. `update()` itself — the real design question

`update()` calls, in fixed order: `process_packets()` (`kfx_net`),
`update_local_cameras()` (`kfx_render`), `api_update_server()`
(`kfx_script`), then — if not paused — a long sequence of `kfx_sim`/
`kfx_game`/`kfx_render`/`kfx_frontend` functions ending in
`update_player_sounds()` (`kfx_game`). Meaningfully unit-testing this
function means one of:

- **(a) Assert the dispatch order**, not what each sub-call actually
  does — replace every callee with a call-recording double and check the
  recorded sequence matches architecture.md §3's documented order. This
  is the cheapest option and the most honest about what's actually being
  protected (a silent reordering or dropped call, the class of bug a
  refactor is most likely to introduce here) — but every one of those ~20
  callees would need to already be reachable as an overridable seam
  (function pointer, callback struct, or link-time substitution), and
  none of them are today; they're direct calls. Retrofitting that is a
  production-code change in service of testability, which the original
  plan's non-goals (stage-02 intro, and this directory's own §2)
  deliberately avoid doing lightly.
- **(b) Run a real turn and assert on end state** — closer to what
  `src/ftests/` already does (drives a real level for real turns,
  asserts on outcomes), just without the game-data dependency this
  harness is built to avoid. Genuinely not possible without either real
  level data or a from-scratch synthetic level/save-state fixture
  substantial enough to be its own multi-stage project — likely
  comparable in size to everything stages 1–4 already did, combined.
- **(c) Don't test `update()` itself at all** — test its *reachable
  pieces* individually (which is exactly what stages 1–4's whole
  per-library rollout already does: `update_things()`, `process_rooms()`,
  etc. all live in already-tested-or-testable libraries) and treat
  `update()`'s own orchestration correctness as `src/ftests/`'s job,
  since that's precisely what a real running level is good at verifying
  and a unit fixture is not.

**Recommendation: (c), by default — revisit (a) only if a specific
`update()`-ordering regression actually happens** (a callee dropped or
reordered by a future refactor) and it turns out `src/ftests/` didn't
catch it either. Speculatively building (a)'s call-recording seam now,
against no known past incident, would be exactly the kind of
testability-driven production change this plan's non-goals warn against.
This recommendation is the concrete answer to
[`../stage-02-testability-and-fakes.md` §4](../stage-02-testability-and-fakes.md#4-pattern-c--things-that-stay-out-of-scope-for-now)'s
open "needs its own design pass" note for `kfx_apploop` — not a deferral
of the decision, a decision, subject to a maintainer overriding it.

## 3. `kfx_apploop_utest`'s CMake shape (landed)

Confirmed, not just predicted: the widest `target_link_libraries` list
yet (everything `kfx_script_utest` already links, plus `kfx_apploop`
itself), and — per `check_layering_symbols.py` reporting zero accepted
residuals for `kfx_apploop` — it linked clean on the first attempt with
no stub library at all, the same outcome [stage-04g](../stage-04g-kfx-frontend.md)
found for `kfx_frontend_utest`.

## 4. What actually landed

`src/kfx_apploop/tests/game_session_loop_test.cpp` — 4 `TEST_CASE`s
against `display_should_be_updated_this_turn()`, covering: the paused
short-circuit; the non-fast-forward branch with `frame_skip` disabled;
the non-fast-forward branch's turn-modulo check, asserted on *both* the
true and false side (not just "doesn't crash"); and the fast-forward
branch's turn-low-bits check, likewise both sides. The fixture
(`AppLoopFixture`) combines three things for the first time together in
one test file:

- **Pattern A** on `kfx_sim_state`/`kfx_net_state` (the `memset` reset,
  same as every prior stage).
- **A fake `LbTimerClock`**, needed only to avoid a null-pointer call
  inside `find_frame_rate()`/`packet_load_find_frame_rate()` — the test
  never asserts anything about the value it returns, since neither of
  those two functions' own outputs are under test here (§1).
- **Pattern B** on `kfx_platform`'s `GetGameTurnFunc` provider
  (`set_get_gameturn_provider()`) — the first library in this whole
  plan to register a fake provider from a test rather than relying on the
  default stub, per [stage-08 §1](../stage-08-comprehensive-library-passes.md#1-what-comprehensive-enough-means-absent-a-coverage-number)'s
  "at least one pattern-B test per library" checklist item. Without it,
  `get_gameturn()`'s default-stub return of `0` makes both the
  turn-modulo branches degenerate (`0 % anything == 0`,
  `0 & 0x3F == 0`) — always true, never reachable as false. The fake
  provider is what makes the "returns false" assertions meaningful rather
  than accidentally unreachable.

## 5. Concrete next actions (remaining)

1. `find_frame_rate()`/`packet_load_find_frame_rate()` themselves — the
   `static`-local-state wrinkle noted in §1 needs a decision (accept
   testing them only via a fixed call sequence within one `TEST_CASE`, or
   flag the `static` locals as a production-code change worth making) —
   not resolved here.
2. `keeper_wait_for_next_turn()`/`keeper_screen_swap()` — not yet
   surveyed in the same depth §1 gave `display_should_be_updated_this_turn()`.
3. Leave `update()` itself alone per §2's recommendation unless a
   maintainer decides otherwise.
