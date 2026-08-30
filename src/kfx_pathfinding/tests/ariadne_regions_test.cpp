// kfx_pathfinding: ariadne_regions.c -- targeted per the user's request to
// focus coverage around scripts/check_layering.py's CURRENTLY-FAILING
// (not accepted) kfx_pathfinding -> kfx_sim violation: this file
// #includes kfx_sim's player_data.h, but only for two symbols --
// PLAYERS_COUNT (a plain #define) and PlayerNumber (which is actually
// typedef'd in kfx_platform's globals.h already, not player_data.h at
// all). The one and only place PLAYERS_COUNT is used is
// navigation_regions_connected()'s owner-range bounds check -- so that
// function is this file's most direct test target for derisking a
// future fix of the violation (e.g. passing the bound in as a parameter
// instead of including the header for one constant).
//
// region_store_init/region_get/region_put (a plain circular queue, no
// dependencies) and regions_connected()'s early-return bounds/blocking
// checks are covered too, both because they're cheap and because
// navigation_regions_connected() delegates its own bounds/blocking
// pass-through straight to regions_connected().
//
// navigation_regions_connected()'s owner-in-range path additionally
// requires regions_connected() to return true, which (for two
// **never-yet-connected** triangles) means walking the real
// region_connect()/region_lnk() BFS over Triangles[].tags[] -- exactly
// the "needs a small triangulated fixture" territory stage-08's docs
// flagged for this library's other untested functions. Built the
// smallest possible one here: two triangles with a single mutual
// tag-link at corner 0 (all other corners -1, i.e. open/border edges) --
// hand-traced through region_alloc/region_lnk/region_connect's actual
// bodies before asserting, confirmed against the real run, not assumed.
#include <catch2/catch_test_macros.hpp>

#include "ariadne_regions.h"
#include "ariadne_tringls.h"
#include "ariadne.h" // NAVMAP_FLOORHEIGHT_MAX/_MASK, NAVMAP_OWNERSELECT_BIT

#include <cstring>

namespace {
// Scratch triangle indices, far from every other ariadne_tringls_test.cpp/
// ariadne_regions_test.cpp index (0-3, TRIANLGLES_COUNT-1) to avoid
// cross-test collisions within the same binary run.
constexpr long kTriA = 50000;
constexpr long kTriB = 50001;

struct ResetRegions {
    ResetRegions() {
        triangulation_init_regions();
        std::memset(&Triangles[kTriA], 0, sizeof(Triangles[kTriA]));
        std::memset(&Triangles[kTriB], 0, sizeof(Triangles[kTriB]));
        Triangles[kTriA].tags[0] = -1;
        Triangles[kTriA].tags[1] = -1;
        Triangles[kTriA].tags[2] = -1;
        Triangles[kTriB].tags[0] = -1;
        Triangles[kTriB].tags[1] = -1;
        Triangles[kTriB].tags[2] = -1;
    }

    // Links A and B as mutual neighbors at corner 0 -- the minimal
    // adjacency regions_connected()'s real BFS needs to put them in the
    // same region.
    void link_a_and_b() {
        Triangles[kTriA].tags[0] = kTriB;
        Triangles[kTriB].tags[0] = kTriA;
    }
};
}

TEST_CASE("region_store_init/region_get/region_put form a plain FIFO circular queue", "[kfx_pathfinding][ariadne_regions]") {
    region_store_init();
    CHECK(region_get() == -1); // empty

    region_put(10);
    region_put(20);
    region_put(30);
    CHECK(region_get() == 10);
    CHECK(region_get() == 20);
    CHECK(region_get() == 30);
    CHECK(region_get() == -1); // drained back to empty
}

TEST_CASE("regions_connected rejects out-of-range triangle indices", "[kfx_pathfinding][ariadne_regions]") {
    CHECK_FALSE(regions_connected(-1, 0));
    CHECK_FALSE(regions_connected(0, TRIANLGLES_COUNT));
}

TEST_CASE_METHOD(ResetRegions, "regions_connected is false when either triangle is floor-height-blocked", "[kfx_pathfinding][ariadne_regions]") {
    Triangles[kTriA].tree_alt = NAVMAP_FLOORHEIGHT_MAX; // blocked (tree_alt & MASK == MAX)
    CHECK_FALSE(regions_connected(kTriA, kTriB));
}

TEST_CASE_METHOD(ResetRegions, "regions_connected finds two directly tag-linked triangles connected", "[kfx_pathfinding][ariadne_regions]") {
    link_a_and_b();
    CHECK(regions_connected(kTriA, kTriB));
}

