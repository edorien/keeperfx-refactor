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
#include "kfx_net_state.h"
#include "net_game.h" // net_player_info[]/MAX_NET_USERS, for checksums_different's network_player_active check

#include <cstring>

namespace {
struct ResetSimState {
    ResetSimState() { std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state)); }
};

// checksums_different() also touches kfx_net_state (packets) and
// net_player_info (network_player_active); get_host_player_id() is
// hardcoded to 0, so kfx_sim_state.players[0] is always "the host" here.
struct ResetChecksumState {
    ResetChecksumState() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::memset(&kfx_net_state, 0, sizeof(kfx_net_state));
        std::memset(net_player_info, 0, sizeof(net_player_info));
        kfx_sim_state.players[0].packet_num = 0;
    }

    // Makes player i (1 <= i < MAX_NET_USERS) an active, non-computer
    // network client with its own packet slot equal to its player index.
    void make_active_client(int i) {
        kfx_sim_state.players[i].allocflags |= PlaF_Allocated;
        kfx_sim_state.players[i].packet_num = i;
        net_player_info[i].network_user_active = 1;
    }
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

TEST_CASE_METHOD(ResetChecksumState, "checksums_different is false when no other player has an active network packet to compare", "[kfx_net][net_checksums]") {
    kfx_net_state.packets[0].checksum = 0xAABBCCDD;
    CHECK_FALSE(checksums_different());
}

TEST_CASE_METHOD(ResetChecksumState, "checksums_different is false when a client's checksum matches the host's", "[kfx_net][net_checksums]") {
    kfx_net_state.packets[0].checksum = 0xAABBCCDD;
    make_active_client(1);
    kfx_net_state.packets[1].checksum = 0xAABBCCDD;
    kfx_net_state.packets[1].action = 1; // non-empty, so is_packet_empty() doesn't short-circuit to "missing"
    CHECK_FALSE(checksums_different());
}

TEST_CASE_METHOD(ResetChecksumState, "checksums_different is true when a client's checksum differs from the host's", "[kfx_net][net_checksums]") {
    kfx_net_state.packets[0].checksum = 0xAABBCCDD;
    make_active_client(1);
    kfx_net_state.packets[1].checksum = 0x11223344;
    kfx_net_state.packets[1].action = 1;
    CHECK(checksums_different());
}

TEST_CASE_METHOD(ResetChecksumState, "checksums_different is true when an active client's checksum packet is entirely empty", "[kfx_net][net_checksums]") {
    kfx_net_state.packets[0].checksum = 0xAABBCCDD;
    make_active_client(1); // packets[1] left fully zeroed -- is_packet_empty() is true
    CHECK(checksums_different());
}

TEST_CASE_METHOD(ResetChecksumState, "checksums_different skips a player marked computer-controlled (PlaF_CompCtrl)", "[kfx_net][net_checksums]") {
    kfx_net_state.packets[0].checksum = 0xAABBCCDD;
    make_active_client(1);
    kfx_sim_state.players[1].allocflags |= PlaF_CompCtrl;
    kfx_net_state.packets[1].checksum = 0x11223344; // would mismatch, but the player is skipped entirely
    kfx_net_state.packets[1].action = 1;
    CHECK_FALSE(checksums_different());
}

TEST_CASE_METHOD(ResetChecksumState, "checksums_different skips a player whose network slot isn't active", "[kfx_net][net_checksums]") {
    kfx_net_state.packets[0].checksum = 0xAABBCCDD;
    kfx_sim_state.players[1].allocflags |= PlaF_Allocated;
    kfx_sim_state.players[1].packet_num = 1;
    // net_player_info[1].network_user_active left at 0 -- not an active network slot.
    kfx_net_state.packets[1].checksum = 0x11223344;
    kfx_net_state.packets[1].action = 1;
    CHECK_FALSE(checksums_different());
}
