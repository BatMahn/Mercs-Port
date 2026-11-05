/* Actual GPU regression for opt-in DOS filtering and the two texture switches.
 * Deliberately unrelated palette indices make interpolating indices fail:
 * smooth pixels must blend the four resolved colours, including their LUMA
 * remapping. No game assets or renderer internals are used by this fixture. */
#define SDL_MAIN_HANDLED
#include "sdl3_test.h"
#include "gpu_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glr.h"

enum { WIDTH = 128, HEIGHT = 128, BYTES = WIDTH * HEIGHT * 4 };
static uint8_t palette[256][3], luma[4096];
static const uint8_t indices[4] = {10,70,160,230}, shaded_indices[4] = {40,210,5,120};
static const uint8_t raw_colours[4][3] = {{20,220,90},{240,40,30},{70,10,240},{240,210,70}};
static const uint8_t shaded_colours[4][3] = {{240,20,10},{10,230,30},{20,40,220},{210,200,25}};
static const uint8_t fallback[3] = {31,83,147};
static uint32_t checker_pixels[4], keyed_pixels[4];
static texture checker, keyed;
static texture *slots[GLR_SLOTS], *bank0[GLR_SLOTS];
static uint8_t original[BYTES], nearest[BYTES], smooth[BYTES], restored[BYTES], background[BYTES];
static int passed, failed;

static void check(int condition, const char *name)
{
    if (condition) passed++;
    else { fprintf(stderr, "FAIL: %s\n", name); failed++; }
}

static void setup_palette(void)
{
    int i, j;
    for (i = 0; i < 256; i++) {
        palette[i][0] = palette[i][2] = 255; palette[i][1] = 0;
        for (j = 0; j < 16; j++) luma[j * 256 + i] = (uint8_t)i;
    }
    for (i = 0; i < 4; i++) {
        memcpy(palette[indices[i]], raw_colours[i], 3);
        memcpy(palette[shaded_indices[i]], shaded_colours[i], 3);
        for (j = 0; j < 15; j++) luma[j * 256 + indices[i]] = shaded_indices[i];
        checker_pixels[i] = 0xff000000u | indices[i];
        keyed_pixels[i] = i & 1 ? 0x000000ffu : 0xff000000u | indices[0];
    }
    memcpy(palette[52], fallback, 3); /* material ramp48 + shade4 */
    palette[0x60][0] = 7; palette[0x60][1] = 11; palette[0x60][2] = 19;
    palette[0xd0][0] = 12; palette[0xd0][1] = 34; palette[0xd0][2] = 56;
    checker.w = checker.h = keyed.w = keyed.h = 2;
    checker.rgba = checker_pixels; keyed.rgba = keyed_pixels;
}

static void upload(glr *r, texture *texture, int billboard, int object_class, int actor, int uv_shift)
{
    static const int corner_x[4] = {-1000,1000,1000,-1000};
    static const int corner_y[4] = {-1000,-1000,1000,1000};
    wtb_vertex vertices[4] = {{0}};
    wtb_poly polygon = {0};
    wtb_object object = {0};
    mech3d_part part = {0};
    mech3d model = {0}, empty = {0};
    int i;
    memset(slots, 0, sizeof slots); memset(bank0, 0, sizeof bank0);
    if (billboard) {
        vertices[0].y = 1000; vertices[0].z = 1000; /* top: u0,v0 */
        vertices[1].y = -1000; vertices[1].z = 1000; vertices[1].b = 1; /* foot */
        vertices[2].x = 1000; vertices[2].y = 1000; vertices[2].z = 1000; vertices[2].a = 1;
        polygon.n = 3; polygon.color = 0x3100; bank0[16] = texture;
    } else {
        for (i = 0; i < 4; i++) {
            vertices[i].x = corner_x[i]; vertices[i].y = corner_y[i]; vertices[i].z = 1000;
            vertices[i].a = (uint16_t)((i == 1 || i == 2 ? 2 : 0) + uv_shift);
            vertices[i].b = (uint16_t)(i < 2 ? 2 : 0);
        }
        polygon.n = 4; polygon.color = 0x5310; slots[16] = texture;
    }
    for (i = 0; i < polygon.n; i++) polygon.idx[i] = (uint16_t)i;
    object.verts = vertices; object.vert_count = polygon.n; object.polys = &polygon; object.poly_count = 1;
    part.model.objects = &object; part.model.object_count = 1; part.objtype = object_class;
    part.has_rot = 1; part.rot[0] = part.rot[4] = part.rot[8] = 1;
    model.parts = &part; model.part_count = 1;
    glr_set_bank0(r, bank0);
    check(glr_set_mech(r, actor ? &empty : &model, palette, slots, 0, 0) >= 0, "upload world fixture");
    check(glr_set_actors(r, actor ? &model : &empty, palette, slots, 0, 0) >= 0, "upload actor fixture");
}

static void draw(glr *r, glr_view *v, uint8_t *out)
{
    gpu_test_draw(r, v, WIDTH, HEIGHT);
    gpu_test_read_rgba(0, 0, WIDTH, HEIGHT, out);
    check(gpu_test_ok(), "GPU draw and readback");
}

