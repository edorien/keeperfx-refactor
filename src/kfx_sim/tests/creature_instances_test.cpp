// kfx_sim "creature" cluster, hard-tail depth increment: creature_instances.c
// was at 0% coverage. Its accessor/predicate layer -- instance-info bounds
// checks, per-instance property-flag predicates (instance_is_*), and the
// creature_has_*_weapon family that scans a CreatureControl's
// instance_available[] against them -- is pure pattern A on
// kfx_config_state.conf.magic_conf.instance_info[]/crtr_conf, no map or
// room needed. The instf_*() instance-effect implementations (actually
// firing a shot, casting a spell, digging, etc.) need a full map/thing
// fixture and are left for a later increment.
#include <catch2/catch_test_macros.hpp>

#include "creature_instances.h"
#include "creature_control.h"
#include "thing_data.h"
#include "kfx_config_state.h"
#include "kfx_sim_state.h"

#include <cstring>

namespace {
struct ResetState {
    ResetState() {
        std::memset(&kfx_sim_state, 0, sizeof(kfx_sim_state));
        std::memset(&kfx_config_state, 0, sizeof(kfx_config_state));
    }
};
}

TEST_CASE_METHOD(ResetState, "creature_instance_info_get bounds-checks inst_idx against instances_count, falling back to slot 0", "[kfx_sim][creature_instances]") {
    kfx_config_state.conf.crtr_conf.instances_count = 3;
    kfx_config_state.conf.magic_conf.instance_info[0].postal_priority = 9;

    CHECK(creature_instance_info_get(-1) == &kfx_config_state.conf.magic_conf.instance_info[0]);
    CHECK(creature_instance_info_get(3) == &kfx_config_state.conf.magic_conf.instance_info[0]);
    CHECK(creature_instance_info_get(1) == &kfx_config_state.conf.magic_conf.instance_info[1]);
}

TEST_CASE_METHOD(ResetState, "creature_instance_info_invalid rejects the reserved slot 0", "[kfx_sim][creature_instances]") {
    CHECK(creature_instance_info_invalid(&kfx_config_state.conf.magic_conf.instance_info[0]));
    CHECK_FALSE(creature_instance_info_invalid(&kfx_config_state.conf.magic_conf.instance_info[1]));
}

TEST_CASE_METHOD(ResetState, "creature_instance_is_available reads the thing's CreatureControl, false when invalid", "[kfx_sim][creature_instances]") {
    struct Thing *thing = thing_get(1);
    thing->ccontrol_idx = 0;
    CHECK_FALSE(creature_instance_is_available(thing, 5));

    thing->ccontrol_idx = 1;
    creature_control_get(1)->instance_available[5] = true;
    CHECK(creature_instance_is_available(thing, 5));
    CHECK_FALSE(creature_instance_is_available(thing, 6));
}

TEST_CASE_METHOD(ResetState, "creature_choose_first_available_instance picks the first learned instance that's available", "[kfx_sim][creature_instances]") {
    struct Thing *thing = thing_get(1);
    thing->ccontrol_idx = 1;
    struct CreatureControl *cctrl = creature_control_get(1);
    kfx_config_state.conf.crtr_conf.model[0].learned_instance_id[0] = 3; // not yet available
    kfx_config_state.conf.crtr_conf.model[0].learned_instance_id[1] = 7;
    cctrl->instance_available[7] = true;

    CHECK(creature_choose_first_available_instance(thing));
    CHECK(cctrl->active_instance_id == 7);
}

TEST_CASE_METHOD(ResetState, "creature_choose_first_available_instance resets to CrInst_NULL when nothing is available", "[kfx_sim][creature_instances]") {
    struct Thing *thing = thing_get(1);
    thing->ccontrol_idx = 1;
    struct CreatureControl *cctrl = creature_control_get(1);
    cctrl->active_instance_id = 42;

    CHECK_FALSE(creature_choose_first_available_instance(thing));
    CHECK(cctrl->active_instance_id == CrInst_NULL);
}

TEST_CASE_METHOD(ResetState, "creature_increase_available_instances unlocks instances at or below the creature's next level", "[kfx_sim][creature_instances]") {
    struct Thing *thing = thing_get(1);
    thing->ccontrol_idx = 1;
    thing->owner = 0;
    struct CreatureControl *cctrl = creature_control_get(1);
    cctrl->creature_control_flags = CCFlg_Exists;
    cctrl->exp_level = 2;
    kfx_config_state.conf.crtr_conf.model[0].learned_instance_id[0] = 5;
    kfx_config_state.conf.crtr_conf.model[0].learned_instance_level[0] = 3; // <= exp_level+1 (3)

    CHECK(creature_increase_available_instances(thing));
    CHECK(cctrl->instance_available[5]);
}

