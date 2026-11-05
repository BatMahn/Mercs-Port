/* Numeric expectations are executed-original fixtures, not a second copy of
 * the particle formulas. GPU submission and simulator observers are separate
 * tests. No original executable/data files are required by this test. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mercsparticles.h"
#include "../tests/fixtures/particles/original-particles.h"

static mercs_particles p;
static mercs_particle_primitive vertices[MERCS_PARTICLE_PRIMITIVES];
static int random_calls;
static int test_random(void *unused, int n)
{
    int value = (random_calls++ * 37 + 11) & 0x7fff;
    (void)unused;
    return value % n;
}
static int32_t test_depth(void *depth, int32_t z)
{
    (void)z;
    return *(const int *)depth;
}
static void init(int edition)
{
    mercs_particles_init(&p, edition, 1);
    p.random_value = test_random;
    random_calls = 0;
}
static void vec(const int32_t got[3], const int expected[3])
{
    int j;
    for (j = 0; j < 3; ++j) assert(got[j] == expected[j]);
}
static void cases(void)
{
    size_t i;
    for (i = 0; i < sizeof original_particle_cases / sizeof *original_particle_cases; ++i) {
        const original_particle_case *c = &original_particle_cases[i];
        mercs_particle_emitter *e;
        int slot, q, j, k;
        init(c->edition);
        slot = mercs_particles_emit(&p, c->kind, c->origin, c->delta, c->birth, NULL, 0);
        assert(slot >= 0 && p.count == 1);
        e = &p.emitter[slot];
        vec(e->pos, c->position); vec(e->velocity, c->velocity);
        assert(e->born == c->birth && e->color == c->color);
        for (q = 0; q < c->offset_n; ++q) vec(e->offset[q], c->offsets[q]);
        assert(random_calls == c->random_calls);
        for (q = 0; q < 9; ++q) {
            const original_particle_update *u = &c->updates[q];
            mercs_particles_step(&p, c->birth + u->age);
            assert(p.count == u->alive);
            vec(e->pos, u->position);
            assert(e->color == u->color && random_calls == u->random_calls);
        }
        /* The original fixture directly invokes the draw consumer on the last
         * stored emitter, even after expiry; restore only the list membership
         * for this independent draw test, not normal application behavior. */
        p.count = 1; p.active[0] = (uint16_t)slot;
        for (q = 0; q < 4; ++q) {
            const original_particle_draw *d = &c->draws[q];
            int n = mercs_particles_collect(&p, test_depth, (void *)&d->depth,
                                           vertices, MERCS_PARTICLE_PRIMITIVES);
            int index = 0, lines = 0, points = 0;
            assert(n == d->points + d->lines);
            for (j = 0; j < n; ++j) {
                const mercs_particle_primitive *v = &vertices[j];
                assert(v->color == e->color);
                for (k = 0; k < 3; ++k) assert(v->pos[k] == d->vertices[index][k]);
                ++index;
                if (v->line) {
                    ++lines;
                    for (k = 0; k < 3; ++k) assert(v->end[k] == d->vertices[index][k]);
                    ++index;
                } else ++points;
            }
            assert(index + 1 == d->projection_calls);
            assert(lines == d->lines && points == d->points);
        }
    }
    printf("PASS: %zu original particle emission/update cases and 48 original draw/endpoint cases\n", i);
}
static void gates(void)
{
    const int32_t delta[3] = {16, 16, 16}, eye[3] = {0, 0, 0};
    size_t i;
    for (i = 0; i < sizeof original_particle_gates / sizeof *original_particle_gates; ++i) {
        const original_particle_gate *g = &original_particle_gates[i];
        init(g->edition); p.enabled = g->enabled;
        p.count = g->count; p.free_count = MERCS_PARTICLE_EMITTERS - g->count;
        mercs_particles_emit(&p, MERCS_PARTICLE_MISSILE, g->origin, delta, 100, eye, 1000000);
        assert(p.count == g->after && random_calls == g->random_calls);
    }
    printf("PASS: %zu original particle enable/range/capacity gates\n", i);
}
static void native_lifecycle(void)
{
    const int32_t delta[3] = {-333, -177, -499}, origin[3] = {1000, 2000, 3000};
    int slot, next, i; int32_t stale[24][3];
    init(MERCS_PARTICLES_3DFX);
    slot = mercs_particles_emit(&p, MERCS_PARTICLE_MISSILE, origin, delta, 100, NULL, 0);
    memcpy(stale, p.emitter[slot].offset + 40, sizeof stale);
    mercs_particles_step(&p, 191); assert(p.count == 0);
    next = mercs_particles_emit(&p, MERCS_PARTICLE_BALLISTIC, origin, delta, 192, NULL, 0);
    assert(next == slot && !memcmp(stale, p.emitter[next].offset + 40, sizeof stale));
    assert(mercs_particles_collect(&p, NULL, NULL, vertices, 7) == 7);
    mercs_particles_enable(&p, 0);
    assert(!p.count && !mercs_particles_collect(&p, NULL, NULL, vertices, 7));
    assert(mercs_particles_emit(&p, 1, origin, delta, 193, NULL, 0) < 0);
    mercs_particles_enable(&p, 1);
    for (i = 0; i < 999; ++i) assert(mercs_particles_emit(&p, 3, origin, delta, 200, NULL, 0) >= 0);
    assert(mercs_particles_emit(&p, 3, origin, delta, 200, NULL, 0) < 0);
    mercs_particles_step(&p, 246);
    assert(!p.count && p.free_count == MERCS_PARTICLE_EMITTERS);
    for (i = 0; i < 999; ++i) assert(mercs_particles_emit(&p, 3, origin, delta, 247, NULL, 0) >= 0);
    puts("PASS: disable clears, bounded output, complete pool recycle, and retained 3dfx ballistic tail offsets");
}
int main(void)
{
    cases(); gates(); native_lifecycle();
    return 0;
}
