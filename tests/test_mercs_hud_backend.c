/* Focused real-GPU HUD contracts for the backend migration. No career or
 * simulation runs: authored SHP art is only read from the supplied archive. */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "hud.h"
#include "rb.h"
#include "shp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 640
#define HEIGHT 480
static unsigned char pixels[WIDTH * HEIGHT * 3];
static int checks, failures;

static void check(int ok, const char *what)
{
    checks++;
    if (!ok) { fprintf(stderr, "FAIL: %s\n", what); failures++; }
}

static void pixel(int x, int top_y, int r, int g, int b, int tolerance, const char *what)
{
    const unsigned char *p = pixels + ((size_t)(HEIGHT - 1 - top_y) * WIDTH + (size_t)x) * 3;
    int ok = abs((int)p[0] - r) <= tolerance && abs((int)p[1] - g) <= tolerance && abs((int)p[2] - b) <= tolerance;
    if (!ok) fprintf(stderr, "  (%d,%d): %u,%u,%u expected %d,%d,%d\n", x, top_y, p[0], p[1], p[2], r, g, b);
    check(ok, what);
}

static void begin(hud *h, const float clear[4])
{
    rb_frame(WIDTH, HEIGHT);
    rb_target(0);
    rb_scissor(0, 0, 0, 0, 0);
    rb_depth_write(1);
    rb_clear(clear, 1, 1);
    hud_begin(h, WIDTH, HEIGHT);
}

static void read_picture(hud *h)
{
    hud_end(h);
    rb_read_rgb(0, 0, WIDTH, HEIGHT, pixels);
}

static void orientation_and_clip(hud *h)
{
    /* Two top-first texture rows, magnified with nearest sampling. */
    static const unsigned char rgba[16] = {
        255,0,0,255, 0,255,0,255,
        0,0,255,255, 255,255,255,255
    };
    static const float black[4] = {0,0,0,1}, magenta[4] = {1,0,1,1};
    hud_sprite s = {0};
    s.tex = rb_tex_new(); s.w = s.h = 32;
    rb_tex_image(s.tex, 2, 2, rgba, 0);
    rb_tex_wrap(s.tex, RB_CLAMP_TO_EDGE, RB_CLAMP_TO_EDGE);
    begin(h, black);
    hud_draw(h, &s, 20, 30, NULL);
    hud_clip(h, 28, 38, 8, 8);
    hud_rect(h, 20, 30, 32, 32, magenta);
    hud_clip_off(h);
    read_picture(h);
    pixel(25,35,255,0,0,0,"top-first texture upload and bottom-first RGB readback preserve top-left red");
    pixel(45,35,0,255,0,0,"top-right green is not mirrored");
    pixel(25,55,0,0,255,0,"bottom-left blue is not vertically flipped");
    pixel(45,55,255,255,255,0,"bottom-right white is not mirrored");
    pixel(30,40,255,0,255,0,"HUD top-left scissor reaches intended pixels");
    pixel(26,40,255,0,0,0,"HUD scissor preserves adjacent pixels");
    pixel(30,47,0,0,255,0,"HUD scissor bottom edge is exclusive");
    hud_sprite_free(&s);
}

static void within_frame_palette(hud *h, prj_archive *a)
{
    static const float black[4] = {0,0,0,1};
    unsigned char red[768] = {0}, green[768] = {0};
    hud_sprite s;
    prj_record record;
    shp_frame art;
    int i, opaque = -1;
    for (i = 0; i < 256; i++) { red[i*3] = 63; green[i*3+1] = 63; }
    if (prj_read_named(a,"SHP","RETICLE6",&record) != PRJ_OK) {
        check(0,"RETICLE6 resource readable"); return;
    }
    if (shp_decode(record.data,record.size,0,&art) != 0) {
        check(0,"RETICLE6 resource decodes"); prj_record_free(&record); return;
    }
    for (i = 0; i < art.w * art.h; i++) if (art.mask[i]) { opaque = i; break; }
    check(opaque >= 0,"authored reticle has an opaque sample");
    hud_set_palette(h,red);
    if (hud_sprite_load(h,a,"RETICLE6",0,&s) != 0) {
        check(0,"RETICLE6 loads into HUD cache");
    } else {
        begin(h,black);
        hud_draw(h,&s,100,100,NULL);
        check(hud_repalette_deferred(h,green),"palette changes between two recorded draws");
        hud_draw(h,&s,200,100,NULL);
        read_picture(h);
        if (opaque >= 0) {
            int x = s.left + opaque % art.w, y = s.top + opaque / art.w;
            pixel(100+x,100+y,255,0,0,0,"earlier draw retains old texture contents after deferred palette upload");
            pixel(200+x,100+y,0,255,0,0,"later draw uses new texture contents in the same frame");
        }
        hud_sprite_free(&s);
    }
    shp_frame_free(&art);
    prj_record_free(&record);
}

static void off_target_clip(hud *h)
{
    static const float black[4]={0,0,0,1}, white[4]={1,1,1,1}, green[4]={0,1,0,1};
    int x, y, untouched=1;
    begin(h,black);
    hud_clip(h,WIDTH+16,0,8,8);
    hud_rect(h,0,0,WIDTH,HEIGHT,white);
    hud_clip(h,0,HEIGHT+16,8,8);
    hud_rect(h,0,0,WIDTH,HEIGHT,white);
    hud_clip_off(h);
    hud_rect(h,4,4,4,4,green);
    read_picture(h);
    for (y=0;y<HEIGHT;y++) for (x=0;x<WIDTH;x++) {
        const unsigned char *p=pixels+((size_t)(HEIGHT-1-y)*WIDTH+(size_t)x)*3;
        if (x>=4&&x<8&&y>=4&&y<8) continue;
        if (p[0]||p[1]||p[2]) untouched=0;
    }
    check(untouched,"fully off-target horizontal and vertical scissors produce no output");
    pixel(5,5,0,255,0,0,"valid drawing resumes after an empty clip is disabled");
}

