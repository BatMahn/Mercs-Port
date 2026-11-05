#include "sdl3_test.h"
/* Reproduce stale logical/viewport state without needing a Retina display.
 * Software-rendered pixels verify the entire 640x480 canvas remains visible;
 * separate point/drawable dimensions exercise 2x and fractional DPI input. */
#include "mercsviewport.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>

static int failures, frames, mappings;
static void check(int ok, const char *label)
{
    if (!ok) { fprintf(stderr, "FAIL: %s (%s)\n", label, SDL_GetError()); failures++; }
}

static Uint32 pixel(const SDL_Surface *surface, int x, int y)
{
    return *(const Uint32 *)((const Uint8 *)surface->pixels + (size_t)y * (size_t)surface->pitch + (size_t)x * 4) & 0xffffff;
}

static void draw_page(SDL_Renderer *renderer)
{
    SDL_FRect top_left = {0, 0, 12, 12}, top_right = {628, 0, 12, 12};
    SDL_FRect bottom_left = {0, 468, 12, 12}, bottom_right = {628, 468, 12, 12};
    SDL_FRect settings_row = {40, 240, 560, 20}, modal = {245, 214, 150, 54};
    SDL_SetRenderDrawColor(renderer, 32, 48, 64, 255); SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); SDL_RenderFillRect(renderer, &top_left);
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); SDL_RenderFillRect(renderer, &top_right);
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); SDL_RenderFillRect(renderer, &bottom_left);
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); SDL_RenderFillRect(renderer, &bottom_right);
    /* Ordinary SDL settings/modal primitives must use the same coordinates as
     * the original CPU art framebuffer, including after a custom clip. */
    SDL_SetRenderDrawColor(renderer, 128, 0, 128, 255); SDL_RenderFillRect(renderer, &settings_row);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &modal);
}

static void canvas_case(int width, int height, SDL_Rect expected)
{
    SDL_Surface *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *renderer = surface ? SDL_CreateSoftwareRenderer(surface) : NULL;
    SDL_Rect poison = {157, 19, 533, 307}, clip = {200, 30, 70, 90}, actual;
    mercs_viewport view;
    int i, x, y, lw, lh, ow, oh;
    float sx, sy;
    check(renderer != NULL, "software renderer allocated");
    if (!renderer) { SDL_DestroySurface(surface); return; }
    mercs_viewport_init(&view, renderer);
    for (i = 0; i < 5; i++) {
        /* Model the stale state left by logical rendering, a movie, and an
         * already-scaled custom viewport. Production must not inherit it. */
        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderLogicalPresentation(renderer, 640, 480,
            i & 1 ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetRenderScale(renderer, 2.0f + (float)i, 1.5f);
        SDL_SetRenderViewport(renderer, &poison);
        SDL_SetRenderClipRect(renderer, &clip);
        check(mercs_viewport_begin(&view) == 0, "begin clears stale presentation state");
        check(SDL_GetRenderTarget(renderer) == view.canvas, "begin selects the authored canvas");
        SDL_GetCurrentRenderOutputSize(renderer, &ow, &oh);
        SDL_GetRenderLogicalPresentation(renderer, &lw, &lh, NULL);
        SDL_GetRenderScale(renderer, &sx, &sy);
        check(ow == 640 && oh == 480 && !lw && !lh && sx == 1.0f && sy == 1.0f &&
              !SDL_RenderClipEnabled(renderer), "canvas dimensions and unit coordinates");
        check(mercs_viewport_window_point(&view, NULL, (float)(width / 2), (float)(height / 2), &x, &y) == 1 &&
              x >= 319 && x <= 320 && y >= 239 && y <= 240,
              "mouse mapping during composition uses physical output dimensions");
        draw_page(renderer);
        check(mercs_viewport_end(&view) == 0, "end fits the complete canvas");
        SDL_GetRenderViewport(renderer, &actual);
        SDL_GetRenderLogicalPresentation(renderer, &lw, &lh, NULL);
        SDL_GetRenderScale(renderer, &sx, &sy);
        check(!SDL_GetRenderTarget(renderer) && !lw && !lh && sx == 1.0f && sy == 1.0f &&
              !actual.x && !actual.y && actual.w == width && actual.h == height,
              "window remains in physical pixel coordinates");
        check(mercs_test_read_pixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) == 0,
              "read complete physical drawable");
        check(pixel(surface, expected.x + 2, expected.y + 2) == 0xff0000 &&
              pixel(surface, expected.x + expected.w - 3, expected.y + 2) == 0x00ff00 &&
              pixel(surface, expected.x + 2, expected.y + expected.h - 3) == 0x0000ff &&
              pixel(surface, expected.x + expected.w - 3, expected.y + expected.h - 3) == 0xffff00,
              "all four authored corners remain visible and aligned");
        check(pixel(surface, width / 2, height / 2) == 0xffffff,
              "modal remains centered on the same canvas as art and settings");
        check(pixel(surface, expected.x + expected.w / 8, expected.y + expected.h * 250 / 480) == 0x800080,
              "settings row follows the authored canvas");
        if (expected.x) check(!pixel(surface, expected.x - 1, height / 2), "left letterbox bar remains black");
        if (expected.y) check(!pixel(surface, width / 2, expected.y - 1), "top letterbox bar remains black");
        if (expected.x + expected.w < width)
            check(!pixel(surface, expected.x + expected.w, height / 2), "right letterbox bar remains black");
        if (expected.y + expected.h < height)
            check(!pixel(surface, width / 2, expected.y + expected.h), "bottom letterbox bar remains black");
        if (i == 2) mercs_viewport_reset(&view);
        frames++;
    }
    /* A reset while drawing must detach the target before destroying it. */
    check(mercs_viewport_begin(&view) == 0, "begin before device invalidation");
    mercs_viewport_reset(&view);
    check(!view.canvas && !SDL_GetRenderTarget(renderer), "device reset safely detaches the canvas");
    check(mercs_viewport_begin(&view) == 0 && mercs_viewport_end(&view) == 0,
          "device reset recreates a usable canvas");
    mercs_viewport_reset(&view);
    SDL_DestroyRenderer(renderer); SDL_DestroySurface(surface);
}

