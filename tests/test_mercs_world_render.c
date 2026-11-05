#include "sdl3_test.h"
/* Actual GPU regression for Mercs accelerated world fog.
 *
 * The references below are hand-computed meshes already clipped at z=60 cm.
 * No clipping code is copied from the renderer, and no game assets are used.
 * Rendering the original triangle must match rendering that reference: the
 * original engine clips first, then evaluates fog on the resulting vertices.
 * Evaluating fog before hardware clipping gives different colours, especially
 * when a vertex lies behind the eye.  A white texture and black fog isolate
 * this error from lighting, texture sampling, and the backdrop.
 */
#define SDL_MAIN_HANDLED
#include "gpu_test.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glr.h"

enum { MAX_W = 256, MAX_H = 192 };

typedef struct { int x, y, z; } point;
typedef struct {
    const char *name;
    point original[3];
    point clipped[4];
    int clipped_count;
    int crosses;
} clip_fixture;

static const clip_fixture clips[] = {
    /* At (-60 -> 180), z=60 is halfway along either crossing edge. */
    {"one behind eye",
     {{0,120,-60}, {-120,-120,180}, {120,-120,180}},
     {{-60,0,60}, {-120,-120,180}, {120,-120,180}, {60,0,60}}, 4, 1},
    /* The same exact intersections, with side-plane clipping too. This
     * catches a GPU noperspective clipping failure that the small fixtures
     * do not expose; explicit fog*w transport must survive those planes. */
    {"near and side planes",
     {{0,480,-60}, {-480,-480,180}, {480,-480,180}},
     {{-240,0,60}, {-480,-480,180}, {480,-480,180}, {240,0,60}}, 4, 1},
    {"two behind eye",
     {{-80,-180,-60}, {80,-180,-60}, {0,120,180}},
     {{-40,-30,60}, {40,-30,60}, {0,120,180}}, 3, 1},
    /* At (0 -> 180), z=60 is one third along the crossing edges. */
    {"one on eye plane",
     {{0,120,0}, {-180,-120,180}, {180,-120,180}},
     {{-60,40,60}, {-180,-120,180}, {180,-120,180}, {60,40,60}}, 4, 1},
    /* At (30 -> 150), z=60 is one quarter along the crossing edges. */
    {"one between eye and near",
     {{0,90,30}, {-120,-90,150}, {120,-90,150}},
     {{-30,45,60}, {-120,-90,150}, {120,-90,150}, {30,45,60}}, 4, 1},
    {"none behind near",
     {{-50,-50,100}, {50,-50,100}, {0,50,200}},
     {{-50,-50,100}, {50,-50,100}, {0,50,200}}, 3, 0},
    {"all behind near",
     {{-50,-50,-60}, {50,-50,0}, {0,50,30}},
     {{0,0,0}}, 0, 0},
    {"one exactly on near",
     {{0,45,60}, {-90,-75,180}, {90,-75,180}},
     {{0,45,60}, {-90,-75,180}, {90,-75,180}}, 3, 0},
    {"all exactly on near",
     {{-40,-30,60}, {40,-30,60}, {0,40,60}},
     {{-40,-30,60}, {40,-30,60}, {0,40,60}}, 3, 0},
    {"near vertex and behind vertex",
     {{-30,30,60}, {0,-90,-60}, {90,-30,180}},
     {{-30,30,60}, {45,-60,60}, {90,-30,180}}, 3, 1},
    {"two near vertices and behind vertex",
     {{-40,-30,60}, {40,-30,60}, {0,40,-60}},
     {{0,0,0}}, 0, 0}
};

