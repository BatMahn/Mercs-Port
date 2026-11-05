/* Mercenaries palette particles. DOS MW2.EXE and dedicated 3dfx MERCSW;
 * Titanium's separate particle.PID path is not represented here. */
#ifndef MERCS_PARTICLES_H
#define MERCS_PARTICLES_H
#include <stdint.h>

#define MERCS_PARTICLE_EMITTERS 1000
#define MERCS_PARTICLE_POINTS 64
#define MERCS_PARTICLE_PRIMITIVES (MERCS_PARTICLE_EMITTERS * MERCS_PARTICLE_POINTS)
enum { MERCS_PARTICLES_DOS, MERCS_PARTICLES_3DFX };
enum { MERCS_PARTICLE_MISSILE = 1, MERCS_PARTICLE_BALLISTIC = 3 };
typedef struct {
    int32_t pos[3], velocity[3], offset[MERCS_PARTICLE_POINTS][3], born;
    int kind;
    uint8_t color;
} mercs_particle_emitter;
typedef struct {
    mercs_particle_emitter emitter[MERCS_PARTICLE_EMITTERS];
    uint16_t active[MERCS_PARTICLE_EMITTERS], available[MERCS_PARTICLE_EMITTERS];
    uint16_t random_table[127];
    int count, free_count, edition, enabled, random_index;
    int (*random_value)(void *user, int limit); /* optional deterministic fixture boundary */
    void *random_user;
} mercs_particles;
typedef struct { float pos[3], end[3]; uint8_t color; int line; } mercs_particle_primitive;

void mercs_particles_init(mercs_particles *p, int edition, uint32_t seed);
void mercs_particles_enable(mercs_particles *p, int enabled);
/* Integer cm, once per original producer event. Only missile trails are
 * range-culled by the original approximate distance. Ballistic launch vectors
 * are initial cm/tick velocities; missile vectors are actual frame moves. */
int mercs_particles_emit(mercs_particles *p, int kind, const int32_t origin[3],
                         const int32_t delta[3], int32_t now,
                         const int32_t eye[3], int32_t far_distance);
/* Once per simulation frame with a nonzero clock delta, never a redraw.
 * The originals advance drift once per frame, not once per 182-Hz tick. */
void mercs_particles_step(mercs_particles *p, int32_t now);
/* Original draw callbacks make a preliminary depth query using (0,0,pos.z).
 * Supply its camera Z in quarter cm; actual primitives retain full world xyz.
 * A missing callback selects near density (useful for non-viewer tests). */
int mercs_particles_collect(const mercs_particles *p,
                            int32_t (*depth_for_z)(void *user, int32_t z), void *user,
                            mercs_particle_primitive *out, int capacity);
#endif
