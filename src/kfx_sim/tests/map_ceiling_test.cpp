// kfx_sim: map_ceiling.c's ceiling_set_info() -- the one function in
// this file with no Map/Column world-state dependency, just validation
// plus a handful of kfx_sim_state.ceiling_* writes.
// ceiling_partially_recompute_heights()/ceiling_init() both need a real
// populated map (get_map_block_at/get_map_column, spiral_step[]) and
// aren't attempted here.
#include <catch2/catch_test_macros.hpp>

#include "globals.h" // MapSubtlCoord -- map_ceiling.h relies on this being included first
#include "map_ceiling.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct ResetCeilingState {
    ResetCeilingState() { std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state)); }
};
}

TEST_CASE_METHOD(ResetCeilingState, "ceiling_set_info rejects a non-positive step without touching any state", "[kfx_sim][map_ceiling]") {
    CHECK(ceiling_set_info(10, 0, 0) == 0);
    CHECK(kfx_sim_state.ceiling_dist == 0);
}

TEST_CASE_METHOD(ResetCeilingState, "ceiling_set_info rejects a height_max above 15", "[kfx_sim][map_ceiling]") {
    CHECK(ceiling_set_info(16, 0, 1) == 0);
}

TEST_CASE_METHOD(ResetCeilingState, "ceiling_set_info rejects height_min greater than height_max", "[kfx_sim][map_ceiling]") {
    CHECK(ceiling_set_info(5, 10, 1) == 0);
}

TEST_CASE_METHOD(ResetCeilingState, "ceiling_set_info sets ceiling_dist even when the resulting distance is rejected as too large", "[kfx_sim][map_ceiling]") {
    // height_min isn't bounded below (only height_min > height_max is
    // rejected), so a very negative height_min still clears the earlier
    // guards while pushing dist = (height_max-height_min)/step past 20:
    // (15 - (-100)) / 1 == 115. kfx_sim_state.ceiling_dist is written
    // *before* the dist>20 check, so it ends up set on this otherwise-
    // rejected call -- confirmed by running, not assumed from the source.
    CHECK(ceiling_set_info(15, -100, 1) == 0);
    CHECK(kfx_sim_state.ceiling_dist == 115);
    CHECK(kfx_sim_state.ceiling_height_max == 0); // NOT set: the function returned before this line
}

TEST_CASE_METHOD(ResetCeilingState, "ceiling_set_info on success populates every ceiling_* field, including the derived search_dist", "[kfx_sim][map_ceiling]") {
    CHECK(ceiling_set_info(12, 2, 2) == 1);
    CHECK(kfx_sim_state.ceiling_height_max == 12);
    CHECK(kfx_sim_state.ceiling_height_min == 2);
    CHECK(kfx_sim_state.ceiling_step == 2);
    CHECK(kfx_sim_state.ceiling_dist == 5); // (12-2)/2
    CHECK(kfx_sim_state.ceiling_search_dist == 121); // (2*5+1)^2
}