static int near_rgb(const uint8_t *pixel, const uint8_t *rgb, int tolerance)
{
    int i;
    for (i = 0; i < 3; i++) if (abs((int)pixel[i] - rgb[i]) > tolerance) return 0;
    return pixel[3] == 255;
}

static const uint8_t *at(const uint8_t *pixels, int x, int y)
{
    return pixels + (y * WIDTH + x) * 4;
}

static void filter_checks(glr *r, glr_view *v, int billboard)
{
    const uint8_t (*colours)[3] = billboard ? raw_colours : shaded_colours;
    uint8_t middle[3];
    const float t = 63.0f / 128.0f; /* pixel(63,64): four weights around the texture centre */
    int i, c, changed = 0, original_bad = 0;
    upload(r, &checker, billboard, 0x100, 0, 0);
    v->dos_texture_linear = 0; v->tex_min = v->tex_mag = 0;
    draw(r, v, original);
    check(near_rgb(at(original, 16, 112), colours[0], 0) &&
          near_rgb(at(original, 112, 112), colours[1], 0) &&
          near_rgb(at(original, 16, 16), colours[2], 0) &&
          near_rgb(at(original, 112, 16), colours[3], 0),
          billboard ? "billboard original palette colours" : "surface original LUMA and palette colours");
    for (i = 0; i < WIDTH * HEIGHT; i++) {
        int texel = (i % WIDTH >= WIDTH / 2) + (i / WIDTH < HEIGHT / 2 ? 2 : 0);
        if (!near_rgb(original + i * 4, colours[texel], 0)) original_bad++;
    }
    check(!original_bad, "every original pixel matches the exact nearest-sampled colour fixture");
    v->tex_min = v->tex_mag = RB_NEAREST;
    draw(r, v, nearest);
    check(!memcmp(original, nearest, BYTES), "explicit nearest is bit-identical to original");
    v->dos_texture_linear = 1;
    draw(r, v, smooth);
    for (c = 0; c < 3; c++) middle[c] = (uint8_t)lroundf(
        (1 - t) * (1 - t) * colours[0][c] + t * (1 - t) * colours[1][c] +
        (1 - t) * t * colours[2][c] + t * t * colours[3][c]);
    if (!near_rgb(at(smooth, 63, 64), middle, 1)) {
        const uint8_t *p = at(smooth, 63, 64);
        fprintf(stderr, "%s centre got %u,%u,%u wanted %u,%u,%u\n", billboard ? "billboard" : "surface",
                p[0],p[1],p[2],middle[0],middle[1],middle[2]);
    }
    check(near_rgb(at(smooth, 63, 64), middle, 1),
          billboard ? "billboard smooth blends resolved RGB, never indices" : "surface smooth blends after per-tap LUMA lookup");
    for (i = 0; i < WIDTH * HEIGHT; i++) if (memcmp(original + i * 4, smooth + i * 4, 4)) changed++;
    check(changed > WIDTH * HEIGHT / 2, "linear filter changes the rendered image");
    v->dos_texture_linear = 0;
    draw(r, v, restored);
    check(!memcmp(original, restored, BYTES), "restoring original restores every pixel");
    if (!billboard) {
        upload(r, &checker, 0, 0x100, 0, 2); v->dos_texture_linear = 1;
        draw(r, v, restored);
        check(!memcmp(smooth, restored, BYTES), "linear surface sampling retains RB_REPEAT across a full UV period");
    }
    upload(r, &keyed, billboard, 0x100, 0, 0); v->dos_texture_linear = 1;
    draw(r, v, smooth);
    check(near_rgb(at(smooth, 48, 96), colours[0], 1), "transparent texels do not tint visible filtered edges");
    check(!memcmp(at(smooth, 96, 96), at(background, 96, 96), 4), "filtered keyed holes remain transparent");
    check(at(smooth, 48, 96)[3] == 255, "visible DOS cutout stays opaque");
    v->dos_texture_linear = 0;
}

static void toggle_checks(glr *r, glr_view *v)
{
    static const int classes[4] = {0x100,0x800,0,0};
    int i, object, terrain, smooth_mode;
    for (i = 0; i < 4; i++) {
        upload(r, &checker, 0, classes[i], i == 3, 0);
        for (smooth_mode = 0; smooth_mode < 2; smooth_mode++) {
            v->dos_texture_linear = smooth_mode; v->notex_actors = v->notex_world = 0;
            draw(r, v, original);
            for (object = 0; object < 2; object++) for (terrain = 0; terrain < 2; terrain++) {
                int off = ((i == 0 || i == 3) && object) || (i == 1 && terrain);
                v->notex_actors = object; v->notex_world = terrain;
                draw(r, v, restored);
                check(off ? near_rgb(at(restored, 64, 64), fallback, 0) : !memcmp(original, restored, BYTES),
                      "each DOS texture switch affects only its matching object/terrain class");
                if (off) check(near_rgb(at(restored, 16, 16), fallback, 0) &&
                               near_rgb(at(restored, 112, 112), fallback, 0),
                               "texture-off retains full geometry in its authored palette shade");
            }
        }
    }
    v->notex_actors = v->notex_world = v->dos_texture_linear = 0;
}

