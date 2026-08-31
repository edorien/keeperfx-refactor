// kfx_config: config_powerhands.c's load_powerhands_config_file() -- a
// second worked example of the per-loader-schema TOML-fixture pattern
// config_textures_test.cpp established (docs/Architecture/
// testing-harness.md §10's "volume problem, not a capability gap" for
// the remaining config_*.c loaders). load_powerhands_config_file itself
// is `static` -- reached here through the public
// keeper_powerhands_file_data.load_func field, same ConfigFileData
// interface every config_*.c loader implements.
#include <catch2/catch_test_macros.hpp>

#include "kfx_config_test_paths.h" // KFX_CONFIG_TEST_FIXTURES_DIR
#include "config_powerhands.h"
#include "kfx_config_state.h"

#include <cstring>

namespace {
struct ResetConfigState {
    ResetConfigState() { std::memset(&kfx_config_state, 0, sizeof(kfx_config_state)); }
};
}

TEST_CASE_METHOD(ResetConfigState, "load_powerhands_config_file maps a hand section's name/anim/speed fields into kfx_config_state", "[kfx_config][config_powerhands]") {
    REQUIRE(keeper_powerhands_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/powerhands_minimal.toml", 0));

    struct PowerHandConfigStats *stat = &kfx_config_state.conf.power_hand_conf.pwrhnd_cfg_stats[0];
    CHECK(std::strcmp(stat->code_name, "HAND0") == 0);
    CHECK(stat->anim_idx[HndA_Hold] == 100);
    CHECK(stat->anim_idx[HndA_HoldGold] == 101);
    CHECK(stat->anim_idx[HndA_Hover] == 102);
    CHECK(stat->anim_idx[HndA_Pickup] == 103);
    CHECK(stat->anim_idx[HndA_SideHover] == 104);
    CHECK(stat->anim_idx[HndA_SideSlap] == 105);
    CHECK(stat->anim_idx[HndA_Slap] == 106);
    CHECK(stat->anim_speed[HndA_Hold] == 1);
    CHECK(stat->anim_speed[HndA_HoldGold] == 2);
    CHECK(stat->anim_speed[HndA_Hover] == 3);
    CHECK(stat->anim_speed[HndA_Pickup] == 4);
    CHECK(stat->anim_speed[HndA_SideHover] == 5);
    CHECK(stat->anim_speed[HndA_SideSlap] == 6);
    CHECK(stat->anim_speed[HndA_Slap] == 7);
}

TEST_CASE_METHOD(ResetConfigState, "load_powerhands_config_file leaves unconfigured hand slots untouched", "[kfx_config][config_powerhands]") {
    REQUIRE(keeper_powerhands_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/powerhands_minimal.toml", 0));

    struct PowerHandConfigStats *stat = &kfx_config_state.conf.power_hand_conf.pwrhnd_cfg_stats[1];
    CHECK(stat->code_name[0] == '\0');
    CHECK(stat->anim_idx[HndA_Hold] == 0);
    CHECK(stat->anim_speed[HndA_Hold] == 0);
}

TEST_CASE_METHOD(ResetConfigState, "load_powerhands_config_file returns false for a missing file", "[kfx_config][config_powerhands]") {
    CHECK_FALSE(keeper_powerhands_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/does_not_exist.toml", CnfLd_IgnoreErrors));
}

TEST_CASE("keeper_powerhands_file_data has no pre/post-load hooks", "[kfx_config][config_powerhands]") {
    CHECK(keeper_powerhands_file_data.pre_load_func == nullptr);
    CHECK(keeper_powerhands_file_data.post_load_func == nullptr);
    CHECK(std::strcmp(keeper_powerhands_file_data.filename, "powerhands.toml") == 0);
}
