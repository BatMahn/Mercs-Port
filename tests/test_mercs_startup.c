#include "sdl3_test.h"
/* Regression for cold launch replaying an old failed mission. Exercise the
 * actual shell startup and Load/Save handlers with original shell artwork.
 * Usage: test_mercs_startup MERCS_GAME EMPTY_PRIVATE_OUTPUT_DIRECTORY
 * Headless: SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy.
 * No original game data is written. EOF uses a tiny generated valid Smacker
 * fixture; the key-skip path opens the supplied original MINTRO movie. */
#include <SDL3/SDL.h>
static bool startup_present(SDL_Renderer *renderer);
#define SDL_RenderPresent startup_present
#include "../tools/mercsshell.c"
#undef SDL_RenderPresent
#include <assert.h>
#include <dirent.h>
#include <inttypes.h>
#include <sys/stat.h>

typedef struct { uint64_t names, bytes; unsigned files, directories; } tree_stamp;
static uint64_t last_pixels;
static unsigned last_colored;
static const char *capture_frame;
static SDL_AtomicInt skip_sent;

static uint64_t fnv(uint64_t hash, const void *data, size_t length)
{
    const unsigned char *p = data;
    size_t i;
    for (i = 0; i < length; i++) { hash ^= p[i]; hash *= UINT64_C(1099511628211); }
    return hash;
}

static bool startup_present(SDL_Renderer *renderer)
{
    SDL_Surface *surface;
    int w, h, x, y;
    assert(SDL_GetCurrentRenderOutputSize(renderer, &w, &h));
    surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
    assert(surface);
    assert(!mercs_test_read_pixels(renderer, NULL, surface->format,
                                 surface->pixels, surface->pitch));
    last_pixels = UINT64_C(14695981039346656037); last_colored = 0;
    for (y = 0; y < h; y++) {
        const Uint32 *row = (const Uint32 *)((const Uint8 *)surface->pixels + y * surface->pitch);
        last_pixels = fnv(last_pixels, row, (size_t)w * sizeof *row);
        for (x = 0; x < w; x++) if (row[x] & 0xffffffu) last_colored++;
    }
    if (capture_frame) assert(SDL_SaveBMP(surface, capture_frame));
    SDL_DestroySurface(surface);
    return SDL_RenderPresent(renderer);
}

static void join(char *out, size_t cap, const char *directory, const char *leaf)
{
    int n = snprintf(out, cap, "%s/%s", directory, leaf);
    assert(n >= 0 && (size_t)n < cap);
}

static void directory_new(const char *path)
{
#ifdef _WIN32
    assert(!_mkdir(path));
#else
    assert(!mkdir(path, 0700));
#endif
}

/* Commutative per-entry sums make readdir ordering irrelevant. Both names and
 * complete file bytes are covered, including all immutable generations. */
static void stamp_tree(const char *path, tree_stamp *stamp)
{
    DIR *directory = opendir(path);
    struct dirent *entry;
    assert(directory);
    stamp->directories++;
    while ((entry = readdir(directory)) != NULL) {
        char child[4096];
        struct stat st;
        uint64_t hash = UINT64_C(14695981039346656037);
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        join(child, sizeof child, path, entry->d_name);
        assert(!lstat(child, &st)); assert(!S_ISLNK(st.st_mode));
        hash = fnv(hash, child, strlen(child));
        stamp->names += hash;
        if (S_ISDIR(st.st_mode)) stamp_tree(child, stamp);
        else {
            FILE *file;
            uint8_t bytes[4096];
            size_t count;
            assert(S_ISREG(st.st_mode));
            file = fopen(child, "rb"); assert(file);
            while ((count = fread(bytes, 1, sizeof bytes, file))) hash = fnv(hash, bytes, count);
            assert(!ferror(file)); assert(!fclose(file));
            stamp->bytes += hash; stamp->files++;
        }
    }
    assert(!closedir(directory));
}

static tree_stamp snapshot(const char *directory)
{ tree_stamp result = {0}; stamp_tree(directory, &result); return result; }

static void unchanged(const tree_stamp *before, const tree_stamp *after)
{
    assert(before->names == after->names && before->bytes == after->bytes);
    assert(before->files == after->files && before->directories == after->directories);
}

static void checked(int rc, const char *error)
{ if (rc) { fprintf(stderr, "startup check failed (%d): %s\n", rc, error); abort(); } }

static void shell_ok(shell *s)
{ checked(s->status[0] != 0, s->status); assert(s->running); }

