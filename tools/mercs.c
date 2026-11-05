/* Native Mercenaries startup: find/choose the game folder and enter its shell.
 * Developer mission commands retain the same explicit-player and write-overlay
 * checks; the shell may use them to launch a separate simulation process. */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <errno.h>

#include "portcfg.h"
#include "datapath.h"
#include "bwd.h"
#include "prj.h"
#include "mek.h"
#include "shelldb.h"
#include "mercspicker.h"
#include "mercsshell.h"
#include "mercsexchange.h"
#include "mercsstore.h"
#include "mercsshell_data.h"
#include "mercsinstant.h"

int mercs_sim_main(int argc, char **argv);
void mercs_sim_take_generated(prj_record *records, size_t count);

/* A procedural graph belongs to one accepted career seed. The shell commits
 * that career before spawning us; read it under the parent's launch lock and
 * require the exact process-return identity. No loose RAND files or inherited
 * environment variable can silently select a different mission. */
static int prepare_generated_launch(const portcfg *cfg, const char *overlay,
                                   const char *scene, const char *edition,
                                   const char *token, prj_archive *archive,
                                   char *error, size_t capacity)
{
    mercs_campaign career={0};
    mercs_shell_state *view=NULL;
    mercs_exchange_launch launch;
    char retail[PORTCFG_PATH], directory[PORTCFG_PATH+32];
    int rc=-1,n;
    if(strlen(scene)!=8 || strncasecmp(scene,"RAN",3) ||
       scene[3]<'0' || scene[3]>'9' || strcasecmp(scene+4,"SCN1")) return 0;
    snprintf(error,capacity,"A generated mission requires its accepted career launch.");
    if(!token || mercs_exchange_load(overlay,token,&launch) ||
       strcasecmp(launch.scene,scene) || strcmp(launch.edition,edition) ||
       portcfg_overlay_dir(cfg,"retail",retail,sizeof retail,0)) return -1;
    n=snprintf(directory,sizeof directory,"%s/MERCS-CAREERS",retail);
    if(n<0 || (size_t)n>=sizeof directory || !(view=calloc(1,sizeof *view))) return -1;
    if(mercs_store_read(directory,-1,archive,&career,view,error,capacity)) goto done;
    if(career.phase!=MERCS_CAMPAIGN_DEPLOYED || !career.active ||
       strcmp(career.launch_token,token) || strcmp(career.launch_edition,edition) ||
       career.accepted<0 || career.accepted>=view->mission_count ||
       strcasecmp(view->missions[career.accepted].scene,scene) ||
       !prj_is_generated(archive,"BWD",scene)) {
        snprintf(error,capacity,"The generated mission does not match the deployed career.");goto done;
    }
    rc=0;
done:
    mercs_campaign_free(&career);free(view);return rc;
}

static void usage(const char *exe)
{
    printf("MechWarrior 2: Mercenaries native port\n"
           "Usage: %s [GAME_FOLDER] [--edition dos|direct3d|3dfx]\n"
           "\nOpen the game normally to choose its folder and enter the menus.\n"
           "The folder is remembered for later launches.\n"
           "\nDeveloper commands:\n"
           "  %s [GAME_FOLDER] --mission SCENE [--edition dos|direct3d|3dfx] [--inspect]\n"
           "  %s [GAME_FOLDER] --list [--edition dos|direct3d|3dfx]\n", exe, exe, exe);
}

static void clear_game_paths(portcfg *cfg)
{
    cfg->game[0] = cfg->install[0] = cfg->cd[0] = cfg->music[0] = 0;
    cfg->models[0] = cfg->textures[0] = cfg->skygnd[0] = cfg->skygnd_fog[0] = 0;
    cfg->titanium[0] = cfg->mt32[0] = cfg->ultrasnd[0] = 0;
}

