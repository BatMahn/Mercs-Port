/* Captured original coordinate contracts through real native projectile and
 * frontend consumers. Sound OUTPUT is intercepted after the real snd_at
 * camera-relative calculation; no audio device/pixels/full mission executes.
 * The frontend's libc RNG service supplies zero, matching the original probe.
 * Native collision, direct damage, allocation, light/palette admission and
 * simulation RNG are not replaced. */
#include <stdlib.h>
#include "src/sfx.h"
static int fixture_rand(void);
static void fixture_sound(sfx *,int,float,float,float,float,int);
#define main projectile_effect_embedded_frontend_main
#define rand fixture_rand
#define sfx_play_at3 fixture_sound
#include "tools/glview.c"
#undef main
#undef rand
#undef sfx_play_at3
#include "projectile_effect_fixture.h"
#include "original-projectile-effect-fixtures.h"
#include <assert.h>

typedef struct {int type, area_before, area_after, pool_after, refused, scoped_valid, gt_hp;float point[3], scoped[3], arm_hp;} callback_event;
typedef struct {int id, half, pool, scoped_valid;float delta[3], yaw;} sound_event;
static callback_event callbacks[32];static sound_event sounds[32];
static int callback_count,sound_count,rng_calls;
static int checks,failures,unavailable,producer_rows,consumer_rows,light_rows,full_steps,native_cases;
static uint32_t physics_digest=2166136261u;
static const char *context="startup";

