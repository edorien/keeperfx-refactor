#pragma once

#include "globals.h"

#ifdef FUNCTESTING

// Shared by ftest_net_enet_loopback_host.c and ftest_net_enet_loopback_join.c
// (docs/refactor/todo/ftest-fake-multiplayer.md Phase 2): these two tests
// are two halves of one real ENet loopback session and only make sense run
// together, as two separate keeperfx processes, coordinated by
// scripts/run_ftest_net_enet_loopback.sh -- neither passes anything by
// itself. A fixed port (rather than an OS-assigned ephemeral one) keeps
// this simple: there's no cross-process channel to hand a dynamically
// chosen port from the host process to the join process, and the two
// processes otherwise share nothing (see the plan doc's "port allocation"
// open question for the CI-collision risk this accepts).
#define FTEST_NET_ENET_LOOPBACK_PORT 23477
#define FTEST_NET_ENET_LOOPBACK_JOIN_ADDRESS "127.0.0.1:23477"
#define FTEST_NET_ENET_LOOPBACK_JOIN_USER_ID 1

#define FTEST_NET_ENET_LOOPBACK_PING_MSG "PING-FROM-HOST"
#define FTEST_NET_ENET_LOOPBACK_PONG_MSG "PONG-FROM-JOIN"

// After the ping/pong round trip, the host also exercises sendmsg_all()
// (bf_enet_sendmsg_all(), previously uncovered) and
// sendmsg_single_unsequenced() (bf_enet_sendmsg_single_unsequenced(), same)
// -- two more messages the join side needs to read before either side tears
// its connection down, so the message count and content are shared here too.
#define FTEST_NET_ENET_LOOPBACK_BROADCAST_MSG "BROADCAST-FROM-HOST"
#define FTEST_NET_ENET_LOOPBACK_UNSEQUENCED_MSG "UNSEQUENCED-FROM-HOST"

// The host waits for this before calling drop_user()/exit() -- without it,
// the host could disconnect before the join side has actually pulled the
// two messages above out of its own kernel socket buffer and into ENet's
// internal queue (still polling, on its own schedule, across separate
// processes); a disconnect event processed mid-poll on the join side wipes
// its still-unread incoming queue (destroy_incoming_queue(), bflib_enet.cpp),
// which would silently drop a message that had technically already arrived.
// Same shape as the ping/pong exchange's own settle-before-exit fix, just
// with an explicit ack instead of a blind timeout, since here the host
// needs to *know* the join side is done, not just guess at how long that
// takes.
#define FTEST_NET_ENET_LOOPBACK_EXTRAS_ACK_MSG "EXTRAS-ACK-FROM-JOIN"

// Real network I/O over real time -- these turn budgets are generous
// (loopback connects/round-trips in well under a second in practice) but
// bounded, so a genuine failure (e.g. run standalone without its other
// half) still exits rather than hanging the process forever.
#define FTEST_NET_ENET_LOOPBACK_TURN_BUDGET 600

#endif // FUNCTESTING
