/* Titanium dynamic salvage: the wreck file, the yard's rules and actions, and
 * (with the game data) the Salvage Yard screen rendered to PPM files.
 * Usage: test_mercs_salvage MW2.PRJ CD_DIRECTORY [TITANIUM_DIRECTORY OUT_PREFIX] */
#include <SDL3/SDL.h>
#include "../tools/mercssalvage_view.h"
#include "mercscampaign.h"
#include "mercsshell_data.h"
#include "shelldb.h"
#include "datapath.h"
#include "mercsexchange.h"
#include "mercseconomy.h"
#include "mercsmechlab.h"
#include "mercsstore.h"
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int checks;
/* the yard's art plays no clip audio here */
#include "sfx.h"
void sfx_stop_pcm(sfx *s, const unsigned char *p) { (void)s; (void)p; }
int sfx_play_pcm_s16(sfx *s, const int16_t *p, int frames, int channels, int rate, float volume, int loop)
{ (void)s; (void)p; (void)frames; (void)channels; (void)rate; (void)volume; (void)loop; return 0; }
#define T(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); exit(1); } checks++; } while (0)

static void w32(uint8_t *m, size_t at, int32_t v) { uint32_t u = (uint32_t)v; m[at]=(uint8_t)u; m[at+1]=(uint8_t)(u>>8); m[at+2]=(uint8_t)(u>>16); m[at+3]=(uint8_t)(u>>24); }
static int32_t r32(const uint8_t *m, size_t at) { return (int32_t)((uint32_t)m[at]|(uint32_t)m[at+1]<<8|(uint32_t)m[at+2]<<16|(uint32_t)m[at+3]<<24); }

static void rules(void)
{
    mercs_salvage_wreck w[2], back[2];
    static uint8_t bytes[MERCS_SALVAGE_FILE_MAX];
    size_t size, n, i;
    uint32_t seed = 1;
    uint8_t m[MERCS_CAREER_MECH_BYTES];
    /* the C runtime's rand(): srand(1) gives 41, 18467, 6334 */
    T(mercs_salvage_rand(&seed) == 41 && mercs_salvage_rand(&seed) == 18467 && mercs_salvage_rand(&seed) == 6334);
    memset(w, 0, sizeof w);
    strcpy(w[0].name, "ANI00"); strcpy(w[1].name, "TBR00"); strcpy(w[1].title, "Timber Wolf");
    for (i = 0; i < 8; i++) { w[0].front[i] = (int32_t)i; w[0].rear[i] = -1; w[0].internal[i] = 10 + (int32_t)i; }
    for (i = 0; i < 96; i++) w[1].rnd[i] = (int32_t)(i * 7);
    T(!mercs_salvage_encode(w, 2, bytes, sizeof bytes, &size) && size == 4 + 2 * MERCS_SALVAGE_RECORD_BYTES);
    T(r32(bytes, 0) == 2 && !memcmp(bytes + 4, "ANI00", 6));
    /* the first location record after name 0x19, title 0x32 and two words */
    T(r32(bytes, 4 + 0x19 + 0x32 + 8 + 0x28 * 3 + 8) == 13);
    T(!mercs_salvage_decode(bytes, size, back, 2, &n) && n == 2 && !memcmp(back, w, sizeof w));
    T(mercs_salvage_decode(bytes, size - 1, back, 2, &n) == MERCS_SALVAGE_INVALID);
    /* categories (0x10061b13) */
    T(mercs_salvage_category(0) == -1 && mercs_salvage_category(105) == 0 && mercs_salvage_category(1205) == 2 &&
      mercs_salvage_category(2301) == 1 && mercs_salvage_category(4101) == 0 && mercs_salvage_category(10601) == 4 &&
      mercs_salvage_category(8003) == 3 && mercs_salvage_category(8501) == 3 && mercs_salvage_category(9551) == 3 &&
      mercs_salvage_category(7850) == -1 && mercs_salvage_category(9000) == -1);
    /* the debrief gate */
    T(mercs_salvage_allowed("bet1scn1", 0) && !mercs_salvage_allowed("bet2scn1", 0) &&
      !mercs_salvage_allowed("iva2scn1", 0) && mercs_salvage_allowed("mcc3scn1", 0) &&
      !mercs_salvage_allowed("rec1scn1", 0) && mercs_salvage_allowed("med2scn1", 0) &&
      !mercs_salvage_allowed("ran1scn1", 0) && mercs_salvage_allowed("ran1scn1", 1) &&
      mercs_salvage_allowed("out3scn1", 0) && mercs_salvage_is_out3("OUT3SCN1"));
    /* location condition (0x1004ae6f): baselines 15/15, i.e. a 75-point location */
    memset(m, 0, sizeof m);
    w32(m, 0x760, 25); w32(m, 0x768, 50); w32(m, 0x76c, 50); m[0x770] = 0xff;
    T(mercs_salvage_condition(m, 0) == 0);
    w32(m, 0x768, 40); T(mercs_salvage_condition(m, 0) == 1);
    w32(m, 0x760, 5); w32(m, 0x768, 5); T(mercs_salvage_condition(m, 0) == 2);
    w32(m, 0x760, 0); w32(m, 0x768, 0); w32(m, 0x76c, 0); T(mercs_salvage_condition(m, 0) == 3);
}

