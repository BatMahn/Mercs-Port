/* Rendering-only cockpit placement: a shared seam and the eye must follow
 * the same fractional pose, while DOS and ordinary actors keep their original
 * integer positions. No game archive or mission simulation is required. */
#include "mercscockpit.h"
#include "anim.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(c, msg) do { checks++; if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); failures++; \
} } while (0)

static void world_point(const mech3d_part *part, const float local[3], double out[3])
{
    int k;
    for (k = 0; k < 3; k++)
        out[k] = (double)part->rot[k * 3] * local[0] +
                 (double)part->rot[k * 3 + 1] * local[1] +
                 (double)part->rot[k * 3 + 2] * local[2] +
                 mech3d_render_coord(part, k);
}

static double gap(const double a[3], const double b[3])
{
    double x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return sqrt(x * x + y * y + z * z);
}

static void moving_pose(void)
{
    mech3d source = {0};
    mech3d_part parts[3] = {0};
    mercs_cockpit_layer layer = {0};
    anim_set animation = {0};
    wtb_vertex vertices[3] = {{0}};
    wtb_poly polygon = {0};
    wtb_object object = {0};
    const int32_t offsets[5][3] = {{0, 0, 0}, {37, 11, 19}, {13, 5, 7}, {2, 3, -3}, {-17, 0, 31}};
    const float seam_head[3] = {13, 5, 7}, seam_child[3] = {0, 0, 0}, eye_head[3] = {2, 3, -3};
    double old_gap = 0, new_gap = 0, eye_min = 1e9, eye_max = -1e9;
    int saved_dos = mech3d_dos_hips, frame, i, k;

    vertices[0].x = -10; vertices[0].y = -10; vertices[0].z = 100;
    vertices[1].x = 10; vertices[1].y = -10; vertices[1].z = 100;
    vertices[2].y = 10; vertices[2].z = 100;
    polygon.n = 3; polygon.idx[0] = 0; polygon.idx[1] = 1; polygon.idx[2] = 2;
    object.verts = vertices; object.vert_count = 3; object.polys = &polygon; object.poly_count = 1;
    source.parts = parts; source.part_count = 3; source.skel_count = 5;
    source.heading = 27; source.twist_node = -1; source.eye_node = 3;
    source.fall_node[0] = source.fall_node[1] = -1;
    for (i = 0; i < 8; i++) source.pofo_node[i] = -1;
    source.pofo_node[0] = 3;
    for (i = 0; i < 5; i++) {
        source.skel_index[i] = (int16_t)i;
        source.skel_parent[i] = (int16_t)(i == 0 ? -1 : i == 1 || i == 4 ? 0 : 1);
        memcpy(source.skel_offset[i], offsets[i], sizeof offsets[i]);
    }
    for (i = 0; i < 3; i++) {
        parts[i].node = i == 2 ? 4 : i + 1;
        parts[i].objtype = i == 0 ? 0x1a0 : 0x100;
        parts[i].model.objects = &object; parts[i].model.object_count = 1;
        source.bind_node[i] = 0; source.bind_track[i] = (int16_t)i;
    }
    source.bind_count = 3;
    animation.track_count = 3; animation.key_count = 1;
    animation.tracks[0].type = 0; animation.tracks[0].pose[1] = 0.99f;
    animation.tracks[1].type = 1; animation.tracks[1].pose[1] = 2.0f;
    animation.tracks[2].type = 5; animation.tracks[2].pose[0] = -1.0f; animation.tracks[2].pose[1] = 1.0f;
    mech3d_dos_hips = 0;

    for (frame = 0; frame < 121; frame++) {
        mech3d snapshot;
        mech3d_part part_snapshot[3];
        double a[3], b[3], eye_expected[3], g;
        mech3d_pose(&source, &animation, (float)frame / 120.0f);
        snapshot = source; memcpy(part_snapshot, parts, sizeof parts);
        CHECK(source.eye_ok && (source.mount_ok & 1u), "pose publishes the eye and mount");
        CHECK(!memcmp(source.eye_pos, source.mount_pos[0], sizeof source.eye_pos), "eye and mount stay on their authored node");
        for (i = 0; i < 3; i++) {
            CHECK(parts[i].pose_pos_valid && !parts[i].render_pose_pos, "posing records precision without enabling it for actors");
            for (k = 0; k < 3; k++) {
                CHECK(parts[i].pos[k] == (int32_t)lrintf(parts[i].pose_pos[k]), "legacy integer placement retained");
                CHECK(mech3d_render_coord(&parts[i], k) == (double)parts[i].pos[k], "ordinary actor drawing remains integer");
            }
        }
        CHECK(mercs_cockpit_update(&layer, &source, source.eye_pos) == 0, "legacy cockpit update");
        world_point(&layer.model.parts[0], seam_head, a);
        world_point(&layer.model.parts[1], seam_child, b);
        g = gap(a, b); if (g > old_gap) old_gap = g;
        for (i = 0; i < 3; i++) CHECK(!layer.model.parts[i].render_pose_pos, "legacy API disables fractional drawing");

        CHECK(mercs_cockpit_update_render(&layer, &source, source.eye_pos, 1) == 0, "accelerated cockpit update");
        CHECK(layer.model.part_count == 3 && layer.pit_parts == 2, "body stays outside the head subtree");
        for (i = 0; i < 3; i++) {
            const mech3d_part *draw = &layer.model.parts[i];
            CHECK(draw->render_pose_pos, "all repr4 drawing copies use the same precision");
            CHECK(!memcmp(draw->pos, parts[i].pos, sizeof draw->pos), "drawing does not rewrite integer placement");
            CHECK(!memcmp(draw->rot, parts[i].rot, sizeof draw->rot), "drawing preserves animated rotation");
        }
        world_point(&layer.model.parts[0], seam_head, a);
        world_point(&layer.model.parts[1], seam_child, b);
        g = gap(a, b); if (g > new_gap) new_gap = g;
        CHECK(g < 0.0001, "shared head/visor seam remains joined through fractional motion");
        world_point(&layer.model.parts[0], eye_head, eye_expected);
        for (k = 0; k < 3; k++) CHECK(fabs(eye_expected[k] - source.eye_pos[k]) < 0.0001, "cockpit follows the same pose as its eye");
        {
            mech3d_part ordinary = layer.model.parts[1], near = ordinary;
            ordinary.dos_pit = 0; near.dos_pit = 1;
            for (k = 0; k < 3; k++) CHECK(mech3d_render_coord(&ordinary, k) == mech3d_render_coord(&near, k), "world and near copies keep identical placement");
        }
        CHECK(!memcmp(&source, &snapshot, sizeof source) && !memcmp(parts, part_snapshot, sizeof parts), "layer construction leaves source pose, camera, mounts and parts untouched");
        if (source.eye_pos[1] < eye_min) eye_min = source.eye_pos[1];
        if (source.eye_pos[1] > eye_max) eye_max = source.eye_pos[1];
    }
    CHECK(old_gap > 0.25, "negative control reproduces independent part rounding");
    CHECK(new_gap < 0.0001, "fractional drawing removes the shared-seam error");
    CHECK(eye_max - eye_min > 2.0, "intentional vertical animation remains active");

    /* A reused drawing copy and a removed node must not carry stale overrides
     * into the integer path or a later pose. */
    parts[0].render_pose_pos = 1;
    CHECK(mercs_cockpit_update_render(&layer, &source, source.eye_pos, 0) == 0, "explicit integer update after accelerated update");
    for (i = 0; i < 3; i++) CHECK(!layer.model.parts[i].render_pose_pos, "integer update clears copied render activation");
    CHECK(parts[0].render_pose_pos, "clearing the drawing flag does not mutate the source");
    parts[2].node = 60; parts[2].render_pose_pos = parts[2].pose_pos_valid = 1;
    mech3d_pose(&source, &animation, 0.5f);
    CHECK(!parts[0].render_pose_pos && !parts[2].render_pose_pos && !parts[2].pose_pos_valid, "posing clears activation and invalid-node precision");
    CHECK(mercs_cockpit_update_render(&layer, &source, source.eye_pos, 1) == 0 && !layer.model.parts[2].render_pose_pos, "unsolved part falls back to integer drawing");

    parts[2].node = 4; source.centre_h = 100; mech3d_dos_hips = 1;
    mech3d_pose(&source, &animation, 0.372f);
    CHECK(mercs_cockpit_update(&layer, &source, source.eye_pos) == 0, "DOS wrapper accepts original quantized pose");
    for (i = 0; i < 3; i++) for (k = 0; k < 3; k++) {
        CHECK(parts[i].pose_pos[k] == (float)parts[i].pos[k], "DOS solver translations remain whole units");
        CHECK(mech3d_render_coord(&layer.model.parts[i], k) == (double)parts[i].pos[k], "DOS drawing remains on original integer coordinates");
    }
    mech3d_dos_hips = saved_dos;
    mercs_cockpit_free(&layer);
    printf("Moving seam: legacy maximum %.6f cm, fractional maximum %.9f cm; eye motion %.6f cm\n", old_gap, new_gap, eye_max - eye_min);
}

