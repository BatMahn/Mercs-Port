#include "sdl3_test.h"
/* Real shell events, original game assets and durable career transactions.
 * Supply a PRIVATE writable game folder. Screenshots stay outside source.
 * SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy on headless Linux. */
#include <SDL3/SDL.h>
#include "../tools/mercsshell.c"
#include <assert.h>

static const char *captures;
static int frames, actions;
static void healthy(shell *s)
{
    if (!s->running || s->status[0]) {
        fprintf(stderr,"FAIL economy UI: running=%d status=%s SDL=%s\n",s->running,s->status,SDL_GetError());
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
static void hotspot(shell *s,const mercs_ui_hotspot *h)
{ mouse(s,(h->x1+h->x2)/2,(h->y1+h->y2)/2,0); }

static void finance_inventory(shell *s)
{
    int i;
    set_page(s,PAGE_COMPUTER);hotspot(s,&ui_computer_hotspots[3]);healthy(s);
    assert(s->page==PAGE_FINANCE&&s->finance_view.text.count>0);frame(s,"economy-office-finance");
    mouse(s,550,450,0);assert(s->page==PAGE_COMPUTER);
    hotspot(s,&ui_computer_hotspots[2]);assert(s->page==PAGE_INVENTORY);frame(s,"economy-office-inventory-mechs");
    for(i=1;i<6;i++){
        char name[80];mouse(s,70+90*i,55,0);healthy(s);assert(s->inventory_view.category==i);
        snprintf(name,sizeof name,"economy-office-inventory-%d",i);frame(s,name);
    }
    mouse(s,550,450,0);assert(s->page==PAGE_COMPUTER);
    set_page(s,PAGE_COMMAND);mouse(s,100,50,0);assert(s->page==PAGE_DROP_FINANCE);frame(s,"economy-dropship-finance");
    mouse(s,550,450,0);assert(s->page==PAGE_COMMAND);
    mouse(s,100,145,0);assert(s->page==PAGE_DROP_INVENTORY);frame(s,"economy-dropship-inventory-mechs");
    mouse(s,247,55,0);assert(s->inventory_view.category==2);frame(s,"economy-dropship-inventory-energy");
    mouse(s,550,450,0);assert(s->page==PAGE_COMMAND);
}
static void personnel(shell *s)
{
    size_t before,available;int32_t funds=s->career.funds;uint8_t hired[32];
    set_page(s,PAGE_COMPUTER);hotspot(s,&ui_computer_hotspots[1]);assert(s->page==PAGE_PERSONNEL);
    before=mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_PILOTS);
    available=mercs_save_list_count(s->campaign.document,MERCS_SAVE_SHOP_PILOTS);assert(available);
    memcpy(hired,mercs_save_list_record(s->campaign.document,MERCS_SAVE_SHOP_PILOTS,0),32);
    mouse(s,450,117,0);healthy(s);assert(s->personnel_view.list==1&&s->personnel_view.index==0);
    frame(s,"economy-personnel-biography");mouse(s,525,395,0);healthy(s);
    assert(mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_PILOTS)==before+1);
    assert(mercs_save_list_count(s->campaign.document,MERCS_SAVE_SHOP_PILOTS)==available-1);
    assert(!memcmp(hired,mercs_save_list_record(s->campaign.document,MERCS_SAVE_OWNED_PILOTS,before),32));
    assert(s->career.funds==funds);frame(s,"economy-personnel-hired");
    mouse(s,240,117,0);assert(s->personnel_view.list==0);
    mouse(s,305,395,0);healthy(s);
    assert(mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_PILOTS)==before);
    assert(mercs_save_list_count(s->campaign.document,MERCS_SAVE_SHOP_PILOTS)==available);
    assert(s->career.funds==funds);mouse(s,550,450,0);assert(s->page==PAGE_COMPUTER);
}
static void trading(shell *s)
{
    int category,id;size_t owned,shop,group;int32_t funds,price,value;
    mercs_campaign saved={0};mercs_shell_state *view=calloc(1,sizeof *view);
    char error[256],directory[PORTCFG_PATH];assert(view);
    set_page(s,PAGE_OFFICE);hotspot(s,&ui_office_hotspots[1]);assert(s->page==PAGE_MECHBAY);
    frame(s,"economy-mech-bay");mouse(s,40,260,0);assert(s->page==PAGE_MECHOPS);frame(s,"economy-mech-ops");
    mouse(s,80,50,0);assert(s->page==PAGE_TRADING);
    for(category=0;category<5;category++){
        mouse(s,20+90*category,25,0);healthy(s);
        if(s->trading_view.count[1])break;
    }
    assert(category<5);frame(s,"economy-equipment-shop");
    assert(category<4); /* Authored BET1 has stock weapons, no ammunition. */
    owned=mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_WEAPONS);
    shop=mercs_save_list_count(s->campaign.document,MERCS_SAVE_SHOP_WEAPONS);
    id=s->trading_view.groups[1][0].id;price=s->trading_view.groups[1][0].price;funds=s->career.funds;
    value=mev_i32(mercs_save_list_record(s->campaign.document,MERCS_SAVE_SHOP_WEAPONS,
                         s->trading_view.groups[1][0].first_index)+4);
    mouse(s,260,143,0);assert(s->trading_view.side==1);mouse(s,330,395,0);healthy(s);
    assert(s->career.funds==funds-price);
    assert(mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_WEAPONS)==owned+1);
    assert(mercs_save_list_count(s->campaign.document,MERCS_SAVE_SHOP_WEAPONS)==shop-1);
    assert(!mercs_store_read(s->save_dir,-1,s->archive,&saved,view,error,sizeof error));
    assert(view->funds==s->career.funds&&mercs_save_list_count(saved.document,MERCS_SAVE_OWNED_WEAPONS)==owned+1);
    mercs_campaign_free(&saved);frame(s,"economy-equipment-bought");
    for(group=0;group<s->trading_view.count[0];group++)if(s->trading_view.groups[0][group].id==id)break;
    assert(group<s->trading_view.count[0]&&group<20);
    mouse(s,470,143+(int)group*10,0);assert(s->trading_view.side==0);mouse(s,530,395,0);healthy(s);
    assert(s->career.funds==funds-price+value);
    assert(mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_WEAPONS)==owned);
    /* An invalid save directory must not install the staged buy or charge. */
    mouse(s,260,143,0);funds=s->career.funds;strcpy(directory,s->save_dir);
    assert(strlen(directory)+sizeof "/not-a-save-directory"<=sizeof s->save_dir);
    strcpy(s->save_dir,directory);strcat(s->save_dir,"/not-a-save-directory");
    mouse(s,330,395,0);assert(s->status[0]&&s->career.funds==funds);
    assert(mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_WEAPONS)==owned);
    strcpy(s->save_dir,directory);s->status[0]=0;
    mouse(s,550,450,0);assert(s->page==PAGE_MECHOPS);
    mouse(s,80,145,0);assert(s->page==PAGE_FACTORY);
    frame(s,"economy-mech-factory");
    mouse(s,100,160,0);assert(s->factory_view.details);frame(s,"economy-mech-factory-details");
    { SDL_Event yes;size_t n=mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_MECHS);int32_t sold,bought;
      assert(n==1);assert(!mercs_economy_mech_price(s->campaign.document,0,0,&sold));funds=s->career.funds;
      mouse(s,470,97,0);assert(s->factory_view.side==0);SDL_zero(yes);yes.type=SDL_EVENT_KEY_DOWN;yes.key.key=SDLK_Y;
      assert(SDL_PushEvent(&yes)==1);mouse(s,530,395,0);healthy(s);
      assert(s->career.funds==funds+sold&&mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_MECHS)==0);
      assert(mev_u32(mercs_save_list_record(s->campaign.document,MERCS_SAVE_OWNED_PILOTS,0)+0x20)==UINT32_MAX);
      assert(!mercs_economy_mech_price(s->campaign.document,1,0,&bought));
      mouse(s,270,97,0);assert(s->factory_view.side==1);assert(SDL_PushEvent(&yes)==1);mouse(s,330,395,0);healthy(s);
      assert(s->career.funds==funds+sold-bought&&mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_MECHS)==1);
      assert(mercs_save_list_count(s->campaign.document,MERCS_SAVE_SHOP_MECHS)==mercs_save_list_count(s->campaign.document,MERCS_SAVE_SHOP_REFERENCES));
      assert(!mercs_store_read(s->save_dir,-1,s->archive,&saved,view,error,sizeof error));
      assert(view->funds==s->career.funds&&mercs_save_list_count(saved.document,MERCS_SAVE_OWNED_MECHS)==1);
      mercs_campaign_free(&saved);frame(s,"economy-mech-factory-purchased");
    }
    mouse(s,550,450,0);assert(s->page==PAGE_MECHOPS);
    mouse(s,80,330,0);assert(s->page==PAGE_REPAIR&&s->repair_return==PAGE_MECHOPS);
    close_repair(s,0,PAGE_MECHOPS);healthy(s);assert(!s->repair&&s->page==PAGE_MECHOPS);
    mouse(s,560,450,0);assert(s->page==PAGE_MECHBAY);
    mouse(s,400,450,0);assert(s->page==PAGE_OFFICE);free(view);
}

