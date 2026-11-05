#include "mercsviewport.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

int mercs_renderer_pixels(SDL_Renderer *renderer)
{
    if (!renderer) { SDL_SetError("Mercs viewport needs a renderer"); return -1; }
    /* SDL3 keeps presentation, scale, viewport and clip state per target.
     * Reset the selected target explicitly so the original canvas and window
     * each use their own pixel coordinates, without an extra input transform. */
    if (!SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED) ||
        !SDL_SetRenderScale(renderer, 1.0f, 1.0f) ||
        !SDL_SetRenderViewport(renderer, NULL) ||
        !SDL_SetRenderClipRect(renderer, NULL)) return -1;
    return 0;
}

void mercs_viewport_init(mercs_viewport *view, SDL_Renderer *renderer)
{
    if (!view) return;
    memset(view, 0, sizeof *view);
    view->renderer = renderer;
}

void mercs_viewport_reset(mercs_viewport *view)
{
    if (!view) return;
    if (view->canvas) {
        if (view->renderer && SDL_GetRenderTarget(view->renderer) == view->canvas) {
            SDL_SetRenderTarget(view->renderer, NULL);
            mercs_renderer_pixels(view->renderer);
        }
        SDL_DestroyTexture(view->canvas);
        view->canvas = NULL;
    }
    view->output_w = view->output_h = 0;
}

int mercs_viewport_fit(int output_w, int output_h, SDL_Rect *destination)
{
    if (!destination) return -1;
    memset(destination, 0, sizeof *destination);
    if (output_w < 0 || output_h < 0) return -1;
    if (!output_w || !output_h) return 0;
    if ((int64_t)output_w * MERCS_VIEWPORT_HEIGHT <= (int64_t)output_h * MERCS_VIEWPORT_WIDTH) {
        destination->w = output_w;
        destination->h = (int)((int64_t)output_w * MERCS_VIEWPORT_HEIGHT / MERCS_VIEWPORT_WIDTH);
    } else {
        destination->h = output_h;
        destination->w = (int)((int64_t)output_h * MERCS_VIEWPORT_WIDTH / MERCS_VIEWPORT_HEIGHT);
    }
    if (!destination->w) destination->w = 1;
    if (!destination->h) destination->h = 1;
    destination->x = (output_w - destination->w) / 2;
    destination->y = (output_h - destination->h) / 2;
    return 1;
}

static int window_pixels(mercs_viewport *view)
{
    if (!SDL_SetRenderTarget(view->renderer, NULL) ||
        mercs_renderer_pixels(view->renderer) < 0 ||
        !SDL_GetRenderOutputSize(view->renderer, &view->output_w, &view->output_h)) return -1;
    return 0;
}

static int frame_error(mercs_viewport *view)
{
    char error[512];
    SDL_strlcpy(error, SDL_GetError(), sizeof error);
    if (view && view->renderer) {
        SDL_SetRenderTarget(view->renderer, NULL);
        mercs_renderer_pixels(view->renderer);
    }
    SDL_SetError("%s", error[0] ? error : "Mercs viewport presentation failed");
    return -1;
}

int mercs_viewport_begin(mercs_viewport *view)
{
    if (!view || !view->renderer) { SDL_SetError("Mercs viewport needs an initialized renderer"); return -1; }
    if (window_pixels(view) < 0) return frame_error(view);
    if (!view->canvas) {
        view->canvas = SDL_CreateTexture(view->renderer, SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_TARGET, MERCS_VIEWPORT_WIDTH, MERCS_VIEWPORT_HEIGHT);
        if (!view->canvas) return frame_error(view);
        if (!SDL_SetTextureBlendMode(view->canvas, SDL_BLENDMODE_NONE)) return frame_error(view);
        if (!SDL_SetTextureScaleMode(view->canvas, SDL_SCALEMODE_NEAREST)) return frame_error(view);
    }
    if (!SDL_SetRenderTarget(view->renderer, view->canvas) ||
        mercs_renderer_pixels(view->renderer) < 0 ||
        !SDL_SetRenderDrawBlendMode(view->renderer, SDL_BLENDMODE_BLEND) ||
        !SDL_SetRenderDrawColor(view->renderer, 0, 0, 0, 255) ||
        !SDL_RenderClear(view->renderer)) return frame_error(view);
    return 0;
}

