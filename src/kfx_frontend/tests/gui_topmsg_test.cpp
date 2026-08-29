// First kfx_frontend coverage, per docs/refactor/testing/
// stage-02-testability-and-fakes.md's rollout order (lowest priority of
// the "normal" libraries, most pattern-C-heavy by file count -- most of
// gui_topmsg.c's neighbors in this library are GUI-state/rendering-
// coupled the way stage-02 §4 predicted). erstat_inc()'s error-statistics
// counter is the exception: a small, self-contained module-static array
// with its own reset function (erstats_clear()) -- pattern A, same idiom
// as ariadne_points.c/light_data.c in earlier stages.
#include <catch2/catch_test_macros.hpp>

#include "gui_topmsg.h"

namespace {
struct ResetErrorStats {
    ResetErrorStats() { erstats_clear(); }
};
}

TEST_CASE_METHOD(ResetErrorStats, "erstat_inc returns the count of new occurrences since the last flush", "[kfx_frontend][gui_topmsg]") {
    CHECK(erstat_inc(0) == 1);
    CHECK(erstat_inc(0) == 2);
    CHECK(erstat_inc(0) == 3);
}

TEST_CASE_METHOD(ResetErrorStats, "erstat_inc tracks stats independently", "[kfx_frontend][gui_topmsg]") {
    CHECK(erstat_inc(0) == 1);
    CHECK(erstat_inc(1) == 1);
    CHECK(erstat_inc(0) == 2);
}

TEST_CASE_METHOD(ResetErrorStats, "erstat_inc rejects an out-of-range stat_num without side effects", "[kfx_frontend][gui_topmsg]") {
    CHECK(erstat_inc(-1) == 1);
    CHECK(erstat_inc(99999) == 1);
    // Neither out-of-range call should have touched a real slot.
    CHECK(erstat_inc(0) == 1);
}
