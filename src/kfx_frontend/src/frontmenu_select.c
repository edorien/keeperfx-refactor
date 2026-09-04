/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file frontmenu_select.c
 *     GUI menus for level and campaign select screens.
 * @par Purpose:
 *     Structures to show and maintain menus used for level and campaign list screens.
 * @par Comment:
 *     None.
 * @author   KeeperFX Team
 * @date     07 Dec 2012 - 11 Aug 2014
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "renderer/RendererManager.h"
#include "frontmenu_select.h"
#include "globals.h"
#include "bflib_basics.h"

#include "bflib_guibtns.h"
#include "bflib_sprite.h"
#include "bflib_sprfnt.h"
#include "bflib_video.h"
#include "bflib_vidraw.h"
#include "config_strings.h"
#include "game_saves.h"
#include "gui_draw.h"
#include "gui_frontbtns.h"
#include "gui_soundmsgs.h"
#include "player_data.h"
#include "packets.h"
#include "frontend.h"
#include "frontmenu_selectlist.h"
#include "front_landview.h"
#include "sprites.h"
#include "front_network.h"
#include "game_legacy.h"
#include "kfx_sim_state.h"
#include "kjm_input.h"
#include "highscores.h"
#include "post_inc.h"

/******************************************************************************/
static long frontend_level_select_count(void);
static long frontend_campaign_select_count(void);
static long frontend_mappack_select_count(void);
static long frontend_mp_mappack_select_count(void);

int number_of_freeplay_levels = 0;

// level_select_list uses its own row_base (FE_LEVEL_SELECTLIST_ROW_BASE,
// frontmenu_select.h) rather than the shared FE_SELECTLIST_ROW_BASE -- it's
// the only list ever visible alongside another one (mappack_select_list,
// on the merged Free play screen) and needs non-overlapping content.lval
// values to avoid cross-list hover-highlight bleed; see
// struct FrontendSelectList's row_base comment.
static struct FrontendSelectList level_select_list =
    { .items_visible_max = frontend_select_level_items_max_visible, .item_count = frontend_level_select_count, .row_base = FE_LEVEL_SELECTLIST_ROW_BASE };
static struct FrontendSelectList campaign_select_list =
    { .items_visible_max = frontend_select_campaign_items_max_visible, .item_count = frontend_campaign_select_count, .row_base = FE_SELECTLIST_ROW_BASE };
static struct FrontendSelectList mappack_select_list =
    { .items_visible_max = frontend_select_mappack_items_max_visible, .item_count = frontend_mappack_select_count, .row_base = FE_SELECTLIST_ROW_BASE };
static struct FrontendSelectList mp_mappack_select_list =
    { .items_visible_max = frontend_select_mp_mappack_items_max_visible, .item_count = frontend_mp_mappack_select_count, .row_base = FE_SELECTLIST_ROW_BASE };

// Merged Free play screen (mappack list + level list, both shown together
// on one screen instead of navigating between two): which mappack the list
// is currently highlighting, so a repeat click on the same row is a no-op
// and so the screen can auto-highlight the first mappack on entry.
static struct GameCampaign *freeplay_highlighted_mappack = NULL;

static long frontend_level_select_count(void)
{
    return number_of_freeplay_levels;
}

static long frontend_campaign_select_count(void)
{
    return campaigns_list.items_num;
}

static long frontend_mappack_select_count(void)
{
    return mappacks_list.items_num;
}

static long frontend_mp_mappack_select_count(void)
{
    return mp_mappacks_list.items_num;
}
/******************************************************************************/
void frontend_level_select_up(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_up(&level_select_list);
}

void frontend_level_select_down(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_down(&level_select_list);
}

void frontend_level_select_scroll(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_to_offset(&level_select_list, gbtn);
}

void frontend_level_select_up_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_up_maintain(&level_select_list, gbtn);
}

void frontend_level_select_down_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_down_maintain(&level_select_list, gbtn);
}

void frontend_level_select_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_row_maintain(&level_select_list, gbtn);
}

