/**
 * @file ftest_gui_seam_ingame.h
 * @brief Phase 0 of the in-game GUI -> Dear ImGui migration
 * (docs/refactor/ingame-gui/01-seam-and-toggle.md): the per-GMnu_* opt-in
 * seam, proven with the migrated quit-confirm menu (GMnu_QUIT).
 *
 * Asserts the seam predicates report the migrated menu correctly, that the
 * legacy menu-stack machinery (turn_on_menu / menu_is_active) is untouched,
 * and that the migrated "Yes" action produces the same command packet
 * (PckA_QuitToMainMenu) the legacy gui_quit_game() sends.
 *
 * This test quits the level as its final act, so it is registered on its
 * own rather than folded into ftest_gui_packet_parity.
 */
#pragma once

#include "globals.h"

#ifdef FUNCTESTING

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char TbBool;

TbBool ftest_gui_seam_ingame_init();

#ifdef __cplusplus
}
#endif

#endif /* FUNCTESTING */
