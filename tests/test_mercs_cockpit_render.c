#include "sdl3_test.h"
/* Exercise the actual GPU shaders with synthetic geometry and original
 * DOS shade-consumer goldens. No commercial game assets are embedded. */
#define SDL_MAIN_HANDLED
#include "gpu_test.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glr.h"
#include "mercscockpit.h"
#include "skygnd.h"
#include "bwd.h"
#include "bwdplace.h"
#include "mercs_cockpit_shading_fixtures.h"

static unsigned char palette[256][3];
static texture *slots[GLR_SLOTS];
static wtb_vertex vertices[4];
static wtb_poly polygon;
static wtb_object object;
static mech3d_part part;
static mech3d model;

static void quad(int depth, int half_size, unsigned color, int cockpit)
{
    int k;
    const int xy[4][2] = {{-1,-1},{1,-1},{1,1},{-1,1}};
    memset(vertices, 0, sizeof vertices);
    memset(&polygon, 0, sizeof polygon);
    memset(&object, 0, sizeof object);
    memset(&part, 0, sizeof part);
    memset(&model, 0, sizeof model);
    for (k = 0; k < 4; k++) {
        vertices[k].x = xy[k][0] * half_size;
        vertices[k].y = xy[k][1] * half_size;
        vertices[k].z = depth;
        vertices[k].a = 79;
        polygon.idx[k] = (uint16_t)k;
    }
    polygon.n = 4; polygon.color = (uint16_t)color;
    object.verts = vertices; object.vert_count = 4;
    object.polys = &polygon; object.poly_count = 1;
    part.model.objects = &object; part.model.object_count = 1;
    part.dos_pit = cockpit; part.objtype = cockpit ? 0x1a0 : 0x100;
    model.parts = &part; model.part_count = 1;
}

static int sample(glr *renderer, glr_view *view, unsigned char out[4])
{
    if (glr_set_actors(renderer, &model, palette, slots, 0, 0) < 0) return -1;
    gpu_test_draw(renderer, view, 128, 128);
    gpu_test_read_rgba(64, 64, 1, 1, out);
    return gpu_test_ok() ? 0 : -1;
}

