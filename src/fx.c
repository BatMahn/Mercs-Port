/*
 * fx.c - weapon effects (see fx.h).
 */
#include "fx.h"
#include "wtable.h"
#include "bwd.h"
#include "wtb.h"
#include "mercsfx_data.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int timing_dos = 1;
void fx_set_timing(int dos) { timing_dos = dos != 0; }
static const int *parameters(int type)
{
    return type >= 0 && type < FX_TYPES ? mercs_fx_windows[type] : NULL;
}

/* Mercs firing loop 0x100623f9..0x1006241b: +0x18 and +0x1c. */
int fx_muzzle(int weapon, int k) { return weapon >= 0 && weapon < SIM_WEAPON_COUNT && (k == 0 || k == 1) ? sim_weapons[weapon].muzzle[k] : -1; }

int fx_duration(int type) { const int *p = parameters(type); return p ? (timing_dos ? mercs_fx_dos_duration[type] : p[0]) : 0; }
int fx_debris_duration(void) { return timing_dos ? 3640 : 3620; }
int fx_sound(int type) { const int *p = parameters(type); return p ? p[2] : -1; }
int fx_light(int type) { const int *p = parameters(type); return p ? p[4] : 0; }
int fx_palette_index(int type) { const int *p = parameters(type); return p ? p[1] : -1; }
int fx_local_light(int visual) { return visual == 3 || visual == 6 || visual == 34 || visual == 35; }
int fx_sound_roll(int type, int roll)
{
    const int *p = parameters(type);
    return p && (p[3] == -1 || (roll >= 0 && roll < p[3])) ? p[2] : -1;
}
int fx_damage_q16(int type) { const int *p = parameters(type); return p && p[6] ? p[8] : 0; }
int fx_heat_q16(int type) { const int *p = parameters(type); return p && p[6] ? p[9] : 0; }
int fx_always_splash(int type) { return type == 22 || type == 35; }
int fx_attaches(int type) { const int *p = parameters(type); return p && (p[10] & 1); }
int fx_initial_life(int type, int attached, int frame_ticks)
{
    /* DOS 0x64f72..0x64f7c / Win0x1004b117..0x1004b121: an unattached type10
     * has a one-second life. A valid unit node keeps the full table life. */
    return (fx_attaches(type) && !attached ? (timing_dos ? 182 : 181) : fx_duration(type)) + frame_ticks;
}
int fx_thing_damage(int coefficient, int frame_ticks)
{
    int64_t q = (int64_t)coefficient * frame_ticks + 32768;
    return q >= 0 ? (int)(q / 65536) : (int)(-((-q + 65535) / 65536));
}
float fx_area_radius(int type, float radius, int elapsed_ticks)
{
    /* DOS 0x65262 / Windows 0x1004b5d7: elapsed = duration - remaining.
     * Creation adds the current delta to life, so the first radius is negative.
     * The originals do not clamp it before passing it to the area consumers. */
    return type == 35 ? (float)elapsed_ticks * 24.0f : radius;
}

static int rd16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }

int fx_impact_type(int visual, int flags)
{
    int t = visual & 0xFF;
    if (visual < 0 || t >= FX_TYPES) return -1;
    switch (t) {
    case 0: case 1: case 2:   /* lasers */
        if (flags & FX_HIT_PLAYER) t = 0x0e + t;
        else if (flags & FX_HIT_GROUND) t = 9;
        break;
    case 3: case 4: case 34: case 35: /* Mercs missile dispatch includes the two new blast visuals. */
        if (flags & FX_HIT_GROUND) t = 0x0c;
        else if (flags & FX_HIT_PLAYER) t = 0x11;
        else if (flags & FX_HIT_STRUCT) t = t == 4 ? 0x14 : 0x13;
        break;
    case 5:                   /* bullets */
        if (flags & FX_HIT_UNIT) t = 8;
        else if (flags & FX_HIT_GROUND) t = 9;
        else if (flags & FX_HIT_PLAYER) t = 0x12;
        break;
    case 6:                   /* PPC */
        if (flags & FX_HIT_GROUND) t = 0x15;
        break;
    case 0x0b:                /* Mercs 0x1004ab46 -> 0x1004be92: chunks, no ordinary effect */
        return -1;
    default:
        break;
    }
    return t;
}
int fx_impact_chunks(int visual, int flags)
{
    if (visual < 0 || (visual & 0xff) != 11) return 0;
    if (flags & FX_HIT_GAUSS) return 16;
    return flags & (FX_HIT_UNIT | FX_HIT_STRUCT) ? 4 : 8;
}

