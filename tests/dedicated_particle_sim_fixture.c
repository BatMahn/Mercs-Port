/* Test-only wrapper around the unchanged actual native class5 launch site. */
#include "src/msim.c"
void dedicated_particle_fixture_launch(msim *s,int weapon,const float from[3],const float to[3])
{
    spawn_shot_h(s,weapon,-1,from,to,0,-2);
}
