#include "sdl3_test.h"
/* Original Cockpit Controls through real SDL menu events and overlay I/O.
 * Supply a PRIVATE game folder; no original assets are distributed here. */
#include <SDL3/SDL.h>
static bool controls_present(SDL_Renderer *renderer);
#define SDL_RenderPresent controls_present
#include "../tools/mercsshell.c"
#undef SDL_RenderPresent
#include <assert.h>
#include <sys/stat.h>
static shell *observed;
static const char *captures;
static int selection_frames, editor_frames, saved_slot, changed_axis;
static void original_title(SDL_Renderer *renderer)
{
    SDL_Rect view,rect;SDL_Surface *pixels;int w,h,x,y,red=0;
    assert(SDL_GetCurrentRenderOutputSize(renderer,&w,&h));assert(mercs_viewport_fit(w,h,&view)==1);
    rect.x=view.x+150*view.w/640;rect.y=view.y+20*view.h/480;
    rect.w=340*view.w/640;rect.h=36*view.h/480;
    pixels=SDL_CreateSurface(rect.w, rect.h, SDL_PIXELFORMAT_ARGB8888);assert(pixels);
    assert(!mercs_test_read_pixels(renderer,&rect,SDL_PIXELFORMAT_ARGB8888,pixels->pixels,pixels->pitch));
    for(y=0;y<pixels->h;y++)for(x=0;x<pixels->w;x++) {
        Uint32 pixel=*(Uint32 *)((Uint8 *)pixels->pixels+y*pixels->pitch+x*4);
        if(((pixel>>16)&255)>200&&((pixel>>8)&255)<70&&(pixel&255)<70)red++;
    }
    SDL_DestroySurface(pixels);assert(red>200); /* Original red title, not the adjacent Credits resource. */
}
static void capture(SDL_Renderer *renderer, const char *name)
{
    SDL_Surface *surface;
    int w, h;
    char path[2048];
    if (!captures) return;
    assert(SDL_GetCurrentRenderOutputSize(renderer, &w, &h));
    surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888); assert(surface);
    assert(!mercs_test_read_pixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,surface->pixels,surface->pitch));
    snprintf(path,sizeof path,"%s/%s.bmp",captures,name);
    assert(SDL_SaveBMP(surface,path)); SDL_DestroySurface(surface);
}

static bool controls_present(SDL_Renderer *renderer)
{
    shell *s=observed;
    if (s && s->controls.active) {
        mercs_controls_view *v=&s->controls;
        assert(!s->game_menu.active && !s->dialog.active);
        if (!v->editor) { if (!selection_frames++) {original_title(renderer);capture(renderer,"controls-devices");} }
        else {
            if (!editor_frames++) capture(renderer,"controls-editor");
            if (!saved_slot && v->slot==1) { saved_slot=1; capture(renderer,"controls-custom-saved"); }
            if (v->config.b[0][0].device==4 && v->config.b[0][0].type==0 &&
                v->config.b[0][0].a==0 && v->config.b[0][0].flags==1u) changed_axis=1;
        }
    }
    return SDL_RenderPresent(renderer);
}
static void push_key(SDL_Keycode key)
{
    SDL_Event e;
    SDL_zero(e); e.type=SDL_EVENT_KEY_DOWN; e.key.key=key; e.key.down=true;
    assert(SDL_PushEvent(&e)==1);
}

static void coords(shell *s,int x,int y,int *rx,int *ry)
{
    SDL_Rect dst;
    int ww,wh,ow,oh;
    SDL_GetWindowSize(s->window,&ww,&wh);
    assert(SDL_GetCurrentRenderOutputSize(s->renderer,&ow,&oh));
    assert(mercs_viewport_fit(ow,oh,&dst)==1);
    *rx=(int)lround((dst.x+(x+0.5)*(double)dst.w/640)*ww/ow);
    *ry=(int)lround((dst.y+(y+0.5)*(double)dst.h/480)*wh/oh);
}