static int weapon_groups(shell *s)
{
    mercs_campaign saved={0};mercs_shell_state *view=calloc(1,sizeof *view);
    const uint8_t *m;uint8_t before[MERCS_CAREER_MECH_BYTES];uint32_t link[14];
    char error[384],directory[PORTCFG_PATH];int i,slot=-1,top;
    assert(view);set_page(s,PAGE_MECHOPS);mouse(s,80,425,0);healthy(s);assert(s->page==PAGE_GROUPS);
    frame(s,"economy-weapon-groups");
    m=mercs_save_list_record(s->campaign.document,MERCS_SAVE_OWNED_MECHS,0);
    for(i=0;i<14;i++){link[i]=mev_u32(m+0x2f4+8*i);if(slot<0&&mev_i32(m+0x2f0+8*i)>=0)slot=i;}
    assert(slot>=0);top=mgv_top(s->campaign.document,mercs_art_height(s->art,10));
    mouse(s,430,450,0);healthy(s); /* Authored Default button. */
    m=mercs_save_list_record(s->campaign.document,MERCS_SAVE_OWNED_MECHS,0);
    for(i=0;i<14;i++)assert(mev_u32(m+0x2f4+8*i)==(link[i]&~0xf00u));
    for(i=1;i<=5;i++){
        mouse(s,450,top+2,0);healthy(s);
        assert(((mev_u32(mercs_save_list_record(s->campaign.document,MERCS_SAVE_OWNED_MECHS,0)+0x2f4+8*slot)>>8)&15)==(unsigned)(i%5));
    }
    /* Failed persistence cannot edit the live loadout. */
    memcpy(before,mercs_save_list_record(s->campaign.document,MERCS_SAVE_OWNED_MECHS,0),sizeof before);
    strcpy(directory,s->save_dir);assert(strlen(directory)+sizeof "/not-a-save-directory"<=sizeof s->save_dir);
    strcat(s->save_dir,"/not-a-save-directory");mouse(s,450,top+2,0);assert(s->status[0]);
    assert(!memcmp(before,mercs_save_list_record(s->campaign.document,MERCS_SAVE_OWNED_MECHS,0),sizeof before));
    strcpy(s->save_dir,directory);s->status[0]=0;
    for(i=0;i<4;i++){mouse(s,450,top+2,0);healthy(s);}
    frame(s,"economy-weapon-group-four");
    mouse(s,200,370,0);assert(s->groups_view.selected==1);frame(s,"economy-weapon-groups-next-mech");
    mouse(s,100,370,0);assert(s->groups_view.selected==0);
    assert(!mercs_store_read(s->save_dir,-1,s->archive,&saved,view,error,sizeof error));
    assert(((mev_u32(mercs_save_list_record(saved.document,MERCS_SAVE_OWNED_MECHS,0)+0x2f4+8*slot)>>8)&15)==4);
    mercs_campaign_free(&saved);free(view);
    mouse(s,550,450,0);healthy(s);assert(s->page==PAGE_MECHOPS);set_page(s,PAGE_ROSTER);return slot;
}

