/* SDL 3 test readback: renderers own their capture format. Convert the returned
 * surface into the fixture's explicit byte format before comparing pixels.
 * This helper is test-only; production uses SDL 3 APIs directly. */
#ifndef MERCS_SDL3_TEST_H
#define MERCS_SDL3_TEST_H
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

static inline int mercs_test_read_pixels(SDL_Renderer *renderer,
                                       const SDL_Rect *rect,
                                       SDL_PixelFormat format,
                                       void *pixels, int pitch)
{
    SDL_Surface *capture = SDL_RenderReadPixels(renderer, rect);
    bool ok;
    if (!capture) return -1;
    ok = SDL_ConvertPixels(capture->w, capture->h, capture->format,
                           capture->pixels, capture->pitch, format, pixels, pitch);
    SDL_DestroySurface(capture);
    return ok ? 0 : -1;
}
#endif