static void capacity_fixtures(glr *renderer, glr_view *view, int *passed, int *failures)
{
    enum { MANY = 320 };
    wtb_vertex *many_vertices = calloc(MANY * 4, sizeof *many_vertices);
    wtb_poly *many_polygons = calloc(MANY, sizeof *many_polygons);
    unsigned char *baseline = malloc(128 * 128 * 4), *actual = malloc(128 * 128 * 4);
    unsigned char pixel[4];
    int mode, k, v;
    if (!many_vertices || !many_polygons || !baseline || !actual) {
        fprintf(stderr, "cockpit capacity fixture allocation failed\n"); (*failures)++; goto done;
    }
    /* The final face is nearest and completely covers the preceding faces.
     * Inserting 319 occluded polygons must leave its actual GPU output intact.
     * This reaches beyond both the old128 rank cap and the float edge-bit
     * precision boundary at id256. The Gouraud case also reads its fan's
     * edge metadata from the vertex buffer. This is an invariance check,
     * not a claim of whole-frame parity with the original game. */
    for (mode = 0; mode < 2; mode++) {
        const unsigned color = mode ? 0x44ffu : 0x0470u;
        quad(9, 12, color, 1);
        if (sample(renderer, view, pixel) != 0) { (*failures)++; continue; }
        gpu_test_read_rgba(0, 0, 128, 128, baseline);
        for (k = 0; k < MANY; k++) {
            many_polygons[k] = polygon;
            if (k != MANY - 1) many_polygons[k].color = 0x0430;
            for (v = 0; v < 4; v++) {
                many_vertices[k * 4 + v] = vertices[v];
                if (k != MANY - 1) many_vertices[k * 4 + v].z = 20;
                many_polygons[k].idx[v] = (uint16_t)(k * 4 + v);
            }
        }
        object.verts = many_vertices; object.vert_count = MANY * 4;
        object.polys = many_polygons; object.poly_count = MANY;
        if (sample(renderer, view, pixel) != 0) { (*failures)++; continue; }
        gpu_test_read_rgba(0, 0, 128, 128, actual);
        if (!gpu_test_ok() || memcmp(baseline, actual, 128 * 128 * 4)) {
            fprintf(stderr, "320-polygon cockpit %s changed nearest face or edge metadata\n", mode ? "Gouraud" : "flat");
            (*failures)++;
        } else (*passed)++;
    }
    {
        wtb_object objects[2];
        wtb_vertex alternate_vertices[4];
        wtb_poly alternate_polygon;
        /* Flag-0x1000 WTBO records are alternative distance levels, not
         * simultaneous surfaces (Mercs DOS 0x6304c/0x45c04/0x4b6b0).
         * Keep the current first-level selection: a closer, differently
         * coloured alternative must not cover the selected geometry. */
        quad(200, 80, 0x0470, 0);
        objects[0] = objects[1] = object;
        objects[0].flags = objects[1].flags = 0x1002;
        strcpy(objects[0].name, "surface_1");
        strcpy(objects[1].name, "surface_35000");
        memcpy(alternate_vertices, vertices, sizeof alternate_vertices);
        for (v = 0; v < 4; v++) alternate_vertices[v].z = 100;
        alternate_polygon = polygon; alternate_polygon.color = 0x04b0;
        objects[1].verts = alternate_vertices;
        objects[1].polys = &alternate_polygon;
        part.model.objects = objects; part.model.object_count = 2;
        view->far_cull = 500;
        if (sample(renderer, view, pixel) != 0 || pixel[0] != 71) {
            fprintf(stderr, "alternate WTBO level overlaid the selected surface: got%u expected71\n", pixel[0]);
            (*failures)++;
        } else (*passed)++;
        view->far_cull = 0;
    }
done:
    free(many_vertices); free(many_polygons); free(baseline); free(actual);
    quad(20, 6, 0x0460, 1);
}

static int save_cockpit_frame(const char *directory,const char *name,int dos,int pose,
                              const unsigned char *rgba,int w,int h)
{
    char path[4096];FILE *f;int x,y,n;
    if(!directory)return 0;
    n=snprintf(path,sizeof path,"%s/%s-%s-%s.ppm",directory,name,dos?"dos":"accelerated",
               pose==0?"upright":pose==1?"fallen":"missing-roll-control");
    if(n<0||(size_t)n>=sizeof path||(f=fopen(path,"wb"))==NULL)return -1;
    fprintf(f,"P6\n%d %d\n255\n",w,h);
    for(y=h-1;y>=0;y--)for(x=0;x<w;x++)if(fwrite(rgba+(y*w+x)*4,1,3,f)!=3){fclose(f);return -1;}
    return fclose(f);
}

static int cockpit_pixel(const unsigned char *p)
{ return p[0]!=255||p[1]!=0||p[2]!=255; }

/* Original Mercs hardware suppresses a kind5/6/7 whose bitmap lookup
 * fails. It still supports the object's texture-off shade, wireframe and
 * satellite modes. Exercise those independent paths in the real shaders:
 * the training road's absent0xe6 must not become an opaque palette face. */
