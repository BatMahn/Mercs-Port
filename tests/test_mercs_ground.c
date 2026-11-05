/* Native spawn recovery tests. These are declared geometry fixtures, not
 * captured original-engine results. Optional PRJ argument exercises TRA1's
 * authored starts and actual terrain, including its previously buried Thug. */
#include "msim.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../tests/fixtures/pose-contact/original-contact.h"

static void plane(mech3d_part *p, int y_left, int y_right, int type)
{
    wtb_object *o;
    int i;
    memset(p, 0, sizeof *p);
    p->coll = type;
    p->model.objects = calloc(1, sizeof *p->model.objects);
    p->model.object_count = 1;
    o = p->model.objects; assert(o);
    o->verts = calloc(4, sizeof *o->verts);
    o->polys = calloc(1, sizeof *o->polys);
    o->vert_count = 4; o->poly_count = 1;
    assert(o->verts && o->polys);
    o->verts[0].x = -10000; o->verts[0].z = -10000; o->verts[0].y = y_left;
    o->verts[1].x =  10000; o->verts[1].z = -10000; o->verts[1].y = y_right;
    o->verts[2].x =  10000; o->verts[2].z =  10000; o->verts[2].y = y_right;
    o->verts[3].x = -10000; o->verts[3].z =  10000; o->verts[3].y = y_left;
    o->polys[0].n = 4;
    for (i = 0; i < 4; i++) o->polys[0].idx[i] = (uint16_t)i;
}

static msim *sim_new(int actors)
{
    msim *s = calloc(1, sizeof *s); assert(s);
    s->player_unit_class = 1; s->player_centre_h = 632;
    s->actor_count = actors;
    s->actors = calloc((size_t)(actors ? actors : 1), sizeof *s->actors);
    s->units = calloc((size_t)(actors ? actors : 1), sizeof *s->units);
    assert(s->actors && s->units);
    return s;
}

static void sim_delete(msim *s)
{
    world_actor *actors = s->actors;
    msim_free(s); free(actors); free(s);
}

static void original_window(void)
{
    size_t row;
    for(row=0;row<sizeof original_contacts/sizeof original_contacts[0];row++) {
        mech3d w={0};msim *s=sim_new(0);int i,v;
        w.part_count=original_contacts[row].count;
        w.parts=calloc((size_t)(w.part_count?w.part_count:1),sizeof *w.parts);assert(w.parts);
        for(i=0;i<w.part_count;i++) {
            const int *surface=original_contacts[row].surface[i];
            plane(&w.parts[i],surface[1],surface[1],surface[0]);
            if(!surface[2])for(v=0;v<4;v++)w.parts[i].model.objects[0].verts[v].x+=30000;
        }
        msim_set_world(s,&w);
        {
            float actual=msim_ground(s,0,0,(float)original_contacts[row].y);
            if(actual!=(float)original_contacts[row].height)
                fprintf(stderr,"original ground fixture %zu: native %g expected %d\n",row,(double)actual,original_contacts[row].height);
            assert(actual==(float)original_contacts[row].height);
        }
        sim_delete(s);mech3d_free(&w);
    }
    printf("PASS: %zu executed-original ground windows, boundaries, roofs, missing surfaces and orderings\n",
           sizeof original_contacts/sizeof original_contacts[0]);
}