int mercs_viewport_end(mercs_viewport *view)
{
    SDL_Rect destination;
    SDL_FRect draw;
    int fit;
    if (!view || !view->renderer || !view->canvas) {
        SDL_SetError("Mercs viewport has no completed canvas"); return -1;
    }
    if (window_pixels(view) < 0) return frame_error(view);
    fit = mercs_viewport_fit(view->output_w, view->output_h, &destination);
    if (fit < 0) { SDL_SetError("Mercs viewport received invalid drawable dimensions"); return frame_error(view); }
    if (!fit) return 0;
    draw = (SDL_FRect){(float)destination.x, (float)destination.y,
                      (float)destination.w, (float)destination.h};
    if (!SDL_SetRenderDrawBlendMode(view->renderer, SDL_BLENDMODE_NONE) ||
        !SDL_SetRenderDrawColor(view->renderer, 0, 0, 0, 255) ||
        !SDL_RenderClear(view->renderer) ||
        !SDL_RenderTexture(view->renderer, view->canvas, NULL, &draw)) return frame_error(view);
    return 0;
}

int mercs_viewport_map_point(int window_w, int window_h, int output_w, int output_h,
                           int raw_x, int raw_y, int *x, int *y)
{
    SDL_Rect destination;
    int64_t px, py, left, top, width, height;
    if (x) *x = -1;
    if (y) *y = -1;
    if (window_w <= 0 || window_h <= 0 ||
        mercs_viewport_fit(output_w, output_h, &destination) != 1) return -1;
    if (raw_x < 0 || raw_y < 0 || raw_x >= window_w || raw_y >= window_h) return 0;
    /* Keep the boundary comparison exact even when point and pixel sizes have
     * a fractional ratio. Use a wider intermediate for the final conversion
     * rather than overflowing a 64-bit product at theoretical INT_MAX sizes. */
    px = (int64_t)raw_x * output_w; py = (int64_t)raw_y * output_h;
    left = (int64_t)destination.x * window_w; top = (int64_t)destination.y * window_h;
    width = (int64_t)destination.w * window_w; height = (int64_t)destination.h * window_h;
    if (px < left || py < top || px - left >= width || py - top >= height) return 0;
    if (x) *x = (int)((long double)(px - left) * MERCS_VIEWPORT_WIDTH / width);
    if (y) *y = (int)((long double)(py - top) * MERCS_VIEWPORT_HEIGHT / height);
    return 1;
}

int mercs_viewport_window_point(mercs_viewport *view, SDL_Window *window,
                              float raw_x, float raw_y, int *x, int *y)
{
    int window_w, window_h;
    if (x) *x = -1;
    if (y) *y = -1;
    if (!view || !view->renderer) return -1;
    /* Query the actual window directly while a target is selected. SDL3's
     * software backend can report its active target surface as output size.
     * Surface-only test renderers have no window; keep the physical dimensions
     * captured by begin/end until their canvas is detached. */
    if (window) {
        if (!SDL_GetWindowSizeInPixels(window, &view->output_w, &view->output_h)) return -1;
    } else if (!SDL_GetRenderTarget(view->renderer) &&
               !SDL_GetRenderOutputSize(view->renderer, &view->output_w, &view->output_h)) return -1;
    window_w = view->output_w; window_h = view->output_h;
    if (window && !SDL_GetWindowSize(window, &window_w, &window_h)) return -1;
    return mercs_viewport_map_point_f(window_w, window_h, view->output_w, view->output_h, raw_x, raw_y, x, y);
}

int mercs_viewport_map_point_f(int window_w, int window_h, int output_w, int output_h,
                             float raw_x, float raw_y, int *x, int *y)
{
    SDL_Rect destination;
    long double px, py, left, top, width, height;
    if (x) *x = -1;
    if (y) *y = -1;
    if (window_w <= 0 || window_h <= 0 ||
        mercs_viewport_fit(output_w, output_h, &destination) != 1) return -1;
    if (!isfinite(raw_x) || !isfinite(raw_y) || raw_x < 0 || raw_y < 0 ||
        (double)raw_x >= window_w || (double)raw_y >= window_h) return 0;
    px = (long double)raw_x * output_w; py = (long double)raw_y * output_h;
    left = (long double)destination.x * window_w; top = (long double)destination.y * window_h;
    width = (long double)destination.w * window_w; height = (long double)destination.h * window_h;
    if (px < left || py < top || px - left >= width || py - top >= height) return 0;
    if (x) *x = (int)((px - left) * MERCS_VIEWPORT_WIDTH / width);
    if (y) *y = (int)((py - top) * MERCS_VIEWPORT_HEIGHT / height);
    return 1;
}
