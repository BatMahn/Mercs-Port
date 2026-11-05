/* Actual frontend callbacks, original-contract producer and complete native
 * step, plus real GPU palette upload/readback. The actor/table inputs are the
 * existing declared dispatch fixture; no original scene pixels/career run. */
#define main unused_native_frontend_main
#include "tools/glview.c"
#undef main
#include <SDL3/SDL_main.h>
#include "original-palette-fixtures.h"

extern msim *palette_fixture_create(void);
extern void palette_fixture_clear(msim *);
extern void palette_fixture_producer(msim *,const float[3]);

static int pchecks,pfails,palette_requests,effect_requests,presentation_calls,end_tick_calls,uploads,phase_watch;
static int expecting_busy,expecting_refused;
static glr *test_renderer;
static const char *test_case;
static void pcheck(int good,const char *what)
{
    pchecks++;
    if(!good){if(pfails<35)fprintf(stderr,"%s: %s\n",test_case,what);pfails++;}
}
static void observed_upload(void *user,const uint8_t *rgb6)
{
    uploads++;
    if(phase_watch)pcheck(g_sim.logic.current[5]==1,"RGB submits before the instant node's MTBL completion");
    dos_palette_submit(user,rgb6);
}
static void observed_palette(void *user,int slot,int duration,int mode,int ticks)
{
    palette_requests++;
    pcheck(slot==18 && mode==2 && duration==3640 && ticks==6,"real nuclear callback arguments");
    pcheck(g_sim.nuclear_wave.radius_cap_cm==10000 && g_sim.nuclear_wave.remaining_ticks==0,"palette request after cap but before wave rearm");
    pcheck(effect_requests==0,"palette request precedes visual allocation");
    on_palette(user,slot,duration,mode,ticks);
    pcheck(g_fxw.palette_left==(expecting_busy?30:1),"shared busy state accepts or refuses mode2");
}
static void observed_effect(void *user,int type,const float point[3])
{
    if(type==22) {
        effect_requests++;
        pcheck(palette_requests==1,"producer delivered palette before type22");
        pcheck(g_sim.nuclear_wave.remaining_ticks==3640,"wave armed independently of visual allocation");
    }
    on_effect(user,type,point);
    if(type==22)pcheck(g_sim.fx_refused==expecting_refused,"actual visual pool admission/refusal");
}
static void observed_presentation(void *user,int ticks)
{
    presentation_calls++;
    pcheck(effect_requests==1,"presentation follows the AI producer");
    if(presentation_calls==1)pcheck(g_sim.logic.current[5]==1,"presentation precedes MTBL pass");
    on_presentation(user,ticks);
}
static void observed_end_tick(void *user,int ticks)
{
    int before=g_fxw.palette_left;end_tick_calls++;
    pcheck(presentation_calls==end_tick_calls,"one presentation phase per complete native step");
    on_effect_tick(user,ticks);
    pcheck(g_fxw.palette_left==before,"end-of-step FX consumer cannot double-step bound palette");
}
static void bind_actual_bank(void)
{
    size_t i;
    memset(g_dos_slot_ok,0,sizeof g_dos_slot_ok);
    for(i=0;i<ORIGINAL_PALETTE_RESOURCES_COUNT;i++) {
        const original_palette_resource *r=&original_palette_resources[i];
        memcpy(g_dos_slotpal[r->slot],r->rgb6,768);g_dos_slot_ok[r->slot]=1;
    }
    fx_palette_bind(&g_fxw,g_dos_slotpal,g_dos_slot_ok,0,observed_upload,test_renderer);
    /* The production loader allocates the DOS table before binding/submitting.
     * A newly created bare test renderer has no table storage yet. */
    glr_set_dos(test_renderer,g_scene.palette,NULL);
    dos_apply_palette(test_renderer,g_dos_slotpal[0]);
}
static void draw_patch(glr_view *view,int white)
{
    uint8_t got[4];int before=g_fxw.palette_left;
    rb_frame(128,128);rb_target(0);glr_draw(test_renderer,view,128,128);
    pcheck(rb_read_rgba(48,64,1,1,got),"indexed patch RGBA readback succeeds");
    pcheck(!memcmp(got,g_scene.palette[239],3),"actual submitted frontend palette reaches indexed GPU card");
    if(white)pcheck(got[0]==255 && got[1]==255 && got[2]==255,"second original mode2 phase shows ALLWHT entry239");
    pcheck(rb_read_rgba(80,64,1,1,got),"entry240 RGBA readback succeeds");
    pcheck(!memcmp(got,g_scene.palette[240],3),"actual entry240 remains separate from white region");
    pcheck(g_fxw.palette_left==before,"drawing never advances palette time");
}
static void production_route(void)
{
    int busy,refuse,i,j;uint32_t patches[2]={0xffefefefu,0xfff0f0f0u};texture tex={0};glr_sprite sprite={0};glr_view view;
    tex.w=2;tex.h=1;tex.rgba=patches;sprite.tex=&tex;sprite.pos[2]=256;sprite.half_w=sprite.half_h=128;sprite.indexed=1;
    glr_default_view(&view);view.free_cam=1;view.vfov=90;view.fog_density=0;view.ambient=1;view.bilinear=0;
    for(busy=0;busy<2;busy++)for(refuse=0;refuse<2;refuse++) {
        msim *allocated=palette_fixture_create();g_sim=*allocated;free(allocated);
        g_actors=g_sim.actors;g_actor_count=g_sim.actor_count;g_sim_ok=1;g_world=1;g_pilot=g_pl_ok=0;
        g_tex_kind=g_fx_ati=3;g_edition=PORTCFG_ED_DOS;g_sfx=NULL;g_hud=NULL;g_fx_ok=1;
        g_dos_day=0;g_dos_amp=g_vision=0;g_dos_evals=0;g_dos_group=-1;g_dos_next_eval=0;
        memset(&g_fxw,0,sizeof g_fxw);memset(&g_fxd,0,sizeof g_fxd);
        for(i=0;i<FX_TYPES;i++)for(j=0;j<FX_CARD_SLOTS;j++)g_fxd.control[i][j]=g_fxd.render[i][j]=-1;
        g_fxd.slots[22]=refuse?0:1;
        palette_requests=effect_requests=presentation_calls=end_tick_calls=uploads=0;
        expecting_busy=busy;expecting_refused=refuse;
        test_case=busy?(refuse?"busy fade / refused visual":"busy fade / accepted visual"):(refuse?"idle fade / refused visual":"idle fade / accepted visual");
        bind_actual_bank();glr_set_dos(test_renderer,g_scene.palette,NULL);glr_set_sprites(test_renderer,&sprite,1);
        if(busy)pcheck(fx_palette_request(&g_fxw,17,364,1,6),"declared damage fade starts through shared service");
        g_sim.on_palette=observed_palette;g_sim.on_effect=observed_effect;g_sim.on_presentation=observed_presentation;
        g_sim.on_effect_tick=observed_end_tick;g_sim.on_effect_retains=on_effect_retains;
        fx_set_timing(1);combat_set_weapon_timing(1);phase_watch=1;
        msim_step(&g_sim,33);phase_watch=0;
        pcheck(palette_requests==1 && effect_requests==1 && presentation_calls==1 && uploads==1,"one producer, one palette submission");
        pcheck(g_sim.logic.current[5]==3,"following MTBL pass completes the instant order");
        pcheck(g_sim.area_count==(refuse?0:1),"ordinary type22 area still follows visual admission");
        pcheck(g_sim.nuclear_wave.remaining_ticks==3640,"singleton remains armed on allocation refusal");
        if(busy)pcheck(g_fxw.palette.want==17 && g_fxw.palette_left==29,"busy damage fade is not bypassed by nuclear request");
        else pcheck(g_fxw.palette.current==18 && g_fxw.palette_left==101,"original one-step outbound and frame-count return");
        draw_patch(&view,0);
        msim_step(&g_sim,33);
        pcheck(palette_requests==1 && effect_requests==1 && presentation_calls==2 && uploads==2,"second native frame does not repeat the instant producer");
        draw_patch(&view,!busy);
        if(!busy) {
            int oldleft=g_fxw.palette_left;red_flash(364,g_sim.now);
            pcheck(g_fxw.palette_left==oldleft && g_fxw.palette.want==0,"actual damage caller refuses during nuclear return");
            g_vision=1;on_presentation(NULL,6);
            pcheck(g_fxw.palette.want==12 && g_fxw.palette.back_slot==-1 && g_fxw.palette_back==0,"vision replaces live current RGB with no return phase");
            pcheck(g_fxw.palette.base==12 && g_fxw.palette.queued==12 && g_fxw.palette_left==29,"vision shares base queue and frame counters");
            draw_patch(&view,0);
        }
        palette_fixture_clear(&g_sim);g_actors=NULL;g_actor_count=0;
    }
}
static void effect_and_environment(void)
{
    const float pos[3]={10,0,10},camera[3]={0,0,0};int i,j;
    test_case="ordinary effect and base producers share palette";
    memset(&g_fxw,0,sizeof g_fxw);memset(&g_fxd,0,sizeof g_fxd);
    for(i=0;i<FX_TYPES;i++)for(j=0;j<FX_CARD_SLOTS;j++)g_fxd.control[i][j]=g_fxd.render[i][j]=-1;
    g_fxd.slots[3]=1;bind_actual_bank();
    /* Slot1 isn't in phase-one resource captures. This declared resource
     * alias tests relative dispatch only; its bytes are not a new golden. */
    memcpy(g_dos_slotpal[1],g_dos_slotpal[17],768);g_dos_slot_ok[1]=1;
    i=fx_spawn(&g_fxw,&g_fxd,3,pos,0,6);
    pcheck(i>=0 && fx_light_offer(&g_fxw,i,camera,1,1,6),"actual ordinary light producer accepts owner");
    pcheck(g_fxw.palette.want==1 && g_fxw.palette_back>0,"ordinary effect requests relative base palette through canonical service");
    pcheck(fx_retains(&g_fxw,i,g_fxw.e[i].serial),"existing light retention observes the shared counters");
    g_dos_day=86400;g_dos_t0=12*3600;g_dos_group=-1;g_dos_evals=0;g_dos_next_eval=0;g_dos_amp=g_vision=0;g_sim.fx_delta_ticks=6;
    fx_palette_step(&g_fxw,6);dos_time_of_day(0);
    pcheck(g_fxw.palette.want==0 && g_fxw.palette.back_slot==-1 && g_fxw.palette_back==0,"time of day replaces current effect colors");
    pcheck(g_fxw.palette_left==60 && g_fxw.palette.base==0 && g_fxw.palette.queued==0,"time-of-day request duration and shared queue");
    g_dos_day=0;
}
#include "palette_cache_fixture.h"
int main(int argc,char **argv)
{
    char err[256]={0};
    SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO))return 2;
    if(rb_init(NULL,RB_GPU_AUTO,1,err,sizeof err)!=0){fprintf(stderr,"%s\n",err);return 2;}
    rb_frame(128,128);rb_target(0);
    test_renderer=glr_create();if(!test_renderer)return 2;
    if(argc!=2){fprintf(stderr,"usage: %s retail-MW2.PRJ\n",argv[0]);return 2;}
    production_route();effect_and_environment();actual_effect_cache(argv[1]);
    glr_destroy(test_renderer);rb_quit();SDL_Quit();
    printf("{\"checks\":%d,\"failures\":%d,\"native_full_step_sequences\":4,\"native_full_steps\":8,"
           "\"scope\":\"actual callbacks and GPU upload; declared native actors/resources; no original pixels or career run\"}\n",pchecks,pfails);
    return pfails?1:0;
}
