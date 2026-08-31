// kfx_config: config_mods.c -- get_loaded_mods_conf() is pure (branches
// only on config_keeperfx.c's config_network_is_active() check).
// load_mods_order_config_file()/recheck_all_mod_exist() are not
// attempted here: same class of gap as config_settings_test.cpp's
// load_settings()/highscores_test.cpp's load_high_score_table() -- they
// read a hardcoded real path (prepare_file_path(FGrp_Main,
// "mods/load_order.cfg")) with no caller-supplied fname, so there's no
// way to point them at a test fixture without a real file-I/O side
// effect against the actual game install directory or a change to the
// functions' own signatures.
#include <catch2/catch_test_macros.hpp>

#include "config_mods.h"
#include "config_keeperfx.h"

namespace {
struct ResetNetworkActiveCheck {
    ~ResetNetworkActiveCheck() { set_config_network_is_active_check(nullptr); }
};
}

TEST_CASE_METHOD(ResetNetworkActiveCheck, "get_loaded_mods_conf returns a stable pointer to the real config when no network-active check is installed", "[kfx_config][config_mods]") {
    const struct ModsConfig *first = get_loaded_mods_conf();
    const struct ModsConfig *second = get_loaded_mods_conf();
    CHECK(first == second);
    CHECK(mods_conf.after_base_cnt == 0);
    CHECK(mods_conf.after_campaign_cnt == 0);
    CHECK(mods_conf.after_map_cnt == 0);
}

TEST_CASE_METHOD(ResetNetworkActiveCheck, "get_loaded_mods_conf falls back to an empty config while a network-active check reports true, reverting once cleared", "[kfx_config][config_mods]") {
    const struct ModsConfig *real = get_loaded_mods_conf();

    set_config_network_is_active_check([]() -> TbBool { return true; });
    const struct ModsConfig *empty = get_loaded_mods_conf();
    CHECK(empty != real);
    CHECK(empty->after_base_cnt == 0);
    CHECK(empty->after_campaign_cnt == 0);
    CHECK(empty->after_map_cnt == 0);

    set_config_network_is_active_check(nullptr);
    CHECK(get_loaded_mods_conf() == real);
}
