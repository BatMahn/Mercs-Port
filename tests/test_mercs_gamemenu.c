#include "sdl3_test.h"
#include "../tools/mercsgamemenu_view.h"
#include "../tests/fixtures/dialog/menu_original_fixtures.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int selected, text_index;

int mercs_art_height(mercs_art *a, int font)
{ (void)a; assert(font == 8); return 15; }
int mercs_art_shape(mercs_art *a, int entry, int frame, int x, int y)
{ (void)a; assert(entry == 1 && frame == 0 && x == 198 && y == 116); return 0; }
void mercs_art_center(mercs_art *a, int font, int x, int y, const char *text, int ink)
{
    const menu_text *t;
    (void)a;
    assert(text_index >= 0 && text_index < 7);
    t = &menu_text_fixtures[text_index];
    assert(font == t->font && x-t->width/2 == t->x && y == t->y);
    assert(!strcmp(text,t->text));
    assert(ink == (text_index == selected ? 17 : t->ink));
    text_index++;
}

static SDL_Keycode native_key(int key)
{
    switch (key) {
    case 0x8048:return SDLK_UP;
    case 0x804b:return SDLK_LEFT;
    case 0x804d:return SDLK_RIGHT;
    case 0x8050:return SDLK_DOWN;
    default:return (SDL_Keycode)key;
    }
}

int main(void)
{
    size_t i;
    int j,result;
    mercs_game_menu m;
    for (i = 0; i < sizeof menu_fixtures/sizeof menu_fixtures[0]; i++) {
        const menu_fixture *f = &menu_fixtures[i];
        mercs_game_menu_open(&m); result = -1;
        for (j = 0; j < f->event_count; j++) {
            const menu_input *e = &f->events[j];
            selected = m.selected; text_index = 0;
            assert(!mercs_game_menu_draw((mercs_art *)1,&m) && text_index == 7);
            if (e->kind == 3) result = mercs_game_menu_key(&m,native_key(e->key));
            else result = mercs_game_menu_mouse(&m,e->x,e->y,e->kind == 4 ? MERCS_GAME_MENU_RIGHT_PRESS : e->kind);
            if (j+1 < f->event_count) assert(result == -1 && m.active);
        }
        assert(result == f->answer && m.answer == f->answer && !m.active);
        assert(mercs_game_menu_key(&m,SDLK_RETURN) == -1);
        assert(mercs_game_menu_mouse(&m,320,153,MERCS_GAME_MENU_PRESS) == -1);
    }
    mercs_game_menu_open(&m);
    for (j = 0; j < 7; j++) assert(mercs_game_menu_key(&m,SDLK_DOWN) == -1);
    assert(m.selected == 0);
    for (j = 0; j < 7; j++) assert(mercs_game_menu_key(&m,SDLK_KP_4) == -1);
    assert(m.selected == 0);
    assert(mercs_game_menu_key(&m,SDLK_KP_2) == -1 && m.selected == 1);
    assert(mercs_game_menu_key(&m,SDLK_KP_ENTER) == 1);
    puts("PASS: 40 original DOS Escape-menu input/layout goldens; native keypad mapping and closed-modal guards");
    return 0;
}
