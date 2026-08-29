// kfx_pathfinding: ariadne_tringls.c, per docs/refactor/testing/
// stage-08-comprehensive-library-passes.md's kfx_pathfinding row.
// Covers the bounds-checked field accessors (region id / edge length /
// tree_alt), the pointer-bounds-checked invalid/get pair, link_find's
// pure array search, and the free-list-vs-ix_Triangles allocator split
// in tri_new/tri_dispose -- all self-contained against the module-level
// Triangles[] array, no PathfindingWorldCallbacks needed.
#include <catch2/catch_test_macros.hpp>

#include "ariadne_tringls.h"
#include "ariadne.h" // NAVMAP_FLOORHEIGHT_MAX

namespace {
// A scratch slot far from anything triangulation_init_triangles touches
// (which only ever writes Triangles[0]/Triangles[1]), so accessor tests
// can't collide with the allocator tests below even though both share
// the same module-level array across the whole binary.
constexpr long kScratchTri = TRIANLGLES_COUNT - 1;

struct ResetScratchTriangle {
    ResetScratchTriangle() {
        Triangles[kScratchTri].region_and_edgelen = 0;
        Triangles[kScratchTri].tree_alt = 0;
        Triangles[kScratchTri].tags[0] = 0;
        Triangles[kScratchTri].tags[1] = 0;
        Triangles[kScratchTri].tags[2] = 0;
    }
};
}

TEST_CASE_METHOD(ResetScratchTriangle, "set/get_triangle_region_id round-trips without disturbing edgelen", "[kfx_pathfinding][ariadne_tringls]") {
    REQUIRE(set_triangle_edgelen(kScratchTri, 5));
    REQUIRE(set_triangle_region_id(kScratchTri, 123));
    CHECK(get_triangle_region_id(kScratchTri) == 123);
    CHECK(get_triangle_edgelen(kScratchTri) == 5);
}

TEST_CASE_METHOD(ResetScratchTriangle, "set/get_triangle_edgelen round-trips without disturbing region id", "[kfx_pathfinding][ariadne_tringls]") {
    REQUIRE(set_triangle_region_id(kScratchTri, 77));
    REQUIRE(set_triangle_edgelen(kScratchTri, 9));
    CHECK(get_triangle_edgelen(kScratchTri) == 9);
    CHECK(get_triangle_region_id(kScratchTri) == 77);
}

TEST_CASE("get/set_triangle_region_id and _edgelen reject out-of-range triangle ids", "[kfx_pathfinding][ariadne_tringls]") {
    CHECK(get_triangle_region_id(-1) == -1);
    CHECK(get_triangle_region_id(TRIANLGLES_COUNT) == -1);
    CHECK_FALSE(set_triangle_region_id(-1, 1));
    CHECK_FALSE(set_triangle_region_id(TRIANLGLES_COUNT, 1));

    CHECK(get_triangle_edgelen(-1) == 0);
    CHECK(get_triangle_edgelen(TRIANLGLES_COUNT) == 0);
    CHECK_FALSE(set_triangle_edgelen(-1, 1));
    CHECK_FALSE(set_triangle_edgelen(TRIANLGLES_COUNT, 1));
}

TEST_CASE_METHOD(ResetScratchTriangle, "get_triangle_tree_alt reads the field for a valid id", "[kfx_pathfinding][ariadne_tringls]") {
    Triangles[kScratchTri].tree_alt = 42;
    CHECK(get_triangle_tree_alt(kScratchTri) == 42);
}

TEST_CASE("get_triangle_tree_alt rejects out-of-range triangle ids", "[kfx_pathfinding][ariadne_tringls]") {
    // NavColour is uint16_t, so the function's literal "return -1" actually
    // comes back as NAV_COL_UNSET (65535), not -1 -- confirmed by running
    // before asserting, not assumed from the source's literal "-1".
    CHECK(get_triangle_tree_alt(-1) == NAV_COL_UNSET);
    CHECK(get_triangle_tree_alt(TRIANLGLES_COUNT) == NAV_COL_UNSET);
}