static shell *fresh_shell(portcfg *cfg, const char *save_dir)
{
    shell *s = calloc(1, sizeof *s);
    char path[PORTCFG_PATH], error[256], models[PORTCFG_PATH], textures[PORTCFG_PATH];
    char kind[PORTCFG_PATH], sky[PORTCFG_PATH], fog[PORTCFG_PATH];
    assert(s);
    s->cfg = cfg; s->exe = "unused-startup-test"; s->running = 1;
    s->mode = 1; s->edition = PORTCFG_ED_DOS; s->accepted = -1; s->transition = -1;
    s->mouse_x = s->mouse_y = -1;
    snprintf(s->save_dir, sizeof s->save_dir, "%s", save_dir);
    assert(!dp_resolve("DATABASE.MW2", path, sizeof path));
    assert(!shelldb_open(path, &s->database)); assert(s->database.count == 120);
    assert(!portcfg_edition(cfg, s->edition, models, textures, kind, sky, fog));
    s->archive = prj_open(models, error, sizeof error); assert(s->archive);
    s->window = SDL_CreateWindow("Mercs startup regression", SW, SH, SDL_WINDOW_HIDDEN);
    assert(s->window);
    s->renderer = SDL_CreateRenderer(s->window, "software");
    assert(s->renderer);
    mercs_viewport_init(&s->viewport, s->renderer);
    s->art = mercs_art_create(&s->database, s->renderer); assert(s->art);
    mercs_options_defaults(&s->options);
    return s;
}

static void free_shell(shell *s)
{
    int i;
    mercs_text_free(&s->body); mercs_debrief_view_free(&s->debrief_view);
    mercs_repair_free(s->repair); mercs_campaign_free(&s->campaign);
    free(s->panels); mercs_art_snapshot_free(s->modal_background);
    mercs_viewport_reset(&s->viewport); mercs_art_destroy(s->art);
    for (i = 0; i < TEXT_CACHE; i++) SDL_DestroyTexture(s->text[i].texture);
    for (i = 0; i < 120; i++) SDL_DestroyTexture(s->backgrounds[i]);
    prj_close(s->archive); shelldb_close(&s->database);
    SDL_DestroyRenderer(s->renderer); SDL_DestroyWindow(s->window); free(s);
}

static void roots(const portcfg *cfg, const char *fixture_dir)
{
    assert(!setenv("MERCS_INSTALL_DIR", cfg->install, 1));
    assert(!setenv("MERCS_CD_DIR", cfg->cd, 1));
    dp_clear_roots();
    if (fixture_dir) assert(!dp_add_root(fixture_dir));
    dp_add_roots_from_env(); assert(!dp_set_write_root(cfg->install));
}

static void put32(uint8_t *p, uint32_t n)
{ int i; for (i = 0; i < 4; i++) p[i] = (uint8_t)(n >> (i * 8)); }

static void movie_fixture(const char *directory)
{
    uint8_t bytes[140] = {0};
    char subdir[4096], path[4096];
    FILE *file;
    int i;
    directory_new(directory); join(subdir, sizeof subdir, directory, "SMK"); directory_new(subdir);
    join(path, sizeof path, subdir, "MINTRO.SMK");
    memcpy(bytes, "SMK2", 4); put32(bytes + 4, 4); put32(bytes + 8, 4);
    put32(bytes + 12, 4); put32(bytes + 16, 20);
    for (i = 0; i < 4; i++) put32(bytes + 104 + i * 4, 4);
    bytes[120] = 1; bytes[124] = 1; bytes[125] = 63; bytes[126] = 10; bytes[127] = 5;
    file = fopen(path, "wb"); assert(file);
    assert(fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes); assert(!fclose(file));
}

static Uint32 SDLCALL skip_intro(void *unused, SDL_TimerID timer_id, Uint32 interval)
{
    SDL_Event event;
    (void)timer_id; (void)interval; (void)unused;
    SDL_zero(event); event.type = SDL_EVENT_KEY_DOWN; event.key.key = SDLK_SPACE; event.key.down = true;
    if (SDL_PushEvent(&event) == 1) SDL_SetAtomicInt(&skip_sent, 1);
    return 0;
}

