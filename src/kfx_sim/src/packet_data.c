/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file packet_data.c
 *     struct Packet's storage and trivial accessors -- see packet_data.h.
 * @par Purpose:
 *     Moved down from kfx_net's packets.c/packets_misc.c/kfx_net_state.h
 *     (docs/refactor/todo/remove-symbol-level-layering-residuals.md):
 *     sim_packets[]/bad_packet and these four accessors never actually
 *     needed anything net-specific, just get_player() (kfx_sim) and
 *     bounds checks, so nothing about them justified kfx_sim/kfx_render
 *     reaching up into kfx_net to call them. kfx_net's own
 *     packets.c/packets_misc.c/net_exchange_gameplay.c keep writing into
 *     sim_packets[] directly for their own genuinely net-owned logic
 *     (checksums, wire buffer packing, turn-history exchange) -- that's a
 *     higher-ranked library writing into a lower-ranked library's state,
 *     which is fine; only the reverse (what this file replaces) was the
 *     violation.
 * @par Comment:
 *     None.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "packet_data.h"

#include "globals.h"
#include "bflib_basics.h"
#include "player_data.h"
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct Packet sim_packets[PACKETS_COUNT];
struct Packet bad_packet;

/**
 * Gives a pointer for the player's packet.
 * @param plyr_idx The player index for which we want the packet.
 * @return Returns Packet pointer. On error, returns a dummy structure.
 */
struct Packet *get_packet(long plyr_idx)
{
    struct PlayerInfo* player = get_player(plyr_idx);
    if (player_invalid(player))
        return INVALID_PACKET;
    if (player->packet_num >= PACKETS_COUNT)
        return INVALID_PACKET;
    return &sim_packets[player->packet_num];
}

/**
 * Gives a pointer to packet of given index.
 * @param pckt_idx Packet index in the array. Note that it may differ from player index.
 * @return Returns Packet pointer. On error, returns a dummy structure.
 */
struct Packet *get_packet_direct(long pckt_idx)
{
    if ((pckt_idx < 0) || (pckt_idx >= PACKETS_COUNT))
        return INVALID_PACKET;
    return &sim_packets[pckt_idx];
}

void set_packet_action(struct Packet *pckt, unsigned char pcktype, long par1, long par2, unsigned short par3, unsigned short par4)
{
    pckt->actn_par1 = par1;
    pckt->actn_par2 = par2;
    pckt->actn_par3 = par3;
    pckt->actn_par4 = par4;
    pckt->action = pcktype;
}

void set_players_packet_action(struct PlayerInfo *player, unsigned char pcktype,
        unsigned long par1, unsigned long par2, unsigned short par3, unsigned short par4)
{
    struct Packet* pckt = get_packet_direct(player->packet_num);
    pckt->actn_par1 = par1;
    pckt->actn_par2 = par2;
    pckt->actn_par3 = par3;
    pckt->actn_par4 = par4;
    pckt->action = pcktype;
}
/******************************************************************************/
#ifdef __cplusplus
}
#endif
