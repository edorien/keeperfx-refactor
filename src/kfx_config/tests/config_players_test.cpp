// kfx_config: config_players.c's pure accessors (player_state_code_name,
// get_player_state_stats), pattern A on the public player_state_commands[]/
// kfx_config_state globals. load_playerstate_config_file() itself is
// `static` and not attempted here (see config_textures_test.cpp for a
// worked TOML-loader-fixture example of that shape).
#include <catch2/catch_test_macros.hpp>

#include "config_players.h"
#include "kfx_config_state.h"

#include <cstring>

namespace {
struct ResetPlayerState {
    ResetPlayerState() {
        // player_state_commands[] is declared `extern ...[]` (incomplete
        // array type) in the header, so sizeof() isn't available here --
        // its real definition (config_players.c) sizes it to
        // PLAYER_STATES_COUNT_MAX.
        std::memset(player_state_commands, 0, PLAYER_STATES_COUNT_MAX * sizeof(struct NamedCommand));
        std::memset(&kfx_config_state.conf.plyr_conf, 0, sizeof(kfx_config_state.conf.plyr_conf));
    }
};
}

TEST_CASE_METHOD(ResetPlayerState, "player_state_code_name resolves a registered player-state id to its config name", "[kfx_config][config_players]") {
    player_state_commands[0] = {"PSt_MyState", 5};
    CHECK(std::strcmp(player_state_code_name(5), "PSt_MyState") == 0);
}

TEST_CASE_METHOD(ResetPlayerState, "player_state_code_name falls back to INVALID for an unregistered id", "[kfx_config][config_players]") {
    CHECK(std::strcmp(player_state_code_name(99), "INVALID") == 0);
}

TEST_CASE_METHOD(ResetPlayerState, "get_player_state_stats returns a pointer into kfx_config_state's player-state config array", "[kfx_config][config_players]") {
    struct PlayerStateConfigStats *stats = get_player_state_stats(3);
    CHECK(stats == &kfx_config_state.conf.plyr_conf.plrst_cfg_stats[3]);
    stats->stop_own_units = true;
    CHECK(kfx_config_state.conf.plyr_conf.plrst_cfg_stats[3].stop_own_units);
}
