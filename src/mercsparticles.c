#include "mercsparticles.h"
#include <limits.h>
#include <string.h>

/* Behavioral authority: DOS 298f0/29684/29724,2a0d4/29f38/29fa4;
 * dedicated 3dfx 10042080/10041c50/10041cf0,100427a0/10042730/10042350.
 * See tests/fixtures/particles for instruction-derived fixtures and boundaries. */
static int32_t asr(int32_t x, unsigned bits)
{
    /* Define arithmetic right shift for negative values on every C compiler. */
    return x >= 0 ? x >> bits : (int32_t)(-((-(int64_t)x + ((1u << bits) - 1u)) >> bits));
}
static int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int random_n(mercs_particles *p, int n)
{
    int v;
    if (p->random_value) return p->random_value(p->random_user, n);
    v = p->random_table[p->random_index++];
    if (p->random_index == 127) p->random_index = 0;
    return v % n;
}
static void empty(mercs_particles *p)
{
    int i;
    p->count = 0; p->free_count = MERCS_PARTICLE_EMITTERS;
    for (i = 0; i < MERCS_PARTICLE_EMITTERS; ++i) p->available[i] = (uint16_t)i;
}
void mercs_particles_init(mercs_particles *p, int edition, uint32_t seed)
{
    int i;
    memset(p, 0, sizeof *p);
    p->edition = edition == MERCS_PARTICLES_3DFX ? edition : MERCS_PARTICLES_DOS;
    p->enabled = 1;
    empty(p);
    /* Original 127-entry random lookup pattern (DOS 5c6e0/5c764 and 3dfx
     * 10013800/10013890), using each edition's CRT recurrence. The port keeps
     * this visual stream private: changing PARTICLES cannot alter combat RNG. */
    for (i = 0; i < 127; ++i) {
        seed = p->edition == MERCS_PARTICLES_DOS ? seed * 1103515245u + 12345u : seed * 214013u + 2531011u;
        p->random_table[i] = (uint16_t)((seed >> 16) & 0x7fffu);
    }
}
void mercs_particles_enable(mercs_particles *p, int enabled)
{
    if (!enabled && p->count) empty(p);
    p->enabled = enabled != 0;
}
static int beyond_view(const int32_t origin[3], const int32_t eye[3], int32_t far_distance)
{
    int i; int64_t a[3], largest, sum;
    if (!eye || far_distance <= 0) return 0;
    for (i = 0; i < 3; ++i) { a[i] = (int64_t)origin[i] - eye[i]; if (a[i] < 0) a[i] = -a[i]; }
    largest = a[0] > a[1] ? a[0] : a[1]; if (a[2] > largest) largest = a[2];
    sum = a[0] + a[1] + a[2];
    return (3 * largest + sum) / 4 > far_distance;
}
int mercs_particles_emit(mercs_particles *p, int kind, const int32_t origin[3],
                         const int32_t delta[3], int32_t now,
                         const int32_t eye[3], int32_t far_distance)
{
    mercs_particle_emitter *e;
    int slot, i, j, n; int32_t step[3], offset[3];
    if (!p || !p->enabled || !origin || !delta ||
        (kind != MERCS_PARTICLE_MISSILE && kind != MERCS_PARTICLE_BALLISTIC) ||
        p->count + 1 >= MERCS_PARTICLE_EMITTERS || p->free_count <= 0) return -1;
    if (kind == MERCS_PARTICLE_MISSILE && beyond_view(origin, eye, far_distance)) return -1;
    slot = p->available[--p->free_count];
    e = &p->emitter[slot];
    /* Deliberately retain uninitialized-by-this-producer slots on reuse. In
     * 3dfx, ballistic producer initializes40 entries; shared draw reads64. */
    e->kind = kind; e->born = now;
    memcpy(e->pos, origin, sizeof e->pos);
    n = kind == MERCS_PARTICLE_MISSILE ? (p->edition == MERCS_PARTICLES_DOS ? 16 : 64)
                                      : (p->edition == MERCS_PARTICLES_DOS ? 10 : 40);
    for (j = 0; j < 3; ++j) {
        e->velocity[j] = asr(delta[j], kind == MERCS_PARTICLE_MISSILE ? 3 : 4);
        offset[j] = step[j] = kind == MERCS_PARTICLE_MISSILE ? asr(delta[j], 4) : delta[j] / n;
    }
    for (i = 0; i < n; ++i) {
        for (j = 0; j < 3; ++j) offset[j] = add32(offset[j], step[j]);
        for (j = 0; j < 3; ++j)
            e->offset[i][j] = add32(offset[j], kind == MERCS_PARTICLE_MISSILE ? random_n(p, j == 1 ? 50 : 10) - (j == 1 ? 25 : 5) : 0);
    }
    p->active[p->count++] = (uint16_t)slot;
    return slot;
}
void mercs_particles_step(mercs_particles *p, int32_t now)
{
    int i = 0;
    if (!p || !p->enabled) return;
    while (i < p->count) {
        int slot = p->active[i], j;
        mercs_particle_emitter *e = &p->emitter[slot];
        int64_t age = (int64_t)now - e->born;
        int life = e->kind == MERCS_PARTICLE_MISSILE ? (p->edition == MERCS_PARTICLES_DOS ? 92 : 91) : 46;
        if (age < 0 || age >= life) {
            p->available[p->free_count++] = (uint16_t)slot;
            memmove(p->active + i, p->active + i + 1, (size_t)(--p->count - i) * sizeof *p->active);
            continue;
        }
        for (j = 0; j < 3; ++j) e->pos[j] = add32(e->pos[j], e->velocity[j]);
        if (e->kind == MERCS_PARTICLE_MISSILE) {
            /* Float constants loaded by the originals, evaluated without an
             * intermediate float store, then truncate toward zero. */
            double scale = p->edition == MERCS_PARTICLES_DOS ? (double)(1.0f / 91.0f) : (double)(1.0f / 90.0f);
            int c = (int)((double)age * scale * 17.0);
            e->pos[1] = add32(e->pos[1], random_n(p, 10));
            e->color = (uint8_t)(c < 2 ? 21 : 64 - c / 2);
        } else e->color = 3;
        ++i;
    }
}
int mercs_particles_collect(const mercs_particles *p,
                            int32_t (*depth_for_z)(void *user, int32_t z), void *user,
                            mercs_particle_primitive *out, int capacity)
{
    int i, count = 0;
    if (!p || !p->enabled || !out || capacity <= 0) return 0;
    for (i = 0; i < p->count; ++i) {
        const mercs_particle_emitter *e = &p->emitter[p->active[i]];
        int32_t depth = depth_for_z ? depth_for_z(user, e->pos[2]) : 0;
        int j, k, n = p->edition == MERCS_PARTICLES_3DFX ? 64 : e->kind == MERCS_PARTICLE_MISSILE ? 16 : 10;
        int stride = e->kind == MERCS_PARTICLE_BALLISTIC && depth > 10000 ? 2 : 1;
        int line = e->kind == MERCS_PARTICLE_MISSILE && depth <= 20000;
        for (j = 0; j < n && count < capacity; j += stride) {
            mercs_particle_primitive *v = &out[count++];
            v->line = line; v->color = e->color;
            for (k = 0; k < 3; ++k) {
                int32_t c = add32(e->pos[k], e->offset[j][k]);
                v->pos[k] = (float)c;
                v->end[k] = (float)add32(c, line ? e->velocity[k] / 10 : 0);
            }
        }
    }
    return count;
}
