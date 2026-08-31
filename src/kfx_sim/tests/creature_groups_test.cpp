// kfx_sim "creature" cluster, hard-tail depth increment: creature_groups.c
// was at 0% coverage. Its accessor cluster (get_no_creatures_in_group,
// get_first/last_follower_creature_in_group, get_group_leader,
// creature_is_group_member/_is_group_leader) walks a singly-linked chain
// of real thing_get()/CreatureControl slots via next_in_group, the same
// linked-list-through-CreatureControl shape used throughout this plan
// (calculate_free_lair_space, battle_with_creature_of_player). The
// group-mutation functions (add/remove_creature_from_group,
// leader_find_positions_for_followers, get_best_creature_to_lead_group)
// need a fuller Room/navigation fixture and are left for a later
// increment.
#include <catch2/catch_test_macros.hpp>

#include "creature_groups.h"
#include "creature_control.h"
#include "thing_data.h"
#include "kfx_sim_state.h"
#include "kfx_sim_test_fixtures.h"

#include <cstring>

namespace {
// Builds a 3-member chain: leader (thing 1) -> follower A (thing 2) ->
// follower B (thing 3), via next_in_group. Every member's own
// group_leader_idx points back at the leader (thing 1), matching how the
// real chain-building functions set it up (confirmed by reading
// internal_add_member_to_group_chain_head's body before assuming it).
struct GroupFixture {
    struct Thing *leader;
    struct Thing *followerA;
    struct Thing *followerB;

    GroupFixture() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));

        leader = thing_get(1);
        leader->index = 1; // thing_get() doesn't set ->index -- callers/allocators normally do
        leader->ccontrol_idx = 1;
        followerA = thing_get(2);
        followerA->index = 2;
        followerA->ccontrol_idx = 2;
        followerB = thing_get(3);
        followerB->index = 3;
        followerB->ccontrol_idx = 3;

        struct CreatureControl *leadctrl = creature_control_get(1);
        struct CreatureControl *actrl = creature_control_get(2);
        struct CreatureControl *bctrl = creature_control_get(3);

        leadctrl->group_leader_idx = 1; // the leader's own chain points at itself
        leadctrl->next_in_group = 2;
        actrl->group_leader_idx = 1;
        actrl->next_in_group = 3;
        bctrl->group_leader_idx = 1;
        bctrl->next_in_group = 0; // end of chain
    }
};
}

TEST_CASE("get_no_creatures_in_group returns 1 for a solo creature (group_leader_idx==0)", "[kfx_sim][creature_groups]") {
    std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
    struct Thing *solo = thing_get(1);
    solo->ccontrol_idx = 1; // creature_control_get(1) defaults to group_leader_idx==0

    CHECK(get_no_creatures_in_group(solo) == 1);
}

TEST_CASE_METHOD(GroupFixture, "get_no_creatures_in_group counts every member of a real chain", "[kfx_sim][creature_groups]") {
    CHECK(get_no_creatures_in_group(leader) == 3);
    CHECK(get_no_creatures_in_group(followerB) == 3); // any member sees the same chain
}

TEST_CASE_METHOD(GroupFixture, "get_first_follower_creature_in_group returns the chain's second member", "[kfx_sim][creature_groups]") {
    CHECK(get_first_follower_creature_in_group(leader) == followerA);
}

TEST_CASE_METHOD(GroupFixture, "get_last_follower_creature_in_group returns the chain's final member", "[kfx_sim][creature_groups]") {
    CHECK(get_last_follower_creature_in_group(leader) == followerB);
}

TEST_CASE("get_first/_last_follower_creature_in_group return INVALID_THING for a solo creature", "[kfx_sim][creature_groups]") {
    std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
    struct Thing *solo = thing_get(1);
    solo->ccontrol_idx = 1;

    CHECK(get_first_follower_creature_in_group(solo) == INVALID_THING);
    CHECK(get_last_follower_creature_in_group(solo) == INVALID_THING);
}

TEST_CASE_METHOD(GroupFixture, "get_group_leader resolves any member's group_leader_idx to the leader thing", "[kfx_sim][creature_groups]") {
    CHECK(get_group_leader(followerA) == leader);
    CHECK(get_group_leader(leader) == leader);
}

TEST_CASE_METHOD(GroupFixture, "creature_is_group_member/_is_group_leader distinguish the leader from its followers", "[kfx_sim][creature_groups]") {
    CHECK(creature_is_group_member(followerA));
    CHECK_FALSE(creature_is_group_leader(followerA));

    CHECK(creature_is_group_member(leader)); // the leader's own group_leader_idx is also >0
    CHECK(creature_is_group_leader(leader));
}

