/* This validation-only translation unit replaces msim.o in the playthrough
 * executable. Including the unchanged implementation lets the observer use
 * its exact private geometry primitives without exposing a production API.
 * The shipped executable continues compiling src/msim.c directly. */
#include "../src/msim.c"
#include "mercs_playthrough_geometry.h"

#ifndef MERCS_PLAYTHROUGH_GEOMETRY_CALLOC
#define MERCS_PLAYTHROUGH_GEOMETRY_CALLOC calloc
#endif

#ifndef MERCS_PLAYTHROUGH_GEOMETRY_MALLOC
#define MERCS_PLAYTHROUGH_GEOMETRY_MALLOC malloc
#endif

int mercs_playthrough_world_obstruction(const msim *s, const float from[3],
                                      const float to[3],
                                      mercs_playthrough_world_ray *out)
{
    const terrain_t *live;
    terrain_t terrain;
    msim *view;
    float delta[3], length, distance = -1.0f, normal[3] = {0, 0, 0};
    size_t stamp_count;
    int i, ci = -1, hit;
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->hit_part = out->hit_collision_type = out->hit_thing = -1;
    out->hit_distance_cm = -1.0f;
    if (!s || !s->terrain || !s->world || !from || !to) return -1;
    for (i = 0; i < 3; i++) {
        if (!isfinite(from[i]) || !isfinite(to[i]) ||
            fabsf(from[i]) > 100000000.0f || fabsf(to[i]) > 100000000.0f) return -1;
        delta[i] = to[i] - from[i];
    }
    length = sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (!(length > 0.0f) || !isfinite(length)) return -1;
    live = (const terrain_t *)s->terrain;
    if (live->nparts < 0 || live->nco < 0 ||
        (live->nco && (!live->co || !live->nparts))) return -1;
    stamp_count = (size_t)(live->nparts ? live->nparts : 1);
    if (stamp_count > SIZE_MAX / sizeof *terrain.stamp) return -1;
    terrain = *live;
    terrain.stamp = MERCS_PLAYTHROUGH_GEOMETRY_CALLOC(stamp_count, sizeof *terrain.stamp);
    if (!terrain.stamp) return -1;
    terrain.stamp_now = 0;
    view = MERCS_PLAYTHROUGH_GEOMETRY_MALLOC(sizeof *view);
    if (!view) { free(terrain.stamp); return -1; }
    memcpy(view, s, sizeof *view); view->terrain = &terrain;

    /* This chain writes only the private stamp storage/counter. Keep every
     * collision result local; g_block_*, g_hit_* and g_impact_* stay untouched. */
    hit = clist_seg(view, from, to, 0, &distance, normal, &ci);
    if (hit) {
        out->hit_part = terrain.co[ci].part;
        out->hit_collision_type = terrain.co[ci].type;
        out->hit_thing = shot_thing(view, out->hit_part);
        out->hit_distance_cm = distance;
        memcpy(out->normal, normal, sizeof out->normal);
        for (i = 0; i < 3; i++) out->hit_point[i] = from[i] + delta[i] * (distance / length);
    }
    memcpy(out->from, from, sizeof out->from); memcpy(out->to, to, sizeof out->to);
    out->segment_length_cm = length;
    out->blocked = hit; out->available = 1;
    free(view); free(terrain.stamp);
    return 0;
}

static void playthrough_geometry_obstruction(const msim *view,
                                             const float feet[3],
                                             mercs_playthrough_direction *d)
{
    float p0[3] = {feet[0], feet[1] + view->player_centre_h, feet[2]};
    float p1[3] = {d->position[0], p0[1], d->position[1]};
    float push[3], normal[3], distance;
    float dx = p1[0] - p0[0], dz = p1[2] - p0[2];
    float length = sqrtf(dx * dx + dz * dz);
    const terrain_t *terrain = (const terrain_t *)view->terrain;
    int ci;
    d->hit_part = -1;
    d->hit_distance_cm = -1.0f;
    /* Same ordering as msim_can_move: endpoint type-6 spheres first, then
     * the complete flat centre segment extended by the actual contact radius.
     * Keep every result local; never overwrite g_block_* or g_hit_* scratch. */
    if (sphere_push(view, p1, view->player_radius, push, normal, &ci)) {
        d->blocked = 1; d->hit_kind = 2;
        d->hit_part = terrain->co[ci].part;
        memcpy(d->hit_normal, normal, sizeof d->hit_normal);
        return;
    }
    if (!(length > 0.0f)) return;
    p1[0] += dx / length * view->player_radius;
    p1[2] += dz / length * view->player_radius;
    if (clist_seg(view, p0, p1, 1, &distance, normal, &ci)) {
        d->blocked = 1; d->hit_kind = 1;
        d->hit_part = terrain->co[ci].part;
        d->hit_distance_cm = distance;
        memcpy(d->hit_normal, normal, sizeof d->hit_normal);
    }
}

