/* Mercs DOS shell0x416cc, widget table0xb70a0, callbacks0x40ea8..0x4158b.
 * Layout is in original 640x480 pixels. The port's existing NATIVE resolution
 * joins the three original sizes; it never changes the original DLL bytes. */
#ifndef MERCS_COMBAT_VIEW_H
#define MERCS_COMBAT_VIEW_H
#include "mercsart.h"
#include "mercsoptions.h"
#include "portcfg.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int active, drag, resolution, resolution_changed, preview_value;
    int graphics, graphics_focus, edition, edition_changed, available[3];
    int texture_filter, dos_shading, hud_original;
    int gpu, gpu_changed;
} mercs_combat_view;
static const int mercs_combat_y[13] = {199,223,240,257,274,291,308,325,342,359,376,393,410};
static const int mercs_combat_offset[13] = {-1,-1,MO_OBJECT_TEXTURES,MO_TERRAIN_TEXTURES,
    MO_DISPLAY_DETAIL,MO_OBJECT_DENSITY,MO_CHUNKY_EXPLOSIONS,-1,MO_PARTICLES,
    MO_MULTIPLE_LIGHTS,MO_SHADOWS,MO_TRANSITIONS,MO_SUBTITLES};
static const int mercs_combat_volume_offset[3] = {MO_MUSIC,MO_EFFECTS,MO_VOICE};
/* This extra page is a native port addition. It reuses the original backdrop,
 * palette and fonts; the thirteen authored controls above remain untouched. */
static const int mercs_combat_editions[3] = {PORTCFG_ED_DOS, PORTCFG_ED_D3D, PORTCFG_ED_3DFX};
static const int mercs_combat_graphics_y[6] = {140,210,244,278,312,346};
static const char *const mercs_combat_graphics_labels[6] = {"RENDERER", "TEXTURE FILTER",
    "OBJECT TEXTURES", "TERRAIN TEXTURES", "DOS SHADING", "HUD LAYOUT"};
static const char *const mercs_combat_gpu_notice = "GPU changes apply to the next mission. Restart for menus.";

static void mercs_combat_open(mercs_combat_view *v, const portcfg *cfg)
{
    memset(v, 0, sizeof *v); v->active = 1; v->drag = -1; v->preview_value = -1;
    v->resolution = cfg->render_w == 320 ? 1 : cfg->render_w == 640 ? 2 : cfg->render_w == 1024 ? 3 : 0;
    v->edition = portcfg_mercs_name(cfg->renderer) ? cfg->renderer : PORTCFG_ED_DOS;
    v->texture_filter = cfg->texture_filter >= 0 && cfg->texture_filter < PORTCFG_FILTER_COUNT ? cfg->texture_filter : PORTCFG_FILTER_ORIGINAL;
    v->dos_shading = cfg->dos_sky_smooth; v->hud_original = cfg->hud_original;
    v->gpu = cfg->gpu >= 0 && cfg->gpu < PORTCFG_GPU_COUNT ? cfg->gpu : PORTCFG_GPU_AUTO;
}

/* The GPU is the host graphics API, independent of the original edition's
 * appearance. Keep the authored controls and their hit rectangles intact. */
static int mercs_combat_gpu_enabled(void)
{
#ifdef _WIN32
    return 1;
#else
    return 0;
#endif
}

static const char *mercs_combat_gpu_text(const mercs_combat_view *v)
{
#if defined(_WIN32)
    return v->gpu == PORTCFG_GPU_D3D12 ? "D3D12" : v->gpu == PORTCFG_GPU_VULKAN ? "VULKAN" : "AUTO";
#elif defined(__APPLE__)
    (void)v; return "METAL";
#else
    (void)v; return "VULKAN";
#endif
}

static int mercs_combat_gpu_hit(mercs_art *art, int x, int y)
{
    return mercs_combat_gpu_enabled() && x >= 484 && x < 628 &&
        y >= mercs_combat_y[7] && y < mercs_combat_y[7] + mercs_art_height(art, 6);
}

static void mercs_combat_gpu_step(mercs_combat_view *v)
{
    if (!mercs_combat_gpu_enabled()) return;
    v->gpu = (v->gpu + 1) % PORTCFG_GPU_COUNT;
    v->gpu_changed = 1;
}

static void mercs_combat_graphics_availability(mercs_combat_view *v, const portcfg *cfg)
{
    int i;
    char models[PORTCFG_PATH], textures[PORTCFG_PATH], kind[PORTCFG_PATH], sky[PORTCFG_PATH], fog[PORTCFG_PATH];
    for (i = 0; i < 3; i++)
        v->available[i] = !portcfg_edition(cfg, mercs_combat_editions[i], models, textures, kind, sky, fog);
}

