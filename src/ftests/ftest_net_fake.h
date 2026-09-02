#pragma once

#include "globals.h"

#ifdef FUNCTESTING

#include "net_main.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A fake, in-process struct NetSP that stands in for InitEnetSP()
 * (bflib_enet.cpp) so a single ftest process can drive the real netcode
 * (net_resync.cpp, net_exchange_*.c, packets*.c) through netstate.sp
 * without any real sockets or a second process.
 *
 * Modeled as one mailbox per NetUserId, keyed the same way the real
 * NetSP's msgready()/readmsg(source, ...) are: by the identity of
 * whoever sent the message, not who it's addressed to. Since only one
 * identity is ever "active" in a single process at a time, the caller
 * declares which side it's currently playing via
 * ftest_net_fake_set_role_host()/ftest_net_fake_set_role_client() before
 * driving code that calls netstate.sp; a message sent while playing one
 * role becomes readable once the caller switches role and reads from the
 * matching source id (SERVER_ID for the host's mailbox, or the id passed
 * to ftest_net_fake_set_role_client() for a client's).
 *
 * Delivery is instant and unconditional -- there's no real transport to
 * fail, drop, reorder, or wait on. This only fakes the NetSP contract,
 * not a live two-sided session (no real second process, no ENet, no
 * timing) -- see docs/refactor/todo/ftest-fake-multiplayer.md for the
 * full design and what's deliberately out of scope.
 */
struct NetSP *ftest_net_fake_sp(void);

/**
 * @brief Declares which identity subsequent netstate.sp calls act as.
 * Only affects which mailbox sendmsg_single()/sendmsg_all() write into --
 * msgready()/readmsg() always read the mailbox named by their own
 * `source` argument, same as the real NetSP.
 */
void ftest_net_fake_set_role_host(void);
void ftest_net_fake_set_role_client(NetUserId as_id);

/**
 * @brief Drops every queued fake message and resets the active role back
 * to host. Call before installing the fake and again after restoring the
 * real netstate.sp, so a later test starts from a clean slate.
 */
void ftest_net_fake_reset(void);

#ifdef __cplusplus
}
#endif

#endif // FUNCTESTING
