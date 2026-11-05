#include "sdl3_test.h"
/* Real GPU tests for Mercs' opaque palette point/line adapter. No game data. */
#define SDL_MAIN_HANDLED
#include "gpu_test.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "glr.h"

static uint8_t palette[256][3], pixels[128 * 128 * 4];
static const uint8_t colour[3] = {17,231,49};
static int passed, failed;

static void inspect(glr *r, glr_view *v, const char *name, const uint8_t expected[3], int least, int most)
{
    int i, count = 0;
    gpu_test_draw(r, v, 128, 128);
    gpu_test_read_rgba(0, 0, 128, 128, pixels);
    for (i = 0; i < 128 * 128; i++)
        if (!memcmp(pixels + i * 4, expected, 3) && pixels[i * 4 + 3] == 255) count++;
    if (!gpu_test_ok() || count < least || count > most) {
        fprintf(stderr, "%s: %d exact opaque palette pixels, expected %d..%d\n", name, count, least, most);
        failed++;
    } else passed++;
}

static void tests(glr *r)
{
    glr_view v;
    wtb_vertex vertices[4] = {{0}};
    wtb_poly polygon = {0};
    wtb_object object = {0};
    mech3d_part part = {0};
    mech3d cockpit = {0}, empty = {0};
    glr_palette_particle p;
    texture *slots[GLR_SLOTS] = {0};
    float focal;
    int i, line, position;
    static const float depths[] = {300,600,20,-20};
    glr_default_view(&v); v.free_cam = 1; v.vfov = 73.74f;
    v.fog_density = 2; v.ambient = 0; /* Palette primitives ignore both. */
    focal = 64.0f / tanf(v.vfov * 0.008726646259971648f);
    for (i = 0; i < 4; i++) {
        vertices[i].x = i == 0 || i == 3 ? -500 : 500;
        vertices[i].y = i < 2 ? -500 : 500;
        vertices[i].z = 500; polygon.idx[i] = (uint16_t)i;
    }
    polygon.n = 4; polygon.color = 0;
    object.verts = vertices; object.vert_count = 4; object.polys = &polygon; object.poly_count = 1;
    part.model.objects = &object; part.model.object_count = 1; part.dos_pit = 1; part.objtype = 0x1a0;
    part.has_rot = 1; part.rot[0] = part.rot[4] = part.rot[8] = 1;
    cockpit.parts = &part; cockpit.part_count = 1;
    glr_set_mech(r, &empty, palette, slots, 0, 0);
    glr_set_actors(r, &cockpit, palette, slots, 0, 0);
    memcpy(palette[7], colour, 3);
    for (line = 0; line < 2; line++) for (position = 0; position < 4; position++) {
        char name[80];
        memset(&p, 0, sizeof p); p.color = 7; p.line = line;
        p.pos[2] = p.end[2] = depths[position];
        p.pos[1] = p.end[1] = 0.5f * depths[position] / focal;
        p.pos[0] = line ? -50 : 0.5f * depths[position] / focal;
        p.end[0] = 50;
        glr_set_palette_particles(r, &p, 1, palette);
        snprintf(name, sizeof name, "%s depth%.0f against cockpit500", line ? "line" : "point", (double)depths[position]);
        inspect(r, &v, name, colour, position ? 0 : line ? 20 : 1, position ? 0 : line ? 40 : 1);
    }
    /* A line crossing world near60 must be clipped before the shared cockpit
     * near8 depth mapping. It still has a visible segment in front of500. */
    p.line = 1; p.pos[0] = -10; p.pos[2] = 20; p.pos[1] = 0.5f * 20 / focal;
    p.end[0] = 50; p.end[2] = 300; p.end[1] = 0.5f * 300 / focal;
    glr_set_palette_particles(r, &p, 1, palette);
    inspect(r, &v, "near-crossing line", colour, 10, 80);
    p.line = 0; p.pos[2] = 300; p.pos[0] = p.pos[1] = 0.5f * 300 / focal;
    glr_set_palette_particles(r, &p, 1, palette);
    p.pos[2] = -20; memset(palette[7], 0, 3);
    inspect(r, &v, "copied list and palette", colour, 1, 1);
    glr_set_palette_particles(r, NULL, 0, NULL);
    inspect(r, &v, "clear removes previous frame", colour, 0, 0);
    /* DOS uses the currently faded global renderer palette at draw time. */
    glr_set_actors(r, &empty, palette, slots, 0, 0);
    glr_set_dos(r, palette, NULL);
    p.pos[2] = 300; p.pos[0] = p.pos[1] = 0.5f * 300 / focal;
    glr_set_palette_particles(r, &p, 1, palette);
    memcpy(palette[7], colour, 3); glr_set_dos_palette(r, palette);
    inspect(r, &v, "DOS palette changed after list upload", colour, 1, 1);
    palette[7][0] = 129; palette[7][1] = 73; palette[7][2] = 215;
    glr_set_dos_palette(r, palette);
    inspect(r, &v, "DOS palette fade update", palette[7], 1, 1);
    memcpy(palette[7], colour, 3); glr_set_dos_palette(r, palette);
    /* DOS ordinary world surfaces retain projection depth. Its cockpit-only
     * painter ranks are a separate reserved foreground band. Exercise both
     * paths instead of assuming all DOS depth values are painter ranks. */
    part.dos_pit = 0; part.objtype = 0x800;
    glr_set_mech(r, &cockpit, palette, slots, 0, 0);
    for (line = 0; line < 2; line++) for (position = 0; position < 2; position++) {
        char name[80];
        p.line = line; p.pos[2] = p.end[2] = depths[position];
        p.pos[1] = p.end[1] = 0.5f * depths[position] / focal;
        p.pos[0] = line ? -50 : 0.5f * depths[position] / focal; p.end[0] = 50;
        glr_set_palette_particles(r, &p, 1, palette);
        snprintf(name, sizeof name, "DOS %s %s opaque world", line ? "line" : "point", position ? "behind" : "before");
        inspect(r, &v, name, colour, position ? 0 : line ? 20 : 1, position ? 0 : line ? 40 : 1);
    }
    part.dos_pit = 1; part.objtype = 0x1a0;
    glr_set_mech(r, &empty, palette, slots, 0, 0);
    glr_set_actors(r, &cockpit, palette, slots, 0, 0);
    for (line = 0; line < 2; line++) for (position = 0; position < 2; position++) {
        p.line = line; p.pos[2] = p.end[2] = position ? 60.1f : 300;
        p.pos[1] = p.end[1] = 0.5f * p.pos[2] / focal;
        p.pos[0] = line ? -10 : 0.5f * p.pos[2] / focal; p.end[0] = 10;
        glr_set_palette_particles(r, &p, 1, palette);
        inspect(r, &v, position ? "DOS cockpit covers near-edge palette effect" : "DOS cockpit covers palette effect", colour, 0, 0);
    }
    glr_set_palette_particles(r, NULL, 0, NULL);
}

int main(void)
{
    SDL_Window *w;
    int c;
    glr *r;
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) return 2;
    w = SDL_CreateWindow("Mercs palette primitives", 128, 128, SDL_WINDOW_HIDDEN);
    c = gpu_test_start();
    if (!c) { fprintf(stderr, "%s\n", SDL_GetError()); SDL_Quit(); return 2; }
    r = glr_create();
    if (!r) { rb_quit(); SDL_DestroyWindow(w); SDL_Quit(); return 2; }
    tests(r);
    glr_destroy(r); rb_quit(); SDL_DestroyWindow(w); SDL_Quit();
    printf("Mercs palette primitive GPU: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
