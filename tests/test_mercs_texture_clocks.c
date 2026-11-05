/* Actual original-instruction fixtures plus real archive pool integration.
 * No original executable bytes are embedded in this test. */
#include "fx.h"
#include "../tests/fixtures/fidelity-followup/original-texture-clocks.h"
#include "../tests/fixtures/fidelity-followup/original-texture-bindings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) ((int)(sizeof(a)/sizeof((a)[0])))

static mercs_texclock clock_from(const int a[5])
{
    mercs_texclock c;
    c.frame=a[0]; c.rate=a[1]; c.mode=a[2]; c.state=a[3]; c.stamp=a[4];
    return c;
}
static void check(const mercs_texclock *c, const int a[5])
{
    assert(c->frame==a[0] && c->rate==a[1] && c->mode==a[2] && c->state==a[3] && c->stamp==a[4]);
}
static void act(mercs_texclock *c, const int cel[32], int action, int value)
{
    switch(action) {
    case 0: mercs_texclock_step(c,cel,value); break;
    case 1: mercs_texclock_mode(c,value); break;
    case 2: mercs_texclock_reset(c,cel,(unsigned)value); break;
    case 3: c->rate=(uint16_t)value; break;
    default: assert(0);
    }
}
static void original_cases(void)
{
    int i,k;
    mercs_texclock c[3]={{0,7,0,1,-1},{0,7,0,1,-1},{0,45,1,1,-1}};
    const int cel[32]={101,102,103,104,105};
    for(i=0;i<COUNT(original_texture_cases);i++) {
        const original_texture_case *f=&original_texture_cases[i];
        mercs_texclock x=clock_from(f->before);
        act(&x,f->cel,f->action,f->value);
        check(&x,f->after);
    }
    for(i=0;i<COUNT(original_texture_graph);i++) {
        const original_texture_graph_step *f=&original_texture_graph[i];
        if(!f->action) for(k=0;k<3;k++) act(&c[k],cel,0,f->value);
        else act(&c[f->slot],cel,f->action,f->value);
        for(k=0;k<3;k++) check(&c[k],f->after[k]);
    }
    for(i=0;i<COUNT(original_fx_distances);i++) {
        const original_fx_distance_case *f=&original_fx_distances[i];
        assert(fx_near_distance(f->delta[0],f->delta[1],f->delta[2])==f->distance);
    }
    printf("PASS: %d original DOS/Windows texture-service fixtures and %d-step shared-group graph\n",
           COUNT(original_texture_cases),COUNT(original_texture_graph));
    printf("PASS: %d original Windows integer proximity distances (including signed wrap boundaries)\n",COUNT(original_fx_distances));
}

static void binding_cases(void)
{
    mercs_texbinding slots[512];
    mercs_texmap_reader reader;
    bwd_chunk chunk;
    bwd_record record;
    bwd_mission mission;
    unsigned char bytes[6];
    int i;
    memset(&record, 0, sizeof record); memset(&mission, 0, sizeof mission);
    record.chunks = &chunk; record.chunk_count = 1;
    mission.records = &record; mission.record_count = 1;
    for (i = 0; i < COUNT(original_binding_steps); i++) {
        const original_binding_step *f = &original_binding_steps[i];
        static const char *tags[] = {"BMPJ", "BMID", "BSEC", "BMEN", "XPLO"};
        const mercs_texbinding *slot;
        int id = f->action == 4 ? 0 : f->id;
        int value = f->action == 4 ? f->id : f->value;
        if (f->first) {
            memset(slots, 0, sizeof slots); memset(&reader, 0, sizeof reader);
            reader.slots = slots;
        }
        memset(&chunk, 0, sizeof chunk); strcpy(chunk.tag, tags[f->action]);
        bytes[0] = (unsigned char)id; bytes[1] = (unsigned char)((unsigned)id >> 8);
        bytes[2] = (unsigned char)value; bytes[3] = (unsigned char)((unsigned)value >> 8);
        bytes[4] = 13; bytes[5] = 0;
        chunk.data = bytes; chunk.size = f->action == 4 ? 6 : 4;
        assert(!mercs_texmap_chunk(&mission, 0, 0, NULL, &reader));
        slot = &slots[f->watch];
        assert(!memcmp(slot->cel, f->cel, sizeof slot->cel));
        assert(slot->frames == f->frames && slot->rate == f->rate &&
               slot->mode == f->mode && slot->bound == f->bound);
    }
    printf("PASS: %d original loader states, including repeated binding, shared groups, single-frame rate retention and invalid slots\n", COUNT(original_binding_steps));
}