static void missing_bitmap_checks(glr *renderer,int *passed,int *failures)
{
    static const unsigned classes[10]={0x400,0x400,0x400,0x100,0x100,0x800,0x800,0x400,0x400,0x800};
    static const unsigned masks[10]={0,0,0,1,2,2,1,3,0,0};
    static const char *labels[10]={"missing","empty image","assigned bitmap","object off","wrong object switch",
        "terrain off","wrong terrain switch","unclassified off","wire","satellite"};
    uint32_t texels[4]={0xff3bc911u,0xff3bc911u,0xff3bc911u,0xff3bc911u};
    texture valid={0},empty={0};texture *hardware_slots[GLR_SLOTS]={0};
    glr_ti_shade shade={0};mech3d none={0};int backend,kind,test,k;
    unsigned char frame[128*128*4],pixel[4];int depth_clear;
    valid.w=valid.h=empty.w=empty.h=2;valid.rgba=texels;
    shade.on=1;shade.amb=128;shade.gi=0;
    glr_set_dos(renderer,NULL,NULL);glr_set_environment(renderer,NULL,NULL,1,1);
    glr_set_skydome(renderer,NULL,slots);glr_set_mercs_ground(renderer,NULL,NULL);
    glr_set_actors(renderer,&none,palette,slots,0,0);
    glr_set_titanium(renderer,hardware_slots,NULL);
    glr_set_titanium_extra(renderer,NULL,&shade);
    for(backend=1;backend<=2;backend++)for(kind=5;kind<=7;kind++) {
        for(test=0;test<10;test++) {
            glr_view v;int ok,expected=0;
            quad(1000,500,(unsigned)(kind*0x1000+0xde6),0);
            part.ti=1;part.objtype=(int)classes[test];
            hardware_slots[0xe6]=test==1?&empty:test==2?&valid:NULL;
            glr_default_view(&v);v.free_cam=1;v.vfov=73.739795f;v.ambient=1;
            v.ti_fog_vtx=backend;v.ti_light=1;
            v.ti_amb[0]=v.ti_amb[1]=v.ti_amb[2]=1;
            v.sky_color[0]=v.sky_color[1]=v.sky_color[2]=0;
            v.notex_actors=(masks[test]&1)!=0;v.notex_world=(masks[test]&2)!=0;
            v.wire=test==8;v.satellite=test==9;v.map_low=0;v.map_high=1000;
            ok=glr_set_mech(renderer,&model,palette,slots,0,0)==0;
            gpu_test_draw(renderer,&v,128,128);
            gpu_test_read_rgba(64, 64, 1, 1, pixel);
            depth_clear = 0;
            if(test==2)ok=ok&&pixel[0]==17&&pixel[1]==201&&pixel[2]==59;
            else if(test==8) {
                int visible=0;gpu_test_read_rgba(0, 0, 128, 128, frame);
                for(k=0;k<128*128;k++)visible+=frame[k*4]!=0||frame[k*4+1]!=0||frame[k*4+2]!=0;
                ok=ok&&visible>100;
            } else {
                /* Map corner heights clamp to0/8; the centre interpolates4. */
                if(test==3)expected=217;else if(test==5)expected=220;else if(test==9)expected=212;
                ok=ok&&pixel[0]==expected&&pixel[1]==expected&&pixel[2]==expected;
                if(!expected){depth_clear=gpu_test_depth_clear(64,64);ok=ok&&depth_clear;} /* a failed bitmap must not occlude later geometry */
            }
            ok=ok&&gpu_test_ok();
            if(ok)(*passed)++;
            else {fprintf(stderr,"FAIL hardware%d kind%d %s: rgb%u,%u,%u depth-clear%d\n",
                    backend,kind,labels[test],pixel[0],pixel[1],pixel[2],depth_clear);(*failures)++;}
        }
    }
    /* Legitimate untextured material kinds remain visible without any
     * bitmap assignment. Their palette indices are not texture slots. */
    for(backend=1;backend<=2;backend++)for(kind=0;kind<3;kind++) {
        static const unsigned colors[3]={0x0d70,0x1de6,0x4dff};
        static const unsigned expected[3]={215,221,79};glr_view v;int ok;
        quad(1000,500,colors[kind],0);part.ti=1;part.objtype=0x400;
        hardware_slots[0xe6]=NULL;glr_default_view(&v);v.free_cam=1;
        v.ti_fog_vtx=backend;v.ti_light=1;v.ambient=1;
        v.sky_color[0]=v.sky_color[1]=v.sky_color[2]=0;
        ok=glr_set_mech(renderer,&model,palette,slots,0,0)==0;
        gpu_test_draw(renderer,&v,128,128);gpu_test_read_rgba(64, 64, 1, 1, pixel);
        ok=ok&&pixel[0]==expected[kind]&&pixel[1]==expected[kind]&&pixel[2]==expected[kind]&&gpu_test_ok();
        if(ok)(*passed)++;
        else {fprintf(stderr,"FAIL hardware%d legitimate palette%04x: rgb%u,%u,%u\n",
                backend,colors[kind],pixel[0],pixel[1],pixel[2]);(*failures)++;}
    }
    glr_set_titanium(renderer,NULL,NULL);
    glr_set_mech(renderer,&none,palette,slots,0,0);
}

