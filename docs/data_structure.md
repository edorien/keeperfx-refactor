## Library architecture

The codebase is split into internal libraries under `src/` (e.g.
`src/kfx_sim/include/`, `src/kfx_sim/src/`, alongside `src/main.cpp` and
`src/ftests/`), each a CMake `OBJECT` library with its own `include/`/`src/`.
Dependencies are strictly one-directional — a library may only `#include`
headers from itself or a library below it in this list:

```
kfx_platform → kfx_config → kfx_sim → kfx_render → kfx_net → kfx_game → kfx_frontend → kfx_script → kfx_apploop → app_entry (main.cpp)
```

`kfx_script`, `kfx_apploop`, and `main.cpp` sit at the top and may depend
on anything below them; nothing depends on them. This ordering is enforced
in CI (`scripts/check_layering.py --strict`, see
[stage-13-enforce-and-document.md](refactor/stage-13-enforce-and-document.md)) —
a back-edge fails the build, not just a code-review nitpick.

| library | owns |
|---|---|
| `kfx_platform` | OS/SDL integration (`bflib_*`), the low-level shared vocabulary header `globals.h` (coordinate structs, domain-ID typedefs), memory/zip/version helpers |
| `kfx_config` | Config-file loading (`config_*`) and the config-owned callback structs (`ConfigReloadCallbacks`, `SimFeedbackCallbacks`, `GameCallbacks`, etc.) that let lower/adjacent layers reach state or behavior owned above them without a direct `#include` |
| `kfx_sim` | The simulation core: map/slab/thing/creature/room/player/dungeon state and logic, pathfinding (`ariadne*`) |
| `kfx_render` | Rendering: the 3D engine, lighting, textures, sprites, video modes |
| `kfx_net` | Multiplayer networking and packet handling |
| `kfx_game` | Game-loop orchestration, level scripting data/CRUD, save/load |
| `kfx_frontend` | The UI: menus, in-game panels, input handling |
| `kfx_script` | Lua scripting bindings — deliberately wide access, since it's the mod-facing API surface |
| `kfx_apploop` | The top-level per-frame session loop tying every other layer together |

When a lower layer genuinely needs something from a higher one (state
reads, live-thing-list mutation triggered by a config reload, etc.), the
fix is an interface — a callback struct declared in the lower layer and
implemented by the higher one (wired up in `src/main.cpp`), a narrow
accessor function, or moving the field/function to whichever library is
actually the lowest-ranked real consumer — never a raw `#include` of a
higher layer's header. See
[00-overview.md §4](refactor/00-overview.md#4-target-architecture) for the
full per-library file-ownership breakdown and
[stage-13-enforce-and-document.md](refactor/stage-13-enforce-and-document.md)
for the catalogue of fix patterns used throughout.

## Structure

*Map structure*

 * Slabs
 * Subtiles
 * Columns
 * Cubes

*Things*

Every thing has
  * class_id
    * Empty - Nothing
    * Object - Real
    * Shot
    * EffectElem
    * DeadCreature
    * Creature
    * Effect
    * EffectGen
    * Trap
    * Door
    * Unkn10 - Unused?
    * Unkn11 - Unused?
    * AmbientSnd
    * CaveIn
  * owner
  * model

Everything is a thing
  * `struct Thing`

Creatures have additional structure
  * `struct CreatureControl`

### Slabs

Slab is one of a map.
Most important attributes are:
  * "type of a room"
  * "ownership"

AI "pathfinding" is slab-based.

Some slabs settings are hardcoded other settings are stored at `terrain.cfg`

* Slb_ID - used for "altering walls" so slabs with same Slb_ID are not altered \
  0 - solid environment (dirt, rocks, gold etc, Slab50)
  1 - ? (path, bridge guardpost)
  2 - dungeon (claimed path, doors)
  3 - lava
  4 - water
  5 - entrance center and wall
  ... other rooms and walls share same id

Doors are implemented as replacing slabs with some delay

For each subtile there is a mapping of columns and objects to place whenever such tile is placed on map. 

I.e. torches on wall, chandeliers on treasure room etc

That mapping is depends on tiles next to one placed

### Subtiles (aka STLs)


Subtile is minimal 2d part of a map.
Vision is STL-based

### Columns

Column is a stack of cubes for each subtile

### Cubes

Cube is one textured cube.
Mapped to texutres at `cubes.cfg`

There are
  * animated textures (i.e Magic door == Center of a temple)
  * static textures (simple walls)
