/*
 * wtb.h - MechWarrior 2 polygon models (POLY resources, originally .WTB files).
 *
 * A record is one or more objects back to back (1-9; validated: all 3,798
 * records in MW2.PRJ parse to the exact byte, all 85,594 polygon vertex
 * references are in range). Each object:
 *
 *   0x00  "WTBO"
 *   0x04  u32 ?
 *   0x08  char name[16]   each byte negated, e.g. "tw1_hips"
 *   0x18  u16 vertex_count
 *   0x1A  u16 polygon_count
 *   0x1C  u16 flags       low bits ?; Mercs DOS 0x6304c / 0x45c04:
 *                         0x1000 links distance alternatives (name suffix),
 *                         0x2000 creates translated sibling objects using
 *                         the last vertex as the offset. Parsed WTBO blocks
 *                         are therefore not all additive geometry.
 *   0x1E  u16 ?
 *   0x20  vertex_count x { i32 x, i32 y, i32 z, u16 a, u16 b }   (a, b: unknown)
 *   then  polygon_count records, FIXED size by vertex count (this is how the
 *         engine walks them, MW2.EXE 0x50073):
 *           n <= 4: 12 bytes { u16 color, u16 n, u16 idx[4] }
 *           n 5-7:  18 bytes { u16 color, u16 n, u16 idx[7] }
 *         color: high byte = palette index of the polygon's base colour, shaded
 *         through a 16-level lighting table (see render.h); low byte = flags.
 *
 * Coordinates: y up, +z forward; same units as skeleton offsets.
 *
 * 3D-accelerated editions (3dfx, ATi, S3, PowerVR; 1996) use an extended
 * layout, detected automatically (all 6,574 models parse; one known odd
 * record excepted):
 *   name stored plainly (not negated)
 *   vertices 40 bytes: i32 x, y, z; double normal[3]; u16 u, v (texel coords)
 *   polygons: n <= 4: 40 bytes, n 5-7: 46 bytes
 *     { u16 color, u16 material, u16 ?, double normal[3], u16 n, u16 idx[n] }
 *     material 0x0Bxx = textured with the mech's camouflage
 *     (verified by rendering the 3dfx Timber Wolf with camo and insignia).
 */
#ifndef MW2_WTB_H
#define MW2_WTB_H

#include <stddef.h>
#include <stdint.h>

#define WTB_MAX_POLY_VERTS 7

typedef struct {
    int32_t  x, y, z;
    uint16_t a, b;         /* DOS: unknown; 3D editions: texel u, v */
    float    normal[3];    /* 3D editions only (smooth shading) */
} wtb_vertex;

typedef struct {
    uint16_t color;
    uint16_t material;     /* 3D editions only; 0x0Bxx = camo-textured */
    uint16_t unknown;
    uint16_t n;
    uint16_t idx[WTB_MAX_POLY_VERTS];
    float    normal[3];    /* 3D editions: bytes 6-29, three doubles (the engine loads them negated, 0x10042140) */
    int      has_normal;
} wtb_poly;

typedef struct {
    char        name[17];
    uint16_t    flags;
    wtb_vertex *verts;
    int         vert_count;
    wtb_poly   *polys;
    int         poly_count;
} wtb_object;

typedef struct {
    wtb_object *objects;
    int         object_count;
    int         extended;  /* 1 = 3D-edition layout */
} wtb_model;

/* Parse a POLY payload. Returns 0 on success, -1 if malformed. */
int  wtb_parse(const uint8_t *data, size_t len, wtb_model *out);
void wtb_free(wtb_model *m);

/* Original Mercs distance alternatives (flag 0x1000): the numeric suffix
 * after the first '_' is the threshold, sorted stably by the loader. A zero
 * first threshold disables switching. The interval uses -1 for an unbounded
 * end; a false return means this record is not an active alternative.
 * Non-distance/translated records retain their first-object interpretation. */
int wtb_lod_interval(const wtb_model *m, int object, int32_t *lower, int32_t *upper);
int wtb_lod_select_q16(const wtb_model *m, int32_t depth, int32_t camera_scale);
int wtb_lod_select_float(const wtb_model *m, float depth, float camera_scale);

/* Original Mercs DOS 0x45988 / 0x48003 and retail Windows 0x1001cfba /
 * 0x1001b971: polygon normal in signed Q29 units. The constructor uses the
 * original longest-edge approximation, 64-bit cross-product bit index and
 * square-root table normalization. Coordinates are the already scaled model
 * vertices; the caller applies the world rotation. Returns -1 for invalid
 * inputs or coordinate differences outside the supported signed-32 range. */
int wtb_mercs_normal_q29(const wtb_object *object, const wtb_poly *polygon,
                        int32_t normal[3]);

#endif
