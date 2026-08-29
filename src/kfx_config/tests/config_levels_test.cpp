// kfx_config: config.c's campaign level-membership predicates, per
// docs/refactor/testing/comprehensive/stage-08-comprehensive-library-
// passes.md's kfx_config row. All read the module-level `campaign`
// global (config_campaigns.h) -- pattern A, but on `campaign` rather
// than `kfx_config_state`, since it's a separate top-level global.
#include <catch2/catch_test_macros.hpp>

#include "config.h"
#include "config_campaigns.h"

#include <cstring>

namespace {
struct ResetCampaign {
    ResetCampaign() { std::memset(&campaign, 0, sizeof(campaign)); }
};
}

TEST_CASE_METHOD(ResetCampaign, "is_bonus_level checks array membership and rejects lvnum < 1 upfront", "[kfx_config][config]") {
    campaign.bonus_levels[2] = 42;
    CHECK(is_bonus_level(42));
    CHECK_FALSE(is_bonus_level(99));  // not present
    CHECK_FALSE(is_bonus_level(0));   // rejected before the array is even checked
}

TEST_CASE_METHOD(ResetCampaign, "is_extra_level checks array membership", "[kfx_config][config]") {
    campaign.extra_levels[1] = 7;
    CHECK(is_extra_level(7));
    CHECK_FALSE(is_extra_level(8));
}

TEST_CASE_METHOD(ResetCampaign, "storage_index_for_bonus_level counts only the non-zero slots before the match, not the raw array index", "[kfx_config][config]") {
    // A zero entry is "no slot stored yet" and is skipped in the count --
    // this is a compacted index, not campaign.bonus_levels[i]'s own
    // index, confirmed by hand-tracing the loop before asserting.
    campaign.bonus_levels[0] = 5;
    campaign.bonus_levels[1] = 0;
    campaign.bonus_levels[2] = 7;
    campaign.bonus_levels[3] = 42;
    CHECK(storage_index_for_bonus_level(5) == 0);  // first match, nothing counted yet
    CHECK(storage_index_for_bonus_level(42) == 2); // only slots 0 and 2 counted; slot 1 (zero) skipped
}

TEST_CASE_METHOD(ResetCampaign, "storage_index_for_bonus_level returns -1 for an unstored or invalid level", "[kfx_config][config]") {
    campaign.bonus_levels[0] = 5;
    CHECK(storage_index_for_bonus_level(999) == -1);
    CHECK(storage_index_for_bonus_level(0) == -1);
}

TEST_CASE_METHOD(ResetCampaign, "array_index_for_singleplayer_level returns the raw array index of a match", "[kfx_config][config]") {
    campaign.single_levels[3] = 7;
    CHECK(array_index_for_singleplayer_level(7) == 3);
    CHECK(array_index_for_singleplayer_level(999) == -1);
    CHECK(array_index_for_singleplayer_level(0) == -1);
}

TEST_CASE_METHOD(ResetCampaign, "bonus_level_for_singleplayer_level looks up the bonus at the same slot as the single-player level", "[kfx_config][config]") {
    campaign.single_levels[3] = 7;
    campaign.bonus_levels[3] = 42;
    CHECK(bonus_level_for_singleplayer_level(7) == 42);
    CHECK(bonus_level_for_singleplayer_level(999) == 0); // not found -> 0, not -1
}

TEST_CASE_METHOD(ResetCampaign, "is_singleplayer_level/is_multiplayer_level/is_freeplay_level each check their own array", "[kfx_config][config]") {
    campaign.single_levels[0] = 10;
    campaign.multi_levels[0] = 20;
    campaign.freeplay_levels[0] = 30;

    CHECK(is_singleplayer_level(10));
    CHECK_FALSE(is_singleplayer_level(20)); // multiplayer level, not single-player
    CHECK(is_multiplayer_level(20));
    CHECK(is_freeplay_level(30));
    CHECK_FALSE(is_freeplay_level(10));
}

TEST_CASE_METHOD(ResetCampaign, "is_campaign_level is true if any of the single/bonus/extra/multi arrays match", "[kfx_config][config]") {
    campaign.multi_levels[0] = 20;
    CHECK(is_campaign_level(20));
    CHECK_FALSE(is_campaign_level(999));
}

TEST_CASE_METHOD(ResetCampaign, "is_singleplayer_like_level treats the finished/not-started sentinels as single-player, alongside real single-player levels", "[kfx_config][config]") {
    campaign.single_levels[0] = 7;
    CHECK(is_singleplayer_like_level(SINGLEPLAYER_FINISHED));
    CHECK(is_singleplayer_like_level(SINGLEPLAYER_NOTSTARTED));
    CHECK(is_singleplayer_like_level(7));
    CHECK_FALSE(is_singleplayer_like_level(999));
}