static void synthetic(void)
{
    mech3d w = {0}; msim *s; int i;
    w.parts = calloc(2, sizeof *w.parts); assert(w.parts);
    w.part_count = 1;
    plane(&w.parts[0], 1000, 3000, 5);
    s = sim_new(9);
    for (i = 0; i < 8; i++) s->actors[i].unit_class = i + 1;
    s->actors[8].unit_class = 1; s->units[8].y = 3500; s->units[8].vy = -2;
    s->actors[8].mech.origin[1] = 3500;
    msim_set_world(s, &w);
    assert(fabsf(s->player_unit.y - 2000) < .01f);
    assert(fabsf(s->player_unit.ground - 2000) < .01f);
    for (i = 0; i < 8; i++) {
        /* Class2 actors retain their authored baseline at load; the
         * original movement publication supplies their absolute altitude.
         * See aircraft-coordinates/original for the executed class rule. */
        int cls = i + 1, raised = cls == 1 || cls == 4 || cls == 6;
        assert(fabsf(s->units[i].y - (raised ? 2000.0f : 0.0f)) < .01f);
        assert(s->actors[i].mech.origin[1] == (raised ? 2000 : 0));
    }
    assert(s->units[8].y == 3500 && s->units[8].vy == -2);
    /* The original Mercs runtime selection is unchanged, including its strict
     * lower limit and its fall-back to a surface below the querying centre. */
    assert(msim_ground(s, 0, 0, 632) == 0);
    assert(msim_ground(s, 0, 0, 1000) == 0);
    assert(fabsf(msim_ground(s, 0, 0, 1001) - 2000) < .01f);
    assert(fabsf(msim_ground(s, 0, 0, 5000) - 2000) < .01f);
    s->player_unit.y = 0; s->units[0].y = 0;
    msim_set_world(s, &w); /* destructible world rebuild, not a new spawn */
    assert(s->player_unit.y == 0 && s->units[0].y == 0);
    sim_delete(s);mech3d_free(&w);

    /* A roof above the road must not replace the road as spawn support. */
    memset(&w, 0, sizeof w); w.part_count = 2;
    w.parts = calloc(2, sizeof *w.parts); assert(w.parts);
    plane(&w.parts[0], 0, 0, 5); plane(&w.parts[1], 2500, 2500, 0);
    s = sim_new(1); s->actors[0].unit_class = 1; s->units[0].y = -100;
    msim_set_world(s, &w);
    assert(s->player_unit.y == 0 && s->units[0].y == 0);
    sim_delete(s);
    /* Also preserve an elevated start under that roof. */
    s = sim_new(0); s->player_unit.y = 1200; s->player_unit.vy = -3;
    msim_set_world(s, &w);
    assert(s->player_unit.y == 1200 && s->player_unit.vy == -3);
    sim_delete(s); mech3d_free(&w);

    /* A bridge deck and its road can share one terrain model. Put the deck
     * first so choosing the quadtree's first surface would fail this case. */
    memset(&w, 0, sizeof w); w.part_count = 1;
    w.parts = calloc(1, sizeof *w.parts); assert(w.parts);
    plane(&w.parts[0], 2500, 2500, 5);
    {
        wtb_object *o = w.parts[0].model.objects;
        o->verts = realloc(o->verts, 8 * sizeof *o->verts);
        o->polys = realloc(o->polys, 2 * sizeof *o->polys);
        assert(o->verts && o->polys);
        o->vert_count = 8; o->poly_count = 2;
        o->polys[1] = o->polys[0];
        for (i = 0; i < 4; i++) {
            o->verts[i + 4] = o->verts[i]; o->verts[i + 4].y = 0;
            o->polys[1].idx[i] = (uint16_t)(i + 4);
        }
    }
    s = sim_new(1); s->actors[0].unit_class = 1; s->units[0].y = -100;
    msim_set_world(s, &w);
    assert(s->player_unit.y == 0 && s->units[0].y == 0);
    sim_delete(s);
    /* GRA5-style overlapping faces: the lower face is not selectable by
     * the runtime quadtree and lies above the implicit plane. Placing a
     * buried spawn there makes it fall through on the first update. */
    for(i=4;i<8;i++)w.parts[0].model.objects[0].verts[i].y=500;
    s=sim_new(1);s->actors[0].unit_class=1;
    msim_set_world(s,&w);
    assert(s->units[0].y==2500);
    assert(msim_ground(s,0,0,s->units[0].y+632)==2500);
    sim_delete(s);
    /* An authored elevated start keeps its height even in that geometry. */
    s=sim_new(1);s->actors[0].unit_class=1;s->units[0].y=1000;
    msim_set_world(s,&w);assert(s->units[0].y==1000);
    sim_delete(s);mech3d_free(&w);

    /* Without a terrain polygon there is no evidence to relocate a spawn. */
    memset(&w, 0, sizeof w); w.part_count = 1;
    w.parts = calloc(1, sizeof *w.parts); assert(w.parts);
    plane(&w.parts[0], 2500, 2500, 0);
    s = sim_new(0); msim_set_world(s, &w);
    assert(s->player_unit.y == 0);
    sim_delete(s); mech3d_free(&w);
    puts("PASS: buried mobile spawns, elevated starts, fixed/air units, roofs, stacked terrain, missing terrain and rebuilds");
}

