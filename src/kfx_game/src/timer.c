/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file timer.c
 *     Level-load timing support functions. See timer.h.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "timer.h"

#include "globals.h"
#include "bflib_datetm.h"

#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
static TbClockMSec level_load_times[LevelLoadTime_Count];
static TbClockMSec level_load_total_start;
static TbClockMSec level_load_phase_start;
static enum LevelLoadTimeKind level_load_phase;
static TbBool level_load_time_active;
/******************************************************************************/

void level_load_time_phase(enum LevelLoadTimeKind kind)
{
    TbClockMSec now = LbTimerClock();
    if (kind == LevelLoadTime_EngineStartup && !level_load_time_active) {
        memset(level_load_times, 0, sizeof(level_load_times));
        level_load_total_start = now;
        level_load_phase_start = now;
        level_load_phase = LevelLoadTime_EngineStartup;
        level_load_time_active = true;
        return;
    }
    if (!level_load_time_active)
        return;
    level_load_times[level_load_phase] += now - level_load_phase_start;
    if (kind == LevelLoadTime_Total) {
        level_load_times[LevelLoadTime_Total] = now - level_load_total_start;
        JUSTLOG("Level load timing: Engine startup: %d ms, Custom sprites: %d ms, Config files: %d ms, Level data: %d ms, Navigation: %d ms, Game setup: %d ms, Total: %d ms", level_load_times[LevelLoadTime_EngineStartup], level_load_times[LevelLoadTime_Sprites], level_load_times[LevelLoadTime_Configs], level_load_times[LevelLoadTime_Data], level_load_times[LevelLoadTime_Navigation], level_load_times[LevelLoadTime_GameSetup], level_load_times[LevelLoadTime_Total]);
        level_load_time_active = false;
        return;
    }
    level_load_phase = kind;
    level_load_phase_start = now;
}
/******************************************************************************/
#ifdef __cplusplus
}
#endif
