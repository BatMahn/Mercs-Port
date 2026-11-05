/* common3d.h - shared setup for the GL tools: a mech plus its mission's textures. */
#ifndef MW2_COMMON3D_H
#define MW2_COMMON3D_H
#include "bwd.h"
#include "glr.h"
#include "mech3d.h"
#include "prj.h"
#include "render.h"
#include "tex.h"
#include "skygnd.h"
#include "wtb.h"
#include "mercstexclock.h"
#include "mercstexmap.h"
#include <strings.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    texture *slot[GLR_SLOTS];
    texture *bank0[GLR_SLOTS];   /* the bank-0 bitmaps (scrub billboards, colour type 3) */
    uint8_t  palette[256][3];
    int      textures_loaded;
    /* the mission's PALG slots (PAL ids; 0 day, 4 dawn / dusk, 8 night, 0xc INFRARED, 0x10 black, 0x11 red) - the 3D
     * editions' palette fades (0x1002ad50) */
    uint8_t  slotpal[20][256][3];
    int      slot_ok[20];
    /* bank-1 frame groups (bwd_texmap frames; engine 0x1002ce50 steps them): frame 0 is slot[]; fr[k] k >= 1 the rest
     * (owned); rate in ticks a frame (0: still), mode 0 still / 1 loop / 2 once */
    int      nfr[GLR_SLOTS], rate[GLR_SLOTS], mode[GLR_SLOTS];
    texture *fr[GLR_SLOTS][BWD_TEXMAP_FRAMES];
    mercs_texclock clock[GLR_SLOTS]; /* scenery-only slots; effect aliases observe fx_world */
} c3d_scene;

/* Textures come from `texs`. An ATi archive has one format (ARGB1555). For 3dfx/S3
 * the format per texture isn't stored; insignia (L1/L2...) are ARGB4444, the rest
 * are taken as RGB555 - right for nearly all surfaces, ATi is the exact source. */
/* The inherited Titanium geometry/lighting path. Native Mercs accelerated profiles
 * set this to the SAME edition archive as the mission and use ati=5 throughout:
 * their CELs are 1.2x palette textures and never fall back to another edition.
 * Only the foundation's legacy overlay path searches this archive by name first. */
static prj_archive *c3d_ti;
static int c3d_ti_hits, c3d_ti_miss;

/* `alpha`: the bitmap is drawn with the engine's alpha flag (polygon flag 2) - the bank-0 billboards. The 3Dfx edition
 * stores no format: its Texus conversion reads a CEL as ARGB1555 (Glide format 0xb) for a polygon drawn without flag 2
 * and as ARGB4444 (0xc) with it (0x10067240: DAT_10265710 = (flags & 2) >> 1; 0x10066a30 -> 0x100685f0 / 0x10068710),
 * and the billboard 0x1002a8e0 always passes flag 2 (0x1002aaea: 0x1002c880 param_8 = 1 -> local_50 = 2) */
static texture *c3d_texture(prj_archive *texs, int ati, const char *name, int alpha)
{
    prj_record r;
    texture *t = calloc(1, sizeof *t);
    /* `ati` is the archive format: 0 legacy 3Dfx / S3, 1 ATi, 2 PowerVR, 3 DOS,
     * 4 Matrox, 5 Mercs accelerated / Titanium 1.2x. */
    int enc = ati == 1 ? TEX_ATI : ati == 2 ? TEX_PVR : ati == 3 ? TEX_DOS : ati == 4 ? TEX_MGA : ati == 5 ? TEX_TITANIUM
            : (alpha || (toupper((unsigned char)name[0]) == 'L' && isdigit((unsigned char)name[1]))) ? TEX_3DFX_4444 : TEX_3DFX_555;
    if (!t) return NULL;
    if (c3d_ti && ati != 3 && ati != 5) {
        if (prj_read_named(c3d_ti, "CEL", name, &r) == PRJ_OK) {
            int ok = tex_decode(r.data, r.size, TEX_TITANIUM, t) == 0;
            prj_record_free(&r);
            if (ok) { c3d_ti_hits++; return t; }
        }
        c3d_ti_miss++;
    }
    if (prj_read_named(texs, "CEL", name, &r) != PRJ_OK) { free(t); return NULL; }
    if (tex_decode(r.data, r.size, enc, t) != 0) { free(t); t = NULL; }
    else if (c3d_ti && t->alt) { free(t->alt); t->alt = NULL; }   /* (the TEXTURES TITANIUM looks: one reading, as before) */
    if (t) t->cel = r.id;   /* (the 3Dfx mw2.mip index, glr_view.tex_cache) */
    prj_record_free(&r);
    return t;
}

