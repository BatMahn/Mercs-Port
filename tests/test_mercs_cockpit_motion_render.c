#include "sdl3_test.h"
/* A rigid cockpit translated together with its eye must remain fixed on
 * screen. Three synthetic struts provide an image-space oracle independent
 * of the pose bookkeeping. The legacy integer path is a required negative
 * control: it must visibly move as fractional translation crosses a cm.
 * No commercial models or images are embedded. */
#define SDL_MAIN_HANDLED
#include "gpu_test.h"
#include "mercscockpit.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PARTS = 3, FRAMES = 17 };
static unsigned char palette[256][3];
static texture textures[PARTS], *slots[GLR_SLOTS];
static uint32_t texels[PARTS][4] = {
    {0xff0000ffu,0xff0000ffu,0xff0000ffu,0xff0000ffu},
    {0xff00ff00u,0xff00ff00u,0xff00ff00u,0xff00ff00u},
    {0xffff0000u,0xffff0000u,0xffff0000u,0xffff0000u}
};
static wtb_vertex vertices[PARTS][4];
static wtb_poly polygons[PARTS];
static wtb_object objects[PARTS];
static mech3d_part parts[PARTS];
static mech3d model;

static void make_cockpit(void)
{
    static const int rect[PARTS][4] = {{-15,-13,-12,12},{13,15,-12,12},{-13,13,10,12}};
    static const int corner[4][2] = {{0,2},{1,2},{1,3},{0,3}};
    int p, k;
    memset(&model,0,sizeof model);
    model.parts=parts; model.part_count=PARTS; model.skel_count=PARTS;
    for(p=0;p<PARTS;p++) {
        mech3d_part *part=&parts[p];
        model.skel_index[p]=(int16_t)p; model.skel_parent[p]=(int16_t)(p?0:-1);
        for(k=0;k<4;k++) {
            vertices[p][k].x=rect[p][corner[k][0]];
            vertices[p][k].y=rect[p][corner[k][1]];
            vertices[p][k].z=32; vertices[p][k].a=79;
            polygons[p].idx[k]=(uint16_t)k;
        }
        polygons[p].n=4; polygons[p].color=(uint16_t)(0x5010+p);
        polygons[p].has_normal=1; polygons[p].normal[2]=536870912;
        objects[p].verts=vertices[p]; objects[p].vert_count=4;
        objects[p].polys=&polygons[p]; objects[p].poly_count=1;
        part->model.objects=&objects[p]; part->model.object_count=1;
        part->node=p; part->objtype=p?0x100:0x1a0; part->ti=1;
        part->has_rot=1; part->rot[0]=part->rot[4]=part->rot[8]=1;
        textures[p].w=textures[p].h=2; textures[p].rgba=texels[p];
        slots[16+p]=&textures[p];
    }
}

static void set_pose(glr_view *view,int translated,int frame)
{
    static const float local[PARTS][3] = {{-.25f,.125f,0},{.25f,-.125f,0},{0,.375f,0}};
    const float common[3] = {(float)(frame-8)*.125f,(float)(frame-8)*.0625f,0};
    int p,k;
    view->eye[0]=(translated?16800.0f:0)+common[0];
    view->eye[1]=(translated?1000.0f:0)+common[1];
    view->eye[2]=(translated?-15300.0f:0)+common[2];
    for(p=0;p<PARTS;p++)for(k=0;k<3;k++) {
        parts[p].pose_pos[k]=view->eye[k]+local[p][k];
        parts[p].pos[k]=(int32_t)lrintf(parts[p].pose_pos[k]);
        parts[p].pose_pos_valid=1; parts[p].render_pose_pos=0;
    }
}

static size_t differences(const unsigned char *a,const unsigned char *b,size_t pixels)
{
    size_t i,total=0;
    for(i=0;i<pixels;i++) if(memcmp(a+4*i,b+4*i,3))total++;
    return total;
}

int main(int argc,char **argv)
{
    static const int sizes[2][2]={{640,480},{1920,1080}};
    const size_t maximum=(size_t)1920*1080*4;
    unsigned char *reference=malloc(maximum),*actual=malloc(maximum);
    mercs_cockpit_layer layer={0};
    glr_view view;
    glr *renderer;
    char error[512];
    int samples=argc>1?atoi(argv[1]):1,size,translated,mode,frame,failures=0,cases=0;
    size_t total_pixels=0;
    SDL_SetMainReady();
    if(!reference||!actual||!SDL_Init(SDL_INIT_VIDEO)||
       rb_init(NULL,RB_GPU_AUTO,samples,error,sizeof error)!=0) {
        fprintf(stderr,"cockpit motion GPU initialization failed: %s\n",SDL_GetError());
        free(reference);free(actual);SDL_Quit();return 2;
    }
    rb_frame(1,1); renderer=glr_create();
    if(!renderer){free(reference);free(actual);rb_quit();SDL_Quit();return 2;}
    make_cockpit(); glr_set_titanium(renderer,slots,NULL);
    glr_default_view(&view);view.free_cam=1;view.vfov=90;view.ambient=1;
    for(size=0;size<2;size++)for(translated=0;translated<2;translated++) {
        size_t changed[2]={0,0},max_changed[2]={0,0};
        const int w=sizes[size][0],h=sizes[size][1];
        const size_t pixels=(size_t)w*(size_t)h;
        for(mode=0;mode<2;mode++)for(frame=0;frame<FRAMES;frame++) {
            size_t delta;
            set_pose(&view,translated,frame);
            if(mercs_cockpit_update_render(&layer,&model,view.eye,mode)!=0 ||
               layer.model.part_count!=PARTS || layer.pit_parts!=PARTS ||
               glr_set_actors(renderer,&layer.model,palette,slots,0,0)!=0) {
                fprintf(stderr,"cockpit motion upload failed\n");failures++;continue;
            }
            gpu_test_draw(renderer,&view,w,h);
            gpu_test_read_rgba(0,0,w,h,actual);
            if(frame==0) {
                size_t visible=0,i;
                memcpy(reference,actual,pixels*4);
                for(i=0;i<pixels;i++) visible+=actual[i*4]>200||actual[i*4+1]>200||actual[i*4+2]>200;
                if(visible<pixels/100 || visible>pixels/2) {
                    fprintf(stderr,"invalid cockpit oracle coverage: %zu/%zu\n",visible,pixels);failures++;
                }
            }
            delta=differences(reference,actual,pixels);
            changed[mode]+=delta;if(delta>max_changed[mode])max_changed[mode]=delta;
            total_pixels+=pixels;
        }
        cases++;
        if(!gpu_test_ok() || !changed[0] || changed[1])failures++;
        printf("CASE {\"samples\":%d,\"width\":%d,\"height\":%d,\"translated\":%d,"
               "\"frames_per_mode\":%d,\"legacy_changed_pixels\":%zu,\"legacy_max_changed_pixels\":%zu,"
               "\"corrected_changed_pixels\":%zu,\"corrected_max_changed_pixels\":%zu}\n",
               samples,w,h,translated,FRAMES,changed[0],max_changed[0],changed[1],max_changed[1]);
    }
    printf("RESULT {\"samples\":%d,\"cases\":%d,\"frames\":%d,\"pixels_compared\":%zu,\"failures\":%d}\n",
           samples,cases,cases*2*FRAMES,total_pixels,failures);
    mercs_cockpit_free(&layer);glr_destroy(renderer);rb_quit();SDL_Quit();
    free(reference);free(actual);return failures?1:0;
}
