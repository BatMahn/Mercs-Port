/* Reuse the existing declared WOL1 order setup and actual msim implementation.
 * This is an isolated native fixture, never an active campaign record. */
#define main unused_dispatch_fixture_main
#include "test_mercs_mission_dispatch.c"
#undef main

msim *palette_fixture_create(void)
{
    const int point[3]={79000,3200,19300}; capture temporary;
    msim *s=synthetic(&temporary,5,point);
    published_instant(s,5);s->shot_user=NULL;s->on_effect=NULL;
    return s;
}
void palette_fixture_clear(msim *s)
{
    world_actor *actors=s->actors;msim_free(s);free(actors);memset(s,0,sizeof *s);
}
void palette_fixture_producer(msim *s,const float point[3])
{
    nuclear_start(s,point);
}
