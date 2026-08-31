// kfx_frontend: gui_frontmenu.c's pure scans over the real,
// directly-settable active_menus[] array (kfx_platform's own
// bflib_guibtns.h struct GuiMenu, complete array type -- unlike some
// other extern arrays in this plan, sizeof() works fine here).
// point_is_over_gui_menu() had no header declaration anywhere -- added
// to gui_frontmenu.h, the usual "add the missing declaration" fix.
//
// Everything else in this file (refresh_active_button_sprites_for_player,
// update_busy_doing_gui_on_menu, turn_on/_off_menu, set_menu_mode,
// kill_menu, add/remove_from_menu_stack, ...) mutates real GUI/button
// state (active_buttons[], the menu stack, screen coordinates from
// GetMouseX/Y) and isn't attempted here.
#include <catch2/catch_test_macros.hpp>

#include "gui_frontmenu.h"
#include "bflib_guibtns.h"

#include <cstring>

namespace {
struct ResetActiveMenus {
    ResetActiveMenus() { std::memset(active_menus, 0, sizeof(active_menus)); }
};
}

TEST_CASE_METHOD(ResetActiveMenus, "get_active_menu returns the menu at a valid index", "[kfx_frontend][gui_frontmenu]") {
    active_menus[3].ident = 9;
    CHECK(get_active_menu(3) == &active_menus[3]);
}

TEST_CASE_METHOD(ResetActiveMenus, "get_active_menu clamps a negative or out-of-range index to slot 0", "[kfx_frontend][gui_frontmenu]") {
    CHECK(get_active_menu(-1) == &active_menus[0]);
    CHECK(get_active_menu(ACTIVE_MENUS_COUNT) == &active_menus[0]);
}

TEST_CASE_METHOD(ResetActiveMenus, "first_monopoly_menu finds the first visible menu with is_monopoly_menu set", "[kfx_frontend][gui_frontmenu]") {
    active_menus[1].visual_state = 1;
    active_menus[1].is_monopoly_menu = 0;
    active_menus[2].visual_state = 1;
    active_menus[2].is_monopoly_menu = 1;
    CHECK(first_monopoly_menu() == 2);
}

TEST_CASE_METHOD(ResetActiveMenus, "first_monopoly_menu ignores a monopoly menu that isn't currently visible", "[kfx_frontend][gui_frontmenu]") {
    active_menus[2].visual_state = 0; // not visible
    active_menus[2].is_monopoly_menu = 1;
    CHECK(first_monopoly_menu() == -1);
}

TEST_CASE_METHOD(ResetActiveMenus, "menu_id_to_number finds a visible menu by its ident", "[kfx_frontend][gui_frontmenu]") {
    active_menus[4].visual_state = 1;
    active_menus[4].ident = 42;
    CHECK(menu_id_to_number(42) == 4);
}

TEST_CASE_METHOD(ResetActiveMenus, "menu_id_to_number returns MENU_INVALID_ID when no visible menu matches", "[kfx_frontend][gui_frontmenu]") {
    active_menus[4].visual_state = 0; // matches ident but not visible
    active_menus[4].ident = 42;
    CHECK(menu_id_to_number(42) == MENU_INVALID_ID);
}

TEST_CASE_METHOD(ResetActiveMenus, "point_is_over_gui_menu finds the menu whose rectangle contains the point", "[kfx_frontend][gui_frontmenu]") {
    active_menus[5].visual_state = 2;
    active_menus[5].is_turned_on = 1;
    active_menus[5].pos_x = 10;
    active_menus[5].pos_y = 20;
    active_menus[5].width = 30;
    active_menus[5].height = 40;
    CHECK(point_is_over_gui_menu(15, 25) == 5);
}

TEST_CASE_METHOD(ResetActiveMenus, "point_is_over_gui_menu ignores a menu that's turned off or not at visual_state 2", "[kfx_frontend][gui_frontmenu]") {
    active_menus[5].visual_state = 2;
    active_menus[5].is_turned_on = 0; // turned off
    active_menus[5].pos_x = 10;
    active_menus[5].pos_y = 20;
    active_menus[5].width = 30;
    active_menus[5].height = 40;
    CHECK(point_is_over_gui_menu(15, 25) == MENU_INVALID_ID);
}

TEST_CASE_METHOD(ResetActiveMenus, "point_is_over_gui_menu returns MENU_INVALID_ID for a point outside every menu", "[kfx_frontend][gui_frontmenu]") {
    active_menus[5].visual_state = 2;
    active_menus[5].is_turned_on = 1;
    active_menus[5].pos_x = 10;
    active_menus[5].pos_y = 20;
    active_menus[5].width = 30;
    active_menus[5].height = 40;
    CHECK(point_is_over_gui_menu(0, 0) == MENU_INVALID_ID);
}
