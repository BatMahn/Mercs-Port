#ifndef MERCS_DIALOG_VIEW_H
#define MERCS_DIALOG_VIEW_H

/* Original DOS message/confirmation host0x5cd5c and button layout0x5cc84.
 * DATABASE resource1 is native entry0: panel frame2, normal/pressed frame0/1.
 * Font resource11 is native entry10. Coordinates are original640x480 pixels.
 *
 * open() takes the original "line|line#Yes|No" syntax and the ORIGINAL default
 * choice index. BOTH Enter and Escape choose that index. Initial-letter keys
 * choose the first matching label, case-insensitively. There is no Tab/arrow
 * navigation in the original routine. Mouse activation requires a fresh left
 * press and release within that same button's inclusive original hit rectangle.
 *
 * The caller traps input while active, keeps window/resize/fullscreen events
 * working, suspends text input, converts event points to640x480, and draws this
 * overlay after the existing page. It must never forward modal input to the
 * room behind it. cancel_press() prevents focus-loss releases activating it.
 *
 * notice() is explicitly native error presentation: original font/box with
 * bounded word wrapping and Next/Ok pages. Original authored messages use
 * open() so their deliberate line breaks and centering remain unchanged.
 */
#include "mercsart.h"
#include <stddef.h>
#include <string.h>

enum {
    MERCS_DIALOG_PENDING = -1,
    MERCS_DIALOG_INVALID = -2,
    MERCS_DIALOG_MOVE = 0,
    MERCS_DIALOG_PRESS = 1,
    MERCS_DIALOG_RELEASE = 2,
    MERCS_DIALOG_FONT = 10,
    MERCS_DIALOG_INK = 17,
    MERCS_DIALOG_LINES_PER_PAGE = 4,
    MERCS_DIALOG_MAX_LINES = 64,
    MERCS_DIALOG_LINE_BYTES = 256,
    MERCS_DIALOG_MAX_BUTTONS = 4,
    MERCS_DIALOG_LABEL_BYTES = 128,
    MERCS_DIALOG_MESSAGE_BYTES = 4096
};
typedef struct mercs_dialog {
    char lines[MERCS_DIALOG_MAX_LINES][MERCS_DIALOG_LINE_BYTES];
    char labels[MERCS_DIALOG_MAX_BUTTONS][MERCS_DIALOG_LABEL_BYTES];
    int line_count, count, default_choice, pressed, held, pointer_x, pointer_y;
    int active, answer, page, paginated;
} mercs_dialog;

static inline int mercs_dialog_lower(int c)
{ return c >= 'A' && c <= 'Z' ? c+'a'-'A' : c; }

static inline void mercs_dialog_cancel_press(mercs_dialog *d)
{ if (d) { d->pressed = -1; d->held = 0; } }

static inline int mercs_dialog_open(mercs_dialog *d, const char *spec, int default_choice)
{
    mercs_dialog next;
    const char *p, *end;
    size_t total = 0, n;
    if (!d || !spec) return MERCS_DIALOG_INVALID;
    while (total < MERCS_DIALOG_MESSAGE_BYTES && spec[total]) total++;
    if (total == MERCS_DIALOG_MESSAGE_BYTES) return MERCS_DIALOG_INVALID;
    memset(&next,0,sizeof next); p = spec;
    while (*p && *p != '#') {
        end = p; while (*end && *end != '|' && *end != '#') end++;
        n = (size_t)(end-p);
        if (next.line_count == MERCS_DIALOG_LINES_PER_PAGE || n >= MERCS_DIALOG_LINE_BYTES)
            return MERCS_DIALOG_INVALID;
        memcpy(next.lines[next.line_count++],p,n);
        p = *end == '|' ? end+1 : end;
    }
    if (*p++ != '#') return MERCS_DIALOG_INVALID;
    while (*p) {
        end = p; while (*end && *end != '|') end++;
        n = (size_t)(end-p);
        if (!n || next.count == MERCS_DIALOG_MAX_BUTTONS || n >= MERCS_DIALOG_LABEL_BYTES)
            return MERCS_DIALOG_INVALID;
        memcpy(next.labels[next.count++],p,n);
        if (*end == '|' && !end[1]) return MERCS_DIALOG_INVALID;
        p = *end ? end+1 : end;
    }
    if (!next.count || default_choice < 0 || default_choice >= next.count)
        return MERCS_DIALOG_INVALID;
    next.default_choice = default_choice; next.active = 1;
    next.pressed = next.answer = -1;
    *d = next;
    return 0;
}

/* Native diagnostics preserve all non-whitespace bytes across bounded pages.
 * Do not silently replace the current modal if a message exceeds these bounds. */
