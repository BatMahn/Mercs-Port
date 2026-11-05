/* Native flight-centre regression on the supplied archives. These are actual
 * model/mission inputs, not an original whole-mission oracle. The original
 * root publication is documented in fidelity-followup/README.md. */
#include "../src/msim.c"
#include <assert.h>

static void close3(const float a[3], const float b[3])
{
    int k;
    for (k = 0; k < 3; k++) assert(fabsf(a[k] - b[k]) <= 2.0f);
}

/* Independently measure the first torso part in the posed draw geometry.
 * Target selection must refer to this same point, in the same coordinates. */
static int body_point(const mech3d *m, float out[3])
{
    int i, k, v;
    for (i = 0; i < m->part_count; i++) {
        const mech3d_part *p = &m->parts[i];
        const wtb_object *o;
        float low[3] = {1e20f, 1e20f, 1e20f}, high[3] = {-1e20f, -1e20f, -1e20f}, c[3];
        if (p->group != 3 || !p->model.object_count) continue;
        o = &p->model.objects[0];
        if (!o->vert_count) continue;
        for (v = 0; v < o->vert_count; v++) {
            const float q[3] = {(float)o->verts[v].x, (float)o->verts[v].y, (float)o->verts[v].z};
            for (k = 0; k < 3; k++) {
                if (q[k] < low[k]) low[k] = q[k];
                if (q[k] > high[k]) high[k] = q[k];
            }
        }
        for (k = 0; k < 3; k++) c[k] = (low[k] + high[k]) * .5f;
        for (k = 0; k < 3; k++)
            out[k] = (float)p->pos[k] + (p->has_rot ? p->rot[3*k]*c[0] + p->rot[3*k+1]*c[1] + p->rot[3*k+2]*c[2] : c[k]);
        return 1;
    }
    return 0;
}

static void aligned(mech3d *m, const float origin[3])
{
    float drawn[3], target[3], mount[3];
    int k;
    if (body_point(m, drawn)) {
        assert(mech3d_group_point(m, 3, origin, m->heading, target) == 0);
        close3(target, drawn);
    }
    for (k = 0; k < 8; k++) if (m->mount_ok & (1u << k)) {
        assert(mech3d_mount_world(m, k, origin, m->heading, mount) == 0);
        close3(mount, m->mount_pos[k]);
    }
}

static int flight_class(prj_archive *a, const char *name)
{
    prj_record rec;
    bwd_chunk *chunks;
    int n, k, cls = 0;
    if (prj_read_named(a, "BWD", name, &rec) != PRJ_OK) return 0;
    n = bwd_chunks(rec.data, rec.size, NULL, 0);
    chunks = calloc((size_t)(n > 0 ? n : 1), sizeof *chunks);
    assert(chunks);
    if (n > 0) bwd_chunks(rec.data, rec.size, chunks, n);
    for (k = 0; k < n; k++)
        if (!strcmp(chunks[k].tag, "GP") && chunks[k].size >= 4)
            cls = (int16_t)(chunks[k].data[2] | chunks[k].data[3] << 8);
    free(chunks); prj_record_free(&rec);
    return cls == 5 || cls == 8;
}

