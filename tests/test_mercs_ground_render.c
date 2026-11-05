/* Synthetic original-binary decisions and real GPU ground regressions. */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "sdl3_test.h"
#include "gpu_test.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glr.h"
#include "mercs_ground_original.h"

static int passed, failed;
static void check(int condition, const char *name)
{
    if (condition) passed++;
    else { fprintf(stderr, "FAIL: %s\n", name); failed++; }
}
static void originals(void)
{
    size_t i;
    for (i = 0; i < sizeof original_ground_grid / sizeof original_ground_grid[0]; i++) {
        int32_t tiles[4][2];
        check(mercs_ground_tiles(original_ground_grid[i].x,original_ground_grid[i].z,
                                original_ground_grid[i].side,tiles)==4 &&
              memcmp(tiles,original_ground_grid[i].tiles,sizeof tiles)==0, "executed original DOS ground grid");
    }
    for (i = 0; i < sizeof original_ground_shade / sizeof original_ground_shade[0]; i++)
        check(mercs_ground_shade(original_ground_shade[i].ambient,original_ground_shade[i].delta,
              original_ground_shade[i].near_cm,original_ground_shade[i].far_cm,original_ground_shade[i].first,
              original_ground_shade[i].depth)==original_ground_shade[i].value, "executed original DOS ground shade");
}

static int read_ground(glr *r, glr_view *v, int x, int y)
{
    unsigned char p[4];
    gpu_test_draw(r,v,320,320);
    gpu_test_read_rgba(x, y, 1, 1, p);
    if (!gpu_test_ok() || p[0]!=p[1] || p[1]!=p[2] || p[3]!=255) return -1000;
    return p[0];
}

static int original_fog(int mode, int depth, int z)
{
    size_t i;
    for (i=0;i<sizeof original_ground_fog/sizeof original_ground_fog[0];i++)
        if (original_ground_fog[i].depth==depth && original_ground_fog[i].z==z &&
            original_ground_fog[i].y==0 && original_ground_fog[i].camera==1)
            return mode==2 ? original_ground_fog[i].dedicated : original_ground_fog[i].titanium;
    return -10000;
}

static void accelerated(glr *r, glr_view *v, texture *white, mercs_ground *ground)
{
    size_t i;
    int mode;
    for (mode=2;mode<=3;mode++) {
        ground->mode=mode; ground->size=mode==2?320000:640000;
        glr_set_mercs_ground(r,white,ground);
        v->ti_fog_vtx=mode==2?2:1; v->look_pitch=-90;
        for (i=0;i<sizeof original_ground_fog/sizeof original_ground_fog[0];i++) {
            int got,want;
            /* Beyond all dedicated bands / native far is a geometry case,
             * not a uniform-plane fog fixture. */
            if (original_ground_fog[i].depth>=20000) continue;
            v->eye[1]=original_ground_fog[i].depth==60?60.1f:original_ground_fog[i].depth;
            v->ti_fog[0]=1; v->ti_fog[1]=original_ground_fog[i].z; v->ti_fog[2]=original_ground_fog[i].camera;
            v->ti_fog_height[0]=original_ground_fog[i].y; v->ti_fog_height[1]=original_ground_fog[i].minimum;
            want=mode==2?original_ground_fog[i].dedicated:original_ground_fog[i].titanium;
            got=read_ground(r,v,160,160);
            if (abs(got-want)>1) fprintf(stderr,"mode%d original-fog%zu depth%g got%d wanted%d\n",mode,i,(double)v->eye[1],got,want);
            check(abs(got-want)<=1,"GPU ground original fog byte / H(0) / camera factor");
        }
    }
    /* At a45-degree downward camera, depth(y)=sqrt(2)*height/(1-NDCy).
     * Screen-linear interpolation between executed-original endpoint bytes
     * is an independent reference. A per-fragment reciprocal/linear fog
     * curve produces substantially different pixels and fails these checks. */
    v->eye[1]=1000; v->look_pitch=-45;
    v->ti_fog[0]=1; v->ti_fog[2]=1; v->ti_fog_height[0]=0;
    for (mode=2;mode<=3;mode++) {
        static const int sample_y[]={16,32,60,120,160,230,300};
        ground->mode=mode; ground->size=mode==2?320000:640000;
        glr_set_mercs_ground(r,white,ground);
        v->ti_fog_vtx=mode==2?2:1; v->ti_fog[1]=mode==2?1000:4000;
        for (i=0;i<sizeof sample_y/sizeof sample_y[0];i++) {
            const float sy=(2.0f*sample_y[i]+1)/320-1;
            const float d=sqrtf(2.0f)*1000/(1-sy);
            int lo,hi,a,b,got,want;
            float yl,yh,t;
            if (mode==2) {
                if (d<1250) {lo=60;hi=1250;}
                else if (d<2500) {lo=1250;hi=2700;}
                else if (d<11250) {lo=2500;hi=11250;}
                else {lo=11250;hi=20000;}
            } else {
                if (d<2500) {lo=60;hi=2500;}
                else if (d<8000) {lo=2500;hi=8000;}
                else {lo=8000;hi=10000000;}
            }
            a=original_fog(mode,lo,(int)v->ti_fog[1]); b=original_fog(mode,hi,(int)v->ti_fog[1]);
            yl=1-sqrtf(2.0f)*1000/lo; yh=1-sqrtf(2.0f)*1000/hi;
            t=(sy-yl)/(yh-yl); want=(int)lroundf(a+(b-a)*t);
            got=read_ground(r,v,160,sample_y[i]);
            if (abs(got-want)>2) fprintf(stderr,"mode%d band%d..%d depth%.2f got%d expected%d\n",mode,lo,hi,(double)d,got,want);
            check(abs(got-want)<=2,"GPU original ground bands interpolate fog in screen space");
        }
    }
}

