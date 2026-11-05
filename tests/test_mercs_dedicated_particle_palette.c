/* Actual archive loader -> native particle frontend -> copied renderer state
 * and GPU readback. Original goldens are graphics arguments and RGB tables,
 * not original hardware/framebuffer pixels. No career or mission step runs. */
#define main unused_native_frontend_main
#include "tools/glview.c"
#undef main
#include "gpu_test.h"
#include <SDL3/SDL_main.h>
#include "dedicated-particle-fixtures.h"

extern int dedicated_particle_renderer_state(const glr *,unsigned char[256][3],int *,int *);
extern int dedicated_particle_renderer_primitives(const glr *,glr_palette_particle *,int);
extern void dedicated_particle_fixture_launch(msim *,int,const float[3],const float[3]);

static int checks,failures,loads,readbacks,actual_launches,declared_emissions;
static const char *label;
static void check(int ok,const char *what)
{
    checks++;
    if(!ok){if(failures<40)fprintf(stderr,"FAIL %s: %s\n",label,what);failures++;}
}
static unsigned char expand6(unsigned v){return (unsigned char)((v<<2)|(v>>4));}
static void setup_view(glr *r,glr_view *v)
{
    mech3d empty={0};texture *slots[GLR_SLOTS]={0};
    glr_default_view(v);v->free_cam=1;v->vfov=73.74f;
    v->eye[0]=1000;v->eye[1]=2000;v->eye[2]=1000;
    v->look_yaw=v->look_pitch=0;v->look_rot_ok=0;v->far_cull=1000000;
    /* The actual scene supplies palette/profile bindings. Empty renderer
     * geometry is a declared aperture for the point/line rasterizer only. */
    glr_set_mech(r,&empty,(const uint8_t (*)[3])g_scene.palette,slots,0,0);
    glr_set_actors(r,&empty,(const uint8_t (*)[3])g_scene.palette,slots,0,0);
    glr_set_sprites(r,NULL,0);glr_set_particles(r,NULL,0,0);
}
static void emit_declared(int kind)
{
    g_palette_particle_mode=-2;g_sim.now=550; /* floor(550*182/1000)=100 */
    on_particle(NULL,kind,original_particle_origin,original_particle_delta);
    g_sim.now=584;on_effect_tick(NULL,6); /* floor(...)=106 */
    declared_emissions++;
}
static int submit(glr *r,glr_view *v,unsigned char out[256][3])
{
    unsigned char scene[sizeof g_scene.palette],slots[sizeof g_scene.slotpal];
    float hp[256][4];msim *sim=malloc(sizeof *sim);int dos,pid,n;
    check(sim!=NULL,"simulator snapshot allocated");if(!sim)return 0;
    memcpy(scene,g_scene.palette,sizeof scene);memcpy(slots,g_scene.slotpal,sizeof slots);
    memcpy(hp,g_hp,sizeof hp);memcpy(sim,&g_sim,sizeof *sim);
    palette_particles(r,v);
    n=dedicated_particle_renderer_state(r,out,&dos,&pid);
    check(!memcmp(scene,g_scene.palette,sizeof scene),"particle submission preserves scene palette");
    check(!memcmp(slots,g_scene.slotpal,sizeof slots),"particle submission preserves every slot palette");
    check(!memcmp(hp,g_hp,sizeof hp),"particle submission preserves HUD HP colors");
    check(!memcmp(sim,&g_sim,sizeof *sim),"particle submission preserves complete simulator state");
    check(dos==(g_edition==PORTCFG_ED_DOS),"actual loaded renderer DOS mode matches profile");
    check(pid==0,"declared empty PID aperture remains separate");free(sim);return n;
}
static void gpu(glr *r,glr_view *v,const unsigned char expected[3],const char *stage)
{
    unsigned char before[128*128*4],after[128*128*4],pal[256][3];
    glr_palette_particle saved[64];int n,dos,pid,i,changed=0,bad=0;
    n=dedicated_particle_renderer_primitives(r,saved,64);
    (void)dedicated_particle_renderer_state(r,pal,&dos,&pid);
    check(n>0&&n<=64,"real frontend provided bounded visible primitive set");if(n<=0)return;
    glr_set_palette_particles(r,NULL,0,NULL);gpu_test_draw(r,v,128,128);
    gpu_test_read_rgba(0, 0, 128, 128, before);
    glr_set_palette_particles(r,saved,n,(const uint8_t (*)[3])pal);gpu_test_draw(r,v,128,128);
    gpu_test_read_rgba(0, 0, 128, 128, after);readbacks++;
    for(i=0;i<128*128;i++)if(memcmp(before+i*4,after+i*4,4)){
        changed++;if(memcmp(after+i*4,expected,3)||after[i*4+3]!=255)bad++;
    }
    check(changed>0,"native point/line changes at least one framebuffer pixel");
    check(!bad,"changed native framebuffer pixels match required opaque RGB8");
    check(gpu_test_ok(),"native primitive render has successful GPU readback");
    printf("DRAW {\"profile\":%d,\"stage\":\"%s\",\"primitives\":%d,\"changed_pixels\":%d,\"bad_pixels\":%d,\"expected_rgb\":[%u,%u,%u]}\n",
           g_edition,stage,n,changed,bad,expected[0],expected[1],expected[2]);
}
static prj_archive *actual_load(glr *r,const char *path,int edition,glr_view *v)
{
    char error[256];prj_archive *a=prj_open(path,error,sizeof error);
    check(a!=NULL,"actual archive opens");if(!a){fprintf(stderr,"%s\n",error);exit(2);}
    g_edition=edition;g_tex_kind=edition==PORTCFG_ED_DOS?3:5;g_texset=PORTCFG_TX_ORIGINAL;
    g_world=1;g_pilot=0;g_cockpit=1;g_win_cols=640;g_win_lines=480;g_render_w=g_render_h=0;
    g_sfx=NULL;g_ma=a;g_fx_ok=0;glr_default_view(v);g_view=v;
    if(g_tex_kind==5)check(accelerated_archive_open(path)==0,"actual accelerated archive selected");
    check(load(r,a,a,g_tex_kind,"","WOL1",0,0)==0,"actual native WOL1 frontend load succeeds");loads++;
    check(g_hud&&g_sim_ok&&g_scene.slot_ok[0],"actual load provides HUD/simulator/primary palette");
    check(g_sim.on_particle==on_particle,"actual load binds native particle observer");
    setup_view(r,v);return a;
}
static void dedicated(glr *r,const char *path,glr_view *v)
{
    unsigned char pal[256][3],scene[sizeof g_scene.palette],slots[sizeof g_scene.slotpal];
    glr_palette_particle primitive[64];prj_record asset;prj_archive *a;int i,k,stage;
    label="dedicated actual palette-particle route";a=actual_load(r,path,PORTCFG_ED_3DFX,v);
    check(prj_read_named(a,"PAL","WOL10FDA",&asset)==PRJ_OK,"same original palette asset resolves");
    check(asset.size==768&&!memcmp(asset.data,original_particle_rgb6[0],768),"complete actual palette equals captured original input");
    prj_record_free(&asset);
    for(i=0;i<256;i++)for(k=0;k<3;k++)check(g_scene.palette[i][k]==expand6(original_particle_rgb6[0][i*3+k]),"actual native loader retains established scene RGB8 expansion");
    memcpy(scene,g_scene.palette,sizeof scene);memcpy(slots,g_scene.slotpal,sizeof slots);
    emit_declared(MERCS_PARTICLE_BALLISTIC);
    check(g_palette_particles.count==1,"declared original event creates one actual native emitter");
    if(g_palette_particles.count){const mercs_particle_emitter *e=&g_palette_particles.emitter[g_palette_particles.active[0]];
        for(k=0;k<3;k++)check(e->pos[k]==original_particle_after[k],"native event/update position matches captured original setup");
        check(e->color==3,"native ballistic updater selects captured index3");}
    for(stage=0;stage<3;stage++){
        const unsigned char *rgb=original_particle_rgb6[stage];
        check(g_fxw.palette.submit!=NULL,"actual loader installed shared palette submission callback");
        if(g_fxw.palette.submit)g_fxw.palette.submit(g_fxw.palette.user,rgb);
        for(k=0;k<3;k++)check((int)lrintf(g_hp[3][k]*255)==expand6(rgb[9+k]),"real HUD callback changes HP for captured initial/white/restored input");
        check(!memcmp(scene,g_scene.palette,sizeof scene)&&!memcmp(slots,g_scene.slotpal,sizeof slots),"HUD palette callback preserves scene/slot palette ownership");
        check(submit(r,v,pal)==64,"real dedicated frontend submits64 ballistic points");
        check(!memcmp(pal,original_particle_table_rgb8,sizeof pal),"complete native particle palette matches captured original float table");
        for(k=0;k<3;k++)check(pal[3][k]==original_particle_argb[stage][k+1],"native index3 submission equals original packed draw argument channels");
        check(dedicated_particle_renderer_primitives(r,primitive,64)==64&&primitive[0].color==3&&!primitive[0].line,"real collected primitive identity retained");
        gpu(r,v,original_particle_table_rgb8[3],stage==0?"captured-initial":stage==1?"captured-allwht":"captured-restored");
    }
    /* Real class5 native shot consumer, independently of the declared emitter
     * vector replay above. No shot movement, mission logic or career return. */
    {
        const float from[3]={1000,2000,3000},to[3]={1000,2000,20000};int previous=g_sim.shot_count;
        g_palette_particle_mode=-2;g_sim.now=550;dedicated_particle_fixture_launch(&g_sim,46,from,to);actual_launches++;
        check(g_sim.shot_count==previous+1,"actual AC5 class5 launch creates a native shot");
        check(g_palette_particles.count==1&&g_palette_particle_emitted[3]==1,"actual shot branch reaches exactly one ballistic observer");
        g_sim.now=584;on_effect_tick(NULL,6);check(submit(r,v,pal)==64,"actual class5 launch reaches64-point renderer submission");
        check(!memcmp(pal[3],original_particle_table_rgb8[3],3),"actual launch color equals captured original table");
        gpu(r,v,original_particle_table_rgb8[3],"actual-native-class5-launch");
    }
    /* One missile line uses the same statically verified float table. This
     * extends native adapter coverage, not original dynamic color coverage. */
    emit_declared(MERCS_PARTICLE_MISSILE);check(submit(r,v,pal)==64,"native missile emits64 near lines");
    check(dedicated_particle_renderer_primitives(r,primitive,64)==64&&primitive[0].color==21&&primitive[0].line,"native near missile index21 line identity");
    check(!memcmp(pal[21],original_particle_table_rgb8[21],3),"same captured float table supplies missile index21");
    gpu(r,v,original_particle_table_rgb8[21],"native-missile-line-control");
    emit_declared(MERCS_PARTICLE_BALLISTIC);
    for(i=0;i<2;i++){
        g_tex_kind=i;check(submit(r,v,pal)==64,"declared legacy raw path retains particle count");
        check(!memcmp(pal,scene,sizeof pal),"legacy texture kinds0/1 retain previous expanded particle palette");
        gpu(r,v,scene+9,i?"legacy-kind1-control":"legacy-kind0-control");
    }
    g_tex_kind=5;g_scene.slot_ok[0]=0;check(submit(r,v,pal)==64,"absent-primary-PALG control retains particle count");
    check(!memcmp(pal,scene,sizeof pal),"absent-primary-PALG control retains previous palette");
    gpu(r,v,scene+9,"absent-primary-PALG-control");g_scene.slot_ok[0]=1;
    check(!memcmp(scene,g_scene.palette,sizeof scene)&&!memcmp(slots,g_scene.slotpal,sizeof slots),"all dedicated controls leave shared scene palette banks unchanged");
}
static void dos_control(glr *r,const char *path,glr_view *v)
{
    unsigned char pal[256][3],rgb6[768],expected[3];int k;
    label="DOS unchanged draw-time palette";(void)actual_load(r,path,PORTCFG_ED_DOS,v);
    emit_declared(MERCS_PARTICLE_BALLISTIC);check(submit(r,v,pal)==10,"actual DOS profile retains10 ballistic points");
    memcpy(expected,g_scene.palette[3],3);gpu(r,v,expected,"dos-initial");
    memcpy(rgb6,g_pal_slot[0],768);rgb6[9]=13;rgb6[10]=26;rgb6[11]=37;
    dos_apply_palette(r,rgb6); /* Real existing DOS callback, AFTER list upload. */
    for(k=0;k<3;k++)expected[k]=expand6(rgb6[9+k]);
    gpu(r,v,expected,"dos-draw-time-palette-update");
    check(submit(r,v,pal)==10,"DOS resubmission retains count after palette update");
    check(!memcmp(pal,g_scene.palette,sizeof pal),"DOS frontend submission does not receive dedicated conversion");
}
static void titanium_control(glr *r,const char *path,glr_view *v)
{
    unsigned char pal[256][3],scene[sizeof g_scene.palette];int dos,pid;
    label="Titanium separate PID route";(void)actual_load(r,path,PORTCFG_ED_D3D,v);memcpy(scene,g_scene.palette,sizeof scene);
    emit_declared(MERCS_PARTICLE_BALLISTIC);check(submit(r,v,pal)==0,"actual Titanium profile emits no palette ballistic primitives");
    check(g_palette_particle_mode==-1&&!g_palette_particles.enabled,"Titanium palette kernel remains disabled");
    emit_declared(MERCS_PARTICLE_MISSILE);check(submit(r,v,pal)==0,"Titanium missile observer remains outside DOS/dedicated palette kernel");
    check(dedicated_particle_renderer_state(r,pal,&dos,&pid)==0&&!dos,"no stale dedicated primitive list survives Titanium load");
    check(!memcmp(scene,g_scene.palette,sizeof scene),"Titanium scene palette remains unchanged");
}
int main(int argc,char **argv)
{
    SDL_Window *window;int context;glr *r;glr_view view;
    if(argc!=4){fprintf(stderr,"usage: %s DOS-MW2.PRJ dedicated-models.prj Titanium-MW2.PRJ\n",argv[0]);return 2;}
    SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO))return 2;
    window=SDL_CreateWindow("Private dedicated particle palette",640,480,SDL_WINDOW_HIDDEN);
    context = gpu_test_start();if(!context){fprintf(stderr,"%s\n",SDL_GetError());return 2;}
    r=glr_create();if(!r)return 2;dp_clear_roots();dp_set_write_root(NULL);
    dedicated(r,argv[2],&view);dos_control(r,argv[1],&view);titanium_control(r,argv[3],&view);
    printf("RESULT {\"checks\":%d,\"failures\":%d,\"actual_scene_loads\":%d,\"framebuffer_readback_cases\":%d,\"actual_native_class5_launches\":%d,\"declared_native_emissions\":%d,\"original_executions_added\":0,\"scope\":\"actual loaders/frontend/GPU with captured original RGB/table arguments; no original hardware/framebuffer or whole mission equivalence\"}\n",
        checks,failures,loads,readbacks,actual_launches,declared_emissions);
    return failures?1:0;
}
