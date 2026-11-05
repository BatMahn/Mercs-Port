#include "sdl3_test.h"
/* Real GPU depth competition between the Mercs cockpit, body and world.
 * The oracle is physical distance, independent of projection coefficients.
 * No game assets are embedded. */
#define SDL_MAIN_HANDLED
#include "gpu_test.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glr.h"

static unsigned char palette[256][3];
static texture textures[3], *slots[GLR_SLOTS];
static uint32_t texels[3][4] = {
    {0xff0000ffu, 0xff0000ffu, 0xff0000ffu, 0xff0000ffu},
    {0xff00ff00u, 0xff00ff00u, 0xff00ff00u, 0xff00ff00u},
    {0xffff0000u, 0xffff0000u, 0xffff0000u, 0xffff0000u}
};
static int passed, failed;

typedef struct {
    wtb_vertex vertices[4];
    wtb_poly polygon;
    wtb_object object;
    mech3d_part part;
} plane;

static void make_plane(plane *p, int depth, int cockpit, int colour)
{
    static const int xy[4][2] = {{-1,-1}, {1,-1}, {1,1}, {-1,1}};
    int k;
    memset(p, 0, sizeof *p);
    for (k = 0; k < 4; k++) {
        p->vertices[k].x = xy[k][0] * 500;
        p->vertices[k].y = xy[k][1] * 500;
        p->vertices[k].z = depth;
        p->polygon.idx[k] = (uint16_t)k;
    }
    p->polygon.n = 4; p->polygon.color = (uint16_t)(0x5010 + colour);
    p->object.verts = p->vertices; p->object.vert_count = 4;
    p->object.polys = &p->polygon; p->object.poly_count = 1;
    p->part.model.objects = &p->object; p->part.model.object_count = 1;
    p->part.ti = 1; p->part.dos_pit = cockpit;
    p->part.objtype = cockpit ? 0x1a0 : 0x100;
    p->part.has_rot = 1;
    p->part.rot[0] = p->part.rot[4] = p->part.rot[8] = 1;
}

static int read_colour(int width, int height, unsigned char out[4])
{
    gpu_test_read_rgba(width / 2, height / 2, 1, 1, out);
    return gpu_test_ok();
}

static void depth_cases(glr *renderer, glr_view *view)
{
    static const struct { int pit, body, world, colour; } cases[] = {
        {500,600,5000,0}, {500,1000,5000,0}, {500,0,1000,0},
        {700,500,5000,1}, {500,0,300,2}, {20,100,5000,0},
        {9,61,5000,0}, {7,100,5000,1}, {100,20,5000,0},
        {100,0,20,0}, {20,0,61,0}, {500,600,700,0}
    };
    static const int sizes[2][2] = {{128,128}, {256,192}};
    size_t i;
    int size, translated, order;
    for (size = 0; size < 2; size++)
    for (translated = 0; translated < 2; translated++)
    for (order = 0; order < 2; order++)
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        plane planes[3];
        mech3d_part actor_parts[2];
        mech3d actors = {0}, world = {0};
        unsigned char out[4];
        int p, k, w = sizes[size][0], h = sizes[size][1];
        make_plane(&planes[0], cases[i].pit, 1, 0);
        make_plane(&planes[1], cases[i].body, 0, 1);
        make_plane(&planes[2], cases[i].world, 0, 2);
        planes[2].part.objtype = 0x800;
        view->eye[0] = translated ? -60000.0f : 0;
        view->eye[1] = translated ? 2000.0f : 0;
        view->eye[2] = translated ? 165000.0f : 0;
        for (p = 0; p < 3; p++)
            for (k = 0; k < 3; k++) planes[p].part.pos[k] = (int32_t)view->eye[k];
        actor_parts[0] = planes[0].part;
        actor_parts[1] = planes[1].part;
        if (order && cases[i].body) {
            actor_parts[0] = planes[1].part; actor_parts[1] = planes[0].part;
        }
        actors.parts = actor_parts; actors.part_count = cases[i].body ? 2 : 1;
        world.parts = &planes[2].part; world.part_count = 1;
        if (glr_set_mech(renderer, &world, palette, slots, 0, 0) < 0 ||
            glr_set_actors(renderer, &actors, palette, slots, 0, 0) < 0) { failed++; continue; }
        gpu_test_draw(renderer, view, w, h);
        if (!read_colour(w, h, out) || out[cases[i].colour] != 255 ||
            out[(cases[i].colour + 1) % 3] || out[(cases[i].colour + 2) % 3]) {
            fprintf(stderr, "depth case%zu %dx%d translated%d order%d: got%u,%u,%u expected channel%d\n",
                    i, w, h, translated, order, out[0], out[1], out[2], cases[i].colour);
            failed++;
        } else passed++;
    }
    view->eye[0] = view->eye[1] = view->eye[2] = 0;
}

