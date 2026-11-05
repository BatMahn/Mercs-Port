#include "sdl3_test.h"
/* Real shell events, original game assets and durable career transactions.
 * Supply a PRIVATE writable game folder. Screenshots stay outside source.
 * SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy on headless Linux. */
#include <SDL3/SDL.h>
#include "../tools/mercsshell.c"
#include <assert.h>
#include <sys/stat.h>

static const char *captures;
static int frames, actions;
static void healthy(shell *s)
{
    if (!s->running || s->status[0]) {
        fprintf(stderr,"FAIL interchange UI: running=%d status=%s SDL=%s\n",s->running,s->status,SDL_GetError());
        abort();
    }
}
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
    e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.windowID=SDL_GetWindowID(s->window);
    e.button.button=SDL_BUTTON_LEFT;e.button.clicks=(Uint8)(twice?2:1);
    e.button.x=(int)lround((d.x+(x+0.5)*(double)d.w/640)*ww/ow);
    e.button.y=(int)lround((d.y+(y+0.5)*(double)d.h/480)*wh/oh);
    dispatch_event(s,&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;dispatch_event(s,&e);actions++;
}

static void queue_key(SDL_Keycode k)
{SDL_Event e;SDL_zero(e);e.type=SDL_EVENT_KEY_DOWN;e.key.key=k;assert(SDL_PushEvent(&e)==1);}
static void press(shell *s,SDL_Keycode k,SDL_Keymod mods)
{SDL_Event e;SDL_zero(e);e.type=SDL_EVENT_KEY_DOWN;e.key.key=k;e.key.mod=mods;dispatch_event(s,&e);actions++;}
static void type(shell *s,const char *text)
{SDL_Event e;SDL_zero(e);e.type=SDL_EVENT_TEXT_INPUT;e.text.text=text;dispatch_event(s,&e);actions++;}
static void put32(uint8_t *p,uint32_t v)
{p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static uint8_t *read_file(const char *path,size_t *size)
{FILE *f=fopen(path,"rb");long z;uint8_t *b;assert(f);assert(!fseek(f,0,SEEK_END));z=ftell(f);rewind(f);
 assert(z>=0);b=malloc((size_t)z+1);assert(b);assert(fread(b,1,(size_t)z,f)==(size_t)z&&!fclose(f));*size=(size_t)z;return b;}
static void write_file(const char *path,const uint8_t *b,size_t size)
{FILE *f=fopen(path,"wb");assert(f);assert(fwrite(b,1,size,f)==size&&!fclose(f));}
static void file_equal(const char *path,const uint8_t *b,size_t size)
{size_t z;uint8_t *actual=read_file(path,&z);assert(z==size&&!memcmp(actual,b,size));free(actual);}
static void choose(shell *s,int slot)
{static const int y[]={97,132,167,202,237,272,307,342,377,412};mouse(s,100,y[slot],0);assert(s->save_slot==slot);}
static void describe(shell *s,int slot,const char *title)
{choose(s,slot);if(!s->save_editing)choose(s,slot);assert(s->save_editing);
 press(s,SDLK_A,SDL_KMOD_CTRL);type(s,title);press(s,SDLK_RETURN,0);assert(!strcmp(s->save_titles[slot],title));}
static void save_load(shell *s,const char *fixtures,const char *overlay)
{
    char source[4096],target[4096],error[384],title[256];size_t size,sz;uint8_t *bytes,*actual,*before,*after;
    mercs_campaign saved={0};mercs_shell_state *v=calloc(1,sizeof *v);assert(v);
    snprintf(source,sizeof source,"%s/regular-hall.sav",fixtures);bytes=read_file(source,&size);
    strcpy((char*)bytes+2,"ORIGINAL DOS SLOT");
    snprintf(target,sizeof target,"%s/mersave7.sav",overlay);write_file(target,bytes,size);
    mercs_save_header_edit(s->campaign.document)[0]=1;set_page(s,PAGE_HALL);press(s,SDLK_F5,0);healthy(s);
    assert(s->page==PAGE_SAVE&&!strcmp(s->save_titles[7],(const char*)bytes+2));
    choose(s,7);frame(s,"save-original-slot");
    queue_key(SDLK_N);mouse(s,450,410,0);healthy(s);assert(s->page==PAGE_SAVE&&!strcmp(s->career.pilot,"NATIVE"));
    queue_key(SDLK_Y);mouse(s,450,410,0);healthy(s);assert(s->page==PAGE_HALL&&!strcmp(s->career.pilot,"IMPORTER"));
    file_equal(target,bytes,size);frame(s,"save-original-loaded");
    press(s,SDLK_F5,0);describe(s,1,"EXPORTED CAREER");frame(s,"save-native-description");
    mouse(s,560,410,0);healthy(s);assert(s->page==PAGE_HALL);
    snprintf(source,sizeof source,"%s/mersave1.sav",overlay);
    assert(!mercs_store_original_title(source,title,sizeof title)&&!strcmp(title,"EXPORTED CAREER"));
    assert(mercs_store_original_differs(s->save_dir,1,source)==0);
    file_equal(target,bytes,size);free(bytes);bytes=read_file(source,&size);
    /* A file changed by the original game remains a separately selectable
     * source. Neither choice silently overwrites that source file. */
    put32(bytes+0x12a,424242);strcpy((char*)bytes+2,"EDITED IN ORIGINAL");write_file(source,bytes,size);
    press(s,SDLK_F5,0);choose(s,1);queue_key(SDLK_N);mouse(s,450,410,0);healthy(s);
    assert(s->career.funds==1000000&&s->page==PAGE_HALL);file_equal(source,bytes,size);
    press(s,SDLK_F5,0);choose(s,1);queue_key(SDLK_Y);mouse(s,450,410,0);healthy(s);
    assert(s->career.funds==424242&&s->page==PAGE_HALL);file_equal(source,bytes,size);free(bytes);
    /* Invalid imports cannot replace live state or the working generation. */
    snprintf(source,sizeof source,"%s/mersave8.sav",overlay);write_file(source,(const uint8_t*)"bad",3);
    assert(!mercs_save_encode(s->campaign.document,0,&before,&size));
    press(s,SDLK_F5,0);choose(s,8);mouse(s,450,410,0);assert(s->status[0]);
    assert(!mercs_save_encode(s->campaign.document,0,&after,&sz));assert(size==sz&&!memcmp(before,after,size));free(before);free(after);
    assert(!mercs_store_read(s->save_dir,-1,s->archive,&saved,v,error,sizeof error));assert(v->funds==424242);
    mercs_campaign_free(&saved);s->status[0]=0;frame(s,"save-invalid-source-retained");
    /* Export is a second atomic file. Refuse a directory without discarding
     * the already committed native slot, and allow an explicit retry. */
    snprintf(source,sizeof source,"%s/mersave2.sav",overlay);assert(!mkdir(source,0700));
    describe(s,2,"EXPORT RETRY");save_slot(s);assert(strstr(s->status,"was saved"));
    assert(!mercs_store_read(s->save_dir,2,s->archive,&saved,v,error,sizeof error));assert(v->funds==424242);
    mercs_campaign_free(&saved);assert(!rmdir(source));s->status[0]=0;
    queue_key(SDLK_Y);mouse(s,560,410,0);healthy(s);assert(s->page==PAGE_HALL);
    assert(mercs_store_original_differs(s->save_dir,2,source)==0);
    actual=read_file(source,&sz);assert(sz>MERCS_SAVE_HEADER_BYTES);free(actual);free(v);
    puts("PASS Save/Load SDL workflow: original slots, dirty cancellation, descriptions, export, competing original/native choice, invalid import and export retry");
}
static void lab_settlement(shell *s)
{
    mercs_campaign saved={0};mercs_shell_state *v=calloc(1,sizeof *v);char error[384],directory[PORTCFG_PATH];
    const char *committed=(const char*)s->career.owned[0].current.bytes+0x100;char original[256];
    assert(v);snprintf(original,sizeof original,"%s",committed);set_page(s,PAGE_MECHOPS);mouse(s,110,240,0);
    assert(s->page==PAGE_MECHLAB&&s->mechlab);mouse(s,100,70,0);assert(s->mechlab_editing);
    press(s,SDLK_A,SDL_KMOD_CTRL);type(s,"SAVED LAB DESIGN");press(s,SDLK_RETURN,0);
    assert(mercs_mechlab_dirty(s->mechlab));frame(s,"save-lab-before-settlement");
    strcpy(directory,s->save_dir);strcat(s->save_dir,"/invalid");queue_key(SDLK_Y);press(s,SDLK_F5,0);
    assert(s->mechlab&&s->page==PAGE_MECHLAB&&s->status[0]);
    assert(!strcmp((const char*)s->career.owned[0].current.bytes+0x100,original));
    strcpy(s->save_dir,directory);s->status[0]=0;queue_key(SDLK_Y);press(s,SDLK_F5,0);healthy(s);
    assert(!s->mechlab&&s->page==PAGE_SAVE&&s->save_return==PAGE_MECHLAB);
    assert(!strcmp((const char*)s->career.owned[0].current.bytes+0x100,"SAVED LAB DESIGN"));
    assert(!mercs_store_read(s->save_dir,-1,s->archive,&saved,v,error,sizeof error));
    assert(!strcmp((const char*)v->owned[0].current.bytes+0x100,"SAVED LAB DESIGN"));mercs_campaign_free(&saved);
    describe(s,3,"MECH LAB SAVED");mouse(s,560,410,0);healthy(s);
    assert(s->page==PAGE_MECHLAB&&s->mechlab&&!mercs_mechlab_dirty(s->mechlab));frame(s,"save-lab-reopened");
    assert(!mercs_store_read(s->save_dir,3,s->archive,&saved,v,error,sizeof error));
    assert(restored_page(&saved)==PAGE_MECHOPS);mercs_campaign_free(&saved);
    mouse(s,100,70,0);press(s,SDLK_A,SDL_KMOD_CTRL);type(s,"CANCELLED DESIGN");press(s,SDLK_RETURN,0);
    queue_key(SDLK_N);press(s,SDLK_F5,0);healthy(s);assert(s->page==PAGE_SAVE&&!s->mechlab);
    assert(!strcmp((const char*)s->career.owned[0].current.bytes+0x100,"SAVED LAB DESIGN"));
    mouse(s,500,452,0);healthy(s);assert(s->page==PAGE_MECHLAB&&s->mechlab);
    close_mechlab(s,0,PAGE_MECHOPS);healthy(s);free(v);
    puts("PASS Mech Lab Save/Load settlement: private name, failed-save retry, one committed design, save/exit reopen, cancel and owner-room restore");
}
static void large_fleet_ui(shell *s,const char *fixtures,const char *overlay)
{
    char source[4096],target[4096];uint8_t *bytes;size_t size;SDL_Event e;int h,last_y;
    snprintf(source,sizeof source,"%s/fleet-96.sav",fixtures);bytes=read_file(source,&size);
    snprintf(target,sizeof target,"%s/mersave9.sav",overlay);write_file(target,bytes,size);free(bytes);
    mercs_save_header_edit(s->campaign.document)[0]=0;press(s,SDLK_F5,0);choose(s,9);mouse(s,450,410,0);healthy(s);
    assert(s->career.owned_count==96&&s->career.owned_index==95&&s->page==PAGE_COMMAND);
    set_page(s,PAGE_INVENTORY);assert(s->inventory_view.count==96);SDL_zero(e);e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.y=-100;
    dispatch_event(s,&e);actions++;h=mercs_art_height(s->art,10);last_y=115+(2+95-s->inventory_view.scroll)*h+h/2;
    mouse(s,100,last_y,0);healthy(s);assert(s->inventory_view.selected==95);frame(s,"save-fleet-96-inventory");
    set_page(s,PAGE_GROUPS);assert(s->groups_view.selected==0);mouse(s,100,370,0);healthy(s);
    assert(s->groups_view.selected==95);frame(s,"save-fleet-96-groups");
    puts("PASS 96-owned fleet via original Load, scrolling inventory, final-row selection and Weapon Groups");
}
static void windows_ui(shell *s,const char *fixtures,const char *overlay)
{
    char source[4096],target[4096],error[384];uint8_t *bytes,*expected,*actual;size_t size,es,as;
    mercs_campaign saved={0};mercs_shell_state *v=calloc(1,sizeof *v);assert(v);
    snprintf(source,sizeof source,"%s/windows-traded-lab.sav",fixtures);bytes=read_file(source,&size);
    strcpy((char*)bytes+2,"ORIGINAL WINDOWS SLOT");
    snprintf(source,sizeof source,"%s/mersave6.sav",overlay);write_file(source,bytes,size);
    mercs_save_header_edit(s->campaign.document)[0]=0;set_page(s,PAGE_COMMAND);press(s,SDLK_F5,0);
    choose(s,6);mouse(s,450,410,0);healthy(s);assert(s->page==PAGE_COMMAND&&mercs_save_windows(s->campaign.document));
    file_equal(source,bytes,size);frame(s,"save-original-windows-loaded");
    press(s,SDLK_F5,0);describe(s,5,"WINDOWS EXPORTED");mouse(s,560,410,0);healthy(s);
    assert(s->page==PAGE_COMMAND&&mercs_save_windows(s->campaign.document));
    assert(!mercs_store_read(s->save_dir,5,s->archive,&saved,v,error,sizeof error));assert(mercs_save_windows(saved.document));
    assert(!mercs_save_encode(saved.document,1,&expected,&es));
    snprintf(target,sizeof target,"%s/mersave5.sav",overlay);actual=read_file(target,&as);
    assert(as==es&&!memcmp(actual,expected,as));file_equal(source,bytes,size);
    free(bytes);free(actual);free(expected);mercs_campaign_free(&saved);free(v);
    puts("PASS original Windows Load and Save SDL flow: source untouched, native generation and original export retain full0x853 records");
}
static void crew_ui(shell *s,const char *overlay)
{
    uint8_t *before,*after;size_t bs,as;int i;char path[4096];
    assert(!mercs_save_encode(s->campaign.document,0,&before,&bs));
    set_page(s,PAGE_MECHBAY);mouse(s,423,143,0);healthy(s);assert(s->page==PAGE_CREW);frame(s,"crew-original-photo");
    for(i=0;i<16;i++){
        const mercs_ui_hotspot *h=&mercs_crew_hotspots[i];
        mouse(s,(h->x1+h->x2)/2,(h->y1+h->y2)/2,0);healthy(s);
        assert(s->page==PAGE_CREW_BIO&&s->crew_selected==i&&s->body.count&&s->body.expanded[0]);
        while(s->reveal_active)poll_shell(s);
        if(i==0)frame(s,"crew-original-biography");
        if(i==15)frame(s,"crew-final-biography");
        mouse(s,550,450,0);healthy(s);assert(s->page==PAGE_CREW);
    }
    mouse(s,320,20,0);healthy(s);assert(s->page==PAGE_MECHBAY);
    assert(!mercs_save_encode(s->campaign.document,0,&after,&as));assert(as==bs&&!memcmp(before,after,bs));free(before);free(after);
    mouse(s,423,143,0);press(s,SDLK_F5,0);describe(s,0,"ORIGINAL CREW PHOTO");mouse(s,560,410,0);healthy(s);
    assert(s->page==PAGE_CREW);set_page(s,PAGE_MECHBAY);press(s,SDLK_F5,0);choose(s,0);mouse(s,450,410,0);healthy(s);
    assert(s->page==PAGE_CREW);
    /* Original biographies store only their event, not the selected global
     * person. Restore the photo's complete selection context. */
    snprintf(path,sizeof path,"%s/mersave0.sav",overlay);before=read_file(path,&bs);
    put32(before+0x23c3,MERCS_ORIGINAL_CREW_BIO);put32(before+0x23c7,MERCS_ORIGINAL_CREW_BIO);
    put32(before+0x23cb,MERCS_ORIGINAL_CREW_BIO);snprintf(path,sizeof path,"%s/mersave4.sav",overlay);write_file(path,before,bs);
    press(s,SDLK_F5,0);choose(s,4);mouse(s,450,410,0);healthy(s);assert(s->page==PAGE_CREW);
    file_equal(path,before,bs);free(before);mouse(s,320,20,0);healthy(s);assert(s->page==PAGE_MECHBAY);
    puts("PASS original crew route: all16 authored biographies, exit, no career mutation, native/original Save/Load and missing-global parent-context restore");
}
int main(int argc,char **argv)
{
    shell *s=calloc(1,sizeof *s);portcfg cfg;SDL_Event e;
    char path[PORTCFG_PATH],overlay[PORTCFG_PATH],error[384],models[PORTCFG_PATH],textures[PORTCFG_PATH];
    char kind[PORTCFG_PATH],sky[PORTCFG_PATH],fog[PORTCFG_PATH];int i;
    assert(argc==4&&s);captures=argv[3];
    portcfg_defaults(&cfg);assert(!portcfg_resolve(&cfg,NULL,argv[1]));cfg.mode=1;
    s->cfg=&cfg;s->running=1;s->mode=1;s->edition=PORTCFG_ED_DOS;
    s->accepted=-1;s->transition=-1;s->mouse_x=s->mouse_y=-1;
    assert(!portcfg_overlay_dir(&cfg,"retail",overlay,sizeof overlay,1));
    assert(!mercs_store_directory(overlay,s->save_dir,sizeof s->save_dir));
    assert(!SDL_setenv_unsafe("MERCS_INSTALL_DIR",overlay,1)&&!SDL_setenv_unsafe("MERCS_CD_DIR",cfg.cd,1));
    dp_clear_roots();dp_add_roots_from_env();assert(!dp_set_write_root(overlay));
    assert(!dp_resolve("DATABASE.MW2",path,sizeof path)&&!shelldb_open(path,&s->database));
    assert(!portcfg_edition(&cfg,s->edition,models,textures,kind,sky,fog));
    s->archive=prj_open(models,error,sizeof error);assert(s->archive);
    assert(SDL_Init(SDL_INIT_VIDEO));
    s->window=SDL_CreateWindow("Mercs Save/Load verification", 1280, 830, SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);assert(s->window);
    s->renderer=SDL_CreateRenderer(s->window, "software");assert(s->renderer);
    mercs_viewport_init(&s->viewport,s->renderer);s->art=mercs_art_create(&s->database,s->renderer);assert(s->art);
    assert(!mercs_campaign_new(s->archive,1,"NATIVE","TEST COMPANY",&s->campaign,&s->career,error,sizeof error));
    s->accepted=s->campaign.accepted;s->selected=s->campaign.selected;
    strcpy(s->pilot,s->career.pilot);strcpy(s->unit,s->career.unit);while(SDL_PollEvent(&e)){}
    save_load(s,argv[2],overlay);lab_settlement(s);large_fleet_ui(s,argv[2],overlay);windows_ui(s,argv[2],overlay);crew_ui(s,overlay);healthy(s);
    mercs_inventory_view_free(&s->inventory_view);mercs_campaign_free(&s->campaign);mercs_text_free(&s->body);
    mercs_art_snapshot_free(s->modal_background);mercs_art_destroy(s->art);mercs_viewport_reset(&s->viewport);
    for(i=0;i<TEXT_CACHE;i++)SDL_DestroyTexture(s->text[i].texture);
    for(i=0;i<120;i++)SDL_DestroyTexture(s->backgrounds[i]);
    prj_close(s->archive);shelldb_close(&s->database);SDL_DestroyRenderer(s->renderer);SDL_DestroyWindow(s->window);
    dp_clear_roots();SDL_Quit();free(s->panels);free(s);
    printf("PASS: %d actual SDL Save/Load/Lab/fleet events, %d original-art frames\n",actions,frames);return 0;
}
