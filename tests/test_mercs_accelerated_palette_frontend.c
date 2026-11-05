/* Private actual-load negative control and captured original glyph stages.
 * The native RGB8 display uses the existing bit-replicating six-bit upload;
 * original565/555 packed words are distinct exact comparison domains. */
#define main unused_native_frontend_main
#include "tools/glview.c"
#undef main
#include <SDL3/SDL_main.h>
#include "accelerated-palette-fixtures.h"

extern void palette_fixture_producer(msim *,const float[3]);
extern msim *palette_fixture_create(void);
extern void palette_fixture_clear(msim *);
extern size_t accelerated_hud_atlas_snapshot(hud *,unsigned char **);
static int check_count,fail_count,loads,draws,original_operations,original_submissions,full_steps,full_sequences;
static const char *label;
static void check(int ok,const char *what)
{
    check_count++;
    if(!ok){if(fail_count<45)fprintf(stderr,"FAIL %s: %s\n",label,what);fail_count++;}
}
static unsigned pack565(const unsigned char rgb[3])
{return ((unsigned)(rgb[0]>>1)<<11)|((unsigned)rgb[1]<<5)|(unsigned)(rgb[2]>>1);}
static unsigned pack555(const unsigned char rgb[3])
{return ((unsigned)(rgb[0]>>1)<<10)|((unsigned)(rgb[1]>>1)<<5)|(unsigned)(rgb[2]>>1);}
static int expand6(int v){return (v<<2)|(v>>4);}
static void draw_font(prj_archive *a,const unsigned char rgb6[3],const unsigned char *atlas,size_t atlas_bytes)
{
    prj_record font; unsigned char pixels[32*32*4],*current=NULL;size_t n;unsigned off;int x,y,w,h;
    unsigned char bg[3]={29,47,83};
    check(prj_read_named(a,"FONT","BASE6X76",&font)==PRJ_OK,"actual FONT record readable");
    off=(unsigned)font.data[16+'A'*4]|(unsigned)font.data[17+'A'*4]<<8|(unsigned)font.data[18+'A'*4]<<16;
    w=font.data[off]|font.data[off+1]<<8;h=font.data[8]|font.data[9]<<8;
    check(w>0&&w<32&&h>0&&h<32&&font.data[12]==255,"actual glyph dimensions and transparent255");
    {const float clear[4]={bg[0]/255.0f,bg[1]/255.0f,bg[2]/255.0f,1};
     rb_frame(640,480);rb_target(0);rb_viewport(0,0,640,480);rb_scissor(0,0,0,0,0);rb_depth_write(1);rb_clear(clear,1,1);}
    hud_begin(g_hud,640,480);hud_text(g_hud,32,32,"A",HP(14));hud_end(g_hud);
    check(rb_read_rgba(32,480-32-h,w,h,pixels),"actual HUD RGBA readback succeeds");draws++;
    for(y=0;y<h;y++)for(x=0;x<w;x++){
        int k;unsigned char v=font.data[off+4+(size_t)y*(size_t)w+(size_t)x];
        const unsigned char *p=&pixels[((h-1-y)*w+x)*4];
        check(v==14||v==255,"declared actual glyph has only ink14/transparent255");
        check(p[3]==255,"opaque glyph or unchanged background retains opaque framebuffer alpha");
        for(k=0;k<3;k++)check(p[k]==(v==255?bg[k]:expand6(rgb6[k])),v==255?"transparent glyph texel preserves sentinel background":"actual glyph ink follows current captured RGB6/nativeRGB8");
    }
    n=accelerated_hud_atlas_snapshot(g_hud,&current);
    check(n==atlas_bytes&&n>0&&!memcmp(current,atlas,n),"both font atlas RGBA submissions remain byte-identical");
    free(current);prj_record_free(&font);
}
static void stage(prj_archive *a,int white,const unsigned char *atlas,size_t n)
{
    unsigned char got6[3],expected[3]={0,49,0};int k;
    if(white)expected[0]=expected[1]=expected[2]=63;
    for(k=0;k<3;k++){
        int got=(int)lrintf(g_hp[14][k]*255);
        check(got==expand6(expected[k]),"HP14 receives captured RGB6 using existing native expansion");got6[k]=(unsigned char)(got>>2);
    }
    check(pack565(got6)==(white?0xffffu:0x0620u),"captured index14 RGB565 word domain");
    if(g_edition==PORTCFG_ED_3DFX)
        check(pack555(got6)==(white?0x7fffu:0x0300u),"captured index14 RGB555 word domain (dedicated declared format)");
    draw_font(a,expected,atlas,n);
}

