#ifndef PROJECTILE_EFFECT_FIXTURE_H
#define PROJECTILE_EFFECT_FIXTURE_H
#include "src/msim.h"
void effect_fixture_base(msim *s);
void effect_fixture_impact(msim *s, int weapon, const float c[3], const float e[3], int flags);
void effect_fixture_segment(msim *s, int weapon, const float c[3], const float from[3], const float to[3], int flags);
void effect_fixture_ordinary(msim *s, int type, const float point[3]);
void effect_fixture_step(msim *s, int ticks);
void effect_fixture_area(msim *s, int ticks);
void effect_fixture_shot(msim *s, int weapon, const float from[3], const float velocity[3]);
void effect_fixture_plane(mech3d *world, int y);
void effect_fixture_gt(msim *s, mech3d *world);
void effect_fixture_unit(msim *s);
void effect_fixture_cleanup(msim *s);
#endif
