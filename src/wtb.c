/* wtb.c - see wtb.h. */
#include "wtb.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

static float rdf64(const uint8_t *p)
{
    uint64_t v = 0;
    double d;
    int i;
    for (i = 7; i >= 0; i--) v = (v << 8) | p[i];
    memcpy(&d, &v, sizeof d);
    return (float)d;
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Parse one object at d[*off]; advances *off. */
static int parse_object(const uint8_t *d, size_t len, size_t *off, wtb_object *o, int ext)
{
    size_t vsize = ext ? 40 : 16;
    size_t p = *off;
    int i, k;

    memset(o, 0, sizeof *o);
    if (len - p < 0x20 || memcmp(d + p, "WTBO", 4) != 0) return -1;
    for (i = 0; i < 16; i++) {
        uint8_t c = ext ? d[p + 8 + (size_t)i] : (uint8_t)(0u - d[p + 8 + (size_t)i]);
        if (!c) break;
        o->name[i] = (char)c;
    }
    o->vert_count = rd16(d + p + 0x18);
    o->poly_count = rd16(d + p + 0x1A);
    o->flags = rd16(d + p + 0x1C);
    p += 0x20;

    if ((size_t)o->vert_count * vsize > len - p) return -1;
    o->verts = calloc((size_t)(o->vert_count ? o->vert_count : 1), sizeof *o->verts);
    o->polys = calloc((size_t)(o->poly_count ? o->poly_count : 1), sizeof *o->polys);
    if (!o->verts || !o->polys) return -1;
    for (i = 0; i < o->vert_count; i++, p += vsize) {
        o->verts[i].x = (int32_t)rd32(d + p);
        o->verts[i].y = (int32_t)rd32(d + p + 4);
        o->verts[i].z = (int32_t)rd32(d + p + 8);
        if (ext) {
            for (k = 0; k < 3; k++) o->verts[i].normal[k] = rdf64(d + p + 12 + (size_t)k * 8);
            o->verts[i].a = rd16(d + p + 36);
            o->verts[i].b = rd16(d + p + 38);
        } else {
            o->verts[i].a = rd16(d + p + 12);
            o->verts[i].b = rd16(d + p + 14);
        }
    }
    for (i = 0; i < o->poly_count; i++) {
        wtb_poly *q = &o->polys[i];
        size_t size;
        size_t ioff = ext ? 32 : 4;
        if (len - p < ioff) return -1;
        q->color = rd16(d + p);
        if (ext) {
            q->material = rd16(d + p + 2);
            q->unknown = rd16(d + p + 4);
            for (k = 0; k < 3; k++) q->normal[k] = (float)rdf64(d + p + 6 + (size_t)k * 8);
            q->has_normal = 1;
            q->n = rd16(d + p + 30);
        } else {
            q->n = rd16(d + p + 2);
        }
        if (q->n < 1 || q->n > WTB_MAX_POLY_VERTS) return -1;
        size = ext ? (q->n <= 4 ? 40 : 46) : (q->n <= 4 ? 12 : 18);
        if (len - p < size) return -1;
        for (k = 0; k < q->n; k++) {
            q->idx[k] = rd16(d + p + ioff + (size_t)k * 2);
            if (q->idx[k] >= o->vert_count) return -1;
        }
        p += size;
    }
    *off = p;
    return 0;
}

static int parse_all(const uint8_t *data, size_t len, wtb_model *out, int ext)
{
    size_t off = 0;
    memset(out, 0, sizeof *out);
    out->extended = ext;
    while (off < len) {
        wtb_object *grown = realloc(out->objects, (size_t)(out->object_count + 1) * sizeof *grown);
        if (!grown) { wtb_free(out); return -1; }
        out->objects = grown;
        if (parse_object(data, len, &off, &out->objects[out->object_count], ext) != 0) {
            out->object_count++; /* so wtb_free releases the partial object */
            wtb_free(out);
            return -1;
        }
        out->object_count++;
    }
    return out->object_count > 0 ? 0 : -1;
}

/* DOS layout first; if it doesn't consume the record exactly, the 3D-edition one. */
int wtb_parse(const uint8_t *data, size_t len, wtb_model *out)
{
    if (parse_all(data, len, out, 0) == 0) return 0;
    return parse_all(data, len, out, 1);
}

void wtb_free(wtb_model *m)
{
    int i;
    for (i = 0; i < m->object_count; i++) {
        free(m->objects[i].verts);
        free(m->objects[i].polys);
    }
    free(m->objects);
    memset(m, 0, sizeof *m);
}

/* DOS 0x6304c / 0x45c04: the distance flag stays active after its first
 * occurrence. A 0x2000 translated-sibling stream is a different layout. */
static int32_t lod_key(const wtb_model *m, int object)
{
    int i, active = 0;
    const char *p;
    int64_t value = 0;
    for (i = 0; i <= object; i++) {
        if (m->objects[i].flags & 0x2000) return -1;
        if (m->objects[i].flags & 0x1000) active = 1;
    }
    if (!active) return 0;
    p = strchr(m->objects[object].name, '_');
    if (!p || p[1] < '0' || p[1] > '9') return 0;
    for (++p; *p >= '0' && *p <= '9'; p++) {
        value = value * 10 + *p - '0';
        if (value > INT32_MAX) return INT32_MAX;
    }
    return (int32_t)value;
}

int wtb_lod_interval(const wtb_model *m, int object, int32_t *lower, int32_t *upper)
{
    int i, first = 0, active = 0, previous = 0, after = -1;
    int32_t key, minimum;
    if (!m || !m->objects || object < 0 || object >= m->object_count || !lower || !upper) return 0;
    *lower = *upper = -1;
    for (i = 0; i < m->object_count; i++) {
        if (m->objects[i].flags & 0x2000) return object == 0;
        active |= m->objects[i].flags & 0x1000;
    }
    if (!active || m->object_count == 1) return object == 0;
    minimum = lod_key(m, 0);
    for (i = 1; i < m->object_count; i++) {
        int32_t k = lod_key(m, i);
        if (k < minimum) { minimum = k; first = i; }
    }
    if (minimum == 0) return object == first;
    key = lod_key(m, object);
    for (i = 0; i < m->object_count; i++) {
        int32_t k;
        if (i == object) continue;
        k = lod_key(m, i);
        if (k < key || (k == key && i < object)) previous = 1;
        else if (after < 0 || k < lod_key(m, after) || (k == lod_key(m, after) && i < after)) after = i;
    }
    if (previous) *lower = key;
    if (after >= 0) *upper = lod_key(m, after);
    /* Equal thresholds can suppress an intermediate entry completely. The
     * first still owns all depths below the equal-threshold boundary. */
    return *lower < 0 || *upper < 0 || *lower != *upper;
}

int wtb_lod_select_q16(const wtb_model *m, int32_t depth, int32_t scale)
{
    int i;
    if (!m) return -1;
    for (i = 0; i < m->object_count; i++) {
        int32_t lo, hi;
        int64_t low, high;
        if (!wtb_lod_interval(m, i, &lo, &hi)) continue;
        low = ((int64_t)lo * scale + 0x8000) >> 16;
        high = ((int64_t)hi * scale + 0x8000) >> 16;
        if ((lo < 0 || depth >= (int32_t)(uint32_t)low) &&
            (hi < 0 || depth < (int32_t)(uint32_t)high)) return i;
    }
    return -1;
}

int wtb_lod_select_float(const wtb_model *m, float depth, float scale)
{
    int i;
    if (!m) return -1;
    for (i = 0; i < m->object_count; i++) {
        int32_t lo, hi;
        if (wtb_lod_interval(m, i, &lo, &hi) &&
            (lo < 0 || depth >= (float)lo * scale) &&
            (hi < 0 || depth < (float)hi * scale)) return i;
    }
    return -1;
}

/* Original Mercs polygon plane construction, shared by rendering and contact. */
static int normal_polygon_valid(const wtb_object *o, const wtb_poly *q)
{
    int k;
    if (!o || !q || !o->verts || q->n < 1 || q->n > WTB_MAX_POLY_VERTS) return 0;
    for (k = 0; k < q->n; k++) if (q->idx[k] >= o->vert_count) return 0;
    return 1;
}

static uint64_t normal_magnitude(int64_t v)
{
    return v < 0 ? UINT64_C(0) - (uint64_t)v : (uint64_t)v;
}

/* 0x48003 scales the cross product to bit 29, then normalizes using the
 * floor(sqrt(i)*1024) table initialized by 0x486d4. Its returned area measure
 * is the highest set bit of the OR of the three unsigned magnitudes. */
static int normal_triangle(const wtb_vertex *a, const wtb_vertex *b,
                           const wtb_vertex *c, int32_t out[3])
{
    int64_t u[3] = {(int64_t)b->x - a->x, (int64_t)b->y - a->y, (int64_t)b->z - a->z};
    int64_t v[3] = {(int64_t)c->x - b->x, (int64_t)c->y - b->y, (int64_t)c->z - b->z};
    int64_t cross[3], scaled[3];
    uint64_t bits, square = 0;
    int e = 0, k, denominator;
    /* WTB coordinates are signed 32-bit, as are the original differences.
     * Reject a difference outside that arithmetic range before multiplying. */
    for (k = 0; k < 3; k++)
        if (u[k] < INT32_MIN || u[k] > INT32_MAX || v[k] < INT32_MIN || v[k] > INT32_MAX)
            return -1;
    /* Unsigned subtraction preserves the original 64-bit cross arithmetic
     * without signed overflow for an extreme but valid input difference. */
    cross[0] = (int64_t)((uint64_t)(u[1] * v[2]) - (uint64_t)(v[1] * u[2]));
    cross[1] = (int64_t)((uint64_t)(u[2] * v[0]) - (uint64_t)(v[2] * u[0]));
    cross[2] = (int64_t)((uint64_t)(u[0] * v[1]) - (uint64_t)(v[0] * u[1]));
    bits = normal_magnitude(cross[0]) | normal_magnitude(cross[1]) | normal_magnitude(cross[2]);
    if (!bits) { out[0] = out[1] = out[2] = -1; return 0; }
    while (bits >>= 1) e++;
    for (k = 0; k < 3; k++) {
        scaled[k] = e > 29 ? cross[k] >> (e - 29) : cross[k] * (INT64_C(1) << (29 - e));
        square += (uint64_t)((scaled[k] >> 16) * (scaled[k] >> 16));
    }
    denominator = (int)(sqrt((double)(square >> 20)) * 1024.0);
    if (!denominator) return -1;
    for (k = 0; k < 3; k++) out[k] = (int32_t)(-(scaled[k] * 8192 / denominator));
    return e;
}

int wtb_mercs_normal_q29(const wtb_object *o, const wtb_poly *q, int32_t out[3])
{
    int a, b, k, best = -1;
    int64_t length = 0;
    if (!out || !normal_polygon_valid(o, q)) return -1;
    out[0] = 0x20000000; out[1] = out[2] = 0;
    if (q->n < 3) return 0;
    if (q->n == 3)
        return normal_triangle(&o->verts[q->idx[0]], &o->verts[q->idx[1]],
                               &o->verts[q->idx[2]], out) < 0 ? -1 : 0;
    a = q->idx[0]; b = q->idx[1];
    for (k = q->n - 1; k >= 0; k--) {
        const wtb_vertex *p = &o->verts[q->idx[k]], *next = &o->verts[q->idx[(k + 1) % q->n]];
        int64_t dx = llabs((int64_t)next->x - p->x), dy = llabs((int64_t)next->y - p->y);
        int64_t dz = llabs((int64_t)next->z - p->z), max = dx > dy ? dx : dy, len;
        if (dz > max) max = dz;
        len = (3 * max + dx + dy + dz) >> 2;
        if (len > length) { length = len; a = q->idx[k]; b = q->idx[(k + 1) % q->n]; }
    }
    if (!length) return 0;
    for (k = q->n - 1; k >= 0; k--) {
        int32_t normal[3];
        int area;
        if (q->idx[k] == a || q->idx[k] == b) continue;
        area = normal_triangle(&o->verts[a], &o->verts[b], &o->verts[q->idx[k]], normal);
        if (area < 0) return -1;
        if (area > best) { best = area; memcpy(out, normal, sizeof normal); }
        if (area > 16) break;
    }
    return 0;
}

