/* Full-screen playback of the supplied Mercenaries Smacker movies. */
#ifndef MERCS_VIDEO_H
#define MERCS_VIDEO_H
#include <SDL3/SDL.h>
#include "portcfg.h"

/* Synchronous playback on the calling (SDL video) thread. window and renderer
 * remain owned by the caller. cfg may be NULL for original / 4:3 defaults.
 *
 * Return 0 when the movie finishes or is skipped, 1 for SDL quit / window close,
 * and -1 on an input, decoder or rendering error (SDL_GetError describes it).
 * Any non-modifier key or mouse button skips; Alt+Enter toggles fullscreen
 * without skipping. Movie audio uses an SDL3 stream with signed-16-bit input
 * and its own playback device;
 * an unavailable audio device is reported to the SDL log and video continues.
 * No global SDL shutdown, shared renderer destruction or shared audio reset.
 * All movie textures/filter caches are released before returning; renderer
 * drawing state is restored. A fullscreen change requested by the user persists.
 */
int mercs_video_play(SDL_Window *window, SDL_Renderer *renderer,
                     const portcfg *cfg, const char *movie_path);
#endif
