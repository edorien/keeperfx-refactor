// First kfx_net coverage, per docs/refactor/testing/
// stage-02-testability-and-fakes.md's rollout order: get_thing_checksum
// is a pure per-Thing rolling checksum, the strong pattern-A fit that
// stage-02 predicted for this library's packet/checksum code. Pattern A
// on kfx_sim_state (stage-02 §2's memset fixture), reused from a kfx_net
// context -- kfx_net legitimately depends on kfx_sim.
//
// Deliberately property-based rather than asserting a specific magic
// checksum number: CHECKSUM_ADD's rolling-hash internals
// (net_checksums.c) aren't part of this function's documented contract,
// only "deterministic, and sensitive to the fields it reads" is -- and a
// hard-coded expected value would just be a second, easier-to-get-wrong
// copy of the implementation (the same near-miss risk flagged in
// stage-04b/04c, applied here as "don't assert what you'd have to
// reimplement to check").
#include <catch2/catch_test_macros.hpp>

#include "net_checksums.h"
#include "thing_data.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct ResetSimState {
    ResetSimState() { std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state)); }
};

// Index 0 is thing_is_invalid()'s reserved sentinel (thing_data.c), same
// convention as ari_Points[0]/lish.lights[0] in earlier stages.
struct Thing *make_existing_thing(ThingIndex idx, unsigned char class_id)
{
    struct Thing *thing = thing_get(idx);
    thing->index = idx;
    thing->class_id = class_id;
    thing->alloc_flags |= TAlF_Exists;
    return thing;
}
}

TEST_CASE_METHOD(ResetSimState, "get_thing_checksum is zero for a non-existent thing", "[kfx_net][net_checksums]") {
    struct Thing *thing = thing_get(1); // alloc_flags never set -- thing_exists() is false
    CHECK(get_thing_checksum(thing) == 0);
}

TEST_CASE_METHOD(ResetSimState, "get_thing_checksum is zero for a non-synchronized thing class", "[kfx_net][net_checksums]") {
    struct Thing *thing = make_existing_thing(1, TCls_EffectElem);
    CHECK(get_thing_checksum(thing) == 0);
}

TEST_CASE_METHOD(ResetSimState, "get_thing_checksum is deterministic for the same thing state", "[kfx_net][net_checksums]") {
    struct Thing *thing = make_existing_thing(1, TCls_Object);
    thing->owner = 2;
    thing->health = 100;

    TbBigChecksum first = get_thing_checksum(thing);
    TbBigChecksum second = get_thing_checksum(thing);
    CHECK(first == second);
    CHECK(first != 0);
}

TEST_CASE_METHOD(ResetSimState, "get_thing_checksum changes when a checksummed field changes", "[kfx_net][net_checksums]") {
    struct Thing *thing = make_existing_thing(1, TCls_Object);
    thing->owner = 2;

    TbBigChecksum before = get_thing_checksum(thing);
    thing->owner = 3;
    TbBigChecksum after = get_thing_checksum(thing);
    CHECK(before != after);
}