TEST_CASE("creature_is_group_member is false for a solo creature", "[kfx_sim][creature_groups]") {
    std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
    struct Thing *solo = thing_get(1);
    solo->ccontrol_idx = 1;

    CHECK_FALSE(creature_is_group_member(solo));
    CHECK_FALSE(creature_is_group_leader(solo)); // get_group_leader resolves to INVALID_THING
}

// --- The group-mutation cluster: real singly/doubly-linked-list surgery
// over CreatureControl::next_in_group/prev_in_group, the same
// "complicated logic" shape as this session's battle-list/task-list
// passes. Uses kfx_sim_test_fixtures.h's make_creature (unlike
// GroupFixture above) because these functions call thing_exists(), which
// needs TAlF_Exists actually set.
using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "internal_update_leader_index_in_group re-stamps group_leader_idx across the whole chain starting from the given thing", "[kfx_sim][creature_groups]") {
    struct Thing *t1 = make_creature(1, 1, 0);
    struct Thing *t2 = make_creature(2, 2, 0);
    struct Thing *t3 = make_creature(3, 3, 0);
    creature_control_get(1)->next_in_group = 2;
    creature_control_get(2)->next_in_group = 3;
    creature_control_get(1)->group_leader_idx = 99; // deliberately wrong, to prove it gets overwritten
    creature_control_get(2)->group_leader_idx = 99;
    creature_control_get(3)->group_leader_idx = 99;

    internal_update_leader_index_in_group(t1);

    CHECK(creature_control_get(1)->group_leader_idx == 1);
    CHECK(creature_control_get(2)->group_leader_idx == 1);
    CHECK(creature_control_get(3)->group_leader_idx == 1);
    (void)t2; (void)t3;
}

