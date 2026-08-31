// kfx_sim coverage: a first pass over thing_physics.c's pure
// coordinate/velocity math and collision-distance predicates -- the
// functions in this file that don't route through
// get_min_floor_and_ceiling_heights_for_rect's subtile-rectangle sweep
// (thing_touching_flight_altitude/_above_flight_altitude,
// get_floor_height_under_thing_at and friends, position_over_floor_level,
// creature_cannot_move_directly_to, map_is_solid_at_height,
// creature_can_pass_through_wall_at, thing_in_wall_at(_with_radius),
// push_thingz_against_wall_at, move_object_to_nearest_free_position) or
// the wall-slide/bounce switch statements (slide_thing_against_wall_at,
// bounce_thing_off_wall_at) -- both left for a later increment.
#include <catch2/catch_test_macros.hpp>

#include "thing_physics.h"
#include "thing_shots.h"
#include "config_magic.h"
#include "kfx_sim_test_fixtures.h"

using namespace kfx_test;

TEST_CASE_METHOD(ResetSimAndConfig, "thing_touching_floor compares floor_height against mappos.z exactly", "[kfx_sim][thing_physics]") {
    struct Thing *thing = make_creature(1, 1, 0);
    thing->floor_height = 100;
    thing->mappos.z.val = 100;
    CHECK(thing_touching_floor(thing));

    thing->mappos.z.val = 101;
    CHECK_FALSE(thing_touching_floor(thing));
}

TEST_CASE_METHOD(ResetSimAndConfig, "positions_equivalent requires all three coordinates to match exactly", "[kfx_sim][thing_physics]") {
    struct Coord3d a = {};
    struct Coord3d b = {};
    a.x.val = 10; a.y.val = 20; a.z.val = 30;
    b.x.val = 10; b.y.val = 20; b.z.val = 30;
    CHECK(positions_equivalent(&a, &b));

    b.z.val = 31;
    CHECK_FALSE(positions_equivalent(&a, &b));
}

TEST_CASE_METHOD(ResetSimAndConfig, "creature_set_speed clamps to +-MAX_VELOCITY and flags the creature as moving", "[kfx_sim][thing_physics]") {
    struct Thing *thing = make_creature(1, 1, 0);

    creature_set_speed(thing, 5);
    struct CreatureControl *cctrl = creature_control_get(1);
    CHECK(cctrl->move_speed == 5);
    CHECK((cctrl->creature_control_flags & CCFlg_MoveY) != 0);

    creature_set_speed(thing, MAX_VELOCITY + 100);
    CHECK(cctrl->move_speed == MAX_VELOCITY);

    creature_set_speed(thing, -MAX_VELOCITY - 100);
    CHECK(cctrl->move_speed == -MAX_VELOCITY);
}

TEST_CASE_METHOD(ResetSimAndConfig, "cross_x_boundary_first/cross_y_boundary_first pick the axis whose subtile boundary is crossed first", "[kfx_sim][thing_physics]") {
    struct Coord3d pos1 = {};
    pos1.x.val = 100;
    pos1.y.val = 100;
    struct Coord3d pos2 = {};
    pos2.x.val = 200; // large x move
    pos2.y.val = 101; // tiny y move

    CHECK(cross_x_boundary_first(&pos1, &pos2));
    CHECK_FALSE(cross_y_boundary_first(&pos1, &pos2));

    pos2.x.val = 101; // tiny x move
    pos2.y.val = 200; // large y move
    CHECK_FALSE(cross_x_boundary_first(&pos1, &pos2));
    CHECK(cross_y_boundary_first(&pos1, &pos2));
}