static unsigned char palette[256][3];
static uint32_t white_pixels[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
static texture white_texture;
static texture *slots[GLR_SLOTS];
static unsigned char actual[MAX_W * MAX_H * 4];
static unsigned char expected[MAX_W * MAX_H * 4];

static int render(glr *renderer, glr_view *view, const point *points, int count,
                  int width, int height, unsigned char *pixels)
{
    wtb_vertex vertices[4];
    wtb_poly polygon;
    wtb_object object;
    mech3d_part part;
    mech3d model;
    int i;
    memset(vertices, 0, sizeof vertices);
    memset(&polygon, 0, sizeof polygon);
    memset(&object, 0, sizeof object);
    memset(&part, 0, sizeof part);
    memset(&model, 0, sizeof model);
    for (i = 0; i < count; i++) {
        vertices[i].x = points[i].x;
        vertices[i].y = points[i].y;
        vertices[i].z = points[i].z;
        polygon.idx[i] = (uint16_t)i;
    }
    polygon.n = (uint16_t)count;
    polygon.color = 0x5010; /* opaque constant white, Titanium slot16 */
    object.verts = vertices;
    object.vert_count = count;
    object.polys = &polygon;
    object.poly_count = count ? 1 : 0;
    part.model.objects = &object;
    part.model.object_count = 1;
    part.ti = 1;
    part.objtype = 0x800; /* terrain, on the world layer */
    /* An explicit identity avoids the renderer's special y=0 ground lift,
     * so the height fog fixtures exercise their authored height exactly. */
    part.has_rot = 1;
    part.rot[0] = part.rot[4] = part.rot[8] = 1;
    model.parts = &part;
    model.part_count = 1;
    if (glr_set_mech(renderer, &model, palette, slots, 0, 0) < 0) return -1;
    gpu_test_draw(renderer, view, width, height);
    gpu_test_read_rgba(0, 0, width, height, pixels);
    return gpu_test_ok() ? 0 : -1;
}

static int is_clear(const unsigned char *pixel)
{
    return pixel[0] == 17 && pixel[1] == 23 && pixel[2] == 31;
}

static int compare(const clip_fixture *fixture, int fog_mode, int fog_on,
                   int width, int height)
{
    /* Unaffected geometry and fog-off rendering must be exact. A two-level
     * tolerance for clipped fog allows the two triangle fans' interpolation
     * and framebuffer rounding, never missing geometry or a dark wedge. */
    const int tolerance = fixture->crosses && fog_on ? 2 : 0;
    int pixel, channel, bad = 0, coverage_bad = 0, max_delta = 0, visible = 0;
    for (pixel = 0; pixel < width * height; pixel++) {
        const unsigned char *a = actual + pixel * 4;
        const unsigned char *e = expected + pixel * 4;
        int pixel_bad = 0;
        if (!is_clear(e)) visible++;
        if (is_clear(a) != is_clear(e)) coverage_bad++;
        for (channel = 0; channel < 3; channel++) {
            int delta = abs((int)a[channel] - (int)e[channel]);
            if (delta > max_delta) max_delta = delta;
            if (delta > tolerance) pixel_bad = 1;
        }
        if (pixel_bad || a[3] != e[3]) bad++;
    }
    if (bad || coverage_bad || (fixture->clipped_count && visible < 100) ||
        (!fixture->clipped_count && visible)) {
        fprintf(stderr, "%s fog=%s %dx%d %s: mismatched=%d coverage=%d max_delta=%d reference_pixels=%d\n",
                fog_mode == 2 ? "dedicated 3dfx" : "Titanium",
                fog_on ? "on" : "off", width, height, fixture->name,
                bad, coverage_bad, max_delta, visible);
        return 0;
    }
    return 1;
}

static void fog_goldens(glr *renderer, glr_view *view, int *passed, int *failed)
{
    /* Dedicated values are actual original-binary results: effective Z=1000
     * and depths 1000/2000/4000 cm yield visibility bytes 255/127/63 (the
     * original truncates to a byte). Titanium's linear equation with
     * effective Z=4000 gives 191.25/127.5/0 before integer conversion.
     * These test the renderer's centimetre units and curve; the SKYGND
     * parameter loader is a separate adapter and is not exercised here.
     * See tests/fixtures/world-renderer/verify_original_world_clip_fog.py.
     * GPU framebuffer conversion may round the equivalent fraction upward
     * by one byte; the tolerance does not permit a different fog curve. */
    static const int depths[] = {1000, 2000, 4000};
    static const int values[2][3] = {{191, 127, 0}, {255, 127, 63}};
    int mode, i;
    for (mode = 1; mode <= 2; mode++) {
        view->ti_fog_vtx = mode;
        view->ti_fog[0] = 1;
        view->ti_fog[1] = mode == 2 ? 1000.0f : 4000.0f;
        view->ti_fog[2] = 1;
        for (i = 0; i < 3; i++) {
            const int z = depths[i], half = z / 2;
            const point plane[4] = {{-half,-half,z}, {half,-half,z}, {half,half,z}, {-half,half,z}};
            const unsigned char *pixel;
            int want = values[mode - 1][i];
            if (render(renderer, view, plane, 4, 128, 128, actual) < 0) {
                fprintf(stderr, "fog golden render failed\n"); (*failed)++; continue;
            }
            pixel = actual + (64 * 128 + 64) * 4;
            if (abs((int)pixel[0] - want) > 1 ||
                abs((int)pixel[1] - want) > 1 ||
                abs((int)pixel[2] - want) > 1 || pixel[3] != 255) {
                fprintf(stderr, "%s depth%d cm: got%u,%u,%u expected%d +/-1\n",
                        mode == 2 ? "dedicated 3dfx" : "Titanium", z,
                        pixel[0], pixel[1], pixel[2], want);
                (*failed)++;
            } else (*passed)++;
        }
    }
}

static void translated_clip(glr *renderer, glr_view *view, int *passed, int *failed)
{
    /* Translate the preceding side-plane fixture and camera together.
     * Coordinates and edge midpoints are literal, independent of the
     * renderer's clipping code. The visible geometry is unchanged. */
    static const clip_fixture fixture = {"near and side planes, translated camera",
        {{-60000,2480,164940}, {-60480,1520,165180}, {-59520,1520,165180}},
        {{-60240,2000,165060}, {-60480,1520,165180},
         {-59520,1520,165180}, {-59760,2000,165060}}, 4, 1};
    static const int dimensions[2][2] = {{128,128}, {256,192}};
    glr_view saved = *view;
    int mode, on, size;
    view->eye[0] = -60000;
    view->eye[1] = 2000;
    view->eye[2] = 165000;
    for (mode = 1; mode <= 2; mode++) {
        view->ti_fog_vtx = mode;
        view->ti_fog[1] = mode == 2 ? 120.0f : 240.0f;
        view->ti_fog[2] = 1;
        for (on = 0; on <= 1; on++) {
            view->ti_fog[0] = (float)on;
            for (size = 0; size < 2; size++) {
                int width = dimensions[size][0], height = dimensions[size][1];
                if (render(renderer, view, fixture.original, 3, width, height, actual) < 0 ||
                    render(renderer, view, fixture.clipped, fixture.clipped_count, width, height, expected) < 0) {
                    fprintf(stderr, "translated clipping: GPU readback error\n"); (*failed)++; continue;
                }
                if (compare(&fixture, mode, on, width, height)) (*passed)++; else (*failed)++;
            }
        }
    }
    *view = saved;
}

static void height_goldens(glr *renderer, glr_view *view, int *passed, int *failed)
{
    /* Exact original dedicated output bytes, including the original
     * minimum >1 branch and low-byte wrap (not saturation). Executed by
     * verify_original_world_clip_fog.py, dedicated_height_byte_cases in
     * tests/fixtures/world-renderer/original_clip_fog_results.json.
     * Z=0 disables the distance term; every corner of a horizontal plane
     * has the same world height, isolating H(worldY)*cameraH. */
    static const struct {
        float density_y, minimum, camera_factor;
        int world_y, byte;
    } cases[] = {
        {2000, 0, 1, 1000, 255},
        {2000, 0, 1, 3000, 170},
        {2000, 0, 1, 10000, 51},
        {2000, 0.25f, 1, 10000, 63},
        {2000, 1.5f, 1, 3000, 126},
        {2000, 2.0f, 1, 3000, 254},
        {2000, 1.5f, 1.5f, 3000, 61},
        {-0.005f, 0.1f, 1, 0, 25},
        {-0.005f, 0.1f, 1, 50, 63},
        {-0.005f, 1.5f, 1, 0, 126}
    };
    glr_view saved = *view;
    size_t i;
    view->ti_fog_vtx = 2;
    view->ti_fog[0] = 1;
    view->ti_fog[1] = 0;
    view->look_pitch = -45;
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const int y = cases[i].world_y;
        const point plane[4] = {{-1000,y,0}, {1000,y,0}, {1000,y,2000}, {-1000,y,2000}};
        const unsigned char *pixel;
        view->eye[1] = (float)(y + 1000);
        view->ti_fog_height[0] = cases[i].density_y;
        view->ti_fog_height[1] = cases[i].minimum;
        view->ti_fog[2] = cases[i].camera_factor;
        if (render(renderer, view, plane, 4, 128, 128, actual) < 0) {
            fprintf(stderr, "height golden%zu: GPU readback error\n", i); (*failed)++; continue;
        }
        pixel = actual + (64 * 128 + 64) * 4;
        if (abs((int)pixel[0] - cases[i].byte) > 1 ||
            abs((int)pixel[1] - cases[i].byte) > 1 ||
            abs((int)pixel[2] - cases[i].byte) > 1 || pixel[3] != 255) {
            fprintf(stderr, "height golden%zu Y%g M%g worldY%d cameraH%g: got%u,%u,%u expected%d +/-1\n",
                    i, (double)cases[i].density_y, (double)cases[i].minimum,
                    y, (double)cases[i].camera_factor,
                    pixel[0], pixel[1], pixel[2], cases[i].byte);
            (*failed)++;
        } else (*passed)++;
    }
    *view = saved;
}