static void fractional_facing(void)
{
    mech3d source = {0};
    mech3d_part part = {0};
    mercs_cockpit_layer layer = {0};
    wtb_vertex vertices[3] = {{0}};
    wtb_poly polygon = {0};
    wtb_object object = {0};
    float eye[3] = {0, 0, 100.2f};
    int rotated;
    vertices[0].x = -1; vertices[1].x = 1; vertices[2].y = 1;
    polygon.n = 3; polygon.idx[0] = 0; polygon.idx[1] = 1; polygon.idx[2] = 2;
    polygon.has_normal = 1; polygon.normal[2] = 1;
    object.verts = vertices; object.vert_count = 3; object.polys = &polygon; object.poly_count = 1;
    source.parts = &part; source.part_count = 1; source.skel_count = 1;
    source.skel_index[0] = 0; source.skel_parent[0] = -1;
    part.node = 0; part.objtype = 0x1a0; part.model.objects = &object; part.model.object_count = 1;
    part.pos[2] = 100; part.pose_pos[2] = 100.4f; part.pose_pos_valid = 1;
    part.rot[0] = part.rot[4] = part.rot[8] = 1;
    for (rotated = 0; rotated < 2; rotated++) {
        part.has_rot = rotated;
        CHECK(mercs_cockpit_update(&layer, &source, eye) == 0 && layer.model.parts[0].model.objects[0].poly_count == 0, "integer plane is behind the eye");
        CHECK(mercs_cockpit_update_render(&layer, &source, eye, 1) == 0 && layer.model.parts[0].model.objects[0].poly_count == 1, "facing uses the same fractional plane as drawing");
        CHECK(!part.render_pose_pos && part.pos[2] == 100, "fractional facing does not edit source part");
        CHECK(mercs_cockpit_update(&layer, &source, eye) == 0 && layer.model.parts[0].model.objects[0].poly_count == 0, "switching to integer mode restores its original plane");
    }
    part.pos[2] = 16777217; eye[2] = 16777216.0f;
    CHECK(mercs_cockpit_face_visible(&part, &object, &polygon, eye), "legacy strict plane test retains exact integer-to-double conversion");
    mercs_cockpit_free(&layer);
}

int main(void)
{
    moving_pose(); fractional_facing();
    printf("%s: %u cockpit motion checks, %u failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
