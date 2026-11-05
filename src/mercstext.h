#ifndef MERCS_TEXT_H
#define MERCS_TEXT_H

#include <stddef.h>
#include "mercsart.h"

/* Positioned original-font runs, in authored shell pixels. The layout owns
 * every string and may be zero-initialized; free it before discarding it.
 * y=0 is the first row. Font 6 is the original large/news font. */
typedef struct mercs_text_run {
    int x, y, font, width, height;
    int ink;                    /* -1: caller ink; otherwise original remap */
    char *text;
} mercs_text_run;

typedef struct mercs_text_layout {
    mercs_text_run *runs;
    size_t count, capacity;
    int width, height, line_height, rows;
    char *expanded;
    /* A section break stops before emitting the pending line, as the original
     * shell does. next_offset indexes expanded, after the consumed \\s. At EOF
     * it equals strlen(expanded). It is not an offset into the caller's raw
     * text, since name substitution can change the length. */
    size_t next_offset;
    int stopped;
    unsigned unsupported_controls;
} mercs_text_layout;

int mercs_text_prepare(mercs_text_layout *out, mercs_art *art,
                       const char *raw, const char *pilot, int font, int width);
int mercs_text_prepare_names(mercs_text_layout *out, mercs_art *art,
                             const char *raw, const char *pilot,
                             const char *unit, const char *extra,
                             int font, int width);
void mercs_text_draw(const mercs_text_layout *layout, mercs_art *art,
                      int x, int y, int height, int scroll_pixels, int ink);
/* Original 0x703d4 completes one prepared line per UI poll. All inline runs
 * on that authored row appear together. A negative limit draws every row. */
void mercs_text_draw_rows(const mercs_text_layout *layout, mercs_art *art,
                           int x, int y, int height, int scroll_pixels, int ink,
                           int visible_rows);
void mercs_text_free(mercs_text_layout *layout);
int mercs_text_rows(const mercs_text_layout *layout);

#endif