static int set_paths(const portcfg *cfg, const char *save, const char *edition,
                     const char *sky, const char *fog, int writable)
{
    if (setenv("MERCS_INSTALL_DIR", save, 1) != 0 || setenv("MERCS_CD_DIR", cfg->cd, 1) != 0 ||
        setenv("MERCS_EDITION", edition, 1) != 0 || setenv("MERCS_TEXTURES", "original", 1) != 0) return -1;
    unsetenv("MERCS_CD_IMAGE");   /* this validated folder supplies the disc */
    if (cfg->music[0]) setenv("MERCS_MUSIC_DIR", cfg->music, 1); else unsetenv("MERCS_MUSIC_DIR");
    if (sky[0]) setenv("MERCS_SKYGND", sky, 1); else unsetenv("MERCS_SKYGND");
    if (fog[0]) setenv("MERCS_SKYGND_FOG", fog, 1); else unsetenv("MERCS_SKYGND_FOG");
    dp_clear_roots(); dp_add_roots_from_env();
    return dp_set_write_root(writable ? save : NULL);
}

static int start_writes(portcfg *cfg, int ed, char *save, size_t size,
                        const char *sky, const char *fog)
{
    const char *name = portcfg_mercs_name(ed), *overlay = portcfg_mercs_overlay(ed);
    if (!name || !overlay) { errno = EINVAL; return -1; }
    if (portcfg_overlay_dir(cfg, overlay, save, size, 1) != 0) return -1;
    if (set_paths(cfg, save, name, sky, fog, 1) != 0) return -1;
    snprintf(cfg->install, sizeof cfg->install, "%s", save);
    cfg->renderer = ed;
    cfg->texset = PORTCFG_TX_ORIGINAL;
    return 0;
}

static int custom_player_mek(const char *overlay, const char *name)
{
    char relative[32], path[PORTCFG_PATH * 2];
    uint8_t data[MEK_MAX_SERIALIZED_BYTES];
    size_t i, length = strlen(name), got;
    uint32_t items;
    unsigned links;
    FILE *f;
    int extra, bad;
    mek_def loadout;
    /* The original GPS writer stores eight-character DOS basenames. Never let
     * a user-design name turn the explicit overlay lookup into another path. */
    if (!length || length > 8) return -1;
    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return -1;
    }
    snprintf(relative, sizeof relative, "MEK\\%s.MEK", name);
    if (dp_resolve_in(overlay, relative, path, sizeof path) != 0 || !(f = fopen(path, "rb"))) return -1;
    got = fread(data, 1, sizeof data, f);
    extra = fgetc(f); bad = ferror(f);
    fclose(f);
    if (bad || extra != EOF || got < 0x158 + 50) return -1;
    items = (uint32_t)data[16] | (uint32_t)data[17] << 8 | (uint32_t)data[18] << 16 | (uint32_t)data[19] << 24;
    links = (unsigned)data[20] | (unsigned)data[21] << 8;
    if (items > MEK_MAX_ITEMS || links > MEK_MAX_ITEMS - items) return -1;
    return mek_parse(data, got, &loadout);
}

