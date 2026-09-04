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
#include "frontmenu_landpreview.h"
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

// Land selection screen (merged campaign list + interactive land preview):
// which campaign the list is currently highlighting -- NULL until
// frontend_campaign_list_load/frontend_campaign_select first sets it.
// land_preview.highlighted_lvnum (frontmenu_landpreview.h) tracks the
// finer-grained "which level's ensign was clicked" on top of this.
static struct GameCampaign *land_selection_highlighted_campaign = NULL;

// Merged Free play screen (mappack list + level list + land preview):
// which mappack the list is currently highlighting, and which level
// within it -- both NULL/0 until frontend_mappack_list_load/
// frontend_mappack_select/frontend_level_select first set them. Unlike
// Land selection, land_preview.highlighted_lvnum isn't reused for this:
// that field tracks an ensign clicked *inside* the preview panel, and
// this screen's preview never shows ensigns (see
// LandPreviewPanel.show_ensigns) -- level highlight comes from the level
// list instead.
static struct GameCampaign *freeplay_highlighted_mappack = NULL;
static LevelNumber freeplay_highlighted_level = 0;

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
    // frontend_draw_scroll_tab (frontend.cpp) only ever draws the movable
    // thumb sprite -- never a groove/track of its own. The ornate
    // GFS_scrollbar_vert_ct_long/short groove sprite (what the classic
    // frontend_draw_scroll_box's own draw_scrollbar=true mode uses) reads
    // as a free-floating fragment sitting on its own in this screen's
    // narrower column, without the rest of that box's border around it --
    // a plain thin line reads better here, with no up/down arrows drawn at
    // all (see this screen's up/down buttons, which no longer have a
    // draw_call).
    frontend_draw_simple_scroll_track(gbtn);
    frontend_selectlist_draw_scroll_tab(&level_select_list, gbtn);
}

// Highlights a level under a clicked row: loads its own land preview
// (no ensigns -- see LandPreviewPanel.show_ensigns). Committing is
// frontend_freeplay_enter's job -- see the header.
void frontend_level_select(struct GuiButton *gbtn)
{
    long i = frontend_selectlist_row_to_item_index(&level_select_list, gbtn);
    long lvnum = 0;
    if ((i >= 0) && (i < campaign.freeplay_levels_count))
      lvnum = campaign.freeplay_levels[i];
    if (lvnum <= 0)
        return;
    if (lvnum == freeplay_highlighted_level)
        return;
    freeplay_highlighted_level = lvnum;
    land_preview_load(&land_preview, lvnum, false);
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

/** Highlights a campaign under a clicked list row: loads its land preview
 * and resets the detail panel to the campaign's own description.
 * Committing is frontend_land_selection_enter's job -- see the header.
 */
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
    if (campgn == land_selection_highlighted_campaign)
        return; // already highlighted, nothing to reload
    if (!change_campaign(CampgnT_Campaign, campgn->fname))
    {
        ERRORLOG("Unable to load campaign for land preview");
        return;
    }
    land_selection_highlighted_campaign = campgn;
    // land_preview_load() releases the previous ensign sheet itself and
    // doesn't need land_preview_unload() first -- that also tears down
    // gameplay state meant for actually leaving this screen (see its
    // comment), which running on every campaign click would be wasteful
    // and risk the crash-after-several-switches class of bug.
    land_preview_load(&land_preview, SINGLEPLAYER_NOTSTARTED, true);
}

/** "Enter this land": commits whichever the detail panel is currently
 * showing -- the highlighted level within the highlighted campaign if an
 * ensign was clicked in the preview panel, otherwise the campaign's own
 * first level, same as the old immediate-commit-on-row-click behavior.
 */
// Neither of these unloads the land preview themselves --
// frontend_set_state always calls frontend_shutdown_state(<state being
// left>) first, which does it centrally for every way of leaving
// FeSt_CAMPAIGN_SELECT (these two buttons, but also e.g. an ESC handler
// added later), the same way FeSt_LAND_VIEW's frontmap_unload() already
// works.
void frontend_land_selection_enter(struct GuiButton *gbtn)
{
    if (land_selection_highlighted_campaign == NULL)
        return;
    if (!frontend_start_new_campaign(land_selection_highlighted_campaign->fname))
    {
        ERRORLOG("Unable to start new campaign");
        return;
    }
    // FeSt_CAMPAIGN_INTRO immediately redirects to FeSt_LAND_VIEW
    // (frontend_setup_state's redirect table) -- the old full-screen
    // cutscene this screen replaces. What actually launches a level
    // without it is FeSt_START_KPRLEVEL, driven by
    // set_selected_level_number (not set_continue_level_number, which
    // only sets the *resume point* for "Continue Game") -- same call
    // clicked_map_level_ensign() used to make before triggering it via
    // the old zoom-in animation.
    LevelNumber lvnum = (land_preview.highlighted_lvnum != SINGLEPLAYER_NOTSTARTED)
        ? land_preview.highlighted_lvnum : first_singleplayer_level();
    set_selected_level_number(lvnum);
    frontend_set_state(FeSt_START_KPRLEVEL);
}