static const char *mercs_combat_edition_text(int edition)
{
    return edition == PORTCFG_ED_D3D ? "TITANIUM DIRECT3D" : edition == PORTCFG_ED_3DFX ? "3DFX" : "DOS";
}

static int mercs_combat_graphics_enabled(const mercs_combat_view *v, int row)
{
    int i, count = 0;
    if (row == 0) { for (i = 0; i < 3; i++) count += v->available[i] != 0; return count > 1; }
    return row != 4 || v->edition == PORTCFG_ED_DOS;
}

static void mercs_combat_graphics_draw(mercs_art *art, const mercs_combat_view *v, const mercs_options *o)
{
    static const char *const filters[PORTCFG_FILTER_COUNT] = {"ORIGINAL", "NEAREST", "SMOOTH"};
    int i;
    mercs_art_background(art, 26);
    mercs_art_rect(art, 0, 100, 640, 335, 0);
    mercs_art_center(art, 6, 320, 105, "GRAPHICS", 17);
    for (i = 0; i < 6; i++) {
        const char *value = i == 0 ? mercs_combat_edition_text(v->edition) :
            i == 1 ? filters[v->texture_filter] :
            i == 2 ? (mercs_options_get(o, MO_OBJECT_TEXTURES) ? "ON" : "OFF") :
            i == 3 ? (mercs_options_get(o, MO_TERRAIN_TEXTURES) ? "ON" : "OFF") :
            i == 4 ? (v->dos_shading ? "SMOOTH" : "DITHERED") :
                     (v->hud_original ? "ORIGINAL" : "WIDE");
        /* DATABASE26 uses red51; red220 belongs to the controls-screen palette
         * and is almost black here. Preserve this backdrop's own title red. */
        int ink = !mercs_combat_graphics_enabled(v, i) ? 22 : v->graphics_focus == i ? 51 : 17;
        mercs_art_text(art, 6, 32, mercs_combat_graphics_y[i], mercs_combat_graphics_labels[i], ink);
        mercs_art_center(art, 6, 458, mercs_combat_graphics_y[i], value, ink);
    }
    mercs_art_text(art, 5, 32, 172, "TEXTURE SET", 22);
    mercs_art_center(art, 5, 458, 172,
        v->edition == PORTCFG_ED_D3D ? "TITANIUM" : v->edition == PORTCFG_ED_3DFX ? "3DFX" : "DOS", 22);
    mercs_art_center(art, 5, 320, 389, "Renderer changes apply to the next mission.", 17);
    mercs_art_center(art, 5, 320, 408, "Use arrows or click a value. Esc returns to Combat Variables.", 22);
    mercs_art_center(art, 6, 320, 442, "BACK", 17);
}

static void mercs_combat_draw(mercs_art *art, const mercs_combat_view *v, const mercs_options *o)
{
    static const char *const difficulty[] = {"EASY","MEDIUM","HARD"};
    static const char *const resolution[] = {"NATIVE","320x200","640x480","1024x768"};
    int i;
    if (v->graphics) { mercs_combat_graphics_draw(art, v, o); return; }
    mercs_art_background(art, 26);
    for (i = 0; i < 13; i++) {
        const char *value;
        if (i == 0) value = difficulty[o->dif[5] <= 2 ? o->dif[5] : 1];
        else if (i == 1) value = o->dif[4] ? "ON" : "OFF";
        else if (i == 7) value = resolution[v->resolution];
        else if (i == 4 || i == 5) value = mercs_options_get(o, mercs_combat_offset[i]) ? "HIGH" : "LOW";
        else value = mercs_options_get(o, mercs_combat_offset[i]) ? "ON" : "OFF";
        mercs_art_center(art, 6, 423, mercs_combat_y[i], value, 17);
    }
    for (i = 0; i < 3; i++) {
        int value = mercs_options_volume(o, mercs_combat_volume_offset[i]);
        mercs_art_shape(art, 2, 1, 335, 113 + 24 * i);
        mercs_art_shape(art, 2, 0, 343 + value / 256, 112 + 24 * i);
    }
    {
        int ink = mercs_combat_gpu_enabled() ? 17 : 22;
        mercs_art_text(art, 5, 488, mercs_combat_y[7], "GPU:", ink);
        mercs_art_center(art, 5, 584, mercs_combat_y[7], mercs_combat_gpu_text(v), ink);
        if (v->gpu_changed)
            mercs_art_center(art, 5, 320, 427, mercs_combat_gpu_notice, 22);
    }
    mercs_art_center(art, 6, 320, 442, "GRAPHICS OPTIONS (G)", 17);
}

