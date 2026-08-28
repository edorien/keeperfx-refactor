/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/**
 * @author   KeeperFX Team
 * @date     18 Oct 2022
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/

#ifndef GIT_BFLIB_ENET_H
#define GIT_BFLIB_ENET_H

#include <stdint.h>
#include <stddef.h>
#include "bflib_basics.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENET_DEFAULT_PORT 5556

enum {
    ENET_CHANNEL_RELIABLE = 0,
    ENET_CHANNEL_UNSEQUENCED = 1
};

struct NetSP;
struct NetSP* InitEnetSP();
unsigned long GetPing(int id, int local_player_id);
unsigned int GetPacketLoss(int id, int local_player_id);
unsigned int GetClientDataInTransit();
unsigned int GetClientPacketsLost();
unsigned int GetUploadRateBytesPerSecond();
unsigned int GetDownloadRateBytesPerSecond();
int enet_matchmaking_host_update(void);
extern uint16_t external_ipv4_port;
extern int skip_holepunch;
uint16_t enet_get_bound_ipv6_port(void);

struct _ENetHost;
struct _ENetAddress;

// Mirrors net_matchmaking.h's PunchAddresses (same field sizes/order) so
// this header doesn't need to include it. See EnetConnectivityServices
// below and docs/refactor/stage-02-decouple-bflib.md.
#define ENET_MATCHMAKING_ID_MAX 64
#define ENET_MATCHMAKING_IP_MAX 64

struct EnetPunchAddresses {
    char ipv4[ENET_MATCHMAKING_IP_MAX];
    char ipv6[ENET_MATCHMAKING_IP_MAX];
    int ipv4_port;
    int ipv6_port;
};

// Injected by the net layer so bflib_enet.cpp (platform layer) doesn't
// reach upward into front_network.h/net_holepunch.h/net_matchmaking.h/
// net_portforward.h directly. See docs/refactor/stage-02-decouple-bflib.md.
struct EnetConnectivityServices {
    void (*display_attempting_to_join_message)(int seconds_remaining);
    TbBool (*attempting_to_join_cancel_requested)(void);
    uint16_t (*holepunch_stun_query)(struct _ENetHost *host, char *output_ip, size_t output_ip_buffer_size);
    void (*holepunch_punch_to)(struct _ENetHost *host, const struct _ENetAddress *target);
    int (*matchmaking_punch)(const char *lobby_id, int udp_ipv4_port, int udp_ipv6_port, struct EnetPunchAddresses *output);
    int (*matchmaking_poll_punch)(struct EnetPunchAddresses *output);
    int (*port_forward_add_mapping)(uint16_t port);
    void (*port_forward_remove_mapping)(void);
};

void bf_enet_set_connectivity_services(const struct EnetConnectivityServices *services);

// Sets the pending join target (lobby id, or "LAN:<ip>:<port>", or empty
// for a plain direct-connect session string). Mirrors net_matchmaking.h's
// join_lobby_id global, owned by the net layer.
void bf_enet_set_join_lobby_id(const char *lobby_id);

#ifdef __cplusplus
}
#endif

#endif //GIT_BFLIB_ENET_H