void frontend_land_selection_return_to_main(struct GuiButton *gbtn)
{
    frontend_set_state(FeSt_MAIN_MENU);
}

/** A resizable panel background, genuinely independent of width/height
 * (unlike frontend_draw_scroll_box/_tab, which couples row height to
 * width -- see gui_draw_scroll_box's own comment). Reuses the same ornate
 * hugearea border art as the classic scroll box, at native scale, cropped
 * to this panel's own (narrower) rect -- see gui_draw_scroll_box_cropped's
 * comment -- instead of a flat opaque fill.
 */
void frontend_draw_land_selection_panel_bg(struct GuiButton *gbtn)
{
    gui_draw_scroll_box_cropped(gbtn, false);
}

/** Detail panel: the highlighted level's name+description if an ensign is
 * highlighted, otherwise the highlighted campaign's own.
 */
void frontend_draw_land_selection_detail(struct GuiButton *gbtn)
{
    const char *name = NULL;
    const char *description = NULL;
    if (land_preview.highlighted_lvnum != SINGLEPLAYER_NOTSTARTED)
    {
        struct LevelInformation *lvinfo = get_level_info(land_preview.highlighted_lvnum);
        if (lvinfo != NULL)
        {
            name = (lvinfo->name_stridx > 0) ? get_string(lvinfo->name_stridx) : lvinfo->name;
            description = lvinfo->description;
        }
    }
    if ((name == NULL) && (land_selection_highlighted_campaign != NULL))
    {
        name = land_selection_highlighted_campaign->display_name;
        description = land_selection_highlighted_campaign->description;
    }
    if (name == NULL)
        return;
    frontend_draw_land_selection_panel_bg(gbtn);
    RendererSetDrawFlags(Lb_TEXT_HALIGN_LEFT);
    // Fixed ~20px-tall reference line for the name, independent of
    // gbtn->height -- deriving scale from the *panel's* full height (as
    // frontend_draw_product_version-style code does for a short, single-
    // line button) stretches the text to fill this whole multi-line
    // panel, rendering it far too large. Description follows at half
    // that scale; LbTextDrawResized already word-wraps within
    // LbTextSetWindow's width, no extra wrapping needed.
    LbTextSetFont(frontend_font[1]);
    int name_upp = (20 * 13 / 11) * 16 / LbTextLineHeight();
    int name_line_h = LbTextLineHeight() * name_upp / 16;
    long pad = 10 * name_upp / 16;
    long inner_x = gbtn->scr_pos_x + pad;
    long inner_w = gbtn->width - 2*pad;
    LbTextSetWindow(inner_x, gbtn->scr_pos_y + pad, inner_w, name_line_h);
    LbTextDrawResized(0, 0, name_upp, name);
    if ((description != NULL) && (description[0] != '\0'))
    {
        int desc_upp = max(4, name_upp / 2);
        long desc_y = gbtn->scr_pos_y + pad + name_line_h + (4 * name_upp / 16);
        long desc_h = gbtn->scr_pos_y + gbtn->height - pad - desc_y;
        if (desc_h > 0)
        {
            LbTextSetWindow(inner_x, desc_y, inner_w, desc_h);
            LbTextDrawResized(0, 0, desc_upp, description);
        }
    }
}

// "Return to Main"/"Enter this land" are frontend_draw_button_icon
// buttons, same as the main menu's -- auto-sized to their own caption via
// frontend_menu_button_natural_width (frontend.h, the same helper
// frontend.cpp's main-menu buttons use), chained left-to-right from
// FE_LANDSEL_COL_X (not from FE_LANDSEL_PANEL_X, the preview column's own
// start -- anchoring the two from opposite ends of only ~384px risks
// overlap depending on their actual rendered width; starting both at the
// screen's left margin gives the full ~590px budget the main menu's own
// bottom row uses). Return sits on the left, Enter/Play on the right.
#define FE_LANDSEL_BOTTOMROW_GAP 24

