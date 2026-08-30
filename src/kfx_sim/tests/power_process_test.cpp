// kfx_sim: power_process.c's players_disease_can_infect_target_players_
// creatures() -- pure logic over kfx_sim_state.players[]/kfx_config_state,
// reusing player_data_test.cpp's ResetSimState fixture shape and its
// already-covered players_are_enemies()/player_allied_with() building
// blocks. Everything else in this file (process_armageddon,
// lightning_modify_palette, god_lightning_*, power-sight explored-flag
// bookkeeping, ...) reaches into real Thing/shot/light state and isn't
// attempted here.
//
// Found by testing, not assumed: reaching the allies_share_disease
// branch at all requires source_player to already have target flagged
// as an ally (that's exactly what makes players_are_enemies() return
// false and fall through to this branch) -- and player_allied_with()
// checks that *same* flag in the *same* direction. So
// "allies_share_disease == true" doesn't mean "infection allowed
// between allies who don't explicitly share disease" as the name might
// suggest; by construction, every case that reaches this branch already
// has the one-directional alliance flag set, so it always resolves to
// "can't infect".
#include <catch2/catch_test_macros.hpp>

#include "power_process.h"
#include "player_data.h" // get_player, to_flag
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

TEST_CASE_METHOD(ResetSimState, "players_disease_can_infect_target_players_creatures is false for a player and themself", "[kfx_sim][power_process]") {
    CHECK_FALSE(players_disease_can_infect_target_players_creatures(2, 2));
}

TEST_CASE_METHOD(ResetSimState, "players_disease_can_infect_target_players_creatures is false when the source is the neutral player", "[kfx_sim][power_process]") {
    kfx_config_state.neutral_player_num = 3;
    CHECK_FALSE(players_disease_can_infect_target_players_creatures(3, 5));
}

TEST_CASE_METHOD(ResetSimState, "players_disease_can_infect_target_players_creatures is true between two non-allied (i.e. enemy) players", "[kfx_sim][power_process]") {
    kfx_config_state.neutral_player_num = 6; // out of the way, so 0/1 are real non-neutral players
    // No allied_players flags set -- players_are_enemies() is true.
    CHECK(players_disease_can_infect_target_players_creatures(0, 1));
}

TEST_CASE_METHOD(ResetSimState, "players_disease_can_infect_target_players_creatures is false between allies once allies_share_disease is on", "[kfx_sim][power_process]") {
    kfx_config_state.neutral_player_num = 6;
    struct PlayerInfo *source = get_player(0);
    source->allied_players |= to_flag(1); // player 0 flags player 1 as an ally
    kfx_config_state.conf.rules[0].gameplay.allies_share_disease = true;
    CHECK_FALSE(players_disease_can_infect_target_players_creatures(0, 1));
}

TEST_CASE_METHOD(ResetSimState, "players_disease_can_infect_target_players_creatures is true between allies when allies_share_disease is off", "[kfx_sim][power_process]") {
    kfx_config_state.neutral_player_num = 6;
    struct PlayerInfo *source = get_player(0);
    source->allied_players |= to_flag(1);
    kfx_config_state.conf.rules[0].gameplay.allies_share_disease = false;
    CHECK(players_disease_can_infect_target_players_creatures(0, 1));
}