static void c3d_free_scene(c3d_scene *s)
{
    int i;
    for (i = 0; i < GLR_SLOTS; i++)
    {
        int k;
        if (s->slot[i]) { tex_free(s->slot[i]); free(s->slot[i]); s->slot[i] = NULL; }
        if (s->bank0[i]) { tex_free(s->bank0[i]); free(s->bank0[i]); s->bank0[i] = NULL; }
        for (k = 1; k < BWD_TEXMAP_FRAMES; k++) if (s->fr[i][k]) { tex_free(s->fr[i][k]); free(s->fr[i][k]); s->fr[i][k] = NULL; }
        s->nfr[i] = 0;
    }
}

/* the S3 engine's texture cache forgets the readings it holds (texture.upfmt): a palette reload (0x1003ec90 -> 0x1000f870
 * snapshots the use counter 0x1006225c into 0x10062260; 0x10010170 uploads again, in the drawing polygon's format, any
 * texture last used before it) */
static void c3d_texture_cache_reset(c3d_scene *s)
{
    int i, k;
    for (i = 0; i < GLR_SLOTS; i++) {
        if (s->slot[i]) s->slot[i]->upfmt = 0;
        if (s->bank0[i]) s->bank0[i]->upfmt = 0;
        for (k = 1; k < BWD_TEXMAP_FRAMES; k++) if (s->fr[i][k]) s->fr[i][k]->upfmt = 0;
    }
}

/* mission: four-letter code; texture maps and the PALG palette table come
 * from the full include tree, not an assumed <mission>_DA palette name. */
