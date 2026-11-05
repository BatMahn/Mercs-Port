#include "sdl3_test.h"
/* Satellite material/filter rendering and restoration. Original arithmetic
 * fixtures are tested separately by test_mercs_map. Synthetic geometry only. */
#define SDL_MAIN_HANDLED
#include "gpu_test.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include "glr.h"

int main(void)
{
    SDL_Window *window;int context;glr *r;glr_view v;
    unsigned char pal[256][3]={{0}},image[128*128*4],before[128*128*4];
    texture *slots[GLR_SLOTS]={0};wtb_vertex vertices[4]={{0}};
    wtb_poly polygon={0};wtb_object object={0};mech3d_part part={0};mech3d model={0};
    int profile,k,c,failed=0,passed=0;
    const struct {int height,type,side,flags,color;} cases[]={
        {2000,0x800,0,0,246},{6400,0x800,0,0,255},
        {2000,0x30,0,0,0},{2000,0x70,0,0,0},
        {2000,0x100,0,0,14},{2000,0x100,1,0,10},{2000,0x100,2,0,6},
        {2000,0x100,0,0x10,0},{2000,0x100,1,2,0},
        {2000,0x200,1,0,11},{2000,0x400,0,0,2}};
    SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO))return 2;
    window=SDL_CreateWindow("Mercs satellite checks", 128, 128, SDL_WINDOW_HIDDEN);
    context = gpu_test_start();if(!context)return 2;
    r=glr_create();if(!r)return 2;
    for(k=1;k<256;k++)pal[k][0]=pal[k][1]=pal[k][2]=(unsigned char)k;
    for(k=0;k<4;k++){
        vertices[k].x=(k==0||k==3)?-8000:8000;
        vertices[k].z=k<2?-8000:8000;vertices[k].a=79;polygon.idx[k]=(uint16_t)k;
    }
    polygon.n=4;polygon.color=0x4f00;object.verts=vertices;object.vert_count=4;
    object.polys=&polygon;object.poly_count=1;part.model.objects=&object;part.model.object_count=1;
    model.parts=&part;model.part_count=1;
    for(profile=0;profile<2;profile++){
        glr_set_dos(r,profile?NULL:pal,NULL);glr_default_view(&v);
        v.free_cam=1;v.ortho_w=32000;v.look_pitch=-90;v.eye[1]=100000;
        v.dos_lpos[0]=v.dos_lpos[1]=v.dos_lpos[2]=999999;
        v.dos_amb=v.dos_intensity=128;v.dos_sky_index=v.dos_ground_index=0;
        v.map_low=-1000;v.map_high=6400;
        for(k=0;k<4;k++)vertices[k].y=2000;
        part.objtype=0x800;part.map_flags=part.map_alliance=0;
        if(glr_set_mech(r,&model,pal,slots,0,0))return 2;
        gpu_test_draw(r,&v,128,128);gpu_test_read_rgba(0, 0, 128, 128, before);
        v.satellite=1;
        for(c=0;c<(int)(sizeof cases/sizeof cases[0]);c++){
            for(k=0;k<4;k++)vertices[k].y=cases[c].height;
            part.objtype=cases[c].type;part.map_alliance=(unsigned)cases[c].side;part.map_flags=(unsigned)cases[c].flags;
            if(glr_set_mech(r,&model,pal,slots,0,0))return 2;
            gpu_test_draw(r,&v,128,128);gpu_test_read_rgba(0, 0, 128, 128, image);
            if(gpu_test_ok()&&image[(64*128+64)*4]==cases[c].color)passed++;
            else {fprintf(stderr,"FAIL map profile%d case%d got%d wanted%d\n",profile,c,image[(64*128+64)*4],cases[c].color);failed++;}
        }
        for(k=0;k<4;k++)vertices[k].y=2000;
        part.objtype=0x800;part.map_flags=part.map_alliance=0;v.satellite=0;
        if(glr_set_mech(r,&model,pal,slots,0,0))return 2;
        gpu_test_draw(r,&v,128,128);gpu_test_read_rgba(0, 0, 128, 128, image);
        if(!memcmp(before,image,sizeof image))passed++;else{fputs("FAIL map state restoration\n",stderr);failed++;}
    }
    glr_destroy(r);rb_quit();SDL_DestroyWindow(window);SDL_Quit();
    printf("Mercs satellite GPU: %d passed, %d failed\n",passed,failed);return failed?1:0;
}
