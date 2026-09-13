/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file packet_data.h
 *     struct Packet and its trivial accessors, split out of kfx_net's
 *     packets.h (stage 13.3, docs/refactor/stage-13-enforce-and-document.md).
 * @par Purpose:
 *     struct Packet holds one player's resolved per-turn input (mouse
 *     position, action, control-key flags); kfx_sim (roomspace.c/
 *     roomspace_prediction.c/creature_instances.c) and kfx_render
 *     (cursor_tag.c/engine_redraw.c/local_camera.c) all dereference its
 *     fields directly and pervasively, not just via an occasional
 *     function call, so it must live at or below kfx_sim's own layer --
 *     same shape as camera_data.h's struct Camera split out of
 *     engine_camera.h. The accessor functions declared here
 *     (get_packet/get_packet_direct/set_packet_action/...) are trivial
 *     wrappers around sim_packets[] + get_player() -- both now defined
 *     here too (packet_data.c), moved down from kfx_net's
 *     packets.c/packets_misc.c/kfx_net_state.h (docs/refactor/todo/
 *     remove-symbol-level-layering-residuals.md) now that nothing about
 *     them is actually net-specific. kfx_net's own packet-exchange code
 *     still legitimately *writes* into sim_packets[] from above (a
 *     higher-ranked library writing into a lower-ranked library's state
 *     is fine -- only a lower library reaching upward is a violation).
 *     packets.h keeps re-including this header, so none of its other
 *     (same-or-higher-ranked) consumers need any changes.
 *
 *     The packet *processing* functions (process_packets/
 *     exchange_packets/process_camera_controls/process_first_person_look/
 *     can_process_creature_input/...) stay declared in kfx_net's
 *     packets.h -- they're genuine network/simulation orchestration, not
 *     data, and several of them reach into kfx_net_state/net_callbacks
 *     directly.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 */
/******************************************************************************/

#ifndef DK_PACKET_DATA_H
#define DK_PACKET_DATA_H

#include "bflib_basics.h"
#include "bflib_keybrd.h"
#include "bflib_netsp.h"
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct PlayerInfo;

