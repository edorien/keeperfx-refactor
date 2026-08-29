// kfx_pathfinding: ariadne_navitree.c, per docs/refactor/testing/
// comprehensive/stage-08-comprehensive-library-passes.md's kfx_pathfinding
// row -- the tag/route bookkeeping functions (tags_init, store/
// is_current_tag, copy_tree_to_route, update_border_tags/
// border_tags_to_current) are pure array operations over Tags[]/
// tree_dad[]/tag_current, no triangulated fixture needed. delaunay_init/
// _add/_add_triangle/optimise_heuristic/delaunay_seeded are deliberately
// NOT attempted here -- they need a real triangulated area, the same
// "big three" territory stage-08's kfx_pathfinding row flags.
//
// Tags[]/tree_dad[]/tag_current had no header declaration anywhere
// (only used within ariadne_navitree.c itself, or through the functions
// tested here) -- added to ariadne_navitree.h, the same "add the missing
// declaration" fix used repeatedly across this plan.
#include <catch2/catch_test_macros.hpp>

#include "ariadne_navitree.h"

#include <cstring>

namespace {
struct ResetNavitree {
    ResetNavitree() {
        std::memset(Tags, 0, sizeof(Tags));
        std::memset(tree_dad, 0, sizeof(tree_dad));
        tag_current = 1; // nonzero, so untouched (zeroed) Tags[] slots read as "not current"
    }
};
}

TEST_CASE_METHOD(ResetNavitree, "tags_init increments tag_current without clearing Tags below the wrap point", "[kfx_pathfinding][ariadne_navitree]") {
    tag_current = 5;
    Tags[10] = 5;
    tags_init();
    CHECK(tag_current == 6);
    CHECK(Tags[10] == 5); // untouched -- no wrap happened
}

TEST_CASE_METHOD(ResetNavitree, "tags_init wraps and clears every Tags slot once tag_current reaches 255", "[kfx_pathfinding][ariadne_navitree]") {
    tag_current = 255;
    Tags[10] = 99;
    tags_init();
    CHECK(tag_current == 1); // cleared to 0, then incremented
    CHECK(Tags[10] == 0);    // the whole array was cleared
}

TEST_CASE_METHOD(ResetNavitree, "store_current_tag/is_current_tag round-trip against the live tag_current value", "[kfx_pathfinding][ariadne_navitree]") {
    tag_current = 7;
    store_current_tag(5);
    CHECK(is_current_tag(5));
    CHECK_FALSE(is_current_tag(6)); // untouched slot, Tags[6] == 0 != tag_current
}

TEST_CASE_METHOD(ResetNavitree, "is_current_tag stops matching a slot once tag_current moves on", "[kfx_pathfinding][ariadne_navitree]") {
    tag_current = 7;
    store_current_tag(5);
    tag_current = 8; // a later tags_init()-style advance
    CHECK_FALSE(is_current_tag(5)); // still stamped with the old tag value
}

TEST_CASE_METHOD(ResetNavitree, "copy_tree_to_route walks tree_dad back from start to end", "[kfx_pathfinding][ariadne_navitree]") {
    tree_dad[10] = 5;
    tree_dad[5] = 2;
    int32_t route_pts[8] = {};
    long last = copy_tree_to_route(10, 2, route_pts, 8);
    CHECK(last == 2);
    CHECK(route_pts[0] == 10);
    CHECK(route_pts[1] == 5);
    CHECK(route_pts[2] == 2);
}

TEST_CASE_METHOD(ResetNavitree, "copy_tree_to_route returns 0 and just the end point when start equals end", "[kfx_pathfinding][ariadne_navitree]") {
    int32_t route_pts[4] = {};
    CHECK(copy_tree_to_route(3, 3, route_pts, 4) == 0);
    CHECK(route_pts[0] == 3);
}

TEST_CASE_METHOD(ResetNavitree, "copy_tree_to_route returns -1 when route_len is too small to hold the chain", "[kfx_pathfinding][ariadne_navitree]") {
    tree_dad[10] = 5;
    tree_dad[5] = 2;
    int32_t route_pts[2] = {};
    CHECK(copy_tree_to_route(10, 2, route_pts, 2) == -1);
}

TEST_CASE_METHOD(ResetNavitree, "update_border_tags stamps every in-range index and skips out-of-range ones", "[kfx_pathfinding][ariadne_navitree]") {
    int32_t border_pt[4] = {3, 7, -1, TREEITEMS_COUNT};
    long set_count = update_border_tags(42, border_pt, 4);
    CHECK(set_count == 2); // the two out-of-range entries are skipped
    CHECK(Tags[3] == 42);
    CHECK(Tags[7] == 42);
    CHECK(tag_current == 42); // set unconditionally, even though 2 of 4 were skipped
}

TEST_CASE_METHOD(ResetNavitree, "border_tags_to_current stamps using the live tag_current as the tag id", "[kfx_pathfinding][ariadne_navitree]") {
    tag_current = 9;
    int32_t border_pt[1] = {3};
    CHECK(border_tags_to_current(border_pt, 1) == 1);
    CHECK(Tags[3] == 9);
}