/* Actual archived reduced representations, attached to their full original
 * skeletons and filtered by the production cockpit layer. The grayscale
 * palette isolates geometry: these are camera/attachment checks, not an
 * edition texture or original-framebuffer comparison. The negative control
 * deliberately drops the full fall basis, reproducing the lost-roll fault. */
static void archive_cockpit_falls(glr *renderer,const char *path,const char *captures,
                                  int *passed,int *failures)
{
    enum { W=640,H=480 };
    static const char *names[]={"COMMANDO","KODIAK","MADDOG"};
    unsigned char *images[3]={NULL,NULL,NULL};
    char error[256];prj_archive *archive=prj_open(path,error,sizeof error);
    int dos,n,k,pose,old_hips=mech3d_dos_hips;
    if(!archive){fprintf(stderr,"archive cockpit fixture: %s\n",error);(*failures)++;return;}
    for(k=0;k<3;k++)if(!(images[k]=malloc(W*H*4))){(*failures)++;goto done;}
    palette[255][0]=255;palette[255][1]=0;palette[255][2]=255;
    for(dos=0;dos<2;dos++)for(n=0;n<3;n++) {
        mech3d full={0},pit={0};mercs_cockpit_layer layer={0};glr_view view;
        int ok=1,visible[2]={0},correct_difference=0,missing_roll_difference=0;
        if(mech3d_load(archive,names[n],0,&full)||mech3d_load(archive,names[n],4,&pit)) {
            fprintf(stderr,"cannot load original cockpit %s\n",names[n]);(*failures)++;goto release;
        }
        mech3d_use_skeleton(&pit,&full);mech3d_dos_hips=dos;
        glr_set_dos(renderer,dos?palette:NULL,NULL);
        glr_set_environment(renderer,NULL,NULL,1,1);glr_set_mercs_ground(renderer,NULL,NULL);
        glr_default_view(&view);view.free_cam=1;view.look_yaw=112;view.look_pitch=12;
        view.vfov=73.739795f;view.ambient=1;view.dos_amb=128;view.dos_intensity=0;
        view.dos_lpos[0]=view.dos_lpos[1]=view.dos_lpos[2]=999999;
        view.dos_sky_index=view.dos_ground_index=255;view.dos_band=0;
        view.sky_color[0]=view.sky_color[2]=1;view.sky_color[1]=0;
        for(pose=0;pose<3;pose++) {
            int32_t center,delta[3];
            full.heading=pit.heading=27;full.twist=pit.twist=20;
            full.fall_active=pit.fall_active=1;
            full.fall_pitch=pit.fall_pitch=pose?60*65536:0;
            mech3d_fall_geometry(0,full.fall_pitch,full.centre_h,27*65536,&center,delta);
            full.fall_center=pit.fall_center=center;
            full.origin[0]=pit.origin[0]=1000+delta[0];full.origin[2]=pit.origin[2]=3000+delta[2];
            mech3d_pose(&full,NULL,0);mech3d_pose(&pit,NULL,0);
            ok=ok&&pit.eye_ok&&!memcmp(pit.eye_pos,full.eye_pos,sizeof pit.eye_pos);
            memcpy(view.eye,pit.eye_pos,sizeof view.eye);
            mech3d_fall_camera(&pit,112,12,view.look_rot);view.look_rot_ok=pose!=2;
            if(mercs_cockpit_update(&layer,&pit,pit.eye_pos)||
               glr_set_actors(renderer,&layer.model,palette,slots,0,0)) {ok=0;break;}
            gpu_test_draw(renderer,&view,W,H);gpu_test_read_rgba(0, 0, W, H, images[pose]);
            ok=ok&&gpu_test_ok();
            if(save_cockpit_frame(captures,names[n],dos,pose,images[pose],W,H))ok=0;
        }
        if(ok)for(k=0;k<W*H;k++) {
            int a=cockpit_pixel(images[0]+k*4),b=cockpit_pixel(images[1]+k*4),c=cockpit_pixel(images[2]+k*4);
            visible[0]+=a;visible[1]+=b;correct_difference+=a!=b;missing_roll_difference+=a!=c;
        }
        /* Integer node rounding can shift silhouettes after rigid rotation;
         * the executed-original placement fixtures bound that separately.
         * A complete fall basis must preserve the cockpit far better than
         * the direction-only negative control that loses its roll. */
        ok=ok&&visible[0]>500&&visible[1]>500&&missing_roll_difference>W*H/100&&
           2*correct_difference<missing_roll_difference;
        printf("Archive cockpit %s %s: visible %d/%d, fall edge changes %d, missing-roll control %d\n",
               names[n],dos?"DOS":"accelerated",visible[0],visible[1],correct_difference,missing_roll_difference);
        if(ok)(*passed)++;
        else {fprintf(stderr,"FAIL archived fallen cockpit %s mode%d\n",names[n],dos);(*failures)++;}
release:
        mercs_cockpit_free(&layer);mech3d_free(&pit);mech3d_free(&full);
    }
done:
    for(k=0;k<3;k++)free(images[k]);
    mech3d_dos_hips=old_hips;prj_close(archive);
    palette[255][0]=palette[255][1]=palette[255][2]=255;
}