void frontend_land_selection_enter_maintain(struct GuiButton *gbtn)
{
    int units_per_px = simple_frontend_sprite_height_units_per_px(gbtn, GFS_hugebutton_a05l, 100);
    long x = FE_LANDSEL_COL_X + frontend_menu_button_natural_width(FEBtn_MnuReturnToMain, units_per_px)
        + FE_LANDSEL_BOTTOMROW_GAP * units_per_px / 16;
    gbtn->width = frontend_menu_button_natural_width(FEBtn_MnuEnterLand, units_per_px);
    gbtn->pos_x = x;
    gbtn->scr_pos_x = x;
}

void frontend_land_selection_return_to_main_maintain(struct GuiButton *gbtn)
{
    int units_per_px = simple_frontend_sprite_height_units_per_px(gbtn, GFS_hugebutton_a05l, 100);
    gbtn->width = frontend_menu_button_natural_width(FEBtn_MnuReturnToMain, units_per_px);
    gbtn->pos_x = FE_LANDSEL_COL_X;
    gbtn->scr_pos_x = FE_LANDSEL_COL_X;
}

// "Play this level": commits the highlighted level directly, same as the
// original (pre-merge) frontend_level_select did on click -- no
// intermediate cutscene for free play, unlike campaign levels.
// frontend_shutdown_state's FeSt_MAPPACK_SELECT case unloads the preview
// centrally (mirrors FeSt_CAMPAIGN_SELECT's), not this function.
void frontend_freeplay_enter(struct GuiButton *gbtn)
{
    if (freeplay_highlighted_level <= 0)
        return;
    kfx_sim_state.selected_level_number = freeplay_highlighted_level;
    frontend_set_state(FeSt_START_KPRLEVEL);
}

void frontend_freeplay_return_to_main(struct GuiButton *gbtn)
{
    frontend_set_state(FeSt_MAIN_MENU);
}

// Auto-fit to caption + left-anchored pair, same pattern as Land
// selection's Enter/Return buttons -- Return on the left, Play on the right.
void frontend_freeplay_enter_maintain(struct GuiButton *gbtn)
{
    int units_per_px = simple_frontend_sprite_height_units_per_px(gbtn, GFS_hugebutton_a05l, 100);
    long x = FE_LANDSEL_COL_X + frontend_menu_button_natural_width(FEBtn_MnuReturnToMain, units_per_px)
        + FE_LANDSEL_BOTTOMROW_GAP * units_per_px / 16;
    gbtn->width = frontend_menu_button_natural_width(FEBtn_MnuPlayLevel, units_per_px);
    gbtn->pos_x = x;
    gbtn->scr_pos_x = x;
}

void frontend_freeplay_return_to_main_maintain(struct GuiButton *gbtn)
{
    int units_per_px = simple_frontend_sprite_height_units_per_px(gbtn, GFS_hugebutton_a05l, 100);
    gbtn->width = frontend_menu_button_natural_width(FEBtn_MnuReturnToMain, units_per_px);
    gbtn->pos_x = FE_LANDSEL_COL_X;
    gbtn->scr_pos_x = FE_LANDSEL_COL_X;
}

/** Detail panel: the highlighted level's name + description. Unlike Land
 * selection's equivalent there's no campaign-level fallback -- Free play
 * always has a specific level highlighted (or none at all, if the
 * mappack has no levels).
 */
void frontend_draw_freeplay_detail(struct GuiButton *gbtn)
{
    if (freeplay_highlighted_level <= 0)
        return;
    struct LevelInformation *lvinfo = get_level_info(freeplay_highlighted_level);
    if (lvinfo == NULL)
        return;
    const char *name = (lvinfo->name_stridx > 0) ? get_string(lvinfo->name_stridx) : lvinfo->name;
    frontend_draw_land_selection_panel_bg(gbtn);
    RendererSetDrawFlags(Lb_TEXT_HALIGN_LEFT);
    LbTextSetFont(frontend_font[1]);
    int name_upp = (20 * 13 / 11) * 16 / LbTextLineHeight();
    int name_line_h = LbTextLineHeight() * name_upp / 16;
    long pad = 10 * name_upp / 16;
    long inner_x = gbtn->scr_pos_x + pad;
    long inner_w = gbtn->width - 2*pad;
    LbTextSetWindow(inner_x, gbtn->scr_pos_y + pad, inner_w, name_line_h);
    LbTextDrawResized(0, 0, name_upp, name);
    if (lvinfo->description[0] != '\0')
    {
        int desc_upp = max(4, name_upp / 2);
        long desc_y = gbtn->scr_pos_y + pad + name_line_h + (4 * name_upp / 16);
        long desc_h = gbtn->scr_pos_y + gbtn->height - pad - desc_y;
        if (desc_h > 0)
        {
            LbTextSetWindow(inner_x, desc_y, inner_w, desc_h);
            LbTextDrawResized(0, 0, desc_upp, lvinfo->description);
        }
    }
}

