#include "sdl3_test.h"
/* GPU regression for DOS 0x12df0/0x131bc's constant-divisor map
 * projection. Synthetic geometry; no original assets embedded. */
#define SDL_MAIN_HANDLED
#include "gpu_test.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glr.h"

/* The main DOS target/nav marks are projected after all cockpit displays.
 * Rendering those displays must not replace their camera with the final
 * little rear/satellite view. Exercise the actual GPU pass and the CPU HUD
 * projector together, including the full pitched fall-camera basis. */
static int auxiliary_projection_checks(glr *r, const glr_view *initial,
                                       int *passed, int *failed)
{
    static const float marks[5][3]={{0,0,12000},{1700,0,12000},
        {-900,1600,16000},{30000,-700,2000},{0,0,-12000}};
    int mode, aux, i, k;
    for(mode=0;mode<3;mode++) {
        glr_view main=*initial;
        float right[3]={1,0,0},up[3]={0,1,0},forward[3]={0,0,1};
        float points[5][3];int expected[5][4],vw,vh;uint32_t zoom;
        main.ortho_w=0;main.look_rot_ok=0;main.look_yaw=main.look_pitch=0;
        main.eye[0]=23000;main.eye[1]=4000;main.eye[2]=-45000;
        if(mode==1) {
            const float s=sqrtf(0.5f),t=sqrtf(0.375f);
            right[0]=s;right[2]=-s;
            up[0]=-t;up[1]=0.5f;up[2]=-t;
            forward[0]=s*0.5f;forward[1]=sqrtf(0.75f);forward[2]=s*0.5f;
            for(k=0;k<3;k++) {
                main.look_rot[k*3]=right[k];main.look_rot[k*3+1]=up[k];
                main.look_rot[k*3+2]=forward[k];
            }
            main.look_rot_ok=1;
        } else if(mode==2) {
            main.ortho_w=128000;main.look_pitch=-90;
            up[1]=0;up[2]=1;forward[1]=-1;forward[2]=0;
        }
        for(i=0;i<5;i++)for(k=0;k<3;k++) {
            points[i][k]=main.eye[k]+right[k]*marks[i][0]+up[k]*marks[i][1]+forward[k]*marks[i][2];
            if(k==2)points[i][k]=-points[i][k];
        }
        gpu_test_draw(r,&main,320,240);
        for(i=0;i<5;i++)expected[i][0]=glr_dos_project(r,points[i],
                &expected[i][1],&expected[i][2],&expected[i][3]);
        glr_dos_view(r,&zoom,&vw,&vh);
        if(expected[0][0]==1&&abs(expected[0][1]-160)<=1&&abs(expected[0][2]-119)<=1&&vw==320&&vh==240) (*passed)++;
        else {fprintf(stderr,"FAIL main HUD projection mode%d\n",mode);(*failed)++;}
        for(aux=0;aux<3;aux++) {
            glr_view small=main;int ok=1,w,h;uint32_t zhw;
            small.look_rot_ok=0;small.ortho_w=0;small.look_pitch=0;
            small.eye[0]=-9000;small.eye[1]=8000;small.eye[2]=13000;
            small.look_yaw=aux==0?180:90;
            if(aux==2) {
                small.ortho_w=100000;small.look_yaw=0;small.look_pitch=-90;
                small.eye[1]=100000;small.satellite=1;
            }
            glr_draw_rect(r,&small,7+aux*17,11+aux*13,96+aux*16,72+aux*16);
            for(i=0;i<5;i++) {
                int got[4];got[0]=glr_dos_project(r,points[i],&got[1],&got[2],&got[3]);
                if(memcmp(got,expected[i],sizeof got))ok=0;
            }
            glr_dos_view(r,&zhw,&w,&h);
            ok=ok&&zhw==zoom&&w==vw&&h==vh&&gpu_test_ok();
            if(ok)(*passed)++;
            else {fprintf(stderr,"FAIL auxiliary view%d replaced main HUD camera%d\n",aux,mode);(*failed)++;}
        }
    }
    return *failed?1:0;
}

