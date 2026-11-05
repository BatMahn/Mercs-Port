/* Recovered Mercs ground topology and depth-band preparation.
 * Original DOS / dedicated grid selection and shade rules are tested against
 * executed binaries. Projection/rasterization remains the native GL backend. */
#ifndef MERCS_GROUND_H
#define MERCS_GROUND_H
#include <stdint.h>

enum { MERCS_GROUND_DOS = 1, MERCS_GROUND_3DFX = 2, MERCS_GROUND_TITANIUM = 3 };
typedef struct {
    int mode;
    float size;       /* DOS/3dfx: tile edge; Titanium: camera quad half-size */
    float repeats[2]; /* repeats across that edge / half-size */
    float clip[3];    /* DOS/3dfx SKYGND gclip; Titanium has fixed bands */
    int delta;        /* DOS grounddelta, added to LITE ambient */
} mercs_ground;
typedef struct { float x, z, u, v, depth, shade; } mercs_ground_vertex;
#define MERCS_GROUND_MAX_VERTICES 384

/* Original quadrant/grid rule, including negative exact tile boundaries. */
int mercs_ground_tiles(int32_t x, int32_t z, int32_t side, int32_t tiles[4][2]);
/* Original DOS 0x4a1bc, depth in quarter-centimetres. */
int mercs_ground_shade(int ambient, int delta, int near_cm, int far_cm,
                       int first_clip, int depth_quarter_cm);
/* A small triangle list clipped to the original edition's depth bands before
 * shade/fog evaluation. Eye and forward are game coordinates. */
int mercs_ground_mesh(const mercs_ground *ground, const float eye[3],
                      const float forward[3], float near_cm, int ambient,
                      mercs_ground_vertex out[MERCS_GROUND_MAX_VERTICES]);
#endif