/* These expectations come only from the frozen original report. The same
 * canonical core is exercised through actual frontend binding/request and
 * submit callbacks, with separately declared original operation boundaries. */
static void (*actual_submit)(void *,const uint8_t *);
static void *actual_submit_user;
static const ap_operation *expected_operation;
static int observed_submissions,unexpected_submissions;
static void compare_state(const ap_state *s)
{
    const fx_palette *p=&g_fxw.palette;int k,ok=1;
    check(p->current==s->current,"captured current slot");
    check(p->dirty==s->dirty,"captured dirty flag");
    check(p->want==s->want,"captured target slot");
    check(p->back_slot==s->back_slot,"captured return slot");
    check(g_fxw.palette_back==s->back,"captured saved frame count");
    check(g_fxw.palette_left==s->left,"captured canonical frames left");
    check(p->base==s->base&&p->queued==s->queued,"captured base and queued slots");
    check(p->frames==s->frames&&p->step==s->step,"captured interpolation counters");
    check(p->buffer_present==s->buffer_present,"captured buffer lifetime");
    if(s->rgb>=0){
        check(!memcmp(p->rgb,ap_blobs[s->rgb],768),"captured complete RGB6 buffer");
        check(!memcmp(p->delta,ap_blobs[s->delta],768),"captured signed-byte deltas");
        for(k=0;k<768;k++){
            const uint8_t *b=ap_blobs[s->remainder];int v=b[k*2]|b[k*2+1]<<8;
            if(v>=32768)v-=65536;
            if(p->remainder[k]!=v)ok=0;
        }
        check(ok,"captured complete signed16 remainder buffer");
    }
}
static void captured_submit(void *user,const uint8_t *rgb)
{
    int k,c,colors=1,words=1;const ap_submission *s=NULL;(void)user;
    if(expected_operation && observed_submissions<expected_operation->subcount)
        s=&ap_submissions[expected_operation->subfirst+observed_submissions];
    else unexpected_submissions++;
    observed_submissions++;
    if(s){
        check(!memcmp(rgb,ap_blobs[s->rgb],768),"complete captured RGB6 at submission boundary");
        compare_state(&s->state);
    }
    actual_submit(actual_submit_user,rgb);
    for(k=0;k<256;k++){
        for(c=0;c<3;c++)if((int)lrintf(g_hp[k][c]*255)!=expand6(rgb[k*3+c]))colors=0;
        if(g_hp[k][3]!=1.0f)colors=0;
        if(s){
            const uint8_t *w=ap_blobs[s->packed];unsigned word=w[k*2]|(unsigned)w[k*2+1]<<8;
            if(pack565(rgb+k*3)!=word)words=0;
        }
    }
    check(colors,"all submitted per-draw HP colors retain nativeRGB8 contract");
    if(s){check(words,"all captured565 words agree with submitted RGB6 domain");original_submissions++;}
}
static void captured_sequences(int edition)
{
    int c,o,k;const uint8_t (*bank)[768]=g_fxw.palette.bank;const int *valid=g_fxw.palette.valid;
    actual_submit=g_fxw.palette.submit;actual_submit_user=g_fxw.palette.user;
    for(c=0;c<AP_CASE_COUNT;c++)if(ap_cases[c].edition==edition){
        const ap_case *sc=&ap_cases[c];label=sc->name;
        if(!bank||!actual_submit){check(0,"captured replay requires the actual loader's callback/bank");continue;}
        memset(&g_fxw,0,sizeof g_fxw);
        fx_palette_bind(&g_fxw,bank,valid,sc->initial.current,captured_submit,NULL);
        actual_submit(actual_submit_user,bank[sc->initial.current]);
        /* Exact declared initial busy flags from the original case. No
         * interpolation buffer was supplied in that rejection-only case. */
        g_fxw.palette_left=sc->initial.left;g_fxw.palette_back=sc->initial.back;
        g_vision=0;g_pilot=0;g_sim.now=1000;g_fx_ok=1;
        compare_state(&sc->initial);
        for(o=0;o<sc->count;o++){
            const ap_operation *op=&ap_operations[sc->first+o];const float point[3]={79000,3200,19300};
            expected_operation=op;observed_submissions=unexpected_submissions=0;
            g_sim.fx_delta_ticks=op->ticks;compare_state(&op->before);
            switch(op->kind){
            case 0:
                memset(g_fxw.e,0,sizeof g_fxw.e);g_fxw.light_slot=0;
                for(k=0;k<FX_CARD_SLOTS;k++)g_fxd.control[22][k]=g_fxd.render[22][k]=-1;
                g_fxd.slots[22]=op->refused?0:1;
                palette_fixture_producer(&g_sim,point);
                check(g_sim.fx_refused==op->refused,"real producer preserves declared visual admission/refusal");
                break;
            case 1:g_sim.on_palette(g_sim.shot_user,op->slot,op->duration,op->mode,op->ticks);break;
            case 2:fx_palette_step(&g_fxw,op->ticks);break;
            case 3:
#ifndef MERCS_ACCEL_HUD_BEFORE
                accelerated_palette_base(op->slot,op->duration,op->ticks);
#endif
                break;
            case 4:fx_palette_flush(&g_fxw);break;
            }
            check(observed_submissions==op->subcount&&!unexpected_submissions,"captured operation submission count");
            compare_state(&op->after);original_operations++;
        }
    }
    expected_operation=NULL;
    if(bank&&actual_submit)fx_palette_bind(&g_fxw,bank,valid,0,actual_submit,actual_submit_user);
}

