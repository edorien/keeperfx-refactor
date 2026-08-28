# Stage 3 — Physically extract `kfx_platform`

See [00-overview.md](00-overview.md) for the full plan context.

## Goal

Now that stage 2 proved `bflib_*` acyclic, physically move it (and the
handful of other files independently classified as platform-layer, see
below) into `libs/kfx_platform/`. Mechanical, low-risk, high-file-count —
land it as its own PR, not bundled with other changes.

## Scope

The `bflib_*` files (69), plus these files reclassified into
`kfx_platform` by direct inspection of their includes/content rather than
naming convention (see the investigation summarized in
[00-overview.md §3](00-overview.md#3-current-state--headline-evidence)):

| File | Why platform, not something else |
|---|---|
| `globals.h` | Shared low-level typedefs/geometry structs used by every future library; only depends on `bflib_basics.h`/`version.h`. Confirmed *not* a god header. |
| `thread.hpp`, `mutex.hpp` | Pure threading-primitive polyfills, zero game-domain content. |
| `kfx_memory.c/.h` | Centralized malloc/free wrapper + arena allocator; zero project dependencies beyond `<stddef.h>`. |
| `custom_zip.c/.h` | Low-level "read a named entry out of a level's zip bundle" helper; only depends on `bflib_fileio.h`/`config.h`. |
| `cdrom.cpp/.h` | Win32 MCI Redbook CD-audio wrapper. Its `#include "game_legacy.h"` is entirely unused — drop it as part of the move. |
| `steam_api.cpp/.hpp` | Steamworks SDK dynamic-loading wrapper — vendor/OS integration, not game logic. |
| `windows.cpp`, `linux.cpp` | Concrete OS implementations of `platform.h`'s abstraction functions (`get_os_version`, `install_exception_handler`, etc.). Neither contains a `main()`. |
| `moonphase.c/.h` | Self-contained real-world astronomy calculation (wraps external `astronomy.h`); only depends on `globals.h`. Its *output* feeds gameplay elsewhere, but the module itself has no engine/sim coupling. |
| `sound_manager.cpp/.h` | Bridges `bflib_sndlib` to sound-asset-path resolution. Reads creature model names from `config_creature.h`/`creature_control.h` to build file paths — flag this one dependency for a follow-up look (it's a config-layer read from an otherwise platform-layer file), but it doesn't block the move. |
| `platform.h`, `compiler_compat.h`, `version.h`, `pre_inc.h`, `post_inc.h` | Pure portability/build-wrapper headers. |

`value_util.c/.h` was originally guessed as platform-layer but the config
investigation found it's TOML-parsing-helper code (`load_toml_file`,
`value_parse_class`/`value_parse_model` against `config_*` `NamedCommand`
tables) — it belongs in `kfx_config` (stage 4), not here.

## Work

1. `git mv` the files above into `libs/kfx_platform/{src,include}/`.
2. Update `#include` paths across the tree and in `CMakeLists.txt`.
3. Turn the stage-1 `kfx_platform` `OBJECT` target into a real
   `add_library(kfx_platform STATIC ...)` (or keep `OBJECT` — either works
   for a single-binary build) with its own `target_include_directories`.
4. `sound_manager.*`'s one config-layer read: either accept it for now
   (flag as a known, narrow, one-directional exception) or resolve it by
   having the config layer pass resolved sound file paths in rather than
   `sound_manager` looking up creature config directly — maintainer's call,
   doesn't need to block this stage.

## Exit criterion

`libs/kfx_platform` exists as a real library target with its own include
directory; none of the files listed above remain in `src/`; `keeperfx`/
`keeperfx_hvlog` build and `src/ftests` pass unchanged.