static void scene_bindings(prj_archive *a, fx_defs *d)
{
    int type = prj_find_type(a, "BWD"), row, scenes = 0, changed = 0;
    int expected = d->control[10][0] == 92 ? 14 : 20;
    for (row = 0; row < prj_symbol_count(a, type); row++) {
        const char *name = prj_symbol_name(a, type, row);
        size_t length = strlen(name);
        bwd_mission mission;
        bwd_texmap *old_map;
        int k, old_first = 0;
        if (length < 4 || strcmp(name + length - 4, "SCN1")) continue;
        assert(!bwd_mission_load(a, name, &mission));
        old_map = malloc(sizeof *old_map); assert(old_map);
        /* Reproduce the old breadth-first selection independently so the
         * supplied asset catches a regression to inventory order. */
        for (k = 0; k < mission.record_count; k++) {
            const bwd_record *rec = &mission.records[k];
            assert(!bwd_texmap_decode(rec->data, rec->size, old_map));
            if (old_map->present[76])
                old_first = prj_find_id(a, prj_find_type(a, "CEL"), old_map->name[76]);
        }
        assert(!fx_bind_scene(a, name, d));
        assert(d->scene_bound && d->tex[332].bound);
        if (old_first != d->tex[332].cel[0]) {
            fx_world *world = calloc(1, sizeof *world);
            const float origin[3] = {0, 0, 0};
            int effect;
            assert(world && d->tex[332].cel[0] == 118 && d->tex[332].frames == 32);
            /* A repeated MW2_XPL1 after XPLO rebinds mode1. Effect expiry
             * still stops this same slot, and creation restarts it. */
            assert(d->tex[332].mode == 1 && d->tex[332].rate == 16);
            fx_clocks_init(world, d); assert(world->clocks[332].mode == 1);
            effect = fx_spawn(world, d, 13, origin, 10, 6); assert(effect >= 0);
            assert(fx_frame(world, d, &world->e[effect]) == 118);
            world->e[effect].remaining = 1; fx_update(world, 1);
            assert(!world->clocks[332].mode);
            free(world); changed++;
        }
        scenes++;
        free(old_map); bwd_mission_free(&mission);
    }
    assert(scenes == 78 && changed == expected);
    printf("PASS: %d authored scene binding streams; %d breadth-first slot regressions detected and corrected\n", scenes, changed);
}

static void archive_cases(const char *path)
{
    char error[256];
    prj_archive *a=prj_open(path,error,sizeof error);
    fx_defs *d=calloc(1,sizeof *d);
    fx_world *w=calloc(1,sizeof *w);
    const float p[3]={0,0,0}, far[3]={2000,0,0};
    int i,t,count=0,first,second,alias,control,other,frame;
    if(!a) { fprintf(stderr,"%s\n",error); exit(1); }
    assert(d && w && fx_load(a,d)==0);
    for(t=0;t<FX_TYPES;t++) {
        assert(d->slots[t]<=FX_CARD_SLOTS);
        for(i=0;i<d->slots[t];i++) {
            int r=d->render[t][i],c=d->control[t][i];
            if(c>=0) assert(c<512 && d->tex[c].bound);
            if(r>=0) assert(r<512 && d->tex[r].bound);
            count++;
        }
    }
    assert(count==226 && d->slots[23]==32);
    /* Missile impacts and type34 reuse92..99. Retail DOS burn controls92
     * while its polygons read118; accelerated archives fix the control to118.
     * Preserve the authored edition difference and the two distinct roles. */
    assert(d->control[3][0]==92 && d->control[34][0]==92);
    assert(d->render[3][0]==92 && d->render[34][0]==92 && d->render[10][0]==118);
    assert(d->control[10][0]==92 || d->control[10][0]==118);
    fx_set_timing(1); fx_detail_low=0;
    first=fx_spawn(w,d,3,p,100,6); assert(first>=0);
    fx_animate(w,d,100); fx_animate(w,d,100+d->tex[92].rate*2);
    assert(w->clocks[92].frame==2);
    second=fx_spawn(w,d,3,far,120,6); assert(second>=0);
    other=w->e[second].control_slot; assert(other==93);
    assert(w->clocks[92].frame==2 && w->clocks[93].frame==0);
    alias=fx_spawn(w,d,34,p,130,6); assert(alias>=0);
    assert(w->e[alias].control_slot==92 && w->clocks[92].frame==0);
    assert(fx_frame(w,d,&w->e[first])==fx_frame(w,d,&w->e[alias]));
    fx_animate(w,d,200); fx_animate(w,d,200+d->tex[92].rate);
    frame=w->clocks[93].frame;
    assert(w->clocks[92].frame==1);
    w->e[first].remaining=1; w->e[alias].remaining=w->e[second].remaining=1000;
    fx_update(w,1);
    assert(!w->e[first].alive && w->e[alias].alive && !w->clocks[92].mode);
    assert(fx_frame(w,d,&w->e[alias])==-1);
    assert(w->clocks[93].frame==frame && w->clocks[93].mode);

    /* All32 muzzle objects use their authored render and control binding,
     * including the repeated control340 at the end of the pool. */
    memset(w,0,sizeof *w);
    for(i=0;i<32;i++) {
        int at=fx_spawn(w,d,23,p,100,6);
        assert(at>=0 && w->e[at].slot==i);
        assert(w->e[at].texture_slot==d->render[23][i]);
        assert(w->e[at].control_slot==d->control[23][i]);
    }
    assert(fx_spawn(w,d,23,p,100,6)==-1);
    control=d->control[23][17]; assert(control==335 && d->control[23][31]==340);
    assert(d->render[23][17]!=d->render[23][0]);
    /* Two nuclear types also intentionally alias the same texture slots. */
    for(i=0;i<4;i++) assert(d->control[22][i]==186+i && d->control[35][i]==186+i);
    printf("PASS: %s — 226 pool bindings; independent slots, shared reset/stop, 32 muzzle models and cross-type aliases\n",path);
    scene_bindings(a, d);
    free(w);free(d);prj_close(a);
}
int main(int argc,char **argv)
{
    int i;
    original_cases();
    binding_cases();
    for(i=1;i<argc;i++) archive_cases(argv[i]);
    return 0;
}
