// kfx_sim "room" cluster, per docs/refactor/testing/comprehensive/
// stage-08-comprehensive-library-passes.md §3: room_data.c's own
// accessors follow the same "index 0 reserved sentinel" family as
// thing_data.c/creature_control.c (room_is_invalid/room_exists/
// room_get), plus compute_room_max_health() -- pattern A on
// kfx_config_state (reads conf.rules[0].workers.hits_per_slab) combined
// with saturate_set_unsigned(), the same EmulateIntegerOverflowFunc
// pattern-B target already tested directly in kfx_platform/tests/
// bflib_basics_test.cpp -- here exercised indirectly through its real
// caller instead, with the default (non-emulating) provider in effect.
#include <catch2/catch_test_macros.hpp>

#include "room_data.h"
#include "kfx_sim_state.h"
#include "kfx_config_state.h"

#include <cstring>

namespace {
struct ResetSimState {
    ResetSimState() { std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state)); }
};
}

TEST_CASE_METHOD(ResetSimState, "room_is_invalid rejects null and the reserved index-0 sentinel", "[kfx_sim][room_data]") {
    CHECK(room_is_invalid(nullptr));
    CHECK(room_is_invalid(room_get(0)));
    CHECK(room_is_invalid(INVALID_ROOM));
}

TEST_CASE_METHOD(ResetSimState, "room_is_invalid accepts an in-range slot", "[kfx_sim][room_data]") {
    CHECK_FALSE(room_is_invalid(room_get(1)));
}

TEST_CASE_METHOD(ResetSimState, "room_get returns the sentinel for an out-of-range index", "[kfx_sim][room_data]") {
    CHECK(room_get(0) == INVALID_ROOM);
    CHECK(room_get(ROOMS_COUNT + 1) == INVALID_ROOM);
}

TEST_CASE_METHOD(ResetSimState, "room_exists is false until RoF_Allocated is set", "[kfx_sim][room_data]") {
    struct Room *room = room_get(1);
    CHECK_FALSE(room_exists(room));

    room->alloc_flags |= RoF_Allocated;
    CHECK(room_exists(room));
}

TEST_CASE_METHOD(ResetSimState, "compute_room_max_health multiplies hits_per_slab by slab count", "[kfx_sim][room_data]") {
    std::memset(&kfx_config_state, 0, sizeof(kfx_config_state));
    kfx_config_state.conf.rules[0].workers.hits_per_slab = 200;
    CHECK(compute_room_max_health(100, 0) == 20000); // 200 * 100, well under the 16-bit saturation limit
}

TEST_CASE_METHOD(ResetSimState, "compute_room_max_health saturates at the 16-bit limit", "[kfx_sim][room_data]") {
    std::memset(&kfx_config_state, 0, sizeof(kfx_config_state));
    kfx_config_state.conf.rules[0].workers.hits_per_slab = 250;
    CHECK(compute_room_max_health(300, 0) == 65535); // 250 * 300 = 75000, clamped
}