static void mission(const char *path)
{
    char error[256]; prj_archive *a; mech3d world = {0};
    world_actor *actors = NULL; int count = 0, i, thug = -1; msim *s;
    a = prj_open(path, error, sizeof error);
    if (!a) { fprintf(stderr, "%s\n", error); exit(1); }
    assert(world3d_mission(a, "TRA1SCN1", 1, &world) >= 0);
    assert(world3d_actors(a, "TRA1SCN1", &actors, &count) == 0);
    s = calloc(1, sizeof *s); assert(s);
    assert(msim_init(s, a, "TRA1SCN1", actors, count) == 0);
    for (i = 0; i < count; i++) if (!strcmp(actors[i].skel, "THUG")) thug = i;
    assert(thug >= 0 && s->units[thug].y == 0);
    msim_set_world(s, &world);
    assert(s->units[thug].y > 1600 && s->units[thug].y < 1800);
    assert(fabsf(msim_ground(s, (float)actors[thug].mech.origin[0], (float)actors[thug].mech.origin[2],
                            s->units[thug].y + s->centre_h[thug]) - s->units[thug].y) < 1);
    assert(abs(actors[thug].mech.origin[1] - (int)s->units[thug].y) <= 1);
    printf("PASS: TRA1 authored Thug start now %.3f cm; ordinary ground query agrees\n", s->units[thug].y);
    msim_free(s); free(s);
    for (i = 0; i < count; i++) mech3d_free(&actors[i].mech);
    free(actors); mech3d_free(&world); prj_close(a);
}

/* Run normal NPC motion over the authored mission geometry. The companion
 * runner retains the existing FALLSIM deep-burial diagnostics. This does not
 * synthesize movement commands or replace the original terrain resources. */
static void mission_motion(const char *path,int count,const char **names)
{
    char error[256];prj_archive *a=prj_open(path,error,sizeof error);int scene;
    assert(a);
    for(scene=0;scene<count;scene++) {
        mech3d world={0};world_actor *actors=NULL;msim *s=calloc(1,sizeof *s);
        int actor_count=0,i,frame;float *before;
        assert(s);
        if(world3d_mission(a,names[scene],1,&world)<0||
           world3d_actors(a,names[scene],&actors,&actor_count)||
           msim_init(s,a,names[scene],actors,actor_count)) {
            fprintf(stderr,"motion fixture cannot load %s\n",names[scene]);abort();
        }
        before=calloc((size_t)(actor_count?actor_count:1),sizeof *before);assert(before);
        for(i=0;i<actor_count;i++)before[i]=s->units[i].y;
        msim_set_world(s,&world);
        /* Spawn recovery must preserve the authored vertical placement of
         * fixed units and aircraft, including later simulation rebuilds. */
        for(i=0;i<actor_count;i++) {
            int cls=actors[i].unit_class;
            if(cls!=1&&cls!=4&&cls!=6)assert(before[i]==s->units[i].y);
            else assert(s->units[i].y>=before[i]);
        }
        free(before);
        fprintf(stderr,"TERRAIN_SCENE %s actors=%d\n",names[scene],actor_count);
        s->player_vertical=1;
        for(frame=0;frame<360;frame++)msim_step(s,frame%3==2?34:33);
        for(i=0;i<actor_count;i++) {
            assert(isfinite(s->units[i].y)&&isfinite(s->units[i].ground));
            assert(isfinite(actors[i].mech.heading));
        }
        printf("MOTION %s actors=%d simulated_ms=%d\n",names[scene],actor_count,s->now);
        msim_free(s);free(s);
        for(i=0;i<actor_count;i++)mech3d_free(&actors[i].mech);
        free(actors);mech3d_free(&world);
    }
    prj_close(a);
}

int main(int argc, char **argv)
{
    if(argc>3&&!strcmp(argv[1],"--motion")) {
        mission_motion(argv[2],argc-3,(const char **)argv+3);return 0;
    }
    original_window();synthetic();
    if (argc > 1) mission(argv[1]);
    return 0;
}
