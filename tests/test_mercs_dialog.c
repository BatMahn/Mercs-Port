/* Dialog behavior and initial layout compared to executed DOS0x5cd5c.
 * Original string widths are supplied by that oracle's real DATABASE font.
 * Native diagnostic wrapping/focus safeguards are tested separately. */
#include "../tools/mercsdialog_view.h"
#include "../tests/fixtures/dialog/dialog_original_fixtures.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static const dialog_fixture *fixture;
static int text_index, shape_index, frame_index;
static mercs_dialog *drawing;

int mercs_art_width(mercs_art *art, int font, const char *text)
{
    int i;
    (void)art; assert(font == 10);
    if (fixture) for (i = 0; i < fixture->text_count; i++)
        if (!strcmp(fixture->text[i].text,text)) return fixture->text[i].width;
    return (int)strlen(text)*6;
}
int mercs_art_height(mercs_art *art, int font)
{ (void)art; assert(font == 10); return 10; }
void mercs_art_center(mercs_art *art, int font, int x, int y, const char *text, int ink)
{
    int width = mercs_art_width(art,font,text);
    if (fixture) {
        const dialog_text *t = &fixture->text[text_index];
        assert(text_index < fixture->text_count);
        assert(t->x == x-width/2 && t->y == y && t->font == font && t->ink == ink);
        assert(!strcmp(t->text,text));
    } else assert(width <= 268 || y == 271);
    text_index++;
}
int mercs_art_shape(mercs_art *art, int entry, int frame, int x, int y)
{
    (void)art; assert(entry == 0);
    if (!shape_index) assert(frame == 2 && x == 172 && y == 165);
    else {
        int button = shape_index-1;
        assert(x == 320-(drawing->count-1)*30+61*button-26 && y == 263);
        if (fixture) assert(frame == fixture->frames[frame_index][button]);
        else assert(frame == 0 || frame == 1);
    }
    shape_index++;
    return 0;
}

static void draw(mercs_dialog *dialog)
{
    drawing = dialog; text_index = shape_index = 0;
    assert(!mercs_dialog_draw((mercs_art *)1,dialog));
    if (fixture) assert(text_index == fixture->text_count);
    assert(shape_index == dialog->count+1);
}

static void original_goldens(void)
{
    mercs_dialog dialog;
    size_t i;
    int j,result;
    for (i = 0; i < sizeof dialog_fixtures/sizeof dialog_fixtures[0]; i++) {
        fixture = &dialog_fixtures[i];
        assert(!mercs_dialog_open(&dialog,fixture->spec,fixture->default_choice));
        result = -1;
        for (j = 0; j < fixture->event_count; j++) {
            const dialog_input *e = &fixture->events[j];
            frame_index = j; draw(&dialog);
            if (e->kind == 3) result = mercs_dialog_key(&dialog,(SDL_Keycode)e->key);
            else result = mercs_dialog_mouse(&dialog,e->x,e->y,e->kind);
            if (j+1 < fixture->event_count) assert(result == -1 && dialog.active);
        }
        assert(result == fixture->answer && dialog.answer == fixture->answer && !dialog.active);
    }
    fixture = NULL;
}

static void native_boundaries(void)
{
    mercs_dialog d, before;
    int x0,y0,x1,y1;
    char too_long[MERCS_DIALOG_MESSAGE_BYTES+1];
    assert(!mercs_dialog_open(&d,"Confirm?#Yes|No",1)); before = d;
    assert(mercs_dialog_open(&d,"No button separator",0) == -2);
    assert(!memcmp(&d,&before,sizeof d));
    assert(mercs_dialog_open(&d,"Bad#",0) == -2);
    assert(mercs_dialog_open(&d,"Bad#Yes|",0) == -2);
    assert(mercs_dialog_open(&d,"Bad#Yes||No",0) == -2);
    assert(mercs_dialog_open(&d,"Bad#Yes|No",2) == -2);
    assert(mercs_dialog_open(&d,"1|2|3|4|5#Ok",0) == -2);
    assert(mercs_dialog_open(&d,"#1|2|3|4|5",0) == -2);
    memset(too_long,'x',sizeof too_long-1); too_long[sizeof too_long-1] = 0;
    assert(mercs_dialog_open(&d,too_long,0) == -2);
    assert(!memcmp(&d,&before,sizeof d));
    mercs_dialog_button_rect(&d,0,&x0,&y0,&x1,&y1);
    assert(x0 == 264 && x1 == 316 && y0 == 263 && y1 == 289);
    assert(mercs_dialog_mouse(&d,290,276,MERCS_DIALOG_PRESS) == -1);
    mercs_dialog_cancel_press(&d);
    assert(mercs_dialog_mouse(&d,290,276,MERCS_DIALOG_RELEASE) == -1 && d.active);
    assert(mercs_dialog_key(&d,SDLK_TAB) == -1);
    assert(mercs_dialog_key(&d,SDLK_LEFT) == -1);
    assert(mercs_dialog_key(&d,SDLK_KP_ENTER) == 1);
    assert(mercs_dialog_key(&d,SDLK_Y) == -1 && d.answer == 1);
}

static void native_notice(void)
{
    mercs_dialog d,before;
    char message[2048], recovered[4096];
    size_t n = 0, at = 0, i;
    int pages = 0;
    /* Unique bounded tokens prove paginated diagnostics do not drop their tail. */
    for (i = 0; i < 160; i++) n += (size_t)snprintf(message+n,sizeof message-n,"token%03u%c",(unsigned)i,i%13 == 12 ? '\n' : ' ');
    assert(!mercs_dialog_notice(&d,(mercs_art *)1,message));
    assert(d.line_count > 4 && d.line_count <= 64);
    recovered[0] = 0;
    for (i = 0; i < (size_t)d.line_count; i++) {
        assert(strlen(d.lines[i])*6 <= 268);
        at += (size_t)snprintf(recovered+at,sizeof recovered-at,"%s ",d.lines[i]);
    }
    for (i = 0; i < 160; i++) {
        char token[16]; snprintf(token,sizeof token,"token%03u",(unsigned)i);
        assert(strstr(recovered,token));
    }
    while (d.active) {
        draw(&d); pages++;
        (void)mercs_dialog_key(&d,SDLK_RETURN);
    }
    assert(pages == (d.line_count+3)/4 && d.answer == 0);
    assert(!mercs_dialog_notice(&d,(mercs_art *)1,"one\r\ntwo\n\nthree"));
    assert(d.line_count == 4 && !strcmp(d.lines[0],"one") && !strcmp(d.lines[1],"two") &&
        !strcmp(d.lines[2],"") && !strcmp(d.lines[3],"three"));
    assert(!mercs_dialog_notice(&d,(mercs_art *)1,
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"));
    assert(d.line_count == 3);
    before = d;
    assert(mercs_dialog_notice(&d,(mercs_art *)1,"") == -2 && !memcmp(&d,&before,sizeof d));
    assert(mercs_dialog_notice(&d,NULL,"text") == -2 && !memcmp(&d,&before,sizeof d));
}

int main(void)
{
    original_goldens(); native_boundaries(); native_notice();
    puts("PASS: 29 original DOS dialog layout/input goldens; native bounds, focus, wrapping and complete diagnostic pagination");
    return 0;
}