TEST_CASE("get_triangle bounds-checks and returns the INVALID_TRIANGLE sentinel out of range", "[kfx_pathfinding][ariadne_tringls]") {
    CHECK(get_triangle(-1) == INVALID_TRIANGLE);
    CHECK(get_triangle(TRIANLGLES_COUNT) == INVALID_TRIANGLE);
    CHECK(get_triangle(0) == &Triangles[0]);
    CHECK(get_triangle(kScratchTri) == &Triangles[kScratchTri]);
}

TEST_CASE("triangle_is_invalid bounds-checks the pointer, not just NULL/sentinel", "[kfx_pathfinding][ariadne_tringls]") {
    CHECK(triangle_is_invalid(NULL));
    CHECK(triangle_is_invalid(INVALID_TRIANGLE));
    CHECK(triangle_is_invalid(&Triangles[TRIANLGLES_COUNT])); // one past the end
    CHECK_FALSE(triangle_is_invalid(&Triangles[0]));
    CHECK_FALSE(triangle_is_invalid(&Triangles[kScratchTri]));
}

TEST_CASE_METHOD(ResetScratchTriangle, "link_find returns the tag index matching a value, or -1", "[kfx_pathfinding][ariadne_tringls]") {
    Triangles[kScratchTri].tags[0] = 10;
    Triangles[kScratchTri].tags[1] = 20;
    Triangles[kScratchTri].tags[2] = 30;
    CHECK(link_find(kScratchTri, 10) == 0);
    CHECK(link_find(kScratchTri, 20) == 1);
    CHECK(link_find(kScratchTri, 30) == 2);
    CHECK(link_find(kScratchTri, 999) == -1);
}

TEST_CASE("link_find rejects out-of-range triangle ids", "[kfx_pathfinding][ariadne_tringls]") {
    CHECK(link_find(-1, 0) == -1);
    CHECK(link_find(TRIANLGLES_COUNT, 0) == -1);
}

TEST_CASE("triangulation_init_triangles seeds the two starting triangles", "[kfx_pathfinding][ariadne_tringls]") {
    triangulation_init_triangles(10, 20, 30, 40);
    CHECK(ix_Triangles == 2);
    CHECK(count_Triangles == 2);
    CHECK(Triangles[0].tags[0] == 1);
    CHECK(Triangles[0].tags[1] == -1);
    CHECK(Triangles[0].tags[2] == -1);
    CHECK(Triangles[1].tags[0] == 0);
    CHECK(Triangles[1].tags[1] == -1);
    CHECK(Triangles[1].tags[2] == -1);
    CHECK(Triangles[0].tree_alt == NAVMAP_FLOORHEIGHT_MAX);
    CHECK(Triangles[1].tree_alt == NAVMAP_FLOORHEIGHT_MAX);
    CHECK(Triangles[0].navigation_flags == 7);
    CHECK(Triangles[1].navigation_flags == 7);
}

TEST_CASE("tri_new allocates from the free list before extending ix_Triangles", "[kfx_pathfinding][ariadne_tringls]") {
    // triangulation_init_triangles is also the only public reset for
    // free_Triangles/ix_Triangles/count_Triangles (free_Triangles itself
    // isn't part of the header's public surface), so it doubles as this
    // test's fixture -- mirrors how ariadne_edge_test.cpp uses
    // edge_points_clean() both as production API and as test setup.
    triangulation_init_triangles(0, 1, 2, 3);

    long a = tri_new();
    CHECK(a == 2); // first slot past the two seed triangles
    CHECK(ix_Triangles == 3);
    CHECK(count_Triangles == 3);

    tri_dispose(a);
    CHECK(count_Triangles == 2);
    CHECK(get_triangle_tree_alt(a) == NAV_COL_UNSET);

    long b = tri_new();
    CHECK(b == a); // reused from the free list, not a fresh ix_Triangles slot
    CHECK(ix_Triangles == 3); // unchanged: this allocation came from the free list
    CHECK(count_Triangles == 3);
}