/* a 0x8000 / 0xb000 card's polygons (0x10026e80), not the 0x3000 billboard: pool slot si of the type */
static void card_polys(fx_defs *d, int type, int si, const wtb_object *o, const float sc3[3])
{
    int pq, vq, cq;
    if (o->poly_count <= 0 || !(o->polys[0].color & 0x8000)) return;
    for (pq = 0; pq < o->poly_count && d->card_n[type][si] < 2; pq++) {
        const wtb_poly *pp = &o->polys[pq];
        int ci = d->card_n[type][si];
        if (pp->n < 3 || pp->n > 4) continue;
        for (vq = 0; vq < pp->n; vq++) {
            const wtb_vertex *vv = &o->verts[pp->idx[vq]];
            d->card_v[type][si][ci][vq][0] = (float)vv->x * sc3[0];
            d->card_v[type][si][ci][vq][1] = (float)vv->y * sc3[1];
            d->card_v[type][si][ci][vq][2] = (float)vv->z * sc3[2];
            d->card_uv[type][si][ci][vq][0] = (float)vv->a / 64.0f;
            d->card_uv[type][si][ci][vq][1] = (float)vv->b / 64.0f;
        }
        if (pp->n == 3) for (cq = 0; cq < 3; cq++) { d->card_v[type][si][ci][3][cq] = d->card_v[type][si][ci][2][cq]; if (cq < 2) d->card_uv[type][si][ci][3][cq] = d->card_uv[type][si][ci][2][cq]; }
        for (cq = 0; cq < 3; cq++) d->card_nrm[type][si][ci][cq] = -pp->normal[cq];
        d->card_nv[type][si][ci] = pp->n;
        d->card_n[type][si]++;
    }
}