TEST_CASE_METHOD(ResetSimAndConfig, "clear_thing_acceleration/clear_thing_velocity zero out the push-add/base velocity vectors", "[kfx_sim][thing_physics]") {
    struct Thing *thing = make_creature(1, 1, 0);
    thing->veloc_push_add.x.val = 5;
    thing->veloc_push_add.y.val = 6;
    thing->veloc_push_add.z.val = 7;
    thing->veloc_base.x.val = 1;
    thing->veloc_base.y.val = 2;
    thing->veloc_base.z.val = 3;

    clear_thing_acceleration(thing);
    CHECK(thing->veloc_push_add.x.val == 0);
    CHECK(thing->veloc_push_add.y.val == 0);
    CHECK(thing->veloc_push_add.z.val == 0);
    CHECK(thing->veloc_base.x.val == 1); // untouched by clear_thing_acceleration

    clear_thing_velocity(thing);
    CHECK(thing->veloc_base.x.val == 0);
    CHECK(thing->veloc_base.y.val == 0);
    CHECK(thing->veloc_base.z.val == 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "apply_transitive_velocity_to_thing accumulates into veloc_push_once and sets TF1_PushOnce", "[kfx_sim][thing_physics]") {
    struct Thing *thing = make_creature(1, 1, 0);
    thing->veloc_push_once.x.val = 1;
    struct ComponentVector veloc = {};
    veloc.x = 10;
    veloc.y = 20;
    veloc.z = 30;

    apply_transitive_velocity_to_thing(thing, &veloc);

    CHECK(thing->veloc_push_once.x.val == 11);
    CHECK(thing->veloc_push_once.y.val == 20);
    CHECK(thing->veloc_push_once.z.val == 30);
    CHECK((thing->state_flags & TF1_PushOnce) != 0);
}

TEST_CASE_METHOD(ResetSimAndConfig, "thing_on_thing_at is true when both XY and Z overlap within the combined solid sizes", "[kfx_sim][thing_physics]") {
    struct Thing *firstng = make_creature(1, 1, 0);
    struct Thing *sectng = make_creature(2, 2, 0);
    firstng->solid_size_xy = 40;
    firstng->solid_size_z = 40;
    sectng->solid_size_xy = 40;
    sectng->solid_size_z = 40;
    sectng->mappos.x.val = 100;
    sectng->mappos.y.val = 100;
    sectng->mappos.z.val = 100;

    struct Coord3d pos = {};
    pos.x.val = 100;
    pos.y.val = 100;
    pos.z.val = 100;
    CHECK(thing_on_thing_at(firstng, &pos, sectng));

    pos.x.val = 200; // far outside dist_collide (40)
    CHECK_FALSE(thing_on_thing_at(firstng, &pos, sectng));
}

TEST_CASE_METHOD(ResetSimAndConfig, "things_collide_while_first_moves_to skips the check for two things sharing the same parent", "[kfx_sim][thing_physics]") {
    struct Thing *firstng = make_creature(1, 1, 0);
    struct Thing *sectng = make_creature(2, 2, 0);
    firstng->parent_idx = 9;
    sectng->parent_idx = 9;

    struct Coord3d dst = {};
    dst.x.val = 500;
    CHECK_FALSE(things_collide_while_first_moves_to(firstng, &dst, sectng));
}

TEST_CASE_METHOD(ResetSimAndConfig, "things_collide_while_first_moves_to detects a collision at the destination position", "[kfx_sim][thing_physics]") {
    struct Thing *firstng = make_creature(1, 1, 0);
    struct Thing *sectng = make_creature(2, 2, 0);
    firstng->solid_size_xy = 40;
    firstng->solid_size_z = 40;
    sectng->solid_size_xy = 40;
    sectng->solid_size_z = 40;
    firstng->mappos.x.val = 100;
    firstng->mappos.y.val = 100;
    firstng->mappos.z.val = 100;
    sectng->mappos.x.val = 100;
    sectng->mappos.y.val = 100;
    sectng->mappos.z.val = 100;

    struct Coord3d dst = {}; // no movement: destination == current overlapping position
    dst.x.val = 100;
    dst.y.val = 100;
    dst.z.val = 100;
    CHECK(things_collide_while_first_moves_to(firstng, &dst, sectng));
}

TEST_CASE_METHOD(ResetSimAndConfig, "thing_is_exempt_from_z_axis_clipping exempts non-boulder shots and effect/effect-element things", "[kfx_sim][thing_physics]") {
    struct Thing *creature = make_creature(1, 1, 0);
    CHECK_FALSE(thing_is_exempt_from_z_axis_clipping(creature));

    struct Thing *shot = thing_get(2);
    shot->index = 2;
    shot->alloc_flags = TAlF_Exists;
    shot->class_id = TCls_Shot;
    shot->model = 3;
    kfx_config_state.conf.magic_conf.shot_types_count = 4;
    kfx_config_state.conf.magic_conf.shot_cfgstats[3].model_flags = 0;
    CHECK(thing_is_exempt_from_z_axis_clipping(shot)); // non-boulder shot: exempt

    kfx_config_state.conf.magic_conf.shot_cfgstats[3].model_flags = ShMF_Boulder;
    CHECK_FALSE(thing_is_exempt_from_z_axis_clipping(shot)); // boulder shots aren't exempt

    struct Thing *effect = thing_get(3);
    effect->index = 3;
    effect->alloc_flags = TAlF_Exists;
    effect->class_id = TCls_Effect;
    CHECK(thing_is_exempt_from_z_axis_clipping(effect));

    struct Thing *effect_elem = thing_get(4);
    effect_elem->index = 4;
    effect_elem->alloc_flags = TAlF_Exists;
    effect_elem->class_id = TCls_EffectElem;
    CHECK(thing_is_exempt_from_z_axis_clipping(effect_elem));
}

TEST_CASE_METHOD(ResetSimAndConfig, "remove_relevant_forces_from_thing_after_slide zeroes the velocity component(s) matching the blocked axes", "[kfx_sim][thing_physics]") {
    struct Thing *thing = make_creature(1, 1, 0);
    struct Coord3d pos = {};

    thing->veloc_base.x.val = 1;
    thing->veloc_base.y.val = 2;
    thing->veloc_base.z.val = 3;
    remove_relevant_forces_from_thing_after_slide(thing, &pos, SlbBloF_WalledX);
    CHECK(thing->veloc_base.x.val == 0);
    CHECK(thing->veloc_base.y.val == 2); // untouched
    CHECK(thing->veloc_base.z.val == 3);

    thing->veloc_base.y.val = 2;
    thing->veloc_base.z.val = 3;
    remove_relevant_forces_from_thing_after_slide(thing, &pos, SlbBloF_WalledY | SlbBloF_WalledZ);
    CHECK(thing->veloc_base.y.val == 0);
    CHECK(thing->veloc_base.z.val == 0);
}
