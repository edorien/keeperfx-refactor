// kfx_sim coverage: a first pass over room_library.c's research-list
// bookkeeping -- Dungeon::research[] is a plain array (append-only via
// add_research_to_player, scanned linearly by update_players_research_amount)
// gated by research_num, with research_needed's three-category switch
// (power/room/creature) reading three different per-dungeon "is
// available" arrays depending on category.
//
// Deliberately deferred: everything else in this 840-line file
// (create_spell_in_library/remove_spell_from_library/
// update_library_object_pickup_event, reposition_all_books_in_room_*,
// process_player_research/update_research/send_research_complete_event)
// -- room/thing-list traversal and event-dispatch heavy, genuinely the
// hard tail.
#include <catch2/catch_test_macros.hpp>

#include "room_library.h"
#include "dungeon_data.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "remove_all_research_from_player/research_overriden_for_player/clear_research_for_all_players manage the override flag and research count", "[kfx_sim][room_library]") {
    struct Dungeon *dungeon = get_dungeon(0);
    dungeon->research_num = 5;
    CHECK_FALSE(research_overriden_for_player(0));

    CHECK(remove_all_research_from_player(0));
    CHECK(dungeon->research_num == 0);
    CHECK(research_overriden_for_player(0));

    CHECK(clear_research_for_all_players());
    CHECK_FALSE(research_overriden_for_player(0)); // clear resets the override too
    CHECK(dungeon->research_num == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "add_research_to_player appends to the array and refuses once DUNGEON_RESEARCH_COUNT is reached", "[kfx_sim][room_library]") {
    struct Dungeon *dungeon = get_dungeon(0);
    CHECK(add_research_to_player(0, RsCat_Power, 3, 100));
    CHECK(dungeon->research_num == 1);
    CHECK(dungeon->research[0].rtyp == RsCat_Power);
    CHECK(dungeon->research[0].rkind == 3);
    CHECK(dungeon->research[0].req_amount == 100);

    dungeon->research_num = DUNGEON_RESEARCH_COUNT;
    CHECK_FALSE(add_research_to_player(0, RsCat_Power, 3, 100));
    CHECK(dungeon->research_num == DUNGEON_RESEARCH_COUNT); // unchanged
}

TEST_CASE_METHOD(ResetSimAndConfig, "add_research_to_all_players adds the same entry to every player's dungeon", "[kfx_sim][room_library]") {
    CHECK(add_research_to_all_players(RsCat_Room, 7, 50));
    for (PlayerNumber i = 0; i < PLAYERS_COUNT; i++)
    {
        struct Dungeon *dungeon = get_dungeon(i);
        CHECK(dungeon->research_num == 1);
        CHECK(dungeon->research[0].rkind == 7);
    }
}

TEST_CASE_METHOD(ResetSimAndConfig, "update_players_research_amount rewrites req_amount for every matching entry and reports whether any matched", "[kfx_sim][room_library]") {
    add_research_to_player(0, RsCat_Power, 3, 100);
    add_research_to_player(0, RsCat_Room, 3, 200); // same rkind, different rtyp: must not match

    CHECK(update_players_research_amount(0, RsCat_Power, 3, 999));
    CHECK(get_dungeon(0)->research[0].req_amount == 999);
    CHECK(get_dungeon(0)->research[1].req_amount == 200); // untouched

    CHECK_FALSE(update_players_research_amount(0, RsCat_Creature, 3, 1)); // nothing of that category exists
}

TEST_CASE_METHOD(ResetSimAndConfig, "update_or_add_players_research_amount updates an existing entry, or appends a new one", "[kfx_sim][room_library]") {
    struct Dungeon *dungeon = get_dungeon(0);
    CHECK(update_or_add_players_research_amount(0, RsCat_Power, 3, 100));
    CHECK(dungeon->research_num == 1); // no existing entry: appended

    CHECK(update_or_add_players_research_amount(0, RsCat_Power, 3, 200));
    CHECK(dungeon->research_num == 1); // existing entry updated in place, not duplicated
    CHECK(dungeon->research[0].req_amount == 200);
}

TEST_CASE_METHOD(ResetSimAndConfig, "research_needed is false outright once research_num is 0, regardless of category", "[kfx_sim][room_library]") {
    struct Dungeon *dungeon = get_dungeon(0);
    dungeon->research_num = 0;
    dungeon->magic_resrchable[3] = 1;

    struct ResearchVal rsrchval = {};
    rsrchval.rtyp = RsCat_Power;
    rsrchval.rkind = 3;
    CHECK_FALSE(research_needed(&rsrchval, dungeon));
}

TEST_CASE_METHOD(ResetSimAndConfig, "research_needed for RsCat_Power requires the power to be researchable and not yet learned", "[kfx_sim][room_library]") {
    struct Dungeon *dungeon = get_dungeon(0);
    dungeon->research_num = 1;
    struct ResearchVal rsrchval = {};
    rsrchval.rtyp = RsCat_Power;
    rsrchval.rkind = 3;

    CHECK_FALSE(research_needed(&rsrchval, dungeon)); // not researchable yet

    dungeon->magic_resrchable[3] = 1;
    CHECK(research_needed(&rsrchval, dungeon));

    dungeon->magic_level[3] = 1; // already learned
    CHECK_FALSE(research_needed(&rsrchval, dungeon));
}

TEST_CASE_METHOD(ResetSimAndConfig, "research_needed for RsCat_Room follows the room_resrchable tri-state (never/instant/after-capture)", "[kfx_sim][room_library]") {
    struct Dungeon *dungeon = get_dungeon(0);
    dungeon->research_num = 1;
    struct ResearchVal rsrchval = {};
    rsrchval.rtyp = RsCat_Room;
    rsrchval.rkind = 7;

    CHECK_FALSE(research_needed(&rsrchval, dungeon)); // room_resrchable == 0: not available

    dungeon->room_resrchable[7] = 1;
    CHECK(research_needed(&rsrchval, dungeon));

    dungeon->room_resrchable[7] = 4; // needs the room to have been captured first (bit 2 of room_buildable)
    CHECK_FALSE(research_needed(&rsrchval, dungeon));
    dungeon->room_buildable[7] |= 2;
    CHECK(research_needed(&rsrchval, dungeon));

    dungeon->room_buildable[7] |= 1; // already buildable: research no longer needed
    CHECK_FALSE(research_needed(&rsrchval, dungeon));
}

TEST_CASE_METHOD(ResetSimAndConfig, "research_needed for RsCat_Creature requires the creature to be allowed but not force-enabled", "[kfx_sim][room_library]") {
    struct Dungeon *dungeon = get_dungeon(0);
    dungeon->research_num = 1;
    struct ResearchVal rsrchval = {};
    rsrchval.rtyp = RsCat_Creature;
    rsrchval.rkind = 4;

    CHECK_FALSE(research_needed(&rsrchval, dungeon)); // not allowed yet

    dungeon->creature_allowed[4] = 1;
    CHECK(research_needed(&rsrchval, dungeon));

    dungeon->creature_force_enabled[4] = 1;
    CHECK_FALSE(research_needed(&rsrchval, dungeon)); // force-enabled: no research needed
}
