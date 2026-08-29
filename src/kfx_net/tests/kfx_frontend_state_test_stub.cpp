// Part of net_resync.cpp's accepted raw-blob-resync residual (architecture.md
// §6.2). Split into its own shared library, separate from
// kfx_game_state_test_stubs.cpp, because it stops being needed on a
// different schedule: `kfx_frontend_state` only stops being a stub once a
// test target links the real kfx_frontend, which hasn't happened yet for
// any *_utest target (kfx_game_utest still needs this one, even though it
// no longer needs kfx_game_state_test_stubs.cpp -- see
// docs/refactor/testing/stage-04f-kfx-game.md).
#include "kfx_frontend_state.h"

struct KfxFrontendState kfx_frontend_state;
