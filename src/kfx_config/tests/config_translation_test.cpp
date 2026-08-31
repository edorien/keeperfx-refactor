// kfx_config: config_translation.c's translation-table lookup
// (get_string_id_by_alias/get_translation_file_string), populated via
// load_translation_config_file() -- a real TOML fixture round trip
// through the per-language fallback chain (exact language code -> alias
// itself if no language matches). translation_table[]/translation_count
// are `static` with no direct extern access, so this only observes them
// through the public getter API, not by poking the array directly.
#include <catch2/catch_test_macros.hpp>

#include "kfx_config_test_paths.h" // KFX_CONFIG_TEST_FIXTURES_DIR
#include "config_translation.h"
#include "config_keeperfx.h"
#include "config_strings.h"

#include <string>

namespace {
struct ResetTranslation {
    ResetTranslation() {
        clear_translation_table();
        install_info.lang_id = Lang_English;
    }
    ~ResetTranslation() {
        clear_translation_table();
    }
};
}

TEST_CASE_METHOD(ResetTranslation, "load_translation_config_file resolves the exact-language-match text for an alias", "[kfx_config][config_translation]") {
    REQUIRE(keeper_translation_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/translation_minimal.toml", 0));

    TextStringId id = get_string_id_by_alias("greeting");
    CHECK(id >= TRANSLATION_STRINGS_START);
    CHECK(std::string(get_translation_file_string(id)) == "Hello");
}

TEST_CASE_METHOD(ResetTranslation, "load_translation_config_file falls back to the alias itself when no language (incl. English) matches", "[kfx_config][config_translation]") {
    REQUIRE(keeper_translation_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/translation_minimal.toml", 0));

    TextStringId id = get_string_id_by_alias("no_english_entry");
    CHECK(id >= TRANSLATION_STRINGS_START);
    CHECK(std::string(get_translation_file_string(id)) == "no_english_entry");
}

TEST_CASE_METHOD(ResetTranslation, "get_string_id_by_alias returns -1 for an alias never loaded", "[kfx_config][config_translation]") {
    CHECK(get_string_id_by_alias("never_loaded") == -1);
}

TEST_CASE_METHOD(ResetTranslation, "get_string_id_by_alias parses a plain numeric alias directly, bypassing the translation table", "[kfx_config][config_translation]") {
    CHECK(get_string_id_by_alias("42") == 42);
}

TEST_CASE_METHOD(ResetTranslation, "get_translation_file_string returns the invalid-id sentinel outside the translation table's live range", "[kfx_config][config_translation]") {
    CHECK(std::string(get_translation_file_string(0)) == "oh_crap_invalid_string_id");
    CHECK(std::string(get_translation_file_string(TRANSLATION_STRINGS_START + 999999)) == "oh_crap_invalid_string_id");
}

TEST_CASE_METHOD(ResetTranslation, "clear_translation_table makes every previously loaded alias unresolvable again", "[kfx_config][config_translation]") {
    REQUIRE(keeper_translation_file_data.load_func(KFX_CONFIG_TEST_FIXTURES_DIR "/translation_minimal.toml", 0));
    REQUIRE(get_string_id_by_alias("greeting") != -1);

    clear_translation_table();
    CHECK(get_string_id_by_alias("greeting") == -1);
}