/* Exercise every supplied cockpit through the real draw path in an aim,
 * twist, glance and fallen-view matrix. Translating the complete scene must
 * preserve the cockpit image; this catches parts/camera paths that omit a
 * placement component. This is native geometry coverage, not original GPU
 * image parity. The original controller math has a separate executable oracle. */
static void archive_all_views(glr *renderer,const char *path,int *passed,int *failures,int extreme)
{
    enum { W=320,H=240 };
    static const float ordinary_pose[][4]={{0,0,0,0},{35,-45,70,0},{-35,45,-70,0},{20,30,70,45}};
    static const float extreme_pose[][4]={{0,0,0,90},{0,0,0,-90},{40,35,0,70},{-40,-35,0,-70}};
    const float (*pose)[4]=extreme?extreme_pose:ordinary_pose;
    char error[256];prj_archive *a=prj_open(path,error,sizeof error);
    unsigned char *before=malloc(W*H*4),*after=malloc(W*H*4);
    uint32_t solid_pixels[4]={0xff808080u,0xff808080u,0xff808080u,0xff808080u};
    texture solid={0};
    int type,n,total=0,frames=0,worst=0,sparse_forward=0,old_hips=mech3d_dos_hips;
    if(!a||!before||!after){fprintf(stderr,"all-view allocation/archive failed\n");(*failures)++;goto done;}
    type=prj_find_type(a,"BWD");
    /* Supply a declared neutral test bitmap for every referenced slot:
     * otherwise fully textured cockpit pieces are correctly suppressed by
     * the production missing-bitmap behavior and cannot test placement. */
    solid.w=solid.h=2;solid.rgba=solid_pixels;
    for(n=0;n<GLR_SLOTS;n++)slots[n]=&solid;
    palette[255][0]=255;palette[255][1]=0;palette[255][2]=255;
    for(n=0;n<prj_symbol_count(a,type);n++) {
        const char *name=prj_symbol_name(a,type,n);prj_record r;bwd_chunk *ch;
        int nc,k,reprs=0,eye=-1,dos,state,most_visible[2]={0};
        mech3d full={0},pit={0};mercs_cockpit_layer layer={0};
        if(extreme&&strcmp(name,"COMMANDO")&&strcmp(name,"KODIAK")&&strcmp(name,"MADDOG"))continue;
        if(prj_read_named(a,"BWD",name,&r)!=PRJ_OK)continue;
        nc=bwd_chunks(r.data,r.size,NULL,0);ch=calloc((size_t)(nc>0?nc:1),sizeof *ch);
        if(!ch){(*failures)++;prj_record_free(&r);break;}
        bwd_chunks(r.data,r.size,ch,nc);
        for(k=0;k<nc;k++) {
            if(!strcmp(ch[k].tag,"REPR"))reprs++;
            if(!strcmp(ch[k].tag,"EYEO")&&ch[k].size>=2)eye=(int16_t)(ch[k].data[0]|ch[k].data[1]<<8);
        }
        free(ch);prj_record_free(&r);if(reprs<5||eye<0)continue;
        if(mech3d_load(a,name,0,&full)||mech3d_load(a,name,4,&pit)){(*failures)++;goto release_all;}
        mech3d_use_skeleton(&pit,&full);
        for(dos=0;dos<2;dos++)for(state=0;state<4;state++) {
            glr_view v;int translated,ok=1,visible=0,difference=0;
            mech3d_dos_hips=dos;glr_set_dos(renderer,dos?palette:NULL,NULL);
            glr_set_environment(renderer,NULL,NULL,1,1);glr_set_mercs_ground(renderer,NULL,NULL);
            glr_default_view(&v);v.free_cam=1;v.ambient=1;v.dos_amb=128;v.dos_intensity=0;
            v.dos_lpos[0]=v.dos_lpos[1]=v.dos_lpos[2]=999999;
            v.dos_sky_index=v.dos_ground_index=255;v.dos_band=0;
            v.sky_color[0]=v.sky_color[2]=1;v.sky_color[1]=0;
            v.look_yaw=27+pose[state][1]+pose[state][2];v.look_pitch=pose[state][0];v.vfov=73.739795f;
            for(translated=0;translated<2;translated++) {
                int32_t center,delta[3];
                full.heading=27;full.twist=pose[state][1];full.fall_active=pose[state][3]!=0;
                full.fall_pitch=(int32_t)(pose[state][3]*65536);
                mech3d_fall_geometry(0,full.fall_pitch,full.centre_h,27*65536,&center,delta);
                full.fall_center=center;full.origin[0]=1000+delta[0]+translated*10000;
                full.origin[1]=translated*2000;full.origin[2]=3000+delta[2]-translated*30000;
                mech3d_set_aim(&full,pose[state][0],state?2000:50000);
                mech3d_pose(&full,NULL,0);mech3d_copy_pose(&pit,&full);mech3d_pose(&pit,NULL,0);
                if(!pit.eye_ok||memcmp(full.eye_pos,pit.eye_pos,sizeof pit.eye_pos))ok=0;
                memcpy(v.eye,pit.eye_pos,sizeof v.eye);
                {
                    bwd_transform camera;int32_t angles[3],bounds[6]={0},spec[9]={1,1,1,0,0,0,0,0,0};
                    if(mech3d_cockpit_angles(&pit,pose[state][0],angles))ok=0;
                    spec[3]=angles[0];spec[4]=angles[1]+(int32_t)(pose[state][2]*65536);spec[5]=angles[2];
                    bwd_transform_block(&camera,bounds,spec);
                    for(k=0;k<9;k++)v.look_rot[k]=(float)camera.m[k]/536870912.0f;
                    v.look_rot_ok=1;
                }
                if(mercs_cockpit_update(&layer,&pit,pit.eye_pos)||glr_set_actors(renderer,&layer.model,palette,slots,0,0)){ok=0;break;}
                gpu_test_draw(renderer,&v,W,H);gpu_test_read_rgba(0, 0, W, H, translated?after:before);
                if(!gpu_test_ok())ok=0;
                frames++;
            }
            if(ok)for(k=0;k<W*H;k++) {
                visible+=cockpit_pixel(before+k*4);
                difference+=cockpit_pixel(before+k*4)!=cockpit_pixel(after+k*4);
            }
            if(difference>worst)worst=difference;
            if(visible>most_visible[dos])most_visible[dos]=visible;
            if(!state&&visible<100)sparse_forward++;
            /* Per-node integer rounding may move a few edge pixels under
             * translation. A detached part or stale camera moves far more. */
            if(!ok||difference>W*H/100) {
                fprintf(stderr,"FAIL all-view %s hip%d state%d: pixels%d changed%d\n",name,dos,state,visible,difference);
                (*failures)++;
            } else (*passed)++;
        }
        /* Some authored cockpits have all framing outside the neutral
         * forward view. Require real geometry in the tested view matrix,
         * not an invented minimum amount of forward-facing dashboard. */
        if(!extreme&&(most_visible[0]<100||most_visible[1]<100)) {
            fprintf(stderr,"FAIL all-view %s has no cockpit across the view matrix: %d/%d\n",name,most_visible[0],most_visible[1]);
            (*failures)++;
        }
        total++;
release_all:
        mercs_cockpit_free(&layer);mech3d_free(&pit);mech3d_free(&full);
    }
    if(total!=(extreme?3:65)){fprintf(stderr,"FAIL all-view model count %d\n",total);(*failures)++;}
    printf("Archive %s sweep: %d cockpits, %d GPU frames, largest translation difference %d/%d pixels, %d sparse neutral forward views\n",extreme?"extreme-pose":"all-view",total,frames,worst,W*H,sparse_forward);
done:
    free(before);free(after);if(a)prj_close(a);mech3d_dos_hips=old_hips;
    for(n=0;n<GLR_SLOTS;n++)slots[n]=NULL;
    palette[255][0]=palette[255][1]=palette[255][2]=255;
}