static int c3d_load_scene(prj_archive *models, prj_archive *texs, int ati, const char *mission, c3d_scene *s)
{
    char name[16];
    prj_record rec;
    r_target holder;
    int i, phase = 0, day = 0;

    memset(s, 0, sizeof *s);
    /* Read palette/environment metadata, then replay texture controls in the
     * original expanded chunk order. The record inventory is breadth-first
     * and must not determine BMID overrides or repeated-include state. */
    {
        bwd_mission bm;
        int k, c, maps = 0;
        snprintf(name, sizeof name, "%.4sSCN1", mission);
        if (bwd_mission_load(models, name, &bm) == 0) {
            for (k = 0; k < bm.record_count; k++) {
                int has = 0;
                for (c = 0; c < bm.records[k].chunk_count; c++) {
                    const bwd_chunk *ch = &bm.records[k].chunks[c];
                    if (strcmp(ch->tag, "INIT") == 0 && ch->size >= 12)
                        phase = (int32_t)((uint32_t)ch->data[8] | (uint32_t)ch->data[9] << 8 |
                                          (uint32_t)ch->data[10] << 16 | (uint32_t)ch->data[11] << 24);
                    else if (strcmp(ch->tag, "PLNT") == 0 && ch->size >= 16)
                        day = (int32_t)((uint32_t)ch->data[12] | (uint32_t)ch->data[13] << 8 |
                                        (uint32_t)ch->data[14] << 16 | (uint32_t)ch->data[15] << 24);
                    if (strcmp(ch->tag, "PALG") == 0 && !s->slot_ok[0]) {   /* the first PALG: PAL ids per slot */
                        int s2, pt = prj_find_type(models, "PAL");
                        for (s2 = 0; s2 < 20 && (size_t)(s2 * 2 + 1) < ch->size && pt >= 0; s2++) {
                            prj_record sp;
                            if (prj_read(models, pt, ch->data[s2 * 2] | ch->data[s2 * 2 + 1] << 8, &sp) == PRJ_OK) {
                                if (sp.size >= 768 && r_init(&holder, 1, 1) == 0) {
                                    r_set_palette(&holder, sp.data, sp.size);
                                    memcpy(s->slotpal[s2], holder.palette, sizeof s->slotpal[s2]);
                                    s->slot_ok[s2] = 1;
                                    r_free(&holder);
                                }
                                prj_record_free(&sp);
                            }
                        }
                    }
                }
                for (c = 0; c < bm.records[k].chunk_count; c++)
                    if (strcmp(bm.records[k].chunks[c].tag, "BMID") == 0) { has = 1; break; }
                if (has) maps++;
            }
            {
                mercs_texbinding *bindings = calloc(512, sizeof *bindings);
                if (bindings && !mercs_texmap_read(models, &bm, bindings)) {
                    for (i = 0; i < GLR_SLOTS; i++) {
                        const mercs_texbinding *binding = &bindings[256 + i];
                        int f;
                        if (binding->bound && binding->frames > 0) {
                            for (f = 0; f < binding->frames && f < BWD_TEXMAP_FRAMES; f++) {
                                prj_record bitmap;
                                texture *t;
                                if (prj_read(models, prj_find_type(models, "CEL"), binding->cel[f], &bitmap)) break;
                                t = c3d_texture(texs, ati, bitmap.name, 0);
                                prj_record_free(&bitmap);
                                if (!t) break;
                                s->fr[i][f] = t; s->nfr[i] = f + 1;
                                if (!f) { s->slot[i] = t; s->textures_loaded++; }
                            }
                            s->rate[i] = binding->rate; s->mode[i] = binding->mode;
                            s->clock[i].frame = 0; s->clock[i].rate = binding->rate;
                            s->clock[i].mode = binding->mode; s->clock[i].state = 1;
                            s->clock[i].stamp = -1;
                        }
                        binding = &bindings[i];
                        if (binding->bound && binding->frames > 0) {
                            prj_record bitmap;
                            if (!prj_read(models, prj_find_type(models, "CEL"), binding->cel[0], &bitmap)) {
                                s->bank0[i] = c3d_texture(texs, ati, bitmap.name, 1);
                                prj_record_free(&bitmap);
                            }
                        }
                    }
                }
                free(bindings);
            }
            bwd_mission_free(&bm);
        }
        if (!maps) fprintf(stderr, "no texture map for mission %s\n", mission);
    }
    if (s->slot_ok[0]) {
        /* Mercs INIT +8 selects dawn/day/dusk/night. The original Windows
         * 0x100021b0 phase selector and PALG consumer use slots 4/0/4/8.
         * TRA1, for example, references TRA10NDA through PALG; neither
         * TRA1_DA nor the inherited CYAN_DA fallback exists in its archive. */
        static const int hours[4] = {5, 7, 17, 19}, slots[4] = {4, 0, 4, 8};
        int start = bwd_mercs_day_start(phase, day), slot = 0;
        if (start >= 0) {
            int group = 3;
            for (i = 0; i < 4; i++) if (day / 24 * hours[i] <= start) group = i;
            if (s->slot_ok[slots[group]]) slot = slots[group];
        }
        memcpy(s->palette, s->slotpal[slot], sizeof s->palette);
    } else if (r_init(&holder, 1, 1) == 0) {
        snprintf(name, sizeof name, "%.4s_DA", mission);
        if (prj_read_named(models, "PAL", name, &rec) == PRJ_OK || prj_read_named(models, "PAL", "CYAN_DA", &rec) == PRJ_OK) {
            r_set_palette(&holder, rec.data, rec.size);
            prj_record_free(&rec);
        }
        memcpy(s->palette, holder.palette, sizeof s->palette);
        r_free(&holder);
    }
    return 0;
}
/* Sky, ground and fog for a mission, the "3dfx + PowerVR fog" way:
 * tiling from MW2_SKYGND (3dfx skygnd.par), fog from MW2_SKYGND_FOG (PowerVR one).
 * Fog colour = mean of ground texture, sky texture and ambient grey (PowerVR recipe).
 * fog_override >= 0 replaces the density (per game unit). */