int fx_load(prj_archive *a, fx_defs *d)
{
    prj_record r;
    bwd_chunk *c;
    int n, k, model = -1, scale = 1, run[FX_FRAMES] = {0}, nrun = 0, run_closed = 0;
    float sc3[3] = {1, 1, 1};
    memset(d, 0, sizeof *d);
    for (k = 0; k < FX_TYPES; k++) {
        int si;
        d->anim[k] = -1;
        for (si = 0; si < FX_CARD_SLOTS; si++) d->control[k][si] = d->render[k][si] = -1;
    }

    /* frames: mw2_xpl1 */
    if (prj_read_named(a, "BWD", "MW2_XPL1", &r) == 0) {
        n = bwd_chunks(r.data, r.size, NULL, 0);
        c = n > 0 ? calloc((size_t)n, sizeof *c) : NULL;
        if (c) {
            bwd_chunks(r.data, r.size, c, n);
            for (k = 0; k < n; k++) {
                if (strcmp(c[k].tag, "BMPJ") == 0 && c[k].size >= 2) {
                    if (run_closed) { nrun = 0; run_closed = 0; memset(run, 0, sizeof run); }
                    if (nrun < FX_FRAMES) run[nrun++] = (uint16_t)rd16(c[k].data);
                } else if (strcmp(c[k].tag, "BMID") == 0 && c[k].size >= 2) {
                    fx_anim *an = &d->tex[(uint16_t)rd16(c[k].data) & 0x1FF];
                    memcpy(an->cel, run, sizeof run);
                    an->frames = nrun;
                    an->bound = nrun > 0;
                    an->mode = 1;
                    an->rate = nrun > 1 ? 45 : 0;
                    run_closed = 1;
                } else if (strcmp(c[k].tag, "BSEC") == 0 && c[k].size >= 4) {
                    int slot = (uint16_t)rd16(c[k].data) & 0x1FF, rate = (uint16_t)rd16(c[k].data + 2);
                    d->tex[slot].rate = rate ? rate : 45;
                    d->tex[slot].mode = 1;
                } else if (strcmp(c[k].tag, "BMEN") == 0 && c[k].size >= 4) {
                    int slot = (uint16_t)rd16(c[k].data) & 0x1FF;
                    if (d->tex[slot].bound) d->tex[slot].mode = rd16(c[k].data + 2);
                    if (!d->tex[slot].rate) d->tex[slot].rate = 45;
                }
            }
            free(c);
        }
        prj_record_free(&r);
    }

    /* pool objects: MW2_SHT1 (OBJ then XPLO) */
    if (prj_read_named(a, "BWD", "MW2_SHT1", &r) != 0) return -1;
    n = bwd_chunks(r.data, r.size, NULL, 0);
    c = n > 0 ? calloc((size_t)n, sizeof *c) : NULL;
    if (!c) { prj_record_free(&r); return -1; }
    bwd_chunks(r.data, r.size, c, n);
    for (k = 0; k < n; k++) {
        if (strcmp(c[k].tag, "OBJ") == 0 && c[k].size >= 52) {
            model = (uint16_t)rd16(c[k].data + 48);
            scale = (int32_t)(c[k].data[6] | c[k].data[7] << 8 | c[k].data[8] << 16 | (uint32_t)c[k].data[9] << 24);   /* OBJ scale x */
            if (scale < 1) scale = 1;
            {
                int q;
                for (q = 0; q < 3; q++) {
                    const uint8_t *d3 = c[k].data + 6 + 4 * q;
                    sc3[q] = (float)(int32_t)(d3[0] | d3[1] << 8 | d3[2] << 16 | (uint32_t)d3[3] << 24);
                    if (sc3[q] == 0) sc3[q] = 1;   /* 0x100420b0: 0 -> 1 */
                }
            }
        }
        else if (strcmp(c[k].tag, "XPLO") == 0 && c[k].size >= 6) {
            int texslot = rd16(c[k].data + 2), type = rd16(c[k].data + 4);
            int si;
            if (type < 0 || type >= FX_TYPES) continue;
            si = d->slots[type]++;
            if (si >= FX_CARD_SLOTS) continue;
            d->control[type][si] = texslot >= 0 && texslot < 512 ? texslot : -1;
            d->render[type][si] = d->control[type][si];
            if (si > 0 && model >= 0) {   /* every pool object has its own polygon texture binding */
                prj_record mr;
                if (prj_read(a, prj_find_type(a, "POLY"), model, &mr) == 0) {
                    wtb_model m;
                    if (wtb_parse(mr.data, mr.size, &m) == 0) {
                        if (m.object_count > 0) {
                            const wtb_object *o = &m.objects[0];
                            if (o->poly_count > 0) {
                                unsigned color = o->polys[0].color;
                                if (color & 0x8000) d->render[type][si] = 0x100 | (color & 0xff);
                                else if ((color & 0xf000) == 0x3000) d->render[type][si] = (color >> 4) & 0xff;
                            }
                            card_polys(d, type, si, o, sc3);
                        }
                        wtb_free(&m);
                    }
                    prj_record_free(&mr);
                }
            }
            if (si == 0) {
                prj_record mr;
                d->anim[type] = texslot >= 0 && texslot < 512 ? texslot : -1;
                if (model >= 0 && prj_read(a, prj_find_type(a, "POLY"), model, &mr) == 0) {   /* card size */
                    wtb_model m;
                    if (wtb_parse(mr.data, mr.size, &m) == 0 && m.object_count > 0) {
                        const wtb_object *o = &m.objects[0];
                        int mnx = 1 << 30, mxx = -(1 << 30), mny = 1 << 30, mxy = -(1 << 30), v;
                        for (v = 0; v < o->vert_count; v++) {
                            if (o->verts[v].x < mnx) mnx = o->verts[v].x;
                            if (o->verts[v].x > mxx) mxx = o->verts[v].x;
                            if (o->verts[v].y < mny) mny = o->verts[v].y;
                            if (o->verts[v].y > mxy) mxy = o->verts[v].y;
                        }
                        if (o->vert_count) { d->half_w[type] = (float)(mxx - mnx) * 0.5f * (float)scale; d->half_h[type] = (float)(mxy - mny) * 0.5f * (float)scale; }
                        if (o->poly_count > 0 && (o->polys[0].color & 0xf000) == 0x3000) {   /* 0x10026fc7 -> 0x1002a8e0 */
                            int ft = -1, tp = -1, q;
                            for (q = 0; q < o->polys[0].n; q++) {
                                const wtb_vertex *vv = &o->verts[o->polys[0].idx[q]];
                                if (vv->a != 0) continue;
                                if (vv->b == 0) tp = o->polys[0].idx[q]; else ft = o->polys[0].idx[q];
                            }
                            if (ft >= 0 && tp >= 0) {
                                d->bb[type] = 1;
                                d->bb_foot[type][0] = (float)(o->verts[ft].x * scale); d->bb_foot[type][1] = (float)(o->verts[ft].y * scale); d->bb_foot[type][2] = (float)(o->verts[ft].z * scale);
                                d->bb_top[type][0] = (float)(o->verts[tp].x * scale); d->bb_top[type][1] = (float)(o->verts[tp].y * scale); d->bb_top[type][2] = (float)(o->verts[tp].z * scale);
                            }
                        }
                        /* the card's colour picks its texture slot: bit 15 set -> 0x100 + low byte (DEATH0,
                         * JET0, MZL0); otherwise bits 4-11 (LAS0, XPL0, PPC0, BUL0: equal to XPLO's field) */
                        if (o->poly_count > 0 && (o->polys[0].color & 0x8000)) d->anim[type] = 0x100 | (o->polys[0].color & 0xFF);
                        else if (o->poly_count > 0 && (o->polys[0].color & 0xf000) == 0x3000) d->anim[type] = (o->polys[0].color >> 4) & 0xff;
                        d->render[type][0] = d->anim[type];
                        card_polys(d, type, 0, o, sc3);
                        wtb_free(&m);
                    }
                    prj_record_free(&mr);
                }
            }
        }
    }
    free(c);
    prj_record_free(&r);
    return 0;
}

