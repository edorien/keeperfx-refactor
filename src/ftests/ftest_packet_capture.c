#include "ftest_packet_capture.h"

#ifdef FUNCTESTING

#include "pre_inc.h"

#include <string.h>

#include "ftest.h"

#include "config_keeperfx.h"
#include "packet_data.h"
#include "player_data.h"

#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif

static struct FtestCapturedPacket s_buf[FTEST_PACKET_CAPTURE_MAX];
static int s_count = 0;
static TbBool s_recording = 0;
static TbBool s_have_last_turn = 0;
static GameTurn s_last_turn = 0; /* capture at most once per game turn */

void ftest_packet_capture_begin(void)
{
    s_count = 0;
    s_have_last_turn = 0;
    s_recording = 1;
}

void ftest_packet_capture_end(void)
{
    s_recording = 0;
}

void ftest_packet_capture_reset(void)
{
    s_count = 0;
    s_have_last_turn = 0;
    s_recording = 0;
}

/* A command packet the GUI (or any local input) actually produced. Empty
 * turns -- mouse-move-only, or PCtr_Gui-hover with no action -- are not
 * recorded, so a trace is exactly the sequence of player actions and
 * stays comparable across the classic/ImGui paths regardless of where the
 * cursor happened to rest. */
static TbBool packet_is_interesting(const struct Packet *p)
{
    return p->action != PckA_None;
}

void ftest_packet_capture_tick(void)
{
    if (!s_recording)
        return;

    const GameTurn turn = get_gameturn();
    if (s_have_last_turn && turn == s_last_turn)
        return;
    s_last_turn = turn;
    s_have_last_turn = 1;

    const struct Packet *p = &sim_packets[my_player_number];
    if (!packet_is_interesting(p))
        return;

    if (s_count >= FTEST_PACKET_CAPTURE_MAX)
    {
        FTESTLOG("buffer full (%d), dropping turn %lu", FTEST_PACKET_CAPTURE_MAX, (unsigned long)turn);
        return;
    }

    s_buf[s_count].turn = turn;
    memcpy(&s_buf[s_count].packet, p, sizeof(struct Packet));
    s_count++;
}

int ftest_packet_capture_count(void)
{
    return s_count;
}

const struct FtestCapturedPacket *ftest_packet_capture_at(int idx)
{
    if (idx < 0 || idx >= s_count)
        return NULL;
    return &s_buf[idx];
}

void ftest_packet_capture_dump(void)
{
    FTESTLOG("captured %d packet(s)", s_count);
    for (int i = 0; i < s_count; i++)
    {
        const struct Packet *p = &s_buf[i].packet;
        FTESTLOG("  [%d] turn=%lu action=%u par=%d/%d/%d/%d ctrl=%08x gui=%d",
                 i, (unsigned long)s_buf[i].turn, (unsigned)p->action,
                 (int)p->actn_par1, (int)p->actn_par2, (int)p->actn_par3, (int)p->actn_par4,
                 (unsigned)p->control_flags, (int)((p->control_flags & PCtr_Gui) != 0));
    }
}

static TbBool par_ok(int32_t want, int32_t got)
{
    return (want == FTEST_PKT_ANY) || (want == got);
}

static TbBool pars_ok(const struct Packet *p, int32_t par1, int32_t par2, int32_t par3, int32_t par4)
{
    return par_ok(par1, p->actn_par1) && par_ok(par2, p->actn_par2)
        && par_ok(par3, (int32_t)p->actn_par3) && par_ok(par4, (int32_t)p->actn_par4);
}

TbBool ftest_packet_expect_once(unsigned char action,
                                int32_t par1, int32_t par2, int32_t par3, int32_t par4)
{
    int matches = 0;
    for (int i = 0; i < s_count; i++)
    {
        const struct Packet *p = &s_buf[i].packet;
        if (p->action == action && pars_ok(p, par1, par2, par3, par4))
            matches++;
    }
    if (matches != 1)
    {
        FTEST_FAIL_TEST("expected exactly 1 packet with action=%u pars=%d/%d/%d/%d, found %d",
                        (unsigned)action, (int)par1, (int)par2, (int)par3, (int)par4, matches);
        ftest_packet_capture_dump();
        return 0;
    }
    return 1;
}

TbBool ftest_packet_expect_absent(unsigned char action)
{
    for (int i = 0; i < s_count; i++)
    {
        if (s_buf[i].packet.action == action)
        {
            FTEST_FAIL_TEST("expected no packet with action=%u, found one at turn %lu",
                            (unsigned)action, (unsigned long)s_buf[i].turn);
            ftest_packet_capture_dump();
            return 0;
        }
    }
    return 1;
}

TbBool ftest_packet_trace_matches(const struct FtestPacketExpectation *expect, int n)
{
    int ai = 0;
    for (int i = 0; i < s_count; i++)
    {
        const struct Packet *p = &s_buf[i].packet;
        if (p->action == PckA_None)
            continue;

        if (ai >= n)
        {
            FTEST_FAIL_TEST("trace has more than %d action packet(s)", n);
            ftest_packet_capture_dump();
            return 0;
        }

        const struct FtestPacketExpectation *e = &expect[ai];
        const TbBool gui_ok = !e->require_gui_flag || ((p->control_flags & PCtr_Gui) != 0);
        if (p->action != e->action || !pars_ok(p, e->par1, e->par2, e->par3, e->par4) || !gui_ok)
        {
            FTEST_FAIL_TEST("trace[%d] mismatch: got action=%u pars=%d/%d/%d/%d gui=%d",
                            ai, (unsigned)p->action,
                            (int)p->actn_par1, (int)p->actn_par2, (int)p->actn_par3, (int)p->actn_par4,
                            (int)((p->control_flags & PCtr_Gui) != 0));
            ftest_packet_capture_dump();
            return 0;
        }
        ai++;
    }
    if (ai != n)
    {
        FTEST_FAIL_TEST("trace has %d action packet(s), expected %d", ai, n);
        ftest_packet_capture_dump();
        return 0;
    }
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* FUNCTESTING */
