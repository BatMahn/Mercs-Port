/* Mercenaries BWD loader placement: DOS 0x434bc/0x437ec, Windows
 * 0x1000cb75/0x1000d016. Blocks are scoped by BLK/ENDB in the expanded
 * chunk stream, not by the file containing the chunks. */
#ifndef MERCS_BWDPLACE_H
#define MERCS_BWDPLACE_H
#include "bwd.h"

typedef struct {
    int32_t m[12]; /* row-major 3x3 Q29; integer-centimetre translation */
    int block;    /* original block index, or -1 for the identity */
} bwd_transform;

void bwd_transform_identity(bwd_transform *out);
void bwd_transform_point(const bwd_transform *t, int32_t xyz[3]);
void bwd_transform_multiply(bwd_transform *out, const bwd_transform *a,
                            const bwd_transform *b);
/* Original Mercs sine: signed Q16 degrees to Q29, using the initialized
 * table and its integer interpolation (also used by positional audio). */
int32_t bwd_angle_sin(int32_t angle_q16);
/* Original Mercs inverse sine: Q29 input to Q16 degrees. Shared by block
 * geometry and controller placement; uses the same initialized sine table. */
int32_t bwd_angle_asin(int32_t sine_q29);
/* Original interpolated atan2, returning signed Q16 degrees. */
int32_t bwd_angle_atan2(int32_t y, int32_t x);
/* Original order-0 matrix extraction, including the near-vertical branch. */
void bwd_transform_angles(const bwd_transform *matrix, int32_t angles[3]);
/* Controller matrix order 1 (pitch, then twist; zero roll), as selected by
 * the first THNG setter. Block transforms use the independent order 0. */
void bwd_transform_torso(bwd_transform *out, int32_t pitch, int32_t twist);
/* bounds = BLK {origin xyz, extent xyz}; transform = BLKX {scale xyz,
 * 16.16-degree rotation xyz, translation xyz}. The original builder writes
 * the scale matrix but never multiplies it into the active matrix. */
void bwd_transform_block(bwd_transform *out, const int32_t bounds[6],
                         const int32_t transform[9]);

/* Visit each loaded include occurrence in original depth-first chunk order.
 * chunk_index=-1 enters a record, -2 leaves it. Other values identify a
 * non-placement chunk, including INCL before entering its record. Repeated
 * includes are visited again; a cycle or malformed block stack is rejected.
 * A nonzero visitor return stops traversal and is returned to the caller.
 * The active transform may change within one record and across an INCL. */
typedef int (*bwd_place_visitor)(const bwd_mission *mission, int record_index,
                                 int chunk_index, const bwd_transform *active,
                                 void *user);
int bwd_place_walk(const bwd_mission *mission, bwd_place_visitor visit, void *user);
/* First NAVP of the named record in expanded order; returns 0 on success,
 * -1 when absent/invalid. Heading and NAVP metadata remain authored values. */
int bwd_place_nav(const bwd_mission *mission, const char *record_name,
                   bwd_navpoint *out);
#endif
