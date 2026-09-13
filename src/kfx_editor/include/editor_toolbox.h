/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file editor_toolbox.h
 *     Header file for editor_toolbox.cpp.
 * @par Purpose:
 *     docs/refactor/editor/02-editing-toolbox.md -- the in-game level
 *     editor's tool palette. Internal to kfx_editor (not part of
 *     kfx_editor.h's public surface); split into its own file/header
 *     purely so editor_session.cpp doesn't grow into a catch-all.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 */
/******************************************************************************/
#ifndef DK_EDITOR_TOOLBOX_H
#define DK_EDITOR_TOOLBOX_H

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

// Submits the tool-strip/picker/bottom-bar panel. Called every frame from
// editor_frame() while editor_is_active(); does not check that itself.
void editor_toolbox_frame(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