int fx_bind_scene(prj_archive *models, const char *scene, fx_defs *d)
{
    bwd_mission mission;
    fx_anim *slots;
    int result;
    if (!models || !scene || !d || bwd_mission_load(models, scene, &mission)) return -1;
    slots = calloc(512, sizeof *slots);
    if (!slots) { bwd_mission_free(&mission); return -1; }
    result = mercs_texmap_read(models, &mission, slots);
    if (!result) {
        memcpy(d->tex, slots, sizeof d->tex);
        d->scene_bound = 1;
    }
    free(slots);
    bwd_mission_free(&mission);
    return result;
}

int fx_detail_low;
int32_t fx_near_distance(int32_t x, int32_t y, int32_t z)
{
    int32_t swap, sum;
    /* Windows0x1000fc30; inlined by DOS0x64ad8. Preserve its signed
     * comparisons, 32-bit arithmetic and arithmetic-right-shift rounding. */
    if (x < 0) x = (int32_t)(0u - (uint32_t)x);
    if (y < 0) y = (int32_t)(0u - (uint32_t)y);
    if (z < 0) z = (int32_t)(0u - (uint32_t)z);
    if (x < y) { swap = x; x = y; y = swap; }
    if (x < z) { swap = x; x = z; z = swap; }
    sum = (int32_t)((uint32_t)x * 4u + (uint32_t)y + (uint32_t)z);
    return sum >= 0 ? sum / 4 : -(int32_t)((-(int64_t)sum + 3) / 4);
}

void fx_clocks_init(fx_world *w, const fx_defs *d)
{
    int slot, type;
    for (slot = 0; slot < 512; slot++) {
        mercs_texclock *c = &w->clocks[slot];
        c->frame = 0; c->rate = d->tex[slot].rate; c->mode = d->tex[slot].mode;
        c->state = d->tex[slot].bound ? 1 : -2; c->stamp = -1;
    }
    /* XPLO loader calls the mode setter only for positive control ids. */
    for (type = 0; !d->scene_bound && type < FX_TYPES; type++) for (slot = 0; slot < d->slots[type] && slot < FX_CARD_SLOTS; slot++) {
        int control = d->control[type][slot];
        if (control > 0 && control < 512) mercs_texclock_mode(&w->clocks[control], 0);
    }
    w->definitions = d;
    w->clocks_ready = 1;
}