static void particles_and_shadow(glr *renderer, glr_view *view)
{
    plane pit, ground, shadow;
    mech3d actors = {0}, world = {0};
    mech3d_part parts[2];
    glr_particle particle = {{0,0,0}, 480, &textures[1]};
    unsigned char out[4], with_shadow[4], without_shadow[4];
    int i;
    make_plane(&pit, 500, 1, 0); make_plane(&ground, 1000, 0, 2);
    actors.parts = &pit.part; actors.part_count = 1;
    world.parts = &ground.part; world.part_count = 1;
    glr_set_mech(renderer, &world, palette, slots, 0, 0);
    glr_set_actors(renderer, &actors, palette, slots, 0, 0);
    for (i = 0; i < 2; i++) {
        /* Particle quads are projected on the CPU, then sent through an
         * identity transform. They must compete in the same depth units. */
        particle.pos[2] = i ? 300.0f : 600.0f;
        particle.pos[0] = -particle.pos[2] * 0.1f;
        particle.pos[1] = particle.pos[2] * 0.1f;
        glr_set_particles(renderer, &particle, 1, 1);
        gpu_test_draw(renderer, view, 128, 128);
        if (!read_colour(128, 128, out) || out[i] != 255 || out[1 - i] != 0 || out[2]) {
            fprintf(stderr, "particle %s cockpit: got%u,%u,%u\n", i ? "before" : "behind", out[0], out[1], out[2]);
            failed++;
        } else passed++;
    }
    glr_set_particles(renderer, NULL, 0, 1);
    /* Exercise the actual static-shadow batch, then turn it off on the
     * same uploaded geometry. Clearing actors also removes cockpit state. */
    make_plane(&ground, 1000, 0, 2); make_plane(&shadow, 999, 0, 2);
    shadow.part.shadow = 1; parts[0] = ground.part; parts[1] = shadow.part;
    world.parts = parts; world.part_count = 2; actors.part_count = 0;
    glr_set_actors(renderer, &actors, palette, slots, 0, 0);
    glr_set_mech(renderer, &world, palette, slots, 0, 0);
    view->no_shadows = 0; gpu_test_draw(renderer, view, 128, 128);
    read_colour(128, 128, with_shadow);
    view->no_shadows = 1; gpu_test_draw(renderer, view, 128, 128);
    if (!read_colour(128, 128, without_shadow) || with_shadow[2] < 130 || with_shadow[2] > 150 ||
        without_shadow[2] != 255 || without_shadow[0] || without_shadow[1]) {
        fprintf(stderr, "shadow toggle: blue %u -> %u, expected ~140 ->255\n", with_shadow[2], without_shadow[2]);
        failed++;
    } else passed++;
    view->no_shadows = 0;
}

