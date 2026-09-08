#include "globals.h"

#ifdef FUNCTESTING

#include "pre_inc.h"

#include "ftest.h"

/**
 * Add the header files for all tests below here
 */
#include "tests/ftest_template.h"
#include "tests/ftest_bug_imp_tp_job_attack_door.h"
#include "tests/ftest_bug_pathing_pillar_circling.h"
#include "tests/ftest_bug_imp_goldseam_dig.h"
#include "tests/ftest_bug_invisible_units_cant_select.h"
#include "tests/ftest_bug_pathing_stair_treasury.h"
#include "tests/ftest_bug_ai_bridge.h"
#include "tests/ftest_creature_combat_power_hand.h"
#include "tests/ftest_creature_temple_prayer.h"
#include "tests/ftest_creature_lair_healing.h"
#include "tests/ftest_creature_garden_eating.h"
#include "tests/ftest_creature_training.h"
#include "tests/ftest_creature_guard_post.h"
#include "tests/ftest_creature_barracks.h"
#include "tests/ftest_creature_prison_capture.h"
#include "tests/ftest_creature_torture_ownership.h"
#include "tests/ftest_net_resync_fake_multiplayer.h"
#include "tests/ftest_net_enet_loopback_host.h"
#include "tests/ftest_net_enet_loopback_join.h"
#include "tests/ftest_gui_packet_parity.h"
#include "tests/ftest_gui_seam_ingame.h"
// append your test include here, eg: #include "tests/ftest_your_test_header.h"

#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Append the name/init function of your test here so it can be found/executed.
 */
struct ftest_onlyappendtests__config ftest_onlyappendtests__conf = {

    // place regular tests in this list
    .tests_list = {
         { .test_name="example_template_test",              .init_func=ftest_template_init,                         .level_file="keeporig", .level=8,  .frame_skip=8 },
         { .test_name="bug_imp_tp_attack_door__claim",      .init_func=ftest_bug_imp_tp_attack_door__claim_init,    .level_file="deepdngn", .level=80, .frame_skip=8 },
         { .test_name="bug_imp_tp_attack_door__prisoner",   .init_func=ftest_bug_imp_tp_attack_door__prisoner_init, .level_file="deepdngn", .level=80, .frame_skip=8 },
         { .test_name="bug_imp_tp_attack_door__deadbody",   .init_func=ftest_bug_imp_tp_attack_door__deadbody_init, .level_file="deepdngn", .level=80, .frame_skip=8 },
         { .test_name="bug_imp_goldseam_dig",               .init_func=ftest_bug_imp_goldseam_dig_init,             .level_file="keeporig", .level=1,  .frame_skip=8 },
         { .test_name="bug_pathing_stair_treasury",         .init_func=ftest_bug_pathing_stair_treasury_init,       .level_file="keeporig", .level=1,  .frame_skip=8 },
         { .test_name="creature_combat_power_hand",         .init_func=ftest_creature_combat_power_hand_init,       .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="creature_temple_prayer",             .init_func=ftest_creature_temple_prayer_init,           .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="creature_lair_healing",              .init_func=ftest_creature_lair_healing_init,            .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="creature_garden_eating",             .init_func=ftest_creature_garden_eating_init,           .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="creature_training",                  .init_func=ftest_creature_training_init,                .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="creature_guard_post",                .init_func=ftest_creature_guard_post_init,              .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="creature_barracks",                  .init_func=ftest_creature_barracks_init,                .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="creature_prison_capture",             .init_func=ftest_creature_prison_capture_init,          .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="creature_torture_ownership",          .init_func=ftest_creature_torture_ownership_init,       .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="net_resync_fake_multiplayer",         .init_func=ftest_net_resync_fake_multiplayer_init,      .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="gui_packet_parity",                   .init_func=ftest_gui_packet_parity_init,                .level_file="keeporig", .level=11, .frame_skip=8 },
         { .test_name="gui_seam_ingame",                     .init_func=ftest_gui_seam_ingame_init,                  .level_file="keeporig", .level=11, .frame_skip=8 },

         // GUI/cursor-dependent, not headless-safe: drives mouse-cursor/thing-under-hand
         // selection (ftest_util_center_cursor_over_dungeon_view(), player->thing_under_hand)
         // which never reliably resolves without a real display, so it stalls (rather than
         // fails) under -headless -- confirmed against a real KeeperFX install, 6/7 other
         // registered tests pass in well under a minute combined while this one alone ran
         // past 3 minutes without completing a single one of its 10 repeat iterations. Since
         // -ftests (no name) runs every tests_list entry in one process, a stall here means
         // *no* test's coverage data survives (gcov only flushes .gcda on clean exit) --
         // commented out rather than left to intermittently wedge the KFX_FUNCTESTING+
         // KFX_TEST_COVERAGE `coverage` target (see CMakeLists.txt).
         // { .test_name="bug_invisible_units_cant_select", .init_func=ftest_bug_invisible_units_cant_select_init,  .level_file="keeporig", .level=1,  .frame_skip=0 },

         // WIP TEST { .test_name="bug_pathing_pillar_circling",        .init_func=ftest_bug_pathing_pillar_circling_init,      .level_file="keeporig", .level=1, .frame_skip=0 },
         // WIP TEST { .test_name="bug_invisible_units_cant_select",    .init_func=ftest_bug_invisible_units_cant_select_init,  .level_file="lostlvls", .level=103, .frame_skip=0 },
         // append your test to tests_list here, eg: { .test_name="your_test_name",    .init_func=ftest_your_test_name_init, .level_file="lostlvls", .level=103 },
    },

    // place long-running tests in this list, to include them use the -includelongtests flag
    .long_running_tests_list = {
        { .test_name="bug_ai_bridge",                      .init_func=ftest_bug_ai_bridge_init,                    .level_file="keeporig", .level=15, .frame_skip=128, .seed=1, .repeat_n_times=100 },

        // Not actually long-running -- placed here (rather than tests_list)
        // for the same reason bug_invisible_units_cant_select is commented
        // out above: a bare `-ftests` sweep runs every tests_list entry in
        // one process, and these two are each only one half of a real
        // two-process ENet session (docs/refactor/todo/ftest-fake-multiplayer.md
        // Phase 2) -- run alone, net_enet_loopback_host stalls waiting for a
        // client that never connects, which would wedge the same
        // KFX_FUNCTESTING+KFX_TEST_COVERAGE `coverage` target this comment's
        // neighbor above was excluded to protect. Run together via
        // scripts/run_ftest_net_enet_loopback.sh, not via -includelongtests.
        { .test_name="net_enet_loopback_host",              .init_func=ftest_net_enet_loopback_host_init,           .level_file="keeporig", .level=11, .frame_skip=8 },
        { .test_name="net_enet_loopback_join",              .init_func=ftest_net_enet_loopback_join_init,           .level_file="keeporig", .level=11, .frame_skip=8 },
    }
};


#ifdef __cplusplus
}
#endif

#endif

