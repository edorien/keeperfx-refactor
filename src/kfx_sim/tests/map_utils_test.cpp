// kfx_sim coverage, per docs/refactor/testing/comprehensive/
// stage-02-testability-and-fakes.md's rollout order and stage-08's
// per-cluster follow-up plan: map_utils.c's coordinate-math functions are
// pure (calling into kfx_platform's already-tested LbArcTanAngle), no
// pattern-A reset needed.
#include <catch2/catch_test_macros.hpp>

#include "map_utils.h"

#include <cstdio>

// Values below are the four cardinal directions, confirmed empirically
// against the real LbArcTanAngle rather than hand-derived from its
// documented angle convention alone (see
// docs/refactor/testing/stage-04c-kfx-sim.md for why that distinction
// mattered this session).
TEST_CASE("small_around_index_in_direction resolves the four cardinal directions", "[kfx_sim][map_utils]") {
    CHECK(small_around_index_in_direction(0, 0, 0, -100) == 0); // north (-y)
    CHECK(small_around_index_in_direction(0, 0, 100, 0) == 1);  // east (+x)
    CHECK(small_around_index_in_direction(0, 0, 0, 100) == 2);  // south (+y)
    CHECK(small_around_index_in_direction(0, 0, -100, 0) == 3); // west (-x)
}

TEST_CASE("small_around_index_in_direction is translation-invariant", "[kfx_sim][map_utils]") {
    // Only the source->destination vector matters, not the absolute
    // position -- shifting both points by the same offset must not
    // change the result.
    CHECK(small_around_index_in_direction(500, 500, 500, 400) == 0);
    CHECK(small_around_index_in_direction(500, 500, 600, 500) == 1);
}

// small_around_index_towards_destination's special-case branch (map_utils.c:
// exact 45-degree-multiple angles get a different formula, biased by the
// parity of (dest_x+dest_y)>>1) is exactly what every cardinal direction
// hits -- (0,-y)=angle 0, (+x,0)=512, (0,+y)=1024, (-x,0)=1536, all
// multiples of DEGREES_45=256. Confirmed empirically (a throwaway probe
// run once, not committed) before asserting these, the same discipline
// stage-04c used for small_around_index_in_direction above.
TEST_CASE("small_around_index_towards_destination resolves the four cardinal directions", "[kfx_sim][map_utils]") {
    CHECK(small_around_index_towards_destination(0, 0, 0, -100) == 0); // north (-y)
    CHECK(small_around_index_towards_destination(0, 0, 100, 0) == 1);  // east (+x)
    CHECK(small_around_index_towards_destination(0, 0, 0, 100) == 2);  // south (+y)
    CHECK(small_around_index_towards_destination(0, 0, -100, 0) == 3); // west (-x)
}