void frontend_draw_level_select_button(struct GuiButton *gbtn)
{
    long btn_idx = gbtn->content.lval;
    long i = btn_idx + level_select_list.scroll_offset - level_select_list.row_base;
    long lvnum = 0;
    if ((i >= 0) && (i < campaign.freeplay_levels_count))
      lvnum = campaign.freeplay_levels[i];
    struct LevelInformation* lvinfo = get_level_info(lvnum);
    if (lvinfo == NULL)
      return;
    if ((btn_idx > 0) && (frontend_mouse_over_button == btn_idx))
      i = 2;
    else
    if (get_level_highest_score(lvnum))
      i = 3;
    else
      i = 1;
    RendererSetDrawFlags(Lb_TEXT_HALIGN_LEFT);
    LbTextSetFont(frontend_font[i]);
    // This text is a bit condensed - button size is smaller than text height
    int tx_units_per_px = (gbtn->height * 13 / 11) * 16 / LbTextLineHeight();
    i = LbTextLineHeight() * tx_units_per_px / 16;
    LbTextSetWindow(gbtn->scr_pos_x, gbtn->scr_pos_y, gbtn->width, i);
    if (lvinfo->name_stridx > 0)
    {
        LbTextDrawResized(0, 0, tx_units_per_px, get_string(lvinfo->name_stridx));
    }
    else
    {
        LbTextDrawResized(0, 0, tx_units_per_px, lvinfo->name);
    }
}

void frontend_draw_levels_scroll_tab(struct GuiButton *gbtn)
{
    frontend_selectlist_draw_scroll_tab(&level_select_list, gbtn);
}

// Commits the level directly on click, same as the original single-list
// Free play screen -- no preview/highlight step for freeplay levels.
void frontend_level_select(struct GuiButton *gbtn)
{
    long i = frontend_selectlist_row_to_item_index(&level_select_list, gbtn);
    long lvnum = 0;
    if ((i >= 0) && (i < campaign.freeplay_levels_count))
      lvnum = campaign.freeplay_levels[i];
    if (lvnum <= 0)
        return;
    kfx_sim_state.selected_level_number = lvnum;
    frontend_set_state(FeSt_START_KPRLEVEL);
}

void frontend_level_list_unload(void)
{
  // Nothing needs to be really unloaded; just menu cleanup here
  number_of_freeplay_levels = 0;
}

void frontend_level_list_load(void)
{
    number_of_freeplay_levels = campaign.freeplay_levels_count;
    frontend_selectlist_set_visible(&level_select_list);
}

void frontend_level_select_update(void)
{
    frontend_selectlist_update(&level_select_list);
}

void frontend_draw_level_select_mappack(struct GuiButton *gbtn)
{
    const char *text;
    if (campaign.display_name[0] != '\0')
        text = campaign.display_name;
    else
        text = frontend_button_caption_text(gbtn);
    RendererSetDrawFlags(Lb_TEXT_HALIGN_LEFT);
    LbTextSetFont(frontend_font[2]);
    int tx_units_per_px;
    tx_units_per_px = gbtn->height * 16 / LbTextLineHeight();
    LbTextSetWindow(gbtn->scr_pos_x, gbtn->scr_pos_y, gbtn->width, gbtn->height);
    LbTextDrawResized(0, 0, tx_units_per_px, text);
}

void frontend_campaign_select_up(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_up(&campaign_select_list);
}

void frontend_campaign_select_down(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_down(&campaign_select_list);
}

void frontend_campaign_select_scroll(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_to_offset(&campaign_select_list, gbtn);
}

void frontend_campaign_select_up_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_up_maintain(&campaign_select_list, gbtn);
}

void frontend_campaign_select_down_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_down_maintain(&campaign_select_list, gbtn);
}

void frontend_campaign_select_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_row_maintain(&campaign_select_list, gbtn);
}

void frontend_draw_campaign_select_button(struct GuiButton *gbtn)
{
    if (gbtn == NULL)
      return;
    long btn_idx = gbtn->content.lval;
    long i = frontend_selectlist_row_to_item_index(&campaign_select_list, gbtn);
    struct GameCampaign* campgn = NULL;
    if ((i >= 0) && (i < campaigns_list.items_num))
      campgn = &campaigns_list.items[i];
    if (campgn == NULL)
      return;
    if ((btn_idx > 0) && (frontend_mouse_over_button == btn_idx))
      i = 2;
    else
/*    if (campaign has been passed)
      i = 3;
    else*/
      i = 1;
    RendererSetDrawFlags(Lb_TEXT_HALIGN_LEFT);
    LbTextSetFont(frontend_font[i]);
    // This text is a bit condensed - button size is smaller than text height
    int tx_units_per_px = (gbtn->height * 13 / 11) * 16 / LbTextLineHeight();
    i = LbTextLineHeight() * tx_units_per_px / 16;
    LbTextSetWindow(gbtn->scr_pos_x, gbtn->scr_pos_y, gbtn->width, i);
    LbTextDrawResized(0, 0, tx_units_per_px, campgn->display_name);
}