static int route_palette_calls,route_effect_calls,route_present_calls,route_end_calls,route_uploads;
static int route_busy,route_refused;
static void (*loaded_palette)(void *,int,int,int,int);
static void (*loaded_presentation)(void *,int);
static void phase_submit(void *u,const uint8_t *rgb)
{
    (void)u;route_uploads++;
    if(route_present_calls==1)check(g_sim.logic.current[5]==1,"native submission before following MTBL completion");
    actual_submit(actual_submit_user,rgb);
}
static void phase_palette(void *u,int slot,int duration,int mode,int ticks)
{
    route_palette_calls++;
    check(slot==18&&duration==3620&&mode==2&&ticks==6,"actual accelerated-profile producer callback values");
    check(route_effect_calls==0,"palette request precedes ordinary visual allocation");
    loaded_palette(u,slot,duration,mode,ticks);
    check(g_fxw.palette_left==(route_busy?30:1),"native producer honors canonical shared admission");
}
static void phase_effect(void *u,int type,const float p[3])
{
    if(type==22){route_effect_calls++;check(route_palette_calls==1,"real visual effect follows palette request");}
    on_effect(u,type,p);
    if(type==22)check(g_sim.fx_refused==route_refused,"full-step actual visual allocator result");
}
static void phase_presentation(void *u,int ticks)
{
    route_present_calls++;
    check(route_effect_calls==1,"native presentation follows leader nuclear producer");
    loaded_presentation(u,ticks);
}
static void phase_end(void *u,int ticks)
{
    int left=g_fxw.palette_left,step=g_fxw.palette.step;
    route_end_calls++;
    check(route_end_calls==route_present_calls,"one presentation phase per native step");
    on_effect_tick(u,ticks);
    check(left==g_fxw.palette_left&&step==g_fxw.palette.step,"bound late FX tick cannot advance palette twice");
}
static void native_routes(void)
{
    const uint8_t (*bank)[768]=g_fxw.palette.bank;const int *valid=g_fxw.palette.valid;
    msim *saved=malloc(sizeof *saved);fx_defs *saved_defs=malloc(sizeof *saved_defs);world_actor *saved_actors=g_actors;int saved_count=g_actor_count;
    int busy,refuse,k,j;
    if(!saved||!saved_defs)exit(2);
    *saved=g_sim;*saved_defs=g_fxd;
    loaded_palette=g_sim.on_palette;loaded_presentation=g_sim.on_presentation;
    actual_submit=g_fxw.palette.submit;actual_submit_user=g_fxw.palette.user;
    for(busy=0;busy<2;busy++)for(refuse=0;refuse<2;refuse++){
        msim *s;
        label=busy?(refuse?"native busy/refused":"native busy/accepted"):(refuse?"native idle/refused":"native idle/accepted");
        if(!bank||!loaded_palette||!loaded_presentation){check(0,"full-step route requires actual loader callbacks");continue;}
        s=palette_fixture_create();g_sim=*s;free(s);g_actors=g_sim.actors;g_actor_count=g_sim.actor_count;
        g_pilot=0;g_vision=0;g_fx_ok=1;g_sim_ok=1;g_sim.fx_delta_ticks=6;
        memset(&g_fxw,0,sizeof g_fxw);memset(&g_fxd,0,sizeof g_fxd);
        for(k=0;k<FX_TYPES;k++)for(j=0;j<FX_CARD_SLOTS;j++)g_fxd.control[k][j]=g_fxd.render[k][j]=-1;
        g_fxd.slots[22]=refuse?0:1;fx_palette_bind(&g_fxw,bank,valid,0,phase_submit,NULL);
        route_palette_calls=route_effect_calls=route_present_calls=route_end_calls=route_uploads=0;route_busy=busy;route_refused=refuse;
        if(busy)red_flash(362,g_sim.now);
        g_sim.on_palette=phase_palette;g_sim.on_effect=phase_effect;g_sim.on_presentation=phase_presentation;g_sim.on_effect_tick=phase_end;g_sim.on_effect_retains=on_effect_retains;
        msim_step(&g_sim,33);full_steps++;
        check(route_palette_calls==1&&route_effect_calls==1&&route_present_calls==1&&route_uploads==1,"one actual producer/presentation/upload");
        check(g_sim.logic.current[5]==3,"following native MTBL pass completes instant node");
        check(g_sim.area_count==(refuse?0:1),"ordinary type22 area follows visual admission");
        check(g_sim.nuclear_wave.remaining_ticks==3620,"separate singleton arms even on visual refusal");
        check(g_fxw.palette.want==(busy?17:0)&&g_fxw.palette_left==(busy?29:100),"captured profile return versus still-busy damage path");
        msim_step(&g_sim,33);full_steps++;
        check(route_palette_calls==1&&route_effect_calls==1&&route_present_calls==2&&route_uploads==2,"second native step does not repeat producer or double-update palette");
        palette_fixture_clear(&g_sim);full_sequences++;
    }
    g_sim=*saved;g_fxd=*saved_defs;g_actors=saved_actors;g_actor_count=saved_count;
    free(saved);free(saved_defs);
    if(bank&&actual_submit)fx_palette_bind(&g_fxw,bank,valid,0,actual_submit,actual_submit_user);
}

