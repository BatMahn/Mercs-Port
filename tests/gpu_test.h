/* GPU fixture boundary: real fenced RGBA readback, no window-system graphics
 * context. Fixtures may vary their target size on each draw. No pixel value is
 * fabricated; a failed readback remains a sticky test error. */
#ifndef MERCS_GPU_TEST_H
#define MERCS_GPU_TEST_H
#include "rb.h"
#include "glr.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
static int gpu_test_failed;
static inline int gpu_test_start(void)
{
    char error[512];
    gpu_test_failed = 0;
    if (rb_init(NULL, RB_GPU_AUTO, 1, error, sizeof error) != 0) {
        fprintf(stderr, "GPU fixture: %s\n", error);
        SDL_SetError("%s", error);
        return 0;
    }
    rb_frame(1, 1);
    return 1;
}
static inline void gpu_test_draw(glr *r, const glr_view *v, int w, int h)
{
    rb_frame(w, h); rb_target(0); glr_draw(r, v, w, h);
}
static inline void gpu_test_read_rgba(int x, int y, int w, int h, unsigned char *rgba)
{
    if (!rb_read_rgba(x, y, w, h, rgba)) gpu_test_failed = 1;
}
static inline int gpu_test_ok(void) { return !gpu_test_failed; }
/* A nearly far-plane marker must pass where a discarded bitmap left depth
 * clear. This tests occlusion directly across D24 and D32 devices. */
static inline int gpu_test_depth_clear(int x, int y)
{
    static const float quad[6][7] = {
        {-1,-1,0.999999f,1,1,0,1}, {1,-1,0.999999f,1,1,0,1}, {-1,1,0.999999f,1,1,0,1},
        {-1,1,0.999999f,1,1,0,1}, {1,-1,0.999999f,1,1,0,1}, {1,1,0.999999f,1,1,0,1}
    };
    size_t off; void *vertices = rb_stream(sizeof quad, &off);
    unsigned char pixel[4];
    if (!vertices) { gpu_test_failed = 1; return 0; }
    memcpy(vertices, quad, sizeof quad);
    rb_program(RB_PROG_PARTICLE); rb_vertex(0, RB_STREAM, off);
    rb_blend(RB_BLEND_OFF); rb_depth_test(1); rb_depth_write(0); rb_depth_func(RB_LESS);
    rb_depth_bias(0, 0, 0); rb_scissor(0, 0, 0, 0, 0);
    rb_draw(0, 6); gpu_test_read_rgba(x, y, 1, 1, pixel);
    return gpu_test_ok() && pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 255;
}
#endif
