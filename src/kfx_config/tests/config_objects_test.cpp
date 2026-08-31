// kfx_config: config_objects.c's load_objects_config_file() -- a fourth
// worked example of the NamedField/parse_named_field_blocks pattern.
// crate_thing_to_workshop_item_class/_model reach into
// config_reload_callbacks (thing_is_workshop_crate/get_thing_class_id/
// get_thing_model/thing_is_invalid), exercised here against that
// table's default no-op stubs (already verified safe and exhaustively
// covered in config_reload_callbacks_test.cpp) rather than a fake --
// no new fixture needed.
// get_required_room_capacity_for_object()'s RoRoF_LairStorage/
// RoRoF_DeadStorage cases additionally reach into config_creature.c's
// creature_stats_get()/creature_stats_invalid() -- not attempted here,
// only the cases reachable through get_object_model_stats() directly.
#include <catch2/catch_test_macros.hpp>

#include "kfx_config_test_paths.h" // KFX_CONFIG_TEST_FIXTURES_DIR
#include "config_objects.h"
#include "config_terrain.h" // RoomRole
#include "kfx_config_state.h"

#include <cstring>

namespace {
struct ResetConfigState {
    ResetConfigState() { std::memset(&kfx_config_state, 0, sizeof(kfx_config_state)); }
};
}

TEST_CASE_METHOD(ResetConfigState, "load_objects_config_file maps each object block's Name/Genre", "[kfx_config][config_objects]") {
    REQUIRE(keeper_objects_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/objects_minimal.cfg", 0));

    CHECK(std::strcmp(get_object_model_stats(0)->code_name, "GOLD_PILE") == 0);
    CHECK(std::strcmp(get_object_model_stats(1)->code_name, "CHICKEN") == 0);
    CHECK(std::strcmp(get_object_model_stats(2)->code_name, "WORKSHOP_BOX") == 0);
    CHECK(std::strcmp(get_object_model_stats(3)->code_name, "SPELL_BOOK") == 0);
}

TEST_CASE_METHOD(ResetConfigState, "get_object_model_stats falls back to slot 0 for an out-of-range model", "[kfx_config][config_objects]") {
    REQUIRE(keeper_objects_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/objects_minimal.cfg", 0));
    CHECK(get_object_model_stats(3000) == get_object_model_stats(0)); // ThingModel is short; 3000 > OBJECT_TYPES_MAX(2000) but fits
}

TEST_CASE_METHOD(ResetConfigState, "object_code_name/object_model_id round-trip a loaded object's code name", "[kfx_config][config_objects]") {
    REQUIRE(keeper_objects_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/objects_minimal.cfg", 0));

    CHECK(std::strcmp(object_code_name(1), "CHICKEN") == 0);
    CHECK(object_model_id("CHICKEN") == 1);
    CHECK(object_model_id("NOT_A_REAL_OBJECT") == -1);
}

TEST_CASE_METHOD(ResetConfigState, "get_required_room_capacity_for_object matches gold/food/crate/power genres to their storage room roles", "[kfx_config][config_objects]") {
    REQUIRE(keeper_objects_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/objects_minimal.cfg", 0));

    // GoldStorage: routes through config_reload_callbacks's default
    // get_wealth_size_of_gold_hoard_model, which returns 0.
    CHECK(get_required_room_capacity_for_object(RoRoF_GoldStorage, 0, 0) == 0);
    CHECK(get_required_room_capacity_for_object(RoRoF_FoodStorage, 1, 0) == 1);
    CHECK(get_required_room_capacity_for_object(RoRoF_FoodSpawn, 1, 0) == 1);
    CHECK(get_required_room_capacity_for_object(RoRoF_CratesStorage, 2, 0) == 1);
    CHECK(get_required_room_capacity_for_object(RoRoF_PowersStorage, 3, 0) == 1);
}

TEST_CASE_METHOD(ResetConfigState, "get_required_room_capacity_for_object returns 0 for a genre/role mismatch and for RoRoF_KeeperStorage", "[kfx_config][config_objects]") {
    REQUIRE(keeper_objects_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/objects_minimal.cfg", 0));

    CHECK(get_required_room_capacity_for_object(RoRoF_GoldStorage, 1, 0) == 0); // object 1 is Food, not GoldHoard
    CHECK(get_required_room_capacity_for_object(RoRoF_KeeperStorage, 0, 0) == 0);
}

TEST_CASE_METHOD(ResetConfigState, "crate_thing_to_workshop_item_class/_model resolve against config_reload_callbacks' default no-op stubs", "[kfx_config][config_objects]") {
    // thing_is_invalid defaults to true (a real quirk documented in
    // config_reload_callbacks_test.cpp), so _model's first guard always
    // takes the "invalid thing" early-return branch here.
    CHECK(crate_thing_to_workshop_item_model(nullptr) == kfx_config_state.conf.object_conf.object_to_door_or_trap[0]);
    // thing_is_workshop_crate defaults to false, so _class falls through
    // to config_reload_callbacks->get_thing_class_id, which defaults to 0.
    CHECK(crate_thing_to_workshop_item_class(nullptr) == 0);
}

TEST_CASE_METHOD(ResetConfigState, "load_objects_config_file returns false for a missing file", "[kfx_config][config_objects]") {
    CHECK_FALSE(keeper_objects_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/does_not_exist.cfg", CnfLd_IgnoreErrors));
}

TEST_CASE("keeper_objects_file_data has no pre/post-load hooks", "[kfx_config][config_objects]") {
    CHECK(keeper_objects_file_data.pre_load_func == nullptr);
    CHECK(keeper_objects_file_data.post_load_func == nullptr);
    CHECK(std::strcmp(keeper_objects_file_data.filename, "objects.cfg") == 0);
}
