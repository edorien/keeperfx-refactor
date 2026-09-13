/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file editor_toolbox.cpp
 *     docs/refactor/editor/02-editing-toolbox.md -- the tool palette.
 *     Phase 2's actual scope is much larger than what's here (marking,
 *     fill, brush, objects, traps/doors, lights/FX/AP stubs, query/eraser,
 *     undo/redo journal, definable keybindings -- see that doc's §5 "what
 *     must exist" list). This first slice covers items 1-4: the toolbox
 *     panel itself, terrain/room paint with a generated palette, the
 *     player+experience bottom bar, and creature/hero/digger placement
 *     with a model-grid picker. Every tool here is a thin UI over an
 *     already-live PSt_ work-state and PckA_Cheat packet pair (doc §1) --
 *     no new world-mutation logic, exactly per the doc's "tools = player
 *     work-states" model.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "editor_toolbox.h"

#include "frontgui_widgets.h"
#include <imgui.h>
#include "player_data.h"
#include "packet_data.h"
#include "config_players.h"
#include "config_terrain.h"
#include "config_creature.h"
#include "config_objects.h"
#include "config_trapdoor.h"
#include "kfx_config_state.h"
#include "local_camera.h"
#include "engine_redraw.h"
#include "kjm_input.h"
#include <cstdio>
#include "post_inc.h"

/******************************************************************************/
namespace {

    enum EditorTool {
        EdTool_Terrain,
        EdTool_Fill,
        EdTool_CreatureEvil,
        EdTool_CreatureHero,
        EdTool_Digger,
        EdTool_Object,
        EdTool_Trap,
        EdTool_Door,
        EdTool_Query,
        EdTool_Erase,
    };

    EditorTool s_active_tool = EdTool_Terrain;

    // Local shadow of ustate->cheatselection, kept only so the picker can
    // highlight the current selection -- nothing else in an editor session
    // writes cheatselection, so this can't drift out of sync. Starts at the
    // same value clear_game()'s zero-init leaves chosen_terrain_kind at
    // (SlbT_ROCK == 0) -- matching reality matters more than picking a more
    // "visible" default the engine doesn't actually have set yet. Found
    // live: with no highlight at all, a palette click was indistinguishable
    // from one that didn't register -- worth remembering when testing
    // placement on a New Map canvas that starts 100% rock: painting ROCK
    // over ROCK is real, correct behaviour, just invisible: pick a
    // different kind from the (now highlighted) list first.
    SlabKind s_selected_terrain_kind = SlbT_ROCK;
    ThingModel s_selected_creature_kind = 1;
    ThingModel s_selected_hero_kind = 1;
    ThingModel s_selected_object_model = 1;
    ThingModel s_selected_trap_kind = 1;
    ThingModel s_selected_door_kind = 1;
    PlayerNumber s_selected_owner = 0;
    unsigned char s_selected_level = 0; // 0-indexed -- see draw_bottom_bar()

    void set_work_state(unsigned char state)
    {
        struct PlayerInfo *player = get_my_player();
        // Exactly gf_change_player_state()'s own call (gui_boxmenu.c) --
        // the classic cheat menu's "change mode" buttons do the same thing.
        set_players_packet_action(player, PckA_SetPlyrState, state, 0, 0, 0);
    }

    void draw_tool_strip()
    {
        struct ToolDef { const char *label; EditorTool tool; unsigned char state; };
        static const ToolDef tools[] = {
            {"Terrain",  EdTool_Terrain,       PSt_PlaceTerrain},
            {"Fill",     EdTool_Fill,          PSt_EditorFill},
            {"Creature", EdTool_CreatureEvil,  PSt_MkBadCreatr},
            {"Hero",     EdTool_CreatureHero,  PSt_MkGoodCreatr},
            {"Digger",   EdTool_Digger,        PSt_MkDigger},
            {"Object",   EdTool_Object,        PSt_EditorPlaceObject},
            {"Trap",     EdTool_Trap,          PSt_EditorPlaceTrap},
            {"Door",     EdTool_Door,          PSt_EditorPlaceDoor},
            {"Query",    EdTool_Query,         PSt_QueryAll},
            {"Erase",    EdTool_Erase,         PSt_DestroyThing},
        };
        // One packet action per click, deliberately -- set_players_packet_action()
        // overwrites the single per-turn packet slot, so a tool switch can't
        // also re-sync the chosen kind in the same click (that would just
        // clobber the PckA_SetPlyrState with a PckA_CheatSwitch* before
        // either is ever processed). Same one-action-per-click shape the
        // classic cheat menu's gf_change_player_state() has.
        for (const ToolDef &t : tools)
        {
            bool selected = (s_active_tool == t.tool);
            if (FeNavButton(t.label, selected))
            {
                s_active_tool = t.tool;
                set_work_state(t.state);
            }
        }
    }

    // §2.1 -- slab palette generated from kfx_config_state.conf.slab_conf,
    // so modded slab kinds appear automatically (F15). Rooms are just more
    // SlabKind entries in the same config (assigned_room on the stats, not
    // a separate array) -- PSt_PlaceTerrain already treats them uniformly,
    // so one flat list covers §2.1's Tiles+Rooms without a second picker.
    void draw_terrain_picker()
    {
        struct PlayerInfo *player = get_my_player();
        const struct SlabsConfig &slabc = kfx_config_state.conf.slab_conf;
        bool open = FeBeginListBox("##EdTerrainPicker", ImVec2(240, 320));
        if (open)
        {
            for (int32_t i = 0; i < slabc.slab_types_count; i++)
            {
                const char *name = slab_code_name((SlabKind)i);
                if (FeListRow(name, i == s_selected_terrain_kind))
                {
                    s_selected_terrain_kind = (SlabKind)i;
                    set_players_packet_action(player, PckA_CheatSwitchTerrain, i, 0, 0, 0);
                }
            }
        }
        FeEndListBox(open);
    }

    // §2.5 -- creature-model grid, split evil/hero by which picker is
    // active (the tool strip's own Creature/Hero split), same "loop
    // [1, model_count)" + creature_code_name() the doc calls for. Custom/
    // campaign-modded creatures appear automatically (F15).
    void draw_creature_picker(TbBool hero)
    {
        struct PlayerInfo *player = get_my_player();
        long model_count = kfx_config_state.conf.crtr_conf.model_count;
        bool open = FeBeginListBox("##EdCreaturePicker", ImVec2(240, 320));
        if (open)
        {
            ThingModel &selected = hero ? s_selected_hero_kind : s_selected_creature_kind;
            for (ThingModel m = 1; m < (ThingModel)model_count; m++)
            {
                const char *name = creature_code_name(m);
                if (FeListRow(name, m == selected))
                {
                    selected = m;
                    set_players_packet_action(player,
                        hero ? PckA_CheatSwitchHero : PckA_CheatSwitchCreature, m, 0, 0, 0);
                }
            }
        }
        FeEndListBox(open);
    }

    // §2.6 -- object-model grid from object_conf, same "loop [1, count)" +
    // *_code_name() shape as the terrain/creature pickers. Unlike those,
    // picking a row here only updates the *local* selection -- there's no
    // CheatSelection field for "chosen object model" (F17), so nothing is
    // sent until the world is actually clicked; see
    // handle_object_placement_click() below, which reads
    // s_selected_object_model directly.
    void draw_object_picker()
    {
        long model_count = kfx_config_state.conf.object_conf.object_types_count;
        bool open = FeBeginListBox("##EdObjectPicker", ImVec2(240, 320));
        if (open)
        {
            for (ThingModel m = 1; m < (ThingModel)model_count; m++)
            {
                const char *name = object_code_name(m);
                if (FeListRow(name, m == s_selected_object_model))
                    s_selected_object_model = m;
            }
        }
        FeEndListBox(open);
    }

    // §2.7 -- trap/door pickers, same "loop [1, count)" + *_code_name()
    // shape as the object picker, but selecting a row here *does* send a
    // packet immediately (PckA_CheatSwitchTrap/Door), same as terrain/
    // creature: unlike Objects, UserState already has chosen_trap_kind/
    // chosen_door_kind (used by the classic workshop tools), so there's no
    // F17 gap to work around -- placement itself stays a plain click,
    // handled entirely by PSt_EditorPlaceTrap/PSt_EditorPlaceDoor's own
    // packets_cheats.c dispatch, no render-phase click handler needed.
    void draw_trap_picker()
    {
        struct PlayerInfo *player = get_my_player();
        long model_count = kfx_config_state.conf.trapdoor_conf.trap_types_count;
        bool open = FeBeginListBox("##EdTrapPicker", ImVec2(240, 320));
        if (open)
        {
            for (ThingModel m = 1; m < (ThingModel)model_count; m++)
            {
                const char *name = trap_code_name(m);
                if (FeListRow(name, m == s_selected_trap_kind))
                {
                    s_selected_trap_kind = m;
                    set_players_packet_action(player, PckA_CheatSwitchTrap, m, 0, 0, 0);
                }
            }
        }
        FeEndListBox(open);
    }

    void draw_door_picker()
    {
        struct PlayerInfo *player = get_my_player();
        long model_count = kfx_config_state.conf.trapdoor_conf.door_types_count;
        bool open = FeBeginListBox("##EdDoorPicker", ImVec2(240, 320));
        if (open)
        {
            for (ThingModel m = 1; m < (ThingModel)model_count; m++)
            {
                const char *name = door_code_name(m);
                if (FeListRow(name, m == s_selected_door_kind))
                {
                    s_selected_door_kind = m;
                    set_players_packet_action(player, PckA_CheatSwitchDoor, m, 0, 0, 0);
                }
            }
        }
        FeEndListBox(open);
    }

    // §2.6 -- placement itself. Unlike terrain/creature (a click just sets
    // player->work_state/cheatselection and the *existing* dungeon-control
    // click dispatch in packets_cheats.c does the rest, reading pos_x/pos_y
    // from *within* input() -- before the packet is consumed), there is no
    // server-side "chosen object" to read back there (F17) -- so kfx_editor
    // detects the world click and sends PckA_EditorPlaceObject itself.
    //
    // Found live via unconditional JUSTMSG diagnostics ("no object appears",
    // then confirmed with keeperfx.log): reading the *packet's* pos_x/pos_y
    // here -- from the ImGui-render phase, which runs later in the frame
    // than input() -- always saw MapCoordsValid=0, pos=(0,0). The packet is
    // per-turn scratch state that input() populates and the turn-exchange
    // step (exchange_packets(), called right after input() in
    // game_session_loop.cpp) resets before the render phase runs; by the
    // time this ImGui callback fires, get_local_packet() is already back to
    // a blank next-turn packet. Terrain/Fill/Creature never hit this because
    // their dispatch lives *inside* input()'s per-turn packets_cheats.c
    // switch, not in a render-phase callback.
    //
    // Fix: don't read the packet at all -- recompute the world position
    // directly with the same screen_to_map() the input path itself uses,
    // against the current mouse position and camera, right here at click
    // time. Guarded on !io.WantCaptureMouse so a click on this very
    // toolbox's own picker list doesn't also register as a world placement.
    void handle_object_placement_click()
    {
        ImGuiIO &io = ImGui::GetIO();
        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            return;
        if (io.WantCaptureMouse)
            return;
        struct PlayerInfo *player = get_my_player();
        struct Camera *camera = get_local_active_camera(player);
        struct Coord3d pos;
        if (!screen_to_map(camera, GetMouseX(), GetMouseY(), &pos))
            return;
        set_players_packet_action(player, PckA_EditorPlaceObject, pos.x.val, pos.y.val,
            s_selected_object_model, s_selected_owner);
    }

    // §2 "shared bottom bar" -- player selector (0-3/Neutral) and
    // experience level (1-10), feeding cheatselection for whichever tool
    // is active (terrain owner, creature/digger owner+level). Hero-player
    // omitted from this first slice -- P0-P3 + Neutral covers the normal
    // 4-keeper map.
    void draw_bottom_bar()
    {
        struct PlayerInfo *player = get_my_player();
        FeSeparator();
        // Bracketed label marks the current selection -- FeButton (unlike
        // FeNavButton/FeListRow) has no built-in "selected" look, and this
        // row is laid out horizontally via SameLine(), not the vertical
        // list FeNavButton assumes. Found live: no feedback at all here
        // made every bottom-bar click indistinguishable from a no-op.
        ImGui::TextUnformatted("Owner:");
        for (int p = 0; p < 4; p++)
        {
            ImGui::SameLine();
            char label[10];
            snprintf(label, sizeof(label), (p == s_selected_owner) ? "[P%d]" : "P%d", p);
            if (FeButton(label))
            {
                s_selected_owner = p;
                set_players_packet_action(player, PckA_CheatSwitchPlayer, p, 0, 0, 0);
            }
        }
        ImGui::SameLine();
        {
            bool neutral_selected = (s_selected_owner == kfx_config_state.neutral_player_num);
            if (FeButton(neutral_selected ? "[Neutral]" : "Neutral"))
            {
                s_selected_owner = kfx_config_state.neutral_player_num;
                set_players_packet_action(player, PckA_CheatSwitchPlayer,
                    kfx_config_state.neutral_player_num, 0, 0, 0);
            }
        }

        ImGui::TextUnformatted("Level:");
        for (int lvl = 1; lvl <= 10; lvl++)
        {
            ImGui::SameLine();
            char label[8];
            // chosen_experience_level is 0-indexed on the wire (packets_cheats.c
            // displays it as "+1") -- lvl-1 here, not lvl.
            snprintf(label, sizeof(label), (lvl - 1 == s_selected_level) ? "[%d]" : "%d", lvl);
            if (FeButton(label))
            {
                s_selected_level = lvl - 1;
                set_players_packet_action(player, PckA_CheatSwitchExperience, lvl - 1, 0, 0, 0);
            }
        }
    }

} // namespace

void editor_toolbox_frame(void)
{
    ImGui::SetNextWindowPos(ImVec2(20, 60), ImGuiCond_FirstUseEver);
    ImGui::Begin("##EditorToolbox", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

    FeHeading("Toolbox");
    FeSeparator();
    draw_tool_strip();
    FeSeparator();

    switch (s_active_tool)
    {
        case EdTool_Terrain:      draw_terrain_picker(); break;
        // Fill reuses the exact same terrain palette/selection as the
        // Terrain tool -- packets_cheats.c's PSt_EditorFill keys off the
        // same chosen_terrain_kind/chosen_player, no separate fill-target
        // state to pick.
        case EdTool_Fill:         draw_terrain_picker(); break;
        case EdTool_CreatureEvil: draw_creature_picker(false); break;
        case EdTool_CreatureHero: draw_creature_picker(true); break;
        case EdTool_Object:       draw_object_picker(); break;
        case EdTool_Trap:         draw_trap_picker(); break;
        case EdTool_Door:         draw_door_picker(); break;
        // Digger/Query/Erase need no picker -- they're a bare work-state
        // switch (§2.5/§2.10); the bottom bar below still applies (owner
        // for diggers).
        default: break;
    }

    draw_bottom_bar();

    ImGui::End();

    // Deliberately outside the toolbox's own Begin/End -- this checks
    // clicks anywhere on screen (the 3D dungeon view), not just inside
    // this window; io.WantCaptureMouse inside the function itself is what
    // stops a click on the toolbox from also registering as a placement.
    if (s_active_tool == EdTool_Object)
        handle_object_placement_click();
}
/******************************************************************************/