static void models(prj_archive *a)
{
    int type = prj_find_type(a, "BWD"), row, hips, repr, count = 0, views = 0;
    for (row = 0; row < prj_symbol_count(a, type); row++) {
        const char *name = prj_symbol_name(a, type, row);
        float expected_eye[3] = {0};
        if (!flight_class(a, name)) continue;
        count++;
        for (hips = 0; hips < 2; hips++) {
            mech3d full = {0};
            const float origin[3] = {124700, 321, -177000};
            mech3d_dos_hips = hips;
            assert(!mech3d_load(a, name, 0, &full));
            assert(full.controller_centre && full.centre_h > 0);
            full.origin[0] = 124700; full.origin[1] = 321; full.origin[2] = -177000;
            full.heading = 127;
            mech3d_pose(&full, NULL, 0);
            aligned(&full, origin);
            if (!hips) memcpy(expected_eye, full.eye_pos, sizeof expected_eye);
            else close3(full.eye_pos, expected_eye);
            for (repr = 0; repr < 5; repr++) {
                mech3d view = {0};
                if (mech3d_load(a, name, repr, &view)) continue;
                mech3d_use_skeleton(&view, &full);
                mech3d_copy_pose(&view, &full);
                assert(view.controller_centre);
                mech3d_pose(&view, NULL, 0);
                aligned(&view, origin);
                if (view.eye_ok && full.eye_ok) close3(view.eye_pos, full.eye_pos);
                views++;
                mech3d_free(&view);
            }
            mech3d_free(&full);
        }
    }
    assert(count >= 14);
    printf("PASS: %d flight models, %d representation/profile poses; common eye, target and weapon coordinates\n", count, views);
}

static void scenes(prj_archive *a)
{
    static const char *names[] = {"BET2SCN1", "BIG1SCN1", "BUZ1SCN1", "GRA2SCN1"};
    int scene, hips, checked = 0, negative = 0;
    for (hips = 0; hips < 2; hips++) for (scene = 0; scene < 4; scene++) {
        world_actor *actors = NULL;
        msim *s = calloc(1, sizeof *s);
        int count = 0, i;
        int32_t *start;
        assert(s);
        mech3d_dos_hips = hips;
        assert(!world3d_actors(a, names[scene], &actors, &count));
        start = calloc((size_t)(count ? count : 1), sizeof *start); assert(start);
        for (i = 0; i < count; i++) start[i] = actors[i].mech.origin[1];
        assert(!msim_init(s, a, names[scene], actors, count));
        for (i = 0; i < count; i++) if (actors[i].unit_class == 5 || actors[i].unit_class == 8) {
            mech3d *m = &actors[i].mech;
            float origin[3] = {(float)m->origin[0], s->units[i].y, (float)m->origin[2]};
            float target[3], from[3], dir[3] = {0, 0, 1}, dv[3] = {0, 0, 10000}, dist, t;
            int unit, group;
            assert(m->origin[1] == start[i]);
            assert(msim_draw_y(s, i) == s->units[i].y);
            assert(s->centre_h[i] == (float)m->centre_h);
            aligned(m, origin);
            assert(body_point(m, target));
            memcpy(from, target, sizeof from); from[2] -= 5000;
            assert(aim_ray(s, -1, from, dir, &dist));
            assert(s->reticle_actor == i && dist < 10000);
            assert(msim_test_shot(s, from, dv, &unit, &group, &t));
            assert(unit == i && group > 0 && t >= 0 && t <= 1);
            /* The old duplicate placement is a negative control: a beam at
             * the legitimate helicopter's target point misses its old hull. */
            if (!strcmp(actors[i].skel, "AIRCRFTA")) {
                m->origin[1] += m->centre_h; mech3d_pose(m, NULL, 0);
                assert(!mech3d_hull_hit(m, from, dv, 0, &t, &group));
                m->origin[1] = start[i]; mech3d_pose(m, NULL, 0);
                negative++;
            }
            checked++;
        }
        msim_free(s); free(s); free(start);
        for (i = 0; i < count; i++) mech3d_free(&actors[i].mech);
        free(actors);
    }
    assert(checked >= 8 && negative >= 2);
    printf("PASS: %d authored aircraft/dropship spawns; actual aim and projectile hulls; %d duplicate-height negative controls\n", checked, negative);
}

int main(int argc, char **argv)
{
    int i;
    assert(argc > 1);
    for (i = 1; i < argc; i++) {
        char err[256]; prj_archive *a = prj_open(argv[i], err, sizeof err);
        if (!a) { fprintf(stderr, "%s: %s\n", argv[i], err); return 1; }
        printf("ARCHIVE %s\n", argv[i]);
        models(a); scenes(a); prj_close(a);
    }
    return 0;
}