static int mercs_combat_graphics_button(mercs_art *art, int x, int y)
{
    return x >= 180 && x < 460 && y >= 440 && y < 442 + mercs_art_height(art, 6);
}

static int mercs_combat_graphics_hit(mercs_art *art, int x, int y)
{
    int i, height = mercs_art_height(art, 6);
    for (i = 0; i < 6; i++)
        if (x >= 315 && x < 628 && y >= mercs_combat_graphics_y[i] && y < mercs_combat_graphics_y[i] + height) return i;
    return -1;
}

static void mercs_combat_graphics_step(mercs_combat_view *v, mercs_options *o, int row, int direction)
{
    int i, current = 0;
    if (row < 0 || row >= 6 || !mercs_combat_graphics_enabled(v, row)) return;
    if (row == 0) {
        for (i = 0; i < 3; i++) if (mercs_combat_editions[i] == v->edition) current = i;
        for (i = 0; i < 3; i++) {
            current = (current + direction + 3) % 3;
            if (v->available[current]) { v->edition = mercs_combat_editions[current]; v->edition_changed = 1; break; }
        }
    } else if (row == 1) v->texture_filter = (v->texture_filter + direction + PORTCFG_FILTER_COUNT) % PORTCFG_FILTER_COUNT;
    else if (row == 2 || row == 3) {
        int offset = row == 2 ? MO_OBJECT_TEXTURES : MO_TERRAIN_TEXTURES;
        mercs_options_set(o, offset, !mercs_options_get(o, offset));
    } else if (row == 4) v->dos_shading = !v->dos_shading;
    else v->hud_original = !v->hud_original;
}

/* A profile change carries the controls the player can edit, without copying
 * a different edition's private/driver fields or truncating its trailing data. */
static void mercs_combat_copy_options(mercs_options *destination, const mercs_options *source, int copy_resolution)
{
    int i;
    for (i = 0; i < 13; i++) if (mercs_combat_offset[i] >= 0)
        mercs_options_set(destination, mercs_combat_offset[i], mercs_options_get(source, mercs_combat_offset[i]));
    for (i = 0; i < 3; i++)
        mercs_options_set(destination, mercs_combat_volume_offset[i], mercs_options_get(source, mercs_combat_volume_offset[i]));
    destination->dif[4] = source->dif[4]; destination->dif[5] = source->dif[5];
    if (copy_resolution) memcpy(destination->snd + MO_DRIVER, source->snd + MO_DRIVER, 15);
}

/* Only the values (not the baked-in labels) are clickable, as in0x5cbf0.
 * Original rectangles exclude their right and bottom edges. */
static int mercs_combat_hit(mercs_art *art, int x, int y)
{
    int i, height = mercs_art_height(art, 6);
    for (i = 0; i < 13; i++)
        if (x >= 373 && x < 473 && y >= mercs_combat_y[i] && y < mercs_combat_y[i] + height) return i;
    for (i = 0; i < 3; i++)
        if (x >= 335 && x < 620 && y >= 113 + 24*i && y < 134 + 24*i) return 13+i;
    return -1;
}

static void mercs_combat_toggle(mercs_combat_view *v, mercs_options *o, int row)
{
    if (row == 0) o->dif[5] = o->dif[5] >= 2 ? 0 : (uint8_t)(o->dif[5]+1);
    else if (row == 1) o->dif[4] = o->dif[4] ? 0 : 1;
    else if (row == 7) {
        v->resolution = (v->resolution + 1) % 4; v->resolution_changed = 1;
        if (v->resolution) mercs_options_set_resolution(o, v->resolution - 1);
    } else if (row >= 0 && row < 13)
        mercs_options_set(o, mercs_combat_offset[row], !mercs_options_get(o, mercs_combat_offset[row]));
}

static int mercs_combat_slide(mercs_combat_view *v, mercs_options *o, int x)
{
    int value = x - 350;
    if (v->drag < 0 || v->drag >= 3) return 0;
    if (value < 0) value = 0;
    if (value > 256) value = 256;
    value *= 256;
    mercs_options_set(o, mercs_combat_volume_offset[v->drag], (uint32_t)value);
    /* Original drag callback0x4140c previews when the level changes by0xa00. */
    if (v->preview_value >= 0 && abs(value - v->preview_value) < 0xa00) return 0;
    v->preview_value = value;
    return 1;
}

static void mercs_combat_resolution(const mercs_combat_view *v, portcfg *cfg)
{
    static const int widths[] = {0,320,640,1024}, heights[] = {0,200,480,768};
    cfg->render_w = widths[v->resolution]; cfg->render_h = heights[v->resolution];
}
#endif