int fx_spawn(fx_world *w, const fx_defs *d, int type, const float pos[3], int32_t now, int frame_ticks)
{
    int k, alive = 0, near = 0, free_i = -1, slot;
    uint32_t used = 0;
    if (type < 0 || type >= FX_TYPES || d->slots[type] <= 0 || fx_duration(type) <= 0 || frame_ticks < 0) return -1;
    if (!w->clocks_ready || w->definitions != d) fx_clocks_init(w, d);
    for (k = 0; k < FX_LIVE; k++) {
        const fx_live *e = &w->e[k];
        if (!e->alive) { if (free_i < 0) free_i = k; continue; }
        if (e->type != type) continue;
        alive++;
        if (e->slot >= 0 && e->slot < 32) used |= 1u << e->slot;
        if ((type < 23 || type > 33) && !(mercs_fx_windows[type][10] & 2)) {
            /* Mercs 0x1004aef5..0x1004af6d: 23..33 and flag2 skip the
             * proximity gate; other types use the integer distance helper. */
            int32_t delta[3];
            int axis;
            for (axis = 0; axis < 3; axis++)
                delta[axis] = (int32_t)((uint32_t)(int32_t)e->pos[axis] - (uint32_t)(int32_t)pos[axis]);
            if (fx_near_distance(delta[0], delta[1], delta[2]) < 500 && ++near > (fx_detail_low ? 0 : 2)) return -1;
        }
    }
    if (alive >= d->slots[type] || alive >= FX_CARD_SLOTS || free_i < 0) return -1;
    for (slot = 0; slot < 31 && (used >> slot & 1); slot++) {}   /* 0x10045d00: the type's first free pool entry (pool order) */
    w->e[free_i].slot = slot;
    w->e[free_i].type = type;
    w->e[free_i].source_visual = type;
    w->e[free_i].alive = 1;
    if (++w->next_serial == 0) ++w->next_serial;
    w->e[free_i].serial = w->next_serial;
    w->e[free_i].light_marked = 0;
    w->e[free_i].has_rot = 0;
    w->e[free_i].attach_actor = -2;
    w->e[free_i].attach_node = -1;
    memcpy(w->e[free_i].pos, pos, sizeof w->e[free_i].pos);
    (void)now;
    w->e[free_i].texture_slot = d->render[type][slot];
    w->e[free_i].control_slot = d->control[type][slot];
    if (w->e[free_i].control_slot >= 0 && w->e[free_i].control_slot < 512) {
        int control = w->e[free_i].control_slot;
        mercs_texclock_reset(&w->clocks[control], d->tex[control].cel, 0);
        mercs_texclock_mode(&w->clocks[control], parameters(type)[7]);
    }
    /* DOS 0x64e58..0x64e62, Windows 0x1004aff1..0x1004affc. */
    w->e[free_i].remaining = fx_initial_life(type, 0, frame_ticks);
    return free_i;
}

int fx_palette_begin(fx_world *w, int duration, int mode, int frame_ticks)
{
    int frames;
    if (!w || (w->palette_left > 0 && mode != 0)) return 0;
    if (frame_ticks == 0) frames = 20;
    else {
        if (mode == 1) duration >>= 1;
        frames = frame_ticks > 0 ? duration / frame_ticks : 0;
        if (frames < 1) frames = 1;
    }
    w->palette_left = mode == 2 ? 1 : frames;
    w->palette_back = mode == 1 || mode == 2 ? frames : 0;
    return 1;
}

static int palette_valid(const fx_palette *p, int slot)
{
    return p->bank && p->valid && slot >= 0 && slot < FX_PALETTE_SLOTS && p->valid[slot];
}

