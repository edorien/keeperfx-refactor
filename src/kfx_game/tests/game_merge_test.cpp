// kfx_game: game_merge.c's get_extra_level_kind_visibility, per
// docs/refactor/testing/comprehensive/stage-08-comprehensive-library-
// passes.md's kfx_game row -- considered for the stage-04f pilot and
// deferred there pending a check of is_full_moon/is_near_full_moon's
// nature. They turned out to be plain kfx_platform extern globals
// (moonphase.h), not a callback-struct indirection -- pattern A on two
// separate lower-layer globals (kfx_config's `campaign`, kfx_platform's
// moon-phase flags), not pattern B.
#include <catch2/catch_test_macros.hpp>

#include "game_merge.h"
#include "config.h"
#include "config_campaigns.h"
#include "moonphase.h"

#include <cstring>

namespace {
struct ResetMoonAndCampaign {
    ResetMoonAndCampaign() {
        std::memset(&campaign, 0, sizeof(campaign));
        is_full_moon = 0;
        is_near_full_moon = 0;
        is_new_moon = 0;
        is_near_new_moon = 0;
    }
};
}

TEST_CASE_METHOD(ResetMoonAndCampaign, "get_extra_level_kind_visibility is hidden when the extra level isn't configured at all", "[kfx_game][game_merge]") {
    CHECK(get_extra_level_kind_visibility(ExLv_None) == LvSt_Hidden);
}

TEST_CASE_METHOD(ResetMoonAndCampaign, "get_extra_level_kind_visibility(ExLv_FullMoon) is visible during a full moon", "[kfx_game][game_merge]") {
    campaign.extra_levels[ExLv_FullMoon - 1] = 5; // configured
    is_full_moon = 1;
    CHECK(get_extra_level_kind_visibility(ExLv_FullMoon) == LvSt_Visible);
}

TEST_CASE_METHOD(ResetMoonAndCampaign, "get_extra_level_kind_visibility(ExLv_FullMoon) is a half-show near a full moon", "[kfx_game][game_merge]") {
    campaign.extra_levels[ExLv_FullMoon - 1] = 5;
    is_near_full_moon = 1;
    CHECK(get_extra_level_kind_visibility(ExLv_FullMoon) == LvSt_HalfShow);
}

TEST_CASE_METHOD(ResetMoonAndCampaign, "get_extra_level_kind_visibility(ExLv_FullMoon) is hidden when configured but no moon flag is set", "[kfx_game][game_merge]") {
    campaign.extra_levels[ExLv_FullMoon - 1] = 5;
    CHECK(get_extra_level_kind_visibility(ExLv_FullMoon) == LvSt_Hidden);
}

TEST_CASE_METHOD(ResetMoonAndCampaign, "get_extra_level_kind_visibility(ExLv_NewMoon) is visible during a new moon", "[kfx_game][game_merge]") {
    campaign.extra_levels[ExLv_NewMoon - 1] = 7;
    is_new_moon = 1;
    CHECK(get_extra_level_kind_visibility(ExLv_NewMoon) == LvSt_Visible);
}

TEST_CASE_METHOD(ResetMoonAndCampaign, "get_extra_level_kind_visibility(ExLv_NewMoon) is a half-show near a new moon", "[kfx_game][game_merge]") {
    campaign.extra_levels[ExLv_NewMoon - 1] = 7;
    is_near_new_moon = 1;
    CHECK(get_extra_level_kind_visibility(ExLv_NewMoon) == LvSt_HalfShow);
}

TEST_CASE_METHOD(ResetMoonAndCampaign, "get_extra_level_kind_visibility is hidden regardless of moon phase when the extra level isn't configured", "[kfx_game][game_merge]") {
    is_full_moon = 1; // would show if configured
    CHECK(get_extra_level_kind_visibility(ExLv_FullMoon) == LvSt_Hidden);
}