void frontend_campaign_select_update(void)
{
    frontend_selectlist_update(&campaign_select_list);
}

/** A plain thin vertical line behind the scroll thumb, for this screen's
 * narrower scroll column -- see frontend_draw_levels_scroll_tab's comment
 * for why the ornate groove sprite doesn't fit here.
 */
void frontend_draw_simple_scroll_track(struct GuiButton *gbtn)
{
    long track_w = max(4L, gbtn->width / 3);
    long track_x = gbtn->scr_pos_x + (gbtn->width - track_w) / 2;
    LbDrawBox(track_x, gbtn->scr_pos_y, track_w, gbtn->height, TbPixel_RGB(0, 0, 0));
}

void frontend_draw_campaign_scroll_tab(struct GuiButton *gbtn)
{
    // See frontend_draw_levels_scroll_tab's comment.
    frontend_draw_simple_scroll_track(gbtn);
    frontend_selectlist_draw_scroll_tab(&campaign_select_list, gbtn);
}

/** Switches which mappack is highlighted: loads it (change_campaign),
 * refreshes the level list to its levels (restarting scroll at the top --
 * it's about to show a different set of levels than whatever the
 * previous mappack's list was scrolled to), and auto-highlights/previews
 * that mappack's first level so the panel isn't empty. Shared by the
 * mappack row's click_event and the screen's own entry point.
 */
static void freeplay_highlight_mappack(struct GameCampaign *campgn)
{
    if (!change_campaign(CampgnT_Mappack, campgn->fname))
        return;
    freeplay_highlighted_mappack = campgn;
    level_select_list.scroll_offset = 0;
    frontend_level_list_load();
    freeplay_highlighted_level = 0;
    if (campaign.freeplay_levels_count > 0)
    {
        LevelNumber lvnum = campaign.freeplay_levels[0];
        freeplay_highlighted_level = lvnum;
        land_preview_load(&land_preview, lvnum, false);
    } else
    {
        land_preview_unload(&land_preview);
    }
}

// Runs on every entry into FeSt_MAPPACK_SELECT (frontend_setup_state calls
// it unconditionally, same as frontend_campaign_list_load for Land
// selection), not just this menu's first-ever creation.
void frontend_mappack_list_load(void)
{
    frontend_selectlist_set_visible(&mappack_select_list);
    freeplay_highlighted_mappack = NULL;
    freeplay_highlighted_level = 0;
    land_preview.loaded = false;
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
    // Same groove-behind-the-thumb story as frontend_draw_campaign_scroll_tab
    // -- see its comment.
    frontend_draw_simple_scroll_track(gbtn);
    frontend_selectlist_draw_scroll_tab(&mappack_select_list, gbtn);
}

void frontend_draw_mp_mappack_scroll_tab(struct GuiButton *gbtn)
{
    frontend_selectlist_draw_scroll_tab(&mp_mappack_select_list, gbtn);
}

void frontend_campaign_list_load(void)
{
    frontend_selectlist_set_visible(&campaign_select_list);
    // Highlight the first campaign so the land preview/detail panel isn't
    // empty on entry, same as an implicit first row click. This runs on
    // every entry into FeSt_CAMPAIGN_SELECT (frontend_setup_state calls it
    // unconditionally), not just the menu's first-ever creation, so it's
    // the right place for this rather than the GuiMenu's create_cb.
    land_selection_highlighted_campaign = NULL;
    land_preview.loaded = false;
    if (campaigns_list.items_num > 0)
    {
        struct GameCampaign *campgn = &campaigns_list.items[0];
        if (change_campaign(CampgnT_Campaign, campgn->fname))
        {
            land_selection_highlighted_campaign = campgn;
            land_preview_load(&land_preview, SINGLEPLAYER_NOTSTARTED, true);
        }
    }
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
