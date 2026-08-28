# Stage 1 — In-place `OBJECT` library boundaries

No source files move. This stage exists purely to turn "does library X
depend on library Y" into a question the build system can answer, instead
of something that has to be read out of 266 files by eye.

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

Convert the single `file(GLOB_RECURSE src/*.c src/*.cpp)` in
[`CMakeLists.txt`](../../CMakeLists.txt) into roughly ten
`add_library(kfx_x OBJECT ...)` targets, grouped per the table in
[00-overview.md §4](00-overview.md#4-target-architecture), using each
file's *current* location. Link them all into `keeperfx` and
`keeperfx_hvlog` exactly as today — the output binaries should be
unaffected.

## Work

- Add one `add_library(kfx_platform OBJECT ...)` (etc.) target per row of
  the target-architecture table, listing files by their *current* `src/`
  path (no `git mv` yet).
- At this stage, a file whose final library assignment depends on later
  stages (notably anything touched by stage 5's `struct Game` split) can
  be provisionally assigned to its best-guess destination — it'll move
  again once stage 5 lands, and that's fine; the point of this stage is
  the harness, not a final answer.
- Keep `target_include_directories` pointing at the shared `src/` tree for
  now (real per-library include directories come with the physical moves
  in stages 3+).
- Link all `kfx_*` OBJECT libraries into both `keeperfx` and
  `keeperfx_hvlog` targets, unchanged from today's behavior.

## Exit criterion

- Build output is functionally unchanged (binaries behave identically;
  bit-for-bit identity is not required since object file ordering may
  shift).
- ~10 `OBJECT` targets exist in `CMakeLists.txt`.
- The stage 0 dependency-graph script can now be pointed at real CMake
  target boundaries instead of the static classification table, and used
  as the enforcement mechanism for every stage from here on.

## Implementation notes (from actually doing it)

Implemented and verified end-to-end on both platforms (native Linux and
mingw-w64 cross-compile), both `BFDEBUG_LEVEL` variants, producing four
real, working binaries. Four things came up that weren't obvious from the
plan alone:

**Each library compiles twice, not once.** `globals.h`'s `SYNCDBG`/
`WARNDBG`/etc. debug-logging macros key off `BFDEBUG_LEVEL`, and
`globals.h` is included almost everywhere — so `BFDEBUG_LEVEL` isn't just
a link-time distinction between `keeperfx` (0) and `keeperfx_hvlog` (10),
it changes compiled code across nearly the whole tree. OBJECT library
sources compile once according to that library's own settings; sharing
one set of `kfx_*` libraries between both executables would have silently
dropped `keeperfx_hvlog`'s enhanced debug logging everywhere. Fixed with
two INTERFACE libraries, `kfx_bfdebug_std` (`BFDEBUG_LEVEL=0`) and
`kfx_bfdebug_hvlog` (`BFDEBUG_LEVEL=10`), and every `kfx_add_component()`
call produces `${name}` + `${name}_hvlog` OBJECT library pairs. This
doubles the target count to ~18, not ~10 — no more total compile work
than before (every file was already compiled twice, once per
`add_executable`), just organized differently.

**OBJECT libraries need dependency include paths at their own compile
time, not just at final link time.** The original `deps/CMakeLists.txt`
linked `astronomy_static`/`centijson_static`/SDL2/etc. directly onto
`keeperfx`/`keeperfx_hvlog`. That only makes those include directories
available when the *executable* compiles its own sources — each `kfx_*`
OBJECT library compiles independently before that, so e.g. `moonphase.c`
(now inside `kfx_platform`) couldn't find `astronomy.h` at all. Fixed by:
restructuring `deps/CMakeLists.txt` around a single `kfx_deps` INTERFACE
library (instead of referencing `keeperfx`/`keeperfx_hvlog` by name, which
also let `add_subdirectory(deps)` move earlier, before the `kfx_*`
libraries are defined) and linking `kfx_deps` + the SDL2 family into a new
shared `kfx_common_opts` INTERFACE library that every `kfx_*` library and
both executables link against.

**Two link-order bugs, both from `ld.bfd`'s single left-to-right symbol
resolution pass (confirmed on both native Linux and mingw-w64 — neither
is `--start-group`'d):**
1. `kfx_common_opts` (carrying `kfx_deps`'s static libraries) has to be
   listed *after* the OBJECT libraries that need those symbols in
   `target_link_libraries(keeperfx PRIVATE ...)`, not before — otherwise
   the `.a` files are already "used up" by the time the linker reaches
   the `.o` files referencing them (surfaced as `kfx_script`'s `api.c`
   failing to find centijson's `value_dict_get` and friends).
2. `centitoml` (an OBJECT library itself, from `deps/CMakeLists.txt`)
   didn't get its compiled object into the final link at all when only
   reachable through two levels of INTERFACE-library indirection
   (`centitoml` → `kfx_deps` → `kfx_common_opts`) — its *include*
   directories propagated fine (nothing failed to find `toml.h`), but the
   object file itself didn't. Fixed with a direct
   `target_link_libraries(keeperfx PRIVATE centitoml)`, matching what the
   pre-stage-1 file always did.
3. Windows-only: `libcurl_static` needs `recvfrom`/`gethostname` (winsock),
   and `enet_static` already links `ws2_32` — but only satisfies *enet's*
   winsock needs at the point `enet_static` is processed, not libcurl's
   later, different ones. Matches why `Makefile`'s `LINKLIB` explicitly
   re-lists `-lws2_32` (and `-lwinmm -lmingw32 -lole32 -luuid`) at the very
   end, after everything else — copied that pattern into
   `CMakeLists.txt`'s Windows system-library block.

None of these are specific to the *grouping* concept — they're exactly
the class of bug this stage exists to catch early, before stages 3+ do
real file moves on top of a shaky foundation.
