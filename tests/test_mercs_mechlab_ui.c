#include "sdl3_test.h"
/* Real original-art shell events and durable customization; supplied game
 * directory MUST be private/writable. SDL offscreen + dummy audio supported. */
#include <SDL3/SDL.h>
#include "../tools/mercsshell.c"
#include "fixtures/mercs_mechlab_original.h"
#include <assert.h>

static const char *captures;
static int actions,frames,callback_count,callback_attempts,callback_fail;
static uint8_t instant_result[MERCS_CAREER_DEPLOY_MAX_BYTES];
static size_t instant_length;
static void healthy(shell *s)
{ if(!s->running||s->status[0]){fprintf(stderr,"mechlab UI: %s\n",s->status);abort();} }
static void frame(shell *s,const char *name)
{
    draw(s);healthy(s);frames++;
    if(captures){
        SDL_Surface *surface;char path[4096];int w,h;
        assert(SDL_GetCurrentRenderOutputSize(s->renderer,&w,&h));
        surface=SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);assert(surface);
        assert(!mercs_test_read_pixels(s->renderer,NULL,SDL_PIXELFORMAT_ARGB8888,surface->pixels,surface->pitch));
        snprintf(path,sizeof path,"%s/%s.bmp",captures,name);assert(SDL_SaveBMP(surface,path));SDL_DestroySurface(surface);
    }
}
static void mouse(shell *s,int x,int y,int twice)
{
    SDL_Event e;SDL_Rect d;int ww,wh,ow,oh;
    SDL_GetWindowSize(s->window,&ww,&wh);assert(SDL_GetCurrentRenderOutputSize(s->renderer,&ow,&oh));
    assert(mercs_viewport_fit(ow,oh,&d)==1);SDL_zero(e);
    e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.windowID=SDL_GetWindowID(s->window);e.button.button=SDL_BUTTON_LEFT;e.button.clicks=twice?2:1;
    e.button.x=(int)lround((d.x+(x+0.5)*(double)d.w/640)*ww/ow);
    e.button.y=(int)lround((d.y+(y+0.5)*(double)d.h/480)*wh/oh);
    dispatch_event(s,&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;dispatch_event(s,&e);actions++;
}
static void typed(shell *s,const char *text)
{ SDL_Event e;SDL_zero(e);e.type=SDL_EVENT_TEXT_INPUT;e.text.text=text;dispatch_event(s,&e);actions++; }
static void key_event(shell *s,SDL_Keycode k)
{ SDL_Event e;SDL_zero(e);e.type=SDL_EVENT_KEY_DOWN;e.key.key=k;dispatch_event(s,&e);actions++; }
static void queue_yes(void)
{ SDL_Event e;SDL_zero(e);e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_RETURN;assert(SDL_PushEvent(&e)==1); }
static void equipment(shell *s,int id)
{
    size_t i;
    if(s->mechlab_view.category!=MERCS_ITEM_EQUIPMENT)mouse(s,320,25,0);
    for(i=0;i<s->mechlab_view.count;i++)if(s->mechlab_view.groups[i].id==id)break;
    assert(i<s->mechlab_view.count);mouse(s,480,117+(int)i*10,1);healthy(s);
}
static int raw_finished(void *unused,const uint8_t *raw,size_t size)
{
    (void)unused;callback_attempts++;
    if(callback_fail)return -1;
    assert(size<=sizeof instant_result);memcpy(instant_result,raw,size);
    instant_length=size;callback_count++;return 0;
}
static void editor(shell *s)
{
    int i;uint32_t engine,armor;char error[384],dir[PORTCFG_PATH];size_t n;
    mercs_campaign saved={0};mercs_shell_state *view=calloc(1,sizeof *view);assert(view);
    set_page(s,PAGE_MECHOPS);mouse(s,100,235,0);healthy(s);assert(s->page==PAGE_MECHLAB&&s->mechlab);
    frame(s,"mechlab-original-main");
    for(i=0;i<5;i++){char name[64];mouse(s,40+i*90,25,0);assert(s->mechlab_view.category==i);
        snprintf(name,sizeof name,"mechlab-category-%d",i);frame(s,name);}
    mouse(s,90,115,0);assert(mev_i32(mercs_mechlab_current(s->mechlab)->bytes+0x364)==1);
    equipment(s,0);assert(s->mechlab_view.panel==1);frame(s,"mechlab-engine");
    engine=le32(mercs_mechlab_current(s->mechlab)->bytes+0x20c);
    mouse(s,485,334,0);assert(le32(mercs_mechlab_current(s->mechlab)->bytes+0x20c)!=engine);
    mouse(s,485,345,0);assert(le32(mercs_mechlab_current(s->mechlab)->bytes+0x20c)==engine);
    equipment(s,9);assert(s->mechlab_view.panel==2);frame(s,"mechlab-armor");
    armor=le32(mercs_mechlab_current(s->mechlab)->bytes+0x250);
    mouse(s,480,311,0);assert(le32(mercs_mechlab_current(s->mechlab)->bytes+0x250)==armor+50);
    mouse(s,480,322,0);assert(le32(mercs_mechlab_current(s->mechlab)->bytes+0x250)==armor);
    assert(!mercs_mechlab_rollback(s->mechlab));
    equipment(s,8);assert(mercs_mechlab_pending(s->mechlab,NULL)==14);frame(s,"mechlab-place-critical");
    for(i=0;i<11;i++)mouse(s,480,255,0);
    mouse(s,150,115,0);assert(mev_i32(mercs_mechlab_current(s->mechlab)->bytes+0x364)==3);
    for(i=0;i<3;i++)mouse(s,480,255,0);
    assert(!mercs_mechlab_pending(s->mechlab,NULL));assert(le32(mercs_mechlab_current(s->mechlab)->bytes+0x240)==1);
    frame(s,"mechlab-endo-fitted");assert(!mercs_mechlab_rollback(s->mechlab));
    /* A canceled design never reaches the career generation. */
    close_mechlab(s,0,PAGE_MECHOPS);assert(!s->mechlab);
    mouse(s,100,235,0);assert(s->mechlab);
    mouse(s,42,145,0);assert(mev_i32(mercs_mechlab_current(s->mechlab)->bytes+0x364)==4);
    mouse(s,300,157,0);assert(s->mechlab_view.slot==4);mouse(s,330,400,0);
    assert(mercs_mechlab_cost(s->mechlab)==50000);frame(s,"mechlab-weapon-removed");
    mouse(s,120,70,0);assert(s->mechlab_editing);typed(s,"LAB VERIFIED");key_event(s,SDLK_RETURN);
    assert(!s->mechlab_editing&&!strcmp((const char *)mercs_mechlab_current(s->mechlab)->bytes+0x100,"LAB VERIFIED"));
    frame(s,"mechlab-renamed");
    /* Persistence failure retains the private edit and the previous career. */
    snprintf(dir,sizeof dir,"%s",s->save_dir);
    {char bad[PORTCFG_PATH];FILE *f;
     assert(snprintf(bad,sizeof bad,"%s/mechlab-write-block",dir)>0);f=fopen(bad,"wb");assert(f);assert(!fclose(f));
     assert(strlen(bad)+8<sizeof s->save_dir);strcpy(s->save_dir,bad);strcat(s->save_dir,"/career");}
    close_mechlab(s,1,PAGE_MECHOPS);assert(s->mechlab&&s->status[0]);
    assert(strcmp((const char *)s->career.owned[0].current.bytes+0x100,"LAB VERIFIED"));
    snprintf(s->save_dir,sizeof s->save_dir,"%s",dir);s->status[0]=0;
    queue_yes();mouse(s,550,450,0);healthy(s);assert(!s->mechlab&&s->page==PAGE_MECHOPS);
    assert(!strcmp((const char *)s->career.owned[0].current.bytes+0x100,"LAB VERIFIED"));
    assert(!mercs_store_read(s->save_dir,-1,s->archive,&saved,view,error,sizeof error));
    assert(!strcmp((const char *)view->owned[0].current.bytes+0x100,"LAB VERIFIED"));
    assert(!memcmp(view->owned[0].current.bytes,s->career.owned[0].current.bytes,0x850));
    mercs_campaign_free(&saved);
    /* The same art/editor works on an isolated Instant Action MEK. */
    assert(!mercs_career_deployment_bytes(&s->career.owned[0].current,instant_result,sizeof instant_result,&n));
    assert(!open_mechlab_instant(s,instant_result,n,"cmd00usr.mek",PAGE_MENU,raw_finished,NULL));
    assert(s->mechlab_instant);frame(s,"mechlab-instant");
    mouse(s,385,450,0);assert(s->mechlab_view.technology==0);frame(s,"mechlab-instant-clan-catalog");
    mouse(s,270,450,0);assert(s->mechlab_view.technology==1);
    mouse(s,120,70,0);typed(s,"INSTANT LAB");key_event(s,SDLK_RETURN);
    /* A failed custom-MEK sink keeps the session, name and private edit for
     * a subsequent retry; it cannot close the editor and lose the design. */
    callback_fail=1;queue_yes();mouse(s,550,450,0);healthy(s);
    assert(callback_attempts==1&&callback_count==0&&!instant_length&&s->page==PAGE_MECHLAB&&s->mechlab);
    assert(!strcmp((const char *)mercs_mechlab_current(s->mechlab)->bytes+0x100,"INSTANT LAB"));
    frame(s,"mechlab-instant-write-retry");
    callback_fail=0;queue_yes();mouse(s,550,450,0);
    assert(callback_attempts==2&&callback_count==1&&instant_length>0&&s->page==PAGE_MENU&&!s->mechlab);
    assert(!strcmp((const char *)s->career.owned[0].current.bytes+0x100,"LAB VERIFIED"));free(view);
}
static Uint32 SDLCALL watchdog(void *unused, SDL_TimerID timer_id, Uint32 interval)
{ SDL_Event e;(void)timer_id; (void)interval;(void)unused;SDL_zero(e);e.type=SDL_EVENT_QUIT;SDL_PushEvent(&e);return 0; }
int main(int argc,char **argv)
{
    shell *s=calloc(1,sizeof *s);portcfg cfg;SDL_Event event;SDL_TimerID timer;
    char path[PORTCFG_PATH],error[384],models[PORTCFG_PATH],textures[PORTCFG_PATH],kind[PORTCFG_PATH],sky[PORTCFG_PATH],fog[PORTCFG_PATH];int i;
    assert(argc>=2&&s);captures=argc>2?argv[2]:NULL;
    portcfg_defaults(&cfg);assert(!portcfg_resolve(&cfg,NULL,argv[1]));cfg.mode=1;
    s->cfg=&cfg;s->running=1;s->mode=1;s->edition=PORTCFG_ED_DOS;s->accepted=-1;s->transition=-1;s->mouse_x=s->mouse_y=-1;
    assert(!portcfg_overlay_dir(&cfg,"retail",path,sizeof path,1));assert(!mercs_store_directory(path,s->save_dir,sizeof s->save_dir));
    assert(!SDL_setenv_unsafe("MERCS_INSTALL_DIR",path,1)&&!SDL_setenv_unsafe("MERCS_CD_DIR",cfg.cd,1));
    dp_clear_roots();dp_add_roots_from_env();assert(!dp_set_write_root(path));
    assert(!dp_resolve("DATABASE.MW2",path,sizeof path)&&!shelldb_open(path,&s->database));
    assert(!portcfg_edition(&cfg,s->edition,models,textures,kind,sky,fog));s->archive=prj_open(models,error,sizeof error);assert(s->archive);
    assert(SDL_Init(SDL_INIT_VIDEO));timer=SDL_AddTimer(30000,watchdog,NULL);assert(timer);
    s->window=SDL_CreateWindow("Mercs Mech Lab verification", 1280, 830, SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);assert(s->window);
    s->renderer=SDL_CreateRenderer(s->window, "software");assert(s->renderer);
    mercs_viewport_init(&s->viewport,s->renderer);s->art=mercs_art_create(&s->database,s->renderer);assert(s->art);
    assert(!mercs_campaign_new(s->archive,1,"MECHLAB","TEST COMPANY",&s->campaign,&s->career,error,sizeof error));
    assert(!mercs_save_set_list(s->campaign.document,MERCS_SAVE_OWNED_WEAPONS,mlab_input_weapons,sizeof mlab_input_weapons/26));
    assert(!mercs_save_set_list(s->campaign.document,MERCS_SAVE_OWNED_AMMO,mlab_input_ammo,sizeof mlab_input_ammo/26));
    {uint8_t *h=mercs_save_header_edit(s->campaign.document);h[0x12a]=0;h[0x12b]=0xe1;h[0x12c]=0xf5;h[0x12d]=5;}
    assert(!mercs_campaign_hydrate(s->archive,&s->campaign,&s->career,error,sizeof error));
    s->accepted=s->campaign.accepted;s->selected=s->campaign.selected;strcpy(s->pilot,s->career.pilot);strcpy(s->unit,s->career.unit);
    while(SDL_PollEvent(&event)){}editor(s);healthy(s);SDL_RemoveTimer(timer);
    release_mechlab(s);mercs_campaign_free(&s->campaign);mercs_text_free(&s->body);mercs_art_destroy(s->art);mercs_viewport_reset(&s->viewport);
    for(i=0;i<TEXT_CACHE;i++)SDL_DestroyTexture(s->text[i].texture);
    for(i=0;i<120;i++)SDL_DestroyTexture(s->backgrounds[i]);
    prj_close(s->archive);shelldb_close(&s->database);SDL_DestroyRenderer(s->renderer);SDL_DestroyWindow(s->window);dp_clear_roots();SDL_Quit();free(s);
    printf("PASS: %d original-art Mech Lab frames, %d real shell input events, durable edit/reload, career/custom-MEK failed-write retry and isolated Instant Action adapter\n",frames,actions);return 0;
}
