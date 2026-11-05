/* Original-execution plane fixtures, dynamic layer ownership, and direct
 * BWD/WTB resource counts. The original binary is not needed for this test. */
#include "mercscockpit.h"
#include "bwd.h"
#include "mercs_cockpit_plane_fixtures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int failures, plane_cases, resource_models;
#define CHECK(c, msg) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); failures++; } } while (0)

static void plane_fixtures(void)
{
    size_t i;
    for (i = 0; i < sizeof mercs_cockpit_plane_fixtures / sizeof mercs_cockpit_plane_fixtures[0]; i++) {
        const mercs_cockpit_plane_fixture *f = &mercs_cockpit_plane_fixtures[i];
        mech3d_part part = {0};
        wtb_vertex vertices[7] = {{0}};
        wtb_poly polygon = {0};
        wtb_object object = {0};
        int32_t normal[3];
        float eye[3];
        int k, visible;
        object.verts = vertices; object.vert_count = f->n; polygon.n = (uint16_t)f->n;
        for (k = 0; k < f->n; k++) {
            vertices[k].x = f->vertices[k][0]; vertices[k].y = f->vertices[k][1]; vertices[k].z = f->vertices[k][2];
            polygon.idx[k] = (uint16_t)k;
        }
        for (k = 0; k < 3; k++) eye[k] = (float)f->eye[k];
        CHECK(mercs_cockpit_dos_normal(&object, &polygon, normal) == 0, "normal constructor succeeds");
        visible = mercs_cockpit_face_visible(&part, &object, &polygon, eye);
        if (memcmp(normal, f->normal, sizeof normal) || visible != f->visible) {
            fprintf(stderr, "FAIL original fixture %s: normal %d,%d,%d vs %d,%d,%d; visible %d vs %d\n",
                    f->name, normal[0], normal[1], normal[2], f->normal[0], f->normal[1], f->normal[2], visible, f->visible);
            failures++;
        }
        /* A translated/rotated node must transform its original plane, not
         * recompute a different plane from its first three world corners. */
        part.has_rot = 1; part.rot[2] = 1; part.rot[4] = 1; part.rot[6] = -1;
        part.pos[0] = 1000000; part.pos[1] = -4000; part.pos[2] = -2000000;
        eye[0] = (float)(1000000 + f->eye[2]); eye[1] = (float)(-4000 + f->eye[1]); eye[2] = (float)(-2000000 - f->eye[0]);
        CHECK(mercs_cockpit_face_visible(&part, &object, &polygon, eye) == f->visible, "posed plane preserves original facing");
        plane_cases++;
    }
}

static void dynamic_layer(void)
{
    mech3d source = {0};
    mech3d_part parts[25] = {0};
    mercs_cockpit_layer layer = {0};
    wtb_vertex vertices[3] = {{0}};
    wtb_poly polys[3] = {{0}};
    wtb_object objects[2] = {0};
    float eye[3] = {0, 0, 0};
    int i, j;
    vertices[0].x = -10; vertices[0].y = -10; vertices[0].z = 100;
    vertices[1].x = 10; vertices[1].y = -10; vertices[1].z = 100;
    vertices[2].x = 0; vertices[2].y = 10; vertices[2].z = 100;
    polys[0].n = polys[1].n = 3; polys[2].n = 2;
    polys[0].idx[0] = 0; polys[0].idx[1] = 1; polys[0].idx[2] = 2;
    polys[1].idx[0] = 2; polys[1].idx[1] = 1; polys[1].idx[2] = 0;
    polys[2].idx[0] = 0; polys[2].idx[1] = 1;
    polys[0].color = 0x17f0; polys[1].color = 0x18f0; polys[2].color = 0x1234;
    for (i = 0; i < 2; i++) { objects[i].verts = vertices; objects[i].vert_count = 3; objects[i].polys = polys; objects[i].poly_count = 3; }
    source.parts = parts; source.part_count = 25; source.skel_count = 27;
    for (i = 0; i < 27; i++) { source.skel_index[i] = (int16_t)i; source.skel_parent[i] = i < 4 ? (int16_t)(i - 1) : 3; }
    source.skel_parent[26] = 1;   /* a torso sibling, outside the head */
    for (i = 0; i < 25; i++) {
        parts[i].node = i < 2 ? i + 1 : i + 2;   /* node 3 is a DUMMY intermediary */
        parts[i].objtype = i == 1 ? 0x1a0 : 0x100;
        parts[i].model.objects = objects; parts[i].model.object_count = 2;
    }
    source.part_count = 10;
    CHECK(mercs_cockpit_update(&layer, &source, eye) == 0 && layer.model.part_count == 10, "initial layer");
    source.part_count = 25;
    CHECK(mercs_cockpit_update(&layer, &source, eye) == 0, "grow beyond old 16-part limit");
    CHECK(layer.model.part_count == 25 && layer.head_node == 2 && layer.pit_parts == 23,
          "all 25 parts, only head subtree marked including DUMMY descendants");
    for (i = 0; i < layer.model.part_count; i++) {
        const mech3d_part *p = &layer.model.parts[i];
        CHECK(p->objtype == parts[i].objtype, "subtree membership does not overwrite runtime OBJ class");
        CHECK(p->dos_pit == (i != 0 && i != 24), "torso siblings stay in world pass");
        CHECK(p->model.object_count == 2, "second WTB object retained");
        for (j = 0; j < 2; j++) {
            CHECK(p->model.objects[j].poly_count == 2, "back face removed, line retained");
            CHECK(p->model.objects[j].polys[0].color == 0x17f0 && p->model.objects[j].polys[1].color == 0x1234,
                  "authored polygon colors and emission order preserved");
            CHECK(p->model.objects[j].verts == vertices && p->model.objects[j].polys != polys,
                  "vertices borrowed and visible polygon arrays owned");
        }
    }
    eye[2] = 200;
    CHECK(mercs_cockpit_update(&layer, &source, eye) == 0 &&
          layer.model.parts[24].model.objects[1].polys[0].color == 0x18f0, "back-facing visibility refreshed for new eye");
    parts[24].hidden = 1; parts[23].objtype = 0x120;
    CHECK(mercs_cockpit_update(&layer, &source, eye) == 0 && layer.model.part_count == 23, "hidden and class20 meshes excluded");
    source.part_count = 1;
    CHECK(mercs_cockpit_update(&layer, &source, eye) == 0 && layer.model.part_count == 1 &&
          layer.head_node == -1 && layer.pit_parts == 0, "shrinking/replacing model clears stale head membership");
    CHECK(mercs_cockpit_update(&layer, NULL, eye) == -1 && layer.model.part_count == 0, "invalid update clears output");
    mercs_cockpit_free(&layer);
    CHECK(objects[0].polys == polys && objects[1].poly_count == 3 && polys[0].color == 0x17f0,
          "cache cleanup never frees or edits source resources");
    mercs_cockpit_free(&layer);
}

