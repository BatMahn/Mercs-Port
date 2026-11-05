#ifndef MERCS_VIEWPORT_H
#define MERCS_VIEWPORT_H

#include <SDL3/SDL.h>

enum { MERCS_VIEWPORT_WIDTH = 640, MERCS_VIEWPORT_HEIGHT = 480 };

/* The original shell has one 640x480 coordinate system. Compose every page,
 * settings panel and modal into this target, then copy it once to the physical
 * window. No SDL logical-size transform is left active between frames, so
 * absolute mouse events remain window points (not drawable pixels). */
typedef struct {
    SDL_Renderer *renderer;
    SDL_Texture *canvas;
    int output_w, output_h;
} mercs_viewport;

void mercs_viewport_init(mercs_viewport *view, SDL_Renderer *renderer);
/* Release the target after a renderer/device reset and before renderer cleanup.
 * The renderer association survives, allowing the next begin to recreate it. */
void mercs_viewport_reset(mercs_viewport *view);
/* Begin clears and selects the authored canvas. End selects the window, clears
 * its black bars and copies the canvas to its centered 4:3 destination. Neither
 * calls SDL_RenderPresent. A minimized zero-size drawable is a successful no-op
 * for end. Errors return -1 with SDL_GetError; begin/end leave the window target
 * selected on failure whenever SDL permits it. */
int mercs_viewport_begin(mercs_viewport *view);
int mercs_viewport_end(mercs_viewport *view);

/* Reset coordinate state for the currently selected target: no logical
 * presentation or clip, unit scale and the full target viewport. Draw color
 * and blend mode are left to the caller. Also used by full-window movies. */
int mercs_renderer_pixels(SDL_Renderer *renderer);

/* Fit without SDL state. Returns 1 for a nonempty rectangle, 0 for zero size,
 * -1 for negative dimensions or a null destination. */
int mercs_viewport_fit(int output_w, int output_h, SDL_Rect *destination);
/* Raw window points -> authored coordinates. Returns 1 inside the canvas,
 * 0 outside it (including letterbox bars), -1 for unavailable dimensions.
 * Output coordinates are -1 unless inside. Separate window/drawable sizes
 * handle Retina scaling in both dimensions; no guessed fixed DPI multiplier. */
int mercs_viewport_map_point(int window_w, int window_h, int output_w, int output_h,
                           int raw_x, int raw_y, int *x, int *y);
/* SDL3 mouse positions retain subpixel precision until the final conversion
 * to the original integer canvas. Letterbox boundaries are tested first. */
int mercs_viewport_map_point_f(int window_w, int window_h, int output_w, int output_h,
                             float raw_x, float raw_y, int *x, int *y);
/* Query window points and physical window pixels separately, even while the
 * canvas is selected. A null window is supported by software-surface test
 * renderers: use output pixels captured by begin/end, not the active target. */
int mercs_viewport_window_point(mercs_viewport *view, SDL_Window *window,
                              float raw_x, float raw_y, int *x, int *y);

#endif