static int mission_owned_roster(prj_archive *a,const bwd_mission *m,const bwd_gps *player,
                                const char *overlay,const char *token,const char *scene,const char *edition)
{
    mercs_exchange_launch launch;int r,c,slot=0;size_t found=0;
    if(mercs_exchange_load(overlay,token,&launch)||strcasecmp(launch.scene,scene)||strcmp(launch.edition,edition)||
       launch.unit_count<1||launch.unit_count>3)return -1;
    if (player->mek_id != BWD_EXTERNAL_ID) {
        bwd_gps authored;
        char error[192];
        size_t i;
        if (mercs_shell_scene_player(a,scene,&authored,error,sizeof error)!=1 ||
            strcmp(authored.loadout,player->loadout) || strcmp(authored.skeleton,player->skeleton) ||
            authored.table!=player->table) return -1;
        /* The original shell still writes the assigned owned roster for
         * these scenes. Its external MEKs remain unused by the stock player,
         * and must remain valid for the unchanged-file return/import path. */
        for (i=0;i<launch.unit_count;i++) {
            const mercs_exchange_unit *u=&launch.units[i];
            if (u->actor_index!=-(int32_t)i-1 || !(u->controller_flags&2) ||
                u->alliance!=0 || custom_player_mek(overlay,u->name)) return -1;
        }
        return 0;
    }
    for(r=0;r<m->record_count;r++)for(c=0;c<m->records[r].chunk_count;c++){
        const bwd_chunk *chunk=&m->records[r].chunks[c];bwd_gps gps;
        if(strcmp(chunk->tag,"GPS")||bwd_gps_decode(chunk,&gps)||gps.table!=player->table)continue;
        if((size_t)slot<launch.unit_count){
            const mercs_exchange_unit *u=&launch.units[slot];
            if(!m->records[r].external||strcasecmp(m->records[r].name,"USERSTAR")||
               u->actor_index!=-slot-1||!(u->controller_flags&2)||u->alliance!=0||
               strcasecmp(u->name,gps.loadout)||gps.mek_id!=BWD_EXTERNAL_ID||gps.is_player!=(slot==0)||
               prj_find_id(a,prj_find_type(a,"BWD"),gps.skeleton)!=gps.skeleton_id||
               custom_player_mek(overlay,gps.loadout))return -1;
            found++;
        }
        slot++;
    }
    return found==launch.unit_count?0:-1;
}

static int mission_player(prj_archive *a, const bwd_mission *m, bwd_gps *player, const char *overlay, int instant)
{
    int r, c, count = 0;
    for (r = 0; r < m->record_count; r++) for (c = 0; c < m->records[r].chunk_count; c++) {
        const bwd_chunk *chunk = &m->records[r].chunks[c];
        bwd_gps gps;
        if (strcmp(chunk->tag, "GPS") != 0 || bwd_gps_decode(chunk, &gps) != 0 || !gps.is_player) continue;
        *player = gps;
        count++;
    }
    if (count != 1) {
        fprintf(stderr, "Mission needs exactly one explicit player record; found %d.\n", count);
        return -1;
    }
    {
        prj_record loadout;
        int bwd = prj_find_type(a, "BWD"), mek = prj_find_type(a, "MEK");
        if (prj_find_id(a, bwd, player->skeleton) != player->skeleton_id) {
            fprintf(stderr, "Player resource names and IDs disagree with the selected edition.\n");
            return -1;
        }
        if (player->mek_id == 0xfffe) {
            if(instant&&prj_is_generated(a,"MEK",player->loadout)) {
                mek_def parsed;prj_record custom;
                int rc=prj_read_named(a,"MEK",player->loadout,&custom);
                if(rc)return -1;
                rc=mek_parse(custom.data,custom.size,&parsed);prj_record_free(&custom);return rc;
            }
            /* Mercs' original shell marks owned/custom MEKs with this sentinel
             * (GPS writer 0x1001a480). The registered starter is PLY00MER.MEK,
             * copied from its authored PLY00STD resource, not a fallback mech. */
            if (prj_find_id(a, mek, player->loadout) >= 0) {
                fprintf(stderr, "Custom player loadout %s conflicts with an archived loadout name.\n", player->loadout);
                return -1;
            }
            if (custom_player_mek(overlay, player->loadout) != 0) {
                fprintf(stderr, "Custom player loadout MEK/%s.MEK is missing or invalid in %s.\n", player->loadout, overlay);
                return -1;
            }
            return 0;
        }
        if (prj_find_id(a, mek, player->loadout) != player->mek_id) {
            fprintf(stderr, "Player resource names and IDs disagree with the selected edition.\n");
            return -1;
        }
        if (prj_read_named(a, "MEK", player->loadout, &loadout) != PRJ_OK) {
            fprintf(stderr, "Player loadout %s could not be verified.\n", player->loadout);
            return -1;
        }
        prj_record_free(&loadout);
    }
    return 0;
}