static void equal(const char *field,double got,double want,double tolerance)
{
    checks++;
    if(!isfinite(got)||!isfinite(want)||fabs(got-want)>tolerance){
        failures++;fprintf(stderr,"FAIL %s / %s: %.12g expected %.12g (tol %.4g)\n",context,field,got,want,tolerance);
    }
}
static void xyz(const char *field,const float got[3],const float want[3],double tolerance)
{int k;for(k=0;k<3;k++)equal(field,got[k],want[k],tolerance);}
static void digest_bytes(const void *p,size_t n)
{const unsigned char *b=p;while(n--){physics_digest^=*b++;physics_digest*=16777619u;}}
static void actual_step(void)
{
    int i;effect_fixture_step(&g_sim,1);full_steps++;
    /* A paired-run checksum of unchanged physical results, not a portable
     * golden or an original state-layout assertion. No FX presentation data. */
    digest_bytes(&g_sim.rng,sizeof g_sim.rng);digest_bytes(&g_sim.hits,sizeof g_sim.hits);
    digest_bytes(&g_sim.shot_count,sizeof g_sim.shot_count);
    for(i=-1;i<g_sim.actor_count;i++){
        const combat_unit *u=i<0?&g_sim.player_unit:&g_sim.units[i];
        digest_bytes(u->armor,sizeof u->armor);digest_bytes(u->rear,sizeof u->rear);
        digest_bytes(u->internal,sizeof u->internal);digest_bytes(&u->heat,sizeof u->heat);
        digest_bytes(&u->destroyed,sizeof u->destroyed);
    }
    for(i=0;i<g_sim.bld_count;i++)digest_bytes(&g_sim.bld[i].hp,sizeof g_sim.bld[i].hp);
    digest_bytes(&g_sim.area_count,sizeof g_sim.area_count);
    for(i=0;i<g_sim.area_count;i++){digest_bytes(g_sim.area[i].pos,sizeof g_sim.area[i].pos);digest_bytes(&g_sim.area[i].owner,sizeof g_sim.area[i].owner);}
    digest_bytes(&g_sim.debris_count,sizeof g_sim.debris_count);
}
static int live_count(void)
{int i,n=0;for(i=0;i<FX_LIVE;i++)n+=g_fxw.e[i].alive!=0;return n;}
static fx_live *find_type(int type)
{int i;for(i=0;i<FX_LIVE;i++)if(g_fxw.e[i].alive&&g_fxw.e[i].type==type)return &g_fxw.e[i];return NULL;}
static int light_type(void)
{return g_fxw.light_slot>0?g_fxw.e[g_fxw.light_slot-1].type:-1;}
static int fixture_rand(void) {rng_calls++;return 0;}
static void fixture_sound(sfx *unused,int id,float dx,float dy,float dz,float yaw,int half)
{
    sound_event *e;(void)unused;assert(sound_count<COUNT(sounds));e=&sounds[sound_count++];
    e->id=id;e->half=half;e->delta[0]=dx;e->delta[1]=dy;e->delta[2]=dz;e->yaw=yaw;e->pool=live_count();
#if MERCS_EFFECT_COORDINATES
    e->scoped_valid=g_sim.fx_light_point_valid;
#else
    e->scoped_valid=-1;
#endif
}
static void recorded_frontend(void *user,int type,const float point[3])
{
    callback_event *e;assert(callback_count<COUNT(callbacks));e=&callbacks[callback_count++];
    memset(e,0,sizeof *e);e->type=type;memcpy(e->point,point,sizeof e->point);e->area_before=g_sim.area_count;
    e->gt_hp=g_sim.bld_count?g_sim.bld[0].hp:-1;e->arm_hp=g_sim.actor_count?g_sim.units[0].armor[4]:-1;
#if MERCS_EFFECT_COORDINATES
    e->scoped_valid=g_sim.fx_light_point_valid;memcpy(e->scoped,g_sim.fx_light_point,sizeof e->scoped);
#else
    e->scoped_valid=-1;
#endif
    on_effect(user,type,point);
    e->area_after=g_sim.area_count;e->pool_after=live_count();e->refused=g_sim.fx_refused;
}
static void clear_events(void)
{callback_count=sound_count=rng_calls=0;memset(callbacks,0,sizeof callbacks);memset(sounds,0,sizeof sounds);}
static void definitions(int slots)
{
    int i,j;memset(&g_fxd,0,sizeof g_fxd);
    for(i=0;i<FX_TYPES;i++){g_fxd.slots[i]=slots;g_fxd.anim[i]=-1;for(j=0;j<FX_CARD_SLOTS;j++)g_fxd.control[i][j]=g_fxd.render[i][j]=-1;}
}
static void configure(int profile)
{
    effect_fixture_cleanup(&g_sim);effect_fixture_base(&g_sim);memset(&g_fxw,0,sizeof g_fxw);definitions(4);
    combat_set_weapon_profile(profile);fx_set_timing(profile==COMBAT_PROFILE_DOS);
    /* The direct area calls below explicitly model the post-startup damage
     * latch. Full combat_step normally enables it after player startup. */
    combat_damage_live=1;
    g_sim.on_effect=recorded_frontend;g_sim.on_effect_retains=on_effect_retains;
    g_sim.fx_delta_ticks=6;g_sim.fx_light_ok=1;g_fx_ok=g_sim_ok=1;
    g_tex_kind=profile==COMBAT_PROFILE_DOS?3:profile>=COMBAT_PROFILE_3DFX?5:0;
    g_edition=profile==COMBAT_PROFILE_3DFX?PORTCFG_ED_3DFX:profile==COMBAT_PROFILE_TITANIUM?PORTCFG_ED_D3D:PORTCFG_ED_ENHANCED;
    g_snd_ok=1;g_snd_eye[0]=1000;g_snd_eye[1]=2000;g_snd_eye[2]=3000;g_snd_yaw=0;
    g_cockpit=1;g_camode=0;g_sfx=(sfx *)(void *)&sound_count;
    g_flash_t0=-1;g_amp_fade_end=-1;
    g_fxw.light_pos[0]=4444;g_fxw.light_pos[1]=5555;g_fxw.light_pos[2]=6666;
    clear_events();
}
static int weapon_for_visual(int visual)
{int w;for(w=0;w<128;w++)if(combat_weapon_visual(w)==visual)return w;assert(!"missing weapon visual");return -1;}
static void cleared(void)
{
#if MERCS_EFFECT_COORDINATES
    const float zero[3]={0,0,0};equal("scoped point invalid after return",g_sim.fx_light_point_valid,0,0);xyz("scoped point erased",g_sim.fx_light_point,zero,0);
#else
    unavailable+=4;
#endif
    equal("visual scope cleared",g_sim.fx_visual_valid,0,0);equal("attachment scope cleared",g_sim.fx_attached,0,0);
}

