// kfx_sim "player" cluster, per docs/refactor/testing/comprehensive/
// stage-08-comprehensive-library-passes.md §3: player_data.c's own
// accessors, plus players_are_enemies()/players_are_mutual_allies()'s
// pure short-circuit branches (self, and the neutral player, are special
// cases resolved before any real player-state lookup).
//
// Worth noting explicitly (found by reading the body, not assumed from
// the family resemblance to thing_data.c/creature_control.c/room_data.c):
// player_invalid() is a genuinely different shape from all three. Index
// 0 is a *real, valid* player here (get_player_f() returns
// &kfx_sim_state.players[plyr_idx] directly for 0 <= idx < PLAYERS_COUNT
// -- no "index 0 reserved" convention), and the invalid sentinel is a
// wholly separate global (INVALID_PLAYER == &bad_player), not
// &kfx_sim_state.players[0]. Tested as the function's actual behavior,
// same "test what's there" discipline as creature_control_invalid's
// missing upper-bound check (creature_control_test.cpp).
#include <catch2/catch_test_macros.hpp>

#include "player_data.h"
#include "kfx_sim_state.h"
#include "kfx_config_state.h"

#include <cstring>

namespace {
struct ResetSimState {
    ResetSimState() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::memset(&kfx_config_state, 0, sizeof(kfx_config_state));
    }
};
}

TEST_CASE_METHOD(ResetSimState, "player_invalid rejects the dedicated bad_player sentinel", "[kfx_sim][player_data]") {
    CHECK(player_invalid(INVALID_PLAYER));
}

TEST_CASE_METHOD(ResetSimState, "player_invalid accepts index 0 -- a real player here, unlike thing/creature/room index 0", "[kfx_sim][player_data]") {
    CHECK_FALSE(player_invalid(get_player(0)));
}

TEST_CASE_METHOD(ResetSimState, "player_exists is false until PlaF_Allocated is set", "[kfx_sim][player_data]") {
    struct PlayerInfo *player = get_player(0);
    CHECK_FALSE(player_exists(player));

    player->allocflags |= PlaF_Allocated;
    CHECK(player_exists(player));
}

TEST_CASE_METHOD(ResetSimState, "players_are_enemies: a player is never its own enemy", "[kfx_sim][player_data]") {
    CHECK_FALSE(players_are_enemies(2, 2));
}

TEST_CASE_METHOD(ResetSimState, "players_are_enemies: the neutral player is never anyone's enemy", "[kfx_sim][player_data]") {
    kfx_config_state.neutral_player_num = 3;
    CHECK_FALSE(players_are_enemies(3, 5));
    CHECK_FALSE(players_are_enemies(5, 3));
}

TEST_CASE_METHOD(ResetSimState, "players_are_mutual_allies: a player is always its own ally", "[kfx_sim][player_data]") {
    CHECK(players_are_mutual_allies(4, 4));
}

TEST_CASE_METHOD(ResetSimState, "players_are_mutual_allies: the neutral player can't be allied", "[kfx_sim][player_data]") {
    kfx_config_state.neutral_player_num = 3;
    CHECK_FALSE(players_are_mutual_allies(3, 5));
    CHECK_FALSE(players_are_mutual_allies(5, 3));
}
