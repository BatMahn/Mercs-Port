#include "sdl3_test.h"
/* Controlled frontend integration using the actual static shell handlers and
 * supplied original art/resources. Simulation end states are explicit test
 * inputs, not a claim of autonomous mission completion or platform parity. */
#include "../tools/mercsshell.c"
#include "mercsbridge.h"
#include <assert.h>
#include <strings.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

static void frontend_ok(shell *s)
{
    if (s->status[0]) { fprintf(stderr, "frontend check: %s\n", s->status); abort(); }
}
static void frame(shell *s, const char *directory, const char *name)
{
    char path[4096];
    SDL_Surface *surface;
    int remaining = s->reveal_total + 1;
    while (s->reveal_active && remaining-- > 0) poll_shell(s);
    assert(!s->reveal_active);
    draw(s); frontend_ok(s);
    surface = SDL_CreateSurface(SW, SH, SDL_PIXELFORMAT_ARGB8888); assert(surface);
    assert(!mercs_test_read_pixels(s->renderer, NULL, surface->format, surface->pixels, surface->pitch));
    snprintf(path, sizeof path, "%s/%s.bmp", directory, name);
    assert(SDL_SaveBMP(surface, path)); SDL_DestroySurface(surface);
}
static void press_original(shell *s, const mercs_ui_hotspot *h)
{ click(s, (h->x1+h->x2)/2, (h->y1+h->y2)/2, 1); frontend_ok(s); }
static int mission_index(shell *s, const char *name)
{
    int i;
    for (i = 0; i < s->career.mission_count; i++) if (!strcmp(s->career.missions[i].campaign, name)) return i;
    return -1;
}
static int32_t click_head_armor_repair(shell *s)
{
    int rows[25], heights[25];
    assert(s->repair && s->repair_view.location == 0);
    mercs_repair_view_rows(s->art, mercs_repair_reference(s->repair), 0, rows, heights);
    click(s, 260, rows[7] + heights[7] / 2, 1);
    assert(s->repair_view.part == MERCS_REPAIR_ARMOR);
    click(s, 320, 395, 1); frontend_ok(s);
    return s->repair_view.owed;
}
static void make_pending_return(shell *s, char token[MERCS_EXCHANGE_TOKEN])
{
    mercs_campaign next;
    mercs_shell_state *view;
    mercs_exchange_launch launch = {0};
    msim *sim = calloc(1, sizeof *sim);
    world_actor *actors = NULL;
    char overlay[PORTCFG_PATH];
    int i, count;
    assert(sim);
    assert(!portcfg_overlay_dir(s->cfg, portcfg_mercs_overlay(s->edition), overlay, sizeof overlay, 1));
    assert(!dp_set_write_root(overlay)); assert(!dp_add_root(overlay));
    assert(!stage_career(s, &next, &view));
    assert(!mercs_shell_prepare_launch(s->archive, view, next.accepted, overlay, s->status, sizeof s->status));
    snprintf(launch.scene, sizeof launch.scene, "%s", view->missions[next.accepted].scene);
    snprintf(launch.edition, sizeof launch.edition, "%s", portcfg_mercs_name(s->edition));
    launch.unit_count = 1; launch.units[0].actor_index = -1; launch.units[0].controller_flags = 2;
    snprintf(launch.units[0].name, sizeof launch.units[0].name, "%s", view->starter_loadout);
    assert(!mercs_exchange_begin(overlay, &launch));
    next.phase = MERCS_CAMPAIGN_DEPLOYED;
    strcpy(next.launch_token, launch.token); strcpy(next.launch_edition, launch.edition);
    assert(!commit_career(s, &next, view, PAGE_BRIEFING)); free(view);
    assert(!world3d_actors(s->archive, launch.scene, &actors, &count));
    assert(!msim_init(sim, s->archive, launch.scene, actors, count));
    assert(sim->player_unit.return_valid && sim->player_table == 0);
    sim->player_unit.armor[MEK_HEAD] = 8.0f; /* original player divisor4 ->2 */
    sim->logic.result[sim->player_table] = 2;
    sim->logic.result_time[sim->player_table] = 21000;
    sim->outcome = 2; sim->now = 47000; sim->ended_at = 47000;
    assert(!mercs_bridge_publish(sim, overlay, launch.token, launch.edition));
    strcpy(token, launch.token);
    msim_free(sim); for (i = 0; i < count; i++) mech3d_free(&actors[i].mech);
    free(actors); free(sim);
}
static void assert_recovered(shell *s, const char *token)
{
    mercs_exchange_launch launch;
    mercs_exchange_result *packet = malloc(sizeof *packet);
    char overlay[PORTCFG_PATH];
    assert(packet);
    load_slot(s, -1, 1); frontend_ok(s);
    assert(s->page == PAGE_DEBRIEF && s->campaign.phase == MERCS_CAMPAIGN_DEBRIEF);
    assert(!strcmp(s->career.last_return_token, token));
    assert(!portcfg_overlay_dir(s->cfg, portcfg_mercs_overlay(s->edition), overlay, sizeof overlay, 0));
    assert(!mercs_exchange_load(overlay, token, &launch));
    assert(mercs_exchange_read(overlay, &launch, packet) == MERCS_EXCHANGE_APPLIED);
    free(packet);
}
/* An unreadable completed return is not an absent return. Keep its deployed
 * generation intact so a later retry can still import the original packet.
 * A NULL filename temporarily hides the whole token directory. Faults are
 * confined to the test's private installation, never the CD. */
