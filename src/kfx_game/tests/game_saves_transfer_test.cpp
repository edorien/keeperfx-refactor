// kfx_game: game_saves.c's inter-level creature-transfer bookkeeping
// (add_transfered_creature/get_transferred_creature/
// clear_transfered_creatures) -- pure array management over
// game_merge.h's intralvl global plus kfx_sim's real get_dungeon()/
// dungeon_invalid() (both index kfx_sim_state.dungeon[] directly, no
// allocation-flag gate like players have, so any in-range PlayerNumber
// resolves to a valid dungeon with zero extra fixture work).
//
// Everything else in this file (save_game_chunks/load_game_chunks/
// save_game/load_game/the save-catalogue disk scan, ...) does real file
// I/O against a save-game format and isn't attempted here.
// is_primitive_save_version() was also considered and declined: its
// threshold is a pointer-difference between two unrelated globals
// (`game`/`kfx_sim_state.loaded_level_number`), an inherently unstable
// value to assert on (depends on link layout, not a real invariant).
#include <catch2/catch_test_macros.hpp>

#include "game_saves.h"
#include "game_merge.h" // intralvl
#include "dungeon_data.h" // get_dungeon, DUNGEONS_COUNT
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct ResetTransferState {
    ResetTransferState() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::memset(&intralvl, 0, sizeof(intralvl));
    }
};
}

TEST_CASE_METHOD(ResetTransferState, "add_transfered_creature stores at the dungeon's own creatures_transferred slot", "[kfx_game][game_saves]") {
    kfx_sim_state.dungeon[2].creatures_transferred = 0;
    CHECK(add_transfered_creature(2, 7, 3, const_cast<char*>("Imp")));
    CHECK(intralvl.transferred_creatures[2][0].model == 7);
    CHECK(intralvl.transferred_creatures[2][0].exp_level == 3);
    CHECK(std::strcmp(intralvl.transferred_creatures[2][0].creature_name, "Imp") == 0);
}

TEST_CASE_METHOD(ResetTransferState, "add_transfered_creature rejects an out-of-range player/dungeon index", "[kfx_game][game_saves]") {
    CHECK_FALSE(add_transfered_creature(DUNGEONS_COUNT, 7, 3, const_cast<char*>("Imp")));
    CHECK_FALSE(add_transfered_creature(-1, 7, 3, const_cast<char*>("Imp")));
}

TEST_CASE_METHOD(ResetTransferState, "get_transferred_creature round-trips a stored creature's model/exp_level/name", "[kfx_game][game_saves]") {
    kfx_sim_state.dungeon[1].creatures_transferred = 0;
    add_transfered_creature(1, 9, 5, const_cast<char*>("Dragon"));

    ThingModel model = 0;
    CrtrExpLevel exp_level = 0;
    char name_buf[64] = {0};
    CHECK(get_transferred_creature(1, 0, &model, &exp_level, name_buf, sizeof(name_buf)));
    CHECK(model == 9);
    CHECK(exp_level == 5);
    CHECK(std::strcmp(name_buf, "Dragon") == 0);
}

TEST_CASE_METHOD(ResetTransferState, "get_transferred_creature is false for an empty (never-written) slot", "[kfx_game][game_saves]") {
    ThingModel model = 0;
    CrtrExpLevel exp_level = 0;
    char name_buf[64] = {0};
    CHECK_FALSE(get_transferred_creature(3, 0, &model, &exp_level, name_buf, sizeof(name_buf)));
}

TEST_CASE_METHOD(ResetTransferState, "clear_transfered_creatures empties every player's every slot", "[kfx_game][game_saves]") {
    kfx_sim_state.dungeon[0].creatures_transferred = 0;
    add_transfered_creature(0, 4, 1, const_cast<char*>("Fly"));

    clear_transfered_creatures();

    ThingModel model = 0;
    CrtrExpLevel exp_level = 0;
    char name_buf[64] = {0};
    CHECK_FALSE(get_transferred_creature(0, 0, &model, &exp_level, name_buf, sizeof(name_buf)));
}