static void sky_rotation_checks(glr *r, const unsigned char pal[256][3],
                                int *passed, int *failed)
{
    enum { SIDE=160 };
    unsigned char reference[SIDE*SIDE*4],actual[SIDE*SIDE*4];
    uint32_t texels[16*16];texture sky={0};texture *sky_slots[GLR_SLOTS]={0};
    mech3d empty={0};wtb_vertex verts[4]={{0}};wtb_poly poly={0};wtb_object dome={0};
    int type,turn,x,y,k;
    for(y=0;y<16;y++)for(x=0;x<16;x++)
        texels[y*16+x]=0xff000000u|(uint32_t)(64+x*10)|((uint32_t)(32+y*12)<<8)|((uint32_t)((x+y)*7)<<16);
    sky.w=sky.h=16;sky.rgba=texels;sky_slots[0]=&sky;
    for(k=0;k<4;k++) {
        verts[k].x=(k==0||k==3)?-150000:150000;
        verts[k].y=k<2?-150000:150000;verts[k].z=50000;
        verts[k].a=(k==0||k==3)?0:15;verts[k].b=k<2?0:15;poly.idx[k]=(uint16_t)k;
    }
    poly.n=4;dome.verts=verts;dome.vert_count=4;dome.polys=&poly;dome.poly_count=1;
    glr_set_mech(r,&empty,pal,sky_slots,0,0);glr_set_actors(r,&empty,pal,sky_slots,0,0);
    for(type=0;type<3;type++) {
        glr_view v;float right[3]={1,0,0},up[3],forward[3];int varied=0;
        float pitch=type==1?65.0f:type==2?20.0f:0.0f;
        float radians=pitch*3.14159265f/180.0f;
        glr_set_dos(r,type?NULL:pal,NULL);glr_set_environment(r,NULL,type==1?&sky:NULL,8,1);
        glr_set_skydome(r,type==2?&dome:NULL,sky_slots);
        glr_default_view(&v);v.free_cam=1;v.look_rot_ok=1;v.look_pitch=pitch;v.vfov=60;
        v.dos_sky_index=96;v.dos_ground_index=208;v.dos_band=0;
        v.sky_color[0]=v.sky_color[1]=v.sky_color[2]=0;
        up[0]=0;up[1]=cosf(radians);up[2]=-sinf(radians);
        forward[0]=0;forward[1]=sinf(radians);forward[2]=cosf(radians);
        for(turn=0;turn<3;turn++) {
            for(k=0;k<3;k++) {
                v.look_rot[k*3]=turn==0?right[k]:turn==1?up[k]:-right[k];
                v.look_rot[k*3+1]=turn==0?up[k]:turn==1?-right[k]:-up[k];
                v.look_rot[k*3+2]=forward[k];
            }
            gpu_test_draw(r,&v,SIDE,SIDE);gpu_test_read_rgba(0, 0, SIDE, SIDE, actual);
            if(!turn) {
                memcpy(reference,actual,sizeof reference);
                for(k=0;k<SIDE*SIDE;k++)if(memcmp(reference+k*4,reference,3))varied++;
            } else {
                int bad=0;
                for(y=0;y<SIDE;y++)for(x=0;x<SIDE;x++) {
                    int sx=SIDE-1-(turn==1?y:x),sy=turn==1?x:SIDE-1-y;
                    for(k=0;k<3;k++)if(abs(actual[(y*SIDE+x)*4+k]-reference[(sy*SIDE+sx)*4+k])>2) {bad++;break;}
                }
                if(!bad&&varied>SIDE&&gpu_test_ok())(*passed)++;
                else {fprintf(stderr,"FAIL sky type%d turn%d rotation: %d mismatched, %d varying pixels\n",type,turn,bad,varied);(*failed)++;}
            }
        }
    }
    glr_set_environment(r,NULL,NULL,1,1);glr_set_skydome(r,NULL,sky_slots);
}