typedef struct { int present, parent, type, model; } expected_node;
static int expected_child(const expected_node *nodes, int node, int head)
{
    int guard;
    if (head < 0) return 0;
    for (guard = 0; guard < 256 && node >= 0 && node < 256; guard++) {
        if (node == head) return 1;
        if (!nodes[node].present || nodes[node].parent == node) break;
        node = nodes[node].parent;
    }
    return 0;
}

static void resource_archive(const char *path)
{
    prj_archive *archive;
    char error[256];
    int type, entry, over16 = 0, commando = 0;
    mercs_cockpit_layer layer = {0};
    archive = prj_open(path, error, sizeof error);
    CHECK(archive != NULL, "open supplied archive");
    if (!archive) { fprintf(stderr, "%s\n", error); return; }
    type = prj_find_type(archive, "BWD");
    for (entry = 0; entry < prj_symbol_count(archive, type); entry++) {
        const char *name = prj_symbol_name(archive, type, entry);
        expected_node full_nodes[256] = {{0}}, pit_nodes[256] = {{0}};
        prj_record record;
        bwd_chunk *chunks;
        mech3d pit = {0}, full = {0};
        int count, i, repr = -1, eye = -1, expected_parts = 0, head = -1, torso = -1;
        if (prj_read_named(archive, "BWD", name, &record) != PRJ_OK) continue;
        count = bwd_chunks(record.data, record.size, NULL, 0);
        chunks = calloc((size_t)(count > 0 ? count : 1), sizeof *chunks);
        if (!chunks) { prj_record_free(&record); CHECK(0, "resource chunk allocation"); break; }
        bwd_chunks(record.data, record.size, chunks, count);
        /* Read the authored REPR boundaries independently of mech3d's DTBL
         * grouping, including its root OBJ before the first REPR marker. */
        for (i = 0; i < count; i++) {
            bwd_obj obj;
            if (!strcmp(chunks[i].tag, "REPR")) repr++;
            if (!strcmp(chunks[i].tag, "EYEO") && chunks[i].size >= 2)
                eye = (int16_t)(chunks[i].data[0] | chunks[i].data[1] << 8);
            if (!strcmp(chunks[i].tag, "THNG") && chunks[i].size >= 2 && torso < 0)
                torso = (int16_t)(chunks[i].data[0] | chunks[i].data[1] << 8);
            if (!bwd_obj_decode(&chunks[i], &obj) && obj.index >= 0 && obj.index < 256) {
                expected_node *dst = repr <= 0 ? full_nodes : repr == 4 ? pit_nodes : NULL;
                if (dst) { dst[obj.index].present = 1; dst[obj.index].parent = obj.parent;
                    dst[obj.index].type = obj.objtype; dst[obj.index].model = obj.model; }
            }
        }
        free(chunks); prj_record_free(&record);
        if (repr < 4 || eye < 0) continue;
        for (i = 0; i < 256; i++) if (pit_nodes[i].present) {
            prj_record model;
            CHECK(prj_read(archive, prj_find_type(archive, "POLY"), pit_nodes[i].model, &model) == PRJ_OK,
                  "authored repr4 POLY resolves");
            if (strcasecmp(model.name, "DUMMY")) expected_parts++;
            prj_record_free(&model);
            if ((pit_nodes[i].type & 0xf0) == 0xa0 && head < 0) head = i;
        }
        if (!expected_parts) continue;
        CHECK(mech3d_load(archive, name, 4, &pit) == 0, "load every authored cockpit representation");
        CHECK(mech3d_load(archive, name, 0, &full) == 0, "load matching full skeleton");
        if (full.twist_node != torso) {
            fprintf(stderr, "FAIL %s torso node: authored THNG %d, native %d\n", name, torso, full.twist_node);
            failures++;
        }
        CHECK(pit.part_count == expected_parts, "representation retains every non-DUMMY part");
        mech3d_use_skeleton(&pit, &full); mech3d_pose(&pit, NULL, 0);
        CHECK(mercs_cockpit_update(&layer, &pit, pit.eye_pos) == 0, "build real visible cockpit layer");
        CHECK(layer.head_node == head && layer.model.part_count == pit.part_count,
              "real model head marker and all parts preserved");
        for (i = 0; i < pit.part_count; i++) {
            const mech3d_part *src = &pit.parts[i], *dst = &layer.model.parts[i];
            int j;
            CHECK(src->objtype == (0x100 | (pit_nodes[src->node].type & 0xf0)), "repr4 OBJ class retained");
            CHECK(dst->dos_pit == expected_child(full_nodes, src->node, head), "real head subtree follows full skeleton");
            CHECK(dst->model.object_count == src->model.object_count, "real WTB object count retained");
            for (j = 0; j < src->model.object_count; j++)
                CHECK(dst->model.objects[j].poly_count <= src->model.objects[j].poly_count,
                      "visible polygon count bounded by source geometry");
        }
        for (i = 0; i < full.part_count; i++)
            CHECK(full.parts[i].objtype == (0x100 | (full_nodes[full.parts[i].node].type & 0xf0)),
                  "repr0 keeps its own class instead of leaking cockpit class");
        if (!strcmp(name, "COMMANDO")) {
            mech3d titanium = {0};
            CHECK(pit.part_count == 15 && head == 14 && layer.pit_parts == 4, "Commando: 15 parts, head/visor/two arms in cockpit pass");
            CHECK(mech3d_ti_load(archive, name, 4, &titanium) == 0, "Titanium loading path");
            for (i = 0; i < titanium.part_count; i++)
                CHECK(titanium.parts[i].ti && titanium.parts[i].objtype == pit.parts[i].objtype,
                      "Titanium path retains original representation classes");
            mech3d_free(&titanium); commando++;
        }
        if (pit.part_count > 16) over16++;
        resource_models++;
        mech3d_free(&pit); mech3d_free(&full);
    }
    CHECK(commando == 1 && over16 == 5, "supplied archive includes Commando and all five cockpits beyond the old cap");
    printf("Resource archive: %d representations, %d with more than 16 parts; %s\n", resource_models, over16, path);
    mercs_cockpit_free(&layer); prj_close(archive);
}

