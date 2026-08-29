// First kfx_render coverage, per docs/refactor/testing/
// stage-02-testability-and-fakes.md's rollout order: light_data.c's
// simplest accessors, using pattern A (stage-02 §2) with
// light_initialise() -- the module's own reset function, the same idiom
// kfx_pathfinding_utest's ariadne_points_test.cpp established -- plus
// direct field manipulation on the public lish.lights[] array for the
// one thing light_initialise() alone doesn't set up (an "allocated"
// light to probe). light_create_light()'s full allocation path (dynamic-
// light shadow cache, lighting-table interpolation, ...) is deliberately
// out of scope for this first pass.
#include <catch2/catch_test_macros.hpp>

#include "light_data.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct ResetLighting {
    ResetLighting() {
        light_initialise();
        // light_initialise() only frees shadow_cache slots reachable
        // through an allocated light (light_delete_light() -> _free()) --
        // it never scans lish.shadow_cache[] directly. A test that
        // allocates a shadow cache slot standalone (not through a real
        // light) leaves it allocated for every later test, so this
        // fixture clears it explicitly rather than relying on
        // light_initialise() to do it.
        std::memset(&lish.shadow_cache, 0, sizeof(lish.shadow_cache));
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
    }
};
}

TEST_CASE_METHOD(ResetLighting, "light_is_invalid rejects null and out-of-range lights", "[kfx_render][light_data]") {
    CHECK(light_is_invalid(nullptr));
    CHECK(light_is_invalid(&lish.lights[0])); // index 0 is the reserved sentinel
    CHECK_FALSE(light_is_invalid(&lish.lights[1]));
    CHECK_FALSE(light_is_invalid(&lish.lights[LIGHTS_COUNT - 1]));
}

TEST_CASE_METHOD(ResetLighting, "light_get_light_radius/light_set_light_radius round-trip", "[kfx_render][light_data]") {
    light_set_light_radius(1, 500);
    CHECK(light_get_light_radius(1) == 500);
}

TEST_CASE_METHOD(ResetLighting, "light_is_light_allocated reflects the Allocated flag", "[kfx_render][light_data]") {
    CHECK_FALSE(light_is_light_allocated(1));
    // .index must be set alongside .flags here, not just the flag alone:
    // light_initialise()'s own cleanup loop identifies which slot to
    // delete via lgt->index, not the loop variable -- a light with
    // LgtF_Allocated set but a stale/zero .index survives every future
    // light_initialise() call (light_delete_light(0) early-returns on
    // its idx<=0 guard), permanently polluting every later test in this
    // binary. Found by a later test in this file failing intermittently
    // by allocating index 2 instead of 1 the first time this test ran
    // before it.
    lish.lights[1].flags |= LgtF_Allocated;
    lish.lights[1].index = 1;
    CHECK(light_is_light_allocated(1));
}

TEST_CASE_METHOD(ResetLighting, "light_is_light_allocated rejects out-of-range indices", "[kfx_render][light_data]") {
    CHECK_FALSE(light_is_light_allocated(0));
    CHECK_FALSE(light_is_light_allocated(LIGHTS_COUNT));
}

// light_allocate_light/_free_light/_count_lights: the free-list-free
// linear-scan allocator over lish.lights[] -- same "allocate, dispose,
// reallocate" shape as kfx_pathfinding's tri_new/tri_dispose, just a
// scan instead of a free list (this array doesn't use one).

TEST_CASE_METHOD(ResetLighting, "light_allocate_light returns the first unallocated slot and marks it allocated", "[kfx_render][light_data]") {
    struct Light *lgt = light_allocate_light();
    REQUIRE(lgt != nullptr);
    CHECK(lgt == &lish.lights[1]); // index 0 is the reserved sentinel
    CHECK(light_is_light_allocated(1));
    CHECK(lgt->index == 1);
}

TEST_CASE_METHOD(ResetLighting, "light_count_lights reflects the number of allocated lights", "[kfx_render][light_data]") {
    CHECK(light_count_lights() == 0);
    light_allocate_light();
    light_allocate_light();
    CHECK(light_count_lights() == 2);
}

TEST_CASE_METHOD(ResetLighting, "light_free_light clears the slot so it can be allocated again", "[kfx_render][light_data]") {
    struct Light *first = light_allocate_light();
    light_free_light(first);
    CHECK(light_count_lights() == 0);

    struct Light *reused = light_allocate_light();
    CHECK(reused == first); // the freed slot is the first unallocated one again
}

// light_get_light_intensity/_set_light_intensity: unlike
// light_get_light_radius (a bare field read, no check at all), the
// intensity getter refuses to read an unallocated light -- a real
// asymmetry between two similarly-shaped accessors, confirmed by
// reading both bodies rather than assumed from the shared naming
// pattern.

TEST_CASE_METHOD(ResetLighting, "light_get_light_intensity refuses to read an unallocated light, unlike light_get_light_radius", "[kfx_render][light_data]") {
    CHECK(light_get_light_intensity(1) == 0); // slot 1 not allocated yet
}

TEST_CASE_METHOD(ResetLighting, "light_get_light_intensity/light_set_light_intensity round-trip on an allocated light", "[kfx_render][light_data]") {
    struct Light *lgt = light_allocate_light();
    light_set_light_intensity(lgt->index, 200);
    CHECK(light_get_light_intensity(lgt->index) == 200);
}

// light_allocate_shadow_cache/_shadow_cache_invalid/_index/_free: the
// dynamic-light shadow cache's own linear-scan allocator, the same
// shape as lish.lights[] but a separate array and a different bounds
// check (light_shadow_cache_index recovers the slot number via pointer
// arithmetic, not a stored field).

TEST_CASE_METHOD(ResetLighting, "light_allocate_shadow_cache returns the first unallocated slot", "[kfx_render][light_data]") {
    struct ShadowCache *shdc = light_allocate_shadow_cache();
    REQUIRE(shdc != nullptr);
    CHECK(shdc == &lish.shadow_cache[1]); // index 0 is the reserved sentinel
    CHECK_FALSE(light_shadow_cache_invalid(shdc));
}

TEST_CASE_METHOD(ResetLighting, "light_shadow_cache_invalid rejects null and out-of-range slots", "[kfx_render][light_data]") {
    CHECK(light_shadow_cache_invalid(nullptr));
    CHECK(light_shadow_cache_invalid(&lish.shadow_cache[0])); // reserved sentinel
    CHECK_FALSE(light_shadow_cache_invalid(&lish.shadow_cache[1]));
}

TEST_CASE_METHOD(ResetLighting, "light_shadow_cache_index recovers the slot number from a pointer", "[kfx_render][light_data]") {
    struct ShadowCache *shdc = light_allocate_shadow_cache();
    CHECK(light_shadow_cache_index(shdc) == 1);
}

TEST_CASE_METHOD(ResetLighting, "light_shadow_cache_free clears the slot so it can be allocated again", "[kfx_render][light_data]") {
    struct ShadowCache *first = light_allocate_shadow_cache();
    light_shadow_cache_free(first);
    struct ShadowCache *reused = light_allocate_shadow_cache();
    CHECK(reused == first);
}