static void dos(glr *r, glr_view *v, mercs_ground *ground)
{
    unsigned char palette[256][3],luma[4096];
    uint32_t p=0xff00002au;
    texture indexed;
    int i,j;
    static const struct {int height,ambient,delta,expected;} cases[]={
        {1000,0,0,10},{1000,40,40,170},{1000,50,70,42},
        {10000,40,40,122},{19000,40,40,42},{1000,-10,0,10}
    };
    memset(&indexed,0,sizeof indexed); indexed.w=indexed.h=1; indexed.rgba=&p;
    for (i=0;i<256;i++) {
        palette[i][0]=palette[i][1]=palette[i][2]=(unsigned char)i;
        for (j=0;j<16;j++) luma[j*256+i]=(unsigned char)(j==15?i:j*16+i%16);
    }
    glr_set_dos(r,palette,luma);
    ground->mode=1; ground->size=320000; ground->repeats[0]=ground->repeats[1]=117.1875f;
    v->look_pitch=-90; v->ti_fog[0]=0; v->ti_light=0;
    for (i=0;i<(int)(sizeof cases/sizeof cases[0]);i++) {
        int got;
        ground->delta=cases[i].delta;
        glr_set_mercs_ground(r,&indexed,ground);
        v->eye[1]=(float)cases[i].height; v->dos_amb=(float)cases[i].ambient;
        got=read_ground(r,v,160,160);
        if (got!=cases[i].expected) fprintf(stderr,"DOS ground case%d got%d expected%d\n",i,got,cases[i].expected);
        check(got==cases[i].expected,"DOS texture uses original ground LUMA shade");
    }
    v->notex_world=1;
    check(read_ground(r,v,160,160)==208,"DOS TERRAIN TEXTURES off retains palette ground");
    v->notex_world=0; v->flat_ground=1;
    check(read_ground(r,v,160,160)==208,"DOS flat ground setting retains palette ground");
    v->flat_ground=0;
    /* Four indices must survive sampling and LUMA independently. */
    {
        uint32_t checker[4]={0xff00002au,0xff00002bu,0xff00002cu,0xff00002du};
        unsigned char first[320*320*4],second[320*320*4];
        int seen[4]={0},different=0;
        indexed.w=indexed.h=2; indexed.rgba=checker;
        ground->size=3200; ground->repeats[0]=ground->repeats[1]=16; ground->delta=40;
        glr_set_mercs_ground(r,&indexed,ground);
        v->eye[0]=1234;v->eye[2]=2345;v->eye[1]=1000;v->dos_amb=40;
        gpu_test_draw(r,v,320,320);gpu_test_read_rgba(0, 0, 320, 320, first);
        v->eye[0]+=200; /* exactly one world-anchored texture period */
        gpu_test_draw(r,v,320,320);gpu_test_read_rgba(0, 0, 320, 320, second);
        for (i=0;i<320*320;i++) {
            int c=first[i*4]-170;
            if (c>=0&&c<4) seen[c]++;
            if (memcmp(first+i*4,second+i*4,4)!=0) different++;
        }
        for (i=0;i<4;i++) check(seen[i]>1000,"DOS indexed checker texels remain distinct");
        check(different<320,"ground UV remains world anchored after camera translation");
    }
    glr_set_mercs_ground(r,NULL,NULL);
    check(read_ground(r,v,160,160)==208,"clearing scene removes previous textured ground");
}

int main(void)
{
    SDL_Window *window; int context; glr *r; glr_view v;
    uint32_t pixel=0xffffffffu; texture white; mercs_ground ground;
    originals(); SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    window=SDL_CreateWindow("Mercs original ground", 320, 320, SDL_WINDOW_HIDDEN);
    context = gpu_test_start();
    if (!context) {fprintf(stderr,"GPU: %s\n",SDL_GetError());return 1;}
    r=glr_create(); if(!r)return 1;
    memset(&white,0,sizeof white);white.w=white.h=1;white.rgba=&pixel;
    memset(&ground,0,sizeof ground);ground.repeats[0]=ground.repeats[1]=117;
    ground.clip[0]=2500;ground.clip[1]=20000;ground.clip[2]=50000;
    glr_set_environment(r,&white,NULL,1,1);
    glr_default_view(&v);v.free_cam=1;v.vfov=90;v.ambient=1;v.bilinear=0;
    v.eye[0]=12345;v.eye[2]=-6789;v.ti_light=1;v.ti_gnd[0]=v.ti_gnd[1]=v.ti_gnd[2]=1;
    v.fog_color[0]=v.fog_color[1]=v.fog_color[2]=0;
    accelerated(r,&v,&white,&ground);dos(r,&v,&ground);
    glr_destroy(r);rb_quit();SDL_DestroyWindow(window);SDL_Quit();
    printf("Mercs ground: %d passed, %d failed\n",passed,failed);
    return failed?1:0;
}
