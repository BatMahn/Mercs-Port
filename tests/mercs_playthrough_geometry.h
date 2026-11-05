/* Test-only local geometry observations. These are steering measurements,
 * not movement commands or a replacement for the simulation's collision rules. */
#ifndef MERCS_PLAYTHROUGH_GEOMETRY_H
#define MERCS_PLAYTHROUGH_GEOMETRY_H

#include "msim.h"

#define MERCS_PLAYTHROUGH_GEOMETRY_HEADINGS 16
#define MERCS_PLAYTHROUGH_GEOMETRY_RINGS 4
#define MERCS_PLAYTHROUGH_GEOMETRY_SAMPLES \
    (MERCS_PLAYTHROUGH_GEOMETRY_HEADINGS * MERCS_PLAYTHROUGH_GEOMETRY_RINGS)

typedef struct {
    float angle_relative_deg, distance_cm;
    float position[2];             /* endpoint x,z */
    float ground_y, normal[3];      /* support at the endpoint */
    float min_ground_y, max_ground_y;
    float max_step_down_cm, max_step_up_cm; /* consecutive 100 cm support samples */
    int blocked;                   /* current-height world movement segment */
    int hit_kind, hit_part;         /* 0 none, 1 segment, 2 endpoint sphere */
    float hit_distance_cm;          /* raw centre-to-hit on extended segment; -1 if none/sphere */
    float hit_normal[3];
} mercs_playthrough_direction;

typedef struct {
    int available, sample_count;
    float feet_y, ground_y, query_y, centre_h, radius;
    float vertical_speed;           /* combat_unit.vy: cm per engine tick, not cm/s */
    int airborne;
    float normal[3];
    mercs_playthrough_direction directions[MERCS_PLAYTHROUGH_GEOMETRY_SAMPLES];
} mercs_playthrough_geometry;

/* Returns 0 on success, -1 for invalid input or unavailable scratch storage.
 * On failure out->available and sample_count are zero. The caller must not
 * interpret unavailable observations as unobstructed directions.
 *
 * Rings: 300,1000,3000,6000 cm; heading increments: 22.5 degrees. All support
 * queries use the SAME actual feet Y + live MGEO centre height. They do not
 * advance a hypothetical walker. min/max and step deltas sample every 100 cm
 * along the ray. Their X/Z coordinates are rounded with lrintf to integer
 * centimetres, matching glview's stored position and regular support query;
 * position[] publishes the rounded endpoint. This does not model intermediate
 * fractional contact-response positions. Sub-metre holes can be missed.
 * blocked uses the ordinary
 * flat movement segment and radius extension, including type-5 terrain walls,
 * but omits unit contacts. A clear ray can still be a drop. A blocked ray can
 * meet a climbable slope; hit_normal is reported separately. These finite
 * local samples do not establish whole-route safety or a predicted landing.
 *
 * The implementation is linked only into the playthrough test executable in
 * place of msim.o. No declaration is added to the production simulation API. */
int mercs_playthrough_geometry_sample(const msim *s, const float feet[3],
                                     float heading_deg,
                                     mercs_playthrough_geometry *out);

typedef struct {
    int available, blocked;
    int hit_part, hit_collision_type, hit_thing;
    float from[3], to[3], segment_length_cm;
    float hit_distance_cm, hit_point[3], normal[3];
} mercs_playthrough_world_ray;

/* Fresh world-only observation along exactly from -> to, using the existing
 * clist_seg(walk=0) nearest-hit primitive and an isolated terrain stamp/cache
 * view. No radius extension, pose update, RNG, actor movement or game-state
 * write. A hit is reported separately from any unit/target identity.
 *
 * The observer caller must supply a valid current eye (player_eye_ok) and
 * current target point. A finite nonzero segment is required. Failure leaves
 * available=0; it must never be read as clear. Success leaves available=1,
 * blocked reports a native world hit, and hit_distance_cm is the raw native
 * result. hit_thing is direct GT ownership if known, otherwise -1.
 *
 * Clear does not establish a projectile/muzzle path, posed unit/debris
 * clearance, fresh reticle hit, range eligibility or guided lock. A blocked
 * centre ray may coexist with an exposed target part. Moving-world queries
 * read the current already-published pose/bounds; they do not advance it.
 * This API exists only in the test observer translation unit. */
int mercs_playthrough_world_obstruction(const msim *s, const float from[3],
                                      const float to[3],
                                      mercs_playthrough_world_ray *out);

#endif