static void c3d_environment(glr *r, c3d_scene *s, const char *mission, glr_view *v, float fog_override)
{
    skygnd tiles, fog;
    texture *g = s->slot[SKYGND_GROUND_SLOT], *sky = s->slot[SKYGND_SKY_SLOT];
    float ga[3] = {0.5f, 0.5f, 0.5f}, sa[3] = {0.5f, 0.5f, 0.5f};
    int k;
    skygnd_defaults(&tiles);
    skygnd_lookup(getenv("MERCS_SKYGND"), mission, &tiles);
    /* the PowerVR edition's sky plane repeats every 64e6 / tile_sky (0x1005ED90: q = a2 / a3), twice as often as the 3Dfx
     * ceiling's 128e6 / tile_sky */
    if (getenv("MERCS_EDITION") && strcmp(getenv("MERCS_EDITION"), "pvr") == 0) tiles.tile_sky *= 2.0f;
    skygnd_defaults(&fog);
    skygnd_lookup(getenv("MERCS_SKYGND_FOG"), mission, &fog);
    glr_set_environment(r, g, sky, tiles.tile_sky, tiles.tile_ground);
    if (g) glr_texture_average(g, ga);
    if (sky) glr_texture_average(sky, sa);
    for (k = 0; k < 3; k++) {
        v->fog_color[k] = (ga[k] + sa[k] + v->ambient) / 3.0f;
        v->sky_color[k] = v->fog_color[k];
    }
    v->fog_density = fog_override >= 0 ? fog_override : (fog.has_fog ? fog.fog : 0);   /* the raw sgl_set_fog density */
    v->ti_fog[0] = 0;
    v->ti_light = 0; v->ti_dyn_n = 0;
    glr_set_titanium(r, NULL, NULL);
}

/* The Titanium look's sky and ground (mercsw.dll): its SKYGND.PAR (MW2_SKYGND) for the mission; the Titanium archive's
 * own texture map (its slot numbers differ from the 3D editions') for the ground (slot 0x4a, 0x100559eb) and the sky
 * dome (the SKYDOME object its planets include: clouds 0x8c, mountains 0x5d / 0x6c / 0x6d / 0x8d, moon 0x7c, sun 0x7d);
 * the clear colour sky=, the fog colour fog=, density's Z the full-fog depth (fog: whether the renderer draws fog).
 * glview updates the camera-height terms (c3d_ti_frame) every frame. */
