// First kfx_apploop coverage, per docs/refactor/testing/comprehensive/
// stage-07-kfx-apploop-game-process.md: display_should_be_updated_this_turn()
// was picked as the pilot for being a state read with branching, not
// update()'s dispatcher -- but reading its body (not just its name) found
// it's richer than that: the "not fast-forwarding" branch unconditionally
// calls find_frame_rate(), which reads kfx_platform's LbTimerClock. That
// turned out to already be a registered function-pointer seam
// (bflib_datetm.h: `extern TbClockMSec (*LbTimerClock)(void);`, default-
// NULL until LbTimerInit() runs) rather than a raw platform call, so a
// fake clock is enough to make this safely callable without crashing --
// exactly the kind of thing worth confirming by reading the real
// declaration rather than assuming, per this whole stage's own "verify,
// don't guess" note in stage-04b/04c.
//
// Also exercises pattern B for the first time in this library: kfx_platform's
// get_gameturn() is a thin wrapper around a registered GetGameTurnFunc
// provider (docs/refactor/todo/check-layering-symbol-level-blind-spot.md),
// defaulting to a safe stub (always 0) -- this fixture registers its own
// fake provider so the gameturn-modulo branches below are actually
// reachable and controllable, not stuck at the default's degenerate "0 %
// anything == 0" case.
#include <catch2/catch_test_macros.hpp>

#include "game_session_loop.h"
#include "kfx_sim_state.h"
#include "kfx_net_state.h"
#include "globals.h"
#include "bflib_datetm.h"

#include <cstring>

namespace {
GameTurn g_fake_gameturn = 0;
GameTurn fake_gameturn_provider() { return g_fake_gameturn; }

TbClockMSec fake_clock_provider() { return 0; }

struct AppLoopFixture {
    AppLoopFixture() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::memset(&kfx_net_state, 0, sizeof(kfx_net_state));
        // Avoid a null-pointer call inside find_frame_rate()/
        // packet_load_find_frame_rate() -- LbTimerClock defaults to NULL
        // until LbTimerInit() (never called in this test binary) sets it.
        LbTimerClock = fake_clock_provider;
        g_fake_gameturn = 0;
        set_get_gameturn_provider(fake_gameturn_provider);
    }
    ~AppLoopFixture() {
        set_get_gameturn_provider(nullptr); // restores kfx_platform's own default stub
    }
};
}

TEST_CASE_METHOD(AppLoopFixture, "display_should_be_updated_this_turn returns true when paused", "[kfx_apploop][game_session_loop]") {
    kfx_sim_state.operation_flags |= GOF_Paused;
    CHECK(display_should_be_updated_this_turn());
}

TEST_CASE_METHOD(AppLoopFixture, "display_should_be_updated_this_turn returns true when frame_skip is disabled", "[kfx_apploop][game_session_loop]") {
    kfx_net_state.turns_fastforward = 0;
    kfx_net_state.packet_loading_in_progress = 0;
    kfx_net_state.frame_skip = 0;
    CHECK(display_should_be_updated_this_turn());
}

TEST_CASE_METHOD(AppLoopFixture, "display_should_be_updated_this_turn honors frame_skip against the current turn", "[kfx_apploop][game_session_loop]") {
    kfx_net_state.turns_fastforward = 0;
    kfx_net_state.packet_loading_in_progress = 0;
    kfx_net_state.frame_skip = 5;

    g_fake_gameturn = 10; // 10 % 5 == 0
    CHECK(display_should_be_updated_this_turn());

    g_fake_gameturn = 7; // 7 % 5 != 0
    CHECK_FALSE(display_should_be_updated_this_turn());
}

TEST_CASE_METHOD(AppLoopFixture, "display_should_be_updated_this_turn while fast-forwarding checks the turn's low bits", "[kfx_apploop][game_session_loop]") {
    kfx_net_state.turns_fastforward = 1; // nonzero -- takes the fast-forward branch
    kfx_net_state.packet_loading_in_progress = 0;

    g_fake_gameturn = 0x40; // & 0x3F == 0
    CHECK(display_should_be_updated_this_turn());

    g_fake_gameturn = 0x01; // & 0x3F != 0, and not packet-loading
    CHECK_FALSE(display_should_be_updated_this_turn());
}