static int list_missions(prj_archive *a)
{
    int t = prj_find_type(a, "BWD"), i, count = 0;
    if (t < 0) return 1;
    for (i = 0; i < prj_symbol_count(a, t); i++) {
        const char *name = prj_symbol_name(a, t, i);
        size_t n = strlen(name);
        bwd_mission mission;
        if (n < 4 || strcasecmp(name + n - 4, "SCN1") != 0) continue;
        if (bwd_mission_load(a, name, &mission) != 0) continue;
        printf("%-12s %3d parts  %2d nav points", name, mission.record_count, mission.nav_count);
        if (mission.missing_count) {
            int j;
            printf("  needs");
            for (j = 0; j < mission.missing_count; j++) printf(" %s.BWD", mission.missing[j]);
        }
        putchar('\n'); count++;
        bwd_mission_free(&mission);
    }
    printf("%d mission scenes\n", count);
    return 0;
}

static int SDLCALL mercs_app_main(int argc, char **argv)
{
    const char *game = NULL, *scene = NULL, *edition = NULL, *return_token = NULL, *instant_token = NULL;
    int list = 0, inspect = 0, i, ed, gui, resolved, rc = 1;
    portcfg cfg;
    char models[PORTCFG_PATH], textures[PORTCFG_PATH], kind[PORTCFG_PATH];
    char sky[PORTCFG_PATH], fog[PORTCFG_PATH], save[PORTCFG_PATH + 32], err[PORTCFG_PATH + 500];
    char exe[4096], exe_dir[4096], *slash;
    prj_archive *archive;
    prj_record *generated=NULL;
    size_t generated_count=0;
    bwd_mission mission;
    bwd_gps player;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { usage(argv[0]); return 0; }
        else if (strcmp(argv[i], "--list") == 0) list = 1;
        else if (strcmp(argv[i], "--inspect") == 0) inspect = 1;
        else if (strcmp(argv[i], "--mission") == 0 && i + 1 < argc) scene = argv[++i];
        else if (strcmp(argv[i], "--edition") == 0 && i + 1 < argc) edition = argv[++i];
        else if (strcmp(argv[i], "--return-token") == 0 && i + 1 < argc) return_token = argv[++i];
        else if (strcmp(argv[i], "--instant-token") == 0 && i + 1 < argc) instant_token = argv[++i];
#ifdef __APPLE__
        else if (strncmp(argv[i], "-psn_", 5) == 0) continue;   /* older Finder launch metadata */
#endif
        else if (argv[i][0] != '-' && !game) game = argv[i];
        else { fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]); usage(argv[0]); return 2; }
    }
    if ((list && scene) || (inspect && !scene && !list) || (return_token && (!scene || list || inspect)) ||
        (instant_token && (!scene || list || return_token || strcasecmp(scene,"RAN0SCN1")))) { usage(argv[0]); return 2; }
    /* Only an explicit shell-created launch may publish a career return.
     * An inherited environment variable cannot attach a developer run to it. */
    unsetenv("MERCS_RETURN_TOKEN");
    if (edition && portcfg_mercs_parse(edition) < 0) {
        fprintf(stderr, "Edition must be dos, direct3d or 3dfx.\n"); return 2;
    }
    gui = !list && !scene;
    if (mercs_executable_path(argv[0], exe, sizeof exe) != 0) {
        if (gui) mercs_gui_error("The application could not determine its location. Move it to a local folder and open it again.");
        else perror("Cannot locate the Mercenaries executable");
        return 1;
    }
    snprintf(exe_dir, sizeof exe_dir, "%s", exe);
    slash = strrchr(exe_dir, '/');
#ifdef _WIN32
    { char *backslash = strrchr(exe_dir, '\\'); if (backslash && (!slash || backslash > slash)) slash = backslash; }
