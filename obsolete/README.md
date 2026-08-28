# Obsolete build files

Not part of any build path CI or `build-cmake.sh` actually exercises. Kept
for reference/history rather than deleted outright.

- `keeperfx_vs2010.sln` / `.vcxproj` / `.vcxproj.filters` / `.vcxproj.user` —
  Visual Studio 2010 project. Predates the mingw-w64/CMake toolchains; not
  maintained in sync with `src/kfx_*/`.
- `libexterns.mk` — fetches/builds the SDL3 mingw dev libs for the
  Makefile's own `standard`/`heavylog` source-compile targets. Those targets
  are superseded by the CMake build (see `../CLAUDE.md`'s Build section) and
  aren't wired to fetch their own `sdl/` directory anymore.
- `tool_dkillconv.mk` — already `#include`d as dead code in `../Makefile`
  before this move; nothing enables it.

Everything else at the repo root that `../Makefile` still `include`s
(`package.mk`, `pkg_gfx.mk`, `pkg_lang.mk`, `pkg_sfx.mk`, `prebuilds.mk`,
`version.mk`, the active `tool_*.mk` files) is still load-bearing — CI calls
`make pkg-languages`/`pkg-gfx`/`pkg-enginegfx`/`pkg-assemble` for the
asset/data pipeline, just not for compiling the game binary anymore.
