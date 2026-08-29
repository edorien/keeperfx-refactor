// Shared accepted-symbol-residual stub (docs/refactor/todo/
// check-layering-symbol-level-blind-spot.md; check_layering_symbols.py's
// ACCEPTED_SYMBOL_VIOLATIONS): struct Packet's *type* is declared in
// kfx_sim's own packet_data.h by design ("a higher-ranked library
// implementing a lower-ranked interface is fine -- only the reverse is a
// violation"), but get_packet/get_packet_direct/set_packet_action/
// set_players_packet_action are really implemented in kfx_net's
// packets.c. Production binaries never notice, since kfx_net is always
// linked into the same executable.
//
// A STATIC library, not a source compiled directly into kfx_sim_utest:
// every *_utest target that links kfx_sim (directly, like kfx_sim_utest
// itself, or transitively, like kfx_render_utest) hits this same gap and
// needs it too -- see docs/refactor/testing/stage-04d-kfx-render.md.
// Unlike creature_table_add[] (kfx_sim_test_stubs.cpp, kept separate and
// NOT shared this way), nothing here stops being a stub once a
// higher-ranked library is also linked in -- kfx_net itself is never
// pulled into these test binaries (see stage-04c-kfx-sim.md for why:
// kfx_net has its own accepted residual reaching into kfx_game/
// kfx_frontend that would cascade the problem further).
#include "packet_data.h"
#include "player_data.h"

namespace {
struct Packet stub_packet{};
}

extern "C" {

struct Packet *get_packet_direct(long pckt_idx)
{
    (void)pckt_idx;
    return &stub_packet;
}

struct Packet *get_packet(long plyr_idx)
{
    (void)plyr_idx;
    return &stub_packet;
}

void set_packet_action(struct Packet *pckt, unsigned char pcktype, long par1, long par2, unsigned short par3, unsigned short par4)
{
    (void)pckt; (void)pcktype; (void)par1; (void)par2; (void)par3; (void)par4;
}

void set_players_packet_action(struct PlayerInfo *player, unsigned char pcktype, unsigned long par1, unsigned long par2, unsigned short par3, unsigned short par4)
{
    (void)player; (void)pcktype; (void)par1; (void)par2; (void)par3; (void)par4;
}

} // extern "C"
