# Remove the last symbol-level layering residual: `kfxmain`

Status: **done.**

## Background

`docs/refactor/todo/remove-symbol-level-layering-residuals.md` fixed every
symbol-level residual `check_layering_symbols.py` knew about except one,
`("kfx_platform", "kfxmain", "app_entry")`: `kfx_platform`'s
`PlatformLinux.cpp`/`PlatformWindows.cpp` defined the process's actual
`main()`/`WinMain()`, which called `kfxmain()` — declared in kfx_platform's
own `platform.h` but *defined* in `app_entry`'s `main.cpp`. Both that doc and
the earlier `docs/refactor/todo/check-layering-symbol-level-blind-spot.md`
(which first found it) judged this "by design" and irreducible: the OS has
to call *something* in the platform layer first, so a reverse reference
looked unavoidable.

Prompted by exploring whether the `kfx_*` libraries could become dynamic
(`.so`/`.dll`) instead of static: that surfaced this residual as a real
problem, not just a cosmetic one. Static linking papered over it (everything
lands in one binary, so the linker resolves it regardless of direction). A
real shared library can't cleanly call back into symbols owned by the
executable that loads it without extra plumbing (`-rdynamic` + allowing
undefined symbols on Linux; an executable export table + import lib on
Windows) — so it was worth asking whether the "irreducible" judgment was
actually correct, independent of the static-vs-dynamic question.

It wasn't. The reverse reference existed only because `kfx_platform` owned
the *entry point itself* (`main()`/`WinMain()`), not just platform services.
The entry point's only real job — construct `argc`/`argv`, install a
Windows crash handler, then call `kfxmain()` — is exactly composition-root
work. Only `app_entry` is allowed to depend on every layer, so the fix is to
move the entry point down into `app_entry`, not to keep working around it.

## The fix

- **New `src/kfxmain.h`**: the `kfxmain()` declaration, moved out of
  `kfx_platform`'s `platform.h` (deleted) since only `app_entry` files need
  it now.
- **New `src/native_entry.cpp`**: `main()` (Linux) / `WinMain()` +
  the vectored-exception-handler crash parachute (Windows), both moved
  verbatim out of `PlatformLinux.cpp`/`PlatformWindows.cpp`, branching on
  `#ifdef _WIN32` in one file rather than two — picked up automatically by
  root `CMakeLists.txt`'s existing `file(GLOB "src/*.cpp")` for
  `KFX_SOURCES_REMAINING` (the same glob that already picks up `main.cpp`),
  no CMake changes needed.
- `PlatformLinux.cpp`/`PlatformWindows.cpp` go back to being pure
  platform-service implementations (`GetOSVersion`, `FileFindFirst`,
  `VideoInit`, Steam init/shutdown, …) with no process-bootstrap code at
  all. `platform.h` deleted (nothing left to declare); the stray
  `#include "platform.h"` in `PlatformManager.cpp` (unused there) removed
  too.
- `main.cpp` now includes `kfxmain.h` instead of `platform.h`.

## A bigger payoff than expected: `kfx_test_main` goes away too

Every `kfx_*_utest` transitively links `kfx_platform`, which used to
physically own `main()` — so no `*_utest` could link
`Catch2::Catch2WithMain` (its own `main` would conflict/be silently
dropped). The workaround was `kfx_test_main`
(`src/kfx_platform/tests/kfx_test_main.cpp`), a small shared library every
one of the 10 `tests/CMakeLists.txt` files linked, forwarding `kfxmain()`
into `Catch::Session().run(...)`.

Once `kfx_platform` no longer defines `main()` at all, this whole shim is
unnecessary. Deleted `kfx_test_main.cpp`; all 10 `*_utest` targets now link
plain `Catch2::Catch2WithMain` directly. Simpler in both directions: less
code, and every `tests/CMakeLists.txt`'s explanatory comment about the
workaround is gone too.

## Result

`check_layering_symbols.py`'s `ACCEPTED_SYMBOL_VIOLATIONS` set is now
**empty** — every symbol-level residual `check_layering_symbols.py` was ever
built to track is fixed, not just accepted. Combined with
`check_layering.py --strict` (also zero accepted `#include`-level
violations, since `docs/refactor/todo/remove-remaining-layering-violations.md`),
the dependency ladder is now enforced cleanly at both the `#include` level
and the link-symbol level, with no documented exceptions left at all.

## Verification

- `check_layering.py --strict`: clean.
- `check_layering_symbols.py --strict` (against `out/coverage-ftest`):
  clean, zero accepted residuals reported (down from one).
- Native Linux (`out/coverage-ftest`, `KFX_FUNCTESTING=ON`
  `KFX_TEST_COVERAGE=ON`): `keeperfx`/`keeperfx_hvlog` build and link.
- mingw-w64 Windows cross-compile (`out/windows-test`):
  `keeperfx.exe`/`keeperfx_hvlog.exe` build and link — this is the only
  build that actually compiles `native_entry.cpp`'s `WinMain`/crash-handler
  branch, confirmed clean.
- All 10 `kfx_*_utest` Catch2 suites (`out/coverage`, rebuilt after
  reconfiguring to pick up the new files) pass, linking
  `Catch2::Catch2WithMain` directly.
- Full 16-test `-ftests -headless -exitonfailedtest` sweep passes.
- Real two-process ENet loopback ftest
  (`scripts/run_ftest_net_enet_loopback.sh`) passes.

## Not done as part of this pass

- `docs/Architecture/testing-harness.md` §10's `#include`-level table still
  has some pre-existing stale rows (`net_resync.cpp`/`bflib_enet.cpp` listed
  as "accepted, by design" when both were actually fixed by
  `remove-remaining-layering-violations.md`, and the "4 accepted... down
  from 5" residual count is stale too) — predates this fix, out of scope
  here, flagged separately.