static void shared_producers(void)
{
    int left,step,i,j,original_light_ok=g_sim.fx_light_ok;float before[256][4];const float point[3]={10,0,10};
    const uint8_t (*bank)[768]=g_fxw.palette.bank;const int *valid=g_fxw.palette.valid;
    label="accelerated shared producer admission and legacy isolation";
    if(!bank||!g_sim.on_presentation){check(0,"shared producer route requires actual accelerated binding");return;}
    g_sim.now=1000;g_sim.fx_delta_ticks=6;g_vision=0;g_pilot=0;
    fx_palette_bind(&g_fxw,bank,valid,0,actual_submit,actual_submit_user);
    /* Legacy world carriers deliberately outlive a completed shared request.
     * They remain visual state, not a second admission owner. */
    g_flash_t0=1000;g_flash_in=g_flash_back=10000;g_amp_fade_end=999999;
    check(!fade_running(1000),"idle canonical palette is not blocked by legacy wall-clock carriers");
    red_flash(362,1000);check(g_fxw.palette.want==17&&g_fxw.palette_left==30,"real damage request uses canonical frame counter");
    left=g_fxw.palette_left;g_sim.on_palette(NULL,18,3620,2,6);
    check(g_fxw.palette_left==left&&g_fxw.palette.want==17,"nuclear request cannot bypass active damage fade");
    fx_palette_bind(&g_fxw,bank,valid,0,actual_submit,actual_submit_user);
    g_sim.on_palette(NULL,18,3620,2,6);g_sim.on_presentation(NULL,6);
    left=g_fxw.palette_left;red_flash(362,1000);
    check(g_fxw.palette_left==left&&g_fxw.palette.want==0,"damage request refuses throughout nuclear return");
    g_vision=1;g_cockpit=1;g_sim_ok=g_world=1;g_sim.on_presentation(NULL,6);
    check(g_fxw.palette.want==12&&g_fxw.palette.base==12&&g_fxw.palette.queued==12,"vision calls accelerated base begin then stores both slots");
    check(g_fxw.palette.back_slot==-1&&g_fxw.palette_back==0&&g_fxw.palette_left==29,"vision mode0 interrupts current RGB for181ticks");
    memcpy(before,g_hp,sizeof before);left=g_fxw.palette_left;step=g_fxw.palette.step;
    hud_slot(0);
    check(!memcmp(before,g_hp,sizeof before),"legacy sprite-slot reload cannot overwrite submitted per-draw colors");
    on_effect_tick(NULL,6);check(left==g_fxw.palette_left&&step==g_fxw.palette.step,"late actual effect tick retains palette counters");
    g_vision=0;g_sim.on_presentation(NULL,6);
    check(g_fxw.palette.base==0&&g_fxw.palette.queued==0&&g_fxw.palette_left==29,"vision off shares normal duration181 base request");
    fx_palette_bind(&g_fxw,bank,valid,0,actual_submit,actual_submit_user);
    memset(g_fxw.e,0,sizeof g_fxw.e);g_fxw.light_slot=0;
    for(j=0;j<FX_CARD_SLOTS;j++)g_fxd.control[3][j]=g_fxd.render[3][j]=-1;
    g_fxd.slots[3]=1;g_fxw.external_fade=1;g_fx_ok=1;
    check(original_light_ok==0,"actual WOL1 PLNT+56 disables ordinary effect lights");
    on_effect(NULL,3,point);
    check(g_sim.fx_instance>=0&&g_fxw.palette_left==0&&g_fxw.light_slot==0,"actual WOL1 ordinary effect still refuses the light/palette offer");
    /* Declared allowed-scene control, separate from unchanged actual WOL1
     * initialization (whose PLNT+56 is1). No claim WOL1 enables it later. */
    g_sim.fx_light_ok=1;memset(g_fxw.e,0,sizeof g_fxw.e);g_fxw.light_slot=0;
    on_effect(NULL,3,point);i=g_sim.fx_instance;
    check(i>=0&&g_fxw.palette.want==1&&g_fxw.palette_left>0,"actual ordinary effect uses current base-relative palette despite stale legacy carriers");
    check(g_fxw.external_fade==0,"bound profile has one admission owner");
    left=g_fxw.palette_left;g_sim.on_palette(NULL,18,3620,2,6);
    check(g_fxw.palette_left==left&&g_fxw.palette.want==1,"nuclear request cannot bypass active ordinary-light fade");
    check(on_effect_retains(NULL,i,g_fxw.e[i].serial),"actual light retention observes canonical counters");
    while(g_fxw.palette_left>0)fx_palette_step(&g_fxw,6);
    check(!on_effect_retains(NULL,i,g_fxw.e[i].serial),"legacy wall clock cannot prolong effect retention after shared return");
    /* Legacy foundation profiles remain outside this recovered Mercs route. */
    g_tex_kind=0;left=g_fxw.palette_left;step=g_fxw.palette.step;memcpy(before,g_hp,sizeof before);
    on_palette(NULL,18,3620,2,6);on_presentation(NULL,6);
    check(left==g_fxw.palette_left&&step==g_fxw.palette.step&&!memcmp(before,g_hp,sizeof before),"legacy foundation profile excluded from new callback route");
    g_tex_kind=3;
    { fx_palette saved=g_fxw.palette;g_fxw.palette.bank=NULL;g_fxw.palette_left=0;
      check(!fade_running(1000),"DOS unbound zero-frame busy result remains independent of legacy visual carriers");
      g_fxw.palette_left=3;check(fade_running(1000),"DOS unbound positive counter retains prior busy result");
      g_fxw.palette=saved;g_fxw.palette_left=left; }
    g_tex_kind=5;g_sim.fx_light_ok=original_light_ok;
}