static skygnd_ti c3d_ti_sg;
/* the Titanium texture table (its own slot numbers) and the stars' camouflage sets, kept for its models (glr_set_titanium) */
static c3d_scene c3d_ti_scene;
static int c3d_ti_have_scene, c3d_ti_camo[8];
static texture *c3d_ti_draw[GLR_SLOTS];   /* the table glr draws with: each slot's current bank-1 frame (glview scene_anim) */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static void c3d_ti_environment(glr *r, c3d_scene *s, const char *mission, glr_view *v, int fog)
{
    c3d_scene ts;
    prj_archive *keep = c3d_ti;
    texture *ground = s->slot[SKYGND_GROUND_SLOT];
    int k, dome = 0;
    skygnd_ti_lookup(getenv("MERCS_SKYGND"), mission, &c3d_ti_sg);
    c3d_ti = NULL;   /* (the scene below is all Titanium: no overlay counting) */
    glr_set_titanium(r, NULL, NULL);
    if (c3d_ti_have_scene) { c3d_free_scene(&c3d_ti_scene); c3d_ti_have_scene = 0; }
    c3d_load_scene(keep, keep, 5, mission, &ts);
    c3d_ti = keep;
    if (ts.slot[0x4a]) ground = ts.slot[0x4a];
    /* the ground quad (0x1005545b): half size ground_max x 2 around the camera, UV = world / (half size / tile_ground):
     * in the port's terms (repeats over 32e6, see skygnd.h) 32e6 x tile / half size */
    glr_set_environment(r, c3d_ti_sg.tile_ground > 0 ? ground : NULL, NULL, 4.0f,
                        32.0e6f * c3d_ti_sg.tile_ground / (c3d_ti_sg.ground_max * 2.0f));
    {
        mercs_ground cfg;
        int dedicated = v->ti_fog_vtx == 2;
        memset(&cfg,0,sizeof cfg);
        cfg.mode = dedicated ? MERCS_GROUND_3DFX : MERCS_GROUND_TITANIUM;
        cfg.size = c3d_ti_sg.ground_max * (dedicated ? 1.0f : 2.0f);
        cfg.repeats[0] = cfg.repeats[1] = dedicated ? floorf(c3d_ti_sg.tile_ground) : c3d_ti_sg.tile_ground;
        memcpy(cfg.clip,c3d_ti_sg.gclip,sizeof cfg.clip);
        glr_set_mercs_ground(r,c3d_ti_sg.tile_ground > 0 ? ground : NULL,&cfg);
    }
    {   /* Original WTB prefixes tl1skyd/tl1star also include SKYDOM1 and
         * SKYDOMF. Match the internal WTBO name, not the BWD include name.
         * See tests/fixtures/MERCS_ACCELERATED_BACKDROP_EVIDENCE.md. */
        char name[16];
        bwd_mission bm;
        snprintf(name, sizeof name, "%.4sSCN1", mission);
        if (bwd_mission_load(keep, name, &bm) == 0) {
            for (k = 0; k < bm.record_count && !dome; k++) {
                int c;
                for (c = 0; c < bm.records[k].chunk_count; c++) {
                    const bwd_chunk *ch = &bm.records[k].chunks[c];
                    prj_record pr;
                    wtb_model wm;
                    if (strcmp(ch->tag, "OBJ") != 0 || ch->size < 52) continue;
                    if (prj_read(keep, prj_find_type(keep, "POLY"), ch->data[48] | ch->data[49] << 8, &pr) != PRJ_OK) continue;
                    if (wtb_parse(pr.data, pr.size, &wm) == 0) {
                        if (mech3d_model_is_backdrop(&wm)) { glr_set_skydome(r, &wm.objects[0], ts.slot); dome = 1; }
                        wtb_free(&wm);
                    }
                    prj_record_free(&pr);
                    if (dome) break;
                }
            }
            bwd_mission_free(&bm);
        }
    }
    if (!dome) glr_set_skydome(r, NULL, ts.slot);
    {   /* the stars' camouflage sets: the maps' CAMO chunks {i32 star, i32 set}, each clamped 0..7 (mercsw 0x10073a6b ->
         * 0x100a11f0, initially {0..7}; the engine keeps the previous mission's, here reset per mission) */
        char name[16];
        bwd_mission bm;
        int c;
        for (k = 0; k < 8; k++) c3d_ti_camo[k] = k;
        snprintf(name, sizeof name, "%.4sSCN1", mission);
        if (bwd_mission_load(keep, name, &bm) == 0) {
            for (k = 0; k < bm.record_count; k++)
                for (c = 0; c < bm.records[k].chunk_count; c++) {
                    const bwd_chunk *ch = &bm.records[k].chunks[c];
                    int32_t st, set;
                    if (strcmp(ch->tag, "CAMO") != 0 || ch->size < 8) continue;
                    st = (int32_t)(ch->data[0] | ch->data[1] << 8 | ch->data[2] << 16 | (uint32_t)ch->data[3] << 24);
                    set = (int32_t)(ch->data[4] | ch->data[5] << 8 | ch->data[6] << 16 | (uint32_t)ch->data[7] << 24);
                    st = st < 1 ? 0 : st > 7 ? 7 : st; set = set < 1 ? 0 : set > 7 ? 7 : set;
                    c3d_ti_camo[st] = set;
                }
            bwd_mission_free(&bm);
        }
    }
    c3d_ti_scene = ts;   /* kept: the Titanium models' textures */
    c3d_ti_have_scene = 1;
    memcpy(c3d_ti_draw, c3d_ti_scene.slot, sizeof c3d_ti_draw);
    glr_set_titanium(r, c3d_ti_draw, c3d_ti_camo);
    {   /* its bank-0 bitmaps (the models' colour type 3 billboards) and the palette shade of its colour type 1 polygons
         * (mercsw 0x1002f5e3): the planet's LITE (0x10071631: +8 / +12 / +16 the light vector, 999999 x 3 no light term
         * (0x10097cb4), 888888 x 3 a point light at the eye (0x100ad320; no Titanium mission uses either); +20 the ambient
         * level, +22 directional, +24 the fog range (0x1009e99c)) and SKYGND.PAR's globalintensity (0x1002e8b7: clamped
         * to 0..128) */
        glr_ti_shade sh;
        char name[16];
        prj_record pr;
        memset(&sh, 0, sizeof sh);
        snprintf(name, sizeof name, "%.4sPLT1", mission);
        if (prj_read_named(keep, "BWD", name, &pr) == PRJ_OK) {
            bwd_chunk lc[64];
            int nlc = bwd_chunks(pr.data, pr.size, lc, 64), q;
            for (q = 0; q < nlc; q++)
                if (strcmp(lc[q].tag, "LITE") == 0 && lc[q].size >= 24) {
                    const uint8_t *d = lc[q].data;
                    int32_t x = (int32_t)(d[8] | d[9] << 8 | d[10] << 16 | (uint32_t)d[11] << 24);
                    int32_t y = (int32_t)(d[12] | d[13] << 8 | d[14] << 16 | (uint32_t)d[15] << 24);
                    int32_t z = (int32_t)(d[16] | d[17] << 8 | d[18] << 16 | (uint32_t)d[19] << 24);
                    sh.on = 1;
                    sh.lvec[0] = (float)x; sh.lvec[1] = (float)y; sh.lvec[2] = (float)z;
                    sh.light = !((x == 999999 && y == 999999 && z == 999999) || (x == 888888 && y == 888888 && z == 888888));
                    sh.amb = (float)(int16_t)(d[20] | d[21] << 8);
                    sh.dir = (int16_t)(d[22] | d[23] << 8) != 0;
                    sh.eye = x == 888888 && y == 888888 && z == 888888;
                    if (lc[q].size >= 28) sh.range = (float)(int32_t)(d[24] | d[25] << 8 | d[26] << 16 | (uint32_t)d[27] << 24);
                }
            prj_record_free(&pr);
        }
        sh.gi = c3d_ti_sg.intensity < 1 ? 0 : c3d_ti_sg.intensity > 128 ? 128 : c3d_ti_sg.intensity;
        glr_set_titanium_extra(r, c3d_ti_scene.bank0, &sh);
        printf("titanium: shade %s amb %.0f light %d dir %d vec %.0f %.0f %.0f gi %d eye %d range %.0f\n", sh.on ? "LITE" : "none", (double)sh.amb, sh.light, sh.dir,
               (double)sh.lvec[0], (double)sh.lvec[1], (double)sh.lvec[2], sh.gi, sh.eye, (double)sh.range);
    }
    for (k = 0; k < 3; k++) { v->fog_color[k] = (float)c3d_ti_sg.fog[k] / 255.0f; v->sky_color[k] = (float)c3d_ti_sg.sky[k] / 255.0f; }
    v->fog_density = 0;
    /* fog: with a renderer that fogs (glview look_fog: the PowerVR edition, the enhanced mix) the Titanium fog; else none,
     * and the clear colour is sky= as it is (the engine's Y = 0 case, 0x1006a989) - a port choice */
    v->ti_fog[0] = fog ? 1.0f : 0.0f; v->ti_fog[1] = c3d_ti_sg.dens_z; v->ti_fog[2] = 1;
    v->ti_fog_height[0] = c3d_ti_sg.dens_y;
    v->ti_fog_height[1] = c3d_ti_sg.dens_m;
    /* Dedicated Mercs 3Dfx uses min(1, densityZ / depth_cm), not Titanium's
     * linear fog. Its SKYGND parser leaves Z unscaled (0x10036cf0), and its
     * 4x internal camera depth is divided by four before drawing (0x100145c3).
     * Undo the Titanium parser's x10 here; edition_look chooses vertex mode 2. */
    if (getenv("MERCS_EDITION") && strcmp(getenv("MERCS_EDITION"), "3dfx") == 0)
        v->ti_fog[1] *= 0.1f;
    /* Mercs Titanium's parser multiplies raw Z by2 (0x10053ae2), and
     * its world packet retains cm depth. The inherited MW2 helper uses10. */
    else if (getenv("MERCS_EDITION") && strcmp(getenv("MERCS_EDITION"), "direct3d") == 0)
        v->ti_fog[1] *= 0.2f;
    v->pvr_veil = 0;
    /* the lights (SKYGND.PAR amb / wa / wb -> the engine's light table, 0x10053dc5..0x10053ec0): per vertex on the objects
     * (0x1002ecaa), one colour on the ground (0x1005527e: the ambient + each directional light x |its y| + each point light
     * whole) */
    v->ti_light = !getenv("MERCS_TI_NOLIGHT");   /* (TEST ONLY: MW2_TI_NOLIGHT) */
    for (k = 0; k < 3; k++) {
        v->ti_amb[k] = (float)c3d_ti_sg.amb[k] / 255.0f;
        v->ti_lcol[0][k] = (float)c3d_ti_sg.wa[k] / 255.0f; v->ti_lcol[1][k] = (float)c3d_ti_sg.wb[k] / 255.0f;
        v->ti_lvec[0][k] = c3d_ti_sg.wa_vec[k]; v->ti_lvec[1][k] = c3d_ti_sg.wb_vec[k];
    }
    for (k = 0; k < 3; k++) {
        float g = v->ti_amb[k];
        {
            int q;
            for (q = 0; q < 2; q++) {
                const float *lv = v->ti_lvec[q];
                float l = sqrtf(lv[0] * lv[0] + lv[1] * lv[1] + lv[2] * lv[2]);
                int ty = q ? c3d_ti_sg.wb_type : c3d_ti_sg.wa_type;
                g += ty == 1 ? (l > 0 ? v->ti_lcol[q][k] * fabsf(lv[1] / l) : 0.0f) : v->ti_lcol[q][k];
            }
        }
        v->ti_gnd[k] = g < 1.0f ? g : 1.0f;
    }
    v->ti_ltype[0] = c3d_ti_sg.wa_type; v->ti_ltype[1] = c3d_ti_sg.wb_type;
    v->ti_dyn_n = 0;
    printf("titanium: lights amb %.3f %.3f %.3f wa %d %.3f %.3f %.3f wb %d %.3f %.3f %.3f ground %.3f %.3f %.3f camo %d %d %d %d %d %d %d %d\n",
           v->ti_amb[0], v->ti_amb[1], v->ti_amb[2], v->ti_ltype[0], v->ti_lcol[0][0], v->ti_lcol[0][1], v->ti_lcol[0][2],
           v->ti_ltype[1], v->ti_lcol[1][0], v->ti_lcol[1][1], v->ti_lcol[1][2], v->ti_gnd[0], v->ti_gnd[1], v->ti_gnd[2],
           c3d_ti_camo[0], c3d_ti_camo[1], c3d_ti_camo[2], c3d_ti_camo[3], c3d_ti_camo[4], c3d_ti_camo[5], c3d_ti_camo[6], c3d_ti_camo[7]);
}
/* the camera-height terms (0x1006a851): f = 1; with density Y != 0, above the ground t = Y / height (Y > 0, low cloud)
 * or -Y x height (Y < 0, ground fog), f = t < M ? M : min(t, 1); at or below it f = M when Y < 0. The fog factor
 * multiplies every vertex's (v->ti_fog[2]); the clear colour is sky x g + fog x (1 - g), g = f, x Y / 50000 when Y > 0,
 * clamped to M..1 */
#if defined(__GNUC__)
__attribute__((unused))
#endif
static void c3d_ti_frame(glr_view *v, float eye_y)
{
    const skygnd_ti *t = &c3d_ti_sg;
    float f = 1.0f, g;
    int k;
    if (!v->ti_fog[0]) return;
    if (t->dens_y != 0) {
        if (eye_y > 0) {
            float q = t->dens_y > 0 ? t->dens_y / eye_y : -t->dens_y * eye_y;
            f = q < t->dens_m ? t->dens_m : q < 1.0f ? q : 1.0f;
        } else if (t->dens_y < 0) f = t->dens_m;
    }
    v->ti_fog[2] = f;
    g = f;
    if (t->dens_y != 0) {
        if (t->dens_y > 0) g = t->dens_y / 50000.0f * f;
        if (g < t->dens_m) g = t->dens_m; else if (g > 1.0f) g = 1.0f;
    } else g = 1.0f;
    for (k = 0; k < 3; k++) v->sky_color[k] = ((float)t->sky[k] * g + (float)t->fog[k] * (1.0f - g)) / 255.0f;
}
#endif