static void captured_consumers(void)
{
    int i;
    for(i=0;i<COUNT(original_effect_rows);i++){
        const original_effect_row *r=&original_effect_rows[i];fx_live *e;int before_rng;
        context=r->name;configure(r->profile);consumer_rows++;
        g_sim.fx_light_ok=r->allowed;if(r->refused)definitions(0);before_rng=(int)g_sim.rng;
        effect_fixture_impact(&g_sim,weapon_for_visual(r->visual),r->contact,r->backoff,r->flags);
        equal("one actual frontend callback",callback_count,1,0);equal("pool admission",live_count(),r->active,0);
        equal("refusal propagated",g_sim.fx_refused,r->refused,0);equal("shared light active",g_fxw.light_slot>0,r->light_active,0);
        xyz("shared light captured point",g_fxw.light_pos,r->light,.0003);
        equal("sound count",sound_count,r->sound_count,0);equal("frontend RNG consumed after admission",rng_calls,r->active,0);
        if(r->active){
            e=find_type(r->type);equal("captured remapped effect",e!=NULL,1,0);
            if(e){xyz("captured pool/presentation",e->pos,r->presentation,.0003);equal("captured initial life",e->remaining,r->remaining,0);equal("source visual retained",e->source_visual,r->visual,0);}
            xyz("callback presentation",callbacks[0].point,r->presentation,.0003);
            equal("area not yet admitted at frontend entry",callbacks[0].area_before,0,0);
            equal("area still not admitted at frontend return",callbacks[0].area_after,0,0);
        }
        if(r->sound_count&&sound_count){
            equal("captured sound id",sounds[0].id,r->sound_id,0);equal("captured half flag",sounds[0].half,r->sound_half,0);
            xyz("actual snd_at camera minus presentation",sounds[0].delta,r->sound_delta,.0003);
            equal("pool allocated before positional sound",sounds[0].pool,r->active,0);
        }
#if MERCS_EFFECT_COORDINATES
        equal("E valid during callback",callbacks[0].scoped_valid,1,0);xyz("separate E during callback",callbacks[0].scoped,r->backoff,0);
        if(sound_count)equal("E remains scoped through sound submission",sounds[0].scoped_valid,1,0);
#else
        unavailable+=4+(sound_count!=0);
#endif
        if(r->area_executed){
            equal("native area admitted",g_sim.area_count,1,0);
            if(g_sim.area_count){xyz("captured area center C",g_sim.area[0].pos,r->area_center,.0003);equal("area source owner",g_sim.area[0].owner,7,0);}
            /* Real native consumer discriminator: unit center equals C, with
             * 1cm radius. E is over100cm away, outside the40cm blast. */
            effect_fixture_unit(&g_sim);g_sim.splash=1;g_sim.radius[0]=1;
            g_sim.actors[0].mech.origin[0]=(int32_t)r->contact[0];g_sim.actors[0].mech.origin[2]=(int32_t)r->contact[2];
            g_sim.units[0].y=r->contact[1]-g_sim.centre_h[0];
            effect_fixture_area(&g_sim,1);
            equal("actual area damage around C",g_sim.units[0].armor[4],1000.0-(double)fx_damage_q16(12)/65536.0,.0001);
        }
        if(r->refused){equal("no refused chunks",g_sim.debris_count,0,0);equal("no refused area",g_sim.area_count,0,0);equal("refusal preserves sim RNG",g_sim.rng,before_rng,0);equal("no refused palette",g_fxw.palette_left,0,0);}
        cleared();
    }
}