static void edition(glr *renderer,const char *path,int profile)
{
    char err[256];prj_archive *a;glr_view view;unsigned char *atlas=NULL,scene_pal[256][3];size_t n;int i;
    const float point[3]={79000,3200,19300};
    label=profile==PORTCFG_ED_3DFX?"dedicated actual load":"Titanium actual load";
    a=prj_open(path,err,sizeof err);if(!a){fprintf(stderr,"%s\n",err);exit(2);}
    g_tex_kind=5;g_edition=profile;g_texset=PORTCFG_TX_ORIGINAL;g_world=1;g_pilot=0;g_cockpit=1;
    g_win_cols=640;g_win_lines=480;g_render_w=g_render_h=0;g_sfx=NULL;g_ma=a;g_fx_ok=0;
    glr_default_view(&view);g_view=&view;
    check(accelerated_archive_open(path)==0,"actual selected accelerated archive open");
    if(load(renderer,a,a,5,"","WOL1",0,0)!=0){fprintf(stderr,"actual load failed\n");exit(2);}loads++;
    check(g_hud&&g_pl_ok&&g_sim_ok,"actual loader creates player and HUD");
    check(g_sim.on_palette==on_palette,"actual load binds nuclear palette callback");
    check(g_sim.on_presentation==on_presentation,"actual load binds shared presentation callback");
    check(g_fxw.palette.bank!=NULL,"actual load binds shared accelerated RGB bank");
    check(g_hud_slot==0,"actual WOL1 initial HUD slot0");
    memcpy(scene_pal,g_scene.palette,sizeof scene_pal);
    n=accelerated_hud_atlas_snapshot(g_hud,&atlas);check(n>0,"actual loaded font atlas exists");
    stage(a,0,atlas,n);
    g_sim.fx_delta_ticks=6;g_sim.now=1000;
    palette_fixture_producer(&g_sim,point);
    check(g_fxw.palette_left==1&&g_fxw.palette_back==603,"actual nuclear producer arms one-step outbound603frame constructor");
    if(g_sim.on_presentation)g_sim.on_presentation(g_sim.shot_user,6);
    stage(a,0,atlas,n);
    if(g_sim.on_presentation)g_sim.on_presentation(g_sim.shot_user,6);
    stage(a,1,atlas,n);
    for(i=0;i<100;i++)if(g_sim.on_presentation)g_sim.on_presentation(g_sim.shot_user,6);
    stage(a,0,atlas,n);
    check(!memcmp(scene_pal,g_scene.palette,sizeof scene_pal),"accelerated font submission does not recolor world palette");
    free(atlas);
    captured_sequences(profile==PORTCFG_ED_3DFX?0:1);
    native_routes();shared_producers();
    /* load() handles scene/actor destruction before the next edition. Keep
     * this archive alive until then; the process owns both read-only handles. */
}
int main(int argc,char **argv)
{
    char err[256]={0};glr *renderer;
    if(argc!=3){fprintf(stderr,"usage: %s dedicated-models.prj titanium-MW2.PRJ\n",argv[0]);return 2;}
    SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO))return 2;
    if(rb_init(NULL,RB_GPU_AUTO,1,err,sizeof err)!=0){fprintf(stderr,"%s\n",err);return 2;}
    rb_frame(640,480);rb_target(0);
    renderer=glr_create();if(!renderer)return 2;
    dp_clear_roots();dp_set_write_root(NULL);
    edition(renderer,argv[1],PORTCFG_ED_3DFX);edition(renderer,argv[2],PORTCFG_ED_D3D);
    printf("RESULT {\"checks\":%d,\"failures\":%d,\"actual_scene_loads\":%d,\"glyph_draws\":%d,\"original_state_operations\":%d,\"original_submissions\":%d,\"native_full_steps\":%d,\"native_full_sequences\":%d,\"scope\":\"actual native load/callback/font/GPU; original captured color domains; no original monitor pixels or campaign\"}\n",check_count,fail_count,loads,draws,original_operations,original_submissions,full_steps,full_sequences);
    glr_destroy(renderer);rb_quit();SDL_Quit();
    return fail_count?1:0;
}