#endif
    if (!slash) { fprintf(stderr, "Executable path is not absolute.\n"); return 1; }
    slash[1] = 0;   /* portcfg discovery expects the directory's trailing separator */
    portcfg_load(NULL, &cfg);
    if (!game && (!cfg.game[0] || !portcfg_is_game_dir(cfg.game))) clear_game_paths(&cfg);
    resolved = portcfg_resolve(&cfg, exe_dir, game) == 0;
    if (!resolved && !gui) {
        fprintf(stderr, "Choose a Mercs-game folder containing the original retail or 3dfx cd/MERCS data and PRODUCT.INI.\n"); return 1;
    }
    if (!resolved && game && gui) {
        snprintf(err, sizeof err, "%s is not a Mercenaries game folder. Choose your Mercs-game folder in the next window.", game);
        if (mercs_gui_error(err) != 0) return 1;
    }
    for (;;) {
        if (!resolved) {
            int picked = mercs_pick_game_folder(&cfg, exe_dir);
            if (!picked) return 0;   /* cancellation is a normal, side-effect-free exit */
            if (picked < 0) {
                mercs_gui_error("The system folder chooser could not be opened. Place Mercs-game next to the application "
                    "or in your home folder, then open the game again. On Linux, install zenity or kdialog to enable the folder chooser.");
                return 1;
            }
        }
        err[0] = 0;
        ed = edition ? portcfg_mercs_parse(edition) :
             gui && portcfg_mercs_name(cfg.renderer) ? cfg.renderer : PORTCFG_ED_DOS;
        rc = portcfg_edition(&cfg, ed, models, textures, kind, sky, fog);
        if (rc) snprintf(err, sizeof err, "The requested %s data is missing or invalid in %s. Choose a game folder containing that edition's complete archive and sky data.",
                         portcfg_mercs_label(ed), cfg.game);
        else if (portcfg_overlay_dir(&cfg, portcfg_mercs_overlay(ed), save, sizeof save, 0) != 0)
            snprintf(err, sizeof err, "The Mercenaries installation overlay must use real directories inside the game folder: %s", strerror(errno));
        else {
            archive = prj_open(models, err, sizeof err);
            if (archive) {
                if (prj_find_type(archive, "BWD") >= 0 && prj_find_type(archive, "MEK") >= 0) {
                    if (!gui) break;
                    {
                        shelldb database = {0};
                        char database_path[PORTCFG_PATH];
                        /* Check the shell's original database before remembering
                         * this choice or creating an overlay. A partial copy can
                         * contain a valid PRJ while still lacking its menus. */
                        if (set_paths(&cfg, save, portcfg_mercs_name(ed), sky, fog, 0) != 0)
                            snprintf(err, sizeof err, "Cannot read the selected game folder: %s", strerror(errno));
                        else if (dp_resolve("DATABASE.MW2", database_path, sizeof database_path) != 0 ||
                                 shelldb_open(database_path, &database) != 0 || database.count != 120)
                            snprintf(err, sizeof err, "This folder is missing valid Mercenaries menu data (DATABASE.MW2). "
                                "Choose the complete Mercs-game folder.");
                        else { shelldb_close(&database); break; }
                        shelldb_close(&database);
                    }
                } else snprintf(err, sizeof err, "The game archive in %s is missing its mission or Mech resources.", cfg.game);
                prj_close(archive);
            }
        }
        if (!gui) { fprintf(stderr, "%s\n", err); return 1; }
        if (mercs_gui_error(err) != 0) return 1;
        resolved = 0;
    }
    edition = portcfg_mercs_name(ed);
    if (gui) {
        prj_close(archive);
        if (start_writes(&cfg, ed, save, sizeof save, sky, fog) != 0) {
            snprintf(err, sizeof err, "Cannot use the Mercenaries installation folder: %s", strerror(errno));
            mercs_gui_error(err); return 1;
        }
        if (portcfg_save(NULL, &cfg) != 0)
            mercs_gui_error("The game folder is ready, but its location could not be remembered. You may need to choose it again next time.");
        return mercs_shell_run(exe, &cfg, ed);
    }
    if (set_paths(&cfg, save, edition, sky, fog, 0) != 0) { perror("Cannot set Mercenaries data paths"); prj_close(archive); return 1; }
    if (list) { rc = list_missions(archive); prj_close(archive); return rc; }
    if(instant_token) {
        mercs_instant_launch *launch=calloc(1,sizeof *launch);
        if(!launch||mercs_instant_read(save,instant_token,edition,launch,err,sizeof err)||
           mercs_instant_install(archive,launch,err,sizeof err)) {
            fprintf(stderr,"%s\n",launch?err:"Could not allocate Instant Action launch.");free(launch);prj_close(archive);return 1;
        }
        free(launch);
    } else if(prepare_generated_launch(&cfg,save,scene,edition,return_token,archive,err,sizeof err)) {
        fprintf(stderr,"%s\n",err);prj_close(archive);return 1;
    }
    rc = 1;
    if (bwd_mission_load(archive, scene, &mission) != 0) {
        fprintf(stderr, "Cannot load mission %s from the selected edition.\n", scene);
        prj_close(archive); return 1;
    }
    if (mission.missing_count) {
        fprintf(stderr, "%s needs original launch data in %s:\n", scene, save);
        for (i = 0; i < mission.missing_count; i++) fprintf(stderr, "  %s.BWD\n", mission.missing[i]);
        fprintf(stderr, "No default player or career state will be substituted.\n");
        goto close_mission;
    }
    if (mission_player(archive, &mission, &player, save, instant_token != NULL) != 0) goto close_mission;
    if(return_token&&mission_owned_roster(archive,&mission,&player,save,return_token,scene,edition)){
        fprintf(stderr,"This mission's Duty Roster does not match its deployed USERSTAR and owned MEKs.\n");goto close_mission;
    }
    printf("Mercenaries %s / %s: %d parts, %d nav points, track %d\n", edition, scene,
           mission.record_count, mission.nav_count, mission.music_track);
    printf("Player: %s / %s (skeleton %u, loadout %u)\n", player.skeleton, player.loadout,
           (unsigned)player.skeleton_id, (unsigned)player.mek_id);
    rc = 0;