static void point_case(int ww, int wh, int ow, int oh, int px, int py, int expected, int ex, int ey)
{
    int x = 123, y = 123;
    int result = mercs_viewport_map_point(ww, wh, ow, oh, px, py, &x, &y);
    if (result != expected || x != ex || y != ey) {
        fprintf(stderr, "FAIL: point %d,%d in %dx%d -> %dx%d: got %d:%d,%d expected %d:%d,%d\n",
                px, py, ww, wh, ow, oh, result, x, y, expected, ex, ey);
        failures++;
    }
    mappings++;
}

static void fractional_case(int ww, int wh, int ow, int oh, float px, float py, int expected, int ex, int ey)
{
    int x = 123, y = 123;
    int result = mercs_viewport_map_point_f(ww, wh, ow, oh, px, py, &x, &y);
    if (result != expected || x != ex || y != ey) {
        fprintf(stderr, "FAIL: subpixel point %.7g,%.7g in %dx%d -> %dx%d: got %d:%d,%d expected %d:%d,%d\n",
                px, py, ww, wh, ow, oh, result, x, y, expected, ex, ey);
        failures++;
    }
    mappings++;
}

int main(void)
{
    SDL_Rect rect;
    SDL_SetMainReady();
    if (!SDL_Init(0)) return 2;
    canvas_case(640, 480, (SDL_Rect){0, 0, 640, 480});
    canvas_case(1280, 960, (SDL_Rect){0, 0, 1280, 960});
    canvas_case(2048, 1330, (SDL_Rect){137, 0, 1773, 1330});
    canvas_case(2048, 1286, (SDL_Rect){167, 0, 1714, 1286});
    canvas_case(1600, 900, (SDL_Rect){200, 0, 1200, 900});
    canvas_case(800, 700, (SDL_Rect){0, 50, 800, 600});
    canvas_case(800, 1200, (SDL_Rect){0, 300, 800, 600});
    canvas_case(1920, 1080, (SDL_Rect){240, 0, 1440, 1080});
    point_case(640, 480, 640, 480, 0, 0, 1, 0, 0);
    point_case(640, 480, 640, 480, 639, 479, 1, 639, 479);
    point_case(640, 480, 640, 480, 640, 240, 0, -1, -1);
    point_case(640, 480, 640, 480, -1, 240, 0, -1, -1);
    point_case(1024, 768, 2048, 1536, 512, 384, 1, 320, 240);
    point_case(1024, 768, 2048, 1536, 1023, 767, 1, 639, 479);
    point_case(1024, 665, 2048, 1330, 68, 200, 0, -1, -1);
    point_case(1024, 665, 2048, 1330, 69, 200, 1, 0, 144);
    point_case(1024, 665, 2048, 1330, 954, 200, 1, 639, 144);
    point_case(1024, 665, 2048, 1330, 955, 200, 0, -1, -1);
    point_case(1024, 665, 2048, 1330, 512, 332, 1, 320, 239);
    point_case(1024, 665, 2048, 1330, 512, 333, 1, 320, 240);
    point_case(400, 350, 800, 700, 200, 24, 0, -1, -1);
    point_case(400, 350, 800, 700, 200, 25, 1, 320, 0);
    point_case(400, 350, 800, 700, 200, 324, 1, 320, 478);
    point_case(400, 350, 800, 700, 200, 325, 0, -1, -1);
    point_case(1000, 700, 2500, 1400, 500, 350, 1, 320, 240);
    point_case(1000, 700, 2500, 1400, 126, 350, 0, -1, -1);
    point_case(1000, 700, 2500, 1400, 127, 350, 1, 0, 240);
    point_case(0, 480, 640, 480, 0, 0, -1, -1, -1);
    point_case(640, 480, 0, 0, 0, 0, -1, -1, -1);
    point_case(INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX / 2, INT_MAX / 2, 1, 319, 239);
    fractional_case(640, 480, 640, 480, 0.25f, 0.75f, 1, 0, 0);
    fractional_case(640, 480, 640, 480, -0.25f, 10.0f, 0, -1, -1);
    fractional_case(640, 480, 640, 480, 639.75f, 479.75f, 1, 639, 479);
    fractional_case(640, 480, 640, 480, 640.0f, 479.75f, 0, -1, -1);
    /* Retina letterbox starts halfway through a window point: integer
     * truncation would incorrectly reject the exact left edge and end. */
    fractional_case(1024, 665, 2048, 1330, 68.49f, 200.0f, 0, -1, -1);
    fractional_case(1024, 665, 2048, 1330, 68.5f, 200.0f, 1, 0, 144);
    fractional_case(1024, 665, 2048, 1330, 954.99f, 200.0f, 1, 639, 144);
    fractional_case(1024, 665, 2048, 1330, 955.0f, 200.0f, 0, -1, -1);
    fractional_case(400, 350, 800, 700, 200.0f, 24.99f, 0, -1, -1);
    fractional_case(400, 350, 800, 700, 200.0f, 25.0f, 1, 320, 0);
    fractional_case(400, 350, 800, 700, 200.0f, 324.99f, 1, 320, 479);
    fractional_case(400, 350, 800, 700, 200.0f, 325.0f, 0, -1, -1);
    fractional_case(1000, 700, 2500, 1400, 126.79f, 350.0f, 0, -1, -1);
    fractional_case(1000, 700, 2500, 1400, 126.8f, 350.0f, 1, 0, 240);
    fractional_case(640, 480, 640, 480, NAN, 240.0f, 0, -1, -1);
    fractional_case(640, 480, 640, 480, 320.0f, INFINITY, 0, -1, -1);
    check(mercs_viewport_fit(0, 480, &rect) == 0 && !rect.w && !rect.h,
          "minimized zero-size drawable has no destination");
    check(mercs_viewport_fit(-1, 480, &rect) == -1 &&
          mercs_viewport_fit(640, 480, NULL) == -1, "invalid fit input rejected");
    check(mercs_viewport_begin(NULL) == -1 && mercs_viewport_end(NULL) == -1,
          "uninitialized renderer reports a bounded error");
    SDL_Quit();
    printf("test_mercs_viewport: %s (%d frames, %d point mappings, %d failures)\n",
           failures ? "FAILED" : "ok", frames, mappings, failures);
    return failures != 0;
}