int fx_palette_bind(fx_world *w, const uint8_t bank[FX_PALETTE_SLOTS][FX_PALETTE_COMPONENTS],
                    const int valid[FX_PALETTE_SLOTS], int initial_slot,
                    void (*submit)(void *user, const uint8_t *rgb6), void *user)
{
    if (!w || !bank || !valid || initial_slot < 0 || initial_slot >= FX_PALETTE_SLOTS ||
        !valid[initial_slot]) return 0;
    memset(&w->palette, 0, sizeof w->palette);
    w->palette.bank = bank; w->palette.valid = valid;
    w->palette.submit = submit; w->palette.user = user;
    w->palette.current = initial_slot;
    w->palette.base = w->palette.queued = initial_slot;
    w->palette_left = w->palette_back = 0;
    w->external_fade = 0;
    return 1;
}

int fx_palette_request(fx_world *w, int slot, int duration, int mode, int frame_ticks)
{
    fx_palette *p;
    const uint8_t *source;
    int back_slot, k;
    if (!w || mode < 0 || mode > 2 || (w->palette_left > 0 && mode != 0)) return 0;
    p = &w->palette;
    if (!palette_valid(p, slot)) return 0;
    if (w->palette_left > 0) {
        if (!p->buffer_present) return 0;
        source = p->rgb; back_slot = -1;
    } else {
        if (!palette_valid(p, p->current)) return 0;
        source = p->bank[p->current]; back_slot = p->current;
    }
    /* The original constructor copies the current buffer before freeing it
     * during mode0 interruption. Embedded storage needs no allocation/free. */
    for (k = 0; k < FX_PALETTE_COMPONENTS; k++) {
        int delta = (int)p->bank[slot][k] - (int)source[k];
        p->rgb[k] = source[k];
        p->delta[k] = (int8_t)(delta >= 128 ? delta - 256 : delta < -128 ? delta + 256 : delta);
        p->remainder[k] = 0;
    }
    fx_palette_begin(w, duration, mode, frame_ticks);
    p->want = slot; p->back_slot = back_slot;
    p->frames = w->palette_back > 0 ? w->palette_back : w->palette_left;
    p->step = 0; p->buffer_present = 1;
    return 1;
}

void fx_palette_queue(fx_world *w, int slot)
{
    if (!w || !w->palette.bank || slot < 0 || slot >= FX_PALETTE_SLOTS) return;
    /* DOS4ddcc/Windows10060a30 intentionally leave dirty unchanged. */
    w->palette.base = w->palette.queued = slot;
}

void fx_palette_flush(fx_world *w)
{
    fx_palette *p;
    if (!w || !(p = &w->palette)->bank || !p->dirty || w->palette_left > 0) return;
    p->dirty = 0;
    if (palette_valid(p, p->queued) && p->submit) p->submit(p->user, p->bank[p->queued]);
    p->current = p->queued;
}

void fx_palette_step(fx_world *w, int frame_ticks)
{
    if (!w || w->palette_left <= 0) return;
    if (w->palette.bank) {
        fx_palette *p = &w->palette;
        int k;
        if (p->buffer_present && p->frames > 0 && p->step < p->frames) {
            /* Original signed-byte deltas and signed16 remainders: each
             * component truncates toward zero, retaining its own remainder. */
            for (k = 0; k < FX_PALETTE_COMPONENTS; k++) {
                int r = p->remainder[k] + p->delta[k];
                if (r > 32767) r -= 65536;
                else if (r < -32768) r += 65536;
                p->rgb[k] = (uint8_t)((int)p->rgb[k] + r / p->frames);
                p->remainder[k] = (int16_t)(r % p->frames);
            }
            /* The submission observes the old step/left counters. */
            if (p->submit) p->submit(p->user, p->rgb);
            if (++p->step >= p->frames) p->buffer_present = 0;
        }
        if (--w->palette_left == 0) {
            p->current = p->want;
            if (w->palette_back > 0) {
                int duration = w->palette_back, target = p->back_slot;
                fx_palette_request(w, target, duration, 0, frame_ticks);
            } else p->dirty = 1;
        }
    } else if (--w->palette_left == 0 && w->palette_back > 0) {
        /* The original passes its saved FRAME COUNT as the return duration
         * to the same ticks-to-frames function. Both retail engines do this. */
        int duration = w->palette_back;
        fx_palette_begin(w, duration, 0, frame_ticks);
    }
}