int main(void)
{
    static const int dimensions[2][2] = {{128,128}, {256,192}};
    SDL_Window *window;
    int context;
    glr *renderer;
    glr_view view;
    int mode, on, size, i, passed = 0, failed = 0;
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    window = SDL_CreateWindow("Mercs world fog fixture", MAX_W, MAX_H, SDL_WINDOW_HIDDEN);
    context = gpu_test_start();
    if (!context) {
        fprintf(stderr, "GPU device: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    renderer = glr_create();
    if (!renderer) { rb_quit(); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    for (i = 0; i < 256; i++) palette[i][0] = palette[i][1] = palette[i][2] = (unsigned char)i;
    white_texture.w = white_texture.h = 2;
    white_texture.rgba = white_pixels;
    slots[16] = &white_texture;
    glr_set_titanium(renderer, slots, NULL);
    glr_default_view(&view);
    view.free_cam = 1;
    view.vfov = 90;
    view.ambient = 1;
    view.bilinear = 0;
    view.sky_color[0] = 17.0f / 255;
    view.sky_color[1] = 23.0f / 255;
    view.sky_color[2] = 31.0f / 255;
    view.fog_color[0] = view.fog_color[1] = view.fog_color[2] = 0;
    for (mode = 1; mode <= 2; mode++) {
        view.ti_fog_vtx = mode;
        view.ti_fog[1] = mode == 2 ? 120.0f : 240.0f;
        view.ti_fog[2] = 1;
        for (on = 0; on <= 1; on++) {
            view.ti_fog[0] = (float)on;
            for (size = 0; size < 2; size++) {
                const int width = dimensions[size][0], height = dimensions[size][1];
                for (i = 0; i < (int)(sizeof clips / sizeof clips[0]); i++) {
                    const clip_fixture *fixture = &clips[i];
                    if (render(renderer, &view, fixture->original, 3, width, height, actual) < 0 ||
                        render(renderer, &view, fixture->clipped, fixture->clipped_count, width, height, expected) < 0) {
                        fprintf(stderr, "%s: GPU readback error\n", fixture->name); failed++; continue;
                    }
                    if (compare(fixture, mode, on, width, height)) passed++; else failed++;
                }
            }
        }
    }
    fog_goldens(renderer, &view, &passed, &failed);
    translated_clip(renderer, &view, &passed, &failed);
    height_goldens(renderer, &view, &passed, &failed);
    printf("Mercs world renderer: %d passed, %d failed\n", passed, failed);
    glr_destroy(renderer);
    rb_quit();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return failed ? 1 : 0;
}
