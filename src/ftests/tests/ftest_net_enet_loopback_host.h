#pragma once

#include "globals.h"

#ifdef FUNCTESTING

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char TbBool;

// Host half of a real two-process ENet loopback session -- see
// ftest_net_enet_loopback_shared.h and
// docs/refactor/todo/ftest-fake-multiplayer.md (Phase 2). Only makes sense
// run together with ftest_net_enet_loopback_join_init() as a *separate*
// keeperfx process, via scripts/run_ftest_net_enet_loopback.sh -- run
// standalone, this test times out waiting for a client that never
// connects.
TbBool ftest_net_enet_loopback_host_init();

#ifdef __cplusplus
}
#endif

#endif // FUNCTESTING
