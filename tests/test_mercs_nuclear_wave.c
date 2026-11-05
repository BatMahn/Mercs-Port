/* Private comparison gate: exact original singleton snapshots plus real native
 * consumers. The included existing dispatch fixture remains a separate gate.
 * No original world intersections, palette display, saved career or mission
 * victory is claimed. MERCS_HAVE_NUCLEAR_WAVE=0 runs real before-source full
 * step controls; absent new internals are reported as untested, not passed. */
#include "combat.h"
static void observed_damage(combat_unit *, float, int, unsigned *);
#define combat_damage_at observed_damage
#define main existing_dispatch_main
#include "test_mercs_mission_dispatch.c"
#undef main
#undef combat_damage_at
#include "original-wave-fixtures.h"
#ifndef MERCS_HAVE_NUCLEAR_WAVE
#define MERCS_HAVE_NUCLEAR_WAVE 1
#endif

static msim *observed_sim;
static unsigned damage_calls, golden_ops, area_state_observations, consumer_cases, phase_cases;
static int capture_only;
static float damage_values[32];
#if MERCS_HAVE_NUCLEAR_WAVE
static nuclear_original_state damage_state;
static nuclear_original_state state_of(const msim *s)
{
    nuclear_original_state q; int k;
    for(k=0;k<3;k++) q.center_cm[k]=(int32_t)s->nuclear_wave.pos[k];
    q.radius_cm=s->nuclear_wave.radius_cm;
    q.radius_cap_cm=s->nuclear_wave.radius_cap_cm;
    q.remaining_ticks=s->nuclear_wave.remaining_ticks;
    return q;
}
static void put_state(msim *s, const nuclear_original_state *q)
{
    int k; for(k=0;k<3;k++) s->nuclear_wave.pos[k]=(float)q->center_cm[k];
    s->nuclear_wave.radius_cm=q->radius_cm;
    s->nuclear_wave.radius_cap_cm=q->radius_cap_cm;
    s->nuclear_wave.remaining_ticks=q->remaining_ticks;
}
static void compare_state(const nuclear_original_state *got, const nuclear_original_state *want, const char *label)
{
    int k; for(k=0;k<3;k++) equal(got->center_cm[k],want->center_cm[k],label);
    equal(got->radius_cm,want->radius_cm,label);
    equal(got->radius_cap_cm,want->radius_cap_cm,label);
    equal(got->remaining_ticks,want->remaining_ticks,label);
}
#endif
static void observed_damage(combat_unit *u, float amount, int location, unsigned *rng)
{
    if(observed_sim) {
#if MERCS_HAVE_NUCLEAR_WAVE
        if(!damage_calls) damage_state=state_of(observed_sim);
#endif
        if(damage_calls<COUNT(damage_values)) damage_values[damage_calls]=amount;
        damage_calls++;
    }
    if(!capture_only) combat_damage_at(u,amount,location,rng);
}
typedef struct {
    capture base;
    nuclear_original_state effect_state;
    unsigned effect_damage_calls;
} wave_capture;
static void wave_effect(void *opaque, int type, const float p[3])
{
    wave_capture *c=opaque;
    if(type==22) {
        c->base.count++; c->base.type=type; memcpy(c->base.pos,p,sizeof c->base.pos);
        if(c->base.s->minds && c->base.s->actor_count>1) c->base.member_state=c->base.s->minds[1].state;
        c->base.s->fx_refused=c->base.refuse;
        c->base.s->fx_instance=c->base.instance;
        c->base.s->fx_generation=c->base.generation;
        c->effect_damage_calls=damage_calls;
#if MERCS_HAVE_NUCLEAR_WAVE
        c->effect_state=state_of(c->base.s);
#endif
    }
}
static msim *wave_scene(wave_capture *c, const int p[3])
{
    msim *s; memset(c,0,sizeof *c); s=synthetic(&c->base,5,p);
    s->shot_user=c; s->on_effect=wave_effect;
    /* These declared post-startup consumers follow the archive-initialization
     * gate above, which deliberately resets the shared startup latch. */
    combat_damage_live=1;
    return s;
}
#if MERCS_HAVE_NUCLEAR_WAVE
static float total_health(const combat_unit *u)
{
    float q=0; int k; for(k=0;k<8;k++) q+=u->armor[k]+u->rear[k]+u->internal[k];
    return q;
}