static void dos_joined_faces(glr *renderer, glr_view *view)
{
    /* Two authored faces meet along x50/z75. The right face turns back
     * through the8cm near plane. Their common edge is not an opening.
     * The original project-to-pixel routine0x4d13f supplies integer points
     * to the span rasterizer; mixing those spans with floating GL coverage
     * left visible cracks, especially after model rotation/translation. */
    static const int points[6][3] = {
        {-20,-60,75}, {50,-60,75}, {50,-20,75}, {-20,-20,75},
        {160,-60,-15}, {160,-20,-15}
    };
    static const int indices[2][4] = {{0,1,2,3}, {1,4,5,2}};
    static const int sizes[2][2] = {{640,480}, {2048,1330}};
    static const float headings[4] = {0,45,130,270};
    unsigned char *pixels = malloc(2048u * 1330u * 4u);
    glr_view saved = *view;
    mech3d empty = {0};
    int size, translated, turn, i;
    if (!pixels) { failed++; return; }
    for (i = 0; i < 256; i++) palette[i][0] = palette[i][1] = palette[i][2] = 200;
    glr_set_titanium(renderer, NULL, NULL);
    glr_set_dos(renderer, palette, NULL);
    view->solo = 1; view->dos_amb = 80;
    view->dos_lpos[0] = view->dos_lpos[1] = view->dos_lpos[2] = 999999;
    glr_set_mech(renderer, &empty, palette, slots, 0, 0);
    for (size = 0; size < 2; size++)
    for (translated = 0; translated < 2; translated++)
    for (turn = 0; turn < 4; turn++) {
        wtb_vertex vertices[6] = {{0}};
        wtb_poly polygons[2] = {{0}};
        wtb_object object = {0};
        mech3d_part part = {0};
        mech3d model = {0};
        int k, x, y, holes = 0, w = sizes[size][0], h = sizes[size][1];
        float angle = headings[turn] * 0.017453292519943295f;
        float focal = (float)h * 0.5f / tanf(view->vfov * 0.008726646259971648f);
        int join_x = w / 2 + (int)lroundf(50.0f * focal / 75.0f);
        int bottom = h / 2 - (int)lroundf(60.0f * focal / 75.0f) + 10;
        int top = h / 2 - (int)lroundf(20.0f * focal / 75.0f) - 10;
        if (bottom < 10) bottom = 10;
        for (k = 0; k < 6; k++) {
            vertices[k].x = points[k][0]; vertices[k].y = points[k][1]; vertices[k].z = points[k][2];
            vertices[k].a = 79;
        }
        for (i = 0; i < 2; i++) {
            polygons[i].n = 4; polygons[i].color = 0x44ff;
            for (k = 0; k < 4; k++) polygons[i].idx[k] = (uint16_t)indices[i][k];
        }
        object.verts = vertices; object.vert_count = 6; object.polys = polygons; object.poly_count = 2;
        part.model.objects = &object; part.model.object_count = 1; part.objtype = 0x1a0; part.dos_pit = 1;
        part.has_rot = 1; part.rot[0] = part.rot[8] = cosf(angle); part.rot[4] = 1;
        part.rot[2] = sinf(angle); part.rot[6] = -sinf(angle);
        view->eye[0] = translated ? -60000.0f : 0;
        view->eye[1] = translated ? 1000.0f : 0;
        view->eye[2] = translated ? 165000.0f : 0;
        for (k = 0; k < 3; k++) part.pos[k] = (int32_t)view->eye[k];
        view->look_yaw = headings[turn];
        model.parts = &part; model.part_count = 1;
        glr_set_actors(renderer, &model, palette, slots, 0, 0);
        gpu_test_draw(renderer, view, w, h);
        gpu_test_read_rgba(0, 0, w, h, pixels);
        for (y = bottom; y <= top; y++)
            for (x = join_x - 8; x <= join_x + 8; x++)
                if (!pixels[((size_t)y * (size_t)w + (size_t)x) * 4]) holes++;
        if (!gpu_test_ok() || holes) {
            fprintf(stderr, "DOS shared edge %dx%d translated%d yaw%.0f: %d background pixels through solid join\n",
                    w, h, translated, (double)headings[turn], holes);
            failed++;
        } else passed++;
    }
    *view = saved;
    free(pixels);
}

int main(void)
{
    SDL_Window *window;
    int context;
    glr *renderer;
    glr_view view;
    int i;
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "%s\n", SDL_GetError()); return 2; }
    window = SDL_CreateWindow("Mercs cockpit depth", 2048, 1330, SDL_WINDOW_HIDDEN);
    context = gpu_test_start();
    if (!context) { fprintf(stderr, "%s\n", SDL_GetError()); SDL_Quit(); return 2; }
    renderer = glr_create();
    if (!renderer) { rb_quit(); SDL_DestroyWindow(window); SDL_Quit(); return 2; }
    for (i = 0; i < 3; i++) {
        textures[i].w = textures[i].h = 2; textures[i].rgba = texels[i]; slots[16 + i] = &textures[i];
    }
    glr_set_titanium(renderer, slots, NULL);
    glr_default_view(&view); view.free_cam = 1; view.vfov = 73.74f; view.ambient = 1;
    depth_cases(renderer, &view);
    particles_and_shadow(renderer, &view);
    dos_joined_faces(renderer, &view);
    printf("Mercs cockpit GPU depth: %d passed, %d failed\n", passed, failed);
    glr_destroy(renderer); rb_quit(); SDL_DestroyWindow(window); SDL_Quit();
    return failed ? 1 : 0;
}
