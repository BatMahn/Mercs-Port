/* Original-binary threshold fixtures plus an actual GPU selection check.
 * The three meshes deliberately occupy the same plane. Drawing more than
 * one level, using Euclidean distance, or ignoring detail/FOV cannot pass
 * the coloured centre-pixel checks. No proprietary assets are required. */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "sdl3_test.h"
#include "gpu_test.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "glr.h"
#include "mercs_world_lod_original.h"

static int fail, pass;
static void check(int condition, const char *name)
{
    if (condition) pass++;
    else { fprintf(stderr, "FAIL: %s\n", name); fail++; }
}

static void selectors(void)
{
    wtb_object objects[3];
    wtb_model model;
    size_t i;
    int j;
    memset(&model, 0, sizeof model);
    model.objects = objects;
    for (i = 0; i < sizeof original_world_lod / sizeof original_world_lod[0]; i++) {
        memset(objects, 0, sizeof objects);
        model.object_count = original_world_lod[i].n;
        for (j = 0; j < model.object_count; j++) {
            snprintf(objects[j].name, sizeof objects[j].name, "part_%d", original_world_lod[i].thresholds[j]);
            objects[j].flags = 0x1000;
        }
        check(wtb_lod_select_q16(&model, original_world_lod[i].depth,
                                original_world_lod[i].scale) == original_world_lod[i].fixed_level,
              "DOS / dedicated executed-original selector");
        check(wtb_lod_select_float(&model, (float)original_world_lod[i].depth,
                                  original_world_lod[i].scale / 65536.0f) == original_world_lod[i].float_level,
              "Titanium executed-original selector");
    }
    model.object_count = 3;
    strcpy(objects[0].name, "far_4000");
    strcpy(objects[1].name, "near_1");
    strcpy(objects[2].name, "mid_2000");
    check(wtb_lod_select_q16(&model, 3000, 65536) == 2, "loader sorts thresholds, not stream order");
    check(wtb_lod_select_q16(&model, 0, 65536) == 1, "first level covers below smallest nonzero threshold");
    objects[1].flags = objects[2].flags = 0;
    check(wtb_lod_select_q16(&model, 4500, 65536) == 0, "LOD flag remains active for following objects");
    strcpy(objects[1].name, "near_bad_1");
    check(wtb_lod_select_q16(&model, 999999, 65536) == 1, "first underscore and zero threshold special case");
    objects[0].flags = 0;
    check(wtb_lod_select_q16(&model, 999999, 65536) == 0, "ordinary multistream remains first object");
    objects[0].flags = 0x3000;
    check(wtb_lod_select_q16(&model, 999999, 65536) == 0, "translated stream is not a distance chain");
}

static int upload(glr *r, int depth, int offaxis, unsigned char palette[256][3], texture **slots)
{
    wtb_vertex vertices[4];
    wtb_poly polys[3];
    wtb_object objects[3];
    mech3d_part part;
    mech3d model;
    int i, j;
    static const int x[4] = {-10000,10000,10000,-10000};
    static const int y[4] = {-10000,-10000,10000,10000};
    memset(vertices, 0, sizeof vertices);
    memset(polys, 0, sizeof polys);
    memset(objects, 0, sizeof objects);
    memset(&part, 0, sizeof part);
    memset(&model, 0, sizeof model);
    for (i = 0; i < 4; i++) { vertices[i].x = x[i]; vertices[i].y = y[i]; vertices[i].z = depth; }
    for (i = 0; i < 3; i++) {
        snprintf(objects[i].name, sizeof objects[i].name, "level_%d", i == 0 ? 1 : i * 1000);
        objects[i].flags = 0x1000;
        objects[i].verts = vertices; objects[i].vert_count = 4;
        objects[i].polys = &polys[i]; objects[i].poly_count = 1;
        polys[i].n = 4; polys[i].color = (uint16_t)(0x5010 + i);
        for (j = 0; j < 4; j++) polys[i].idx[j] = (uint16_t)j;
    }
    part.model.objects = objects; part.model.object_count = 3;
    part.ti = 1; part.objtype = 0x800;
    part.has_rot = 1; part.rot[0] = part.rot[4] = part.rot[8] = 1;
    part.sphere_ok = 1; part.hull_center[0] = (float)offaxis;
    part.hull_center[2] = (float)depth; part.hull_radius = 100000;
    model.parts = &part; model.part_count = 1;
    return glr_set_mech(r, &model, palette, slots, 0, 0);
}

int main(void)
{
    SDL_Window *window;
    int context;
    glr *r;
    glr_view view;
    unsigned char palette[256][3] = {{0}};
    uint32_t pixels[3] = {0xff0000ffu,0xff00ff00u,0xffff0000u};
    texture textures[3];
    texture *slots[GLR_SLOTS] = {0};
    static const int depths[] = {200,499,501,749,751,999,1001,1499,1501,1999,2001,3001};
    int mode, detail, size, offaxis, i;
    selectors();
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    window = SDL_CreateWindow("Mercs original world LOD", 640, 480, SDL_WINDOW_HIDDEN);
    context = gpu_test_start();
    if (!context) { fprintf(stderr, "GPU: %s\n", SDL_GetError()); return 1; }
    r = glr_create();
    if (!r) return 1;
    memset(textures, 0, sizeof textures);
    for (i = 0; i < 3; i++) {
        textures[i].w = textures[i].h = 1; textures[i].rgba = &pixels[i]; slots[16+i] = &textures[i];
    }
    glr_set_titanium(r, slots, NULL);
    glr_default_view(&view);
    view.free_cam = 1; view.vfov = 90; view.ambient = 1; view.bilinear = 0;
    for (mode = 1; mode <= 2; mode++) for (detail = 1; detail <= 2; detail++)
    for (size = 0; size < 3; size++) for (offaxis = 0; offaxis <= 1; offaxis++) {
        int height = 160 * (size + 1), width = 640;
        float scale = (size + 1) * .5f / detail;
        view.ti_fog_vtx = mode; view.lod_quality = detail;
        for (i = 0; i < (int)(sizeof depths / sizeof depths[0]); i++) {
            unsigned char pixel[4];
            int want = depths[i] >= 2000 * scale ? 2 : depths[i] >= 1000 * scale ? 1 : 0;
            int ok = upload(r, depths[i], offaxis ? 100000 : 0, palette, slots) == 0;
            gpu_test_draw(r, &view, width, height);
            gpu_test_read_rgba(width/2, height/2, 1, 1, pixel);
            ok = ok && gpu_test_ok() && pixel[want] == 255 &&
                 pixel[(want+1)%3] == 0 && pixel[(want+2)%3] == 0 && pixel[3] == 255;
            if (!ok) fprintf(stderr, "GPU mode%d detail%d %dx%d offaxis%d depth%d: %u,%u,%u expected level%d\n",
                             mode, detail, width, height, offaxis, depths[i], pixel[0], pixel[1], pixel[2], want);
            check(ok, "GPU one original level by axial depth / focal / detail");
        }
    }
    glr_destroy(r); rb_quit(); SDL_DestroyWindow(window); SDL_Quit();
    printf("Mercs world LOD: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