static void original_sequences(void)
{
    unsigned ci,j; int k;
    for(ci=0;ci<NUCLEAR_ORIGINAL_CASES;ci++) {
        const nuclear_original_case *f=&nuclear_original_cases[ci];
        wave_capture c; msim *s=calloc(1,sizeof *s);
        if(!s) exit(2);
        memset(&c,0,sizeof c); c.base.s=s; c.base.instance=186;
        c.base.generation=7; s->shot_user=&c; s->on_effect=wave_effect;
        healthy(&s->player_unit); s->player_radius=10; s->fx_radius[22]=40;
        case_name=f->name; fx_set_timing(f->dos_profile); put_state(s,&f->initial);
        observed_sim=s; capture_only=1;
        for(j=0;j<f->op_count;j++) {
            const nuclear_original_op *op=&nuclear_original_ops[f->first_op+j];
            nuclear_original_state got=state_of(s); int prior_effects=c.base.count;
            int prior_areas=s->area_count;
            compare_state(&got,&op->before,"original before state");
            damage_calls=0;
            if(op->kind==1) {
                float p[3]; for(k=0;k<3;k++) p[k]=(float)op->point_cm[k];
                c.base.refuse=op->allocation_refused; nuclear_start(s,p);
                equal(c.base.count-prior_effects,op->effect_count,"original effect requests");
                equal(c.base.type,op->effect_args[2],"original type22");
                for(k=0;k<3;k++) equal(c.base.pos[k],op->effect_args[3+k],"original producer body point");
                compare_state(&c.effect_state,&op->at_effect,"state already armed at allocation callback");
                equal(s->area_count,prior_areas+!op->allocation_refused,"old ordinary areas retained on replacement/refusal");
                equal(damage_calls,0,"producer does not apply singleton damage immediately");
                if(!op->allocation_refused) equal(s->area[s->area_count-1].owner,op->effect_args[0],"original ordinary effect owner");
            } else {
                s->player[0]=s->nuclear_wave.pos[0]; s->player[2]=s->nuclear_wave.pos[2];
                s->player_unit.y=s->nuclear_wave.pos[1]; s->player_unit.heat=0;
                nuclear_step(s,op->raw_ticks);
                if(op->area_count && op->after.radius_cm>=0) {
                    int32_t q=(int32_t)((int64_t)op->raw_ticks*op->unit_args[5]);
                    equal(damage_calls,8,"live wave reaches all eight native location dispatches");
                    compare_state(&damage_state,&op->at_area,"post-growth pre-subtraction state at native damage");
                    area_state_observations++;
                    equal(damage_values[0],(float)(q>>2)/65536.0f,"original unit coefficient at head consumer");
                    for(k=1;k<8;k++) equal(damage_values[k],(float)q/65536.0f,"original unit coefficient at other consumers");
                } else equal(damage_calls,0,"inactive or declared negative-radius native unit exclusion");
                equal(s->player_unit.heat,0,"original singleton heat coefficient zero");
                equal(c.base.count,prior_effects,"updater does not request another visual");
            }
            got=state_of(s); compare_state(&got,&op->after,"original after state"); golden_ops++;
        }
        observed_sim=NULL; capture_only=0; free(s);
    }
}
static void ordinary_independence(void)
{
    int dos,refuse; const int p[3]={0,0,0};
    for(dos=0;dos<2;dos++) for(refuse=0;refuse<2;refuse++) {
        wave_capture c; msim *s=wave_scene(&c,p); float before,after; int life;
        case_name=refuse?"real singleton with ordinary visual refused":"real independent singleton plus ordinary type22";
        fx_set_timing(dos); c.base.refuse=refuse; s->splash=0; published_instant(s,5);
        node_assign(s,5,0); equal(c.base.count,1,"one real leader producer");
        equal(s->nuclear_wave.remaining_ticks,dos?3640:3620,"profile wave life");
        equal(s->area_count,!refuse,"ordinary allocation independent");
        before=total_health(&s->units[1]); observed_sim=s; damage_calls=0;
        nuclear_step(s,6); after=total_health(&s->units[1]); life=s->nuclear_wave.remaining_ticks;
        equal(before-after,2.71875,"real eight-location wave damage with splash disabled");
        equal(s->units[1].armor[0],99.90625,"real quarter head damage");
        equal(s->units[1].heat,0,"wave does not heat");
        equal(damage_calls,16,"both armed nearby actors receive native damage");
        area_damage(s,6);
        equal(after-total_health(&s->units[1]),refuse?0:5.4375,"ordinary coefficient remains independently active");
        equal(s->units[1].heat,refuse?0:0.09375,"only ordinary type22 contributes heat");
        equal(s->nuclear_wave.remaining_ticks,life,"ordinary effect update does not update singleton twice");
        observed_sim=NULL; destroy_synthetic(s); consumer_cases++;
    }
    {
        wave_capture c; msim *s=wave_scene(&c,p); const float point[3]={0,0,0};
        case_name="ordinary type22 impact is not the mission nuclear producer";
        effect(s,22,point,0); equal(s->area_count,1,"ordinary type22 exists");
        equal(s->nuclear_wave.remaining_ticks,0,"ordinary type22 never arms singleton");
        area_damage(s,6); equal(s->nuclear_wave.remaining_ticks,0,"ordinary update never arms singleton");
        destroy_synthetic(s); consumer_cases++;
    }
}
static void consumer_boundaries(void)
{
    const int p[3]={0,0,0}; wave_capture c; msim *s=wave_scene(&c,p);
    float before; int k;
    case_name="real wave radius, unit exclusion and final full-delta boundaries";
    c.base.refuse=1; published_instant(s,5); node_assign(s,5,0);
    s->actors[0].mech.origin[0]=10011; s->actors[1].mech.origin[0]=10010;
    s->radius[0]=s->radius[1]=10; s->splash=0;
    before=total_health(&s->units[1]); nuclear_step(s,50);
    equal(s->nuclear_wave.radius_cm,10000,"wave caps at original100m");
    equal(total_health(&s->units[1]),before,"strict tangent exclusion");
    s->actors[1].mech.origin[0]=10009; nuclear_step(s,6);
    equal(before-total_health(&s->units[1]),2.71875,"one centimetre inside cap hits");
    before=total_health(&s->units[1]); s->armed[1]=0; nuclear_step(s,6);
    equal(total_health(&s->units[1]),before,"unarmed native actor excluded");
    s->armed[1]=1; s->units[1].body_detached=1; nuclear_step(s,6);
    equal(total_health(&s->units[1]),before,"detached controller excluded");
    s->units[1].body_detached=0; s->units[1].destroyed=1; nuclear_step(s,6);
    equal(before-total_health(&s->units[1]),5.4375,"state4 native area damage doubles");
    s->units[1].intact_out=1; before=total_health(&s->units[1]); nuclear_step(s,6);
    equal(before-total_health(&s->units[1]),2.71875,"state5 does not double damage");
    s->units[1].destroyed=s->units[1].intact_out=0;
    s->nuclear_wave.remaining_ticks=1; before=total_health(&s->units[1]); nuclear_step(s,6);
    equal(before-total_health(&s->units[1]),2.71875,"final positive timer receives entire six-tick damage");
    equal(s->nuclear_wave.remaining_ticks,-5,"final raw timer overshoot retained");
    before=total_health(&s->units[1]); nuclear_step(s,6);
    equal(total_health(&s->units[1]),before,"expired wave does not damage again");
    for(k=0;k<2;k++) equal(s->units[k].heat,0,"wave never adds heat in any state");
    destroy_synthetic(s); consumer_cases++;
}
static void things_and_debris(void)
{
    const int p[3]={0,0,0}; wave_capture c; msim *s=wave_scene(&c,p); int k;
    mech3d world={0}; mech3d_part part={0}; wtb_object object={0}; wtb_vertex vertices[2]={{0},{0}};
    case_name="real native GT and detached-debris consumers";
    world.parts=&part; world.part_count=1; part.parent_obj=-1;
    part.model.objects=&object; part.model.object_count=1; object.verts=vertices; object.vert_count=2;
    vertices[0].x=-20; vertices[1].x=20; s->world=&world;
    s->bld=calloc(1,sizeof *s->bld); if(!s->bld) exit(2); s->bld_count=1;
    s->bld[0].hp=100; s->bld[0].destroyed=-1; s->bld[0].type=6;
    s->bld[0].mn[0]=s->bld[0].mx[0]=1000;
    s->debris_count=1; s->debris[0].kind=3; s->debris[0].part=0;
    s->debris[0].destructible=1; s->debris[0].health_q16=100; s->debris[0].p[0]=1000;
    c.base.refuse=1; published_instant(s,5); node_assign(s,5,0); s->splash=0;
    nuclear_step(s,8);
    equal(s->bld[0].hp,99,"4096 coefficient rounds half HP to one GT point");
    equal(s->debris[0].health_q16,99,"debris preserves whole-point-as-raw-Q16 original quirk");
    equal(s->buildings_destroyed,0,"nonlethal GT remains alive");
    equal(s->area_count,0,"neither consumer depends on accepted visual");
    /* Zero raw delta still dispatches; class0xb0's pre-existing consumer
     * destroys on a zero-point hit. This discriminates an early dt==0 exit. */
    s->bld[0].type=0xb0; nuclear_step(s,0);
    equal(s->bld[0].hp,0,"zero delta preserves class0xb0 dispatch");
    equal(s->buildings_destroyed,1,"class0xb0 normal destruction path");
    equal(s->debris[0].health_q16,99,"zero delta leaves ordinary debris health");
    for(k=0;k<2;k++) equal(s->units[k].heat,0,"no nuclear heat while world consumers run");
    s->world=NULL; destroy_synthetic(s); consumer_cases++;
}
static void source_identity(void)
{
    const int p[3]={0,0,0}; wave_capture c; msim *s=wave_scene(&c,p);
    mercs_car_record wanted, player_kill; mercs_car_unit_loss loss; int k;
    case_name="real lethal wave uses source minus two";
    s->msg_seen=calloc(2,sizeof *s->msg_seen); if(!s->msg_seen) exit(2);
    s->actors[1].alliance=1; s->actors[1].table=5; s->player_alliance=0;
    for(k=0;k<8;k++) s->units[1].armor[k]=s->units[1].internal[k]=0.01f;
    stats_loss_context(s,1,-2,4,&loss); memcpy(wanted.bytes,s->career,sizeof wanted.bytes);
    player_kill=wanted; equal(mercs_car_count_unit_loss(&wanted,&loss),0,"expected non-player loss consumer succeeds");
    loss.killer_is_player=1; equal(mercs_car_count_unit_loss(&player_kill,&loss),0,"discriminating player-source consumer succeeds");
    equal(memcmp(wanted.bytes,player_kill.bytes,sizeof wanted.bytes)!=0,1,"source identity affects real career counters");
    c.base.refuse=1; published_instant(s,5); node_assign(s,5,0); nuclear_step(s,6);
    equal(s->units[1].destroyed,1,"wave actually destroys declared fragile actor");
    equal((s->msg_seen[1]&4)!=0,1,"normal native loss recorder marks the event once");
    equal(memcmp(wanted.bytes,s->career,sizeof wanted.bytes),0,"actual recorded loss matches source minus two");
    destroy_synthetic(s); consumer_cases++;
}
#endif

