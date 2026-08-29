// kfx_pathfinding: ariadne_naviheap.c, per docs/refactor/testing/
// stage-04b-kfx-pathfinding.md's "reasonable next candidates" list.
// Scoped to capacity/empty-state mechanics only -- the heap's actual
// priority ordering is driven by naviheap_item_tree_val(), which reads
// ariadne_navitree's tree_val[] array through naviheap_get(), state this
// pass doesn't set up. Deliberately not tested here; a reasonable next
// increment once ariadne_navitree.c itself has a test fixture.
#include <catch2/catch_test_macros.hpp>

#include "ariadne_naviheap.h"

namespace {
struct ResetNaviheap {
    ResetNaviheap() { naviheap_init(); }
};
}

TEST_CASE_METHOD(ResetNaviheap, "naviheap_empty reflects whether anything has been added", "[kfx_pathfinding][ariadne_naviheap]") {
    CHECK(naviheap_empty());
    REQUIRE(naviheap_add(1));
    CHECK_FALSE(naviheap_empty());
}

TEST_CASE_METHOD(ResetNaviheap, "naviheap_remove returns -1 on an empty heap", "[kfx_pathfinding][ariadne_naviheap]") {
    CHECK(naviheap_remove() == -1);
}

TEST_CASE_METHOD(ResetNaviheap, "naviheap_add rejects once the heap is full", "[kfx_pathfinding][ariadne_naviheap]") {
    // PATH_HEAP_LEN - 1 usable slots (index 0 unused, one more element
    // deliberately left free -- see naviheap_add()'s own comment).
    bool all_accepted = true;
    for (long i = 0; i < PATH_HEAP_LEN - 1; i++) {
        all_accepted = all_accepted && naviheap_add(i);
    }
    CHECK(all_accepted);
    CHECK_FALSE(naviheap_add(9999)); // one past capacity
}