void frontend_campaign_select(struct GuiButton *gbtn)
{
    if (gbtn == NULL)
        return;
    long i = frontend_selectlist_row_to_item_index(&campaign_select_list, gbtn);
    struct GameCampaign* campgn = NULL;
    if ((i >= 0) && (i < campaigns_list.items_num))
        campgn = &campaigns_list.items[i];
    if (campgn == NULL)
        return;
    if (!frontend_start_new_campaign(campgn->fname))
    {
        ERRORLOG("Unable to start new campaign");
        return;
    }
    frontend_set_state(FeSt_CAMPAIGN_INTRO);
}

void frontend_campaign_select_update(void)
{
    frontend_selectlist_update(&campaign_select_list);
}

void frontend_draw_campaign_scroll_tab(struct GuiButton *gbtn)
{
    frontend_selectlist_draw_scroll_tab(&campaign_select_list, gbtn);
}

/** Switches which mappack is highlighted: loads it (change_campaign) and
 * refreshes the level list to its levels (restarting scroll at the top --
 * it's about to show a different set of levels than whatever the previous
 * mappack's list was scrolled to). Shared by the mappack row's click_event
 * and the screen's own entry point.
 */
static void freeplay_highlight_mappack(struct GameCampaign *campgn)
{
    if (!change_campaign(CampgnT_Mappack, campgn->fname))
        return;
    freeplay_highlighted_mappack = campgn;
    level_select_list.scroll_offset = 0;
    frontend_level_list_load();
}

// Runs on every entry into FeSt_MAPPACK_SELECT (frontend_setup_state calls
// it unconditionally), not just this menu's first-ever creation.
void frontend_mappack_list_load(void)
{
    frontend_selectlist_set_visible(&mappack_select_list);
    freeplay_highlighted_mappack = NULL;
    if (mappacks_list.items_num > 0)
        freeplay_highlight_mappack(&mappacks_list.items[0]);
}

void frontend_mappack_select_up(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_up(&mappack_select_list);
}

void frontend_mappack_select_down(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_down(&mappack_select_list);
}

void frontend_mappack_select_scroll(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_to_offset(&mappack_select_list, gbtn);
}

void frontend_mappack_select_up_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_up_maintain(&mappack_select_list, gbtn);
}

void frontend_mappack_select_down_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_down_maintain(&mappack_select_list, gbtn);
}

void frontend_mappack_select_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_row_maintain(&mappack_select_list, gbtn);
}

// Highlights a mappack under a clicked row (see freeplay_highlight_mappack)
// rather than committing/transitioning screens -- the merged Free play
// screen shows both lists together.
void frontend_mappack_select(struct GuiButton *gbtn)
{
    if (gbtn == NULL)
        return;
    long i = frontend_selectlist_row_to_item_index(&mappack_select_list, gbtn);
    struct GameCampaign *campgn = NULL;
    if ((i >= 0) && (i < mappacks_list.items_num))
        campgn = &mappacks_list.items[i];
    if (campgn == NULL)
        return;
    if (campgn == freeplay_highlighted_mappack)
        return; // already highlighted, nothing to reload
    freeplay_highlight_mappack(campgn);
}


void frontend_mp_mappack_list_load(void)
{
    // See frontend_mappack_list_load(): entering multiplayer map-pack
    // select likewise swaps in a new freeplay level list, which must
    // restart at the top.
    level_select_list.scroll_offset = 0;
    frontend_selectlist_set_visible(&mp_mappack_select_list);
}

void frontend_mp_mappack_select_up(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_up(&mp_mappack_select_list);
}

void frontend_mp_mappack_select_down(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_down(&mp_mappack_select_list);
}

void frontend_mp_mappack_select_scroll(struct GuiButton *gbtn)
{
    frontend_selectlist_scroll_to_offset(&mp_mappack_select_list, gbtn);
}

void frontend_mp_mappack_select_up_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_up_maintain(&mp_mappack_select_list, gbtn);
}

void frontend_mp_mappack_select_down_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_down_maintain(&mp_mappack_select_list, gbtn);
}

void frontend_mp_mappack_select_maintain(struct GuiButton *gbtn)
{
    frontend_selectlist_row_maintain(&mp_mappack_select_list, gbtn);
}

void frontend_mp_mappack_select(struct GuiButton *gbtn)
{
    long i;
    struct GameCampaign *campgn;
    if (gbtn == NULL)
        return;
    i = frontend_selectlist_row_to_item_index(&mp_mappack_select_list, gbtn);
    campgn = NULL;
    if ((i >= 0) && (i < mp_mappacks_list.items_num))
        campgn = &mp_mappacks_list.items[i];
    if (campgn == NULL)
        return;

    frontnet_send_campaign_change_message(campgn->fname);

    if (!change_campaign(CampgnT_MultiplayerMappack, campgn->fname))
        return;
    if (net_service_index_selected == FrontendNetSvc_Skirmish)
    {
        frontend_set_state(FeSt_NETLAND_VIEW);
    }
    else
    {
        frontend_set_state(FeSt_NET_START);
    }
}