enum TbPacketAction {
        PckA_None = 0,
        PckA_QuitToMainMenu, // Quit
        PckA_ForceApplicationClose,
        PckA_UnusedSlot003,
        PckA_NoOperation,
        PckA_FinishGame, // 5
        PckA_Login,      // From `enum NetMessageType`
        PckA_UserUpdate,
        PckA_Frame,
        PckA_Resync,
        PckA_UnusedSlot010,//10
        PckA_UnusedSlot011,
        PckA_UnusedSlot012,
        PckA_PlyrMsgBegin,
        PckA_PlyrMsgEnd,
        PckA_UnusedSlot015,//15
        PckA_UnusedSlot016,
        PckA_UnusedSlot017,
        PckA_UnusedSlot018,
        PckA_UnusedSlot019,
        PckA_ToggleLights,//20
        PckA_UnusedSlot021,
        PckA_TogglePause,
        PckA_UnusedSlot023,
        PckA_SetCluedo,
        PckA_ChangeWindowSize,//25
        PckA_BookmarkLoad,
        PckA_SetGammaLevel,
        PckA_SetMinimapConf,
        PckA_SetMapRotation,
        PckA_UnusedSlot030,//30
        PckA_UnusedSlot031,
        PckA_PasngrCtrlExit,
        PckA_DirectCtrlExit,
        PckA_UnusedSlot034,
        PckA_UnusedSlot035,//35
        PckA_SetPlyrState,
        PckA_SwitchView,
        PckA_UnusedSlot038,
        PckA_CtrlCrtrSetInstnc,
        PckA_GenericLevelPower,//40
        PckA_HoldAudience,
        PckA_UnusedSlot042,
        PckA_UnusedSlot043,
        PckA_UnusedSlot044,
        PckA_UnusedSlot045,//45
        PckA_UnusedSlot046,
        PckA_UnusedSlot047,
        PckA_UnusedSlot048,
        PckA_UnusedSlot049,
        PckA_UnusedSlot050,//50
        PckA_UnusedSlot051,
        PckA_UnusedSlot052,
        PckA_UnusedSlot053,
        PckA_UnusedSlot054,
        PckA_ToggleTendency,//55
        PckA_UnusedSlot056,
        PckA_UnusedSlot057,
        PckA_UnusedSlot058,
        PckA_UnusedSlot059,
        PckA_CheatEnter,//60
        PckA_CheatAllFree,
        PckA_CheatCrtSpells, // unused
        PckA_CheatRevealMap,
        PckA_CheatCrAllSpls, // unused
        PckA_CheatUnusedPlaceholder065,//65
        PckA_CheatAllMagic,
        PckA_CheatAllRooms,
        PckA_CheatUnusedPlaceholder068,
        PckA_CheatUnusedPlaceholder069,
        PckA_CheatAllResrchbl,//70
        PckA_UnusedSlot071,
        PckA_UnusedSlot072,
        PckA_UnusedSlot073,
        PckA_UnusedSlot074,
        PckA_UnusedSlot075,//75
        PckA_UnusedSlot076,
        PckA_UnusedSlot077,
        PckA_UnusedSlot078,
        PckA_UnusedSlot079,
        PckA_SetViewType,//80
        PckA_ZoomFromMap,
        PckA_UpdatePause,
        PckA_ZoomToEvent,
        PckA_ZoomToRoom,
        PckA_ZoomToTrap,//85
        PckA_ZoomToDoor,
        PckA_ZoomToPosition,
        PckA_ToggleComputerProcessing,
        PckA_PwrCTADis,
        PckA_UsePwrHandPick,//90
        PckA_UsePwrHandDrop,
        PckA_EventBoxTurnOff,
        PckA_UseSpecialBox,
        PckA_UnusedSlot094,
        PckA_ResurrectCrtr,//95
        PckA_TransferCreatr,
        PckA_UsePwrObey,
        PckA_UsePwrArmageddon,
        PckA_TurnOffQuery,
        PckA_UnusedSlot100,//100
        PckA_UnusedSlot101,
        PckA_UnusedSlot102,
        PckA_UnusedSlot103,
        PckA_ZoomToBattle,
        PckA_UnusedSlot105,//105
        PckA_ZoomToSpell,
        PckA_ToggleComputer,
        PckA_PlyrFastMsg,
        PckA_SetComputerKind,
        PckA_GoSpectator,//110
        PckA_DumpHeldThingToOldPos,
        PckA_UnusedSlot112,
        PckA_UnusedSlot113,
        PckA_PwrSOEDis,
        PckA_EventBoxActivate,//115
        PckA_EventBoxClose,
        PckA_UsePwrOnThing,
        PckA_PlyrToggleAlly,
        PckA_SaveViewType,
        PckA_LoadViewType,//120
        PckA_UnusedSlot121    =  121,
        PckA_PlyrMsgClear,
        PckA_PlyrMsgLast,
        PckA_PlyrMsgCmdAutoCompletion,
        PckA_DirectCtrlDragDrop,
        PckA_CheatPlaceTerrain,
        PckA_CheatMakeCreature,
        PckA_CheatMakeDigger,
        PckA_CheatStealSlab,
        PckA_CheatStealRoom,
        PckA_CheatHeartHealth,
        PckA_CheatKillPlayer,
        PckA_CheatConvertCreature,
        PckA_CheatSwitchTerrain,
        PckA_CheatSwitchPlayer,
        PckA_CheatSwitchCreature,
        PckA_CheatSwitchHero,
        PckA_CheatSwitchExperience,
        PckA_CheatCtrlCrtrSetInstnc,
        PckA_SetFirstPersonDigMode,
        PckA_SwitchTeleportDest,
        PckA_SelectFPPickup,
        PckA_CheatAllDoors,
        PckA_CheatAllTraps,
        PckA_SetRoomspaceAuto,
        PckA_SetRoomspaceMan,
        PckA_SetRoomspaceDrag,
        PckA_SetRoomspaceDefault,
        PckA_SetRoomspaceWholeRoom,
        PckA_SetRoomspaceSubtile,
        PckA_SetRoomspaceHighlight,
        PckA_SetNearestTeleport,
        PckA_SetRoomspaceDragPaint,
        PckA_PlyrQueryCreature,
        PckA_CheatGiveDoorTrap,
        PckA_RoomspaceHighlightToggle,
        PckA_ApplyRoomspaceDigTag,
		PckA_CheatWinLevel,
		PckA_CheatLoseLevel,
		PckA_CheatLevelUp,
		PckA_CheatLevelDown,
		PckA_CheatApplySpell,
		PckA_CheatKillCreature,
        // docs/refactor/editor/02-editing-toolbox.md §2.3 -- appended, not
        // inserted -- same "these numeric values are saved" reasoning as
        // enum PlayerStates (config_players.h).
        PckA_EditorFloodFill,
        // §2.6 -- sent directly by kfx_editor (not generated by the normal
        // per-work-state click dispatch -- see PSt_EditorPlaceObject's own
        // comment), carrying the target subtile position (actn_par1/
        // actn_par2, x/y -- full int32 range, needed since actn_par3/4 are
        // only int16_t and a max-size map's subtile position overflows
        // that), then the object model (actn_par3) and owner (actn_par4).
        // Position is NOT read from the packet's own pos_x/pos_y -- those
        // are per-turn scratch state that input() populates and
        // exchange_packets() resets right after, so by the time this
        // render-phase click handler runs they're already back to (0,0) for
        // the next turn (found live: object placement silently no-op'd,
        // logged MapCoordsValid=0 pos=(0,0) on every attempt). kfx_editor
        // instead recomputes the world position itself via screen_to_map()
        // at click time and sends it explicitly.
        PckA_EditorPlaceObject,
        // §2.7 -- picker selection verbs, same shape as PckA_CheatSwitchTerrain/
        // PckA_CheatSwitchCreature above (unconditionally overwrite the
        // selection, no work-state check) but writing to UserState's own
        // pre-existing chosen_trap_kind/chosen_door_kind (used by the classic
        // workshop PSt_PlaceTrap/PSt_PlaceDoor already) rather than a
        // CheatSelection field. Appended here, not grouped with the other
        // CheatSwitch* entries above, for the same "never renumber" reason.
        PckA_CheatSwitchTrap,
        PckA_CheatSwitchDoor,
        // Placement itself: same free-placement shape as PckA_EditorFloodFill
        // (bypasses the workshop-stock/resource-cost check the classic
        // PSt_PlaceTrap/PSt_PlaceDoor dispatch has, since a blank editor map
        // never has any manufactured stock) -- carries trap/door model
        // (actn_par1) and owner (actn_par2); position IS safe to read from
        // the packet's own pos_x/pos_y here, unlike PckA_EditorPlaceObject,
        // because these are sent from packets_process_cheats()'s per-work-state
        // switch during input() itself, not from a later render-phase callback.
        PckA_EditorPlaceTrap,
        PckA_EditorPlaceDoor,
};