TEST_CASE_METHOD(ResetState, "creature_increase_available_instances revokes a too-high-level instance unless ClscBug_RebirthKeepsSpells is set", "[kfx_sim][creature_instances]") {
    struct Thing *thing = thing_get(1);
    thing->ccontrol_idx = 1;
    thing->owner = 0;
    struct CreatureControl *cctrl = creature_control_get(1);
    cctrl->creature_control_flags = CCFlg_Exists;
    cctrl->exp_level = 0;
    cctrl->instance_available[5] = true; // previously granted, now above the creature's level
    kfx_config_state.conf.crtr_conf.model[0].learned_instance_id[0] = 5;
    kfx_config_state.conf.crtr_conf.model[0].learned_instance_level[0] = 10; // > exp_level+1 (1)

    creature_increase_available_instances(thing);
    CHECK_FALSE(cctrl->instance_available[5]);

    cctrl->instance_available[5] = true;
    kfx_config_state.conf.rules[0].gameplay.classic_bugs_flags = ClscBug_RebirthKeepsSpells;
    creature_increase_available_instances(thing);
    CHECK(cctrl->instance_available[5]); // the classic-bugs flag preserves it
}

TEST_CASE_METHOD(ResetState, "instance_is_disarming_weapon/_draws_possession_swipe/_is_ranged_weapon/_is_melee_attack read distinct instance_property_flags bits", "[kfx_sim][creature_instances]") {
    kfx_config_state.conf.crtr_conf.instances_count = 2;
    struct InstanceInfo *inst = &kfx_config_state.conf.magic_conf.instance_info[1];

    inst->instance_property_flags = InstPF_Disarming;
    CHECK(instance_is_disarming_weapon(1));
    CHECK_FALSE(instance_draws_possession_swipe(1));

    inst->instance_property_flags = InstPF_UsesSwipe;
    CHECK(instance_draws_possession_swipe(1));

    inst->instance_property_flags = InstPF_RangedAttack;
    CHECK(instance_is_ranged_weapon(1));
    CHECK_FALSE(instance_is_melee_attack(1));

    inst->instance_property_flags = InstPF_MeleeAttack;
    CHECK(instance_is_melee_attack(1));
    CHECK_FALSE(instance_is_ranged_weapon(1));
}

TEST_CASE_METHOD(ResetState, "instance_is_ranged_weapon_vs_objects requires Ranged+Destructive but not Dangerous", "[kfx_sim][creature_instances]") {
    kfx_config_state.conf.crtr_conf.instances_count = 2;
    struct InstanceInfo *inst = &kfx_config_state.conf.magic_conf.instance_info[1];

    inst->instance_property_flags = InstPF_RangedAttack | InstPF_Destructive;
    CHECK(instance_is_ranged_weapon_vs_objects(1));

    inst->instance_property_flags |= InstPF_Dangerous;
    CHECK_FALSE(instance_is_ranged_weapon_vs_objects(1)); // dangerous excludes it
}

TEST_CASE_METHOD(ResetState, "instance_is_used_for_going_postal reads a positive postal_priority", "[kfx_sim][creature_instances]") {
    kfx_config_state.conf.crtr_conf.instances_count = 2;
    struct InstanceInfo *inst = &kfx_config_state.conf.magic_conf.instance_info[1];

    CHECK_FALSE(instance_is_used_for_going_postal(1));
    inst->postal_priority = 5;
    CHECK(instance_is_used_for_going_postal(1));
}

TEST_CASE_METHOD(ResetState, "creature_has_ranged_weapon/_has_melee_attack scan the creature's available instances for a matching property", "[kfx_sim][creature_instances]") {
    kfx_config_state.conf.crtr_conf.instances_count = 3;
    kfx_config_state.conf.magic_conf.instance_info[2].instance_property_flags = InstPF_RangedAttack;
    struct Thing *creatng = thing_get(1);
    creatng->ccontrol_idx = 1;
    struct CreatureControl *cctrl = creature_control_get(1);

    CHECK_FALSE(creature_has_ranged_weapon(creatng));
    CHECK_FALSE(creature_has_melee_attack(creatng));

    cctrl->instance_available[2] = true;
    CHECK(creature_has_ranged_weapon(creatng));
    CHECK_FALSE(creature_has_melee_attack(creatng)); // same instance isn't flagged melee
}

TEST_CASE_METHOD(ResetState, "creature_has_disarming_weapon/_has_weapon_for_postal scan the creature's available instances", "[kfx_sim][creature_instances]") {
    kfx_config_state.conf.crtr_conf.instances_count = 3;
    kfx_config_state.conf.magic_conf.instance_info[2].instance_property_flags = InstPF_Disarming;
    kfx_config_state.conf.magic_conf.instance_info[2].postal_priority = 1;
    struct Thing *creatng = thing_get(1);
    creatng->ccontrol_idx = 1;
    struct CreatureControl *cctrl = creature_control_get(1);
    cctrl->instance_available[2] = true;

    CHECK(creature_has_disarming_weapon(creatng));
    CHECK(creature_has_weapon_for_postal(creatng));
}