void frontend_back_from_mp_mappack_list(struct GuiButton *gbtn)
{
    if (net_service_index_selected == FrontendNetSvc_Skirmish)
    {
        frontend_set_state(FeSt_NET_SERVICE);
    }
    else
    {
        frontend_set_state(FeSt_NET_START);
    }
}

void frontend_draw_mp_mappack_select_button(struct GuiButton *gbtn)
{
    struct GameCampaign *campgn;
    long btn_idx;
    long i;
    if (gbtn == NULL)
      return;
    btn_idx = gbtn->content.lval;
    i = frontend_selectlist_row_to_item_index(&mp_mappack_select_list, gbtn);
    campgn = NULL;
    if ((i >= 0) && (i < mp_mappacks_list.items_num))
      campgn = &mp_mappacks_list.items[i];
    if (campgn == NULL)
      return;
    if ((btn_idx > 0) && (frontend_mouse_over_button == btn_idx))
      i = 2;
    else
      i = 1;

    RendererSetDrawFlags(Lb_TEXT_HALIGN_LEFT);
    LbTextSetFont(frontend_font[i]);
    int tx_units_per_px;
    // This text is a bit condensed - button size is smaller than text height
    tx_units_per_px = (gbtn->height*13/11) * 16 / LbTextLineHeight();
    i = LbTextLineHeight() * tx_units_per_px / 16;
    LbTextSetWindow(gbtn->scr_pos_x, gbtn->scr_pos_y, gbtn->width, i);
    LbTextDrawResized(0, 0, tx_units_per_px, campgn->display_name);
}

void frontend_draw_mappack_select_button(struct GuiButton *gbtn)
{
    struct GameCampaign *campgn;
    long btn_idx;
    long i;
    if (gbtn == NULL)
      return;
    btn_idx = gbtn->content.lval;
    i = frontend_selectlist_row_to_item_index(&mappack_select_list, gbtn);
    campgn = NULL;
    if ((i >= 0) && (i < mappacks_list.items_num))
      campgn = &mappacks_list.items[i];
    if (campgn == NULL)
      return;
    if ((btn_idx > 0) && (frontend_mouse_over_button == btn_idx))
      i = 2;
    else
      i = 1;

    RendererSetDrawFlags(Lb_TEXT_HALIGN_LEFT);
    LbTextSetFont(frontend_font[i]);
    int tx_units_per_px;
    // This text is a bit condensed - button size is smaller than text height
    tx_units_per_px = (gbtn->height*13/11) * 16 / LbTextLineHeight();
    i = LbTextLineHeight() * tx_units_per_px / 16;
    LbTextSetWindow(gbtn->scr_pos_x, gbtn->scr_pos_y, gbtn->width, i);
    LbTextDrawResized(0, 0, tx_units_per_px, campgn->display_name);
}

void frontend_mappack_select_update(void)
{
    frontend_selectlist_update(&mappack_select_list);
}

void frontend_mp_mappack_select_update(void)
{
    frontend_selectlist_update(&mp_mappack_select_list);
}

void frontend_draw_mappack_scroll_tab(struct GuiButton *gbtn)
{
    frontend_selectlist_draw_scroll_tab(&mappack_select_list, gbtn);
}

void frontend_draw_mp_mappack_scroll_tab(struct GuiButton *gbtn)
{
    frontend_selectlist_draw_scroll_tab(&mp_mappack_select_list, gbtn);
}

void frontend_campaign_list_load(void)
{
    frontend_selectlist_set_visible(&campaign_select_list);
}

void frontend_draw_variable_mappack_exit_button(struct GuiButton *gbtn)
{
    long str_idx = FEBtn_MnuReturnToFreePlay;
    unsigned short mnu_idx = 34; //map pack selection screen
    if (mappacks_list.items_num == 1)
    {
        str_idx = FEBtn_MnuReturnToMain;
        mnu_idx = 1; //main menu
    }
    gbtn->btype_value = mnu_idx;
    gbtn->content.lval = str_idx;
    const char *text;
    text = frontend_button_caption_text(gbtn);
    frontend_draw_button(gbtn, 1, text, Lb_TEXT_HALIGN_CENTER);
}
/******************************************************************************/
