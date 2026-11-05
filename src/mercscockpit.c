#include "mercscockpit.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    wtb_poly *polys;
    int capacity;
} cockpit_object_cache;

struct mercs_cockpit_part_cache {
    wtb_object *objects;
    cockpit_object_cache *object_cache;
    int capacity;
};

static int polygon_valid(const wtb_object *o, const wtb_poly *q)
{
    int k;
    if (!o || !q || !o->verts || q->n < 1 || q->n > WTB_MAX_POLY_VERTS) return 0;
    for (k = 0; k < q->n; k++) if (q->idx[k] >= o->vert_count) return 0;
    return 1;
}

int mercs_cockpit_dos_normal(const wtb_object *o, const wtb_poly *q, int32_t out[3])
{
    return wtb_mercs_normal_q29(o, q, out);
}

int mercs_cockpit_face_visible(const mech3d_part *part, const wtb_object *o,
                              const wtb_poly *q, const float eye[3])
{
    double normal[3], world_normal[3], delta[3], vertex[3], dot = 0;
    int32_t fixed[3];
    const wtb_vertex *p;
    int k;
    if (!part || !eye || !polygon_valid(o, q)) return 0;
    if (q->n < 3) return 1;
    if (q->has_normal) {
        /* Extended WTB normals are negated when the original engine loads
         * them; the parser retains the file values. */
        for (k = 0; k < 3; k++) normal[k] = -(double)q->normal[k];
    } else if (part->ti || part->ti_model) {
        for (k = 0; k < 3; k++) normal[k] = q->normal[k];
    } else {
        if (mercs_cockpit_dos_normal(o, q, fixed) != 0) return 0;
        for (k = 0; k < 3; k++) normal[k] = fixed[k];
    }
    p = &o->verts[q->idx[0]];   /* the plane point remains corner 0 */
    vertex[0] = p->x; vertex[1] = p->y; vertex[2] = p->z;
    if (part->has_rot) {
        for (k = 0; k < 3; k++) {
            const float *r = &part->rot[k * 3];
            world_normal[k] = r[0] * normal[0] + r[1] * normal[1] + r[2] * normal[2];
            delta[k] = r[0] * vertex[0] + r[1] * vertex[1] + r[2] * vertex[2] +
                       mech3d_render_coord(part, k) - eye[k];
        }
    } else {
        double yaw = part->yaw * 0.017453292519943295, c = cos(yaw), s = sin(yaw);
        world_normal[0] = c * normal[0] + s * normal[2];
        world_normal[1] = normal[1];
        world_normal[2] = -s * normal[0] + c * normal[2];
        delta[0] = c * vertex[0] + s * vertex[2] + mech3d_render_coord(part, 0) - eye[0];
        delta[1] = vertex[1] + mech3d_render_coord(part, 1) - eye[1];
        delta[2] = -s * vertex[0] + c * vertex[2] + mech3d_render_coord(part, 2) - eye[2];
    }
    for (k = 0; k < 3; k++) dot += world_normal[k] * delta[k];
    return dot < 0;   /* 0x4d3f8: strict JL; a point on the plane is culled */
}

static int in_head_subtree(const mech3d *m, int node, int head)
{
    int depth;
    if (head < 0) return 0;
    for (depth = 0; depth <= m->skel_count && node >= 0; depth++) {
        int k, parent = -1;
        if (node == head) return 1;
        for (k = 0; k < m->skel_count; k++)
            if (m->skel_index[k] == node) { parent = m->skel_parent[k]; break; }
        if (parent == node) break;
        node = parent;
    }
    return 0;
}

static int reserve_parts(mercs_cockpit_layer *layer, int count)
{
    struct mercs_cockpit_part_cache *cache;
    mech3d_part *parts;
    if (count <= layer->capacity) return 0;
    cache = calloc((size_t)count, sizeof *cache);
    parts = malloc((size_t)count * sizeof *parts);
    if (!cache || !parts) { free(cache); free(parts); return -1; }
    if (layer->capacity) memcpy(cache, layer->cache, (size_t)layer->capacity * sizeof *cache);
    free(layer->cache); free(layer->model.parts);
    layer->cache = cache; layer->model.parts = parts; layer->capacity = count;
    return 0;
}

