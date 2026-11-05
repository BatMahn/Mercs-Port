/* Real saved-career orchestration for the input-only playthrough driver.
 * Uses the production shell handlers and their actual mission child. Never
 * synthesizes or edits mission results. All paths must be private fixtures. */
#include "sdl3_test.h"
#include "../tools/mercsshell.c"
#include <assert.h>
#include <strings.h>

static void require_shell(shell *s)
{
    if (s->status[0]) { fprintf(stderr, "playthrough career: %s\n", s->status); exit(1); }
}
static void string_json(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    putchar('"');
    for (; *p; p++) {
        if (*p == '\\' || *p == '"') { putchar('\\'); putchar(*p); }
        else if (*p >= 32 && *p < 127) putchar(*p);
        else printf("\\u%04x", *p);
    }
    putchar('"');
}
static unsigned read32(const uint8_t *p)
{ return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24; }
static unsigned fingerprint(const uint8_t *p, size_t n)
{ unsigned h = 2166136261u; size_t i; for (i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; } return h; }
static void report(shell *s)
{
    int i;
    printf("CAREER {\"campaign\":"); string_json(s->campaign.campaign);
    printf(",\"phase\":%d,\"active\":%d,\"accepted\":%d,\"page\":%d,\"funds\":%d,"
           "\"year\":%d,\"month\":%d,\"owned\":%d,\"selected_mech\":%d,\"last_outcome\":%u,"
           "\"salvage_mechs\":%zu,\"salvage_items\":%zu,\"missions\":[",
           s->campaign.phase, s->campaign.active, s->accepted, s->page, s->career.funds,
           s->career.year, s->career.month, s->career.owned_count, s->career.owned_index,
           s->career.last_msn.outcome,
           mercs_save_list_count(s->campaign.document, MERCS_SAVE_SALVAGE_MECHS),
           mercs_save_list_count(s->campaign.document, MERCS_SAVE_SALVAGE_ITEMS));
    for (i = 0; i < s->career.mission_count; i++) {
        const mercs_shell_mission *m = &s->career.missions[i];
        printf("%s{\"campaign\":", i ? "," : ""); string_json(m->campaign);
        printf(",\"scene\":"); string_json(m->scene); printf(",\"title\":"); string_json(m->title);
        printf(",\"launchable\":%d}", m->launchable);
    }
    printf("],\"owned_rows\":[");
    for (i = 0; i < s->career.owned_count; i++) {
        const mercs_career_mech_pair *p = &s->career.owned[i];
        char name[64]; int j;
        assert(!mercs_career_filename(&p->current,name,sizeof name));
        printf("%s{\"name\":",i ? "," : "");string_json(name);
        { char model[257]; printf(",\"model\":"); string_json(miv_mech_name(p->current.bytes,model)); }
        printf(",\"current_fnv\":\"%08x\",\"reference_fnv\":\"%08x\",\"armor\":[",
               fingerprint(p->current.bytes,sizeof p->current.bytes), fingerprint(p->reference.bytes,sizeof p->reference.bytes));
        for (j = 0; j < 8; j++) printf("%s[%d,%d]",j ? "," : "",(int32_t)read32(p->current.bytes+0x768+18*j),(int32_t)read32(p->current.bytes+0x76c+18*j));
        printf("],\"ammunition_links\":[");
        { int emitted=0; for(j=0;j<25;j++) {
            int32_t current=(int32_t)read32(p->current.bytes+0x28c+4*j);
            int32_t reference=(int32_t)read32(p->reference.bytes+0x28c+4*j);
            if(current!=-1||reference!=-1)printf("%s{\"index\":%d,\"current\":%d,\"reference\":%d}",
                emitted++?",":"",j,current,reference);
        }}
        printf("],\"critical_slots\":[");
        { int emitted=0; for (j=0;j<96;j++) {
            int32_t current=(int32_t)read32(p->current.bytes+0x368+4*j);
            int32_t reference=(int32_t)read32(p->reference.bytes+0x368+4*j);
            if(current||reference)printf("%s{\"location\":%d,\"slot\":%d,\"current\":%d,\"reference\":%d}",
                emitted++?",":"",j/12,j%12,current,reference);
        }}
        printf("],\"ammunition_slots\":[");
        { int emitted=0; for (j=0;j<96;j++) {
            int32_t ref=(int32_t)read32(p->reference.bytes+0x368+4*j);
            if (ref>=10000) printf("%s{\"location\":%d,\"slot\":%d,\"current\":%d,\"reference\":%d}",
                emitted++?",":"",j/12,j%12,(int32_t)read32(p->current.bytes+0x368+4*j),ref);
        }}
        printf("]}");
    }
    printf("],\"salvage_item_rows\":[");
    for (i=0; (size_t)i < mercs_save_list_count(s->campaign.document,MERCS_SAVE_SALVAGE_ITEMS); i++) {
        const uint8_t *p=mercs_save_list_record(s->campaign.document,MERCS_SAVE_SALVAGE_ITEMS,(size_t)i);
        printf("%s{\"id\":%u,\"type\":%u,\"selected\":%u,\"threshold\":%d}",i ? "," : "",read32(p),(unsigned)p[8],(unsigned)p[9],(int32_t)read32(p+0x12));
    }
    printf("],\"shop_mechs\":[");
    for (i=0; (size_t)i < mercs_save_list_count(s->campaign.document,MERCS_SAVE_SHOP_MECHS); i++) {
        const uint8_t *m=mercs_save_list_record(s->campaign.document,MERCS_SAVE_SHOP_MECHS,(size_t)i);
        char model[257]; int32_t price=0;
        assert(!mercs_economy_mech_price(s->campaign.document,1,(size_t)i,&price));
        printf("%s{\"index\":%d,\"model\":",i ? "," : "",i);string_json(miv_mech_name(m,model));
        printf(",\"tons\":%d,\"price\":%d,\"walk\":%.1f}",(int32_t)read32(m+0x200)/100,price,(double)(int32_t)read32(m+0x280)*10.8);
    }
    printf("],\"inventory_items\":[");
    {int side,category,emitted=0;for(side=0;side<2;side++)for(category=0;category<=MERCS_ITEM_AMMO;category++){
        mercs_item_group *groups=NULL;size_t count=0,row;
        assert(!mercs_economy_item_groups(s->campaign.document,side,(enum mercs_item_category)category,&groups,&count));
        for(row=0;row<count;row++)printf("%s{\"shop\":%d,\"category\":%d,\"id\":%d,\"type\":%u,\"count\":%zu,\"price\":%d}",
            emitted++?",":"",side,category,groups[row].id,groups[row].type,groups[row].count,groups[row].price);
        free(groups);
    }}
    printf("],\"owned_weapons\":%zu,\"owned_ammo\":%zu,\"combat_variables\":{"
           "\"unlimited_ammo\":%u,\"invulnerable\":%u,\"splash_damage\":%u,"
           "\"collision_damage\":%u,\"heat_tracking\":%u,\"difficulty\":%u,\"raw_cfg_hex\":\"",
           mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_WEAPONS),mercs_save_list_count(s->campaign.document,MERCS_SAVE_OWNED_AMMO),
           s->options.dif[0],s->options.dif[1],s->options.dif[2],s->options.dif[3],s->options.dif[4],s->options.dif[5]);
    for (i=0; (size_t)i<s->options.dif_size; i++) printf("%02x",s->options.dif[i]);
    printf("\"}}\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    shell *s = calloc(1, sizeof *s);
    portcfg cfg;
    char path[PORTCFG_PATH], overlay[PORTCFG_PATH], error[256];
    char models[PORTCFG_PATH], textures[PORTCFG_PATH], kind[PORTCFG_PATH], sky[PORTCFG_PATH], fog[PORTCFG_PATH];
    const char *command;
    int i, found = 0;
    assert(argc >= 5 && s);
    command = argv[4];
    portcfg_defaults(&cfg); assert(!portcfg_resolve(&cfg, NULL, argv[1])); cfg.mode = 1;
    for (i = 0; i < PORTCFG_ED_COUNT; i++) {
        const char *name = portcfg_mercs_name(i);
        if (name && !strcmp(argv[2], name)) { s->edition = i; found = 1; break; }
    }
    assert(found); cfg.renderer = s->edition;
    s->cfg = &cfg; s->exe = argv[3]; s->running = 1; s->mode = 1;
    s->accepted = -1; s->transition = -1; s->mouse_x = s->mouse_y = -1;
    assert(!portcfg_overlay_dir(&cfg, "retail", path, sizeof path, 1));
    assert(!mercs_store_directory(path, s->save_dir, sizeof s->save_dir));
    assert(!portcfg_overlay_dir(&cfg, portcfg_mercs_overlay(s->edition), overlay, sizeof overlay, 1));
    assert(!setenv("MERCS_INSTALL_DIR", overlay, 1)); assert(!setenv("MERCS_CD_DIR", cfg.cd, 1));
    dp_clear_roots(); dp_add_roots_from_env(); assert(!dp_set_write_root(overlay));
    assert(!dp_resolve("DATABASE.MW2", path, sizeof path)); assert(!shelldb_open(path, &s->database));
    assert(!portcfg_edition(&cfg, s->edition, models, textures, kind, sky, fog));
    s->archive = prj_open(models, error, sizeof error); assert(s->archive);
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));
    s->window = SDL_CreateWindow("Mercs campaign input validation", SW, SH, SDL_WINDOW_HIDDEN); assert(s->window);
    s->renderer = SDL_CreateRenderer(s->window, "software"); assert(s->renderer);
    mercs_viewport_init(&s->viewport, s->renderer);
    s->art = mercs_art_create(&s->database, s->renderer); assert(s->art);
    assert(!mercs_options_load(&s->options, overlay, cfg.cd, error, sizeof error));
    /* The original Combat Variables option skips nonessential room movies.
     * Damage, heat, difficulty and mission rules retain their saved values. */
    mercs_options_set(&s->options, MO_TRANSITIONS, 0);
    assert(!mercs_options_save(&s->options, overlay, error, sizeof error));
    if (!strcmp(command, "new")) {
        set_page(s, PAGE_REGISTER); strcpy(s->pilot, "PLAYTHROUGH"); strcpy(s->unit, "INPUT VALIDATION");
        register_pilot(s); require_shell(s);
    } else {
        load_slot(s, -1, 1); require_shell(s);
        if (!strcmp(command, "accept")) {
            assert(argc == 6);
            set_page(s, PAGE_CONTRACTS); s->selected = -1;
            for (i = 0; i < s->career.mission_count; i++)
                if (!strcasecmp(argv[5], s->career.missions[i].campaign)) s->selected = i;
            assert(s->selected >= 0); prepare_lines(s);
            click(s, (ui_contracts_hotspots[0].x1 + ui_contracts_hotspots[0].x2) / 2,
                     (ui_contracts_hotspots[0].y1 + ui_contracts_hotspots[0].y2) / 2, 1);
        } else if (!strcmp(command, "launch")) {
            launch_mission(s);
        } else if (!strcmp(command, "continue")) {
            continue_debrief(s);
        } else if (!strcmp(command, "repair")) {
            set_page(s, PAGE_COMMAND); open_repair(s); require_shell(s);
            assert(s->repair);
            repair_action(s, MERCS_REPAIR_VIEW_ALL); require_shell(s);
            close_repair(s, 1, PAGE_COMMAND);
        } else if (!strcmp(command, "refit")) {
            /* The original armor control transfers a point from rear to
             * front when the mech already uses its full armor allotment. */
            unsigned before;
            set_page(s, PAGE_MECHOPS); open_mechlab(s); require_shell(s); assert(s->mechlab);
            before=read32(mercs_mechlab_current(s->mechlab)->bytes+0x768+18*2);
            mechlab_action(s, MERCS_MECHLAB_VIEW_LOCATION, 2, 1); require_shell(s);
            mechlab_action(s, MERCS_MECHLAB_VIEW_ADJUST, MERCS_MECHLAB_FRONT_ARMOR*2+1, 1); require_shell(s);
            assert(read32(mercs_mechlab_current(s->mechlab)->bytes+0x768+18*2)==before+1);
            close_mechlab(s, 1, PAGE_MECHOPS); require_shell(s); assert(!s->mechlab);
        } else if (!strcmp(command, "armor-max")) {
            /* Ordinary Armor panel controls only. Keep every existing rear
             * point, increase capacity, then fill front-location deficits. */
            int location, point, weight_clicks=0, added=0, cap=0;
            int before_front[8], before_rear[8], maximum[8];
            int32_t quote;
            size_t row;
            const uint8_t *m;
            assert(argc==5);
            set_page(s,PAGE_MECHOPS);open_mechlab(s);require_shell(s);assert(s->mechlab);
            mechlab_action(s,MERCS_MECHLAB_VIEW_CATEGORY,MERCS_ITEM_EQUIPMENT,1);require_shell(s);
            for(row=0;row<s->mechlab_view.count;row++)
                if(s->mechlab_view.groups[row].id==9&&s->mechlab_view.groups[row].type==2)break;
            assert(row<s->mechlab_view.count);
            mechlab_action(s,MERCS_MECHLAB_VIEW_SELECT,(int)row,2);require_shell(s);
            assert(s->mechlab_view.panel==2);
            m=mercs_mechlab_current(s->mechlab)->bytes;
            for(location=0;location<8;location++) {
                before_front[location]=(int32_t)read32(m+0x768+18*location);
                before_rear[location]=(int32_t)read32(m+0x76c+18*location);
                maximum[location]=(int32_t)read32(m+0x764+18*location);
                assert(before_front[location]>=0&&maximum[location]>=before_front[location]);
                cap+=maximum[location];
            }
            while((int32_t)read32(mercs_mechlab_current(s->mechlab)->bytes+0x258)<cap) {
                assert(weight_clicks++<1000);
                mechlab_action(s,MERCS_MECHLAB_VIEW_ADJUST,MERCS_MECHLAB_ARMOR_WEIGHT*2+1,1);
                require_shell(s);
                m=mercs_mechlab_current(s->mechlab)->bytes;
                for(location=0;location<8;location++) {
                    assert((int32_t)read32(m+0x768+18*location)==before_front[location]);
                    assert((int32_t)read32(m+0x76c+18*location)==before_rear[location]);
                }
            }
            for(location=0;location<8;location++) {
                int rear=before_rear[location]>0?before_rear[location]:0;
                int need=maximum[location]-before_front[location]-rear;
                assert(need>=0);
                mechlab_action(s,MERCS_MECHLAB_VIEW_LOCATION,location,1);require_shell(s);
                for(point=0;point<need;point++) {
                    mechlab_action(s,MERCS_MECHLAB_VIEW_ADJUST,MERCS_MECHLAB_FRONT_ARMOR*2+1,1);
                    require_shell(s);
                    m=mercs_mechlab_current(s->mechlab)->bytes;
                    assert((int32_t)read32(m+0x768+18*location)==before_front[location]+point+1);
                    assert((int32_t)read32(m+0x76c+18*location)==before_rear[location]);
                    added++;
                }
            }
            quote=mercs_mechlab_cost(s->mechlab);
            printf("ARMOR_CONTROLS weight_clicks=%d front_points=%d quoted_cost=%d\n",weight_clicks,added,(int)quote);
            close_mechlab(s,1,PAGE_MECHOPS);require_shell(s);assert(!s->mechlab);
        } else if (!strcmp(command,"install") || !strcmp(command,"remove-component")) {
            int type,id,location,slot,category,found_item=0;size_t row;
            assert(argc==6);
            set_page(s,PAGE_MECHOPS);open_mechlab(s);require_shell(s);assert(s->mechlab);
            if(!strcmp(command,"install")) {
                assert(sscanf(argv[5],"%d,%d,%d",&type,&id,&location)==3);
                assert(type>=0&&type<=2&&location>=0&&location<8);
                mechlab_action(s,MERCS_MECHLAB_VIEW_LOCATION,location,1);require_shell(s);
                for(category=0;category<=MERCS_ITEM_AMMO;category++) {
                    mechlab_action(s,MERCS_MECHLAB_VIEW_CATEGORY,category,1);require_shell(s);
                    for(row=0;row<s->mechlab_view.count;row++)
                        if(s->mechlab_view.groups[row].id==id&&(int)s->mechlab_view.groups[row].type==type)break;
                    if(row==s->mechlab_view.count)continue;
                    mechlab_action(s,MERCS_MECHLAB_VIEW_SELECT,(int)row,2);require_shell(s);
                    found_item=1;break;
                }
                assert(found_item);
            } else {
                assert(sscanf(argv[5],"%d,%d",&location,&slot)==2);
                assert(location>=0&&location<8&&slot>=0&&slot<12);
                mechlab_action(s,MERCS_MECHLAB_VIEW_LOCATION,location,1);require_shell(s);
                mechlab_action(s,MERCS_MECHLAB_VIEW_CRITICAL,slot,2);require_shell(s);
            }
            close_mechlab(s,1,PAGE_MECHOPS);require_shell(s);assert(!s->mechlab);
        } else if ((!strcmp(command, "buy") || !strcmp(command, "sell"))) {
            SDL_Event yes; int index, buy=!strcmp(command,"buy");
            assert(argc==6); index=atoi(argv[5]);
            assert(index>=0 && (size_t)index<mercs_save_list_count(s->campaign.document,buy?MERCS_SAVE_SHOP_MECHS:MERCS_SAVE_OWNED_MECHS));
            set_page(s,PAGE_FACTORY); s->factory_view.side=buy; s->factory_view.selected=(size_t)index;
            SDL_zero(yes);yes.type=SDL_EVENT_KEY_DOWN;yes.key.key=SDLK_Y;
            assert(SDL_PushEvent(&yes)); economy_transaction(s,buy);
        } else if (!strcmp(command, "buy-ammo") || !strcmp(command,"buy-weapon") ||
                   !strcmp(command,"buy-equipment")) {
            int weapon,category,first_category,last_category,found_item=0;size_t row;
            assert(argc==6);weapon=atoi(argv[5]);
            set_page(s,PAGE_TRADING);
            first_category = !strcmp(command,"buy-ammo") ? MERCS_ITEM_AMMO :
                             !strcmp(command,"buy-equipment") ? MERCS_ITEM_EQUIPMENT : MERCS_ITEM_MISSILE;
            last_category = !strcmp(command,"buy-weapon") ? MERCS_ITEM_PROJECTILE : first_category;
            for(category=first_category;category<=last_category;category++) {
                assert(!mercs_trading_view_prepare(&s->trading_view,s->campaign.document,category));
                for(row=0;row<s->trading_view.count[1];row++)
                    if(s->trading_view.groups[1][row].id==weapon)break;
                if(row==s->trading_view.count[1])continue;
                assert(!mercs_trading_view_select(&s->trading_view,1,row));
                economy_transaction(s,1);found_item=1;break;
            }
            assert(found_item);
        } else if (!strcmp(command, "rearm")) {
            int location,slot;
            set_page(s,PAGE_COMMAND);open_repair(s);require_shell(s);assert(s->repair);
            for(location=0;location<8;location++)for(slot=0;slot<12;slot++) {
                size_t offset=0x368u+4u*(size_t)(location*12+slot);
                int32_t reference=(int32_t)read32(mercs_repair_reference(s->repair)->bytes+offset);
                int32_t current=(int32_t)read32(mercs_repair_current(s->repair)->bytes+offset);
                if(reference<10000||reference==current)continue;
                s->repair_view.location=location;s->repair_view.part=MERCS_REPAIR_CRITICAL;s->repair_view.slot=slot;
                repair_action(s,MERCS_REPAIR_VIEW_SELECTED);require_shell(s);
            }
            close_repair(s,1,PAGE_COMMAND);
        } else if (!strcmp(command, "assign")) {
            int index,attempt;uint32_t wanted;
            assert(argc==6);index=atoi(argv[5]);assert(index>=0 && index<s->career.owned_count);
            wanted=mercs_career_instance_id(&s->career.owned[index].current);
            set_page(s,PAGE_ROSTER);s->roster_view.selected=0;
            for(attempt=0;attempt<=s->career.owned_count;attempt++) {
                const uint8_t *pilot=mercs_save_list_record(s->campaign.document,MERCS_SAVE_OWNED_PILOTS,0);
                assert(pilot);if(read32(pilot+0x20)==wanted)break;
                roster_change(s,1,0);require_shell(s);
            }
            assert(attempt<=s->career.owned_count);
        } else if (!strcmp(command, "save")) {
            assert(argc == 6); open_save_load(s); s->save_slot = atoi(argv[5]);
            assert(s->save_slot >= 0 && s->save_slot < 10);
            snprintf(s->save_titles[s->save_slot], sizeof s->save_titles[s->save_slot], "INPUT PLAYTHROUGH %d", s->save_slot);
            save_slot(s);
        } else if (!strcmp(command, "load")) {
            assert(argc == 6); load_slot(s, atoi(argv[5]), 0);
        } else assert(!strcmp(command, "status"));
        require_shell(s);
    }
    report(s);
    mercs_campaign_free(&s->campaign); mercs_art_destroy(s->art); prj_close(s->archive);
    shelldb_close(&s->database); SDL_DestroyRenderer(s->renderer); SDL_DestroyWindow(s->window);
    SDL_Quit(); free(s); return 0;
}
