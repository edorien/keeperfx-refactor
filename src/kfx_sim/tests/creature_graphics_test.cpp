// kfx_sim: creature_graphics.c's keepersprite_* dispatch functions --
// targeted per the user's request to focus coverage around
// scripts/check_layering_symbols.py's ACCEPTED (architecture.md §8.2)
// kfx_sim -> kfx_render residual: creature_table_add[], declared here in
// kfx_sim's own creature_graphics.h ("a higher-ranked library
// implementing a lower-ranked interface is fine") but really *populated*
// by kfx_render's custom_sprites.c. Every keepersprite_* function
// dispatches between two data sources by frame-number range:
// creature_table_add[] (the residual, for n in
// [SIM_KEEPERSPRITE_ADD_OFFSET, +SIM_KEEPERSPRITE_ADD_NUM)) or
// creature_list[]/creature_table[] (kfx_sim's own, for n <
// CREATURE_FRAMELIST_LENGTH).
//
// kfx_sim_utest links kfx_sim_test_stubs.cpp's creature_table_add[1] --
// a real, writable array, just sized 1 instead of the production
// SIM_KEEPERSPRITE_ADD_NUM (16383, kfx_render/engine_render.h -- above
// kfx_sim on the ladder, so this stub deliberately doesn't reference
// it). SIM_KEEPERSPRITE_ADD_OFFSET (16384) is used here as a literal,
// matching creature_graphics.c's own #define, since it's a same-file
// implementation constant, never exposed via a header (unlike the
// several missing-*function*-declarations fixed elsewhere in this
// plan, hardcoding an already-private numeric constant isn't the same
// kind of gap). Every test below reads/writes only
// creature_table_add[0] -- the one slot this stub actually has room
// for; never a second index.
#include <catch2/catch_test_macros.hpp>

#include "creature_graphics.h"

#include <cstring>

namespace {
constexpr unsigned short kAddOffset = 16384; // SIM_KEEPERSPRITE_ADD_OFFSET

struct ResetCreatureTableAdd {
    ResetCreatureTableAdd() {
        std::memset(&creature_table_add[0], 0, sizeof(creature_table_add[0]));
        creature_table_length = 0; // creature_table[]/creature_list[]'s own data source stays "not loaded"
    }
};
}

TEST_CASE_METHOD(ResetCreatureTableAdd, "keepersprite_frames/_rotable/_array route an in-range frame number to creature_table_add", "[kfx_sim][creature_graphics]") {
    creature_table_add[0].FramesCount = 12;
    creature_table_add[0].Rotable = 1;

    CHECK(keepersprite_frames(kAddOffset) == 12);
    CHECK(keepersprite_rotable(kAddOffset) == 1);
    CHECK(keepersprite_array(kAddOffset) == &creature_table_add[0]);
}

TEST_CASE("keepersprite_index returns the frame number unchanged when it's in the creature_table_add range", "[kfx_sim][creature_graphics]") {
    CHECK(keepersprite_index(kAddOffset) == kAddOffset);
}

TEST_CASE("keepersprite_frames/_rotable/_array/_index all report the out-of-range fallback for a frame number in neither data source", "[kfx_sim][creature_graphics]") {
    // Between CREATURE_FRAMELIST_LENGTH (982) and SIM_KEEPERSPRITE_ADD_OFFSET
    // (16384) -- genuinely out of range for both dispatch targets, no
    // array access happens on either side.
    const unsigned short n = 5000;
    CHECK(keepersprite_frames(n) == 0);
    CHECK(keepersprite_rotable(n) == 0);
    CHECK(keepersprite_array(n) == nullptr);
    CHECK(keepersprite_index(n) == 0);
}

TEST_CASE("keepersprite_frames falls back to 0 for an in-range creature_list index when the real creature table isn't loaded", "[kfx_sim][creature_graphics]") {
    // creature_table_length defaults to 0 (nothing loaded in a test
    // environment) -- creature_list[]'s own lookup entries are real,
    // baked-in production data, but the "i < creature_table_length"
    // guard rejects every one of them until a real table is populated,
    // so this never dereferences the (also default-null) creature_table
    // pointer.
    CHECK(keepersprite_frames(5) == 0);
}