static void create_checkpoint(prj_archive *archive, const char *directory, int phase,
                               const char *campaign_name)
{
    mercs_campaign campaign = {0}, loaded = {0};
    mercs_shell_state *view = calloc(1, sizeof *view), *restored = calloc(1, sizeof *restored);
    char error[384] = {0};
    int selected;
    assert(view && restored);
    checked(mercs_campaign_new(archive, 1, "STARTUP", "TEST COMPANY", &campaign, view,
                               error, sizeof error), error);
    for (selected = 0; selected < view->mission_count; selected++)
        if (!SDL_strcasecmp(view->missions[selected].campaign, campaign_name)) break;
    assert(selected < view->mission_count);
    checked(mercs_campaign_accept(archive, &campaign, view, selected, error, sizeof error), error);
    campaign.resume_page = PAGE_COMMAND;
    if (phase == MERCS_CAMPAIGN_DEPLOYED) {
        campaign.phase = phase;
        strcpy(campaign.launch_token, "0123456789abcdef0123456789abcdef");
        strcpy(campaign.launch_edition, "dos");
        assert(view->owned_index >= 0);
        mercs_career_set_deployed(&view->owned[view->owned_index].current, 1);
    } else if (phase == MERCS_CAMPAIGN_RETURN || phase == MERCS_CAMPAIGN_DEBRIEF) {
        mercs_car_init_dos(&view->last_car);
        view->last_car.bytes[MERCS_CAR_PLAYER_STATUS_OFFSET] = 0;
        memset(&view->last_msn, 0, sizeof view->last_msn);
        view->last_msn.outcome = 3; view->last_msn.row_count = 1;
        view->last_msn.rows[0].priority = 1;
        strcpy((char *)view->last_msn.rows[0].text, "Controlled failed objective");
        view->last_return_valid = 1; view->last_return_mission = selected;
        strcpy(view->last_return_token, "0123456789abcdef0123456789abcdef");
        checked(mercs_campaign_return(&campaign, view, error, sizeof error), error);
        if (phase == MERCS_CAMPAIGN_DEBRIEF) {
            checked(mercs_campaign_enter_debrief(archive, &campaign, view, error, sizeof error), error);
            assert(!campaign.report.success);
            strcpy((char *)mercs_save_header_edit(campaign.document) + 2, "FAILED DEBRIEF");
            checked(mercs_store_write(directory, 0, &campaign, view, error, sizeof error), error);
        }
    }
    checked(mercs_store_write(directory, -1, &campaign, view, error, sizeof error), error);
    checked(mercs_store_read(directory, -1, archive, &loaded, restored, error, sizeof error), error);
    assert(loaded.phase == phase && restored->registered);
    if (phase == MERCS_CAMPAIGN_DEBRIEF) assert(!loaded.report.success);
    mercs_campaign_free(&loaded); mercs_campaign_free(&campaign); free(view); free(restored);
}

static uint64_t cold_boot(portcfg *cfg, const char *directory, const char *fixture_dir,
                          int intro, uint64_t expected_pixels)
{
    tree_stamp before = snapshot(directory), after;
    SDL_TimerID timer = 0;
    Uint64 began;
    shell *s;
    SDL_Event event;
    roots(cfg, intro == 1 ? fixture_dir : NULL);
    s = fresh_shell(cfg, directory);
    while (SDL_PollEvent(&event)) {}
    if (!intro) assert(!setenv("MERCS_SHELL_SKIP_INTRO", "1", 1));
    else assert(!unsetenv("MERCS_SHELL_SKIP_INTRO"));
    if (intro == 2) {
        SDL_SetAtomicInt(&skip_sent, 0);
        timer = SDL_AddTimer(50, skip_intro, NULL); assert(timer);
    }
    began = SDL_GetTicks(); start_shell(s);
    if (intro == 1) assert(SDL_GetTicks() - began >= 70);
    if (timer) { SDL_RemoveTimer(timer); assert(SDL_GetAtomicInt(&skip_sent)); }
    shell_ok(s);
    assert(s->page == PAGE_MENU && !s->career.registered && !s->campaign.document);
    draw(s); shell_ok(s); assert(last_colored > 10000);
    if (expected_pixels) assert(last_pixels == expected_pixels);
    expected_pixels = last_pixels;
    after = snapshot(directory); unchanged(&before, &after);
    free_shell(s);
    return expected_pixels;
}