close_mission:
    bwd_mission_free(&mission);
    if(!rc && !inspect && prj_get_generated(archive,&generated,&generated_count)) {
        fprintf(stderr,"Could not retain the generated mission resources.\n");rc=1;
    }
    prj_close(archive);
    if (rc || inspect) return rc;
    /* Main and the renderer are in the same child process. Transfer the
     * validated graph into its newly opened archive before any BWD is read. */
    mercs_sim_take_generated(generated,generated_count);
    if (start_writes(&cfg, ed, save, sizeof save, sky, fog) != 0) {
        perror("Cannot use the Mercenaries installation overlay"); return 1;
    }
    if (return_token) {
        mercs_exchange_launch launch;
        if (mercs_exchange_load(save, return_token, &launch) ||
            strcasecmp(launch.scene, scene) || strcmp(launch.edition, edition) ||
            launch.unit_count < 1 || launch.unit_count > 3 || launch.units[0].actor_index != -1 ||
            (player.mek_id == BWD_EXTERNAL_ID && strcasecmp(launch.units[0].name, player.loadout)) ||
            setenv("MERCS_RETURN_TOKEN", return_token, 1)) {
            fprintf(stderr, "This mission's owned-mech return context is missing or inconsistent.\n");
            return 1;
        }
    }
    if (portcfg_save(NULL, &cfg) != 0) fprintf(stderr, "Could not remember the Mercenaries configuration.\n");
    {
        char *sim_argv[] = {exe, models, textures, kind, "@", (char *)scene, NULL};
        return mercs_sim_main(6, sim_argv);
    }
}

/* SDL 3 performs the platform startup before game-folder discovery or UI calls. */
int main(int argc, char **argv)
{
    return SDL_RunApp(argc, argv, mercs_app_main, NULL);
}