static void ground_checks(glr *r, glr_view *v)
{
    mech3d empty = {0};
    mercs_ground ground = {0};
    uint8_t average[3];
    uint32_t ground_pixels[4];
    texture zero_alpha = checker;
    int c;
    glr_set_mech(r, &empty, palette, slots, 0, 0); glr_set_actors(r, &empty, palette, slots, 0, 0);
    ground.mode = 1; ground.size = 3200; ground.delta = 40;
    ground.repeats[0] = ground.repeats[1] = 1;
    ground.clip[0] = 2500; ground.clip[1] = 20000; ground.clip[2] = 50000;
    v->eye[0] = v->eye[2] = 1600; v->eye[1] = 1000; v->look_pitch = -90;
    glr_set_mercs_ground(r, &checker, &ground);
    draw(r, v, original);
    v->dos_texture_linear = 1;
    draw(r, v, smooth);
    for (c = 0; c < 3; c++) average[c] = (uint8_t)lroundf((shaded_colours[0][c] + shaded_colours[1][c] +
                                                         shaded_colours[2][c] + shaded_colours[3][c]) / 4.0f);
    check(near_rgb(at(smooth, 63, 64), average, 3), "ground filter resolves all four LUMA-shaded palette colours");
    check(memcmp(original, smooth, BYTES) != 0, "ground filter visibly changes actual ground rendering");
    for (c = 0; c < 4; c++) ground_pixels[c] = checker_pixels[c] & 0x00ffffffu;
    zero_alpha.rgba = ground_pixels;
    glr_set_mercs_ground(r, &zero_alpha, &ground);
    draw(r, v, restored);
    check(!memcmp(smooth, restored, BYTES), "filtered DOS ground retains its original alpha-ignored behavior");
    glr_set_mercs_ground(r, &checker, &ground);
    v->dos_texture_linear = 0;
    draw(r, v, restored);
    check(!memcmp(original, restored, BYTES), "ground original restored without sticky filter state");
    v->notex_actors = 1;
    draw(r, v, restored);
    check(!memcmp(original, restored, BYTES), "object texture switch does not flatten the ground");
    v->notex_world = 1;
    draw(r, v, restored);
    check(near_rgb(at(restored, 64, 64), palette[0xd0], 0), "terrain texture switch retains DOS ground backdrop");
    glr_set_mercs_ground(r, NULL, NULL);
}

static void clock_visibility_checks(glr *r, glr_view *v)
{
    upload(r, &checker, 0, 0x100, 0, 0);
    draw(r, v, original);
    glr_slot_visible(r, 16, 1, 0);
    draw(r, v, restored);
    check(!memcmp(original, restored, BYTES), "other edition's stopped slot does not hide primary surfaces");
    glr_slot_visible(r, 16, 0, 0);
    draw(r, v, restored);
    check(!memcmp(background, restored, BYTES), "stopped texture slot hides its polygon instead of showing frozen frame zero");
    glr_slot_visible(r, 16, 0, 1);
    draw(r, v, restored);
    check(!memcmp(original, restored, BYTES), "restarting a texture slot restores every visible pixel");
    glr_slot_frame(r, 16, &keyed);
    draw(r, v, restored);
    check(memcmp(original, restored, BYTES) != 0, "shared-clock frame selection reaches uploaded surface pixels");
    glr_slot_frame(r, 16, &checker);
    draw(r, v, restored);
    check(!memcmp(original, restored, BYTES), "restoring the texture frame restores every pixel");
    glr_slot_visible(r, 16, 1, 1);
}

int main(void)
{
    SDL_Window *window;
    int context;
    glr *renderer;
    glr_view view;
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) return 2;
    window = SDL_CreateWindow("DOS palette-aware textures", WIDTH, HEIGHT, SDL_WINDOW_HIDDEN);
    context = gpu_test_start();
    if (!context) { fprintf(stderr, "%s\n", SDL_GetError()); SDL_Quit(); return 2; }
#if defined(_WIN32)
    if (mw2gl_load()) return 2;
#endif
    renderer = glr_create(); if (!renderer) return 2;
    setup_palette(); glr_set_dos(renderer, palette, luma);
    glr_default_view(&view); view.free_cam = 1; view.vfov = 90; view.dos_amb = 40;
    view.dos_lpos[0] = view.dos_lpos[1] = view.dos_lpos[2] = 999999;
    draw(renderer, &view, background);
    filter_checks(renderer, &view, 0); filter_checks(renderer, &view, 1);
    toggle_checks(renderer, &view); clock_visibility_checks(renderer, &view); ground_checks(renderer, &view);
    glr_destroy(renderer); rb_quit(); SDL_DestroyWindow(window); SDL_Quit();
    printf("DOS texture filtering and switches: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