static void ppm(mercs_art *art, SDL_Renderer *renderer, SDL_Surface *surface, const char *path)
{
    FILE *f; int x, y;
    mercs_art_present(art); SDL_FlushRenderer(renderer);
    f = fopen(path, "wb"); T(f != NULL);
    fprintf(f, "P6\n640 480\n255\n");
    for (y = 0; y < 480; y++) for (x = 0; x < 640; x++) {
        Uint32 p = ((Uint32 *)((uint8_t *)surface->pixels + y * surface->pitch))[x];
        fputc((int)((p >> 16) & 255), f); fputc((int)((p >> 8) & 255), f); fputc((int)(p & 255), f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    char e[256], path[4096];
    prj_archive *a;
    mercs_campaign c;
    mercs_shell_state *v;
    mercs_salvage_wreck w[3];
    mercs_salvage_yard y;
    size_t i, owned, weapons;
    int k, l;
    uint32_t seed = 7;
    rules();
    {   /* the wreck file travels beside the mission's return */
        mercs_exchange_launch launch;
        mercs_salvage_wreck one, back;
        static uint8_t bytes[MERCS_SALVAGE_FILE_MAX], read[MERCS_SALVAGE_FILE_MAX];
        size_t size, got, n;
        char overlay[] = "/tmp/mercs_salvage_overlayXXXXXX";
        T(mkdtemp(overlay) != NULL);
        memset(&launch, 0, sizeof launch);
        strcpy(launch.scene, "GRA1SCN1"); strcpy(launch.edition, "direct3d");
        launch.unit_count = 1; launch.units[0].actor_index = -1;
        launch.units[0].controller_flags = 2; strcpy(launch.units[0].name, "PLY00MER");
        T(!mercs_exchange_begin(overlay, &launch));
        T(mercs_exchange_read_salvage(overlay, &launch, read, sizeof read, &got) == MERCS_EXCHANGE_PENDING);
        memset(&one, 0, sizeof one); strcpy(one.name, "CAT00"); one.internal[2] = 9; one.rnd[95] = 1234;
        T(!mercs_salvage_encode(&one, 1, bytes, sizeof bytes, &size));
        T(!mercs_exchange_publish_salvage(overlay, &launch, bytes, size));
        T(!mercs_exchange_read_salvage(overlay, &launch, read, sizeof read, &got) && got == size);
        T(!mercs_salvage_decode(read, got, &back, 1, &n) && n == 1 && !memcmp(&back, &one, sizeof one));
        T(mercs_exchange_publish_salvage(overlay, &launch, bytes, size) == MERCS_EXCHANGE_INVALID);   /* once */
    }
    if (argc < 3) { printf("test_mercs_salvage: ok (%d rule checks; no game data)\n", checks); return 0; }
    a = prj_open(argv[1], e, sizeof e); T(a != NULL);
    T(!dp_add_root(argv[2]));
    v = calloc(1, sizeof *v); memset(&c, 0, sizeof c);
    T(!mercs_campaign_new(a, 1, "SALVOR", "TEST UNIT", &c, v, e, sizeof e));
    /* three wrecks: an Annihilator with a shot-off arm, an intact Timber Wolf,
     * and a Catapult with its centre torso gone */
    memset(w, 0, sizeof w);
    strcpy(w[0].name, "ANI00"); strcpy(w[1].name, "TBR00"); strcpy(w[2].name, "CAT00");
    for (k = 0; k < 3; k++) for (i = 0; i < 96; i++) w[k].rnd[i] = mercs_salvage_rand(&seed);
    for (l = 0; l < 8; l++) {
        w[0].front[l] = 20; w[0].rear[l] = 8; w[0].internal[l] = 15;
        w[1].front[l] = 40; w[1].rear[l] = 15; w[1].internal[l] = 20;
        w[2].front[l] = 10; w[2].rear[l] = 2; w[2].internal[l] = 5;
    }
    w[0].front[4] = w[0].rear[4] = w[0].internal[4] = 0;
    w[2].front[2] = w[2].rear[2] = w[2].internal[2] = 0;
    T(!mercs_salvage_yard_open(&y, a, w, 3, 0, c.document) && y.count == 3);
    T(mercs_salvage_condition(y.entries[0].current.bytes, 4) == 3);
    T(!y.entries[2].whole);                       /* centre torso destroyed: never whole */
    owned = mercs_save_list_count(y.document, MERCS_SAVE_OWNED_MECHS);
    weapons = mercs_save_list_count(y.document, MERCS_SAVE_OWNED_WEAPONS);
    /* take the first salvageable component of the Timber Wolf */
    {
        int taken = 0;
        for (l = 0; l < 8 && !taken; l++) for (k = 0; k < 12 && !taken; k++) {
            int32_t id = r32(y.entries[1].current.bytes, 0x368 + 48 * l + 4 * k);
            if (id > 0 && mercs_salvage_category(id) >= 0 && mercs_salvage_category(id) <= 2 &&
                !mercs_salvage_take_component(&y, 1, l, k)) {
                taken = 1;
                T(r32(y.entries[1].current.bytes, 0x368 + 48 * l + 4 * k) == 0);
                T(mercs_save_list_count(y.document, MERCS_SAVE_OWNED_WEAPONS) == weapons + 1);
                T(mercs_salvage_take_component(&y, 1, l, k) == MERCS_SALVAGE_INVALID);   /* already taken */
                T(!mercs_salvage_leave_item(&y, 0, id / 100) && y.discard_count == 1);
                T(!mercs_salvage_take_discard(&y, 0, id / 100) && !y.discard_count);
            }
        }
        T(taken);
    }
    /* put the only owned mech back: refused; recover a whole wreck, then put one back */
    T(mercs_salvage_discard_owned(&y, 0) == MERCS_SALVAGE_LAST_MECH);
    for (i = 0; i < y.count && !y.entries[i].whole; i++);
    if (i < y.count) {
        size_t before = y.count;
        T(!mercs_salvage_recover(&y, a, i) && y.count == before - 1);
        T(mercs_save_list_count(y.document, MERCS_SAVE_OWNED_MECHS) == owned + 1);
        T(!mercs_salvage_discard_owned(&y, owned) && y.count == before && y.entries[before - 1].owned);
        T(!mercs_salvage_recover(&y, a, before - 1) && mercs_save_list_count(y.document, MERCS_SAVE_OWNED_MECHS) == owned + 1);
    }
    {   /* an ammunition bin becomes an ammo row of its weapon's type (0x1004a6fe) */
        int taken = 0;
        size_t e2;
        for (e2 = 0; e2 < y.count && !taken; e2++)
            for (l = 0; l < 8 && !taken; l++) for (k = 0; k < 12 && !taken; k++) {
                int32_t id = r32(y.entries[e2].current.bytes, 0x368 + 48 * l + 4 * k);
                size_t ammo = mercs_save_list_count(y.document, MERCS_SAVE_OWNED_AMMO);
                if (id >= 10000 && !mercs_salvage_take_component(&y, e2, l, k)) {
                    const uint8_t *p;
                    taken = 1;
                    T(mercs_save_list_count(y.document, MERCS_SAVE_OWNED_AMMO) == ammo + 1);
                    p = mercs_save_list_record(y.document, MERCS_SAVE_OWNED_AMMO, ammo);
                    T(r32(p, 0) == (id - 10000) / 10000 && p[8] == 1 && !mercs_economy_item_damaged(p));
                }
            }
        T(taken);
    }
    mercs_salvage_yard_free(&y);
    {   /* Titanium's damaged salvage (Armsdlr.cpp, Mech Lab 0x10049802) */
        mercs_save_state *d = mercs_save_clone(c.document);
        uint8_t row[MERCS_SAVE_ITEM_BYTES], *list;
        mercs_item_group *groups = NULL;
        mercs_mechlab_session *lab = NULL;
        size_t n, g, count = 0, at;
        int32_t funds, price = 10000, cut = (int32_t)((float)price * 0.15f);
        int found = 0, rc_damaged = 0, loc;
        T(d != NULL);
        n = mercs_save_list_count(d, MERCS_SAVE_OWNED_WEAPONS);
        memset(row, 0, sizeof row);
        w32(row, 0, 4); w32(row, 4, price); row[8] = 0; w32(row, 0xa, -1); w32(row, 0x12, 100); w32(row, 0x16, 1);
        T(mercs_economy_item_damaged(row));
        list = malloc((n + 2) * MERCS_SAVE_ITEM_BYTES); T(list != NULL);
        for (i = 0; i < n; i++) memcpy(list + i * MERCS_SAVE_ITEM_BYTES, mercs_save_list_record(d, MERCS_SAVE_OWNED_WEAPONS, i), MERCS_SAVE_ITEM_BYTES);
        for (i = 0; i < n; i++) T(!mercs_economy_item_damaged(list + i * MERCS_SAVE_ITEM_BYTES));
        memcpy(list + n * MERCS_SAVE_ITEM_BYTES, row, sizeof row);
        memcpy(list + (n + 1) * MERCS_SAVE_ITEM_BYTES, row, sizeof row);
        T(!mercs_save_set_list(d, MERCS_SAVE_OWNED_WEAPONS, list, n + 2)); free(list);
        /* ammunition is never damaged (its +0x16 is a count) */
        row[8] = 1; T(!mercs_economy_item_damaged(row)); row[8] = 0;
        /* trading lists the damaged copies separately */
        T(!mercs_economy_item_groups(d, 0, (enum mercs_item_category)mercs_economy_item_category(row), &groups, &count));
        for (g = 0; g < count; g++) if (groups[g].id == 4 && groups[g].price == price) { found = 1; T(groups[g].damaged == 2); }
        T(found); free(groups);
        /* the Mech Lab installs only intact stock */
        for (k = 0; k < 2; k++) {   /* damaged copies only, then the same rows intact */
            mercs_save_state *probe = mercs_save_clone(d);
            int rc = -1;
            T(probe != NULL);
            if (k) for (i = n; i < n + 2; i++) w32(mercs_save_list_record_edit(probe, MERCS_SAVE_OWNED_WEAPONS, i), 0x16, 0);
            T(!mercs_mechlab_begin(probe, 0, &lab));
            for (loc = 0; loc < 8 && rc; loc++)
                if (!mercs_mechlab_select_location(lab, loc)) rc = mercs_mechlab_add(lab, 0, 4);
            if (!k) rc_damaged = rc; else T(!rc && rc_damaged);
            mercs_mechlab_free(lab); mercs_save_free(probe);
        }
        /* repair: 15% of value, clears the mark; no funds, no repair */
        at = n;
        funds = (int32_t)r32(mercs_save_header(d), 0x12a);
        T(!mercs_economy_repair_item(d, 0, at));
        T((int32_t)r32(mercs_save_header(d), 0x12a) == funds - cut);
        T(mercs_save_list_count(d, MERCS_SAVE_OWNED_WEAPONS) == n + 2);
        T(!mercs_economy_item_damaged(mercs_save_list_record(d, MERCS_SAVE_OWNED_WEAPONS, n)) +
          !mercs_economy_item_damaged(mercs_save_list_record(d, MERCS_SAVE_OWNED_WEAPONS, n + 1)) == 1);
        /* selling prefers the damaged copy: 15% and scrapped */
        funds = (int32_t)r32(mercs_save_header(d), 0x12a);
        T(!mercs_economy_trade_item(d, 0, 0, n));
        T((int32_t)r32(mercs_save_header(d), 0x12a) == funds + cut);
        T(mercs_save_list_count(d, MERCS_SAVE_OWNED_WEAPONS) == n + 1);
        T(!mercs_economy_item_damaged(mercs_save_list_record(d, MERCS_SAVE_OWNED_WEAPONS, n)));
        T(mercs_economy_repair_item(d, 0, n) == MERCS_ECONOMY_EMPTY);
        /* an intact sale keeps the DOS price (stored value) */
        funds = (int32_t)r32(mercs_save_header(d), 0x12a);
        T(!mercs_economy_trade_item(d, 0, 0, n));
        T((int32_t)r32(mercs_save_header(d), 0x12a) == funds + price);
        mercs_save_free(d);
    }
    {   /* the wreck list kept in a native generation with the career */
        char dir[] = "/tmp/mercs_salvage_storeXXXXXX";
        static uint8_t bytes[MERCS_SALVAGE_FILE_MAX], back[MERCS_SALVAGE_FILE_MAX];
        size_t size, got;
        T(mkdtemp(dir) != NULL);
        T(!mercs_salvage_encode(w, 3, bytes, sizeof bytes, &size));
        T(mercs_store_read_salvage(dir, -1, back, sizeof back, &got) == 1);
        T(!mercs_store_write_attached(dir, -1, &c, v, bytes, size, e, sizeof e));
        T(!mercs_store_read_salvage(dir, -1, back, sizeof back, &got) && got == size && !memcmp(back, bytes, size));
        T(mercs_store_read_salvage(dir, -1, back, size - 1, &got) == -1);
        T(!mercs_store_write_attached(dir, 3, &c, v, bytes, size, e, sizeof e));
        T(!mercs_store_read_salvage(dir, 3, back, sizeof back, &got) && got == size);
        /* a later generation without wrecks (the debrief left) drops them */
        T(!mercs_store_write(dir, -1, &c, v, e, sizeof e));
        T(mercs_store_read_salvage(dir, -1, back, sizeof back, &got) == 1);
        T(!mercs_store_read_salvage(dir, 3, back, sizeof back, &got));
    }
    if (argc >= 5) {
        shelldb db, tdb;
        shell_image bg;
        uint8_t *bytes = NULL;
        long length;
        SDL_Surface *surface;
        SDL_Renderer *renderer;
        mercs_art *art;
        mercs_salvage_view view;
        const char *notice;
        T(SDL_Init(0));
        T(!dp_resolve("DATABASE.MW2", path, sizeof path) && !shelldb_open(path, &db));
        T(!dp_resolve_in(argv[3], "DATABASE.MW2", path, sizeof path) && !shelldb_open(path, &tdb));
        length = shelldb_entry(&tdb, 46, &bytes); T(length > 0 && !shell_pcx(bytes, (size_t)length, &bg, NULL)); free(bytes);
        surface = SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_ARGB8888); T(surface != NULL);
        renderer = SDL_CreateSoftwareRenderer(surface); T(renderer != NULL);
        art = mercs_art_create(&db, renderer); T(art != NULL);
        mercs_art_set_extra_dir(art, argv[3]);
        T(!mercs_salvage_yard_open(&y, a, w, 3, 0, c.document));
        mercs_salvage_view_open(&view, &y, &bg);
        mercs_art_begin_page(art); mercs_salvage_view_draw(&view, art, &y);
        snprintf(path, sizeof path, "%s_yard.ppm", argv[4]); ppm(art, renderer, surface, path);
        /* the Timber Wolf, its right torso, the first item selected */
        mercs_salvage_view_click(&view, &y, a, 100, 70, &notice);           /* open the wreck list */
        mercs_art_begin_page(art); mercs_salvage_view_draw(&view, art, &y);
        snprintf(path, sizeof path, "%s_list.ppm", argv[4]); ppm(art, renderer, surface, path);
        mercs_salvage_view_click(&view, &y, a, 60, 137 + 1, &notice);       /* second wreck */
        mercs_salvage_view_click(&view, &y, a, 150, 130, &notice);          /* left torso area */
        mercs_salvage_view_click(&view, &y, a, 260, 115, &notice);          /* first slot */
        mercs_art_begin_page(art); mercs_salvage_view_draw(&view, art, &y);
        snprintf(path, sizeof path, "%s_item.ppm", argv[4]); ppm(art, renderer, surface, path);
        mercs_salvage_view_click(&view, &y, a, 300, 395, &notice);          /* Take */
        mercs_art_begin_page(art); mercs_salvage_view_draw(&view, art, &y);
        snprintf(path, sizeof path, "%s_taken.ppm", argv[4]); ppm(art, renderer, surface, path);
        mercs_salvage_view_click(&view, &y, a, 490, 25, &notice);           /* Chassis tab */
        mercs_art_begin_page(art); mercs_salvage_view_draw(&view, art, &y);
        snprintf(path, sizeof path, "%s_chassis.ppm", argv[4]); ppm(art, renderer, surface, path);
        mercs_salvage_yard_free(&y);
        mercs_art_destroy(art); SDL_DestroyRenderer(renderer); SDL_DestroySurface(surface);
        shell_image_free(&bg); shelldb_close(&db); shelldb_close(&tdb); SDL_Quit();
    }
    mercs_campaign_free(&c); free(v); prj_close(a);
    printf("test_mercs_salvage: ok (%d checks)\n", checks);
    return 0;
}