static void full_roster(shell *s)
{
    mercs_campaign saved={0};mercs_shell_state *view=calloc(1,sizeof *view);
    mercs_exchange_launch launch={0};char error[384],overlay[PORTCFG_PATH],directory[PORTCFG_PATH];
    uint8_t *h,*before,*after;size_t before_size,after_size;uint32_t ids[3];mercs_career_mech refs[3];int i,group_slot;
    assert(view);mercs_campaign_free(&s->campaign);
    assert(!mercs_campaign_new(s->archive,1,"ROSTER","TEST COMPANY",&s->campaign,&s->career,error,sizeof error));
    h=mercs_save_header_edit(s->campaign.document);
    {uint32_t funds=100000000;for(i=0;i<4;i++)h[0x12a+i]=(uint8_t)(funds>>(8*i));h[0x2df7]=3;h[0x2df8]=h[0x2df9]=h[0x2dfa]=0;}
    { uint8_t two[2*MERCS_SAVE_PILOT_BYTES];
      const uint8_t *p=mercs_save_list_record(s->campaign.document,MERCS_SAVE_SHOP_PILOTS,0);assert(p);
      memcpy(two,p,MERCS_SAVE_PILOT_BYTES);memcpy(two+MERCS_SAVE_PILOT_BYTES,p,MERCS_SAVE_PILOT_BYTES);
      strcpy((char*)two+MERCS_SAVE_PILOT_BYTES,"SECOND TEST PILOT");
      assert(!mercs_save_set_list(s->campaign.document,MERCS_SAVE_SHOP_PILOTS,two,2));
    }
    /* Explicit generous test contract: actual stock and pilot records, with
     * three allowed starmates. The assignment UI uses the real SDL path. */
    for(i=0;i<2;i++){
        assert(!mercs_economy_hire(s->campaign.document,MERCS_PERSONNEL_MECHWARRIOR,0));
        assert(!mercs_economy_trade_mech(s->campaign.document,1,0));
    }
    assert(!mercs_campaign_hydrate(s->archive,&s->campaign,&s->career,error,sizeof error));
    s->accepted=s->campaign.accepted;s->selected=s->campaign.selected;
    for(i=0;i<3;i++){ids[i]=mercs_career_instance_id(&s->career.owned[i].current);refs[i]=s->career.owned[i].reference;}
    set_page(s,PAGE_ROSTER);frame(s,"economy-duty-roster-unassigned");
    for(i=1;i<3;i++){
        mouse(s,200,121+i*10,0);assert(s->roster_view.selected==(size_t)i);
        mouse(s,570,310,0);healthy(s);
        assert(mev_u32(mercs_save_list_record(s->campaign.document,MERCS_SAVE_OWNED_PILOTS,(size_t)i)+0x20)==ids[i]);
    }
    frame(s,"economy-duty-roster-three-pilots");
    mouse(s,200,117,0);assert(s->roster_view.selected==0);mouse(s,570,310,0);healthy(s);assert(s->career.owned_index==-1);
    mouse(s,570,310,0);healthy(s);assert(s->career.owned_index==0);
    for(i=0;i<3;i++){assert(mercs_career_instance_id(&s->career.owned[i].current)==ids[i]);assert(!memcmp(&refs[i],&s->career.owned[i].reference,sizeof refs[i]));}
    assert(!mercs_store_read(s->save_dir,-1,s->archive,&saved,view,error,sizeof error));
    assert(view->owned_count==3&&view->owned_index==0);mercs_campaign_free(&saved);
    assert(!mercs_save_encode(s->campaign.document,0,&before,&before_size));
    strcpy(directory,s->save_dir);assert(strlen(directory)+sizeof "/not-a-save-directory"<=sizeof s->save_dir);
    strcat(s->save_dir,"/not-a-save-directory");mouse(s,200,131,0);mouse(s,470,310,0);assert(s->status[0]);
    assert(!mercs_save_encode(s->campaign.document,0,&after,&after_size));assert(before_size==after_size&&!memcmp(before,after,before_size));
    free(before);free(after);strcpy(s->save_dir,directory);s->status[0]=0;
    group_slot=weapon_groups(s);
    for(i=0;i<3;i++){assert(mercs_career_instance_id(&s->career.owned[i].current)==ids[i]);assert(!memcmp(&refs[i],&s->career.owned[i].reference,sizeof refs[i]));}
    assert(!portcfg_overlay_dir(s->cfg,"retail",overlay,sizeof overlay,0));s->campaign.accepted=1;
    assert(!prepare_roster_launch(s,&s->campaign,&s->career,overlay,&launch));assert(launch.unit_count==3);
    {uint8_t *bytes;size_t size;mek_def m;char file[32];int found=0;
     int id=mev_i32(s->career.owned[0].current.bytes+0x2f0+8*group_slot);
     snprintf(file,sizeof file,"MEK/%.8s.MEK",launch.units[0].name);
     assert(!dp_read_file(file,&bytes,&size)&&!mek_parse(bytes,size,&m));free(bytes);
     for(i=0;i<m.item_count;i++)if(m.items[i].id==(unsigned)id&&((m.items[i].linked_id>>8)&15)==4)found++;
     assert(found==1);}
    for(i=0;i<3;i++)assert(mercs_career_deployed(&s->career.owned[i].current)==1);
    frame(s,"economy-duty-roster-ready");mouse(s,550,450,0);healthy(s);assert(s->page==PAGE_COMMAND);
    free(view);
}

