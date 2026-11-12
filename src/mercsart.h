#ifndef MERCS_ART_H
#define MERCS_ART_H

#include <SDL3/SDL.h>
#include "shelldb.h"
#include "sfx.h"

/* Original 640x480 Mercs shell composition. All pixels come from DATABASE.MW2,
 * the original fonts, or the original SHP/SMK clips. Coordinates are authored
 * shell pixels; no replacement fonts, reskinned controls, or asset repacking. */
typedef struct mercs_art mercs_art;
typedef struct mercs_art_snapshot mercs_art_snapshot;
mercs_art *mercs_art_create(const shelldb *database, SDL_Renderer *renderer);
void mercs_art_destroy(mercs_art *art);
void mercs_art_reset_device(mercs_art *art);
void mercs_art_set_audio(mercs_art *art, sfx *audio);
/* Freeze the CPU pixels and their palette before composing a cursor or modal.
 * This does not copy decoder state, advance clips, or transfer asset/texture
 * ownership. Capture returns NULL with SDL error if allocation fails. */
mercs_art_snapshot *mercs_art_capture(const mercs_art *art);
void mercs_art_restore(mercs_art *art, const mercs_art_snapshot *snapshot);
void mercs_art_snapshot_free(mercs_art_snapshot *snapshot);
void mercs_art_begin_page(mercs_art *art);
int mercs_art_background(mercs_art *art, int entry);
/* A full-screen picture from another shell database (Titanium's own). */
int mercs_art_background_image(mercs_art *art, const shell_image *image);
/* A second folder searched for named clips (SMK\\NAME.SMK / .SHP) after the
 * game's own: Titanium's shell art. Empty clears it. */
void mercs_art_set_extra_dir(mercs_art *art, const char *dir);
int mercs_art_width(mercs_art *art, int font_entry, const char *text);
int mercs_art_height(mercs_art *art, int font_entry);
void mercs_art_text(mercs_art *art, int font_entry, int x, int y,
                    const char *text, int remap);
/* Text-only vertical clipping in authored pixels; top is inclusive, bottom
 * exclusive. Partial glyph rows are retained. No persistent clip state. */
void mercs_art_text_clipped(mercs_art *art, int font_entry, int x, int y,
                            const char *text, int remap, int top, int bottom);
void mercs_art_center(mercs_art *art, int font_entry, int x, int y,
                      const char *text, int remap);
/* Named clips are original cd/SMK files, resolved through the selected game
 * folder. Still SHPs retain their mask and hotspot offsets. SMKs advance at
 * their own frame duration. restart resets a transition to its first frame. */
int mercs_art_clip(mercs_art *art, const char *name, int x, int y, int restart);
/* Original visual-object flags: bit 1 pauses, bit 4 holds the final frame,
 * bit 8 repeats. The shell explicitly shows its constructed objects (bit 32).
 * Returns 1 when a non-looping movie has finished, 0 while
 * playing (or a still shape), -1 for a missing/invalid asset. */
int mercs_art_clip_flags(mercs_art *art, const char *name, int x, int y,
                         unsigned flags, int restart);
int mercs_art_button(mercs_art *art, const char *name, int x, int y, int active);
/* Named shell SHP, explicit authored frame. Repair body pieces use states
 * 0..2, their selected states3..5, and selected destroyed state6. */
int mercs_art_named_shape(mercs_art *art, const char *name, int frame, int x, int y);
int mercs_art_shape(mercs_art *art, int database_entry, int frame, int x, int y);
void mercs_art_rect(mercs_art *art, int x, int y, int w, int h, int palette_index);
void mercs_art_line(mercs_art *art, int x0, int y0, int x1, int y1, int palette_index);
/* Copy the original framebuffer to the caller's current target; does not
 * present the window. The shell selects mercs_viewport's 640x480 canvas first,
 * so its pages, settings and modal overlays share exactly the same transform. */
int mercs_art_present(mercs_art *art);

#endif
