#ifndef FRONTGUI_INGAME_GRIDS_H
#define FRONTGUI_INGAME_GRIDS_H

// The room/spell/trap build grids (docs/refactor/ingame-gui/10-maintainability-refactors.md
// §7). Called from frontgui_ingame_tabcontent.cpp's dispatch; do_sell_rooms/
// do_sell_traps are exposed separately for its FUNCTESTING hook.

#ifdef __cplusplus

void room_grid(void);
void spell_grid(void);
void trap_grid(void);

void do_sell_rooms(void);
void do_sell_traps(void);

#endif // __cplusplus
#endif // FRONTGUI_INGAME_GRIDS_H