static int reserve_objects(struct mercs_cockpit_part_cache *part, int count)
{
    cockpit_object_cache *cache;
    wtb_object *objects;
    if (count <= part->capacity) return 0;
    cache = calloc((size_t)count, sizeof *cache);
    objects = malloc((size_t)count * sizeof *objects);
    if (!cache || !objects) { free(cache); free(objects); return -1; }
    if (part->capacity) memcpy(cache, part->object_cache, (size_t)part->capacity * sizeof *cache);
    free(part->object_cache); free(part->objects);
    part->object_cache = cache; part->objects = objects; part->capacity = count;
    return 0;
}

int mercs_cockpit_update(mercs_cockpit_layer *layer, const mech3d *posed, const float eye[3])
{
    return mercs_cockpit_update_render(layer, posed, eye, 0);
}

int mercs_cockpit_update_render(mercs_cockpit_layer *layer, const mech3d *posed,
                               const float eye[3], int fractional_translation)
{
    int i, j;
    if (!layer) return -1;
    layer->model.part_count = layer->pit_parts = 0; layer->head_node = -1;
    if (!posed || !eye || posed->part_count < 0 || (posed->part_count && !posed->parts) ||
        posed->skel_count < 0 || posed->skel_count > MECH3D_MAX_NODES) return -1;
    if (reserve_parts(layer, posed->part_count) != 0) return -1;
    /* 0x1038c selects the first class-0xa0 object, then 0x4bcf0 traverses
     * its node and children. Runtime class and subtree membership differ:
     * 0x449b4 can reset a child to 0x100 after the parent's 0x25754 stamp. */
    for (i = 0; i < posed->part_count; i++)
        if ((posed->parts[i].objtype & 0xf0) == 0xa0) { layer->head_node = posed->parts[i].node; break; }
    for (i = 0; i < posed->part_count; i++) {
        const mech3d_part *src = &posed->parts[i];
        const wtb_model *model = src->ti_model ? src->ti_model : &src->model;
        struct mercs_cockpit_part_cache *cache = &layer->cache[i];
        mech3d_part draw = *src, *dst;
        draw.render_pose_pos = fractional_translation && src->pose_pos_valid;
        if (src->hidden || (src->objtype & 0xf0) == 0x20 || !model->object_count) continue;
        if (model->object_count < 0 || !model->objects || reserve_objects(cache, model->object_count) != 0) goto fail;
        for (j = 0; j < model->object_count; j++) {
            const wtb_object *o = &model->objects[j];
            cockpit_object_cache *oc = &cache->object_cache[j];
            int k, count = 0;
            if (o->poly_count < 0 || (o->poly_count && !o->polys)) goto fail;
            if (o->poly_count > oc->capacity) {
                wtb_poly *p = realloc(oc->polys, (size_t)o->poly_count * sizeof *p);
                if (!p) goto fail;
                oc->polys = p; oc->capacity = o->poly_count;
            }
            for (k = 0; k < o->poly_count; k++)
                if (mercs_cockpit_face_visible(&draw, o, &o->polys[k], eye)) oc->polys[count++] = o->polys[k];
            cache->objects[j] = *o;
            cache->objects[j].polys = oc->polys; cache->objects[j].poly_count = count;
        }
        dst = &layer->model.parts[layer->model.part_count++];
        *dst = draw;
        dst->model = *model; dst->model.objects = cache->objects; dst->ti_model = NULL;
        dst->ti = src->ti || src->ti_model != NULL;
        dst->dos_pit = in_head_subtree(posed, src->node, layer->head_node);
        layer->pit_parts += dst->dos_pit;
    }
    return 0;
fail:
    layer->model.part_count = layer->pit_parts = 0;
    return -1;
}

void mercs_cockpit_free(mercs_cockpit_layer *layer)
{
    int i, j;
    if (!layer) return;
    for (i = 0; i < layer->capacity; i++) {
        struct mercs_cockpit_part_cache *p = &layer->cache[i];
        for (j = 0; j < p->capacity; j++) free(p->object_cache[j].polys);
        free(p->object_cache); free(p->objects);
    }
    free(layer->cache); free(layer->model.parts);
    memset(layer, 0, sizeof *layer); layer->head_node = -1;
}
