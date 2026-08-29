// kfx_net: packets.c, per docs/refactor/testing/comprehensive/
// stage-08-comprehensive-library-passes.md's kfx_net row -- pattern-A
// candidates in the same "construct a value, assert a property" shape
// as net_checksums_test.cpp's get_thing_checksum tests. set_packet_action
// and is_packet_empty are both pure struct Packet field operations, no
// kfx_net_state/player lookup involved.
#include <catch2/catch_test_macros.hpp>

#include "packets.h"

#include <cstring>

namespace {
struct ZeroedPacket {
    ZeroedPacket() { std::memset(&pckt, 0, sizeof(pckt)); }
    struct Packet pckt;
};
}

TEST_CASE_METHOD(ZeroedPacket, "set_packet_action writes the action kind and all four parameters", "[kfx_net][packets]") {
    set_packet_action(&pckt, 7, 111, 222, 333, 444);
    CHECK(pckt.action == 7);
    CHECK(pckt.actn_par1 == 111);
    CHECK(pckt.actn_par2 == 222);
    CHECK(pckt.actn_par3 == 333);
    CHECK(pckt.actn_par4 == 444);
}

TEST_CASE_METHOD(ZeroedPacket, "set_packet_action leaves fields it doesn't own untouched", "[kfx_net][packets]") {
    pckt.turn = 99;
    pckt.pos_x = 5;
    pckt.pos_y = 6;
    set_packet_action(&pckt, 1, 0, 0, 0, 0);
    CHECK(pckt.turn == 99);
    CHECK(pckt.pos_x == 5);
    CHECK(pckt.pos_y == 6);
}

TEST_CASE_METHOD(ZeroedPacket, "is_packet_empty is true for an all-zero packet", "[kfx_net][packets]") {
    CHECK(is_packet_empty(&pckt));
}

TEST_CASE_METHOD(ZeroedPacket, "is_packet_empty is false once any checked field is non-zero", "[kfx_net][packets]") {
    pckt.action = 1;
    CHECK_FALSE(is_packet_empty(&pckt));
}

TEST_CASE_METHOD(ZeroedPacket, "is_packet_empty checks every field set_packet_action can write", "[kfx_net][packets]") {
    // Each of these alone should be enough to flip is_packet_empty false --
    // regression coverage for is_packet_empty's field list drifting out of
    // sync with struct Packet (e.g. a new field added to one but not the
    // other).
    pckt.actn_par1 = 1;
    CHECK_FALSE(is_packet_empty(&pckt));
    pckt.actn_par1 = 0;

    pckt.actn_par2 = 1;
    CHECK_FALSE(is_packet_empty(&pckt));
    pckt.actn_par2 = 0;

    pckt.actn_par3 = 1;
    CHECK_FALSE(is_packet_empty(&pckt));
    pckt.actn_par3 = 0;

    pckt.actn_par4 = 1;
    CHECK_FALSE(is_packet_empty(&pckt));
    pckt.actn_par4 = 0;

    CHECK(is_packet_empty(&pckt)); // back to all-zero
}

TEST_CASE_METHOD(ZeroedPacket, "is_packet_empty also checks input_lag_turns, not just the action fields", "[kfx_net][packets]") {
    pckt.input_lag_turns = 1;
    CHECK_FALSE(is_packet_empty(&pckt));
}
