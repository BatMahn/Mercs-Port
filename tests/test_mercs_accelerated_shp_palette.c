/* Actual native loader/cache/draw discriminator for captured RETICLE6 colors.
 * RGB6->RGB8 remains the native bit-replication contract. Original packed
 * words are compared separately; no original live-reticle caller is claimed. */
#include <SDL3/SDL.h>
#include "src/rb.h"
static void fixture_present(void);
#define rb_present fixture_present
#define main unused_native_frontend_main
#include "tools/glview.c"
#undef main
#undef rb_present
#include <SDL3/SDL_main.h>
#include "src/shp.h"
#include "accelerated-shp-fixtures.h"
extern void palette_fixture_producer(msim *,const float[3]);
extern void shp_fixture_upload_reset(void);
extern unsigned shp_fixture_upload_calls(void);
extern uint64_t shp_fixture_upload_bytes(void);
extern int shp_fixture_cache_count(void);
extern int shp_fixture_index_used(int);
extern int shp_fixture_source_matches(const hud_sprite *,const shp_frame *);
extern size_t shp_fixture_sources_snapshot(unsigned char **);
extern size_t shp_fixture_atlas_snapshot(hud *,unsigned char **);
extern size_t shp_fixture_texture_snapshot(unsigned,unsigned char **,int *,int *);
extern int shp_fixture_texture_live(unsigned);
extern int shp_fixture_dirty(unsigned);
extern int shp_fixture_dirty_count(void);
extern void shp_fixture_fail_allocation(size_t);
extern uint64_t shp_fixture_upload_ns(void);
extern void shp_fixture_track_draws(int);
extern unsigned shp_fixture_drawn_count(void);
extern unsigned shp_fixture_draw_calls(void);
static int checks,failures,loads,gpu_draws,original_draw_comparisons,cache_checks;
static int full_hud_audit,audit_frames,deferred_controls,deferred_draws;
static uint64_t audit_since;
static const char *label;
static void check(int ok,const char *what)
{
    checks++;if(!ok){if(failures<40)fprintf(stderr,"FAIL %s: %s\n",label,what);failures++;}
}
static int expand6(int x){return (x<<2)|(x>>4);}
static unsigned pack(const unsigned char *p,int format)
{
    return format==1?((unsigned)(p[0]>>3)<<10)|((unsigned)(p[1]>>3)<<5)|(p[2]>>3):
                     ((unsigned)(p[0]>>3)<<11)|((unsigned)(p[1]>>2)<<5)|(p[2]>>3);
}
static void unchanged_sources(const unsigned char *saved,size_t size,const unsigned char *atlas,size_t atlas_size)
{
    unsigned char *now=NULL;size_t n=shp_fixture_sources_snapshot(&now);
    check(n==size&&n>0&&!memcmp(now,saved,n),"cached indexed art, masks, remaps and texture IDs unchanged");free(now);
    n=shp_fixture_atlas_snapshot(g_hud,&now);
    check(n==atlas_size&&n>0&&!memcmp(now,atlas,n),"font atlas RGBA submissions unchanged by SHP palette upload");free(now);cache_checks++;
}
static void draw_check(int profile,int stage,int placement,const hud_sprite *sprite)
{
    unsigned char rgb[64*64*3];unsigned short expected[4096];int f,di,k,x,y;
    const unsigned char bg[3]={29,47,83};const shp_original_draw *input=NULL;
    for(di=0;di<SHP_ORIGINAL_DRAW_COUNT;di++)if(shp_original_draws[di].format==(profile?2:0)&&shp_original_draws[di].stage==stage&&shp_original_draws[di].placement==placement){input=&shp_original_draws[di];break;}
    check(input!=NULL,"captured original draw input found");if(!input)return;
    {const float clear[4]={bg[0]/255.0f,bg[1]/255.0f,bg[2]/255.0f,1};
     rb_frame(640,480);rb_target(0);rb_viewport(0,0,640,480);rb_scissor(0,0,0,0,0);rb_depth_write(1);rb_clear(clear,1,1);}
    hud_begin(g_hud,640,480);hud_draw(g_hud,sprite,(float)input->x,(float)input->y,NULL);hud_end(g_hud);
    rb_read_rgb(0,480-64,64,64,rgb);gpu_draws++;
    for(f=profile?2:0;f<=(profile?2:1);f++){
        const shp_original_draw *d=NULL;int words_ok=1,rgb_ok=1,bg_ok=1,opaque=0;
        for(di=0;di<SHP_ORIGINAL_DRAW_COUNT;di++)if(shp_original_draws[di].format==f&&shp_original_draws[di].stage==stage&&shp_original_draws[di].placement==placement){d=&shp_original_draws[di];break;}
        check(d!=NULL,"captured original format variant found");if(!d)continue;
        for(k=0;k<4096;k++)expected[k]=0x5a5a;
        for(k=0;k<d->count;k++){const shp_original_pixel *p=&shp_original_pixels[d->first+k];expected[p->at]=p->word;}
        for(y=0;y<64;y++)for(x=0;x<64;x++){
            const unsigned char *p=rgb+((63-y)*64+x)*3;unsigned want=expected[y*64+x];
            if(want==0x5a5a){for(k=0;k<3;k++)if(p[k]!=bg[k])bg_ok=0;}
            else {
                int ci;opaque++;if(pack(p,f)!=want)words_ok=0;
                for(ci=13;ci<=15&&shp_original_words[f][stage][ci]!=want;ci++);
                if(ci>15)rgb_ok=0;
                else for(k=0;k<3;k++)if(p[k]!=expand6(shp_original_rgb6[stage][ci*3+k]))rgb_ok=0;
            }
        }
        check(opaque==d->count,"captured opaque footprint count");
        check(words_ok,"actual GPU cached reticle matches captured original packed word colors");
        check(rgb_ok,"actual GPU retains explicit native RGB8 expansion");
        check(bg_ok,"original skipped pixels/clipping preserve native sentinel background");original_draw_comparisons++;
    }
}
static void stage_checks(int profile,int stage,const unsigned char *sources,size_t size,const unsigned char *atlas,size_t atlas_size)
{
    int index,k;float p[4];
    for(index=13;index<=15;index++){
        hud_pal_color(g_hud,index,p);
        for(k=0;k<3;k++){
            check((int)lrintf(g_hp[index][k]*255)==expand6(shp_original_rgb6[stage][index*3+k]),"current HP table matches captured reticle ink");
            check((int)lrintf(p[k]*255)==expand6(shp_original_rgb6[stage][index*3+k]),"cached sprite palette matches captured reticle ink");
        }
    }
    draw_check(profile,stage,0,&g_hs_reticle);draw_check(profile,stage,1,&g_hs_reticle);
    unchanged_sources(sources,size,atlas,atlas_size);
}
static void metrics(int profile,const char *stage)
{
    printf("UPLOAD {\"profile\":%d,\"stage\":\"%s\",\"cache_entries\":%d,\"calls\":%u,\"rgba_bytes\":%llu,\"mapping_scale_bleed_upload_ns\":%llu}\n",profile,stage,shp_fixture_cache_count(),shp_fixture_upload_calls(),(unsigned long long)shp_fixture_upload_bytes(),(unsigned long long)shp_fixture_upload_ns());
}
static void fixture_present(void)
{
    if(full_hud_audit&&g_hud&&g_sim_ok){
        uint64_t now=SDL_GetTicksNS();
        printf("HUD_FRAME {\"frame\":%d,\"sim_ms\":%d,\"palette_left\":%d,\"cache_entries\":%d,\"drawn_entries\":%u,\"sprite_draw_calls\":%u,\"uploads\":%u,\"rgba_bytes\":%llu,\"upload_ns\":%llu,\"instrumented_frame_ns\":%llu}\n",audit_frames,g_sim.now,g_fxw.palette_left,shp_fixture_cache_count(),shp_fixture_drawn_count(),shp_fixture_draw_calls(),shp_fixture_upload_calls(),(unsigned long long)shp_fixture_upload_bytes(),(unsigned long long)shp_fixture_upload_ns(),(unsigned long long)(audit_since?now-audit_since:0));
        /* Declared visual request through the actual bound callback, without
         * invoking a nuclear damage producer or touching a saved career. */
        if(audit_frames==0&&g_sim.on_palette)g_sim.on_palette(g_sim.shot_user,18,3620,2,6);
        audit_frames++;shp_fixture_upload_reset();shp_fixture_track_draws(1);audit_since=SDL_GetTicksNS();
    }
    rb_present();
}
#ifdef MERCS_SHP_DEFERRED
static unsigned char *texture_snapshot(const hud_sprite *s,size_t *size)
{
    unsigned char *p=NULL;
    *size=shp_fixture_texture_snapshot(s->tex,&p,NULL,NULL);return p;
}
static void render_route(const hud_sprite *s,int route,unsigned char *pixels)
{
    const float clear[4]={.1f,.2f,.3f,1};
    rb_frame(640,480);rb_target(0);rb_scissor(0,0,0,0,0);rb_depth_write(1);rb_clear(clear,1,1);hud_begin(g_hud,640,480);
    switch(route){
    case 0:hud_draw(g_hud,s,100,100,NULL);break;
    case 1:hud_draw_scaled(g_hud,s,100,100,2);break;
    case 2:hud_draw_f(g_hud,s,100,100,1.25f);break;
    case 3:hud_draw_stretch_row(g_hud,s,25,0);break;
    case 4:hud_draw_window(g_hud,s,.2f,21,100,100,NULL);break;
    case 5:hud_draw_region(g_hud,s,100,100,3,4,20,19);break;
    case 6:hud_draw_window_v(g_hud,s,.1f,25,100,100);break;
    }
    hud_end(g_hud);rb_read_rgb(0,0,640,480,pixels);deferred_draws++;
}
static void deferred_checks(prj_archive *a)
{
    hud_sprite remapped,fresh;unsigned char *expected=malloc(640*480*3),*actual=malloc(640*480*3),*texture=NULL,*got=NULL;
    size_t texture_size=0,got_size=0;int route,cache_before;unsigned dead;
    check(expected&&actual,"deferred comparison buffers allocated");if(!expected||!actual)exit(2);
    hud_repalette(g_hud,shp_original_rgb6[0]);
    check(hud_sprite_load_remap(g_hud,a,"RETICLE6",0,13,15,&remapped)==0,"actual remapped SHP cached");
    for(route=0;route<7;route++){
        hud_repalette(g_hud,shp_original_rgb6[1]);render_route(&remapped,route,expected);
        free(texture);texture=texture_snapshot(&remapped,&texture_size);
        hud_repalette(g_hud,shp_original_rgb6[0]);shp_fixture_upload_reset();
        hud_repalette_deferred(g_hud,shp_original_rgb6[1]);
        check(shp_fixture_upload_calls()==0&&shp_fixture_dirty(remapped.tex),"deferred RGB change marks affected remapped cache without upload");
        render_route(&remapped,route,actual);
        check(shp_fixture_upload_calls()==1&&!shp_fixture_dirty(remapped.tex),"each public sprite draw route resolves dirtiness once");
        check(!memcmp(expected,actual,640*480*3),"deferred draw is pixel-identical to eager Scale3x/blend/filter result");
        free(got);got=texture_snapshot(&remapped,&got_size);
        check(got_size==texture_size&&!memcmp(texture,got,got_size),"complete deferred upload matches eager RGBA including alpha/bleed");
        shp_fixture_upload_reset();render_route(&remapped,route,actual);check(shp_fixture_upload_calls()==0,"repeat draw does not rebuild clean sprite");
        deferred_controls++;
    }
    hud_repalette(g_hud,shp_original_rgb6[0]);shp_fixture_upload_reset();
    hud_repalette_deferred(g_hud,shp_original_rgb6[1]);hud_repalette_deferred(g_hud,shp_original_rgb6[0]);hud_repalette_deferred(g_hud,shp_original_rgb6[1]);
    check(shp_fixture_upload_calls()==0,"three unrendered submissions coalesce without upload");render_route(&remapped,0,actual);
    check(shp_fixture_upload_calls()==1,"latest of collapsed submissions uploads once at draw");
    /* Eager submission of identical RGB must flush pending other entries. */
    cache_before=shp_fixture_dirty_count();shp_fixture_upload_reset();
    check(hud_repalette(g_hud,shp_original_rgb6[1])==0,"identical palette retains existing changed-return contract");
    check(cache_before>0&&shp_fixture_upload_calls()==(unsigned)cache_before&&!shp_fixture_dirty_count(),"eager identical input flushes all pending dirty caches");
    /* Refuse the next actual RGBA scratch allocation; retry is bounded to
     * this draw and does not change controller or career state. */
    hud_repalette(g_hud,shp_original_rgb6[0]);free(texture);texture=texture_snapshot(&remapped,&texture_size);
    hud_repalette_deferred(g_hud,shp_original_rgb6[1]);shp_fixture_upload_reset();shp_fixture_fail_allocation(33u*33u*4u);
    render_route(&remapped,0,actual);free(got);got=texture_snapshot(&remapped,&got_size);
    check(shp_fixture_upload_calls()==0&&shp_fixture_dirty(remapped.tex),"allocation refusal retains pending rebuild");
    check(got_size==texture_size&&!memcmp(texture,got,got_size),"failed rebuild leaves previous complete texture intact");
    shp_fixture_upload_reset();render_route(&remapped,0,actual);
    check(shp_fixture_upload_calls()==1&&!shp_fixture_dirty(remapped.tex),"next draw retries and completes refused rebuild");
    cache_before=shp_fixture_cache_count();dead=remapped.tex;hud_repalette_deferred(g_hud,shp_original_rgb6[0]);hud_sprite_free(&remapped);
    check(shp_fixture_cache_count()==cache_before-1&&!shp_fixture_texture_live(dead),"free removes dirty cache and texture before reload");
    check(hud_sprite_load_remap(g_hud,a,"RETICLE6",0,13,15,&remapped)==0&&!shp_fixture_dirty(remapped.tex),"reloaded remapped sprite immediately uses current palette");
    /* Initial upload allocation failure also remains retryable in retained
     * cache. General cache-growth or GPU-driver OOM is outside this case. */
    shp_fixture_fail_allocation(33u*33u*4u);check(hud_sprite_load(g_hud,a,"RETICLE6",0,&fresh)==0&&shp_fixture_dirty(fresh.tex),"retained new sprite tracks refused initial RGBA upload");
    shp_fixture_upload_reset();render_route(&fresh,0,actual);check(shp_fixture_upload_calls()==1&&!shp_fixture_dirty(fresh.tex),"new sprite allocation refusal retries on first draw");
    hud_sprite_free(&fresh);hud_sprite_free(&remapped);hud_repalette(g_hud,shp_original_rgb6[0]);
    free(texture);free(got);free(expected);free(actual);deferred_controls+=7;
}
#endif
static void edition(glr *renderer,const char *path,int profile)
{
    char err[256];prj_archive *a;prj_record resource;shp_frame frame;glr_view view;
    unsigned char *sources=NULL,*atlas=NULL,world_pal[256][3],*texture=NULL;size_t size,atlas_size;int i,w,h;
    unsigned texture_id;const float point[3]={79000,3200,19300};
    label=profile?"Titanium actual cached SHP":"dedicated actual cached SHP";
    a=prj_open(path,err,sizeof err);if(!a){fprintf(stderr,"%s\n",err);exit(2);}
    g_tex_kind=5;g_edition=profile?PORTCFG_ED_D3D:PORTCFG_ED_3DFX;g_texset=PORTCFG_TX_ORIGINAL;
    g_world=1;g_pilot=0;g_cockpit=1;g_win_cols=640;g_win_lines=480;g_render_w=g_render_h=0;g_sfx=NULL;g_ma=a;g_fx_ok=0;
    glr_default_view(&view);g_view=&view;check(accelerated_archive_open(path)==0,"actual accelerated archive selected");
    if(load(renderer,a,a,5,"","WOL1",0,0)){fprintf(stderr,"actual load failed\n");exit(2);}loads++;
    check(g_hud&&g_sim_ok&&g_sim.on_palette==on_palette&&g_sim.on_presentation==on_presentation,"actual loader binds shared callbacks");
    check(!strcmp(g_hud_sfx,"6")&&g_hs_reticle.tex!=0,"actual loader selects RETICLE6 cache entry");
    check(prj_read_named(a,"SHP","RETICLE6",&resource)==PRJ_OK,"actual reticle record readable");
    check(resource.size==347&&shp_decode(resource.data,resource.size,0,&frame)==0,"actual347-byte SHP frame decoded");
    check(frame.w==33&&frame.h==33&&frame.left==-16&&frame.top==-16,"original authored reticle bounds");
    check(shp_fixture_source_matches(&g_hs_reticle,&frame),"actual loader cache retains exact resource indices and mask");
    size=shp_fixture_sources_snapshot(&sources);atlas_size=shp_fixture_atlas_snapshot(g_hud,&atlas);texture_id=g_hs_reticle.tex;
    memcpy(world_pal,g_scene.palette,sizeof world_pal);
    stage_checks(profile,0,sources,size,atlas,atlas_size);
    shp_fixture_upload_reset();g_sim.fx_delta_ticks=6;g_sim.now=1000;palette_fixture_producer(&g_sim,point);
    g_sim.on_presentation(g_sim.shot_user,6);metrics(profile,"initial-outbound");
    check(shp_fixture_upload_calls()==0,"unchanged first RGB6 submission does not reupload cached textures");
    shp_fixture_upload_reset();g_sim.on_presentation(g_sim.shot_user,6);metrics(profile,"ALLWHT");
#ifdef MERCS_SHP_DEFERRED
    check(shp_fixture_upload_calls()==0&&shp_fixture_dirty(g_hs_reticle.tex),"changed RGB defers affected reticle upload until actual draw");
#else
    check(shp_fixture_upload_calls()>0,"changed submitted palette refreshes affected cached SHP textures");
#endif
    stage_checks(profile,1,sources,size,atlas,atlas_size);
    /* A delayed legacy world-slot reload must not replace submitted colors. */
    shp_fixture_upload_reset();hud_slot(0);metrics(profile,"delayed-slot0");
    check(shp_fixture_upload_calls()==0,"bound delayed legacy slot cannot upload stale sprite palette");
    stage_checks(profile,1,sources,size,atlas,atlas_size);
    /* Lazy load uses the current submitted palette, without rewriting art. */
    {hud_sprite lazy;check(hud_sprite_load(g_hud,a,"RETICLE6",0,&lazy)==0,"lazy actual reticle load succeeds");
     check(shp_fixture_source_matches(&lazy,&frame),"lazy cache retains unchanged indices and mask");draw_check(profile,1,0,&lazy);hud_sprite_free(&lazy);}
    shp_fixture_upload_reset();for(i=0;i<100;i++)g_sim.on_presentation(g_sim.shot_user,6);metrics(profile,"return100");
    stage_checks(profile,2,sources,size,atlas,atlas_size);
    check(g_hs_reticle.tex==texture_id,"palette transitions preserve actual texture object identity");
    check(!memcmp(world_pal,g_scene.palette,sizeof world_pal),"SHP callback does not write world palette");
    /* Repeated same-palette submissions must not upload cached art. */
    shp_fixture_upload_reset();g_fxw.palette.submit(g_fxw.palette.user,shp_original_rgb6[2]);
    check(shp_fixture_upload_calls()==0,"identical submitted palette skips every cached texture upload");
    /* A declared changed index unused by this actual HUD changes no texture. */
    for(i=0;i<256&&shp_fixture_index_used(i);i++);
    check(i<256,"actual cache has an unused palette index for bounded control");
    if(i<256){unsigned char p[768];memcpy(p,shp_original_rgb6[2],sizeof p);p[i*3]^=1;
      shp_fixture_upload_reset();g_fxw.palette.submit(g_fxw.palette.user,p);check(shp_fixture_upload_calls()==0,"unused-index change skips all texture uploads");
      g_fxw.palette.submit(g_fxw.palette.user,shp_original_rgb6[2]);}
    /* Check a representative cache texture remains allocated after return. */
    {size_t bytes=shp_fixture_texture_snapshot(g_hs_reticle.tex,&texture,&w,&h);
     check(w==99&&h==99,"retained native Scale3x texture dimensions");
     check(texture!=NULL&&bytes==99u*99u*4u&&shp_fixture_texture_live(g_hs_reticle.tex),"cached texture retains complete RGBA submission");free(texture);}
    /* Declared unbound control preserves the pre-binding legacy slot path. */
    {
        const uint8_t (*bank)[768]=g_fxw.palette.bank;float color[4];int k;
        g_fxw.palette.bank=NULL;hud_slot(12);
        for(i=13;i<=15;i++){
            hud_pal_color(g_hud,i,color);
            for(k=0;k<3;k++)check((int)lrintf(color[k]*255)==expand6(g_pal_slot[12][i*3+k])&&
                (int)lrintf(g_hp[i][k]*255)==expand6(g_pal_slot[12][i*3+k]),"unbound accelerated slot still owns cache and HP");
        }
        hud_slot(0);g_fxw.palette.bank=bank;
    }
    free(sources);free(atlas);shp_frame_free(&frame);prj_record_free(&resource);
#ifdef MERCS_SHP_DEFERRED
    deferred_checks(a);
#endif
}
int main(int argc,char **argv)
{
    char err[256]={0};glr *renderer;
    if(argc==3&&!strcmp(argv[1],"--full-hud")){
        char *native_argv[]={argv[0],argv[2],argv[2],"mercs16","@","WOL1",NULL};
        full_hud_audit=1;return unused_native_frontend_main(6,native_argv);
    }
    if(argc!=3){fprintf(stderr,"usage: %s dedicated-models.prj titanium-MW2.PRJ\n",argv[0]);return 2;}
    SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO))return 2;
    if(rb_init(NULL,RB_GPU_AUTO,1,err,sizeof err)!=0){fprintf(stderr,"%s\n",err);return 2;}
    rb_frame(640,480);rb_target(0);
    renderer=glr_create();if(!renderer)return 2;dp_clear_roots();dp_set_write_root(NULL);
    edition(renderer,argv[1],0);edition(renderer,argv[2],1);
    printf("RESULT {\"checks\":%d,\"failures\":%d,\"actual_scene_loads\":%d,\"captured_render_cases\":%d,\"deferred_render_cases\":%d,\"render_cases_total\":%d,\"original_draw_comparisons\":%d,\"cache_snapshots\":%d,\"deferred_control_groups\":%d,\"scope\":\"actual native loader/cache/RETICLE6 GPU; original captured RGB6/packed words; no original live reticle binding or world recoloring\"}\n",checks,failures,loads,gpu_draws,deferred_draws,gpu_draws+deferred_draws,original_draw_comparisons,cache_checks,deferred_controls);
    glr_destroy(renderer);rb_quit();SDL_Quit();
    return failures?1:0;
}