static void flash_and_tint(hud *h)
{
    static const float black[4] = {0,0,0,1}, ink[4] = {.25f,.5f,.75f,1};
    static const float base[4] = {.2f,.4f,.6f,1}, mul[3] = {.5f,.25f,1}, add[3] = {.1f,.2f,0};
    hud_set_flash(h,1,0,0,.5f);
    begin(h,black);
    hud_rect(h,10,10,20,20,ink);
    read_picture(h);
    pixel(20,20,159,64,96,1,"HUD flash uniform preserves authored color mixing");
    hud_set_flash(h,0,0,0,0);
    begin(h,base);
    hud_tint(h,WIDTH,HEIGHT,mul,add);
    read_picture(h);
    pixel(20,20,51,77,153,1,"HUD multiply/add blending preserves light amplification");
}

static void clear_target(rb_tex target, int width, int height, const float color[4])
{
    rb_target(target);
    rb_viewport(0,0,width,height);
    rb_scissor(0,0,0,0,0);
    rb_clear(color,1,0);
}

static void solid_target(rb_tex target, int width, int height, const int rgb[3], const char *what)
{
    unsigned char small[16*16*4];
    int i, k, ok = 1;
    rb_target(target);
    check(rb_read_rgba(0,0,width,height,small),"scratch-blit target RGBA readback succeeds");
    for (i=0;i<width*height;i++) {
        for (k=0;k<3;k++) if (abs((int)small[i*4+k]-rgb[k])>1) ok=0;
        if (small[i*4+3]!=255) ok=0;
    }
    check(ok,what);
}

/* A 16->8 nearest scale takes the bottom-edge tie-breaking scratch path.
 * Fill its resource table before the first use, then restart the device with
 * live sentinels occupying the old scratch handles. This catches stale rtex
 * pointers after table growth and scratch handles surviving rb_quit. */
static void blit_lifecycle(int samples)
{
    static const float source_color[4]={.25f,.5f,.75f,1};
    static const float sentinel_a_color[4]={1,0,1,1}, sentinel_b_color[4]={0,1,1,1};
    static const int source_rgb[3]={64,128,191}, magenta[3]={255,0,255}, cyan[3]={0,255,255};
    int cycle;
    rb_quit();
    for (cycle=0;cycle<2;cycle++) {
        char err[256]={0};
        rb_tex source, destination, sentinel_a=0, sentinel_b=0;
        int i, ok=rb_init(NULL,RB_GPU_AUTO,samples,err,sizeof err)==0;
        check(ok,"backend restarts with a fresh resource table");
        if (!ok) { fprintf(stderr,"%s\n",err); return; }
        rb_frame(32,32);
        source=rb_target_new(16,16,0);
        destination=rb_target_new(8,8,0);
        for (i=0;i<60;i++) (void)rb_tex_new();
        if (cycle) {
            sentinel_a=rb_target_new(16,16,0);
            sentinel_b=rb_target_new(8,8,0);
            clear_target(sentinel_a,16,16,sentinel_a_color);
            clear_target(sentinel_b,8,8,sentinel_b_color);
        }
        clear_target(source,16,16,source_color);
        rb_blit(source,0,0,16,16,destination,0,0,8,8,0);
        solid_target(destination,8,8,source_rgb,"nearest scale survives resource-table growth and device restart");
        if (cycle) {
            solid_target(sentinel_a,16,16,magenta,"old scratch handle cannot overwrite new source-sized target");
            solid_target(sentinel_b,8,8,cyan,"old scratch handle cannot overwrite new destination-sized target");
        }
        rb_quit();
    }
}

int main(int argc, char **argv)
{
    char err[256] = {0};
    char backend[32];
    prj_archive *a;
    hud *h;
    int samples = argc > 2 ? atoi(argv[2]) : 1;
    if (argc < 2 || argc > 3) { fprintf(stderr,"usage: %s models-or-MW2.PRJ [samples]\n",argv[0]); return 2; }
    a = prj_open(argv[1],err,sizeof err);
    if (!a) { fprintf(stderr,"%s\n",err); return 2; }
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO) || rb_init(NULL,RB_GPU_AUTO,samples,err,sizeof err) != 0) {
        fprintf(stderr,"%s\n",err[0] ? err : SDL_GetError()); prj_close(a); return 2;
    }
    h = hud_create(a);
    if (!h) { rb_quit(); SDL_Quit(); prj_close(a); return 2; }
    orientation_and_clip(h);
    off_target_clip(h);
    within_frame_palette(h,a);
    flash_and_tint(h);
    snprintf(backend,sizeof backend,"%s",rb_backend_name());
    hud_destroy(h);
    blit_lifecycle(samples);
    printf("{\"checks\":%d,\"failures\":%d,\"backend\":\"%s\",\"requested_samples\":%d,\"scope\":\"real HUD: orientation, scissor, within-frame palette cycling, flash, blending, scratch-blit resource growth and device restart; no career\"}\n",checks,failures,backend,samples);
    rb_quit(); SDL_Quit(); prj_close(a);
    return failures ? 1 : 0;
}
