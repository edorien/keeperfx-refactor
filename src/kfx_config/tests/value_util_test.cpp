// First fixture-file-backed test for kfx_config, per docs/refactor/
// testing/comprehensive/stage-08-comprehensive-library-passes.md:
// load_toml_file() is the generic mechanism every TOML-based
// config_*.c loader (config_slabsets.c, config_terrain.c, ...) shares --
// establishing "how do we test file-loading with a fixture" once, here,
// matters more than which specific loader goes first (stage-08's own
// framing). fixtures/sample.toml is deliberately independent of any real
// config/fxdata/*.toml schema.
#include <catch2/catch_test_macros.hpp>

#include "kfx_config_test_paths.h" // KFX_CONFIG_TEST_FIXTURES_DIR
#include "value_util.h" // pulls in <toml.h> -> "value.h"
#include "config.h" // CnfLd_IgnoreErrors

#include <cstring>

TEST_CASE("load_toml_file loads a real fixture file and parses its structure", "[kfx_config][value_util]") {
    VALUE root;
    REQUIRE(load_toml_file(KFX_CONFIG_TEST_FIXTURES_DIR "/sample.toml", &root, 0));

    VALUE *section = value_dict_get(&root, "section");
    REQUIRE(value_type(section) == VALUE_DICT);

    VALUE *name = value_dict_get(section, "Name");
    REQUIRE(value_type(name) == VALUE_STRING);
    CHECK(std::strcmp(value_string(name), "test") == 0);

    VALUE *val = value_dict_get(section, "Value");
    REQUIRE(value_type(val) == VALUE_INT32);
    CHECK(value_int32(val) == 42);

    value_fini(&root);
}

TEST_CASE("load_toml_file returns false for a missing file", "[kfx_config][value_util]") {
    VALUE root;
    CHECK_FALSE(load_toml_file(KFX_CONFIG_TEST_FIXTURES_DIR "/does_not_exist.toml", &root, CnfLd_IgnoreErrors));
}