static void captured_producers(void)
{
    int i;
    for(i=0;i<COUNT(original_backoff_rows);i++){
        const original_backoff_row *r=&original_backoff_rows[i];fx_live *e;
        context=r->name;configure(r->profile);producer_rows++;
        effect_fixture_segment(&g_sim,53,r->contact,r->from,r->to,FX_HIT_GROUND);
        equal("segment producer reaches frontend",callback_count,1,0);e=find_type(21);equal("PPC ground pool",e!=NULL,1,0);
        if(e)xyz("segment contact stays C",e->pos,r->contact,.0003);
        xyz("original produced E reaches shared light",g_fxw.light_pos,r->backoff,.0003);
        if(sound_count){float d[3]={1000-r->contact[0],2000-r->contact[1],3000-r->contact[2]};xyz("ordinary PPC sound stays C",sounds[0].delta,d,.0003);}
        cleared();
    }
}

static void captured_two_lights(void)
{
    int i;
    for(i=0;i<COUNT(original_light_rows);i++){
        const original_light_row *r=&original_light_rows[i];fx_live *e;unsigned first_serial;
        context=r->name;configure(r->profile);light_rows++;
        effect_fixture_impact(&g_sim,37,r->contact1,r->backoff1,FX_HIT_GROUND);
        equal("first light is missile",light_type(),12,0);xyz("first light stores E1",g_fxw.light_pos,r->backoff1,.0003);
        e=find_type(12);first_serial=e?e->serial:0;
        clear_events();effect_fixture_impact(&g_sim,53,r->contact2,r->backoff2,FX_HIT_GROUND);
        equal("both ordinary effects admitted",live_count(),2,0);equal("captured winning type",light_type(),r->final_type,0);
        xyz("captured final E",g_fxw.light_pos,r->final_light,.0003);
        e=find_type(12);if(e){xyz("first pool C1 unchanged",e->pos,r->contact1,.0003);equal("first instance retained",e->serial,first_serial,0);}
        e=find_type(21);if(e){xyz("second pool C2",e->pos,r->contact2,.0003);equal("second light admission",e->light_marked,r->replaced,0);}
        cleared();
    }
}

static void return_paths(void)
{
    const float c[3]={50,500,100},e[3]={41,593,81},near[3]={999,1999,2999},zero[3]={0,0,0};
    int profile;
    for(profile=0;profile<4;profile++){
        context="native stale E / ordinary producer";configure(profile);native_cases++;
        effect_fixture_impact(&g_sim,37,c,e,FX_HIT_GROUND);clear_events();
        effect_fixture_ordinary(&g_sim,21,near);equal("ordinary replaces nearer",light_type(),21,0);xyz("ordinary light uses own C",g_fxw.light_pos,near,0);
#if MERCS_EFFECT_COORDINATES
        equal("ordinary callback has no E",callbacks[0].scoped_valid,0,0);xyz("ordinary metadata zero",callbacks[0].scoped,zero,0);
#else
        unavailable+=4;
#endif
        cleared();
        context="native refusal then ordinary admission";configure(profile);native_cases++;definitions(0);
        effect_fixture_impact(&g_sim,37,c,e,FX_HIT_GROUND);cleared();equal("refusal no sound",sound_count,0,0);equal("refusal no random",rng_calls,0,0);
        definitions(4);clear_events();effect_fixture_ordinary(&g_sim,21,near);xyz("post-refusal ordinary light",g_fxw.light_pos,near,0);cleared();
        context="native no-display return";configure(profile);native_cases++;g_fx_ok=0;
        effect_fixture_impact(&g_sim,53,c,e,FX_HIT_GROUND);equal("no-display no pool",live_count(),0,0);equal("no-display no sound",sound_count,0,0);cleared();
        g_fx_ok=1;clear_events();effect_fixture_ordinary(&g_sim,21,near);xyz("post-no-display light",g_fxw.light_pos,near,0);cleared();
        context="native missing callback return";configure(profile);native_cases++;g_sim.on_effect=NULL;
        effect_fixture_impact(&g_sim,53,c,e,FX_HIT_GROUND);equal("missing callback no frontend",callback_count,0,0);cleared();
        context="native chunk-only return";configure(profile);native_cases++;
#if MERCS_EFFECT_COORDINATES
        g_sim.fx_light_point_valid=1;memcpy(g_sim.fx_light_point,e,sizeof e);
#endif
        effect_fixture_impact(&g_sim,weapon_for_visual(11),c,e,FX_HIT_STRUCT);
        equal("chunk-only no ordinary callback",callback_count,0,0);equal("chunk count",g_sim.debris_count,4,0);cleared();
        context="native C equals E unit-bullet control";configure(profile);native_cases++;
        effect_fixture_impact(&g_sim,43,c,c,FX_HIT_UNIT);if(find_type(8))xyz("equal points unchanged",find_type(8)->pos,c,0);cleared();
    }
    (void)zero;
}