static void whole_step_phase(void)
{
    static const int delta_ms[]={1,6,33,100,250};
    const int p[3]={79000,3200,19300}; int dos,j,refused,frame;
    for(dos=0;dos<2;dos++) for(j=0;j<COUNT(delta_ms);j++) for(refused=0;refused<2;refused++) {
        wave_capture c; msim *s=wave_scene(&c,p); int dt=delta_ms[j], total=0;
        float initial_head=s->units[1].armor[0];
        case_name=refused?"whole-step independent wave despite refusal":"whole-step wave plus retained ordinary effect";
        fx_set_timing(dos); combat_set_weapon_timing(dos); c.base.refuse=refused;
        s->now=141000-dt; s->fx_radius[22]=0; /* ordinary effect has no unit reach with radius0 below */
        s->radius[0]=s->radius[1]=0; s->player_unit.body_detached=1;
        /* Coincident leader/follower would introduce contact. Move the leader
         * out of both wave and ordinary range while preserving target order. */
        s->actors[0].mech.origin[0]=p[0]+20000;
        for(frame=0;frame<8;frame++) {
            msim_step(s,dt); native_steps++;
            equal(c.base.count,frame?1:0,"one producer for every tested frame interval");
            if(frame<2) equal(s->units[1].armor[0],initial_head,"no wave damage before following step");
            if(frame>=2) total+=s->fx_delta_ticks;
#if MERCS_HAVE_NUCLEAR_WAVE
            equal(s->nuclear_wave.remaining_ticks,frame?2*fx_duration(22)-total:0,"full-step timer phase uses integer FX delta");
            if(frame==1) equal(s->nuclear_wave.radius_cm,0,"new producer waits for next singleton phase");
#endif
        }
        equal(total>0,1,"declared full-step sequence includes at least one integer tick");
        equal(initial_head-s->units[1].armor[0],(float)total/64.0f,"real full-step wave damage after allocation decision");
        equal(s->units[1].heat,0,"zero-radius ordinary area does not mask singleton evidence");
        equal(s->area_count,!refused,"ordinary visual area remains independent");
        equal(s->logic.result[0],0,"fixture supplies no mission victory");
        destroy_synthetic(s); phase_cases++;
    }
}
int main(int argc,char **argv)
{
    unsigned first_checks,first_failures; int old_result;
    old_result=existing_dispatch_main(argc,argv);
    first_checks=checks; first_failures=failures;
    observed_sim=NULL; capture_only=0;
#if MERCS_HAVE_NUCLEAR_WAVE
    original_sequences(); ordinary_independence(); consumer_boundaries(); things_and_debris(); source_identity();
#endif
    whole_step_phase();
    printf("{\"summary\":true,\"checks\":%u,\"failures\":%u,\"wave_checks\":%u,\"wave_failures\":%u,\"has_singleton_internals\":%d,\"original_state_operations\":%u,\"original_states_observed_at_native_damage\":%u,\"real_consumer_cases\":%u,\"whole_step_sequences\":%u,\"native_steps_total\":%u,\"original_palette_requests_excluded\":12,\"scope\":\"original state/effect-callback goldens; native real damage and phase controls; existing dispatch gate; no original geometry, palette display or mission wins\"}\n",checks,failures,checks-first_checks,failures-first_failures,MERCS_HAVE_NUCLEAR_WAVE,golden_ops,area_state_observations,consumer_cases,phase_cases,native_steps);
    return failures||old_result?1:0;
}
