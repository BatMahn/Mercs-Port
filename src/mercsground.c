#include "mercsground.h"
#include <math.h>
#include <stdlib.h>

int mercs_ground_tiles(int32_t x, int32_t z, int32_t side, int32_t tiles[4][2])
{
    int tx, tz, dx, dz, q;
    if (side < 2) return 0;
    tx = x / side - (x < 0); tz = z / side - (z < 0);
    dx = x < 0 ? -1 : 1; dz = z < 0 ? -1 : 1;
    q = abs((x % side) / (side / 2)) * 2 + abs((z % side) / (side / 2));
    if (q > 3) return 0;
    if (!(q & 2)) dx = -dx;
    if (!(q & 1)) dz = -dz;
    tiles[0][0] = tx;      tiles[0][1] = tz;
    tiles[1][0] = tx + dx; tiles[1][1] = tz;
    tiles[2][0] = tx + dx; tiles[2][1] = tz + dz;
    tiles[3][0] = tx;      tiles[3][1] = tz + dz;
    return 4;
}

int mercs_ground_shade(int ambient, int delta, int near_cm, int far_cm,
                       int first_clip, int depth_quarter_cm)
{
    int64_t base = ((int64_t)ambient + delta) >> 3;
    int64_t shade, relative;
    if (base < 0) base = 0;
    if (base > 15) base = 15;
    if (far_cm == first_clip) return (int)base;
    relative = (int64_t)depth_quarter_cm - (int64_t)near_cm * 4;
    if (relative < 0) return 15;
    if (far_cm <= near_cm) return (int)base;
    shade = base * ((int64_t)far_cm * 4 - relative) / ((int64_t)(far_cm - near_cm) * 4);
    return shade < 0 ? 0 : shade > 15 ? 15 : (int)shade;
}

static int clip(const mercs_ground_vertex *in, int n, mercs_ground_vertex *out,
                 float depth, int lower)
{
    int i, count = 0;
    for (i = 0; i < n; i++) {
        const mercs_ground_vertex *a = &in[i], *b = &in[(i+1)%n];
        int ia = lower ? a->depth >= depth : a->depth <= depth;
        int ib = lower ? b->depth >= depth : b->depth <= depth;
        if (ia) out[count++] = *a;
        if (ia != ib) {
            float t = (depth - a->depth) / (b->depth - a->depth);
            mercs_ground_vertex *p = &out[count++];
            p->x = a->x + (b->x - a->x) * t; p->z = a->z + (b->z - a->z) * t;
            p->u = a->u + (b->u - a->u) * t; p->v = a->v + (b->v - a->v) * t;
            p->depth = depth; p->shade = 0;
        }
    }
    return count;
}

int mercs_ground_mesh(const mercs_ground *g, const float eye[3], const float forward[3],
                      float near_cm, int ambient, mercs_ground_vertex out[MERCS_GROUND_MAX_VERTICES])
{
    static const int corner_x[4] = {0,1,1,0}, corner_z[4] = {0,0,1,1};
    int32_t tiles[4][2];
    float bands[5];
    int count = 0, nt = 1, nb, band, tile, i, j;
    if (!g || g->mode < 1 || g->mode > 3 || !(g->size >= 2 && g->size <= 1e7f) ||
        !isfinite(eye[0]) || !isfinite(eye[1]) || !isfinite(eye[2]) ||
        fabsf(eye[0]) > 1e9f || fabsf(eye[2]) > 1e9f) return 0;
    bands[0] = fmaxf(near_cm, 1.0f);
    if (g->mode == MERCS_GROUND_TITANIUM) {
        bands[1] = 2500; bands[2] = 8000; bands[3] = 10000000; nb = 3;
    } else {
        if (!(g->clip[0] > 0 && g->clip[1] > g->clip[0] && g->clip[2] > 0)) return 0;
        nt = mercs_ground_tiles((int32_t)eye[0], (int32_t)eye[2], (int32_t)g->size, tiles);
        if (g->mode == MERCS_GROUND_DOS) {
            bands[1] = g->clip[0]; bands[2] = g->clip[1]; nb = 2;
        } else {
            bands[1] = floorf(g->clip[0] * .5f); bands[2] = g->clip[0];
            bands[3] = g->clip[1] - floorf((g->clip[1] - g->clip[0]) * .5f);
            bands[4] = g->clip[1]; nb = 4;
        }
    }
    for (band = 0; band < nb; band++) for (tile = 0; tile < nt; tile++) {
        mercs_ground_vertex p[8], tmp[8], clipped[8];
        float near_z = fmaxf(bands[0], bands[band]), far_z = bands[band+1];
        float x0, z0, side;
        int n;
        if (g->mode != MERCS_GROUND_TITANIUM && far_z == g->clip[0]) far_z += 200;
        if (far_z <= near_z) continue;
        if (g->mode == MERCS_GROUND_TITANIUM) {
            x0 = eye[0] - g->size; z0 = eye[2] - g->size; side = g->size * 2;
        } else {
            x0 = tiles[tile][0] * g->size; z0 = tiles[tile][1] * g->size; side = g->size;
        }
        for (i = 0; i < 4; i++) {
            p[i].x = x0 + corner_x[i] * side; p[i].z = z0 + corner_z[i] * side;
            if (g->mode == MERCS_GROUND_TITANIUM) {
                p[i].u = p[i].x * g->repeats[0] / g->size;
                p[i].v = p[i].z * g->repeats[1] / g->size;
            } else {
                p[i].u = corner_x[i] * g->repeats[0]; p[i].v = corner_z[i] * g->repeats[1];
            }
            p[i].depth = (p[i].x-eye[0])*forward[0] - eye[1]*forward[1] + (p[i].z-eye[2])*forward[2];
            p[i].shade = 0;
        }
        n = clip(p, 4, tmp, near_z, 1);
        if (n < 3) continue;
        n = clip(tmp, n, clipped, far_z, 0);
        if (n < 3) continue;
        for (i = 0; i < n; i++)
            if (g->mode == MERCS_GROUND_DOS)
                clipped[i].shade = (float)mercs_ground_shade(ambient, g->delta, (int)bands[band],
                      (int)bands[band+1], (int)g->clip[0], (int)floorf(clipped[i].depth*4+.5f));
        for (i = 1; i + 1 < n; i++) {
            if (count + 3 > MERCS_GROUND_MAX_VERTICES) return count;
            for (j = 0; j < 3; j++) out[count++] = clipped[j == 0 ? 0 : j == 1 ? i : i+1];
        }
    }
    return count;
}