/** Packet flags for non-action player operation. */
enum TbPacketControl {
        PCtr_None           = 0x0000,
        PCtr_ViewRotateCW   = 0x0001,
        PCtr_ViewRotateCCW  = 0x0002,
        PCtr_MoveUp         = 0x0004,
        PCtr_MoveDown       = 0x0008,
        PCtr_MoveLeft       = 0x0010,
        PCtr_MoveRight      = 0x0020,
        PCtr_ViewZoomIn     = 0x0040,
        PCtr_ViewZoomOut    = 0x0080,
        PCtr_LBtnClick      = 0x0100,
        PCtr_RBtnClick      = 0x0200,
        PCtr_LBtnHeld       = 0x0400,
        PCtr_RBtnHeld       = 0x0800,
        PCtr_LBtnRelease    = 0x1000,
        PCtr_RBtnRelease    = 0x2000,
        PCtr_Gui            = 0x4000,
        PCtr_MapCoordsValid = 0x8000,
        PCtr_ViewTiltUp     = 0x10000,
        PCtr_ViewTiltDown   = 0x20000,
        PCtr_ViewTiltReset  = 0x40000,
        PCtr_Ascend         = 0x80000,
        PCtr_Descend        = 0x100000,
        PCtr_ViewZoomPos    = 0x200000,
        PCtr_ViewRotatePos  = 0x400000
};

/**
 * Additional packet flags
 */
enum TbPacketAddValues {
    PCAdV_None              = 0x00, //!< Dummy flag
    PCAdV_SpeedupPressed    = 0x01, //!< The keyboard modified used for speeding up camera movement is pressed.
    PCAdV_ContextMask       = 0x1E, //!< Instead of a single bit, this value stores is 4-bit integer; stores context of map coordinates. The context is used to set the Cursor State.
    PCAdV_CrtrContrlPressed = 0x20, //!< The keyboard modified used for creature control is pressed.
    PCAdV_CrtrQueryPressed  = 0x40, //!< The keyboard modified used for querying creatures is pressed.
    PCAdV_RotatePressed     = 0x80,
};

