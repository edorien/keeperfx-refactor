// kfx_sim-only accepted-symbol-residual stub: struct KeeperSprite's
// creature_table_add[] is declared in kfx_sim's own creature_graphics.h
// ("a higher-ranked library implementing a lower-ranked interface is
// fine -- only the reverse is a violation") but really *populated* in
// kfx_render's custom_sprites.c. See packet_test_stubs.cpp for the
// (shared, unlike this one) get_packet family stub and the full
// docs/refactor/todo/check-layering-symbol-level-blind-spot.md account.
//
// Deliberately NOT shared the way packet_test_stubs.cpp is: once a test
// target also links kfx_render (e.g. kfx_render_utest), custom_sprites.c
// provides the *real* creature_table_add[] -- linking this stub in
// alongside it would be a duplicate-definition error. Only kfx_sim_utest,
// which doesn't link kfx_render, needs this file.
#include "creature_graphics.h"

// Link-only stub, never exercised by the current test suite (nothing here
// tests creature_graphics.c's sprite-frame lookups). Sized 1, not the
// real KEEPERSPRITE_ADD_NUM (16383, kfx_render/engine_render.h) -- that
// constant lives above kfx_sim on the ladder, so kfx_sim's own headers
// (correctly) don't expose it here.
struct KeeperSprite creature_table_add[1];