int main(void)
{
    SDL_Window *window; int context; glr *r; glr_view v;
    unsigned char pal[256][3]={{0}}, base[320*240*4], image[320*240*4];
    texture *slots[GLR_SLOTS]={0}; wtb_vertex vertices[4]={{0}};
    wtb_poly polygon={0}; wtb_object object={0}; mech3d_part part={0}; mech3d model={0};
    int k, size, depth, fog, passed=0, failed=0;
    SDL_SetMainReady(); if(!SDL_Init(SDL_INIT_VIDEO)) return 2;
    window=SDL_CreateWindow("Mercs orthographic regression", 320, 240, SDL_WINDOW_HIDDEN);
    context = gpu_test_start(); if(!context)return 2;
    r=glr_create(); if(!r)return 2;
    for(k=1;k<256;k++) pal[k][0]=pal[k][1]=pal[k][2]=(unsigned char)k;
    glr_set_dos(r,pal,NULL); glr_default_view(&v);
    v.free_cam=1; v.ortho_w=128000; v.dos_amb=128; v.dos_intensity=128;
    v.dos_lpos[0]=v.dos_lpos[1]=v.dos_lpos[2]=999999;
    v.dos_ground_index=v.dos_sky_index=0; v.dos_band=0;
    for(k=0;k<4;k++) {
        vertices[k].x=(k==0||k==3)?-16000:16000;
        vertices[k].y=k<2?-16000:16000; vertices[k].a=79;
        polygon.idx[k]=(uint16_t)k;
    }
    polygon.n=4; polygon.color=0x44ff;
    object.verts=vertices; object.vert_count=4; object.polys=&polygon; object.poly_count=1;
    part.model.objects=&object; part.model.object_count=1; part.objtype=0x800;
    model.parts=&part; model.part_count=1;
    for(size=0;size<2;size++)for(depth=0;depth<2;depth++)for(fog=0;fog<2;fog++) {
        int w=size?320:128,h=size?240:128,count=0,x,y,z,ok;
        for(k=0;k<4;k++)vertices[k].z=depth?500000:2000;
        v.dos_fogdist=fog?4000:0;
        ok=glr_set_mech(r,&model,pal,slots,0,0)==0;
        gpu_test_draw(r,&v,w,h); gpu_test_read_rgba(0, 0, w, h, image);
        for(k=0;k<w*h;k++)if(image[k*4]>0)count++;
        if(!depth&&!fog)memcpy(base,image,(size_t)w*h*4);
        ok=ok&&gpu_test_ok()&&count>500&&count<10000&&
           !memcmp(base,image,(size_t)w*h*4);
        {float p[3]={8000,0,(float)-vertices[0].z};
         ok=ok&&glr_dos_project(r,p,&x,&y,&z)==1&&x==w/2+8000/(128000/w)&&y==h/2-1;}
        if(ok)passed++;else{fprintf(stderr,"FAIL size%d depth%d fog%d count%d\n",size,depth,fog,count);failed++;}
    }
    /* Restore perspective in the same object: a map must not contaminate
     * the next normal HUD projection with its constant divisor. */
    v.ortho_w=0; gpu_test_draw(r,&v,128,128);
    {float p[3]={8000,0,-500000};int x,y,z; if(glr_dos_project(r,p,&x,&y,&z)==1&&x<70)passed++;else failed++;}
    auxiliary_projection_checks(r,&v,&passed,&failed);
    sky_rotation_checks(r,pal,&passed,&failed);
    glr_destroy(r);rb_quit();SDL_DestroyWindow(window);SDL_Quit();
    printf("Mercs orthographic GPU: %d passed, %d failed\n",passed,failed);return failed?1:0;
}