TEST_CASE_METHOD(ResetSimAndConfig, "internal_remove_member_from_group_chain unlinks a middle member and relinks its neighbors", "[kfx_sim][creature_groups]") {
    make_creature(1, 1, 0);
    struct Thing *t2 = make_creature(2, 2, 0);
    make_creature(3, 3, 0);
    creature_control_get(1)->next_in_group = 2;
    creature_control_get(2)->prev_in_group = 1;
    creature_control_get(2)->next_in_group = 3;
    creature_control_get(3)->prev_in_group = 2;

    internal_remove_member_from_group_chain(t2);

    CHECK(creature_control_get(1)->next_in_group == 3);
    CHECK(creature_control_get(3)->prev_in_group == 1);
    CHECK(creature_control_get(2)->next_in_group == 0);
    CHECK(creature_control_get(2)->prev_in_group == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "internal_remove_member_from_group_chain unlinks the chain head/tail without a dangling neighbor pointer", "[kfx_sim][creature_groups]") {
    struct Thing *t1 = make_creature(1, 1, 0);
    make_creature(2, 2, 0);
    creature_control_get(1)->next_in_group = 2;
    creature_control_get(2)->prev_in_group = 1;

    internal_remove_member_from_group_chain(t1); // removing the head: no prev to relink
    CHECK(creature_control_get(2)->prev_in_group == 0);
    CHECK(creature_control_get(2)->next_in_group == 0); // untouched
}

TEST_CASE_METHOD(ResetSimAndConfig, "internal_add_member_to_group_chain_head prepends a new member, demoting the old head", "[kfx_sim][creature_groups]") {
    struct Thing *newhead = make_creature(4, 4, 0);
    struct Thing *oldhead = make_creature(1, 1, 0);
    creature_control_get(1)->group_member_count = 5;

    internal_add_member_to_group_chain_head(newhead, oldhead);

    CHECK(creature_control_get(4)->next_in_group == 1);
    CHECK(creature_control_get(1)->prev_in_group == 4);
    CHECK(creature_control_get(1)->group_member_count == 0); // recomputed elsewhere
}

TEST_CASE_METHOD(ResetSimAndConfig, "add_creature_to_group refuses to add a creature to itself or a group owned by another player", "[kfx_sim][creature_groups]") {
    struct Thing *thing = make_creature(1, 1, 0);
    CHECK_FALSE(add_creature_to_group(thing, thing)); // self

    struct Thing *other_owner_grp = make_creature(2, 2, 1);
    CHECK_FALSE(add_creature_to_group(thing, other_owner_grp));
}

TEST_CASE_METHOD(ResetSimAndConfig, "add_creature_to_group forms a new 2-member group when the target has no existing followers", "[kfx_sim][creature_groups]") {
    struct Thing *leader = make_creature(1, 1, 0);
    struct Thing *joiner = make_creature(2, 2, 0);

    CHECK(add_creature_to_group(joiner, leader));

    CHECK(creature_control_get(1)->next_in_group == 2);
    CHECK(creature_control_get(2)->prev_in_group == 1);
    CHECK(creature_control_get(1)->group_leader_idx == 1);
    CHECK(creature_control_get(2)->group_leader_idx == 1);
    CHECK((joiner->alloc_flags & TAlF_IsFollowingLeader) != 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "add_creature_to_group appends to an existing group's tail rather than replacing it", "[kfx_sim][creature_groups]") {
    struct Thing *leader = make_creature(1, 1, 0);
    struct Thing *followerA = make_creature(2, 2, 0);
    add_creature_to_group(followerA, leader); // build the initial 2-member group

    struct Thing *followerB = make_creature(3, 3, 0);
    CHECK(add_creature_to_group(followerB, leader));

    CHECK(creature_control_get(2)->next_in_group == 3); // A now points to the new tail
    CHECK(creature_control_get(3)->prev_in_group == 2);
    CHECK(creature_control_get(3)->group_leader_idx == 1); // inherited from A's leader
    CHECK(creature_control_get(3)->next_in_group == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "add_creature_to_group_as_leader promotes a new creature above the existing leader, found via any group member", "[kfx_sim][creature_groups]") {
    struct Thing *oldleader = make_creature(1, 1, 0);
    struct Thing *follower = make_creature(2, 2, 0);
    add_creature_to_group(follower, oldleader); // 1 <-> 2, leader index 1 throughout

    struct Thing *newleader = make_creature(3, 3, 0);
    CHECK(add_creature_to_group_as_leader(newleader, follower)); // grptng is a follower, not the leader itself

    CHECK(creature_control_get(3)->next_in_group == 1); // new leader now heads the chain
    CHECK(creature_control_get(1)->prev_in_group == 3);
    CHECK((oldleader->alloc_flags & TAlF_IsFollowingLeader) != 0);
    // internal_update_leader_index_in_group propagates the new leader's index to everyone.
    CHECK(creature_control_get(3)->group_leader_idx == 3);
    CHECK(creature_control_get(1)->group_leader_idx == 3);
    CHECK(creature_control_get(2)->group_leader_idx == 3);
}

TEST_CASE_METHOD(ResetSimAndConfig, "remove_creature_from_group_without_leader_consideration removes a follower without disturbing the rest of the group", "[kfx_sim][creature_groups]") {
    struct Thing *leader = make_creature(1, 1, 0);
    struct Thing *followerA = make_creature(2, 2, 0);
    struct Thing *followerB = make_creature(3, 3, 0);
    add_creature_to_group(followerA, leader);
    add_creature_to_group(followerB, leader); // chain: 1 <-> 2 <-> 3

    CHECK(remove_creature_from_group_without_leader_consideration(followerA)); // group survives (2 members remain)

    CHECK(creature_control_get(1)->next_in_group == 3); // leader now points straight to B
    CHECK(creature_control_get(3)->prev_in_group == 1);
    CHECK(creature_control_get(2)->group_leader_idx == 0); // A fully detached
    CHECK(creature_control_get(2)->next_in_group == 0);
    CHECK_FALSE((followerA->alloc_flags & TAlF_IsFollowingLeader) != 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "remove_creature_from_group_without_leader_consideration disbands a 2-member group once only the leader is left", "[kfx_sim][creature_groups]") {
    struct Thing *leader = make_creature(1, 1, 0);
    struct Thing *follower = make_creature(2, 2, 0);
    add_creature_to_group(follower, leader);
    // Leave leader->active_state at its zeroed default (not CrSt_CreatureFollowLeader),
    // so the disband branch's conditional set_start_state() call -- a deep
    // state-machine dispatch this test isn't set up to exercise -- is skipped.

    CHECK_FALSE(remove_creature_from_group_without_leader_consideration(follower)); // false: group disbanded

    CHECK(creature_control_get(1)->next_in_group == 0);
    CHECK(creature_control_get(1)->prev_in_group == 0);
    CHECK(creature_control_get(1)->group_leader_idx == 0);
    CHECK_FALSE((leader->alloc_flags & TAlF_IsFollowingLeader) != 0);
}