static int32_t light_distance(const float point[3], const float camera[3])
{
    int32_t x = (int32_t)((uint32_t)(int32_t)point[0] - (uint32_t)(int32_t)camera[0]);
    int32_t z = (int32_t)((uint32_t)(int32_t)point[2] - (uint32_t)(int32_t)camera[2]);
    return (int32_t)((uint32_t)((int64_t)x*x) + (uint32_t)((int64_t)z*z));
}

static float light_distance_float(const float point[3], const float camera[3])
{
    /* Titanium10012589..100125de: each difference is stored as float;
     * x87 products/sum precede ONE final float store and comparison. */
    float x = point[0] - camera[0], z = point[2] - camera[2];
    return (float)((double)x * (double)x + (double)z * (double)z);
}

int fx_light_offer_point(fx_world *w, int slot, const float camera[3], int allowed,
                         int palette_available, int frame_ticks,
                         const float light_point[3], int float_coordinates)
{
    fx_live *e;
    if (!w || !camera || slot < 0 || slot >= FX_LIVE || !allowed ||
        !(e = &w->e[slot])->alive || !fx_light(e->type)) return 0;
    if (w->light_slot > 0) {
        if (float_coordinates) {
            if (!(light_distance_float(e->pos, camera) < light_distance_float(w->light_pos, camera))) return 0;
        } else if (light_distance(e->pos, camera) >= light_distance(w->light_pos, camera)) return 0;
    }
    w->light_slot = slot + 1;
    e->light_marked = 1;
    memcpy(w->light_pos, light_point ? light_point : e->pos, sizeof w->light_pos);
    if (fx_palette_index(e->type) >= 0 && palette_available && !w->external_fade) {
        if (w->palette.bank)
            fx_palette_request(w, w->palette.base + fx_palette_index(e->type), fx_duration(e->type), 1, frame_ticks);
        else fx_palette_begin(w, fx_duration(e->type), 1, frame_ticks);
    }
    return 1;
}

int fx_light_offer(fx_world *w, int slot, const float camera[3], int allowed, int palette_available, int frame_ticks)
{
    return fx_light_offer_point(w, slot, camera, allowed, palette_available, frame_ticks, NULL, 0);
}

int fx_retains(const fx_world *w, int slot, unsigned serial)
{
    return w && slot >= 0 && slot < FX_LIVE && w->e[slot].alive && w->e[slot].serial == serial &&
           w->e[slot].light_marked && w->light_slot == slot + 1 &&
           (w->palette_left > 0 || w->external_fade);
}

void fx_update(fx_world *w, int frame_ticks)
{
    int k;
    if (frame_ticks < 0) return;
    for (k = 0; k < FX_LIVE; k++) if (w->e[k].alive) {
        w->e[k].remaining -= frame_ticks;
        if (w->e[k].remaining <= 0 && !fx_retains(w, k, w->e[k].serial)) {
            int control = w->e[k].control_slot;
            if (w->clocks_ready && w->definitions && control >= 0 && control < 512) {
                mercs_texclock_mode(&w->clocks[control], 0);
                mercs_texclock_reset(&w->clocks[control], w->definitions->tex[control].cel, 0);
            }
            w->e[k].remaining = 0;
            w->e[k].alive = 0;
            if (w->e[k].light_marked && w->light_slot == k + 1) w->light_slot = 0;
            w->e[k].light_marked = 0;
        }
    }
}

void fx_animate(fx_world *w, const fx_defs *d, int32_t now)
{
    int k;
    if (!w->clocks_ready || w->definitions != d) fx_clocks_init(w, d);
    for (k = 0; k < 512; k++) mercs_texclock_step(&w->clocks[k], d->tex[k].cel, now);
}

int fx_frame(const fx_world *w, const fx_defs *d, const fx_live *e)
{
    const fx_anim *an;
    const mercs_texclock *c;
    if (!e->alive || !w->clocks_ready || e->texture_slot < 0 || e->texture_slot >= 512) return -1;
    c = &w->clocks[e->texture_slot]; an = &d->tex[e->texture_slot];
    if (c->state < 0 || !c->mode || c->frame < 0 || c->frame >= FX_FRAMES || an->cel[c->frame] <= 0) return -1;
    return an->cel[c->frame];
}
