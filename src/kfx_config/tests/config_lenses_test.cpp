// kfx_config: config_lenses.c's load_lenses_config_file() -- a second
// worked example of the NamedField/parse_named_field_blocks pattern
// (config_cubes_test.cpp was the first). Unlike config_cubes.c, this
// loader's Name field is spelled "NAME" (uppercase) exactly matching
// config.c's set_defaults() auto-registration check, so lenses_desc[]
// (this file's names table) *does* get populated correctly -- a useful
// contrast confirming config_cubes.c's cube_desc gap really is a
// case-sensitivity bug in that file, not in the shared mechanism.
//
// value_displace/value_pallete/value_overlay/value_mist all guard with
// an identical "idx out of [0, max_count)" bounds check that's
// unreachable through the real block-parsing path (idx always comes
// from a validated, already-in-range block number) -- not attempted
// here. PALETTE/OVERLAY/MIST all load real files from the game data
// directory via prepare_file_path(); only DISPLACEMENT is exercised,
// since it's the one field group needing no real file.
#include <catch2/catch_test_macros.hpp>

#include "kfx_config_test_paths.h" // KFX_CONFIG_TEST_FIXTURES_DIR
#include "config_lenses.h"

#include <cstring>

namespace {
struct ResetLenses {
    ResetLenses() { std::memset(&lenses_conf, 0, sizeof(lenses_conf)); }
};
}

TEST_CASE_METHOD(ResetLenses, "load_lenses_config_file maps a lens block's Name/Displacement fields, setting LCF_HasDisplace", "[kfx_config][config_lenses]") {
    REQUIRE(keeper_lenses_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/lenses_minimal.cfg", 0));

    struct LensConfig *lens = get_lens_config(1);
    CHECK(std::strcmp(lens->code_name, "LENS_WIBBLE") == 0);
    CHECK(lens->displace_kind == 1);
    CHECK(lens->displace_magnitude == 9);
    CHECK(lens->displace_period == 10);
    CHECK((lens->flags & LCF_HasDisplace) != 0);
    CHECK((lens->flags & LCF_HasMist) == 0);
}

TEST_CASE_METHOD(ResetLenses, "get_lens_config falls back to slot 0 for an out-of-range index", "[kfx_config][config_lenses]") {
    REQUIRE(keeper_lenses_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/lenses_minimal.cfg", 0));

    CHECK(get_lens_config(0) == get_lens_config(-1));
    CHECK(get_lens_config(0) == get_lens_config(lenses_conf.lenses_count + 1));
}

TEST_CASE_METHOD(ResetLenses, "load_lenses_config_file returns false for a missing file", "[kfx_config][config_lenses]") {
    CHECK_FALSE(keeper_lenses_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/does_not_exist.cfg", CnfLd_IgnoreErrors));
}

TEST_CASE("keeper_lenses_file_data has no pre/post-load hooks", "[kfx_config][config_lenses]") {
    CHECK(keeper_lenses_file_data.pre_load_func == nullptr);
    CHECK(keeper_lenses_file_data.post_load_func == nullptr);
    CHECK(std::strcmp(keeper_lenses_file_data.filename, "lenses.cfg") == 0);
}
