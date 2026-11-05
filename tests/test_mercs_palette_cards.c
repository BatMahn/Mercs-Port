/* Declared native render-backend controls. No original framebuffer pixels are used.
 * Depth behavior is checked by drawing a later rear card through discarded pixels.
 * No API-specific depth-buffer download is required.
 * RGB6 uses the port's existing DOS upload expansion (v<<2)|(v>>4). */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "rb.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "glr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SIDE 128
static uint8_t palette[256][3], pixels[SIDE*SIDE*3], reference[SIDE*SIDE*3];
static const uint8_t backdrop[3] = {11,19,37};
static int checks, failures, draws;
static const char *stage;
static uint64_t nondos_hash = UINT64_C(14695981039346656037);

static void check(int good, const char *what)
{
    checks++;
    if (!good) { if (failures < 35) fprintf(stderr,"%s: %s\n",stage,what); failures++; }
}
static void draw(glr *r, glr_view *v)
{
    rb_frame(SIDE,SIDE);rb_target(0);rb_scissor(0,0,0,0,0);
    glr_draw(r,v,SIDE,SIDE);
    rb_read_rgb(0,0,SIDE,SIDE,pixels);draws++;
}
static void pixel(int x,int y,const uint8_t rgb[3],int tolerance,const char *what)
{
    const uint8_t *p=pixels+3*(y*SIDE+x); int i,ok=1;
    for(i=0;i<3;i++) if(abs((int)p[i]-(int)rgb[i])>tolerance)ok=0;
    if(!ok && failures<35) fprintf(stderr,"  pixel%d,%d got%d,%d,%d expected%d,%d,%d\n",x,y,p[0],p[1],p[2],rgb[0],rgb[1],rgb[2]);
    check(ok,what);
}
static glr_sprite card(const texture *t,float x,float z,float half,int indexed)
{
    glr_sprite s;memset(&s,0,sizeof s);s.tex=t;s.pos[0]=x;s.pos[2]=z;s.half_w=s.half_h=half;
#ifndef BASELINE_CACHED_RGB
    s.indexed=indexed;
#else
    (void)indexed;
#endif
    return s;
}
static void encode(uint32_t *p,const uint8_t *indices,int n)
{
    int i;
    for(i=0;i<n;i++) {
        unsigned ix=indices[i],alpha=ix==255?0:255;
#ifdef BASELINE_CACHED_RGB
        /* Exact old fx_texture behavior: one expansion at first CEL load. */
        p[i]=(uint32_t)palette[ix][0]|(uint32_t)palette[ix][1]<<8|(uint32_t)palette[ix][2]<<16|alpha<<24;
#else
        p[i]=ix|ix<<8|ix<<16|alpha<<24;
#endif
    }
}
static void palette_initial(void)
{
    int i;
    for(i=0;i<256;i++) {palette[i][0]=(uint8_t)(i^37);palette[i][1]=(uint8_t)(i*13);palette[i][2]=(uint8_t)(i*17);}
    palette[239][0]=palette[240][0]=32;palette[239][1]=palette[240][1]=64;palette[239][2]=palette[240][2]=96;
}
static void palette_allwhite(void)
{
    static const uint8_t ramp[16]={0,4,8,13,17,21,25,29,34,38,42,46,50,55,59,63};
    int i,k;
    for(i=0;i<256;i++)for(k=0;k<3;k++) {unsigned v=i<240?63:ramp[i-240];palette[i][k]=(uint8_t)((v<<2)|(v>>4));}
}
static glr *renderer(glr_view *v,int dos)
{
    glr *r=glr_create(); mech3d empty={0}; texture *slots[GLR_SLOTS]={0};int i;
    if(!r)exit(2);
    glr_default_view(v);v->free_cam=1;v->vfov=90;v->ambient=1;v->fog_density=0;v->bilinear=0;
    for(i=0;i<3;i++)v->sky_color[i]=(float)backdrop[i]/255.0f;
    glr_set_mech(r,&empty,palette,slots,0,0);
    if(dos)glr_set_dos(r,palette,NULL);
    return r;
}
static void cached_palette_and_alpha(void)
{
    static const uint8_t ix[8]={0,1,100,239,240,241,254,255};
    uint32_t texels[8],saved[8],farpx=0xff33cc22u;texture t={0},far={0};glr_view v;glr *r;glr_sprite s[2];int k,pass;
    palette_initial();encode(texels,ix,8);memcpy(saved,texels,sizeof saved);
    t.w=8;t.h=1;t.rgba=texels;far.w=far.h=1;far.rgba=&farpx;r=renderer(&v,1);
    s[0]=card(&t,0,256,128,1);glr_set_sprites(r,s,1);
    for(pass=0;pass<3;pass++) {
        stage=pass==0?"cached initial palette":pass==1?"same cached texture after ALLWHT":"same cached texture restored";
        if(pass==1)palette_allwhite();else if(pass==2)palette_initial();
        glr_set_dos_palette(r,palette);draw(r,&v);
        for(k=0;k<8;k++)pixel(36+8*k,64,k==7?palette[0x60]:palette[ix[k]],0,"palette-index patch");
        check(!memcmp(texels,saved,sizeof saved),"texture CPU storage unchanged through palette update");
    }
    /* Opaque black240 must occlude, whereas transparent255 must not write
     * depth. The far RGBA card is drawn second, after the near indexed one. */
    stage="opaque black / keyed transparent depth";palette_allwhite();glr_set_dos_palette(r,palette);
    s[1]=card(&far,0,512,256,0);glr_set_sprites(r,s,2);draw(r,&v);
    pixel(68,64,palette[240],0,"opaque index240 remains black");
    {const uint8_t green[3]={34,204,51};pixel(92,64,green,0,"index255 hole reveals later far card");}
    memcpy(reference,pixels,sizeof pixels);
    {glr_sprite reversed[2]={s[1],s[0]};glr_set_sprites(r,reversed,2);draw(r,&v);}
    check(!memcmp(reference,pixels,sizeof pixels),"opaque and keyed coverage is independent of near/far submission order");
    glr_destroy(r);
}
static void mixed_cards(void)
{
    const uint8_t ia=239,ib=240;uint32_t a,b,rgba=0xff4d7bc9u;texture ta={0},tb={0},tc={0};glr_sprite s[3],rev[3];glr_view v;glr *r;int pass;
    palette_initial();encode(&a,&ia,1);encode(&b,&ib,1);
    ta.w=ta.h=tb.w=tb.h=tc.w=tc.h=1;ta.rgba=&a;tb.rgba=&b;tc.rgba=&rgba;r=renderer(&v,1);
    s[0]=card(&ta,-80,256,28,1);s[1]=card(&tc,0,256,28,0);s[2]=card(&tb,80,256,28,1);
    for(pass=0;pass<3;pass++) {
        static const uint8_t rgb[3]={201,123,77};stage="mixed indexed / procedural RGBA cards";
        if(pass==1)palette_allwhite();else if(pass==2)palette_initial();
        glr_set_dos_palette(r,palette);
        if(pass==2) {rev[0]=s[2];rev[1]=s[1];rev[2]=s[0];glr_set_sprites(r,rev,3);}else glr_set_sprites(r,s,3);
        draw(r,&v);pixel(44,64,palette[ia],0,"first indexed card");pixel(64,64,rgb,0,"procedural RGBA card unchanged");pixel(84,64,palette[ib],0,"last indexed card");
    }
    glr_destroy(r);
}
static void filtered_indices(void)
{
    uint8_t ix[2]={1,2};uint32_t texels[2];texture t={0};glr_sprite s;glr_view v;glr *r;int x;
    memset(palette,0,sizeof palette);palette[1][0]=255;palette[2][2]=255;palette[255][1]=255;
    encode(texels,ix,2);t.w=2;t.h=1;t.rgba=texels;r=renderer(&v,1);v.dos_texture_linear=1;
    s=card(&t,0,256,128,1);glr_set_sprites(r,&s,1);stage="decoded-color linear filtering";draw(r,&v);
    for(x=52;x<=76;x+=4) {
        float w=((float)x+0.5f-32)/32.0f-0.5f;uint8_t expected[3];
        if(w<0)w=0;
        if(w>1)w=1;
        expected[0]=(uint8_t)lroundf(255*(1-w));expected[1]=0;expected[2]=(uint8_t)lroundf(255*w);
        pixel(x,64,expected,1,"neighboring indices mix decoded red/blue, never index numbers");
    }
    /* A new texture pointer gives an independent cache entry; keyed green255
     * cannot bleed into opaque red when the decoded taps are premultiplied. */
    {
        uint32_t keyed_pixels[2];texture keyed=t;ix[1]=255;encode(keyed_pixels,ix,2);keyed.rgba=keyed_pixels;
        s=card(&keyed,0,256,128,1);glr_set_sprites(r,&s,1);stage="premultiplied filtered alpha key";draw(r,&v);
        {const uint8_t red[3]={255,0,0};pixel(60,64,red,1,"transparent key cannot tint accepted texel");}
        pixel(68,64,palette[0x60],0,"filtered alpha below threshold discards to DOS sky palette");
        {uint32_t rear_pixel=0xff33cc22u;texture rear={0};glr_sprite pair[2];
         const uint8_t red[3]={255,0,0},green[3]={34,204,51};
         rear.w=rear.h=1;rear.rgba=&rear_pixel;pair[0]=s;pair[1]=card(&rear,0,512,256,0);
         glr_set_sprites(r,pair,2);draw(r,&v);
         pixel(60,64,red,1,"filtered opaque texel occludes later rear card");
         pixel(68,64,green,0,"filtered key discard leaves depth open to rear card");}
    }
    glr_destroy(r);
}
static void geometry_reference(void)
{
    const uint8_t ix[4]={1,239,240,255};uint32_t indices[4],rgba[4];texture t={0},ref={0};glr *r;glr_view v;glr_sprite s;int geometry,k,changed;
    palette_initial();encode(indices,ix,4);
    for(k=0;k<4;k++)rgba[k]=(uint32_t)palette[ix[k]][0]|(uint32_t)palette[ix[k]][1]<<8|(uint32_t)palette[ix[k]][2]<<16|(uint32_t)(ix[k]==255?0:255)<<24;
    t.w=t.h=ref.w=ref.h=2;t.rgba=indices;ref.rgba=rgba;r=renderer(&v,1);
    for(geometry=0;geometry<4;geometry++) {
        stage=geometry==0?"camera-card decoded reference":geometry==1?"engine billboard decoded reference":geometry==2?"oriented quad decoded reference":"wire geometry reference";
        s=card(&ref,0,256,100,0);
        if(geometry==1){s.bb=1;s.pos[1]=-100;s.top[1]=100;s.top[2]=256;}
        if(geometry==2){s.quad=1;for(k=0;k<4;k++){s.qv[k][0]=(k==0||k==3)?-100:100;s.qv[k][1]=k<2?-100:100;s.qv[k][2]=256;s.quv[k][0]=(k==1||k==2)?1:0;s.quv[k][1]=k>=2?0:1;}}
        v.wire=geometry==3;glr_set_sprites(r,&s,1);draw(r,&v);memcpy(reference,pixels,sizeof pixels);
        s.tex=&t;
#ifndef BASELINE_CACHED_RGB
        s.indexed=1;
#endif
        glr_set_sprites(r,&s,1);draw(r,&v);
        check(!memcmp(reference,pixels,sizeof pixels),"same RGB and coverage as independently expanded RGBA");
        changed=0;for(k=0;k<SIDE*SIDE;k++)if(memcmp(reference+k*3,palette[0x60],3))changed++;
        check(changed>100,"reference geometry visibly exercised");
        /* The rear card has the same projected outline at twice the distance,
         * including wire edges. It exposes any difference in depth coverage. */
        {uint32_t rear_pixel=0xff776655u;texture rear={0};glr_sprite pair[2];
         rear.w=rear.h=1;rear.rgba=&rear_pixel;pair[0]=s;pair[0].tex=&ref;pair[0].indexed=0;
         pair[1]=card(&rear,0,512,200,0);glr_set_sprites(r,pair,2);draw(r,&v);memcpy(reference,pixels,sizeof pixels);
         pair[0]=s;glr_set_sprites(r,pair,2);draw(r,&v);
         check(!memcmp(reference,pixels,sizeof pixels),"same rear-card occlusion as independently expanded RGBA");}
    }
    glr_destroy(r);
}
static void non_dos_control(void)
{
    uint32_t px=0x803264c8u;texture t={0};glr_sprite s;glr_view v;glr *r;int blend,k;
    t.w=t.h=1;t.rgba=&px;palette_initial();r=renderer(&v,0);s=card(&t,0,256,100,0);s.ti=1;
    for(blend=0;blend<2;blend++) {
        uint8_t expected[3];const int raw[3]={200,100,50};stage="non-DOS Titanium tint/blend/depth control";
        v.fx_blend=blend;glr_set_sprites(r,&s,1);draw(r,&v);
        for(k=0;k<3;k++){float c=(float)raw[k]*200/255.0f;if(blend)c=c*128/255.0f+backdrop[k]*127/255.0f;expected[k]=(uint8_t)lroundf(c);}
        pixel(64,64,expected,1,"unchanged per-card Titanium tint and blend");
        for(k=0;k<SIDE*SIDE*3;k++){nondos_hash^=pixels[k];nondos_hash*=UINT64_C(1099511628211);}
        {uint32_t rear_pixel=0xff33cc22u;texture rear={0};glr_sprite pair[2];const uint8_t green[3]={34,204,51};
         rear.w=rear.h=1;rear.rgba=&rear_pixel;pair[0]=s;pair[1]=card(&rear,0,512,200,0);
         glr_set_sprites(r,pair,2);draw(r,&v);
         pixel(64,64,blend?green:expected,1,"Titanium blend disables depth writes; opaque mode occludes rear card");}
    }
    glr_destroy(r);
}
int main(void)
{
    char err[256]={0};
    SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO)){fprintf(stderr,"%s\n",SDL_GetError());return 2;}
    if(rb_init(NULL,RB_GPU_AUTO,1,err,sizeof err)!=0){fprintf(stderr,"%s\n",err);return 2;}
    rb_frame(SIDE,SIDE);rb_target(0);
    fprintf(stderr,"Backend: %s; device: %s\n",rb_backend_name(),rb_device_name());
    cached_palette_and_alpha();mixed_cards();filtered_indices();geometry_reference();non_dos_control();
    rb_quit();SDL_Quit();
    printf("{\"checks\":%d,\"failures\":%d,\"draws\":%d,\"non_dos_rgb_fnv64\":\"%016llx\","
           "\"conversion\":\"native DOS bit replication\",\"original_pixel_claim\":false}\n",checks,failures,draws,(unsigned long long)nondos_hash);
    return failures?1:0;
}