static void manual_resume(portcfg *cfg, const char *directory, uint64_t menu_pixels)
{
    shell *s;
    char exported[PORTCFG_PATH], expected[PORTCFG_PATH];
    int32_t funds, balance;
    uint32_t rng;
    mercs_debrief_report report;
    tree_stamp before, after;
    roots(cfg, NULL); assert(!setenv("MERCS_SHELL_SKIP_INTRO", "1", 1));
    s = fresh_shell(cfg, directory); start_shell(s); shell_ok(s);
    open_save_load(s); s->save_slot = 0;
    assert(s->page == PAGE_SAVE && !strcmp(s->save_titles[0], "FAILED DEBRIEF"));
    click(s, 450, 408, 1); shell_ok(s); /* Original LOAD button. */
    assert(s->page == PAGE_DEBRIEF && s->campaign.phase == MERCS_CAMPAIGN_DEBRIEF);
    assert(!s->campaign.report.success && s->career.registered);
    funds = s->career.funds; balance = s->campaign.report.balance;
    rng = s->campaign.rng_state; report = s->campaign.report;
    open_save_load(s); s->save_slot = 1; strcpy(s->save_titles[1], "RESUME FAILED ONCE");
    /* Fail promptly if the fixture accidentally resolves an occupied external
     * slot, rather than entering a correct overwrite-confirmation modal. */
    assert(original_slot_path(s, 1, exported) != 0);
    click(s, 560, 408, 1); shell_ok(s); /* Original SAVE button, empty slot. */
    assert(s->page == PAGE_DEBRIEF && s->career.funds == funds);
    assert(s->campaign.rng_state == rng && !memcmp(&s->campaign.report, &report, sizeof report));
    join(expected, sizeof expected, cfg->install, "mersave1.sav");
    assert(!original_slot_path(s, 1, exported) && !strcmp(exported, expected));
    free_shell(s);
    (void)cold_boot(cfg, directory, NULL, 0, menu_pixels);
    s = fresh_shell(cfg, directory); start_shell(s); shell_ok(s);
    open_save_load(s); s->save_slot = 1;
    assert(!strcmp(s->save_titles[1], "RESUME FAILED ONCE"));
    click(s, 450, 408, 1); shell_ok(s);
    assert(s->page == PAGE_DEBRIEF && s->career.funds == funds);
    assert(s->campaign.rng_state == rng && !memcmp(&s->campaign.report, &report, sizeof report));
    /* Transition-film preference only avoids unrelated landing runtime here;
     * neither the saved report nor the actual settlement handler is mocked. */
    mercs_options_set(&s->options, MO_TRANSITIONS, 0);
    continue_debrief(s); shell_ok(s);
    assert(s->campaign.phase == MERCS_CAMPAIGN_READY && s->career.funds == balance);
    assert(!s->career.last_return_valid);
    before = snapshot(directory);
    continue_debrief(s); shell_ok(s); assert(s->career.funds == balance);
    after = snapshot(directory); unchanged(&before, &after);
    printf("PASS manual Load/Save: failed report preserved; debrief -> ready; funds %ld -> %ld; repeat does not write.\n",
           (long)funds, (long)balance);
    free_shell(s);
}

int main(int argc, char **argv)
{
    static const char *const states[] = {"ready", "deployed", "returned", "failed-debrief", "corrupt", "missing"};
    static const char *const intros[] = {"disabled", "fixture-eof", "original-key-skip"};
    portcfg cfg;
    shell *seed;
    char fixture_dir[4096], directory[PORTCFG_PATH], failed[PORTCFG_PATH], path[4096];
    uint64_t menu_pixels = 0;
    int state, intro;
    if (argc != 3) { fprintf(stderr, "Usage: %s MERCS_GAME EMPTY_PRIVATE_OUTPUT_DIRECTORY\n", argv[0]); return 2; }
    setvbuf(stdout, NULL, _IOLBF, 0);
    portcfg_defaults(&cfg); assert(!portcfg_resolve(&cfg, NULL, argv[1])); cfg.mode = 1;
    /* Resolve read-only assets from the supplied game before redirecting both
     * write roots. Original-save export derives its retail overlay from game,
     * so changing install alone would leak a test save into the supplied game. */
    assert(strlen(argv[2]) < sizeof cfg.game); strcpy(cfg.game, argv[2]);
    assert(!portcfg_overlay_dir(&cfg, "retail", cfg.install, sizeof cfg.install, 1));
    join(fixture_dir, sizeof fixture_dir, argv[2], "eof-fixture"); movie_fixture(fixture_dir);
    roots(&cfg, NULL); assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));
    seed = fresh_shell(&cfg, cfg.install);
    for (state = 0; state < 6; state++) {
        join(directory, sizeof directory, argv[2], states[state]); directory_new(directory);
        if (state < 4) create_checkpoint(seed->archive, directory, state, "BET1CAM1");
        else if (state == 4) {
            FILE *file;
            join(path, sizeof path, directory, "AUTOSAVE.REF");
            file = fopen(path, "wb"); assert(file);
            assert(fputs("not a valid generation\n", file) >= 0); assert(!fclose(file));
        }
        for (intro = 0; intro < 3; intro++) {
            if (state == 3 && intro == 2) {
                join(path, sizeof path, argv[2], "failed-checkpoint-cold-menu.bmp"); capture_frame = path;
            }
            menu_pixels = cold_boot(&cfg, directory, fixture_dir, intro, menu_pixels);
            capture_frame = NULL;
            printf("PASS cold %-14s intro=%-17s original-menu=%016" PRIx64 " checkpoint unchanged\n",
                   states[state], intros[intro], menu_pixels);
        }
    }
    join(failed, sizeof failed, argv[2], "failed-debrief");
    free_shell(seed);
    manual_resume(&cfg, failed, menu_pixels);
    SDL_Quit();
    puts("PASS: 18 cold-launch cases, unchanged checkpoint trees, original main menu, private original-save export, explicit failed-debrief Load/Save and once-only settlement.");
    return 0;
}