int main(int argc, char **argv)
{
    SDL_Window *window;
    int context;
    glr *renderer;
    glr_view view;
    unsigned char pixel[4];
    size_t i;
    int passed = 0, failures = 0;
    if(argc>1&&skygnd_dos_intensity(argv[1],"TRA1SCN1")!=40) {
        fprintf(stderr,"First argument must be retail SKYGND.PAR (TRA1 globalintensity40): %s\n",argv[1]);
        fprintf(stderr,"usage: %s [retail-SKYGND.PAR [MW2.PRJ [capture-directory [--extreme]]]]\n",argv[0]);
        return 2;
    }
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    window = SDL_CreateWindow("Mercs cockpit shader fixture", 640, 480, SDL_WINDOW_HIDDEN);
    context = gpu_test_start();
    if (!context) { fprintf(stderr, "GPU device: %s\n", SDL_GetError()); return 1; }
    for (i = 0; i < 256; i++) palette[i][0] = palette[i][1] = palette[i][2] = (unsigned char)i;
    renderer = glr_create();
    if (!renderer) return 1;
    glr_set_dos(renderer, palette, NULL);
    glr_default_view(&view);
    view.free_cam = 1; view.vfov = 73.739795f;
    view.dos_dir = 1;
    view.dos_sky_index = view.dos_ground_index = 0;
    view.sky_color[0] = view.sky_color[1] = view.sky_color[2] = 0;
    for (i = 0; i < sizeof mercs_shade_cases / sizeof mercs_shade_cases[0]; i++) {
        const int expected = 64 + mercs_shade_cases[i].level;
        quad(1000, 200, 0x1400u | (unsigned)mercs_shade_cases[i].material, 1);
        view.dos_amb = (float)mercs_shade_cases[i].ambient;
        view.dos_intensity = mercs_shade_cases[i].intensity;
        view.dos_lpos[0] = mercs_shade_cases[i].cosine == 0 ? 1.0f : 0.0f;
        view.dos_lpos[2] = mercs_shade_cases[i].cosine < 0 ? 1.0f : mercs_shade_cases[i].cosine > 0 ? -1.0f : 0.0f;
        if (sample(renderer, &view, pixel) != 0 || pixel[0] != expected || pixel[1] != expected || pixel[2] != expected) {
            fprintf(stderr, "shade case %zu: got %u,%u,%u expected %d\n", i, pixel[0], pixel[1], pixel[2], expected);
            failures++;
        } else passed++;
    }
    for (i = 0; i < sizeof mercs_fog_cases / sizeof mercs_fog_cases[0]; i++) {
        const int expected = 64 + mercs_fog_cases[i].level;
        quad(mercs_fog_cases[i].depth_cm, 12, 0x1400u | (unsigned)mercs_fog_cases[i].material, 1);
        view.dos_amb = 72; view.dos_intensity = 40;
        view.dos_lpos[0] = 0;
        view.dos_lpos[2] = mercs_fog_cases[i].cosine < 0 ? 1.0f : -1.0f;
        view.dos_fogdist = (float)mercs_fog_cases[i].fog_range;
        if (sample(renderer, &view, pixel) != 0 || pixel[0] != expected) {
            fprintf(stderr, "fog case %zu: got%u expected%d\n", i, pixel[0], expected); failures++;
        } else passed++;
    }
    view.dos_fogdist = 0;
    /* Physical near distance must survive the DOS painter-depth override.
     * A cockpit face at20cm is visible; an ordinary body face at20cm still
     * uses the world near plane. Faces wholly before8cm must disappear. */
    view.dos_amb = 72; view.dos_intensity = 0;
    {
        const int depth[] = {7, 9, 20, 20, 61};
        const int cockpit[] = {1, 1, 1, 0, 0};
        const int expected[] = {0, 70, 70, 0, 70};
        for (i = 0; i < sizeof depth / sizeof depth[0]; i++) {
            quad(depth[i], 6, 0x0460, cockpit[i]);
            if (sample(renderer, &view, pixel) != 0 || pixel[0] != expected[i]) {
                fprintf(stderr, "near case %zu: depth%d cockpit%d got%u expected%d\n",
                        i, depth[i], cockpit[i], pixel[0], expected[i]);
                failures++;
            } else passed++;
        }
    }
    /* A single face crossing8cm must clip only its near side. The painter
     * rank alone would leave pixel28 covered, even though it lies at~7.7cm. */
    quad(20, 6, 0x0460, 1);
    vertices[0].z = vertices[3].z = 4;
    if (sample(renderer, &view, pixel) != 0 || pixel[0] != 70) {
        fprintf(stderr, "straddling cockpit lost its visible side: %u\n", pixel[0]); failures++;
    } else passed++;
    gpu_test_read_rgba(28, 64, 1, 1, pixel);
    if (pixel[0] != 0) { fprintf(stderr, "straddling cockpit extends before8cm: %u\n", pixel[0]); failures++; }
    else passed++;
    /* A Gouraud quad at20cm exercises span clipping with the cockpit near
     * plane. World-near60 would reject all its span vertices. */
    quad(20, 6, 0x44ff, 1);
    if (sample(renderer, &view, pixel) != 0 || pixel[0] < 64 || pixel[0] > 79) {
        fprintf(stderr, "close Gouraud cockpit missing: got%u\n", pixel[0]); failures++;
    } else passed++;
    capacity_fixtures(renderer, &view, &passed, &failures);
    if (argc > 1) passed++; /* supplied TRA1 intensity validated before the GPU sweep */
    if (skygnd_dos_intensity(NULL, "TRA1") != 128) failures++; else passed++;
    if(argc>2) {
        int extreme=argc>4&&!strcmp(argv[4],"--extreme");
        if(!extreme)archive_cockpit_falls(renderer,argv[2],argc>3?argv[3]:NULL,&passed,&failures);
        archive_all_views(renderer,argv[2],&passed,&failures,extreme);
    }
    missing_bitmap_checks(renderer,&passed,&failures);
    glr_destroy(renderer);
    rb_quit(); SDL_DestroyWindow(window); SDL_Quit();
    printf("%s: %d actual GPU shade/near-plane/cockpit/material cases; %d failures\n",
           failures ? "FAIL" : "PASS", passed, failures);
    return failures != 0;
}