static int plane_probe(void)
{
    int n;
    while (scanf("%d", &n) == 1) {
        mech3d_part part = {0};
        wtb_vertex vertices[7] = {{0}};
        wtb_poly poly = {0};
        wtb_object obj = {0};
        int32_t normal[3];
        float eye[3];
        int k, x, y, z;
        if (n < 1 || n > 7) return 1;
        obj.verts = vertices; obj.vert_count = n; poly.n = (uint16_t)n;
        for (k = 0; k < n; k++) {
            if (scanf("%d %d %d", &x, &y, &z) != 3) return 1;
            vertices[k].x = x; vertices[k].y = y; vertices[k].z = z; poly.idx[k] = (uint16_t)k;
        }
        if (scanf("%f %f %f", &eye[0], &eye[1], &eye[2]) != 3 || mercs_cockpit_dos_normal(&obj, &poly, normal)) return 1;
        printf("%d %d %d %d\n", normal[0], normal[1], normal[2], mercs_cockpit_face_visible(&part, &obj, &poly, eye));
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--plane-probe")) return plane_probe();
    if (argc > 2) { fprintf(stderr, "usage: %s [MW2.PRJ]\n", argv[0]); return 2; }
    plane_fixtures(); dynamic_layer();
    if (argc == 2) resource_archive(argv[1]);
    if (!failures) printf("PASS: %d original plane fixtures, posed visibility, dynamic multiobject layer and %d resource models.\n", plane_cases, resource_models);
    return failures ? 1 : 0;
}