static void disband_after_sale(shell *s)
{
    mercs_campaign saved={0};mercs_shell_state *view=calloc(1,sizeof *view);
    mercs_lords_table before,after;SDL_Event e;char error[384],directory[PORTCFG_PATH],overlay[PORTCFG_PATH];
    assert(view);mercs_campaign_free(&s->campaign);
    assert(!mercs_campaign_new(s->archive,1,"DISBAND","TEST COMPANY",&s->campaign,&s->career,error,sizeof error));
    {uint8_t *p=mercs_save_header_edit(s->campaign.document)+0x12a;uint32_t value=(uint32_t)-100000000;int i;
     for(i=0;i<4;i++)p[i]=(uint8_t)(value>>(8*i));}
    assert(!mercs_campaign_hydrate(s->archive,&s->campaign,&s->career,error,sizeof error));
    s->accepted=s->campaign.accepted;s->selected=s->campaign.selected;
    assert(!portcfg_overlay_dir(s->cfg,"retail",overlay,sizeof overlay,0));
    set_page(s,PAGE_FACTORY);mouse(s,470,97,0);assert(s->factory_view.side==0);
    strcpy(directory,s->save_dir);assert(strlen(directory)+sizeof "/not-a-save-directory"<=sizeof s->save_dir);
    strcat(s->save_dir,"/not-a-save-directory");
    SDL_zero(e);e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_Y;assert(SDL_PushEvent(&e)==1);
    mouse(s,530,395,0);assert(s->status[0]);
    assert(s->campaign.phase==MERCS_CAMPAIGN_READY&&s->career.owned_count==1&&s->career.funds==-100000000);
    assert(s->panels&&!mercs_scores_load(overlay,NULL,&s->panels->defaults,&before,error,sizeof error));
    strcpy(s->save_dir,directory);s->status[0]=0;
    /* Confirm sale, acknowledge the original disband notice, skip the movie. */
    e.key.key=SDLK_Y;assert(SDL_PushEvent(&e)==1);
    e.key.key=SDLK_RETURN;assert(SDL_PushEvent(&e)==1);
    e.key.key=SDLK_ESCAPE;assert(SDL_PushEvent(&e)==1);
    mouse(s,530,395,0);healthy(s);
    assert(s->campaign.phase==MERCS_CAMPAIGN_ENDED&&s->page==PAGE_MENU&&s->career.owned_count==0);
    assert(!mercs_store_read(s->save_dir,-1,s->archive,&saved,view,error,sizeof error));
    assert(saved.phase==MERCS_CAMPAIGN_ENDED&&!saved.active&&view->owned_count==0);
    assert(!mercs_scores_load(overlay,NULL,&s->panels->defaults,&after,error,sizeof error));
    assert(!memcmp(&before,&after,sizeof before));
    frame(s,"economy-disband-menu");mercs_campaign_free(&saved);free(view);
}

