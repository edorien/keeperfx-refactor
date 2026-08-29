// kfx_sim "thing" cluster, per docs/refactor/testing/comprehensive/
// stage-08-comprehensive-library-passes.md §3: thing_is_invalid/
// thing_exists/thing_get are the highest fan-in-per-effort target in
// this cluster -- already exercised *indirectly* by kfx_net's
// net_checksums_test.cpp (get_thing_checksum calls thing_exists), but
// never tested directly until now. Pattern A on kfx_sim_state, same
// memset fixture used throughout.
#include <catch2/catch_test_macros.hpp>

#include "thing_data.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct ResetSimState {
    ResetSimState() { std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state)); }
};
}

TEST_CASE_METHOD(ResetSimState, "thing_is_invalid rejects null and the reserved index-0 sentinel", "[kfx_sim][thing_data]") {
    CHECK(thing_is_invalid(nullptr));
    CHECK(thing_is_invalid(thing_get(0))); // index 0 is INVALID_THING itself
}

TEST_CASE_METHOD(ResetSimState, "thing_is_invalid accepts an in-range slot", "[kfx_sim][thing_data]") {
    CHECK_FALSE(thing_is_invalid(thing_get(1)));
    CHECK_FALSE(thing_is_invalid(thing_get(THINGS_COUNT - 1)));
}

TEST_CASE_METHOD(ResetSimState, "thing_get returns the reserved sentinel for an out-of-range index", "[kfx_sim][thing_data]") {
    CHECK(thing_get(0) == thing_get(0)); // sanity: INVALID_THING is stable
    CHECK(thing_is_invalid(thing_get(THINGS_COUNT))); // one past the last valid slot
}

TEST_CASE_METHOD(ResetSimState, "thing_exists is false until TAlF_Exists is set on a valid slot", "[kfx_sim][thing_data]") {
    struct Thing *thing = thing_get(1);
    CHECK_FALSE(thing_exists(thing));

    thing->alloc_flags |= TAlF_Exists;
    CHECK(thing_exists(thing));
}

TEST_CASE_METHOD(ResetSimState, "thing_exists is false for an invalid thing regardless of its flags", "[kfx_sim][thing_data]") {
    CHECK_FALSE(thing_exists(nullptr));
    CHECK_FALSE(thing_exists(thing_get(0)));
}