static void helper_boundaries(void)
{
    int profile;
    for(profile=0;profile<4;profile++){
        context="native numeric profile predicate";configure(profile);native_cases++;
#if MERCS_EFFECT_COORDINATES
        equal("exact active table predicate",combat_is_titanium(),profile==COMBAT_PROFILE_TITANIUM,0);
#else
        unavailable++;
#endif
    }
#if MERCS_EFFECT_COORDINATES
    {
        float o[3];const float c[3]={50.25f,500.5f,100.75f};
        const float a[3]={.9f,0,0},b[3]={1.1f,0,0},expected[3]={-49.75f,500.5f,100.75f};
        const float huge[3]={2147483648.0f,0,0},bad[3]={NAN,0,0},z[3]={0,0,0};
        context="declared integer endpoint bridge";configure(COMBAT_PROFILE_DOS);native_cases++;
        equal("endpoint truncation admitted",combat_projectile_backoff(a,b,c,o),1,0);xyz("each endpoint then difference",o,expected,0);
        equal("unsupported integer endpoint refused",combat_projectile_backoff(z,huge,c,o),0,0);xyz("unsupported retains C",o,c,0);
        equal("nonfinite endpoint refused",combat_projectile_backoff(bad,b,c,o),0,0);xyz("nonfinite retains C",o,c,0);
        equal("zero finite segment admitted",combat_projectile_backoff(z,z,c,o),1,0);xyz("zero retains C",o,c,0);
    }
#else
    unavailable+=16;
#endif
}