#define PCtr_LBtnAnyAction (PCtr_LBtnClick | PCtr_LBtnHeld | PCtr_LBtnRelease)
#define PCtr_RBtnAnyAction (PCtr_RBtnClick | PCtr_RBtnHeld | PCtr_RBtnRelease)
#define PCtr_HeldAnyButton (PCtr_LBtnHeld | PCtr_RBtnHeld)

#define INVALID_PACKET (&bad_packet)

/******************************************************************************/
#pragma pack(1)

/**
 * Stores data exchanged between players each turn and used to re-create their input.
 */
struct Packet {
    GameTurn turn;
    TbBigChecksum checksum; //! Checksum of the entire game state of the previous turn, used solely for desync detection
    int8_t input_lag_turns;
    uint8_t action; //! Action kind performed by the player which owns this packet
    int32_t actn_par1; //! Players action parameter #1
    int32_t actn_par2; //! Players action parameter #2
    int32_t pos_x; //! Mouse Cursor Position X
    int32_t pos_y; //! Mouse Cursor Position Y
    uint32_t control_flags;
    uint8_t additional_packet_values; // uses the flags and values from TbPacketAddValues
    int16_t actn_par3; //! Players action parameter #3
    int16_t actn_par4; //! Players action parameter #4
};

struct PacketSaveHead {
    unsigned short game_ver_major;
    unsigned short game_ver_minor;
    unsigned short game_ver_release;
    unsigned short game_ver_build;
    uint32_t level_num;
    PlayerBitFlags players_exist;
    PlayerBitFlags players_comp;
    uint32_t isometric_view_zoom_level;
    uint32_t frontview_zoom_level;
    int isometric_tilt;
    unsigned char video_rotate_mode;
    TbBool chksum_available; // if needed, this can be replaced with flags
    uint32_t action_seed;
    TbBool default_imprison_tendency;
    TbBool default_flee_tendency;
    TbBool skip_heart_zoom;
    TbBool highlight_mode;
};

#pragma pack()

// Moved down from kfx_net's net_game.h (docs/refactor/todo/
// remove-symbol-level-layering-residuals.md) alongside sim_packets[]
// below -- both were the last thing keeping get_packet()/get_packet_direct()/
// set_packet_action()/set_players_packet_action()'s real implementations in
// kfx_net despite the interface already living here. kfx_net's own
// packets.c/packets_misc.c/net_exchange_gameplay.c still need this too, for
// their own genuinely net-owned logic (checksums, wire buffer packing,
// turn-history exchange) -- reached via packets.h's existing #include of
// this header, no new #include needed there.
#define PACKETS_COUNT 9

extern struct Packet bad_packet;

// Per-turn input packets, one per connected player slot. Moved down from
// kfx_net_state (kfx_net_state.h) for the same reason as PACKETS_COUNT
// above -- kfx_net's packet-exchange code still legitimately *writes*
// into this from above (a higher-ranked library writing into a
// lower-ranked library's storage is fine, only the reverse is a
// violation -- see this header's own file comment). Deliberately a bare
// extern, not folded into kfx_sim_state: it isn't one of architecture.md
// §6.2's three raw-blob sync payloads today, and folding it into
// kfx_sim_state would silently add it to all of them.
extern struct Packet sim_packets[PACKETS_COUNT];

/******************************************************************************/
struct Packet *get_local_packet(void);
NetUserId get_local_user(void);
struct Packet *get_packet(NetUserId user);
void set_packet_action(struct Packet *pckt, unsigned char pcktype, long par1, long par2, unsigned short par3, unsigned short par4);
TbBool is_packet_empty(const struct Packet *pckt);
void set_players_packet_action(struct PlayerInfo *player, unsigned char pcktype, unsigned long par1, unsigned long par2, unsigned short par3, unsigned short par4);
void set_packet_control(struct Packet *pckt, unsigned long flag);
void set_players_packet_control(struct PlayerInfo *player, unsigned long flag);
unsigned char get_players_packet_action(struct PlayerInfo *player);
void unset_packet_control(struct Packet *pckt, unsigned long flag);
void unset_players_packet_control(struct PlayerInfo *player, unsigned long flag);
void set_players_packet_position(struct Packet *pckt, long x, long y, unsigned char context);
void set_packet_pause_toggle(void);
TbBool packet_crtr_control_pressed(struct Packet *packet);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