static void push_mouse(shell *s,Uint32 type,int button,int x,int y)
{
    SDL_Event e;
    int rx,ry;
    coords(s,x,y,&rx,&ry); SDL_zero(e); e.type=type;
    if(type==SDL_EVENT_MOUSE_MOTION){e.motion.x=(float)rx;e.motion.y=(float)ry;e.motion.state=SDL_BUTTON_LMASK;}
    else {e.button.button=(Uint8)button;e.button.x=(float)rx;e.button.y=(float)ry;e.button.clicks=1;e.button.down=type==SDL_EVENT_MOUSE_BUTTON_DOWN;}
    assert(SDL_PushEvent(&e)==1);
}

static void push_click(shell *s,int x,int y)
{
    push_mouse(s,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,x,y);
    push_mouse(s,SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,x,y);
}

static Uint32 SDLCALL watchdog(void *unused, SDL_TimerID timer_id, Uint32 interval)
{
    SDL_Event e;(void)timer_id; (void)interval;(void)unused;
    SDL_zero(e);e.type=SDL_EVENT_QUIT;SDL_PushEvent(&e);
    fputs("FAIL: Cockpit Controls modal stalled\n",stderr);return 0;
}

int main(int argc,char **argv)
{
    shell *s=calloc(1,sizeof *s);portcfg cfg;
    mercs_control_config baseline,saved,slot;mercs_options options;
    SDL_Event e;SDL_TimerID timer;char path[PORTCFG_PATH*2],error[384],map[32768];
    uint8_t before[MC_CPC_BYTES],after[MC_CPC_BYTES];size_t size;unsigned char *actual;
    int i;
    assert(argc>=2 && s);captures=argc>2?argv[2]:NULL;
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO));
    portcfg_defaults(&cfg);cfg.mode=1;
    assert(!portcfg_resolve(&cfg,NULL,argv[1]));
    assert(!portcfg_overlay_dir(&cfg,"retail",cfg.install,sizeof cfg.install,1));
    assert(!portcfg_save(NULL,&cfg));
    assert(!dp_resolve_in(cfg.cd,"MERCS/DATABASE.MW2",path,sizeof path) ||
           !dp_resolve_in(cfg.cd,"DATABASE.MW2",path,sizeof path));
    assert(!shelldb_open(path,&s->database));
    dp_clear_roots();assert(!dp_add_root(cfg.install));assert(!dp_add_root(cfg.cd));
    snprintf(path,sizeof path,"%s/MERCS",cfg.cd);dp_add_root(path);
    s->cfg=&cfg;s->running=1;s->page=PAGE_MENU;s->transition=-1;s->mouse_x=s->mouse_y=-1;
    s->window=SDL_CreateWindow("Mercs original Cockpit Controls", 1000, 700, SDL_WINDOW_RESIZABLE);assert(s->window);
    s->renderer=SDL_CreateRenderer(s->window, "software");assert(s->renderer);
    mercs_viewport_init(&s->viewport,s->renderer);s->art=mercs_art_create(&s->database,s->renderer);assert(s->art);
    s->audio=sfx_create(NULL);assert(s->audio);s->shell_audio=mercs_shell_audio_create(s->audio,&s->database);assert(s->shell_audio);
    mercs_options_defaults(&s->options);assert(!mercs_options_save(&s->options,cfg.install,error,sizeof error));
    assert(!mercs_controls_load(&baseline,"KEYBOARD.CPC"));
    assert(!mercs_controls_apply(&baseline,cfg.install,error,sizeof error));
    observed=s;draw(s);while(SDL_PollEvent(&e)){}
    timer=SDL_AddTimer(20000,watchdog,NULL);assert(timer);
    push_key(SDLK_DOWN);push_key(SDLK_DOWN);push_key(SDLK_RETURN); /* original game-menu row2 */
    push_click(s,20,180); /* mouse device, index4 */
    push_click(s,20,425); /* original Custom Configuration */
    push_click(s,20,158); /* mouse device in editor */
    push_click(s,300,115); /* select Throttle */
    push_click(s,490,137); /* original Left/Right mouse axis */
    push_click(s,248,115); /* reverse */
    push_click(s,270,115); /* Ctrl modifier */
    push_click(s,10,390); /* Save Custom1 */
    push_click(s,248,115); /* modify after saving */
    push_click(s,10,335); /* Load Custom1 restores exact config */
    push_click(s,10,467); /* accept */
    push_key(SDLK_ESCAPE); /* leave game menu */
    open_game_menu(s);SDL_RemoveTimer(timer);
    assert(s->running && !s->controls.active && !s->dialog.active && !s->game_menu.active);
    fprintf(stderr,"controls frames selection=%d editor=%d changed=%d saved=%d binding=%d/%d/%d flags=%x\n",selection_frames,editor_frames,changed_axis,saved_slot,s->controls.config.b[0][0].type,s->controls.config.b[0][0].device,s->controls.config.b[0][0].a,s->controls.config.b[0][0].flags);
    assert(selection_frames>0 && editor_frames>=12 && changed_axis && saved_slot);
    assert(!mercs_controls_load(&saved,"CONFIG00.CPC"));assert(!mercs_controls_load(&slot,"CONFIG01.CPC"));
    mercs_controls_encode(&saved,before);mercs_controls_encode(&slot,after);assert(!memcmp(before,after,sizeof before));
    assert(saved.b[0][0].device==4 && saved.b[0][0].type==0 && saved.b[0][0].a==0 && saved.b[0][0].flags==1u);
    size=mercs_controls_generate(&saved,map,sizeof map,NULL);assert(size>0);
    assert(!dp_read_file("INPUT.MAP",&actual,&size));assert(size==strlen(map) && !memcmp(actual,map,size));free(actual);
    /* Reopen, enter editor, alter a binding, then Abort. Map/CPC stay exact. */
    while(SDL_PollEvent(&e)){}
    push_click(s,20,425);push_click(s,248,115);push_key(SDLK_ESCAPE);
    original_cockpit_controls(s);assert(s->running && !s->controls.active);
    assert(!mercs_controls_load(&saved,"CONFIG00.CPC"));mercs_controls_encode(&saved,after);assert(!memcmp(before,after,sizeof before));
    /* Novice uses the original two extra Mercs controls and commits immediately. */
    while(SDL_PollEvent(&e)){}
    push_click(s,20,403);push_key(SDLK_ESCAPE);original_cockpit_controls(s);
    assert(mercs_options_get(&s->options,0x50)==1);
    assert(!mercs_options_load(&options,cfg.install,cfg.cd,error,sizeof error) && mercs_options_get(&options,0x50)==1);
    assert(!dp_read_file("INPUT.MAP",&actual,&size));
    assert(size<sizeof map);memcpy(map,actual,size);map[size]=0;free(actual);
    assert(strstr(map,"novice_forward") && strstr(map,"novice_backward"));
    /* All four pages remain available and selecting a binding reveals its key. */
    assert(!mercs_controls_open(&s->controls,0));s->controls.editor=1;
    for(i=0;i<4;i++){int old=s->controls.level;mercs_controls_click(s->art,&s->controls,380,94,0);assert(s->controls.level==(old+1)%4);}
    s->controls.config.b[0][17]=(mercs_control_binding){1,0,0,0,50,-1};
    s->controls.device=0;s->controls.control=-1;
    mercs_controls_click(s->art,&s->controls,300,112+17*11+2,0);assert(s->controls.scroll==50);
    s->controls.active=0;
    puts("PASS: original Cockpit Controls menu,device selection,custom editor,axis/reverse/modifier,custom save/load,apply,reopen,abort,novice persistence and four pages.");
    observed=NULL;mercs_shell_audio_destroy(s->shell_audio);sfx_destroy(s->audio);mercs_art_destroy(s->art);
    mercs_viewport_reset(&s->viewport);SDL_DestroyRenderer(s->renderer);SDL_DestroyWindow(s->window);shelldb_close(&s->database);free(s);SDL_Quit();return 0;
}
