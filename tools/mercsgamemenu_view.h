#ifndef MERCS_GAME_MENU_VIEW_H
#define MERCS_GAME_MENU_VIEW_H

/* Original DOS0x41c34 Escape/right-click menu. Actions are original row IDs;
 * the caller implements their traced post-selection gates and confirmations.
 * The original draws every row: there is no disabled-row invention here.
 * Mouse activates on LEFT PRESS (unlike the message-dialog release rule).
 * Opening key/button and repeats must not leak into this modal from its host.
 */
#include "mercsart.h"

enum {
    MERCS_GAME_MENU_PENDING = -1,
    MERCS_GAME_MENU_CANCEL = -2,
    MERCS_GAME_MENU_NEW = 0,
    MERCS_GAME_MENU_COMBAT,
    MERCS_GAME_MENU_COCKPIT,
    MERCS_GAME_MENU_LORDS,
    MERCS_GAME_MENU_CIRCUIT,
    MERCS_GAME_MENU_QUIT,
    MERCS_GAME_MENU_SAVE,
    MERCS_GAME_MENU_MOVE = 0,
    MERCS_GAME_MENU_PRESS = 1,
    MERCS_GAME_MENU_RELEASE = 2,
    MERCS_GAME_MENU_RIGHT_PRESS = 3
};
typedef struct mercs_game_menu {
    int selected, active, answer, pointer_x, pointer_y;
} mercs_game_menu;

static inline void mercs_game_menu_open(mercs_game_menu *m)
{
    if (!m) return;
    m->selected = 0; m->active = 1; m->answer = MERCS_GAME_MENU_PENDING;
    m->pointer_x = m->pointer_y = -1;
}

static inline int mercs_game_menu_choose(mercs_game_menu *m, int choice)
{
    if (!m || !m->active || (choice != MERCS_GAME_MENU_CANCEL && (choice < 0 || choice > 6)))
        return MERCS_GAME_MENU_PENDING;
    m->answer = choice; m->active = 0;
    return choice;
}

static inline int mercs_game_menu_hit(int x, int y)
{
    int row;
    if (x < 198 || x > 441 || y < 139 || y > 341) return -1;
    row = (y-139)/29;
    return row <= 6 ? row : -1;
}

static inline int mercs_game_menu_key(mercs_game_menu *m, SDL_Keycode key)
{
    if (!m || !m->active) return MERCS_GAME_MENU_PENDING;
    if (key == SDLK_ESCAPE) return mercs_game_menu_choose(m,MERCS_GAME_MENU_CANCEL);
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) return mercs_game_menu_choose(m,m->selected);
    if (key == SDLK_UP || key == SDLK_LEFT || key == SDLK_8 || key == SDLK_4 ||
        key == SDLK_KP_8 || key == SDLK_KP_4) m->selected = (m->selected+6)%7;
    if (key == SDLK_DOWN || key == SDLK_RIGHT || key == SDLK_2 || key == SDLK_6 ||
        key == SDLK_KP_2 || key == SDLK_KP_6) m->selected = (m->selected+1)%7;
    return MERCS_GAME_MENU_PENDING;
}

static inline int mercs_game_menu_mouse(mercs_game_menu *m, int x, int y, int event)
{
    int row;
    if (!m || !m->active) return MERCS_GAME_MENU_PENDING;
    if (event == MERCS_GAME_MENU_RIGHT_PRESS)
        return mercs_game_menu_choose(m,MERCS_GAME_MENU_CANCEL);
    row = mercs_game_menu_hit(x,y);
    if (x != m->pointer_x || y != m->pointer_y) {
        m->pointer_x = x; m->pointer_y = y;
        if (row >= 0) m->selected = row;
    }
    if (event == MERCS_GAME_MENU_PRESS)
        return mercs_game_menu_choose(m,row >= 0 ? row : MERCS_GAME_MENU_CANCEL);
    return MERCS_GAME_MENU_PENDING;
}

static inline int mercs_game_menu_draw(mercs_art *a, const mercs_game_menu *m)
{
    static const char *const labels[7] = {"NEW MERCENARY","COMBAT VARIABLES","COCKPIT CONTROLS",
        "MERC LORDS","THE FIRST CIRCUIT","FLEE TO DOS","LOAD/SAVE GAME"};
    int i;
    if (!a || !m || !m->active || mercs_art_shape(a,1,0,198,116)) return -1;
    for (i = 0; i < 7; i++)
        mercs_art_center(a,8,320,153+29*i-mercs_art_height(a,8)/2,labels[i],i == m->selected ? 17 : 22);
    return 0;
}

#endif
