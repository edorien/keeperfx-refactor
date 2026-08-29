// Part of net_resync.cpp's accepted raw-blob-resync residual (architecture.md
// §6.2; see net_resync_test_stubs.cpp's original, fuller comment, and
// docs/refactor/testing/stage-04e-kfx-net.md). Split into its own shared
// library (unlike kfx_frontend_state_test_stub.cpp's still-separate one)
// because `game`/`kfx_game_state` stop being stubs -- and become
// duplicate-definition errors -- the moment a test target also links the
// real kfx_game (kfx_game_legacy.c/kfx_game_state.c), the same reasoning
// stage-04d applied to creature_table_add[]. Any *_utest that transitively
// links kfx_net but not kfx_game needs this; kfx_game_utest itself does
// not (see stage-04f-kfx-game.md).
#include "game_legacy.h"
#include "kfx_game_state.h"

struct Game game;
struct KfxGameState kfx_game_state;
