// kfx_game: game_heap.c. setup_heap_manager() opens a real
// "creature.jty" data file, which doesn't exist in the unit-test
// environment (no game data files present) -- LbFileOpen(...,
// Lb_FILE_MODE_READ_ONLY) returns NULL for a missing file, so this
// exercises the real "can't open JTY file" failure path, not a fake.
// he_alloc() is a bare malloc() wrapper. reset_heap_manager() calls
// LbFileClose() unconditionally on jty_file_handle, which starts NULL
// in this binary (setup_heap_manager() never successfully ran) --
// fclose(NULL) is undefined behavior per the C standard, so it's not
// attempted here rather than relying on glibc's specific (non-crashing)
// handling of it.
#include <catch2/catch_test_macros.hpp>

#include "game_heap.h"

#include <cstdlib>

TEST_CASE("setup_heap_manager fails when creature.jty doesn't exist in the test environment", "[kfx_game][game_heap]") {
    CHECK_FALSE(setup_heap_manager());
}

TEST_CASE("he_alloc returns a usable block of the requested size", "[kfx_game][game_heap]") {
    void *p = he_alloc(64);
    REQUIRE(p != nullptr);
    // Writable, since it's a real malloc'd block.
    static_cast<unsigned char*>(p)[0] = 0xAB;
    static_cast<unsigned char*>(p)[63] = 0xCD;
    CHECK(static_cast<unsigned char*>(p)[0] == 0xAB);
    free(p);
}