int main(int argc,char **argv)
{
    shell *s=calloc(1,sizeof *s);portcfg cfg;SDL_Event e;
    char path[PORTCFG_PATH],error[384],models[PORTCFG_PATH],textures[PORTCFG_PATH];
    char kind[PORTCFG_PATH],sky[PORTCFG_PATH],fog[PORTCFG_PATH];int i;
    assert(argc>=2&&s);captures=argc>2?argv[2]:NULL;
    portcfg_defaults(&cfg);assert(!portcfg_resolve(&cfg,NULL,argv[1]));cfg.mode=1;
    s->cfg=&cfg;s->running=1;s->mode=1;s->edition=PORTCFG_ED_DOS;
    s->accepted=-1;s->transition=-1;s->mouse_x=s->mouse_y=-1;
    assert(!portcfg_overlay_dir(&cfg,"retail",path,sizeof path,1));
    assert(!mercs_store_directory(path,s->save_dir,sizeof s->save_dir));
    assert(!SDL_setenv_unsafe("MERCS_INSTALL_DIR",path,1)&&!SDL_setenv_unsafe("MERCS_CD_DIR",cfg.cd,1));
    dp_clear_roots();dp_add_roots_from_env();assert(!dp_set_write_root(path));
    assert(!dp_resolve("DATABASE.MW2",path,sizeof path)&&!shelldb_open(path,&s->database));
    assert(!portcfg_edition(&cfg,s->edition,models,textures,kind,sky,fog));
    s->archive=prj_open(models,error,sizeof error);assert(s->archive);
    assert(SDL_Init(SDL_INIT_VIDEO));
    s->window=SDL_CreateWindow("Mercs economy verification", 1280, 830, SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);assert(s->window);
    s->renderer=SDL_CreateRenderer(s->window, "software");assert(s->renderer);
    mercs_viewport_init(&s->viewport,s->renderer);s->art=mercs_art_create(&s->database,s->renderer);assert(s->art);
    assert(!mercs_campaign_new(s->archive,1,"TRADER","TEST COMPANY",&s->campaign,&s->career,error,sizeof error));
    s->accepted=s->campaign.accepted;s->selected=s->campaign.selected;
    strcpy(s->pilot,s->career.pilot);strcpy(s->unit,s->career.unit);
    while(SDL_PollEvent(&e)){}
    finance_inventory(s);personnel(s);trading(s);full_roster(s);disband_after_sale(s);healthy(s);
    mercs_finance_view_free(&s->finance_view);mercs_personnel_view_free(&s->personnel_view);
    mercs_trading_view_free(&s->trading_view);mercs_inventory_view_free(&s->inventory_view);
    mercs_campaign_free(&s->campaign);mercs_text_free(&s->body);mercs_art_destroy(s->art);
    mercs_viewport_reset(&s->viewport);
    for(i=0;i<TEXT_CACHE;i++)SDL_DestroyTexture(s->text[i].texture);
    for(i=0;i<120;i++)SDL_DestroyTexture(s->backgrounds[i]);
    prj_close(s->archive);shelldb_close(&s->database);
    SDL_DestroyRenderer(s->renderer);SDL_DestroyWindow(s->window);dp_clear_roots();SDL_Quit();free(s->panels);free(s);
    printf("PASS: %d real shell economy events and %d original-art frames; office/DropShip finance/inventory, hire/dismiss, Mech Bay, trading, full Duty Roster and Weapon Groups through deployment; original sale/disband ending with idempotent score, durable reload and failed-write rollback.\n",actions,frames);
    return 0;
}
