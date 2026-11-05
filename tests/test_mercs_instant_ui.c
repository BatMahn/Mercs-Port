#include "sdl3_test.h"
/* Actual original Instant Action shell pages, hitboxes and shared Mech Lab.
 * Supply a private writable game fixture. Assets remain external. */
#include <SDL3/SDL.h>
#include "../tools/mercsshell.c"
#include <assert.h>
static const char *captures;
static int actions,frames;
static void healthy(shell *s)
{if(!s->running||s->status[0]){fprintf(stderr,"Instant UI failed: %s SDL:%s\n",s->status,SDL_GetError());abort();}}
static void frame(shell *s,const char *name)
{
    SDL_Surface *image;int w,h;char path[2048];draw(s);healthy(s);frames++;
    if(!captures)return;
    assert(SDL_GetCurrentRenderOutputSize(s->renderer,&w,&h));
    image=SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);assert(image);
    assert(!mercs_test_read_pixels(s->renderer,NULL,SDL_PIXELFORMAT_ARGB8888,image->pixels,image->pitch));
    snprintf(path,sizeof path,"%s/%s.bmp",captures,name);assert(SDL_SaveBMP(image,path));SDL_DestroySurface(image);
}
static void mouse(shell *s,int x,int y)
{
    SDL_Event e;SDL_Rect d;int ww,wh,ow,oh;SDL_GetWindowSize(s->window,&ww,&wh);
    assert(SDL_GetCurrentRenderOutputSize(s->renderer,&ow,&oh));assert(mercs_viewport_fit(ow,oh,&d));SDL_zero(e);
    e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.windowID=SDL_GetWindowID(s->window);e.button.button=SDL_BUTTON_LEFT;e.button.clicks=1;
    e.button.x=(int)lround((d.x+(x+0.5)*(double)d.w/640)*ww/ow);
    e.button.y=(int)lround((d.y+(y+0.5)*(double)d.h/480)*wh/oh);
    dispatch_event(s,&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;dispatch_event(s,&e);actions++;healthy(s);
}
int main(int argc,char **argv)
{
    shell *s=calloc(1,sizeof *s);portcfg cfg;char path[PORTCFG_PATH],error[384],models[PORTCFG_PATH],textures[PORTCFG_PATH],kind[PORTCFG_PATH],sky[PORTCFG_PATH],fog[PORTCFG_PATH];
    uint8_t *before,*after;size_t before_size,after_size;int i,planet;
    assert(argc>=2&&s);captures=argc>2?argv[2]:NULL;
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO));portcfg_defaults(&cfg);cfg.mode=1;
    assert(!portcfg_resolve(&cfg,NULL,argv[1]));assert(!portcfg_overlay_dir(&cfg,"retail",cfg.install,sizeof cfg.install,1));
    assert(!setenv("MERCS_INSTALL_DIR",cfg.install,1)&&!setenv("MERCS_CD_DIR",cfg.cd,1));dp_clear_roots();dp_add_roots_from_env();
    assert(!dp_resolve("DATABASE.MW2",path,sizeof path));assert(!shelldb_open(path,&s->database));
    s->cfg=&cfg;s->running=1;s->mode=1;s->edition=PORTCFG_ED_DOS;s->transition=-1;s->mouse_x=s->mouse_y=-1;
    assert(!portcfg_edition(&cfg,s->edition,models,textures,kind,sky,fog));s->archive=prj_open(models,error,sizeof error);assert(s->archive);
    s->window=SDL_CreateWindow("Original Mercs Instant Action", 1000, 700, SDL_WINDOW_RESIZABLE);assert(s->window);
    s->renderer=SDL_CreateRenderer(s->window, "software");assert(s->renderer);
    mercs_viewport_init(&s->viewport,s->renderer);s->art=mercs_art_create(&s->database,s->renderer);assert(s->art);
    mercs_options_defaults(&s->options);
    /* Keep an active career in memory while visiting every Instant Action
     * path, including the independently unlimited MechLab session. */
    assert(!mercs_campaign_new(s->archive,1,"CAREER PRESERVED","TEST COMPANY",&s->campaign,&s->career,error,sizeof error));
    assert(!mercs_save_encode(s->campaign.document,1,&before,&before_size));
    set_page(s,PAGE_MENU);{const mercs_ui_hotspot *h=&ui_mainmenu_hotspots[3];mouse(s,(h->x1+h->x2)/2,(h->y1+h->y2)/2);}
    assert(s->page==PAGE_INSTANT&&s->instant.ready);frame(s,"instant-main");
    mouse(s,100,100);assert(s->instant.selected==1);mouse(s,570,220);assert(s->instant.slots[1].chassis==0);
    mouse(s,100,145);mouse(s,570,220);assert(s->instant.slots[2].chassis==0);frame(s,"instant-friendly-roster");
    mouse(s,100,270);mouse(s,570,220);assert(s->instant.slots[3].chassis==0);mouse(s,470,220);assert(s->instant.slots[3].chassis==61);
    mouse(s,570,220);assert(s->instant.slots[3].chassis==-2);frame(s,"instant-random-enemy");
    mouse(s,240,450);assert(s->page==PAGE_INSTANT_PARAMS);frame(s,"instant-parameters");
    for(i=0;i<3;i++){mouse(s,200+i*64,70);assert(s->instant.parameters.skill==i+1);mouse(s,200+i*89,105);assert(s->instant.parameters.kind==i+1);}
    planet=mercs_instant_planet_index(s->instant.parameters.planet);mouse(s,230,140);assert(mercs_instant_planet_index(s->instant.parameters.planet)==(planet+1)%15);
    frame(s,"instant-parameters-changed");mouse(s,550,450);assert(s->page==PAGE_INSTANT&&s->instant.request.kind==3&&s->instant.request.skill==3);
    mouse(s,100,60);mouse(s,60,450);assert(s->page==PAGE_MECHLAB&&s->mechlab_instant&&s->mechlab);
    frame(s,"instant-mechlab");assert(!mercs_mechlab_rename(s->mechlab,"INSTANT TEST MECH"));
    close_mechlab(s,1,PAGE_INSTANT);healthy(s);assert(s->page==PAGE_INSTANT&&!s->mechlab&&s->instant.slots[0].custom_size);
    frame(s,"instant-custom-design");mouse(s,570,310);assert(s->instant.slots[0].variant_active);
    assert(!mercs_save_encode(s->campaign.document,1,&after,&after_size));assert(before_size==after_size&&!memcmp(before,after,before_size));
    s->save_return=PAGE_INSTANT;assert(!can_save_slot(s));s->save_return=PAGE_INSTANT_PARAMS;assert(!can_save_slot(s));
    mouse(s,550,450);assert(s->page==PAGE_MENU);frame(s,"instant-return-to-menu");
    free(before);free(after);mercs_instant_free(&s->instant);mercs_campaign_free(&s->campaign);mercs_art_destroy(s->art);mercs_viewport_reset(&s->viewport);
    SDL_DestroyRenderer(s->renderer);SDL_DestroyWindow(s->window);shelldb_close(&s->database);prj_close(s->archive);free(s);SDL_Quit();
    printf("PASS original Instant Action UI: %d SDL actions,%d original-art frames; six slots, parameters, custom Mech Lab, saved variants; active career unchanged.\n",actions,frames);return 0;
}