static void actual_consumers(void)
{
    int profile;
    for(profile=0;profile<4;profile++){
        mech3d world={0};const float from[3]={0,1000,0},velocity[3]={100,-1000,200},c[3]={50,500,100};
        const float e_int[3]={41,593,81},e_ti[3]={40.24100112915039f,597.5900268554688f,80.48200225830078f};fx_live *e;
        context="full native terrain segment to frontend";configure(profile);native_cases++;
        effect_fixture_plane(&world,500);msim_set_world(&g_sim,&world);effect_fixture_shot(&g_sim,53,from,velocity);
        actual_step();
        equal("actual segment retires",g_sim.shot_count,0,0);equal("actual segment callback",callback_count,1,0);
        e=find_type(21);equal("actual segment pool",e!=NULL,1,0);if(e)xyz("actual terrain C",e->pos,c,.001);
        xyz("actual segment light E",g_fxw.light_pos,profile==3?e_ti:e_int,.001);cleared();
        effect_fixture_cleanup(&g_sim);mech3d_free(&world);

        context="full native GT direct damage at contact";configure(profile);native_cases++;
        effect_fixture_plane(&world,500);effect_fixture_gt(&g_sim,&world);effect_fixture_shot(&g_sim,43,from,velocity);
        actual_step();
        equal("GT direct HP",g_sim.bld[0].hp,10000-combat_weapon_damage(43),0);equal("GT shot retired",g_sim.shot_count,0,0);
        if(callback_count)equal("native GT damage follows effect callback",callbacks[0].gt_hp,10000,0);
        e=find_type(5);equal("GT bullet effect type",e!=NULL,1,0);if(e)xyz("GT ordinary contact C",e->pos,c,.001);cleared();
        effect_fixture_cleanup(&g_sim);mech3d_free(&world);

        context="full native fallback leaves C equals E";configure(profile);native_cases++;
        {
            const float f[3]={10,100,20},v[3]={100,-200,200},fc[3]={60,0,120};
            effect_fixture_shot(&g_sim,53,f,v);actual_step();
            equal("fallback retired",g_sim.shot_count,0,0);e=find_type(21);equal("fallback effect",e!=NULL,1,0);
            if(e)xyz("fallback projected C",e->pos,fc,.001);
            xyz("fallback light equals C",g_fxw.light_pos,fc,.001);cleared();
        }
        context="full native visual5 MG unit hit damage C / presentation E.x,C.y,E.z";configure(profile);native_cases++;
        {
            const float f[3]={-200,500,0},v[3]={400,0,0},presentation[3]={-200,500,0};
            effect_fixture_unit(&g_sim);effect_fixture_shot(&g_sim,43,f,v);actual_step();
            equal("unit hit count",g_sim.hits,1,0);equal("unit direct arm damage",g_sim.units[0].armor[4],1000-combat_weapon_damage(43),.001);
            if(callback_count)equal("unit damage precedes effect callback",callbacks[0].arm_hp,1000-combat_weapon_damage(43),.001);
            equal("unit shot retired",g_sim.shot_count,0,0);e=find_type(8);equal("unit bullet pool",e!=NULL,1,0);
            if(e)xyz("unit bullet backed presentation",e->pos,presentation,.001);
            cleared();
        }
        context="full native forced-unit hit keeps current position";configure(profile);native_cases++;
        {
            const float f[3]={77,550,88},v[3]={400,0,0};effect_fixture_unit(&g_sim);effect_fixture_shot(&g_sim,43,f,v);
            g_sim.shots[0].prox=1;g_sim.shots[0].homing=0;actual_step();
            equal("forced unit direct hit",g_sim.hits,1,0);equal("forced arm damage",g_sim.units[0].armor[4],1000-combat_weapon_damage(43),.001);
            e=find_type(8);equal("forced unit effect",e!=NULL,1,0);if(e)xyz("forced C=E",e->pos,f,0);cleared();
        }
        context="full native visual5 direct damage despite visual refusal";configure(profile);native_cases++;
        {
            const float f[3]={-200,500,0},v[3]={400,0,0};effect_fixture_unit(&g_sim);definitions(0);
            effect_fixture_shot(&g_sim,43,f,v);actual_step();
            equal("refused visual still direct hit",g_sim.hits,1,0);equal("refused visual direct arm damage",g_sim.units[0].armor[4],1000-combat_weapon_damage(43),.001);
            equal("full step refuses visual",g_sim.fx_refused,1,0);equal("full step refused pool",live_count(),0,0);
            equal("full step refused sound",sound_count,0,0);equal("full step refused area",g_sim.area_count,0,0);cleared();
        }
        context="full native AMS/proximity no-target keeps current point";configure(profile);native_cases++;
        {
            const float f[3]={77,550,88},v[3]={400,0,0};effect_fixture_shot(&g_sim,53,f,v);g_sim.shots[0].prox=1;
            actual_step();e=find_type(6);equal("forced untargeted effect",e!=NULL,1,0);
            if(e)xyz("untargeted C=E",e->pos,f,0);
            xyz("untargeted light C=E",g_fxw.light_pos,f,0);cleared();
        }
    }
}

int main(void)
{
    unsetenv("MERCS_FX_TRACE");unsetenv("MERCS_MISSILE_TRACE");unsetenv("MERCS_AIM_TRACE");unsetenv("MERCS_AI_AIM_TRACE");
    unsetenv("MERCS_TEST_PLHIT_GROUP");unsetenv("MERCS_PROVOKE");
    captured_consumers();captured_producers();captured_two_lights();return_paths();helper_boundaries();actual_consumers();
    effect_fixture_cleanup(&g_sim);g_sfx=NULL;
    printf("RESULT {\"checks\":%d,\"failures\":%d,\"unavailable_internal_checks\":%d,\"captured_producer_rows\":%d,\"captured_consumer_rows\":%d,\"captured_two_light_sequences\":%d,\"native_control_cases\":%d,\"actual_combat_steps\":%d,\"paired_physics_fnv\":\"%08x\",\"candidate_api\":%d}\n",
           checks,failures,unavailable,producer_rows,consumer_rows,light_rows,native_cases,full_steps,physics_digest,MERCS_EFFECT_COORDINATES);
    return failures?1:0;
}