static inline int mercs_dialog_notice(mercs_dialog *d, mercs_art *art, const char *message)
{
    mercs_dialog next;
    char line[MERCS_DIALOG_LINE_BYTES];
    size_t total = 0, used = 0, last_space = 0;
    const char *p;
    if (!d || !art || !message || mercs_art_height(art,MERCS_DIALOG_FONT) <= 0)
        return MERCS_DIALOG_INVALID;
    while (total < MERCS_DIALOG_MESSAGE_BYTES && message[total]) total++;
    if (total == MERCS_DIALOG_MESSAGE_BYTES) return MERCS_DIALOG_INVALID;
    memset(&next,0,sizeof next); p = message;
    while (*p || used) {
        int newline = *p == '\n' || *p == '\r';
        int full = used == sizeof line-2;
        if (*p && !newline && !full) {
            line[used] = *p; line[used+1] = 0;
            if (mercs_art_width(art,MERCS_DIALOG_FONT,line) <= 268) {
                if (*p == ' ' || *p == '\t') last_space = used+1;
                used++; p++; continue;
            }
        }
        if (!used && !newline && *p) return MERCS_DIALOG_INVALID;
        if (next.line_count == MERCS_DIALOG_MAX_LINES) return MERCS_DIALOG_INVALID;
        if (*p && !newline && last_space) {
            size_t remainder = used-last_space;
            line[last_space-1] = 0;
            memcpy(next.lines[next.line_count++],line,last_space);
            memmove(line,line+last_space,remainder); used = remainder;
        } else {
            line[used] = 0;
            memcpy(next.lines[next.line_count++],line,used+1); used = 0;
            if (newline) { if (*p++ == '\r' && *p == '\n') p++; }
        }
        last_space = 0;
        if (used) {
            size_t i;
            for (i = 0; i < used; i++) if (line[i] == ' ' || line[i] == '\t') last_space = i+1;
        } else while (*p == ' ' || *p == '\t') p++;
    }
    if (!next.line_count) return MERCS_DIALOG_INVALID;
    next.count = 1; next.active = 1; next.answer = next.pressed = -1;
    next.paginated = 1;
    strcpy(next.labels[0],next.line_count > MERCS_DIALOG_LINES_PER_PAGE ? "Next" : "Ok");
    *d = next;
    return 0;
}

static inline void mercs_dialog_button_rect(const mercs_dialog *d, int index,
                                             int *left, int *top, int *right, int *bottom)
{
    int x = 320-(d->count-1)*30+61*index-26;
    if (left) *left = x;
    if (top) *top = 263;
    if (right) *right = x+52;
    if (bottom) *bottom = 289;
}

static inline int mercs_dialog_hit(const mercs_dialog *d, int x, int y)
{
    int i;
    if (!d || !d->active || y < 263 || y > 289) return -1;
    for (i = 0; i < d->count; i++) {
        int left;
        mercs_dialog_button_rect(d,i,&left,NULL,NULL,NULL);
        if (x >= left && x <= left+52) return i;
    }
    return -1;
}

static inline int mercs_dialog_choose(mercs_dialog *d, int choice)
{
    if (!d || !d->active || choice < 0 || choice >= d->count) return MERCS_DIALOG_PENDING;
    mercs_dialog_cancel_press(d);
    if (d->paginated && (d->page+1)*MERCS_DIALOG_LINES_PER_PAGE < d->line_count) {
        d->page++;
        strcpy(d->labels[0],(d->page+1)*MERCS_DIALOG_LINES_PER_PAGE < d->line_count ? "Next" : "Ok");
        return MERCS_DIALOG_PENDING;
    }
    d->answer = choice; d->active = 0;
    return choice;
}

static inline int mercs_dialog_key(mercs_dialog *d, SDL_Keycode key)
{
    int i;
    if (!d || !d->active) return MERCS_DIALOG_PENDING;
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_ESCAPE)
        return mercs_dialog_choose(d,d->default_choice);
    if (key > 255u) return MERCS_DIALOG_PENDING;   /* SDL 3 keycodes are unsigned; accelerators are original single-byte labels. */
    for (i = 0; i < d->count; i++)
        if (mercs_dialog_lower((unsigned char)d->labels[i][0]) == mercs_dialog_lower((int)key))
            return mercs_dialog_choose(d,i);
    return MERCS_DIALOG_PENDING;
}

static inline int mercs_dialog_mouse(mercs_dialog *d, int x, int y, int event)
{
    int choice;
    if (!d || !d->active) return MERCS_DIALOG_PENDING;
    d->pointer_x = x; d->pointer_y = y;
    if (event == MERCS_DIALOG_PRESS) {
        if (!d->held) { d->held = 1; d->pressed = mercs_dialog_hit(d,x,y); }
    } else if (event == MERCS_DIALOG_RELEASE) {
        choice = d->held && d->pressed == mercs_dialog_hit(d,x,y) ? d->pressed : -1;
        mercs_dialog_cancel_press(d);
        if (choice >= 0) return mercs_dialog_choose(d,choice);
    }
    return MERCS_DIALOG_PENDING;
}

static inline int mercs_dialog_draw(mercs_art *a, const mercs_dialog *d)
{
    int i, first;
    if (!a || !d || !d->active) return MERCS_DIALOG_INVALID;
    if (mercs_art_shape(a,0,2,172,165)) return MERCS_DIALOG_INVALID;
    first = d->page*MERCS_DIALOG_LINES_PER_PAGE;
    for (i = 0; i < MERCS_DIALOG_LINES_PER_PAGE && first+i < d->line_count; i++)
        mercs_art_center(a,MERCS_DIALOG_FONT,320,197+16*i,d->lines[first+i],MERCS_DIALOG_INK);
    for (i = 0; i < d->count; i++) {
        int left, pressed = d->held && d->pressed == i &&
            mercs_dialog_hit(d,d->pointer_x,d->pointer_y) == i;
        mercs_dialog_button_rect(d,i,&left,NULL,NULL,NULL);
        if (mercs_art_shape(a,0,pressed, left,263)) return MERCS_DIALOG_INVALID;
        mercs_art_center(a,MERCS_DIALOG_FONT,left+26,
            276-mercs_art_height(a,MERCS_DIALOG_FONT)/2,d->labels[i],MERCS_DIALOG_INK);
    }
    return 0;
}

#endif