static void assert_pending_read_retry(shell *s, const char *token, const char *filename)
{
    char overlay[PORTCFG_PATH], path[4096], retained[4096];
    mercs_campaign disk = {0};
    mercs_shell_state *view = calloc(1, sizeof *view);
    mercs_career_mech_pair pair = s->career.owned[s->career.owned_index];
    int32_t funds = s->career.funds;
    int index = s->career.owned_index, n;
    assert(view && s->campaign.phase == MERCS_CAMPAIGN_DEPLOYED);
    assert(!portcfg_overlay_dir(s->cfg, portcfg_mercs_overlay(s->edition), overlay, sizeof overlay, 0));
    n = snprintf(path, sizeof path, "%s/MERCS-RETURNS/%s%s%s", overlay, token,
                 filename ? "/" : "", filename ? filename : "");
    assert(n > 0 && (size_t)n < sizeof path);
    n = snprintf(retained, sizeof retained, "%s.retained-for-io-test", path);
    assert(n > 0 && (size_t)n < sizeof retained);
    assert(!rename(path, retained));
    if (filename) {
#ifdef _WIN32
        assert(!_mkdir(path));
#else
        assert(!mkdir(path, 0700));
#endif
    }
    load_slot(s, -1, 1);
    assert(s->status[0] && s->campaign.phase == MERCS_CAMPAIGN_DEPLOYED);
    assert(!strcmp(s->campaign.launch_token, token));
    assert(s->career.owned_index == index && s->career.funds == funds);
    assert(!memcmp(&s->career.owned[index], &pair, sizeof pair));
    s->status[0] = 0;
    assert(!mercs_store_read(s->save_dir, -1, s->archive, &disk, view, s->status, sizeof s->status));
    assert(disk.phase == MERCS_CAMPAIGN_DEPLOYED && !strcmp(disk.launch_token, token));
    assert(view->owned_index == index && view->funds == funds);
    assert(!memcmp(&view->owned[index], &pair, sizeof pair));
    mercs_campaign_free(&disk); free(view);
    if (filename) {
#ifdef _WIN32
        assert(!_rmdir(path));
#else
        assert(!rmdir(path));
#endif
    }
    assert(!rename(retained, path));
}
int main(int argc, char **argv)
{
    shell *s = calloc(1, sizeof *s);
    portcfg cfg;
    SDL_Surface *surface;
    char path[PORTCFG_PATH], error[256], models[PORTCFG_PATH], textures[PORTCFG_PATH];
    char kind[PORTCFG_PATH], sky[PORTCFG_PATH], fog[PORTCFG_PATH], token[MERCS_EXCHANGE_TOKEN];
    mercs_career_mech_pair *owned=calloc(MERCS_SHELL_MAX_OWNED,sizeof *owned);
    int32_t paid, cost;
    uint32_t rolled, identity;
    int i;
    assert(argc == 3 && s && owned);
    portcfg_defaults(&cfg); assert(!portcfg_resolve(&cfg, NULL, argv[1])); cfg.mode = 1;
    s->cfg = &cfg; s->exe = "unused-test-child"; s->running = 1; s->mode = 1;
    s->edition = PORTCFG_ED_DOS; s->accepted = -1; s->transition = -1;
    s->mouse_x = s->mouse_y = -1;
    assert(!portcfg_overlay_dir(&cfg, "retail", path, sizeof path, 1));
    assert(!mercs_store_directory(path, s->save_dir, sizeof s->save_dir));
    assert(!setenv("MERCS_INSTALL_DIR", path, 1)); assert(!setenv("MERCS_CD_DIR", cfg.cd, 1));
    dp_clear_roots(); dp_add_roots_from_env(); assert(!dp_set_write_root(path));
    assert(!dp_resolve("DATABASE.MW2", path, sizeof path)); assert(!shelldb_open(path, &s->database));
    assert(!portcfg_edition(&cfg, s->edition, models, textures, kind, sky, fog));
    s->archive = prj_open(models, error, sizeof error); assert(s->archive);
    assert(SDL_Init(SDL_INIT_EVENTS));
    surface = SDL_CreateSurface(SW, SH, SDL_PIXELFORMAT_ARGB8888); assert(surface);
    s->renderer = SDL_CreateSoftwareRenderer(surface); assert(s->renderer);
    mercs_viewport_init(&s->viewport, s->renderer);
    s->art = mercs_art_create(&s->database, s->renderer); assert(s->art);
    set_page(s, PAGE_REGISTER); strcpy(s->pilot, "FRONTEND"); strcpy(s->unit, "TEST COMPANY");
    register_pilot(s); frontend_ok(s);
    assert(s->page == PAGE_NEWS && s->career.funds == 1000000 && s->career.owned_count == 1);
    frame(s, argv[2], "01-registration-news");
    load_slot(s, -1, 1); frontend_ok(s);
    assert(s->career.news_raw[0] && s->page == PAGE_NEWS);
    set_page(s, PAGE_OFFICE); frame(s, argv[2], "02-original-office");
    set_page(s, PAGE_CONTRACTS); s->selected = mission_index(s, "BET1CAM1"); assert(s->selected >= 0);
    prepare_lines(s); frame(s, argv[2], "03-contract");
    press_original(s, &ui_contracts_hotspots[0]);
    assert(s->campaign.active && !strcasecmp(s->campaign.campaign, "BET1CAM1"));
    open_save_load(s); s->save_slot = 0;
    click(s, 100, 97, 1); assert(s->save_editing);
    key(s, SDLK_A, SDL_KMOD_CTRL); text_input(s, "BET1 START"); key(s, SDLK_RETURN, 0);
    save_slot(s); frontend_ok(s);
    assert(!strcmp(s->save_titles[0], "BET1 START") && s->page == PAGE_COMPUTER);
    assert(mercs_save_header(s->campaign.document)[0] == 0);
    open_save_load(s); frame(s, argv[2], "04-save-load");
    set_page(s, PAGE_ROSTER); roster_change(s, 1, 0); frontend_ok(s); assert(s->career.owned_index == -1);
    roster_change(s, 1, 0); frontend_ok(s); assert(s->career.owned_index == 0);
    frame(s, argv[2], "05-duty-roster");
    make_pending_return(s, token);
    assert_pending_read_retry(s, token, NULL);
    assert_pending_read_retry(s, token, "LAUNCH.DAT");
    assert_pending_read_retry(s, token, "RESULT.DAT");
    /* Reopening, not an in-memory direct import, must consume the packet. */
    assert_recovered(s, token);
    assert(le32(s->career.owned[0].current.bytes + 0x768) == 2);
    assert(s->career.funds == 1000000);
    paid = s->campaign.report.balance; rolled = s->campaign.rng_state;
    frame(s, argv[2], "06-original-debrief");
    click(s, 180, 440, 1); frontend_ok(s);
    assert(s->debrief_view.content_mode == 1 && s->campaign.rng_state == rolled);
    frame(s, argv[2], "07-original-analysis");
    load_slot(s, -1, 1); frontend_ok(s);
    assert(s->campaign.rng_state == rolled && s->career.funds == 1000000);
    assert(s->campaign.report.balance == paid);
    open_save_load(s); s->save_slot = 1;
    click(s, 100, 132, 1); assert(s->save_editing);
    key(s, SDLK_A, SDL_KMOD_CTRL); text_input(s, "BET1 DEBRIEF"); key(s, SDLK_RETURN, 0);
    save_slot(s); frontend_ok(s);
    set_page(s, PAGE_DEBRIEF); continue_debrief(s); frontend_ok(s);
    assert(s->campaign.phase == MERCS_CAMPAIGN_READY && s->career.funds == paid);
    assert(!strcasecmp(s->campaign.campaign, "BET2CAM1") && s->page == PAGE_BRIEFING);
    continue_debrief(s); assert(s->career.funds == paid); /* no second payment */
    set_page(s, PAGE_COMMAND);
    open_repair(s); frontend_ok(s); assert(s->repair && s->page == PAGE_REPAIR);
    frame(s, argv[2], "08-original-repair");
    cost = click_head_armor_repair(s); assert(cost > 0);
    {
        SDL_Event no;
        memset(&no, 0, sizeof no); no.type = SDL_EVENT_KEY_DOWN; no.key.key = SDLK_N;
        assert(SDL_PushEvent(&no) == 1);
        repair_action(s, MERCS_REPAIR_VIEW_ACCEPT); frontend_ok(s);
    }
    assert(!s->repair && s->career.funds == paid && le32(s->career.owned[0].current.bytes + 0x768) == 2);
    open_repair(s); frontend_ok(s);
    cost = click_head_armor_repair(s);
    assert(cost > 0);
    close_repair(s, 1, PAGE_COMMAND); frontend_ok(s);
    assert(!s->repair && s->career.funds == paid - cost);
    paid = s->career.funds;
    assert(le32(s->career.owned[0].current.bytes + 0x768) == le32(s->career.owned[0].reference.bytes + 0x768));
    /* Original Save/EXIT from repair rebuilds the assigned player's repair
     * view, while a later Load resumes the saved parent room, not a draft. */
    set_page(s, PAGE_OFFICE); open_repair(s); frontend_ok(s);
    assert(s->repair_return == PAGE_OFFICE);
    open_save_load(s); frontend_ok(s);
    assert(s->page == PAGE_SAVE && !s->repair && s->save_return == PAGE_REPAIR);
    s->save_slot = 2; strcpy(s->save_titles[2], "REPAIR CHECK");
    save_slot(s); frontend_ok(s);
    assert(s->page == PAGE_REPAIR && s->repair && s->repair_return == PAGE_OFFICE);
    assert(s->repair_index == s->career.owned_index && s->repair_view.part == -1);
    {
        mercs_campaign saved = {0};
        mercs_shell_state *saved_view = calloc(1, sizeof *saved_view);
        assert(saved_view);
        assert(!mercs_store_read(s->save_dir, 2, s->archive, &saved, saved_view, s->status, sizeof s->status));
        assert(saved.resume_page == PAGE_OFFICE);
        mercs_campaign_free(&saved); free(saved_view);
    }
    s->repair_view.part = MERCS_REPAIR_ARMOR;
    open_save_load(s); frontend_ok(s); click(s, 500, 452, 1); frontend_ok(s);
    assert(s->page == PAGE_REPAIR && s->repair && s->repair_return == PAGE_OFFICE);
    assert(s->repair_view.part == -1 && s->career.funds == paid);
    close_repair(s, 1, PAGE_OFFICE); frontend_ok(s);
    load_slot(s, -1, 1); frontend_ok(s); assert(s->career.funds == paid);
    memcpy(owned, s->career.owned, sizeof s->career.owned);
    for (i = 0; i < 3; i++) {
        change_game_look(s, shell_looks[i]);
        if (s->status[0] && !strstr(s->status, "selected for the next mission")) frontend_ok(s);
        s->status[0] = 0;
        assert(s->career.funds == paid && !strcasecmp(s->campaign.campaign, "BET2CAM1"));
        assert(!memcmp(owned, s->career.owned, sizeof s->career.owned));
    }
    change_game_look(s, PORTCFG_ED_DOS); s->status[0] = 0;
    load_slot(s, 1, 0); frontend_ok(s); /* intentionally restore a pre-settlement user save */
    assert(s->campaign.phase == MERCS_CAMPAIGN_DEBRIEF && s->career.funds == 1000000);
    continue_debrief(s); frontend_ok(s);
    assert(!strcasecmp(s->campaign.campaign, "BET2CAM1"));
    /* Training retains the selected Panther and resets damage on fresh entry. */
    s->mode = 0; strcpy(s->pilot, "CADET"); strcpy(s->unit, "TRAINING"); register_pilot(s); frontend_ok(s);
    assert(s->career.owned_count == 3 && s->career.owned_index == -1);
    set_page(s, PAGE_ROSTER); roster_change(s, -1, 0); frontend_ok(s); assert(s->career.owned_index == 2);
    identity = mercs_career_instance_id(&s->career.owned[2].current);
    make_pending_return(s, token); assert_recovered(s, token);
    assert(le32(s->career.owned[2].current.bytes + 0x768) == le32(s->career.owned[2].reference.bytes + 0x768));
    continue_debrief(s); frontend_ok(s);
    assert(!strcasecmp(s->campaign.campaign, "NOVICE2") && s->career.owned_index == 2);
    assert(mercs_career_instance_id(&s->career.owned[2].current) == identity && s->career.funds == 0);
    frame(s, argv[2], "09-training-next-mission");
    mercs_debrief_view_free(&s->debrief_view); mercs_text_free(&s->body);
    mercs_campaign_free(&s->campaign); mercs_art_snapshot_free(s->modal_background);
    free(s->panels); mercs_art_destroy(s->art); mercs_viewport_reset(&s->viewport);
    for (i = 0; i < TEXT_CACHE; i++) SDL_DestroyTexture(s->text[i].texture);
    for (i = 0; i < 120; i++) SDL_DestroyTexture(s->backgrounds[i]);
    prj_close(s->archive); shelldb_close(&s->database); SDL_DestroyRenderer(s->renderer);
    SDL_DestroySurface(surface); SDL_Quit(); free(owned); free(s);
    puts("PASS: controlled original-screen frontend journey, interrupted-return recovery, directory/manifest/result IO retry, once-only settlement, save/load, repair/cancel and three-look preservation");
    return 0;
}
