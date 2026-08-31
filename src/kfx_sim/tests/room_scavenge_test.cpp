// kfx_sim room cluster depth increment, per docs/refactor/testing/
// comprehensive/stage-08b-kfx-sim-clusters.md's "still open" list:
// room_scavenge.c's one function, get_scavenge_effect(). Pattern A on
// kfx_sim_state.dungeon[]'s color_idx, reached through get_player_color_idx
// (player_data.c). PLAYER_NEUTRAL (5) is a special case: get_player_color_idx
// returns the player number itself rather than indexing a dungeon, so the
// neutral owner is tested separately from the normal dungeon-lookup path.
#include <catch2/catch_test_macros.hpp>

#include "room_scavenge.h"
#include "globals.h"
#include "dungeon_data.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct ResetSimState {
    ResetSimState() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
    }
};
}

TEST_CASE_METHOD(ResetSimState, "get_scavenge_effect indexes scavenge_effect[] by the owner's dungeon color_idx", "[kfx_sim][room_scavenge]") {
    struct Dungeon *dungeon = get_dungeon(0);
    dungeon->color_idx = 3;
    CHECK(get_scavenge_effect(0) == TngEff_BallPuffYellow); // index 3
}

TEST_CASE_METHOD(ResetSimState, "get_scavenge_effect wraps the owner into PLAYERS_COUNT before the dungeon lookup", "[kfx_sim][room_scavenge]") {
    // owner=12 wraps to dungeon 3 (12 % PLAYERS_COUNT==9), not dungeon 12.
    struct Dungeon *dungeon = get_dungeon(3);
    dungeon->color_idx = 7;
    CHECK(get_scavenge_effect(12) == TngEff_BallPuffBlack); // index 7
}

TEST_CASE_METHOD(ResetSimState, "get_scavenge_effect treats PLAYER_NEUTRAL as its own color index, not a dungeon lookup", "[kfx_sim][room_scavenge]") {
    // get_player_color_idx special-cases PLAYER_NEUTRAL (5): it returns the
    // player number itself rather than reading a dungeon's color_idx --
    // confirmed by reading player_data.c's body, not assumed. Dungeon 5's
    // color_idx is deliberately left non-zero to prove it's never read.
    struct Dungeon *dungeon = get_dungeon(PLAYER_NEUTRAL);
    dungeon->color_idx = 8;
    CHECK(get_scavenge_effect(PLAYER_NEUTRAL) == TngEff_BallPuffWhite); // index 5
}