TEST_CASE_METHOD(ResetRegions, "regions_connected reports two never-linked triangles as not connected", "[kfx_pathfinding][ariadne_regions]") {
    // Both isolated (no shared edge) -- region_connect(A) claims a fresh
    // region for A alone; B is never reached, so it stays in the
    // (different) default region.
    CHECK_FALSE(regions_connected(kTriA, kTriB));
}

TEST_CASE_METHOD(ResetRegions, "navigation_regions_connected passes through regions_connected's false result regardless of owner", "[kfx_pathfinding][ariadne_regions]") {
    Triangles[kTriA].tree_alt = NAVMAP_FLOORHEIGHT_MAX;
    CHECK_FALSE(navigation_regions_connected(kTriA, kTriB, 0));
    CHECK_FALSE(navigation_regions_connected(kTriA, kTriB, -1));
}

TEST_CASE_METHOD(ResetRegions, "navigation_regions_connected is true for a connected pair with no owner-specific block set", "[kfx_pathfinding][ariadne_regions]") {
    link_a_and_b();
    CHECK(navigation_regions_connected(kTriA, kTriB, 3));
}

TEST_CASE_METHOD(ResetRegions, "navigation_regions_connected is true when the triangle is blocked for that specific owner", "[kfx_pathfinding][ariadne_regions]") {
    // Counterintuitive on a first read: a region reserved/blocked for a
    // given owner short-circuits to *true*, not false -- tested as the
    // function's actual documented-by-code behavior, the same "test
    // what's there" discipline used throughout this plan.
    link_a_and_b();
    Triangles[kTriA].tree_alt = (NavColour)(1 << (NAVMAP_OWNERSELECT_BIT + 2)); // blocks owner 2 only
    CHECK(navigation_regions_connected(kTriA, kTriB, 2));
}

TEST_CASE_METHOD(ResetRegions, "navigation_regions_connected's owner-specific block doesn't affect a different owner", "[kfx_pathfinding][ariadne_regions]") {
    link_a_and_b();
    Triangles[kTriA].tree_alt = (NavColour)(1 << (NAVMAP_OWNERSELECT_BIT + 2)); // blocks owner 2 only
    CHECK(navigation_regions_connected(kTriA, kTriB, 3)); // still true, via the real BFS this time, not the block
}

TEST_CASE_METHOD(ResetRegions, "navigation_regions_connected accepts an out-of-range owner as 'no player restriction'", "[kfx_pathfinding][ariadne_regions]") {
    // The exact reason this file includes kfx_sim's player_data.h: only
    // for the PLAYERS_COUNT bound checked here (9, hardcoded rather than
    // pulling in the header from this test, since PLAYERS_COUNT is the
    // only symbol from it this file's logic actually needs).
    link_a_and_b();
    CHECK(navigation_regions_connected(kTriA, kTriB, -1));
    CHECK(navigation_regions_connected(kTriA, kTriB, 9)); // == PLAYERS_COUNT
}

// region_set_f/region_unset_f/region_unlock all mutate the module-private
// Regions[] array too (num_triangles/is_connected bookkeeping), but
// that's not exposed through any accessor -- only region_set_f's write to
// the *triangle's own* region id (via set_triangle_region_id(), already
// covered directly in ariadne_tringls_test.cpp) is externally observable
// here, through get_triangle_region_id().
TEST_CASE_METHOD(ResetRegions, "region_set updates the triangle's own region id when it actually changes", "[kfx_pathfinding][ariadne_regions]") {
    CHECK(get_triangle_region_id(kTriA) == 0); // ResetRegions leaves the triangle zeroed
    region_set(kTriA, 5);
    CHECK(get_triangle_region_id(kTriA) == 5);
}

TEST_CASE_METHOD(ResetRegions, "region_set rejects an out-of-range triangle id or region id without touching anything", "[kfx_pathfinding][ariadne_regions]") {
    region_set(-1, 5);
    region_set(TRIANLGLES_COUNT, 5);
    region_set(kTriA, REGIONS_COUNT);
    CHECK(get_triangle_region_id(kTriA) == 0); // untouched by any of the three rejected calls
}

TEST_CASE_METHOD(ResetRegions, "region_unset resets the triangle's region id back to 0", "[kfx_pathfinding][ariadne_regions]") {
    region_set(kTriA, 5);
    REQUIRE(get_triangle_region_id(kTriA) == 5);
    region_unset(kTriA, 5);
    CHECK(get_triangle_region_id(kTriA) == 0);
}

TEST_CASE_METHOD(ResetRegions, "region_unlock is a safe no-op from this test's vantage point (Regions[] itself isn't exposed)", "[kfx_pathfinding][ariadne_regions]") {
    region_set(kTriA, 5);
    region_unlock(kTriA); // must not crash; its is_connected write is unobservable here
    CHECK(get_triangle_region_id(kTriA) == 5); // unaffected -- region_unlock never touches the triangle's own id
}