int mercs_playthrough_geometry_sample(const msim *s, const float feet[3],
                                     float heading_deg,
                                     mercs_playthrough_geometry *out)
{
    static const int ring_steps[MERCS_PLAYTHROUGH_GEOMETRY_RINGS] = {3, 10, 30, 60};
    const terrain_t *live;
    terrain_t terrain;
    msim *view;
    int heading, i;
    size_t stamp_count;
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!s || !feet || !s->terrain || !s->world || !isfinite(heading_deg) ||
        !isfinite(s->player_centre_h) || !isfinite(s->player_radius) ||
        !(s->player_centre_h > 0.0f && s->player_centre_h < 100000.0f) ||
        !(s->player_radius > 0.0f && s->player_radius < 100000.0f) ||
        !isfinite(s->player_unit.vy)) return -1;
    /* Keep invalid diagnostic coordinates out of clist_near's integer cell
     * conversion. This bound is not a gameplay/world boundary. */
    for (i = 0; i < 3; i++)
        if (!isfinite(feet[i]) || fabsf(feet[i]) > 100000000.0f) return -1;
    live = (const terrain_t *)s->terrain;
    if (live->nparts < 0 || live->nco < 0 ||
        (live->nco && (!live->co || !live->nparts))) return -1;
    stamp_count = (size_t)(live->nparts ? live->nparts : 1);
    if (stamp_count > SIZE_MAX / sizeof *terrain.stamp) return -1;
    terrain = *live;
    terrain.stamp = MERCS_PLAYTHROUGH_GEOMETRY_CALLOC(stamp_count, sizeof *terrain.stamp);
    if (!terrain.stamp) return -1;
    terrain.stamp_now = 0;
    /* msim is large: copy it on the heap, not the rendering thread's stack.
     * All pointed-to data remain read only except the private stamp storage. */
    view = malloc(sizeof *view);
    if (!view) { free(terrain.stamp); return -1; }
    memcpy(view, s, sizeof *view);
    view->terrain = &terrain;

    out->feet_y = feet[1]; out->centre_h = s->player_centre_h;
    out->radius = s->player_radius; out->query_y = feet[1] + s->player_centre_h;
    out->vertical_speed = s->player_unit.vy;
    out->ground_y = msim_ground_n(view, feet[0], feet[2], out->query_y, out->normal);
    out->airborne = feet[1] > out->ground_y + 1.0f;
    for (heading = 0; heading < MERCS_PLAYTHROUGH_GEOMETRY_HEADINGS; heading++) {
        float relative = (float)heading * 22.5f;
        float degrees, angle, axis, ux, uz, low = out->ground_y, high = out->ground_y;
        float previous = out->ground_y, down = 0.0f, up = 0.0f;
        int step, ring = 0;
        if (relative > 180.0f) relative -= 360.0f;
        degrees = fmodf(heading_deg, 360.0f) + relative;
        angle = degrees * 3.14159265f / 180.0f;
        ux = sinf(angle); uz = cosf(angle);
        /* A named cardinal ray must be exactly axis-aligned. Tiny trig
         * residue at 270 degrees otherwise invents a fractional +z offset
         * inside the native quadtree's 1 cm split seam. This changes only
         * diagnostic ray coordinates; native ground selection is untouched. */
        axis = fmodf(degrees, 180.0f);
        if (axis == 0.0f) ux = 0.0f;
        if (fabsf(axis) == 90.0f) uz = 0.0f;
        for (step = 1; step <= ring_steps[MERCS_PLAYTHROUGH_GEOMETRY_RINGS - 1]; step++) {
            float distance = (float)step * 100.0f;
            /* glview's regular ground query reads its stored integer X/Z,
             * written by lrintf after free motion or a reflected move. Keep
             * these lookahead points in that domain; contact-response float
             * intermediates are not a hypothetical trajectory we simulate. */
            float x = (float)lrintf(feet[0] + ux * distance);
            float z = (float)lrintf(feet[2] + uz * distance);
            float normal[3], ground = msim_ground_n(view, x, z, out->query_y, normal);
            if (ground < low) low = ground;
            if (ground > high) high = ground;
            if (previous - ground > down) down = previous - ground;
            if (ground - previous > up) up = ground - previous;
            previous = ground;
            if (step == ring_steps[ring]) {
                mercs_playthrough_direction *d = &out->directions[out->sample_count++];
                d->angle_relative_deg = relative; d->distance_cm = distance;
                d->position[0] = x; d->position[1] = z;
                d->ground_y = ground; memcpy(d->normal, normal, sizeof d->normal);
                d->min_ground_y = low; d->max_ground_y = high;
                d->max_step_down_cm = down; d->max_step_up_cm = up;
                playthrough_geometry_obstruction(view, feet, d);
                ring++;
                if (ring == MERCS_PLAYTHROUGH_GEOMETRY_RINGS) break;
            }
        }
    }
    out->available = 1;
    free(view); free(terrain.stamp);
    return 0;
}
