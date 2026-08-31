// kfx_sim coverage: a first pass over spdigger_stack.c's imp-task-stack
// bookkeeping -- Dungeon::digger_stack[] is a fixed-size array (capped at
// DIGGER_TASK_MAX_COUNT) that add_to_dungeon_imp_stack_using_pos appends
// to and find_in_(dungeon_)imp_stack_using_pos/find_in_dungeon_imp_stack_starting_at
// search, the latter wrapping around via modulo -- real array logic, not
// just a predicate. is_digging_indestructible_place chains a task-list
// lookup (Dungeon::task_list[], the same "linear scan for a matching
// coords" shape as find_dig_from_task_list's other callers) with a direct
// SlabKind-indexed config check.
//
// find_in_imp_stack_using_pos/find_in_dungeon_imp_stack_using_pos weren't
// declared in spdigger_stack.h (only forward-declared inside
// spdigger_stack.c itself) -- added alongside struct DiggerStack's
// forward declaration, matching this session's established pattern for
// this kind of gap.
//
// Deliberately deferred: find_reachable_imp_tasks_excluding_start (needs
// navigation), remove_task_from_all_other_players_digger_stacks (map
// reveal/pretty-print side effects), and the bulk of this 3500-line file
// (check_out_*/add_*_to_imp_stack spiral searches) -- genuinely the hard
// tail.
#include <catch2/catch_test_macros.hpp>

#include "spdigger_stack.h"
#include "dungeon_data.h"
#include "tasks_list.h"
#include "slab_data.h"
#include "config_terrain.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "creature_task_needs_check_out_after_digger_stack_change compares the dungeon's and creature's last-seen stack update turn", "[kfx_sim][spdigger_stack]") {
    struct Thing *thing = make_creature(1, 1, 0);
    CHECK_FALSE(creature_task_needs_check_out_after_digger_stack_change(thing)); // both default to 0

    get_dungeon(0)->digger_stack_update_turn = 5;
    CHECK(creature_task_needs_check_out_after_digger_stack_change(thing));

    creature_control_get(1)->digger.stack_update_turn = 5;
    CHECK_FALSE(creature_task_needs_check_out_after_digger_stack_change(thing));
}

TEST_CASE_METHOD(ResetSimAndConfig, "add_to_dungeon_imp_stack_using_pos appends a task and reports whether the stack still has room", "[kfx_sim][spdigger_stack]") {
    struct Dungeon *dungeon = get_dungeon(0);
    CHECK(add_to_dungeon_imp_stack_using_pos(42, DigTsk_DigOrMine, dungeon));
    CHECK(dungeon->digger_stack_length == 1);
    CHECK(dungeon->digger_stack[0].stl_num == 42);
    CHECK(dungeon->digger_stack[0].task_type == DigTsk_DigOrMine);

    dungeon->digger_stack_length = DIGGER_TASK_MAX_COUNT - 1;
    CHECK_FALSE(add_to_dungeon_imp_stack_using_pos(1, DigTsk_DigOrMine, dungeon)); // fills the last slot
    CHECK(dungeon->digger_stack_length == DIGGER_TASK_MAX_COUNT);

    CHECK_FALSE(add_to_dungeon_imp_stack_using_pos(1, DigTsk_DigOrMine, dungeon)); // already full: refused outright
    CHECK(dungeon->digger_stack_length == DIGGER_TASK_MAX_COUNT); // unchanged
}

TEST_CASE_METHOD(ResetSimAndConfig, "find_in_imp_stack_using_pos/find_in_dungeon_imp_stack_using_pos require both stl_num and task_type to match", "[kfx_sim][spdigger_stack]") {
    struct Dungeon *dungeon = get_dungeon(0);
    add_to_dungeon_imp_stack_using_pos(10, DigTsk_DigOrMine, dungeon);
    add_to_dungeon_imp_stack_using_pos(20, DigTsk_ReinforceWall, dungeon);

    CHECK(find_in_dungeon_imp_stack_using_pos(10, DigTsk_DigOrMine, dungeon) == 0);
    CHECK(find_in_dungeon_imp_stack_using_pos(20, DigTsk_ReinforceWall, dungeon) == 1);
    CHECK(find_in_dungeon_imp_stack_using_pos(10, DigTsk_ReinforceWall, dungeon) == -1); // right pos, wrong type
    CHECK(find_in_dungeon_imp_stack_using_pos(99, DigTsk_DigOrMine, dungeon) == -1); // not present at all
}

TEST_CASE_METHOD(ResetSimAndConfig, "find_in_dungeon_imp_stack_starting_at wraps around the stack via modulo from the given start index", "[kfx_sim][spdigger_stack]") {
    struct Dungeon *dungeon = get_dungeon(0);
    add_to_dungeon_imp_stack_using_pos(1, DigTsk_ImproveDungeon, dungeon); // index 0
    add_to_dungeon_imp_stack_using_pos(2, DigTsk_ReinforceWall, dungeon);  // index 1
    add_to_dungeon_imp_stack_using_pos(3, DigTsk_ImproveDungeon, dungeon); // index 2

    CHECK(find_in_dungeon_imp_stack_starting_at(DigTsk_ImproveDungeon, 0, dungeon) == 0);
    // Starting at index 1 (ReinforceWall), the search wraps 1 -> 2 -> 0 and finds index 2 first.
    CHECK(find_in_dungeon_imp_stack_starting_at(DigTsk_ImproveDungeon, 1, dungeon) == 2);
    CHECK(find_in_dungeon_imp_stack_starting_at(DigTsk_PicksUpGoldPile, 0, dungeon) == -1); // type never present
}

TEST_CASE_METHOD(ResetSimAndConfig, "is_digging_indestructible_place is true only when the digger's task subtile is both tracked and an indestructible slab kind", "[kfx_sim][spdigger_stack]") {
    struct Thing *thing = make_creature(1, 1, 0);
    struct CreatureControl *cctrl = creature_control_get(1);
    cctrl->digger.task_stl = get_subtile_number(3, 3);
    get_slabmap_block(1, 1)->kind = 7; // subtile (3,3) is on slab (1,1), given STL_PER_SLB == 3

    CHECK_FALSE(is_digging_indestructible_place(thing)); // not in the task list at all

    get_dungeon(0)->highest_task_number = 1;
    get_dungeon(0)->task_list[0].coords = cctrl->digger.task_stl;
    CHECK_FALSE(is_digging_indestructible_place(thing)); // tracked, but slab kind 7 isn't marked indestructible

    kfx_config_state.conf.slab_conf.slab_types_count = 8;
    kfx_config_state.conf.slab_conf.slab_cfgstats[7].indestructible = 1;
    CHECK(is_digging_indestructible_place(thing));
}
