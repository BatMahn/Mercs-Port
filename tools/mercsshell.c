/* Mercenaries retail shell: original DATABASE screens, fonts, SHP controls,
 * SMK room objects and recovered navigation tables. Port-only display settings
 * are separate. Career changes are staged and saved before becoming live. */
#include <SDL3/SDL.h>
#include "mercsicon.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <wchar.h>
#ifndef _WIN32
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "mercsshell.h"
#include "mercsshell_data.h"
#include "mercscampaign.h"
#include "mercsstore.h"
#include "mercsinterchange.h"
#include "mercsscores.h"
#include "mercsrepair.h"
#include "mercsvideo.h"
#include "mercspicker.h"
#include "mercslock.h"
#include "shelldb.h"
#include "datapath.h"
#include "ttext.h"
#include "mercsart.h"
#include "mercsviewport.h"
#include "mercsshell_audio.h"
#include "mercstext.h"
#include "mercsui_layout.h"
#include "sfx.h"
#include "mek.h"
#include "mercsrepair_view.h"
#include "mercsdebrief_view.h"
#include "mercsdialog_view.h"
#include "mercsgamemenu_view.h"
#include "mercspanels_view.h"
#include "mercscombat_view.h"
#include "mercscontrols_view.h"
#include "mercseconomy_view.h"
#include "mercstrading_view.h"
#include "mercsinventory_view.h"
#include "mercsmechbay_view.h"
#include "mercsfactory_view.h"
#include "mercsroster_view.h"
#include "mercsgroups_view.h"
#include "mercsinstant_view.h"
#include "mercsmechlab_view.h"
#include "mercscrew_view.h"
#include "mercssalvage_view.h"

enum { PAGE_MENU, PAGE_HALL, PAGE_REGISTER, PAGE_NEWS, PAGE_OFFICE,
       PAGE_COMPUTER, PAGE_CONTRACTS, PAGE_DROPSHIP, PAGE_COMMAND,
       PAGE_ROSTER, PAGE_BRIEFING, PAGE_SETTINGS, PAGE_DEBRIEF, PAGE_SAVE,
       PAGE_REPAIR, PAGE_FINANCE, PAGE_INVENTORY, PAGE_PERSONNEL, PAGE_TRADING,
       PAGE_MECHBAY, PAGE_MECHOPS, PAGE_DROP_FINANCE, PAGE_DROP_INVENTORY, PAGE_FACTORY, PAGE_GROUPS, PAGE_MECHLAB,
       PAGE_INSTANT, PAGE_INSTANT_PARAMS, PAGE_CREW, PAGE_CREW_BIO };
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
static const char *const page_names[] = {"main-menu", "starport", "registration",
    "news", "office", "office-computer", "contracts", "dropship",
    "mission-command", "duty-roster", "briefing", "video-options",
    "debriefing", "save-load", "repair-reload", "finance", "inventory", "personnel",
    "equipment-trading", "mech-bay", "mech-ops", "dropship-finance", "dropship-inventory", "mech-factory", "weapon-groups", "mech-lab",
    "instant-action", "instant-mission-parameters", "crew-photo", "crew-biography"};
enum { SW = 640, SH = 480, TEXT_CACHE = 256, MAX_LINES = 256, LINE_LEN = 128 };
static const int shell_looks[] = {PORTCFG_ED_DOS, PORTCFG_ED_D3D, PORTCFG_ED_3DFX};
typedef char shell_audio_page_order[
    (int)PAGE_MENU == (int)MSA_PAGE_MENU && (int)PAGE_HALL == (int)MSA_PAGE_HALL &&
    (int)PAGE_REGISTER == (int)MSA_PAGE_REGISTER && (int)PAGE_NEWS == (int)MSA_PAGE_NEWS &&
    (int)PAGE_OFFICE == (int)MSA_PAGE_OFFICE && (int)PAGE_COMPUTER == (int)MSA_PAGE_COMPUTER &&
    (int)PAGE_CONTRACTS == (int)MSA_PAGE_CONTRACTS && (int)PAGE_DROPSHIP == (int)MSA_PAGE_DROPSHIP &&
    (int)PAGE_COMMAND == (int)MSA_PAGE_COMMAND && (int)PAGE_ROSTER == (int)MSA_PAGE_ROSTER &&
    (int)PAGE_BRIEFING == (int)MSA_PAGE_BRIEFING && (int)PAGE_SETTINGS == (int)MSA_PAGE_SETTINGS &&
    (int)PAGE_DEBRIEF == (int)MSA_PAGE_DEBRIEF && (int)PAGE_SAVE == (int)MSA_PAGE_SAVE &&
    (int)PAGE_REPAIR == (int)MSA_PAGE_REPAIR &&
    (int)PAGE_FINANCE == (int)MSA_PAGE_FINANCE && (int)PAGE_INVENTORY == (int)MSA_PAGE_INVENTORY &&
    (int)PAGE_PERSONNEL == (int)MSA_PAGE_PERSONNEL && (int)PAGE_TRADING == (int)MSA_PAGE_TRADING &&
    (int)PAGE_MECHBAY == (int)MSA_PAGE_MECHBAY && (int)PAGE_MECHOPS == (int)MSA_PAGE_MECHOPS &&
    (int)PAGE_DROP_FINANCE == (int)MSA_PAGE_DROP_FINANCE &&
    (int)PAGE_DROP_INVENTORY == (int)MSA_PAGE_DROP_INVENTORY && (int)PAGE_FACTORY == (int)MSA_PAGE_FACTORY &&
    (int)PAGE_GROUPS == (int)MSA_PAGE_GROUPS && (int)PAGE_MECHLAB == (int)MSA_PAGE_MECHLAB &&
    (int)PAGE_INSTANT == (int)MSA_PAGE_INSTANT && (int)PAGE_INSTANT_PARAMS == (int)MSA_PAGE_INSTANT_PARAMS &&
    (int)PAGE_CREW == (int)MSA_PAGE_CREW && (int)PAGE_CREW_BIO == (int)MSA_PAGE_CREW_BIO ? 1 : -1];
typedef struct {
    char text[512];
    int cap, scale, width, height, baseline;
    Uint32 color;
    SDL_Texture *texture;
} text_cache;
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    mercs_viewport viewport;
    SDL_Texture *backgrounds[120];
    shelldb database;
    prj_archive *archive;
    portcfg *cfg;
    const char *exe;
    int edition, running, page, settings_return, mode, focus, selected, list_start;
    int scroll, brief_tab, select_all, training_choice;
    float wheel_remainder;
    int accepted, previous_page, mouse_x, mouse_y, transition, transition_x, transition_y;
    int transition_restart, portrait_restart, brief_video, letter_heard, regular_unassigned;
    const char *transition_clip;
    mercs_art *art;
    mercs_text_layout body;
    int crew_selected;
    sfx *audio;
    mercs_shell_audio *shell_audio;
    mercs_dialog dialog;
    mercs_game_menu game_menu;
    mercs_art_snapshot *modal_background;
    int completed_transition, after_save, menu_backdrop;
    mercs_panels *panels;
    mercs_lords_table lords;
    mercs_options options;
    mercs_combat_view combat;
    mercs_controls_view controls;
    int panel_mode, reveal_rows, reveal_total, reveal_active;
    unsigned panel_polls;
    int look_available[3];
    char pilot[22], unit[22], status[384];
    text_cache text[TEXT_CACHE];
    int text_next;
    unsigned char *fallback_font;
    size_t fallback_size;
    mercs_shell_state career;
    mercs_campaign campaign;
    char save_dir[PORTCFG_PATH];
    char save_titles[10][256];
    char save_edit_backup[256];
    char save_original_path[10][PORTCFG_PATH];
    int save_native[10];
    int save_mechlab_index;
    int save_slot, save_return, save_editing, menu_mnemonic;
    mercs_repair_session *repair;
    int repair_index, repair_return, repair_finished;
    mercs_repair_view repair_view;
    mercs_debrief_view debrief_view;
    int debrief_prepared; /* one-shot view staged with a durable phase commit */
    mercs_finance_view finance_view;
    mercs_personnel_view personnel_view;
    mercs_trading_view trading_view;
    mercs_inventory_view inventory_view;
    mercs_factory_view factory_view;
    mercs_roster_view roster_view;
    mercs_groups_view groups_view;
    mercs_instant instant;
    mercs_mechlab_session *mechlab;
    mercs_mechlab_view mechlab_view;
    int mechlab_return, mechlab_editing, mechlab_instant;
    char mechlab_name[19];
    int (*mechlab_complete)(void *,const uint8_t *,size_t);
    void *mechlab_complete_user;
    int personnel_return, room_restart;
    /* Titanium's dynamic salvage: the wrecks of the last return, its yard (an
     * overlay on the debrief page) and Titanium's background picture */
    mercs_salvage_wreck salvage_wrecks[MERCS_SALVAGE_MAX_WRECKS];
    int salvage_wreck_count, salvage_open, salvage_used, salvage_bg_ok;
    size_t salvage_raw_size;   /* the wrecks' salvage.mw2 bytes, in salvage_raw */
    mercs_salvage_yard salvage_yard;
    mercs_salvage_view salvage_view;
    shell_image salvage_bg;
} shell;

static void apply_shell_volumes(shell *s);
static void controls_action(shell *s, int action);

static Uint32 le32(const unsigned char *p)
{ return (Uint32)p[0] | (Uint32)p[1] << 8 | (Uint32)p[2] << 16 | (Uint32)p[3] << 24; }

static void color(shell *s, Uint32 rgb, int alpha)
{ SDL_SetRenderDrawColor(s->renderer, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, (Uint8)alpha); }

static void box(shell *s, int x, int y, int w, int h, Uint32 rgb, int alpha)
{
    SDL_FRect r = {(float)x, (float)y, (float)w, (float)h};
    color(s, rgb, alpha); SDL_RenderFillRect(s->renderer, &r);
}

static int hit(int x, int y, int bx, int by, int bw, int bh)
{ return x >= bx && y >= by && x < bx + bw && y < by + bh; }

/* Original database font fallback keeps a source-only build usable even if the
 * optional mature-port TrueType font cannot be located next to the executable. */
static void bitmap_text(shell *s, int x, int y, const char *str, Uint32 rgb)
{
    const unsigned char *d = s->fallback_font;
    int height, key;
    if (!d || s->fallback_size < 16) return;
    height = (int)le32(d + 8); key = d[12];
    if (height <= 0 || height > 64) return;
    color(s, rgb, 255);
    for (; *str; str++) {
        unsigned c = (unsigned char)*str & 127u, off;
        int width, gx, gy;
        if (16u + c * 4u + 4u > s->fallback_size) continue;
        off = le32(d + 16 + c * 4);
        if ((size_t)off + 4 > s->fallback_size) continue;
        width = (int)le32(d + off);
        if (width < 0 || width > 64 || (size_t)off + 4 + (size_t)width * (size_t)height > s->fallback_size) continue;
        for (gy = 0; gy < height; gy++) for (gx = 0; gx < width; gx++)
            if (d[off + 4 + (size_t)gy * (size_t)width + (size_t)gx] != key)
                SDL_RenderPoint(s->renderer, x + gx, y + gy);
        x += width;
    }
}

static float width_text(const char *str, int cap)
{ return ttext_available() ? ttext_width(str, (float)cap, 0.90f) : (float)strlen(str) * 7.0f; }

static void text(shell *s, int x, int y, int cap, Uint32 rgb, const char *str)
{
    int ow, oh, scale, i, w, h, baseline;
    float ratio;
    uint8_t *coverage;
    Uint32 *pixels;
    text_cache *entry = NULL;
    SDL_FRect dest;
    if (!str || !str[0]) return;
    if (!ttext_available()) { bitmap_text(s, x, y, str, rgb); return; }
    if (!SDL_GetCurrentRenderOutputSize(s->renderer, &ow, &oh)) { ow = SW; oh = SH; }
    ratio = fminf((float)ow / SW, (float)oh / SH);
    scale = (int)ceilf(ratio); if (scale < 1) scale = 1; if (scale > 4) scale = 4;
    for (i = 0; i < TEXT_CACHE; i++) {
        text_cache *e = &s->text[i];
        if (e->texture && e->cap == cap && e->scale == scale && e->color == rgb && !strcmp(e->text, str)) { entry = e; break; }
    }
    if (!entry) {
        coverage = ttext_render(str, (float)(cap * scale), 0.90f, &w, &h, &baseline);
        if (!coverage) return;
        pixels = malloc((size_t)w * (size_t)h * sizeof *pixels);
        if (!pixels) { free(coverage); return; }
        for (i = 0; i < w * h; i++) pixels[i] = ((Uint32)coverage[i] << 24) | (rgb & 0xffu) << 16 | (rgb & 0xff00u) | (rgb >> 16 & 0xffu);
        free(coverage);
        entry = &s->text[s->text_next++ % TEXT_CACHE];
        SDL_DestroyTexture(entry->texture);
        memset(entry, 0, sizeof *entry);
        entry->texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
        if (!entry->texture) { free(pixels); return; }
        SDL_UpdateTexture(entry->texture, NULL, pixels, w * 4); free(pixels);
        SDL_SetTextureBlendMode(entry->texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(entry->texture, SDL_SCALEMODE_LINEAR);
        snprintf(entry->text, sizeof entry->text, "%s", str);
        entry->cap = cap; entry->scale = scale; entry->color = rgb;
        entry->width = w; entry->height = h; entry->baseline = baseline;
    }
    dest.x = (float)x; dest.y = (float)y - (float)entry->baseline / (float)scale + (float)cap;
    dest.w = (float)entry->width / (float)scale; dest.h = (float)entry->height / (float)scale;
    SDL_RenderTexture(s->renderer, entry->texture, NULL, &dest);
}

static void centered(shell *s, int cx, int y, int cap, Uint32 rgb, const char *str)
{ text(s, cx - (int)(width_text(str, cap) / 2.0f), y, cap, rgb, str); }

static void button(shell *s, int x, int y, int w, int h, const char *label, int active, int enabled)
{
    SDL_FRect r = {(float)x, (float)y, (float)w, (float)h};
    box(s, x, y, w, h, active ? 0x283d58 : 0x0b101b, 242);
    color(s, enabled ? (active ? 0xb6cbea : 0x71839d) : 0x3b4554, 255);
    SDL_RenderRect(s->renderer, &r);
    centered(s, x + w / 2, y + (h - 11) / 2, 11, enabled ? 0xe7eaf1 : 0x68707d, label);
}

static SDL_Texture *background(shell *s, int index)
{
    unsigned char *data = NULL, *rgb;
    shell_image image;
    long length;
    int i;
    SDL_Texture *texture;
    if (index < 0 || index >= 120) return NULL;
    if (s->backgrounds[index]) return s->backgrounds[index];
    length = shelldb_entry(&s->database, index, &data);
    if (length <= 0 || shell_pcx(data, (size_t)length, &image, NULL)) { fprintf(stderr, "mercs shell: DATABASE entry %d: %ld bytes, not a PCX\n", index, length); free(data); return NULL; }
    free(data);
    /* 32-bit: SDL's Vulkan renderer (Windows) has no 24-bit texture format */
    rgb = malloc((size_t)image.w * (size_t)image.h * 4);
    if (!rgb) { shell_image_free(&image); return NULL; }
    for (i = 0; i < image.w * image.h; i++) { memcpy(rgb + (size_t)i * 4, image.pal[image.pix[i]], 3); rgb[(size_t)i * 4 + 3] = 255; }
    texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.w, image.h);
    if (texture) { SDL_UpdateTexture(texture, NULL, rgb, image.w * 4); SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST); }
    else fprintf(stderr, "mercs shell: background %d (%d x %d): %s\n", index, image.w, image.h, SDL_GetError());
    free(rgb); shell_image_free(&image);
    s->backgrounds[index] = texture;
    return texture;
}

static void page_background(shell *s, int index)
{
    SDL_Texture *texture = background(s, index);
    color(s, 0, 255); SDL_RenderClear(s->renderer);
    if (texture) SDL_RenderTexture(s->renderer, texture, NULL, NULL);
}

static void prepare_lines(shell *s);
static void resume_campaign(shell *s);
static void open_repair(shell *s);
static int start_repair(shell *s, int index);
static void close_repair(shell *s, int accept, int destination);
static void repair_action(shell *s, int action);
static int start_mechlab(shell *s, int index);
static void open_mechlab(shell *s);
static void close_mechlab(shell *s, int accept, int destination);
static void finish_mechlab_name(shell *s, int cancel);
static void finish_save_description(shell *s, int cancel);
static void draw(shell *s);
static void dispatch_event(shell *s, const SDL_Event *event);
static int system_event(shell *s, const SDL_Event *event);
static int run_dialog(shell *s);
static void open_game_menu(shell *s);
static int play_cue(shell *s, mercs_shell_audio_cue cue);
static int career_dirty(const shell *s);
static void reset_text_reveal(shell *s);
static void poll_shell(shell *s);
static int prepare_economy_page(shell *s, int fresh);

/* The original's mouse driver puts the pointer back at the centre of the 640 x 480 screen when its movie player or
 * the career shell starts (the driver reset). DOSBox: the pointer jumps to the centre after the launch movie (and so
 * sits on MISSION COMMAND in the DropShip, showing its label) and on entering the StarPort from the main menu, and it
 * stays put between pages without a movie (registration -> news -> StarPort). The picture is fitted about the
 * window's centre, so the screen's centre is the window's. Only while the pointer is over the window. */
static void center_pointer(shell *s)
{
    int w, h;
    if (!s->window || SDL_GetMouseFocus() != s->window || !SDL_GetWindowSize(s->window, &w, &h) || w <= 0 || h <= 0) return;
    SDL_WarpMouseInWindow(s->window, (float)w * 0.5f, (float)h * 0.5f);
}

static void command_auto_assign(shell *s);
static void set_page(shell *s, int page)
{
    if (s->save_editing) finish_save_description(s, 0);
    if (s->mechlab_editing) finish_mechlab_name(s, 0);
    if (s->page == PAGE_MENU && page == PAGE_HALL) center_pointer(s);
    s->previous_page = s->page; s->page = page;
    s->status[0] = 0; s->select_all = 0; s->scroll = 0; s->wheel_remainder = 0;
    s->transition = -1; s->transition_clip = NULL; s->portrait_restart = 0;
    s->completed_transition = 0;
    s->brief_video = 0;
    s->room_restart = 1;
    mercs_art_begin_page(s->art);
    mercs_shell_audio_enter(s->shell_audio, (mercs_shell_audio_page)page,
                           s->career.registered, (mercs_shell_audio_page)s->previous_page);
    if (page == PAGE_REGISTER) SDL_StartTextInput(s->window); else SDL_StopTextInput(s->window);
    if (page == PAGE_COMMAND) command_auto_assign(s);
    prepare_lines(s);
    (void)prepare_economy_page(s, 1);
    fprintf(stderr, "mercs shell: page=%s\n", page_names[page]);
}

enum { SAVE_DISPLAY = 1, SAVE_MOVIE_LOOK = 2, SAVE_MOVIE_FIT = 4, SAVE_GAME_LOOK = 8,
       SAVE_RENDER_SIZE = 16, SAVE_GRAPHICS = 32, SAVE_GPU = 64, SAVE_SALVAGE = 128 };

static void save_settings_fields(shell *s, unsigned changed)
{
    portcfg current;
    /* The separate simulation can save HUD, shading and renderer preferences.
     * Merge this action into the latest file instead of rewriting our older
     * snapshot. The running shell keeps its validated game/archive identity. */
    if (portcfg_load(NULL, &current)) {
        if (errno != ENOENT) {
            snprintf(s->status, sizeof s->status, "Could not read current settings: %s", strerror(errno));
            return;
        }
        current = *s->cfg;
    }
#define KEEP_PATH(k) memcpy(current.k, s->cfg->k, sizeof current.k)
    KEEP_PATH(game); KEEP_PATH(install); KEEP_PATH(cd); KEEP_PATH(music);
    KEEP_PATH(ultrasnd); KEEP_PATH(mt32); KEEP_PATH(models); KEEP_PATH(textures);
    KEEP_PATH(skygnd); KEEP_PATH(skygnd_fog); KEEP_PATH(titanium);
#undef KEEP_PATH
    current.textures_ati = s->cfg->textures_ati;
    current.no3d = s->cfg->no3d;
    if (changed & SAVE_DISPLAY) { current.mode = s->cfg->mode; current.w = s->cfg->w; current.h = s->cfg->h; }
    if (changed & SAVE_MOVIE_LOOK) current.videos = s->cfg->videos;
    if (changed & SAVE_MOVIE_FIT) current.video_fit = s->cfg->video_fit;
    if (changed & SAVE_SALVAGE) current.salvage = s->cfg->salvage;
    if (changed & SAVE_GAME_LOOK) { current.renderer = s->cfg->renderer; current.texset = s->cfg->texset; }
    if (changed & SAVE_RENDER_SIZE) { current.render_w = s->cfg->render_w; current.render_h = s->cfg->render_h; }
    if (changed & SAVE_GPU) current.gpu = s->cfg->gpu;
    if (changed & SAVE_GRAPHICS) {
        current.texture_filter = s->cfg->texture_filter;
        current.dos_sky_smooth = s->cfg->dos_sky_smooth;
        current.hud_original = s->cfg->hud_original;
    }
    if (portcfg_save(NULL, &current)) snprintf(s->status, sizeof s->status, "Could not save settings: %s", strerror(errno));
    else s->status[0] = 0;
}

static void invalidate_shell_textures(shell *s)
{
    int i;
    mercs_viewport_reset(&s->viewport);
    for (i = 0; i < TEXT_CACHE; i++) {
        SDL_DestroyTexture(s->text[i].texture);
        memset(&s->text[i], 0, sizeof s->text[i]);
    }
    s->text_next = 0;
    mercs_art_reset_device(s->art);
    for (i = 0; i < 120; i++) {
        SDL_DestroyTexture(s->backgrounds[i]);
        s->backgrounds[i] = NULL;
    }
}

static void toggle_fullscreen(shell *s)
{
    SDL_WindowFlags flags = SDL_GetWindowFlags(s->window);
    int fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    if (!SDL_SetWindowFullscreen(s->window, !fullscreen))
        snprintf(s->status, sizeof s->status, "Could not change display mode: %s", SDL_GetError());
    else { s->cfg->mode = fullscreen ? 1 : 0; save_settings_fields(s, SAVE_DISPLAY); }
}

static void play_movie(shell *s, const char *name)
{
    char relative[64], path[PORTCFG_PATH], movie_error[320] = "";
    int result, fullscreen_before, fullscreen_after;
    if (s->options.snd_size && !mercs_options_transition(&s->options, name)) return;
    snprintf(relative, sizeof relative, "SMK\\%.16s.SMK", name);
    if (dp_resolve(relative, path, sizeof path)) {
        snprintf(s->status, sizeof s->status, "The %s movie could not be found in the game folder.", name);
        return;
    }
    SDL_StopTextInput(s->window);
    fprintf(stderr, "mercs shell: movie=%s look=%s fit=%s\n", name, PORTCFG_VID_NAME[s->cfg->videos], PORTCFG_FIT_NAME[s->cfg->video_fit]);
    fullscreen_before = (SDL_GetWindowFlags(s->window) & SDL_WINDOW_FULLSCREEN) != 0;
    mercs_shell_audio_suspend(s->shell_audio);
    if (s->audio) sfx_pause(s->audio, 1);
    result = mercs_video_play(s->window, s->renderer, s->cfg, path);
    if (s->audio) sfx_pause(s->audio, 0);
    mercs_shell_audio_resume(s->shell_audio);
    if (result < 0) snprintf(movie_error, sizeof movie_error, "Movie playback failed: %.290s", SDL_GetError());
    /* Playback consumes renderer-reset events while keeping its own texture
     * alive. Rebuild our static artwork/text on demand after returning, too. */
    invalidate_shell_textures(s);
    fullscreen_after = (SDL_GetWindowFlags(s->window) & SDL_WINDOW_FULLSCREEN) != 0;
    s->status[0] = 0;
    if (fullscreen_before != fullscreen_after) {
        s->cfg->mode = fullscreen_after ? 0 : 1;
        save_settings_fields(s, SAVE_DISPLAY);
    }
    if (result == 1) s->running = 0;
    else if (result < 0) snprintf(s->status, sizeof s->status, "%s", movie_error);
    if (s->running && s->page == PAGE_REGISTER) SDL_StartTextInput(s->window);
    if (s->running) center_pointer(s);
    fprintf(stderr, "mercs shell: movie-ended=%s result=%d\n", name, result);
}

/* A complete document and its view travel together. No user-visible mutation
 * is installed until the new autosave generation has committed. */
static int stage_career(shell *s, mercs_campaign *next, mercs_shell_state **view)
{
    memset(next, 0, sizeof *next);
    *view = malloc(sizeof **view);
    if (!*view || mercs_campaign_clone(&s->campaign, next)) {
        free(*view); *view = NULL;
        snprintf(s->status, sizeof s->status, "Not enough memory to update the career.");
        return -1;
    }
    **view = s->career;
    next->accepted = s->accepted; next->selected = s->selected;
    return 0;
}

static uint8_t salvage_raw[MERCS_SALVAGE_FILE_MAX];

/* Every native save carries the unsalvaged wreck list while the career is
 * between its return and leaving the debrief (Titanium keeps salvage.mw2 on
 * disk until then), so quitting there does not lose the Salvage Yard. */
static int store_write(shell *s, int slot, const mercs_campaign *c, const mercs_shell_state *v)
{
    int keep = s->salvage_wreck_count > 0 && !s->salvage_used && s->salvage_raw_size &&
               (c->phase == MERCS_CAMPAIGN_RETURN || c->phase == MERCS_CAMPAIGN_DEBRIEF);
    return mercs_store_write_attached(s->save_dir, slot, c, v, keep ? salvage_raw : NULL,
                                      keep ? s->salvage_raw_size : 0, s->status, sizeof s->status);
}

/* A loaded generation's wreck list, restored only before the debrief is left. */
static void restore_salvage(shell *s, int slot, const mercs_campaign *c)
{
    size_t size = 0, count = 0;
    s->salvage_wreck_count = 0; s->salvage_used = 0; s->salvage_raw_size = 0;
    if ((c->phase != MERCS_CAMPAIGN_RETURN && c->phase != MERCS_CAMPAIGN_DEBRIEF) ||
        mercs_store_read_salvage(s->save_dir, slot, salvage_raw, sizeof salvage_raw, &size) ||
        mercs_salvage_decode(salvage_raw, size, s->salvage_wrecks, MERCS_SALVAGE_MAX_WRECKS, &count)) return;
    s->salvage_wreck_count = (int)count; s->salvage_raw_size = size;
    fprintf(stderr, "mercs shell: restored salvage wrecks=%d\n", s->salvage_wreck_count);
}

static void install_career(shell *s, mercs_campaign *next, const mercs_shell_state *view)
{
    mercs_campaign_free(&s->campaign);
    s->campaign = *next; memset(next, 0, sizeof *next);
    s->career = *view;
    s->selected = s->campaign.selected; s->accepted = s->campaign.accepted;
    s->mode = s->career.mode;
    s->regular_unassigned = s->career.owned_index < 0;
    s->training_choice = s->career.owned_index;
    snprintf(s->pilot, sizeof s->pilot, "%s", s->career.pilot);
    snprintf(s->unit, sizeof s->unit, "%s", s->career.unit);
}

/* Mission Command entry (DOS 0x3cb60 -> 0x3ca00 / 0x3caa8) gives an unassigned player owned mech 0; see
 * mercs_economy_command_assign. Committed like a Duty Roster change; failure leaves the career unchanged. */
static void command_auto_assign(shell *s)
{
    mercs_campaign next;
    mercs_shell_state *view;
    if (!s->career.registered || !s->campaign.document) return;
    {
        mercs_save_state *probe = mercs_save_clone(s->campaign.document);
        int rc = probe ? mercs_economy_command_assign(probe) : -1;
        mercs_save_free(probe);
        if (rc != 1) return;
    }
    if (stage_career(s, &next, &view)) return;
    if (mercs_economy_command_assign(next.document) == 1 &&
        !mercs_campaign_hydrate(s->archive, &next, view, s->status, sizeof s->status)) {
        next.resume_page = PAGE_COMMAND;
        if (!store_write(s, -1, &next, view)) {
            install_career(s, &next, view);
            fprintf(stderr, "mercs shell: command-assigned player=%s mech=%s\n", s->career.pilot, s->career.starter_name);
        }
    }
    mercs_campaign_free(&next); free(view);
}

static int commit_career_cue(shell *s, mercs_campaign *next,
                             const mercs_shell_state *view, int page, int cue)
{
    mercs_debrief_view prepared = {0};
    if (page == PAGE_DEBRIEF) {
        const char *scene = next->accepted >= 0 && next->accepted < view->mission_count ?
                            view->missions[next->accepted].scene : "";
        /* A missing/corrupt authored report must leave the durable RETURN
         * generation retryable before the debrief phase commits. */
        if (mercs_debrief_view_prepare(&prepared, s->art, s->archive,
                next->document, view, &next->terms, &next->report,
                scene, s->status, sizeof s->status)) return -1;
    }
    next->resume_page = page;
    if (store_write(s, -1, next, view)) {
        mercs_debrief_view_free(&prepared); return -1;
    }
    install_career(s, next, view);
    if (prepared.ready) {
        mercs_debrief_view_free(&s->debrief_view);
        s->debrief_view = prepared;
        s->debrief_prepared = 1;
    }
    if (cue >= 0) play_cue(s, (mercs_shell_audio_cue)cue);
    if (page >= 0 && s->running) set_page(s, page);
    return 0;
}

static int commit_career(shell *s, mercs_campaign *next,
                          const mercs_shell_state *view, int page)
{ return commit_career_cue(s, next, view, page, -1); }

static void accept_contract(shell *s)
{
    mercs_campaign next;
    mercs_shell_state *view;
    if (stage_career(s, &next, &view)) return;
    if (!mercs_campaign_accept(s->archive, &next, view, s->selected, s->status, sizeof s->status) &&
        !commit_career(s, &next, view, PAGE_COMPUTER))
        fprintf(stderr, "mercs shell: contract-accepted=%s\n", s->campaign.campaign);
    mercs_campaign_free(&next); free(view);
}

/* Original novice hall boarding selects the authored successor and transfers
 * its roster at event0xc016. Chained next-mission/replay debriefs do not. */
static int accept_training_boarding(shell *s)
{
    mercs_campaign next;
    mercs_shell_state *view;
    uint32_t choice;
    int rc = -1;
    if (s->mode || s->campaign.active) return 0;
    if (!s->campaign.document) return -1;
    choice = le32(mercs_save_header(s->campaign.document) + 0x239e);
    if (choice >= (uint32_t)s->career.mission_count) {
        snprintf(s->status, sizeof s->status, "The authored next training contract is unavailable."); return -1;
    }
    if (stage_career(s, &next, &view)) return -1;
    if (!mercs_campaign_accept(s->archive, &next, view, (int)choice, s->status, sizeof s->status))
        rc = commit_career(s, &next, view, PAGE_HALL);
    mercs_campaign_free(&next); free(view); return rc;
}

static void roster_change(shell *s, int direction, int aero)
{
    mercs_campaign next;
    mercs_shell_state *view;
    int rc;
    if (stage_career(s, &next, &view)) return;
    rc = aero ? mercs_economy_toggle_aerotech(next.document, 0) :
                mercs_economy_cycle_pilot(next.document, s->roster_view.selected, direction);
    if (rc == MERCS_ECONOMY_EMPTY) {
        if (!mercs_dialog_open(&s->dialog, "There are no more mechs in inventory#Ok", 0))
            (void)run_dialog(s);
    } else if (rc) snprintf(s->status, sizeof s->status, "The selected pilot's assignment could not be changed.");
    else if (!mercs_campaign_hydrate(s->archive, &next, view, s->status, sizeof s->status)) {
        next.resume_page = PAGE_ROSTER;
        if (!store_write(s, -1, &next, view)) {
            install_career(s, &next, view);
            mercs_roster_view_prepare(&s->roster_view, s->campaign.document, 0);
        }
    }
    mercs_campaign_free(&next); free(view);
}

static int roster_ready(shell *s)
{
    int mates=0,aero=0,rc=mercs_economy_roster_ready(s->campaign.document,&mates,&aero);
    char notice[128];
    if(!rc)return 1;
    if(rc==MERCS_ECONOMY_PLAYER)strcpy(notice,"You must assign yourself a mech!#Ok");
    else if(rc==MERCS_ECONOMY_LIMIT){
        if(mates>2)strcpy(notice,"You may assign a max of 2 starmates!#Ok");
        else snprintf(notice,sizeof notice,"You can assign only %d starmates|for this mission#Ok",
                      mev_i32(mercs_save_header(s->campaign.document)+0x2df7));
    }else{snprintf(s->status,sizeof s->status,"The saved pilot and mech assignments are inconsistent.");return 0;}
    if(play_cue(s,MSA_CUE_ROSTER_REJECT)&&!mercs_dialog_open(&s->dialog,notice,0))(void)run_dialog(s);
    return 0;
}

static int prepare_roster_launch(shell *s,mercs_campaign *career,mercs_shell_state *view,
                                  const char *overlay,mercs_exchange_launch *launch)
{
    mercs_shell_deploy_pilot pilots[3];mercs_shell_deploy_aircraft support,*aircraft=NULL;
    size_t i,n=1;int player=0,rc;
    memset(pilots,0,sizeof pilots);
    for(i=0;i<mercs_save_list_count(career->document,MERCS_SAVE_OWNED_PILOTS);i++){
        const uint8_t *p=mercs_save_list_record(career->document,MERCS_SAVE_OWNED_PILOTS,i);
        size_t at;int k,j;uint32_t mech=le32(p+0x20);
        if(mech==UINT32_MAX)continue;
        if(p[0x2064]){if(player++)goto invalid;at=0;}else{if(n>=3)goto invalid;at=n++;}
        for(k=0;k<view->owned_count;k++)if(mercs_career_instance_id(&view->owned[k].current)==mech)break;
        if(k==view->owned_count)goto invalid;
        pilots[at].owned_index=k;pilots[at].pilot_instance_id=le32(p+0x24);
        memcpy(pilots[at].name,p,21);pilots[at].name[21]=0;
        for(j=0;j<8;j++){
            unsigned v=(unsigned)p[0x28+j*2]|(unsigned)p[0x29+j*2]<<8;
            pilots[at].attributes[j]=(int16_t)(v<=INT16_MAX?(int)v:(int)v-65536);
        }
    }
    if(!player)goto invalid;
    if(mev_i32(mercs_save_header(career->document)+0x2df7)>=3){
        const uint8_t *tech=mercs_save_list_record(career->document,MERCS_SAVE_OWNED_TECHS,0);
        if(tech&&tech[0x2c]==1){
            int j;memset(&support,0,sizeof support);aircraft=&support;
            memcpy(support.stock,tech+0x4d,8);memcpy(support.name,tech,21);
            support.temporary_instance_id=le32(mercs_save_header(career->document)+0x15e);
            for(j=0;j<8;j++){
                unsigned v=(unsigned)tech[0x2d+j*2]|(unsigned)tech[0x2e + j*2]<<8;
                support.attributes[j]=(int16_t)(v<=INT16_MAX?(int)v:(int)v-65536);
            }
        }
    }
    rc=mercs_shell_prepare_roster(s->archive,view,career->accepted,overlay,pilots,n,aircraft,launch,s->status,sizeof s->status);
    if(!rc&&aircraft){
        uint8_t *h=mercs_save_header_edit(career->document)+0x15e;uint32_t id=support.temporary_instance_id+1u;
        for(i=0;i<4;i++)h[i]=(uint8_t)(id>>(8*i));
    }
    return rc;
invalid:
    snprintf(s->status,sizeof s->status,"The selected Duty Roster cannot be deployed.");return -1;
}

static int prepare_original_panel(shell *s, int which);

static int record_earned_score_document(shell *s,const mercs_save_state *document,const char *token)
{
    mercs_lords_row earned;
    char overlay[PORTCFG_PATH],fallback[PORTCFG_PATH];
    const char *source=NULL;
    if (s->mode!=1) return 0;
    if (prepare_original_panel(s,0)) return -1;
    if (portcfg_overlay_dir(s->cfg,"retail",overlay,sizeof overlay,1) ||
        mercs_scores_earned(document,&earned)) {
        snprintf(s->status,sizeof s->status,"The earned Merc Lords result could not be prepared.");return -1;
    }
    if (!dp_resolve(s->panels->scores_filename,fallback,sizeof fallback)) source=fallback;
    return mercs_scores_commit(overlay,source,&s->panels->defaults,token,
                               &earned,s->status,sizeof s->status);
}

static int record_earned_score(shell *s)
{ return record_earned_score_document(s,s->campaign.document,s->career.last_return_token); }

static int record_disband_score(shell *s,const mercs_save_state *staged)
{
    char token[33];
    if (mercs_scores_document_token(staged,token)) {
        snprintf(s->status,sizeof s->status,"The disbanded unit's Merc Lords result could not be prepared.");return -1;
    }
    return record_earned_score_document(s,staged,token);
}

static void salvage_debrief_note(shell *s);
static int game_question(shell *s, const char *message, int default_choice);
static void game_notice(shell *s, const char *message);

static void enter_debrief(shell *s)
{
    mercs_campaign next;
    mercs_shell_state *view;
    if (s->campaign.phase != MERCS_CAMPAIGN_RETURN) return;
    if (s->campaign.entry.movie == MERCS_DEBRIEF_MOVIE_EJECT) play_movie(s, "MEJECT");
    else if (s->campaign.entry.movie == MERCS_DEBRIEF_MOVIE_DEATH) play_movie(s, "MDEATH");
    if (!s->running || stage_career(s, &next, &view)) return;
    salvage_debrief_note(s);
    if (!mercs_campaign_enter_debrief(s->archive, &next, view, s->status, sizeof s->status))
        commit_career(s, &next, view,
                       next.phase == MERCS_CAMPAIGN_ENDED ? PAGE_MENU : PAGE_DEBRIEF);
    mercs_campaign_free(&next); free(view);
}

/* ---- Titanium's dynamic salvage (see mercssalvage.h) ---- */

static const char *salvage_scene(const shell *s)
{
    return s->campaign.accepted >= 0 && s->campaign.accepted < s->career.mission_count ?
           s->career.missions[s->campaign.accepted].scene : "";
}

/* Titanium's shell art: its DATABASE.MW2 (the Salvage background) and SMK\ */
static int salvage_assets(const shell *s, char database[PORTCFG_PATH * 2], char dir[PORTCFG_PATH])
{
    char probe[PORTCFG_PATH * 2];
    if (!s->cfg->titanium[0]) return -1;
    snprintf(dir, PORTCFG_PATH, "%s", s->cfg->titanium);
    if (dp_resolve_in(dir, "DATABASE.MW2", database, PORTCFG_PATH * 2) ||
        dp_resolve_in(dir, "SMK\\MSBACK.SHP", probe, sizeof probe)) return -1;
    return 0;
}

/* The debrief's Salvage button (Titanium 0x10020000 area): a won mercenary
 * contract whose mission allows salvage, with wrecks recorded - or OUT3, which
 * always opens (the Kodiak). Once per return. */
static int salvage_available(const shell *s)
{
    char database[PORTCFG_PATH * 2], dir[PORTCFG_PATH];
    const char *scene = salvage_scene(s);
    if (s->salvage_used || s->campaign.phase != MERCS_CAMPAIGN_DEBRIEF || s->career.mode != 1 ||
        !s->campaign.report.success || salvage_assets(s, database, dir)) return 0;
    if (mercs_salvage_is_out3(scene)) return 1;
    return s->salvage_wreck_count > 0 &&
           mercs_salvage_allowed(scene, !strncasecmp(s->campaign.campaign, "RAN", 3));
}

/* Debrief text (0x1001d4ec): wrecks were recorded but the mission gave no
 * opportunity for salvage. */
static void salvage_debrief_note(shell *s)
{
    const char *scene = salvage_scene(s);
    mdv_salvage_note = s->salvage_wreck_count > 0 && !s->salvage_used && s->career.mode == 1 &&
        !mercs_salvage_is_out3(scene) &&
        !mercs_salvage_allowed(scene, !strncasecmp(s->campaign.campaign, "RAN", 3)) ?
        "\\nNo opportunity for salvage was available.\\n" : NULL;
}

static void close_salvage(shell *s)
{
    mercs_salvage_yard_free(&s->salvage_yard);
    memset(&s->salvage_view, 0, sizeof s->salvage_view);
    s->salvage_open = 0;
    mercs_art_begin_page(s->art);
}

static void open_salvage(shell *s)
{
    char database[PORTCFG_PATH * 2], dir[PORTCFG_PATH];
    int rc;
    if (!salvage_available(s)) return;
    if (salvage_assets(s, database, dir)) return;
    if (!s->salvage_bg_ok) {
        shelldb db;
        uint8_t *bytes = NULL;
        long length;
        if (shelldb_open(database, &db) == 0) {
            /* resource 47, one-based (Titanium 0x10069bc0: DAT_100b9e00) */
            length = shelldb_entry(&db, 46, &bytes);
            if (length > 0 && !shell_pcx(bytes, (size_t)length, &s->salvage_bg, NULL)) s->salvage_bg_ok = 1;
            free(bytes); shelldb_close(&db);
        }
        if (!s->salvage_bg_ok) { game_notice(s, "Titanium's Salvage background could not be read.#Ok"); return; }
    }
    rc = mercs_salvage_yard_open(&s->salvage_yard, s->archive, s->salvage_wrecks,
                                 (size_t)s->salvage_wreck_count, mercs_salvage_is_out3(salvage_scene(s)),
                                 s->campaign.document);
    if (rc) { game_notice(s, "No salvage.|#Ok"); s->salvage_used = 1; return; }
    mercs_art_set_extra_dir(s->art, dir);
    mercs_art_begin_page(s->art);
    mercs_salvage_view_open(&s->salvage_view, &s->salvage_yard, &s->salvage_bg);
    s->salvage_open = 1;
    fprintf(stderr, "mercs shell: salvage-yard wrecks=%zu\n", s->salvage_yard.count);
}

static void continue_debrief(shell *s);

/* Exit: "Accept Salvage?" (0x10065e2f case 0). Yes commits the yard's career
 * and leaves the debrief, as Titanium does; No stays in the yard. */
static void exit_salvage(shell *s)
{
    mercs_campaign next;
    mercs_shell_state *view;
    if (!game_question(s, "Accept Salvage?|", 0)) return;
    if (stage_career(s, &next, &view)) return;
    mercs_save_swap(next.document, s->salvage_yard.document);
    s->salvage_used = 1;   /* the accepted yard's generation no longer carries the wrecks */
    if (mercs_campaign_hydrate(s->archive, &next, view, s->status, sizeof s->status) ||
        commit_career(s, &next, view, PAGE_DEBRIEF)) {
        s->salvage_used = 0;
        mercs_save_swap(next.document, s->salvage_yard.document);
        mercs_campaign_free(&next); free(view); return;
    }
    mercs_campaign_free(&next); free(view);
    s->salvage_used = 1; s->salvage_wreck_count = 0;
    close_salvage(s);
    continue_debrief(s);
}

static void salvage_click(shell *s, int x, int y)
{
    const char *notice = NULL;
    int at = mercs_salvage_view_click(&s->salvage_view, &s->salvage_yard, s->archive, x, y, &notice);
    if (at == MSV_NOTICE && notice) game_notice(s, notice);
    else if (at == MSV_EXIT) exit_salvage(s);
}

static void continue_debrief(shell *s)
{
    mercs_campaign next;
    mercs_shell_state *view;
    mercs_debrief_gate gate;
    unsigned i;
    int page;
    if (s->campaign.phase != MERCS_CAMPAIGN_DEBRIEF) return;
    if (mercs_campaign_preflight(&s->campaign, &s->career, &gate, s->status, sizeof s->status)) return;
    /* Original bankruptcy/terminal ending call0x46f58 before settlement. A
     * recoverable receipt binds this write to the ending return, so a failed
     * following career-generation commit cannot submit the same score twice. */
    if ((gate.reason==MERCS_DEBRIEF_GATE_BANKRUPT || gate.reason==MERCS_DEBRIEF_GATE_ENDING) &&
        record_earned_score(s)) return;
    for (i = 0; i < gate.movie_count && s->running; i++) play_movie(s, gate.movies[i]);
    if (!gate.block_settlement && s->running &&
        (s->campaign.terms.campaign_type == 2 || s->campaign.terms.campaign_type == 3)) {
        int death_branch = !s->campaign.report.success &&
            !SDL_strcasecmp(s->campaign.terms.failure_next, "DEATH");
        /* Original terminal leave0x3c084 lands before settlement. */
        if (s->mode ? !death_branch : s->campaign.report.success) play_movie(s, "MLAND");
    }
    if (!s->running || stage_career(s, &next, &view)) return;
    if (!mercs_campaign_continue(s->archive, &next, view, s->status, sizeof s->status)) {
        s->salvage_wreck_count = 0; s->salvage_used = 0;   /* Titanium removes salvage.mw2 on leaving the debrief */
        page = next.phase == MERCS_CAMPAIGN_ENDED ? PAGE_MENU : next.active ? PAGE_BRIEFING : PAGE_HALL;
        if (page == PAGE_BRIEFING) s->brief_tab = 0;
        commit_career(s, &next, view, page);
    }
    mercs_campaign_free(&next); free(view);
}

static int original_slot_path(shell *s,int slot,char path[PORTCFG_PATH])
{
    char overlay[PORTCFG_PATH],name[32];
    path[0]=0;if(slot<0||slot>9)return -1;
    snprintf(name,sizeof name,"mersave%d.sav",slot);
    if(!portcfg_overlay_dir(s->cfg,"retail",overlay,sizeof overlay,0)&&
       !dp_resolve_in(overlay,name,path,PORTCFG_PATH))return 0;
    return dp_resolve(name,path,PORTCFG_PATH);
}

static void refresh_save_titles(shell *s)
{
    int i;
    for (i = 0; i < 10; i++) {
        int native;
        s->save_titles[i][0] = 0;
        native=mercs_store_title(s->save_dir, i, s->save_titles[i], sizeof s->save_titles[i]);
        s->save_native[i]=native==0;
        if(original_slot_path(s,i,s->save_original_path[i]))s->save_original_path[i][0]=0;
        if(native==1&&s->save_original_path[i][0])
            native=mercs_store_original_title(s->save_original_path[i],s->save_titles[i],sizeof s->save_titles[i]);
        if (native < 0)
            snprintf(s->save_titles[i], sizeof s->save_titles[i], "Unreadable saved game");
    }
}

static int slot_has_save(shell *s,int slot)
{
    char title[256],path[PORTCFG_PATH];
    return mercs_store_title(s->save_dir,slot,title,sizeof title)!=1 ||
           !original_slot_path(s,slot,path);
}

static int game_question(shell *s, const char *message, int default_choice)
{
    char spec[512];
    int n = snprintf(spec, sizeof spec, "%s#Yes|No", message);
    if (n < 0 || (size_t)n >= sizeof spec ||
        mercs_dialog_open(&s->dialog, spec, default_choice)) {
        fprintf(stderr, "mercs shell: invalid confirmation: %s\n", message);
        return 0;
    }
    return run_dialog(s) == 0 && s->running;
}

static void game_notice(shell *s, const char *message)
{
    fprintf(stderr, "mercs shell: %s\n", message);
    if (!mercs_dialog_notice(&s->dialog, s->art, message)) (void)run_dialog(s);
}

static int prepare_economy_page(shell *s, int fresh)
{
    int rc = 0;
    const mercs_save_state *d = s->campaign.document;
    if (s->page == PAGE_FINANCE || s->page == PAGE_DROP_FINANCE)
        rc = mercs_finance_view_prepare(&s->finance_view, s->art, d, s->page == PAGE_DROP_FINANCE);
    else if (s->page == PAGE_INVENTORY || s->page == PAGE_DROP_INVENTORY)
        rc = mercs_inventory_view_prepare(&s->inventory_view, d, s->page == PAGE_DROP_INVENTORY,
                                           fresh ? 0 : s->inventory_view.category);
    else if (s->page == PAGE_TRADING) {
        if (fresh) mercs_trading_view_free(&s->trading_view);
        rc = mercs_trading_view_prepare(&s->trading_view, d, fresh ? 0 : s->trading_view.category);
    } else if (s->page == PAGE_PERSONNEL) mercs_personnel_view_free(&s->personnel_view);
    else if (s->page == PAGE_FACTORY) mercs_factory_view_prepare(&s->factory_view, d, fresh);
    else if (s->page == PAGE_ROSTER) mercs_roster_view_prepare(&s->roster_view, d, fresh);
    else if (s->page == PAGE_GROUPS) mercs_groups_view_prepare(&s->groups_view, d, fresh);
    if (rc) snprintf(s->status, sizeof s->status, "The original career screen could not be prepared.");
    return rc;
}

/* Retail transactions mutate the complete save. Rehydrate before the store's
 * normal view->document synchronization, or stale view funds/assignments
 * would undo a successful transaction. Keep the live page and document until
 * the durable generation has committed; a failed write can safely retry. */
static void economy_transaction(shell *s, int action)
{
    mercs_campaign next;
    mercs_shell_state *view;
    int rc, disband = 0, ammo = 0, list = s->personnel_view.list;
    size_t index = s->personnel_view.index;
    if (s->page == PAGE_TRADING &&
        mercs_trading_view_transaction(&s->trading_view, action, &ammo, &index)) {
        snprintf(s->status, sizeof s->status, "%s", action ? "Click on item to buy first" : "Click on item to sell first");
        return;
    }
    if (s->page == PAGE_FACTORY) {
        if (s->factory_view.side != action) return;
        index = s->factory_view.selected;
    }
    if (stage_career(s, &next, &view)) return;
    if (s->page == PAGE_TRADING) rc = mercs_economy_trade_item(next.document, ammo, action, index);
    else if (s->page == PAGE_PERSONNEL && list >= 0 && list < 4) {
        enum mercs_personnel_kind kind = list < 2 ? MERCS_PERSONNEL_MECHWARRIOR : MERCS_PERSONNEL_AEROTECH;
        rc = action ? mercs_economy_hire(next.document, kind, index) :
                      mercs_economy_dismiss(next.document, kind, index);
    } else if (s->page == PAGE_FACTORY) rc = mercs_economy_trade_mech(next.document, action, index);
    else rc = MERCS_ECONOMY_INVALID;
    if (!rc && s->page == PAGE_FACTORY && !game_question(s, action ? "Buy this Mech?" : "Sell this Mech?", action ? 0 : 1)) {
        mercs_campaign_free(&next); free(view); return;
    }
    if (rc == MERCS_ECONOMY_FUNDS) snprintf(s->status, sizeof s->status, "Insufficient Funds!");
    else if (rc == MERCS_ECONOMY_CAPACITY && s->page == PAGE_PERSONNEL)
        snprintf(s->status, sizeof s->status, "%s", list < 2 ? "Unable to hire more pilots" :
                 "The dropship can only transport\none Aerotech");
    else if (rc) snprintf(s->status, sizeof s->status, "The selected career transaction could not be completed.");
    else if (!mercs_campaign_hydrate(s->archive, &next, view, s->status, sizeof s->status)) {
        if (!action && (s->page == PAGE_TRADING || s->page == PAGE_FACTORY)) {
            int viable;
            if (mercs_economy_viable(next.document, &viable)) {
                snprintf(s->status, sizeof s->status, "The unit's remaining assets could not be valued."); goto done;
            }
            if (!viable) {
                /* Original sale callers0x29526/0x4e596 call0x5c118 before
                 * returning to the menu. Score the staged post-sale career;
                 * a durable receipt makes retry after a save failure safe. */
                if (record_disband_score(s, next.document)) goto done;
                disband = 1; next.phase = MERCS_CAMPAIGN_ENDED;
                next.active = 0; next.accepted = -1; next.procedural_year = 0;
                next.procedural_seed_override = 0; next.procedural_seed = 0;
                next.launch_token[0] = next.launch_edition[0] = 0;
                view->last_return_valid = 0; view->last_return_mission = -1;
            }
        }
        next.resume_page = disband ? PAGE_MENU : s->page;
        if (!store_write(s, -1, &next, view)) {
            install_career(s, &next, view);
            (void)prepare_economy_page(s, 0);
            fprintf(stderr, "mercs shell: career transaction page=%s action=%d funds=%ld\n",
                    page_names[s->page], action, (long)s->career.funds);
            if (disband) {
                game_notice(s, "You cannot afford even the cheapest mech|Your unit disbands in disgrace!#Ok");
                if (s->running) play_movie(s, "mpoor");
                if (s->running) set_page(s, PAGE_MENU);
            }
        }
    }
done:
    mercs_campaign_free(&next); free(view);
}

/* Titanium Armsdlr.cpp repair (0x10004fff): the selected item's damaged copy
 * is repaired for 15% of its value. */
static void trading_repair(shell *s)
{
    mercs_campaign next;
    mercs_shell_state *view;
    int ammo = 0, rc;
    size_t index = 0;
    if (mercs_trading_view_transaction(&s->trading_view, 0, &ammo, &index)) {
        game_notice(s, "No repairs are required!#Ok"); return;
    }
    if (stage_career(s, &next, &view)) return;
    rc = mercs_economy_repair_item(next.document, ammo, index);
    if (rc == MERCS_ECONOMY_EMPTY) game_notice(s, "No repairs are required!#Ok");
    else if (rc == MERCS_ECONOMY_FUNDS) game_notice(s, "Insufficent funds!#Ok");
    else if (rc) snprintf(s->status, sizeof s->status, "The selected repair could not be completed.");
    else if (!mercs_campaign_hydrate(s->archive, &next, view, s->status, sizeof s->status)) {
        next.resume_page = s->page;
        if (!store_write(s, -1, &next, view)) {
            install_career(s, &next, view);
            (void)prepare_economy_page(s, 0);
        }
    }
    mercs_campaign_free(&next); free(view);
}

static void change_weapon_group(shell *s, int slot)
{
    mercs_campaign next;
    mercs_shell_state *view;
    if (stage_career(s, &next, &view)) return;
    if (mercs_economy_weapon_group(next.document, s->groups_view.selected, slot))
        snprintf(s->status, sizeof s->status, "The selected weapon group could not be changed.");
    else if (!mercs_campaign_hydrate(s->archive, &next, view, s->status, sizeof s->status)) {
        next.resume_page = PAGE_GROUPS;
        if (!store_write(s, -1, &next, view))
            install_career(s, &next, view);
    }
    mercs_campaign_free(&next); free(view);
}

static void open_save_load(shell *s)
{
    int origin = s->page == PAGE_SETTINGS ? s->settings_return : s->page;
    if (s->page == PAGE_SAVE) return;
    if (s->mechlab) {
        int instant=s->mechlab_instant;
        s->save_mechlab_index=(int)mercs_mechlab_index(s->mechlab);
        finish_mechlab_name(s,0);
        close_mechlab(s,!mercs_mechlab_dirty(s->mechlab) ||
                         game_question(s,"Accept customizations?",0),s->mechlab_return);
        if (!s->running || s->mechlab || instant) return;
    }
    if (s->repair)
        close_repair(s, s->repair_finished || !mercs_repair_current(s->repair)->bytes[0x84c] ||
                         game_question(s, "Accept repairs?", 0), s->repair_return);
    if (!s->running || s->repair) return;
    /* Original c02e settles repairs but retains the originating repair event.
     * Save/EXIT opens a new repair session in that same office/command context. */
    s->save_return = origin;
    refresh_save_titles(s); set_page(s, PAGE_SAVE);
}

static int can_load_slot(const shell *s)
{ return s->after_save != 1; }

static int can_save_slot(const shell *s)
{
    return s->career.registered && s->save_return != PAGE_MENU &&
           s->save_return != PAGE_INSTANT && s->save_return != PAGE_INSTANT_PARAMS && !s->mechlab_instant &&
           s->campaign.phase != MERCS_CAMPAIGN_ENDED;
}

static void finish_save_load(shell *s)
{
    int intent = s->after_save;
    s->after_save = 0;
    if (intent == 2) s->running = 0;
    if (!s->running) return;
    if (intent == 1) {
        uint8_t *header = mercs_save_header_edit(s->campaign.document);
        if (header) header[0] = 0;
        set_page(s, PAGE_MENU);
    }
    else if (s->save_return == PAGE_REPAIR) {
        if (s->career.owned_count)
            start_repair(s, s->career.owned_index >= 0 ? s->career.owned_index : 0);
        else {
            set_page(s, s->repair_return);
            if (!mercs_dialog_open(&s->dialog, "You have no Mechs to repair!#Ok", 0))
                (void)run_dialog(s);
        }
    } else if (s->save_return == PAGE_MECHLAB) {
        if(s->career.owned_count)
            (void)start_mechlab(s,s->save_mechlab_index>=0&&s->save_mechlab_index<s->career.owned_count?
                                  s->save_mechlab_index:0);
        else set_page(s,s->mechlab_return);
    } else set_page(s, s->save_return);
}

static void finish_save_description(shell *s, int cancel)
{
    if (!s->save_editing) return;
    if (cancel || !s->save_titles[s->save_slot][0])
        snprintf(s->save_titles[s->save_slot], sizeof s->save_titles[s->save_slot], "%s", s->save_edit_backup);
    s->save_editing = 0; s->select_all = 0; SDL_StopTextInput(s->window);
}

static void edit_save_description(shell *s)
{
    snprintf(s->save_edit_backup, sizeof s->save_edit_backup, "%s", s->save_titles[s->save_slot]);
    s->save_editing = 1; s->select_all = 0; SDL_StartTextInput(s->window);
}

static uint32_t original_save_screen(const shell *s,int page)
{
    /* Native subpanels already resume in their owning room. Emit that same
     * original event so a standalone DOS load has a complete room context. */
    if(page==PAGE_REPAIR)page=s->repair_return;
    if(page==PAGE_SETTINGS)page=s->settings_return;
    switch(page){
    case PAGE_DEBRIEF:return MERCS_ORIGINAL_DEBRIEF;
    case PAGE_REGISTER:return MERCS_ORIGINAL_REGISTER;
    case PAGE_HALL:return MERCS_ORIGINAL_HALL;
    case PAGE_NEWS:return MERCS_ORIGINAL_NEWS;
    case PAGE_OFFICE:return MERCS_ORIGINAL_OFFICE;
    case PAGE_FINANCE:case PAGE_INVENTORY:case PAGE_COMPUTER:return MERCS_ORIGINAL_COMPUTER;
    case PAGE_CONTRACTS:return MERCS_ORIGINAL_CONTRACTS;
    case PAGE_DROPSHIP:return MERCS_ORIGINAL_DROPSHIP;
    case PAGE_COMMAND:case PAGE_DROP_FINANCE:case PAGE_DROP_INVENTORY:return MERCS_ORIGINAL_COMMAND;
    case PAGE_ROSTER:return MERCS_ORIGINAL_ROSTER;
    case PAGE_BRIEFING:return MERCS_ORIGINAL_BRIEFING;
    case PAGE_MECHBAY:return MERCS_ORIGINAL_MECHBAY;
    case PAGE_CREW:case PAGE_CREW_BIO:return MERCS_ORIGINAL_MECH_ROOM;
    case PAGE_PERSONNEL:return s->personnel_return==PAGE_MECHOPS?MERCS_ORIGINAL_MECHOPS:MERCS_ORIGINAL_COMPUTER;
    case PAGE_MECHOPS:case PAGE_FACTORY:case PAGE_TRADING:case PAGE_GROUPS:case PAGE_MECHLAB:return MERCS_ORIGINAL_MECHOPS;
    default:return MERCS_ORIGINAL_MENU;
    }
}

static int imported_save_page(uint32_t event)
{
    switch(event){
    case MERCS_ORIGINAL_DEBRIEF:return PAGE_DEBRIEF;
    case MERCS_ORIGINAL_REGISTER:return PAGE_REGISTER;
    case MERCS_ORIGINAL_HALL:return PAGE_HALL;
    case MERCS_ORIGINAL_NEWS:return PAGE_NEWS;
    case MERCS_ORIGINAL_OFFICE:return PAGE_OFFICE;
    case MERCS_ORIGINAL_FINANCE:case MERCS_ORIGINAL_INVENTORY:case MERCS_ORIGINAL_COMPUTER:return PAGE_COMPUTER;
    case MERCS_ORIGINAL_CONTRACTS:return PAGE_CONTRACTS;
    case MERCS_ORIGINAL_DROPSHIP:return PAGE_DROPSHIP;
    case MERCS_ORIGINAL_COMMAND:return PAGE_COMMAND;
    case MERCS_ORIGINAL_ROSTER:return PAGE_ROSTER;
    case MERCS_ORIGINAL_BRIEFING:return PAGE_BRIEFING;
    case MERCS_ORIGINAL_MECH_ROOM:case MERCS_ORIGINAL_CREW_BIO:return PAGE_CREW;
    case MERCS_ORIGINAL_MECHBAY:return PAGE_MECHBAY;
    case MERCS_ORIGINAL_PERSONNEL:case MERCS_ORIGINAL_MECHOPS:case MERCS_ORIGINAL_FACTORY:
    case MERCS_ORIGINAL_TRADING:case MERCS_ORIGINAL_GROUPS:case MERCS_ORIGINAL_MECHLAB:return PAGE_MECHOPS;
    case MERCS_ORIGINAL_REPAIR:return PAGE_REPAIR;
    default:return PAGE_MENU;
    }
}

static void save_slot(shell *s)
{
    mercs_campaign next;
    mercs_shell_state *view;
    char overlay[PORTCFG_PATH],export_error[256];
    if (!can_save_slot(s)) {
        snprintf(s->status, sizeof s->status, "No current game to save."); return;
    }
    finish_save_description(s, 0);
    if (!s->save_titles[s->save_slot][0]) {
        snprintf(s->status, sizeof s->status, "Enter a saved-game description first."); return;
    }
    if (stage_career(s, &next, &view)) return;
    memcpy(mercs_save_header_edit(next.document) + 2, s->save_titles[s->save_slot],
           strlen(s->save_titles[s->save_slot]) + 1);
    mercs_save_header_edit(next.document)[0x239a] = s->after_save == 1;
    mercs_save_header_edit(next.document)[0x239b] = s->after_save == 2;
    next.resume_page = s->save_return == PAGE_REPAIR ? s->repair_return :
                       s->save_return == PAGE_MECHLAB ? PAGE_MECHOPS : s->save_return;
    if(mercs_interchange_prepare(&next,view,original_save_screen(s,next.resume_page),s->status,sizeof s->status)){
        mercs_campaign_free(&next);free(view);return;
    }
    if (!store_write(s, s->save_slot, &next, view)) {
        fprintf(stderr, "mercs shell: saved slot=%d campaign=%s phase=%d\n",
                s->save_slot, next.campaign, next.phase);
        install_career(s, &next, view);
        mercs_save_header_edit(s->campaign.document)[0] = 0;
        if(portcfg_overlay_dir(s->cfg,"retail",overlay,sizeof overlay,1)||
           mercs_store_export_original(s->save_dir,s->save_slot,overlay,export_error,sizeof export_error)){
            snprintf(s->status,sizeof s->status,"The career was saved, but mersave%d.sav could not be exported. Save again to retry.",s->save_slot);
            refresh_save_titles(s);mercs_campaign_free(&next);free(view);return;
        }
        refresh_save_titles(s);
        play_cue(s, MSA_CUE_SAVE_WRITE);
        finish_save_load(s);
    }
    mercs_campaign_free(&next); free(view);
}

static int restored_page(const mercs_campaign *c)
{
    int p = c->resume_page;
    if (c->phase == MERCS_CAMPAIGN_ENDED) return PAGE_MENU;
    if (c->phase == MERCS_CAMPAIGN_DEBRIEF) return PAGE_DEBRIEF;
    if (c->phase == MERCS_CAMPAIGN_RETURN || c->phase == MERCS_CAMPAIGN_DEPLOYED) return PAGE_COMMAND;
    if (p == PAGE_FINANCE || p == PAGE_INVENTORY) return PAGE_COMPUTER;
    if (p == PAGE_PERSONNEL || p == PAGE_TRADING || p == PAGE_MECHOPS || p == PAGE_FACTORY || p == PAGE_GROUPS || p == PAGE_MECHLAB) return PAGE_MECHOPS;
    if (p == PAGE_MECHBAY) return PAGE_MECHBAY;
    if (p == PAGE_CREW || p == PAGE_CREW_BIO) return PAGE_CREW;
    if (p == PAGE_DROP_FINANCE || p == PAGE_DROP_INVENTORY) return c->active ? PAGE_COMMAND : PAGE_HALL;
    if (p == PAGE_SAVE || p == PAGE_REPAIR || p == PAGE_SETTINGS || p == PAGE_REGISTER ||
        p == PAGE_DEBRIEF || p < PAGE_MENU || p > PAGE_SETTINGS)
        return c->active ? PAGE_COMMAND : PAGE_HALL;
    if ((p == PAGE_BRIEFING || p == PAGE_COMMAND || p == PAGE_DROPSHIP) && !c->active) return PAGE_HALL;
    return p;
}

static void load_slot(shell *s, int slot, int autosave)
{
    mercs_campaign next;
    mercs_debrief_view prepared = {0};
    mercs_shell_state *view = calloc(1, sizeof *view);
    int rc,original=0;char path[PORTCFG_PATH];uint32_t original_event=0;
    memset(&next, 0, sizeof next);
    if (!view) { snprintf(s->status, sizeof s->status, "Not enough memory to load the career."); return; }
    if(!autosave&&slot>=0&&slot<10&&!original_slot_path(s,slot,path)){
        char title[256],question[160];int native=mercs_store_title(s->save_dir,slot,title,sizeof title);
        if(native==1)original=1;
        else if(mercs_store_original_differs(s->save_dir,slot,path)!=0){
            snprintf(question,sizeof question,"An original mersave%d.sav also exists.|Load that file instead of the native slot?",slot);
            original=game_question(s,question,1);
            if(!s->running)goto done;
        }
    }
    if(original){
        rc=mercs_store_import_original(path,s->archive,&next,view,&original_event,s->status,sizeof s->status);
        if(!rc)next.resume_page=imported_save_page(original_event);
    }else rc = mercs_store_read(s->save_dir, slot, s->archive, &next, view, s->status, sizeof s->status);
    if (!rc) {
        if (original) { s->salvage_wreck_count = 0; s->salvage_used = 0; s->salvage_raw_size = 0; }
        else restore_salvage(s, slot, &next);
    }
    if (rc == 1 && !autosave) snprintf(s->status, sizeof s->status, "No saved games...");
    if (!rc) {
        int page = restored_page(&next);
        mercs_save_header_edit(next.document)[0x239a] = 0;
        mercs_save_header_edit(next.document)[0x239b] = 0;
        /* Loading a manual slot becomes the working generation before any
         * result transition is resumed. A failed save retains the old game. */
        if (autosave) {
            if (page == PAGE_DEBRIEF) {
                const char *scene = next.accepted >= 0 && next.accepted < view->mission_count ?
                                    view->missions[next.accepted].scene : "";
                if (mercs_debrief_view_prepare(&prepared, s->art, s->archive,
                       next.document, view, &next.terms, &next.report, scene,
                       s->status, sizeof s->status)) goto done;
            }
            install_career(s, &next, view);
            if (prepared.ready) {
                mercs_debrief_view_free(&s->debrief_view); s->debrief_view = prepared;
                memset(&prepared, 0, sizeof prepared); s->debrief_prepared = 1;
            }
            set_page(s, page);
        }
        else if (commit_career_cue(s, &next, view, page, MSA_CUE_SAVE_LOAD)) goto done;
        s->after_save = 0;
        fprintf(stderr, "mercs shell: loaded slot=%d phase=%d campaign=%s\n",
                slot, s->campaign.phase, s->campaign.campaign);
        resume_campaign(s);
    }
done:
    mercs_debrief_view_free(&prepared);
    mercs_campaign_free(&next); free(view);
}

static void prepare_lines(shell *s)
{
    const char *raw = NULL;
    char *crew_raw = NULL;
    int width = 0;
    mercs_text_free(&s->body);
    s->scroll = 0;
    if (s->page == PAGE_CREW_BIO) {
        raw=crew_raw=mercs_crew_text(s->archive,s->crew_selected);width=549;
        if(!raw)snprintf(s->status,sizeof s->status,"The original crew biography could not be read.");
    }
    else if (s->page == PAGE_NEWS) { raw = s->career.news_raw; width = ui_news_text_w; }
    else if (s->page == PAGE_CONTRACTS && s->selected >= 0 && s->selected < s->career.mission_count) {
        raw = s->career.missions[s->selected].contract_raw; width = ui_contracts_text_w;
    } else if (s->page == PAGE_BRIEFING && s->accepted >= 0 && s->accepted < s->career.mission_count) {
        raw = s->career.missions[s->accepted].briefing_raw; width = ui_mission_briefing_text_w;
        if (s->brief_tab) {
            const char *section = strstr(raw, "\\s");
            if (section) raw = section + 2;
        }
    }
    if (s->page == PAGE_DEBRIEF && s->campaign.phase == MERCS_CAMPAIGN_DEBRIEF) {
        const char *scene = s->accepted >= 0 && s->accepted < s->career.mission_count ?
                            s->career.missions[s->accepted].scene : "";
        if (s->debrief_prepared) s->debrief_prepared = 0;
        else if (mercs_debrief_view_prepare(&s->debrief_view, s->art, s->archive,
               s->campaign.document, &s->career, &s->campaign.terms,
               &s->campaign.report, scene, s->status, sizeof s->status)) return;
    }
    if (raw && mercs_text_prepare_names(&s->body, s->art, raw, s->pilot, s->unit, "",
                                       MERCS_UI_FONT_BODY, width))
        snprintf(s->status, sizeof s->status, "The original screen text could not be laid out.");
    free(crew_raw);
    reset_text_reveal(s);
}

static void reset_text_reveal(shell *s)
{
    mercs_shell_audio_text_reveal(s->shell_audio, 0);
    s->reveal_rows = s->reveal_total = s->reveal_active = 0;
    if (s->page == PAGE_NEWS || s->page == PAGE_CONTRACTS || s->page == PAGE_BRIEFING || s->page == PAGE_CREW_BIO)
        s->reveal_total = s->body.rows;
    else if (s->page == PAGE_DEBRIEF && s->debrief_view.ready)
        s->reveal_total = s->debrief_view.text[s->debrief_view.content_mode ? 1 : 0].rows;
    s->reveal_active = s->reveal_total > 0;
    mercs_shell_audio_text_reveal(s->shell_audio, s->reveal_active);
}

static void poll_shell(shell *s)
{
    mercs_shell_audio_idle_enabled(s->shell_audio,
        !(s->page == PAGE_NEWS && s->mode && !s->letter_heard));
    mercs_shell_audio_tick(s->shell_audio);
    if (!s->dialog.active && !s->game_menu.active && !s->panel_mode && s->reveal_active) {
        if (s->reveal_rows < s->reveal_total) s->reveal_rows++;
        else {
            s->reveal_active = 0;
            mercs_shell_audio_text_reveal(s->shell_audio, 0);
        }
    }
}

static void register_pilot(shell *s)
{
    mercs_campaign next;
    mercs_shell_state *view;
    if (!s->pilot[0]) { s->focus = 0; return; }
    if (!s->unit[0]) { s->focus = 1; return; }
    memset(&next, 0, sizeof next);
    view = calloc(1, sizeof *view);
    if (!view) { snprintf(s->status, sizeof s->status, "Not enough memory to register the pilot."); return; }
    if (!mercs_campaign_new(s->archive, s->mode, s->pilot, s->unit, &next, view, s->status, sizeof s->status)) {
        /* Original menu departure seeds from its event-loop counter. Native
         * elapsed UI ticks are an explicit timing policy; the original LCG
         * and thresholds are preserved, not a globally identical stream. */
        next.rng_state = (uint32_t)SDL_GetTicks();
        if (!commit_career_cue(s, &next, view, PAGE_NEWS, MSA_CUE_REGISTER_ACCEPT)) {
            s->letter_heard = 0;
            fprintf(stderr, "mercs shell: registered pilot=%s unit=%s mode=%d contracts=%d starter=%s\n",
                    s->career.pilot, s->career.unit, s->career.mode, s->career.mission_count, s->career.starter_loadout);
        }
    }
    mercs_campaign_free(&next); free(view);
}

/* Rehydrate resource text from the new edition while retaining the complete
 * owned document and native continuation. This also covers progressed careers
 * whose fleet and contract no longer resemble the initial registration. */
static int change_game_look_options(shell *s, int edition, const mercs_options *preferences, int copy_resolution)
{
    portcfg candidate = *s->cfg;
    prj_archive *archive = NULL;
    mercs_campaign next, snapshot;
    mercs_debrief_view prepared = {0};
    mercs_shell_state *view = NULL;
    uint8_t *original = NULL, resume[MERCS_CAMPAIGN_RESUME_BYTES];
    size_t original_size = 0;
    char models[PORTCFG_PATH], textures[PORTCFG_PATH], kind[PORTCFG_PATH];
    char sky[PORTCFG_PATH], fog[PORTCFG_PATH], overlay[PORTCFG_PATH], error[384];
    const char *profile = portcfg_mercs_overlay(edition);
    mercs_options next_options;
    int result = -1;
    memset(&next, 0, sizeof next); memset(&snapshot, 0, sizeof snapshot);
    if (edition == s->edition) return 0;
    if (!profile) return -1;
    if (s->campaign.phase == MERCS_CAMPAIGN_DEPLOYED || s->campaign.phase == MERCS_CAMPAIGN_RETURN) {
        snprintf(s->status, sizeof s->status, "Finish processing the previous mission before changing the renderer."); return -1;
    }
    if (s->repair) { snprintf(s->status, sizeof s->status, "Finish the current repair before changing the game look."); return -1; }
    if (s->mechlab) { snprintf(s->status, sizeof s->status, "Finish the current customization before changing the game look."); return -1; }
    if (portcfg_edition(&candidate, edition, models, textures, kind, sky, fog)) {
        snprintf(s->status, sizeof s->status, "The game folder does not contain the %s graphics files.", portcfg_mercs_label(edition));
        return -1;
    }
    if (portcfg_overlay_dir(&candidate, profile, overlay, sizeof overlay, 1)) {
        snprintf(s->status, sizeof s->status, "The selected look's installation folder is not usable: %s", strerror(errno)); return -1;
    }
    archive = prj_open(models, error, sizeof error);
    if (!archive) { snprintf(s->status, sizeof s->status, "Cannot open the selected graphics data: %.330s", error); return -1; }
    if (mercs_options_load(&next_options, overlay, candidate.cd, error, sizeof error)) {
        snprintf(s->status, sizeof s->status, "%s", error); goto failed;
    }
    if (s->career.registered) {
        view = calloc(1, sizeof *view);
        if (!view || mercs_campaign_clone(&s->campaign, &snapshot)) {
            snprintf(s->status, sizeof s->status, "Not enough memory to change graphics."); goto failed;
        }
        snapshot.selected = s->selected; snapshot.accepted = s->accepted;
        if (mercs_campaign_sync(&snapshot, &s->career, s->status, sizeof s->status) ||
            mercs_save_encode(snapshot.document, 1, &original, &original_size) ||
            mercs_campaign_encode_resume(&snapshot, &s->career, original, original_size, resume) ||
            mercs_campaign_decode_resume(archive, original, original_size, resume, sizeof resume,
                                          &next, view, s->status, sizeof s->status)) goto failed;
        if (next.phase == MERCS_CAMPAIGN_DEBRIEF) {
            const char *scene = next.accepted >= 0 && next.accepted < view->mission_count ?
                                view->missions[next.accepted].scene : "";
            if (mercs_debrief_view_prepare(&prepared, s->art, archive,
                    next.document, view, &next.terms, &next.report,
                    scene, s->status, sizeof s->status)) goto failed;
        }
    }
    if (preferences) {
        mercs_combat_copy_options(&next_options, preferences, copy_resolution);
        if (mercs_options_save(&next_options, overlay, error, sizeof error)) {
            snprintf(s->status, sizeof s->status, "%s", error); goto failed;
        }
    }
    candidate.renderer = edition; candidate.texset = PORTCFG_TX_ORIGINAL;
    snprintf(candidate.install, sizeof candidate.install, "%s", overlay);
    prj_close(s->archive); s->archive = archive; archive = NULL;
    *s->cfg = candidate; s->edition = edition;
    s->options = next_options;
    mercs_shell_audio_suspend(s->shell_audio);
    apply_shell_volumes(s);
    mercs_shell_audio_resume(s->shell_audio);
    if (view) {
        install_career(s, &next, view);
        if (prepared.ready) {
            mercs_debrief_view_free(&s->debrief_view); s->debrief_view = prepared;
            memset(&prepared, 0, sizeof prepared); s->debrief_prepared = 1;
        }
        prepare_lines(s);
    }
    save_settings_fields(s, SAVE_GAME_LOOK);
    if (s->status[0]) goto failed;
    if (!s->status[0]) snprintf(s->status, sizeof s->status, "%s selected for the next mission.", portcfg_mercs_label(edition));
    fprintf(stderr, "mercs shell: game-look=%s archive=%s pilot=%s assigned=%s\n",
            portcfg_mercs_name(edition), models, s->career.pilot, s->career.starter_loadout);
    result = 0;
failed:
    if (archive) prj_close(archive);
    mercs_debrief_view_free(&prepared);
    mercs_campaign_free(&next); mercs_campaign_free(&snapshot); free(view); free(original);
    return result;
}

static void change_game_look(shell *s, int edition)
{
    (void)change_game_look_options(s, edition, NULL, 0);
}

/* The child may select a renderer for its next mission. Import its result
 * against the old archive/overlay first, then rehydrate the same career in
 * the requested profile. Never reinterpret a still-pending result packet. */
static void refresh_mission_graphics(shell *s)
{
    portcfg saved;
    mercs_options preferences = s->options;
    char previous[sizeof s->status];
    if (portcfg_load(NULL, &saved)) return;
    s->cfg->texture_filter = saved.texture_filter;
    s->cfg->dos_sky_smooth = saved.dos_sky_smooth;
    s->cfg->hud_original = saved.hud_original;
    s->cfg->gpu = saved.gpu;
    if (saved.renderer == s->edition || !portcfg_mercs_name(saved.renderer) ||
        s->campaign.phase == MERCS_CAMPAIGN_DEPLOYED || s->campaign.phase == MERCS_CAMPAIGN_RETURN) return;
    snprintf(previous, sizeof previous, "%s", s->status); s->status[0] = 0;
    if (!change_game_look_options(s, saved.renderer, &preferences, 0))
        snprintf(s->status, sizeof s->status, "%s", previous);
}

/* Start the already-validated application's simulation path with an argv array.
 * User names and paths are never interpolated into a shell command. The child
 * keeps engine globals isolated and opens its own GPU window and audio device. */
static void flush_shell_input(void)
{
    SDL_FlushEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP);
    SDL_FlushEvent(SDL_EVENT_TEXT_EDITING);
    SDL_FlushEvent(SDL_EVENT_TEXT_INPUT);
    SDL_FlushEvents(SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_WHEEL);
}

static void pump_child_events(shell *s)
{
    SDL_Event event;
    SDL_PumpEvents();
    /* Keep quit/close intent even though the shell is hidden. Wait for the
     * mission to finish normally, then leave the shell hidden and exit it. */
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0) s->running = 0;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST) > 0) {
        if (event.window.windowID == SDL_GetWindowID(s->window) && event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            s->running = 0;
    }
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_RENDER_TARGETS_RESET, SDL_EVENT_RENDER_DEVICE_RESET) > 0) {
        if (event.type == SDL_EVENT_RENDER_DEVICE_RESET) invalidate_shell_textures(s);
    }
    flush_shell_input();
}

static int run_mission_child(shell *s, const char *scene, const char *return_token, const char *instant_token)
{
    const char *edition = portcfg_mercs_name(s->edition);
    const char *binding = instant_token ? instant_token : return_token;
    int result = -1;
    if (!binding || !binding[0]) return -1;
#ifndef _WIN32
    const char *binding_option = instant_token ? "--instant-token" : "--return-token";
    pid_t pid;
    int status = 0, code;
    char *args[] = {(char *)s->exe, s->cfg->game, "--mission", (char *)scene,
                    "--edition", (char *)edition, (char *)binding_option, (char *)binding, NULL};
    code = posix_spawn(&pid, s->exe, NULL, NULL, args, environ);
    if (code) { snprintf(s->status, sizeof s->status, "Could not start the mission: %s", strerror(code)); return -1; }
    SDL_HideWindow(s->window);
    for (;;) {
        pid_t done = waitpid(pid, &status, WNOHANG);
        if (done == pid) { result = WIFEXITED(status) ? WEXITSTATUS(status) : -1; break; }
        if (done < 0 && errno != EINTR) break;
        pump_child_events(s); SDL_Delay(20);
    }
#else
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    wchar_t executable[2048], command[8192], game[2048], mission[64], ed[32], token[33];
    DWORD exit_code;
    /* Quotes are invalid in Windows filenames; every variable argument is
     * quoted and the game directory has no trailing separator. */
    if (!MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, s->exe, -1, executable, 2048) ||
        !MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, s->cfg->game, -1, game, 2048) ||
        !MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, scene, -1, mission, 64) ||
        !MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, edition, -1, ed, 32) ||
        !MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, binding, -1, token, 33)) return -1;
    if (wcschr(executable, L'"') || wcschr(game, L'"') || wcschr(mission, L'"')) return -1;
    if (swprintf(command, 8192, L"\"%ls\" \"%ls\" --mission \"%ls\" --edition \"%ls\" %ls %ls", executable, game, mission, ed,
                 instant_token ? L"--instant-token" : L"--return-token", token) < 0) return -1;
    memset(&startup, 0, sizeof startup); startup.cb = sizeof startup;
    memset(&process, 0, sizeof process);
    if (!CreateProcessW(executable, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
        snprintf(s->status, sizeof s->status, "Could not start the mission (Windows error %lu).", (unsigned long)GetLastError()); return -1;
    }
    SDL_HideWindow(s->window);
    while (WaitForSingleObject(process.hProcess, 20) == WAIT_TIMEOUT) pump_child_events(s);
    if (GetExitCodeProcess(process.hProcess, &exit_code)) result = (int)exit_code;
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
#endif
    pump_child_events(s);
    if (result == 42 || result == 43) s->running = 0;
    if (s->running) {
        SDL_ShowWindow(s->window); SDL_RaiseWindow(s->window);
        SDL_SetWindowRelativeMouseMode(s->window, false); SDL_ShowCursor();
        pump_child_events(s); /* Showing a fullscreen window can itself reset its device. */
    }
    return result;
}

/* Returns0 after a durably imported return,1 if no usable packet exists,
 * and-1 if a valid packet must remain pending because persistence failed. */
static int consume_return(shell *s, const char *overlay, const mercs_exchange_launch *launch)
{
    mercs_exchange_result *returned = calloc(1, sizeof *returned);
    mercs_career_import_result imported;
    mercs_campaign next;
    mercs_shell_state *view = NULL;
    int rc = returned ? mercs_exchange_read(overlay, launch, returned) : MERCS_EXCHANGE_IO;
    memset(&next, 0, sizeof next);
    if (rc == MERCS_EXCHANGE_IO) {
        snprintf(s->status, sizeof s->status,
                 "The mission return could not be read. It remains pending; reopening the game will retry it.");
        free(returned); return -1;
    }
    if (rc != MERCS_EXCHANGE_OK) { free(returned); return 1; }
    if (stage_career(s, &next, &view)) { free(returned); return -1; }
    if (mercs_shell_apply_return(view, next.accepted, launch, returned, &imported, s->status, sizeof s->status)) {
        rc = 1; goto done;
    }
    s->salvage_wreck_count = 0; s->salvage_used = 0; s->salvage_raw_size = 0;
    {   /* Titanium's wreck list, for the debrief's Salvage Yard; read before the
         * return commits so the RETURN generation already carries it */
        int i, renderer = -1;
        size_t size = 0, count = 0;
        for (i = 0; i < 3; i++) if (!strcmp(launch->edition, portcfg_mercs_name(shell_looks[i]))) renderer = shell_looks[i];
        if (renderer >= 0 && portcfg_dynamic_salvage(s->cfg, renderer) &&
            !mercs_exchange_read_salvage(overlay, launch, salvage_raw, sizeof salvage_raw, &size) &&
            !mercs_salvage_decode(salvage_raw, size, s->salvage_wrecks, MERCS_SALVAGE_MAX_WRECKS, &count)) {
            s->salvage_wreck_count = (int)count; s->salvage_raw_size = size;
        }
        fprintf(stderr, "mercs shell: salvage wrecks=%d\n", s->salvage_wreck_count);
    }
    if (mercs_campaign_return(&next, view, s->status, sizeof s->status) ||
        commit_career(s, &next, view, PAGE_COMMAND)) { s->salvage_wreck_count = 0; s->salvage_raw_size = 0; rc = -1; goto done; }
    /* Only a committed career may acknowledge or delete a transient import. */
    if (imported.request_delete || returned->mech_count) {
        size_t i;
        for(i=0;i<returned->mech_count;i++){
        char logical[64], actual[PORTCFG_PATH * 2];
        snprintf(logical, sizeof logical, "MEK/%s%s", returned->mechs[i].name,
                 strchr(returned->mechs[i].name, '.') ? "" : ".MEK");
        if (!dp_resolve_in(overlay, logical, actual, sizeof actual) && remove(actual))
            fprintf(stderr, "mercs shell: retained imported transient %s: %s\n", logical, strerror(errno));
        }
    }
    if (mercs_exchange_acknowledge(overlay, launch))
        fprintf(stderr, "mercs shell: durable return retained; its applied receipt could not be finalized\n");
    fprintf(stderr, "mercs shell: return committed; outcome=%u status=%d mech-action=%d\n",
            s->career.last_msn.outcome, mercs_car_player_status(&s->career.last_car), imported.action);
    rc = 0;
done:
    mercs_campaign_free(&next); free(view); free(returned); return rc;
}

static int restore_unreturned_launch(shell *s)
{
    mercs_campaign next;
    mercs_shell_state *view;
    int i, rc;
    if (stage_career(s, &next, &view)) return -1;
    for (i = 0; i < view->owned_count; i++) mercs_career_set_deployed(&view->owned[i].current, 0);
    next.phase = MERCS_CAMPAIGN_READY;
    next.launch_token[0] = next.launch_edition[0] = 0;
    rc = commit_career(s, &next, view, PAGE_COMMAND);
    mercs_campaign_free(&next); free(view); return rc;
}

static void resume_campaign(shell *s)
{
    if (s->campaign.phase == MERCS_CAMPAIGN_DEPLOYED) {
        char overlay[PORTCFG_PATH];
        mercs_launch_lock *lock = NULL;
        mercs_exchange_launch launch;
        int i, edition = -1, rc;
        for (i = 0; i < 3; i++) if (!strcmp(s->campaign.launch_edition, portcfg_mercs_name(shell_looks[i]))) edition = shell_looks[i];
        if (edition < 0 || portcfg_overlay_dir(s->cfg, portcfg_mercs_overlay(edition), overlay, sizeof overlay, 0)) {
            snprintf(s->status, sizeof s->status, "The pending mission's installation folder could not be opened."); return;
        }
        if (mercs_launch_lock_acquire(overlay, &lock, s->status, sizeof s->status) != MERCS_LOCK_OK) return;
        rc = mercs_exchange_load(overlay, s->campaign.launch_token, &launch);
        if (!rc && strcmp(launch.edition, s->campaign.launch_edition)) rc = MERCS_EXCHANGE_INVALID;
        if (!rc) rc = consume_return(s, overlay, &launch);
        else if (rc == MERCS_EXCHANGE_IO) {
            snprintf(s->status, sizeof s->status,
                     "The pending mission's launch record could not be read. Reopening the game will retry it.");
            rc = -1;
        }
        else rc = 1;
        if (rc > 0 && !restore_unreturned_launch(s))
            snprintf(s->status, sizeof s->status,
                     "The interrupted mission has no complete return. Your previous mech condition has been restored.");
        mercs_launch_lock_release(lock);
        if (rc < 0) return;
    }
    if (s->campaign.phase == MERCS_CAMPAIGN_RETURN) enter_debrief(s);
    else if (s->campaign.phase == MERCS_CAMPAIGN_DEBRIEF && s->page != PAGE_DEBRIEF) set_page(s, PAGE_DEBRIEF);
}

static void launch_mission(shell *s)
{
    mercs_launch_lock *launch_lock = NULL;
    mercs_campaign next;
    mercs_shell_state *view = NULL;
    char overlay[PORTCFG_PATH], scene[17];
    mercs_exchange_launch launch = {0};
    int result, captured;
    memset(&next, 0, sizeof next);
    if (s->campaign.phase != MERCS_CAMPAIGN_READY) { resume_campaign(s); return; }
    if (!s->campaign.active || s->accepted < 0 || s->accepted >= s->career.mission_count) return;
    if (!roster_ready(s)) return;
    if (!s->career.missions[s->accepted].launchable) {
        snprintf(s->status, sizeof s->status, "%s", s->career.missions[s->accepted].unavailable); return;
    }
    if (portcfg_overlay_dir(s->cfg, portcfg_mercs_overlay(s->edition), overlay, sizeof overlay, 1) || dp_set_write_root(overlay)) {
        snprintf(s->status, sizeof s->status, "The game installation folder is not writable: %s", strerror(errno)); return;
    }
    if (mercs_launch_lock_acquire(overlay, &launch_lock, s->status, sizeof s->status) != MERCS_LOCK_OK) return;
    if (stage_career(s, &next, &view)) goto done;
    if (prepare_roster_launch(s,&next,view,overlay,&launch)) goto done;
    snprintf(scene, sizeof scene, "%s", view->missions[next.accepted].scene);
    snprintf(launch.scene, sizeof launch.scene, "%s", scene);
    snprintf(launch.edition, sizeof launch.edition, "%s", portcfg_mercs_name(s->edition));
    if (mercs_exchange_begin(overlay, &launch)) {
        snprintf(s->status, sizeof s->status, "Could not prepare this mission's return files."); goto done;
    }
    next.phase = MERCS_CAMPAIGN_DEPLOYED;
    snprintf(next.launch_token, sizeof next.launch_token, "%s", launch.token);
    snprintf(next.launch_edition, sizeof next.launch_edition, "%s", launch.edition);
    if (commit_career(s, &next, view, PAGE_BRIEFING)) goto done;
    fprintf(stderr, "mercs shell: launch=%s campaign=%s pilot=%s\n", scene, s->campaign.campaign, s->career.pilot);
    fprintf(stderr, "mercs shell: launch-profile=%s overlay=%s music=%s\n",
            portcfg_mercs_name(s->edition), overlay, s->cfg->music);
    mercs_shell_audio_suspend(s->shell_audio);
    if (s->audio) sfx_pause(s->audio, 1);
    result = run_mission_child(s, scene, launch.token, NULL);
    if (s->audio) sfx_pause(s->audio, 0);
    {
        mercs_options refreshed;
        char error[384];
        /* The mission's original Audio menu edits this same profile. Reload
         * it before restarting room loops, keeping their new gain on return. */
        if (!mercs_options_load(&refreshed, overlay, s->cfg->cd, error, sizeof error)) {
            s->options = refreshed;
            apply_shell_volumes(s);
        } else fprintf(stderr, "mercs shell: %s\n", error);
    }
    mercs_shell_audio_resume(s->shell_audio);
    captured = consume_return(s, overlay, &launch);
    if (captured > 0 && !restore_unreturned_launch(s))
        snprintf(s->status, sizeof s->status,
                 "The mission has no complete return (status %d). Your previous mech condition has been kept.", result);
    fprintf(stderr, "mercs shell: mission-return=%s result=%d capture=%d\n", scene, result, captured);
done:
    mercs_campaign_free(&next); free(view); mercs_launch_lock_release(launch_lock);
    if (s->running && s->campaign.phase == MERCS_CAMPAIGN_RETURN) enter_debrief(s);
    if (s->running) refresh_mission_graphics(s);
}

static int start_repair(shell *s, int index)
{
    const mercs_save_state *d = s->campaign.document;
    size_t nw = mercs_save_list_count(d, MERCS_SAVE_OWNED_WEAPONS);
    size_t na = mercs_save_list_count(d, MERCS_SAVE_OWNED_AMMO);
    if (index < 0 || index >= s->career.owned_count || s->repair ||
        mercs_repair_begin(&s->career.owned[index],
            mercs_save_list_record(d, MERCS_SAVE_OWNED_WEAPONS, 0), nw,
            mercs_save_list_record(d, MERCS_SAVE_OWNED_AMMO, 0), na,
            s->career.funds, mercs_save_header(d)[0], &s->repair)) {
        snprintf(s->status, sizeof s->status, "The owned mech could not be opened for repair."); return -1;
    }
    s->repair_index = index; s->repair_finished = 0;
    mercs_repair_view_init(&s->repair_view, s->repair,
                           s->repair_return == PAGE_OFFICE || s->repair_return == PAGE_MECHOPS);
    s->repair_view.year = s->career.year;
    set_page(s, PAGE_REPAIR); return 0;
}

static void open_repair(shell *s)
{
    if (!s->campaign.document || s->campaign.phase != MERCS_CAMPAIGN_READY) return;
    if (!s->career.owned_count) {
        if (!mercs_dialog_open(&s->dialog, "You have no Mechs to repair!#Ok", 0)) (void)run_dialog(s);
        return;
    }
    s->repair_return = s->page;
    start_repair(s, s->career.owned_index >= 0 ? s->career.owned_index : 0);
}

static void close_repair(shell *s, int accept, int destination)
{
    mercs_campaign next;
    mercs_shell_state *view;
    const uint8_t *items;
    size_t count;
    int leave = 0, rc;
    if (!s->running || !s->repair) return;
    if (!s->repair_finished) {
        if (!accept) {
            if (!play_cue(s, MSA_CUE_REPAIR_SELECT)) return;
            mercs_repair_rollback(s->repair);
        }
        rc = mercs_repair_finish(s->repair, accept, 0, &leave);
        if (rc == MERCS_REPAIR_INSUFFICIENT_FUNDS) {
            int undo = game_question(s, "Insufficient Funds!|Undo customizations?", 0);
            if (!play_cue(s, MSA_CUE_REPAIR_SELECT) || !undo) return;
            rc = mercs_repair_finish(s->repair, 1, 1, &leave);
        }
        if (rc < 0 || !leave) return;
        s->repair_finished = 1;
    } else if (!accept) {
        /* A completed repair whose save failed has never affected the live
         * career. An explicit rejected repair can discard that private session. */
        mercs_repair_free(s->repair); s->repair = NULL; s->repair_finished = 0;
        set_page(s, destination); return;
    }
    if (stage_career(s, &next, &view)) return;
    view->owned[s->repair_index].current = *mercs_repair_current(s->repair);
    view->owned[s->repair_index].reference = *mercs_repair_reference(s->repair);
    view->funds = mercs_repair_funds(s->repair);
    items = mercs_repair_items(s->repair, MERCS_REPAIR_OWNED_WEAPONS, &count);
    if (mercs_save_set_list(next.document, MERCS_SAVE_OWNED_WEAPONS, items, count)) goto failed;
    items = mercs_repair_items(s->repair, MERCS_REPAIR_OWNED_AMMO, &count);
    if (mercs_save_set_list(next.document, MERCS_SAVE_OWNED_AMMO, items, count)) goto failed;
    mercs_save_header_edit(next.document)[0] = mercs_repair_dirty(s->repair);
    if (mercs_campaign_sync(&next, view, s->status, sizeof s->status) ||
        commit_career(s, &next, view, destination)) goto failed;
    fprintf(stderr, "mercs shell: repair committed mech=%d funds=%ld\n", s->repair_index, (long)s->career.funds);
    mercs_repair_free(s->repair); s->repair = NULL; s->repair_finished = 0;
    mercs_campaign_free(&next); free(view); return;
failed:
    if (!s->status[0]) snprintf(s->status, sizeof s->status,
        "The repair could not be saved. Accept retries saving. Your previous condition is still saved.");
    mercs_campaign_free(&next); free(view);
}

static void repair_action(shell *s, int action)
{
    int32_t amount = 0;
    int rc = MERCS_REPAIR_OK;
    if (!s->repair || action == MERCS_REPAIR_VIEW_NONE) return;
    if (action == MERCS_REPAIR_VIEW_LOCATION || action == MERCS_REPAIR_VIEW_ROW) {
        play_cue(s, action == MERCS_REPAIR_VIEW_LOCATION ? MSA_CUE_REPAIR_CHANGE : MSA_CUE_REPAIR_ROW);
        return;
    }
    if (action == MERCS_REPAIR_VIEW_ACCEPT) {
        if (!play_cue(s, MSA_CUE_REPAIR_EXIT)) return;
        close_repair(s, s->repair_finished || !mercs_repair_current(s->repair)->bytes[0x84c] ||
                         game_question(s, "Accept repairs?", 0), s->repair_return); return;
    }
    if (s->repair_finished) return;
    if (action == MERCS_REPAIR_VIEW_PREV || action == MERCS_REPAIR_VIEW_NEXT) {
        int index = s->repair_index + (action == MERCS_REPAIR_VIEW_NEXT ? 1 : -1);
        if (s->career.owned_count <= 1) return;
        if (!play_cue(s, MSA_CUE_REPAIR_SELECT)) return;
        if (index < 0) index = s->career.owned_count - 1;
        if (index >= s->career.owned_count) index = 0;
        close_repair(s, !mercs_repair_current(s->repair)->bytes[0x84c] ||
                         game_question(s, "Accept repairs?", 0), PAGE_COMMAND);
        if (!s->repair) start_repair(s, index);
        return;
    }
    if (!play_cue(s, MSA_CUE_REPAIR_SELECT)) return;
    if (action == MERCS_REPAIR_VIEW_ALL) {
        rc = mercs_repair_all(s->repair, 1, &amount);
        /* Original UI has a strict > gate, distinct from accepting staged
         * work, which allows exact available funds. */
        if (!rc && mercs_repair_funds(s->repair) > amount) rc = mercs_repair_all(s->repair, 0, &amount);
        else if (!rc) rc = MERCS_REPAIR_INSUFFICIENT_FUNDS;
    } else if (action == MERCS_REPAIR_VIEW_CLEAR) {
        if (!mercs_repair_current(s->repair)->bytes[0x84c] || !game_question(s, "Clear all Repairs?", 1)) return;
        rc = mercs_repair_rollback(s->repair);
    }
    else if (action == MERCS_REPAIR_VIEW_SELECTED)
        rc = mercs_repair_selected(s->repair, s->repair_view.location,
             (enum mercs_repair_part)s->repair_view.part, s->repair_view.slot, &amount);
    if (rc == MERCS_REPAIR_OK && (action == MERCS_REPAIR_VIEW_ALL ||
        (action == MERCS_REPAIR_VIEW_SELECTED &&
         (amount > 0 || s->repair_view.part == MERCS_REPAIR_CRITICAL))))
        play_cue(s, MSA_CUE_REPAIR_APPLY);
    mercs_repair_view_refresh(&s->repair_view, s->repair);
    if (rc == MERCS_REPAIR_NO_REPLACEMENT) snprintf(s->status, sizeof s->status, "No replacement in inventory");
    else if (rc == MERCS_REPAIR_NOT_DAMAGED) snprintf(s->status, sizeof s->status, "Critical needs no repairs");
    else if (rc == MERCS_REPAIR_INTERNALS_FIRST) snprintf(s->status, sizeof s->status, "Internals must be repaired first!");
    else if (rc == MERCS_REPAIR_INSUFFICIENT_FUNDS) snprintf(s->status, sizeof s->status, "Insufficient funds\nfor complete repair!");
    else if (rc < 0) snprintf(s->status, sizeof s->status, "Select critical to repair first.");
}

static void mechlab_notice(shell *s, int rc)
{
    const char *message = NULL;
    switch (rc) {
    case MERCS_MECHLAB_NO_SPACE: message = "Insufficient criticals for item placement!#Ok"; break;
    case MERCS_MECHLAB_NO_ITEM: message = "Select an available item first.#Ok"; break;
    case MERCS_MECHLAB_LOCATION: message = "This item cannot be assigned to the selected location.#Ok"; break;
    case MERCS_MECHLAB_FIXED: message = "Selected critical can not be removed!#Ok"; break;
    case MERCS_MECHLAB_DUPLICATE: message = "This equipment is already present!#Ok"; break;
    case MERCS_MECHLAB_LIMIT: message = "The selected adjustment exceeds this mech's limits.#Ok"; break;
    case MERCS_MECHLAB_HEATSINK_TYPE: message = "Upgrade or downgrade heat sinks before changing their type.#Ok"; break;
    case MERCS_MECHLAB_OVERWEIGHT: message = "Mech exceeds Defined Maximum Tonnage!#Ok"; break;
    case MERCS_MECHLAB_FUNDS: message = "Insufficient Funds!#Ok"; break;
    case MERCS_MECHLAB_PLACEMENT: message = "Place all remaining equipment criticals first.#Ok"; break;
    default: if (rc < 0) message = "The customization could not be completed.#Ok"; break;
    }
    if (message) game_notice(s, message);
}

static void finish_mechlab_name(shell *s, int cancel)
{
    int rc = 0;
    if (!s->mechlab_editing) return;
    if (!cancel && s->mechlab && s->mechlab_name[0])
        rc = mercs_mechlab_rename(s->mechlab, s->mechlab_name);
    s->mechlab_editing = 0; s->mechlab_view.name_override = NULL;
    s->select_all = 0; SDL_StopTextInput(s->window);
    if (rc) mechlab_notice(s, rc);
}

static void release_mechlab(shell *s)
{
    finish_mechlab_name(s, 1);
    mercs_mechlab_free(s->mechlab); s->mechlab = NULL;
    mercs_mechlab_view_free(&s->mechlab_view);
    s->mechlab_instant = 0; s->mechlab_complete = NULL; s->mechlab_complete_user = NULL;
}

static int start_mechlab(shell *s, int index)
{
    if (s->mechlab || s->repair || index < 0 || index >= s->career.owned_count ||
        mercs_mechlab_begin(s->campaign.document, (size_t)index, &s->mechlab)) {
        snprintf(s->status, sizeof s->status, "The owned mech could not be opened for customization."); return -1;
    }
    mercs_mechlab_view_init(&s->mechlab_view);
    if (mercs_mechlab_view_prepare(&s->mechlab_view, s->mechlab, MERCS_ITEM_MISSILE)) {
        release_mechlab(s); snprintf(s->status, sizeof s->status, "The Mech Lab inventory could not be prepared."); return -1;
    }
    set_page(s, PAGE_MECHLAB); return 0;
}

static void open_mechlab(shell *s)
{
    if (!s->campaign.document || s->campaign.phase != MERCS_CAMPAIGN_READY) return;
    if (!s->career.owned_count) { game_notice(s, "You have no mechs in inventory!#Ok"); return; }
    s->mechlab_return = s->page;
    (void)start_mechlab(s, s->career.owned_index >= 0 ? s->career.owned_index : 0);
}

/* The Instant Action slots own raw designs, independently of the career.
 * complete runs only after weight/placement validation; cancel does not touch
 * a slot. The callback copies these temporary bytes before returning. */
static int open_mechlab_instant(shell *s, const uint8_t *raw, size_t length,
                                const char *name, int return_page,
                                int (*complete)(void *, const uint8_t *, size_t), void *user)
{
    if (s->mechlab || s->repair || !complete ||
        mercs_mechlab_begin_mek(raw, length, name, &s->mechlab)) return -1;
    mercs_mechlab_view_init(&s->mechlab_view);
    s->mechlab_view.technology = mercs_mechlab_current(s->mechlab)->bytes[0x84f] != 0;
    if (mercs_mechlab_view_prepare(&s->mechlab_view, s->mechlab, MERCS_ITEM_MISSILE)) {
        release_mechlab(s); return -1;
    }
    s->mechlab_return = return_page; s->mechlab_instant = 1;
    s->mechlab_complete = complete; s->mechlab_complete_user = user;
    set_page(s, PAGE_MECHLAB); return 0;
}

static int refresh_instant(shell *s)
{
    unsigned retry;uint32_t seed=(uint32_t)SDL_GetTicks();int rc=-1;
    /* This is a new, unaccepted preview. An exceptional original RAND_MAX
     * table overflow can choose another seed without changing an accepted
     * career or a launch capsule. */
    for(retry=0;retry<32;retry++){
        rc=mercs_instant_generate(s->archive,&s->instant,seed+retry,s->status,sizeof s->status);
        if(rc!=MERCS_RMG_BAD_SEED)break;
    }
    return rc;
}

static void open_instant(shell *s)
{
    if(!s->instant.ready)mercs_instant_init(&s->instant);
    if(!refresh_instant(s))set_page(s,PAGE_INSTANT);
}

static int complete_instant_design(void *context,const uint8_t *bytes,size_t size)
{
    shell *s=context;char overlay[PORTCFG_PATH],error[384]={0};
    if(portcfg_overlay_dir(s->cfg,portcfg_mercs_overlay(s->edition),overlay,sizeof overlay,1)||
       mercs_instant_design(&s->instant,bytes,size,overlay,error,sizeof error)) {
        game_notice(s,error[0]?error:"The custom Instant Action Mech could not be saved.");
        return -1;
    }
    return 0;
}

static void launch_instant(shell *s)
{
    mercs_instant_launch *launch=calloc(1,sizeof *launch);mercs_launch_lock *lock=NULL;
    char overlay[PORTCFG_PATH];int result;
    if(!launch){snprintf(s->status,sizeof s->status,"Not enough memory to prepare Instant Action.");return;}
    if(mercs_instant_prepare(&s->instant,portcfg_mercs_name(s->edition),s->options.dif[5],launch,s->status,sizeof s->status))goto done;
    if(portcfg_overlay_dir(s->cfg,portcfg_mercs_overlay(s->edition),overlay,sizeof overlay,1)){
        snprintf(s->status,sizeof s->status,"The Instant Action installation folder could not be opened.");goto done;}
    if(mercs_launch_lock_acquire(overlay,&lock,s->status,sizeof s->status)!=MERCS_LOCK_OK||
       mercs_instant_begin(overlay,launch,s->status,sizeof s->status))goto done;
    fprintf(stderr,"mercs shell: instant-launch=RAN0SCN1 edition=%s token=%s\n",launch->edition,launch->token);
    mercs_shell_audio_suspend(s->shell_audio);if(s->audio)sfx_pause(s->audio,1);
    result=run_mission_child(s,"RAN0SCN1",NULL,launch->token);
    if(s->audio)sfx_pause(s->audio,0);
    {mercs_options refreshed;char error[384];
     if(!mercs_options_load(&refreshed,overlay,s->cfg->cd,error,sizeof error)){s->options=refreshed;apply_shell_volumes(s);}}
    mercs_shell_audio_resume(s->shell_audio);
    /* Instant Action has no career deployment/import/debrief. Keep the
     * original selected slots on the original Instant Action screen. */
    s->instant.restart=1;
    if(result&&s->running)snprintf(s->status,sizeof s->status,"Instant Action ended with status %d. Your career has not changed.",result);
    fprintf(stderr,"mercs shell: instant-return result=%d\n",result);
done:
    mercs_launch_lock_release(lock);free(launch);
    if(s->running)refresh_mission_graphics(s);
}

static void instant_click(shell *s,int x,int y)
{
    int parameters=s->page==PAGE_INSTANT_PARAMS,at=mercs_instant_view_hit(parameters,x,y),row;
    char overlay[PORTCFG_PATH];
    if(parameters){
        if(at==0){if(play_cue(s,MSA_CUE_INSTANT_PARAM_EXIT)&&!refresh_instant(s))set_page(s,PAGE_INSTANT);}
        else if(at>=3&&at<=8){
            if(!play_cue(s,MSA_CUE_INSTANT_PARAM_SELECT))return;
            if(at<6)s->instant.parameters.skill=at-2;else s->instant.parameters.kind=at-5;
        }else if(at==9){int planet=mercs_instant_planet_index(s->instant.parameters.planet);
            if(!play_cue(s,MSA_CUE_INSTANT_PARAM_SELECT))return;
            snprintf(s->instant.parameters.planet,sizeof s->instant.parameters.planet,"%s",mercs_instant_planet((planet+1)%15));
        }
        return;
    }
    row=mercs_instant_view_row(x,y);
    if(row>=0){mercs_instant_choose(&s->instant,row,0);return;}
    if(at<0)return;
    if(at<=3){
        if(!play_cue(s,MSA_CUE_INSTANT_SELECT))return;
        if(at>=2)mercs_instant_choose(&s->instant,s->instant.selected,at==2?-1:1);
        else if(!portcfg_overlay_dir(s->cfg,portcfg_mercs_overlay(s->edition),overlay,sizeof overlay,1))
            mercs_instant_variant(&s->instant,at?1:-1,overlay,s->status,sizeof s->status);
    }else if(at==4){if(play_cue(s,MSA_CUE_INSTANT_EXIT))set_page(s,PAGE_MENU);}
    else if(at==5){if(play_cue(s,MSA_CUE_INSTANT_EXIT))launch_instant(s);}
    else if(at==7){if(play_cue(s,MSA_CUE_INSTANT_PARAMS)){s->instant.restart=1;set_page(s,PAGE_INSTANT_PARAMS);}}
    else if(at==6){
        mercs_instant_slot *slot=&s->instant.slots[s->instant.selected];uint8_t bytes[MEK_MAX_SERIALIZED_BYTES];size_t size;
        if(slot->chassis<0){game_notice(s,slot->chassis==-2?"Random mechs cannot be customized.#Ok":"Select mech type first.#Ok");return;}
        if(mercs_instant_loadout(s->archive,&s->instant,s->instant.selected,bytes,sizeof bytes,&size)){
            snprintf(s->status,sizeof s->status,"The selected Instant Action loadout could not be read.");return;}
        if(play_cue(s,MSA_CUE_INSTANT_PARAMS)&&open_mechlab_instant(s,bytes,size,slot->loadout,PAGE_INSTANT,complete_instant_design,s))
            snprintf(s->status,sizeof s->status,"The selected Instant Action Mech could not be opened in the Mech Lab.");
    }
}

static void close_mechlab(shell *s, int accept, int destination)
{
    mercs_save_state *document = NULL;
    mercs_campaign next;
    mercs_shell_state *view;
    int rc, changed;
    if (!s->running || !s->mechlab) return;
    finish_mechlab_name(s, !accept);
    if (!accept) { release_mechlab(s); set_page(s, destination); return; }
    changed = mercs_mechlab_dirty(s->mechlab);
    if (s->mechlab_instant) {
        uint8_t raw[MERCS_CAREER_DEPLOY_MAX_BYTES]; size_t length = 0;
        rc = mercs_mechlab_finish_mek(s->mechlab, 1, raw, sizeof raw, &length);
        if (rc) { mechlab_notice(s, rc); return; }
        if (s->mechlab_complete && s->mechlab_complete(s->mechlab_complete_user, raw, length)) return;
        release_mechlab(s); set_page(s, destination); return;
    }
    rc = mercs_mechlab_finish(s->mechlab, 1, &document);
    if (rc == MERCS_MECHLAB_FUNDS && game_question(s, "Insufficient Funds!|Undo customizations?", 0)) {
        release_mechlab(s); set_page(s, destination); return;
    }
    if (rc) { if (rc != MERCS_MECHLAB_FUNDS) mechlab_notice(s, rc); return; }
    if (!changed) { mercs_save_free(document); release_mechlab(s); set_page(s, destination); return; }
    if (stage_career(s, &next, &view)) { mercs_save_free(document); return; }
    mercs_save_free(next.document); next.document = document;
    if (mercs_campaign_hydrate(s->archive, &next, view, s->status, sizeof s->status) ||
        commit_career_cue(s, &next, view, destination, MSA_CUE_MECHLAB_APPLY)) {
        if (!s->status[0]) snprintf(s->status, sizeof s->status,
            "The customization could not be saved. Accept retries saving; the previous design is still saved.");
    } else {
        fprintf(stderr, "mercs shell: mech-lab committed mech=%zu cost=%ld funds=%ld\n",
            mercs_mechlab_index(s->mechlab), (long)mercs_mechlab_cost(s->mechlab), (long)s->career.funds);
        release_mechlab(s);
    }
    mercs_campaign_free(&next); free(view);
}

static void mechlab_action(shell *s, int action, int value, int clicks)
{
    int rc;
    if (!s->mechlab || action == MERCS_MECHLAB_VIEW_NONE) return;
    finish_mechlab_name(s, 0);
    if (action == MERCS_MECHLAB_VIEW_EXIT) {
        if (!play_cue(s, MSA_CUE_MECHLAB_EXIT)) return;
        close_mechlab(s, !mercs_mechlab_dirty(s->mechlab) ||
                         game_question(s, "Accept customizations?", 0), s->mechlab_return); return;
    }
    if (action == MERCS_MECHLAB_VIEW_PREV || action == MERCS_MECHLAB_VIEW_NEXT) {
        int index = (int)mercs_mechlab_index(s->mechlab), destination = s->mechlab_return;
        if (s->mechlab_instant || s->career.owned_count <= 1) return;
        if (!play_cue(s, MSA_CUE_MECHLAB_SELECT)) return;
        index = (index + s->career.owned_count + (action == MERCS_MECHLAB_VIEW_NEXT ? 1 : -1)) % s->career.owned_count;
        close_mechlab(s, !mercs_mechlab_dirty(s->mechlab) ||
                         game_question(s, "Accept customizations?", 0), destination);
        if (!s->mechlab) (void)start_mechlab(s, index);
        return;
    }
    if (action == MERCS_MECHLAB_VIEW_RENAME) {
        const uint8_t *m = mercs_mechlab_current(s->mechlab)->bytes;
        snprintf(s->mechlab_name, sizeof s->mechlab_name, "%.18s", (const char *)m + (m[0x100] ? 0x100 : 0));
        s->mechlab_editing = 1; s->select_all = 1;
        s->mechlab_view.name_override = s->mechlab_name; SDL_StartTextInput(s->window); return;
    }
    if (!play_cue(s, action == MERCS_MECHLAB_VIEW_LOCATION ? MSA_CUE_MECHLAB_CHANGE :
                  action == MERCS_MECHLAB_VIEW_CRITICAL || action == MERCS_MECHLAB_VIEW_SELECT ?
                  MSA_CUE_MECHLAB_ROW : MSA_CUE_MECHLAB_SELECT)) return;
    rc = mercs_mechlab_view_apply(&s->mechlab_view, s->mechlab, action, value);
    if (!rc && clicks > 1 && (action == MERCS_MECHLAB_VIEW_SELECT || action == MERCS_MECHLAB_VIEW_CRITICAL))
        rc = mercs_mechlab_view_apply(&s->mechlab_view, s->mechlab,
                action == MERCS_MECHLAB_VIEW_SELECT ? MERCS_MECHLAB_VIEW_ADD : MERCS_MECHLAB_VIEW_REMOVE, value);
    mechlab_notice(s, rc);
}

static int spot(shell *s, const mercs_ui_hotspot *h)
{ return s->mouse_x >= h->x1 && s->mouse_x <= h->x2 && s->mouse_y >= h->y1 && s->mouse_y <= h->y2; }

static int clicked(const mercs_ui_hotspot *h, int n, int x, int y)
{
    int i;
    for (i = 0; i < n; i++) if (x >= h[i].x1 && x <= h[i].x2 && y >= h[i].y1 && y <= h[i].y2) return h[i].index;
    return -1;
}

static void sprite(shell *s, const mercs_ui_sprite *v)
{ mercs_art_clip_flags(s->art, v->name, v->x, v->y, v->flags, 0); }

static void art_button(shell *s, const mercs_ui_sprite *v, const mercs_ui_hotspot *h)
{ (void)h; mercs_art_button(s->art, v->name, v->x, v->y, 0); }

/* Only the large office/drop-ship computer selectors change frames on hover.
 * Other button frame1 images are click states or alternate action captions. */
static void art_hover(shell *s, const mercs_ui_sprite *v, const mercs_ui_hotspot *h)
{ mercs_art_button(s->art, v->name, v->x, v->y, h && spot(s, h)); }

static void room_labels(shell *s, const mercs_ui_hotspot *h, int count, unsigned enabled)
{
    int i;
    if (s->transition >= 0) return;
    /* Original hotspot object0x47894 consults MW2SND+0x4c. These are the
     * shell's SUBTITLES; contract text and mission-radio messages are separate. */
    if (s->options.snd_size && !mercs_options_get(&s->options, MO_SUBTITLES)) return;
    for (i = 0; i < count; i++) if ((enabled & (1u << i)) && h[i].label && spot(s, &h[i])) {
        if (h[i].label[0] == '~') mercs_art_center(s->art, MERCS_UI_FONT_MENU, h[i].tx, h[i].ty, h[i].label + 1, 22);
        else mercs_art_text(s->art, MERCS_UI_FONT_MENU, h[i].tx, h[i].ty, h[i].label, 22);
    }
}

static void draw_funds(shell *s, int y)
{
    char plain[32], number[40];
    int i, n, written = 0;
    snprintf(plain, sizeof plain, "%ld", (long)s->career.funds);
    n = (int)strlen(plain);
    for (i = 0; i < n; i++) {
        if (i && (n-i) % 3 == 0 && plain[i-1] != '-') number[written++] = ',';
        number[written++] = plain[i];
    }
    number[written] = 0;
    mercs_art_text(s->art, 6, 490, y, "Funds:", 17);
    mercs_art_text(s->art, 6, 620 - mercs_art_width(s->art, 6, number), y, number, 17);
}

static void draw_original(shell *s)
{
    int i;
    char input[48];
    int modal = s->dialog.active || s->game_menu.active || s->menu_backdrop;
    if (s->controls.active) {
        mercs_controls_draw(s->art, &s->controls);
        if (s->dialog.active) mercs_dialog_draw(s->art, &s->dialog);
        goto cursor;
    }
    if (s->combat.active) {
        mercs_combat_draw(s->art, &s->combat, &s->options);
        if (s->dialog.active) mercs_dialog_draw(s->art, &s->dialog);
        goto cursor;
    }
    if (s->panel_mode) {
        if (s->panel_mode == 1) mercs_lords_draw(s->art, s->panels, &s->lords);
        else mercs_circuit_draw(s->art, s->panels, s->panel_polls / 2u);
        mercs_art_present(s->art);
        return;
    }
    if (modal && s->modal_background) {
        mercs_art_restore(s->art, s->modal_background);
        goto overlays;
    }
    if (s->page == PAGE_MENU) {
        static const char *const hints[] = {"Command a Mercenary Army (Economy Enabled)",
            "Join a Mercenary Army (Economy Disabled)", "Multiplayer Mercenaries",
            "Join Battle right away", "Game settings and options"};
        mercs_art_background(s->art, ui_mainmenu_background);
        for (i = 0; i < 5; i++) {
            art_button(s, &ui_mainmenu_sprites[i], &ui_mainmenu_hotspots[i]);
            if ((!s->options.snd_size || mercs_options_get(&s->options, MO_SUBTITLES)) &&
                spot(s, &ui_mainmenu_hotspots[i])) mercs_art_text(s->art, 5, 29, 442, hints[i], 17);
        }
    } else if (s->page == PAGE_INSTANT || s->page == PAGE_INSTANT_PARAMS) {
        mercs_instant_view_draw(s->art,&s->instant,s->page==PAGE_INSTANT_PARAMS,s->mouse_x,s->mouse_y);
    } else if (s->page == PAGE_REGISTER) {
        mercs_art_background(s->art, ui_registration_background);
        art_button(s, &ui_registration_sprites[0], &ui_registration_hotspots[0]);
        sprite(s, &ui_registration_sprites[1]);
        for (i = 0; i < 2; i++) {
            snprintf(input, sizeof input, "%s%s", i ? s->unit : s->pilot, s->focus == i ? "_" : "");
            mercs_art_text(s->art, MERCS_UI_FONT_INPUT, 330, i ? 350 : 312, input, s->focus == i ? 17 : 22);
        }
    } else if (s->page == PAGE_HALL) {
        mercs_art_background(s->art, ui_mainhall_background);
        sprite(s, &ui_mainhall_sprites[0]); sprite(s, &ui_mainhall_sprites[1]);
        if (s->previous_page == PAGE_OFFICE && s->transition < 0) sprite(s, &ui_mainhall_sprites[2]);
        room_labels(s, ui_mainhall_hotspots, COUNT(ui_mainhall_hotspots), s->mode ? 7u : 3u);
    } else if (s->page == PAGE_OFFICE) {
        mercs_art_background(s->art, ui_office_background);
        sprite(s, &ui_office_sprites[0]); sprite(s, &ui_office_sprites[1]);
        if (s->previous_page == PAGE_HALL && s->transition < 0) sprite(s, &ui_office_sprites[2]);
        room_labels(s, ui_office_hotspots, COUNT(ui_office_hotspots), 14u);
    } else if (s->page == PAGE_COMPUTER) {
        mercs_art_background(s->art, ui_computer_background);
        sprite(s, &ui_computer_sprites[0]);
        art_button(s, &ui_computer_sprites[1], &ui_computer_hotspots[4]);
        for (i = 0; i < 4; i++) art_hover(s, &ui_computer_sprites[2+i], &ui_computer_hotspots[i]);
        if (s->mode) draw_funds(s, 17);
    } else if (s->page == PAGE_NEWS) {
        mercs_art_background(s->art, ui_news_background);
        art_button(s, &ui_news_sprites[0], &ui_news_hotspots[0]);
        if (s->mode && !s->letter_heard) sprite(s, &ui_news_sprites[1]);
        mercs_text_draw_rows(&s->body, s->art, ui_news_text_x, ui_news_text_y, ui_news_text_h, s->scroll, 17, s->reveal_rows);
    } else if (s->page == PAGE_CONTRACTS) {
        static const int map[] = {3, 2, 0, 1};
        mercs_art_background(s->art, ui_contracts_background);
        for (i = 0; i < COUNT(ui_contracts_sprites); i++) {
            if ((i == 0 && s->selected + 1 >= s->career.mission_count) || (i == 1 && s->selected == 0)) continue;
            art_button(s, &ui_contracts_sprites[i], &ui_contracts_hotspots[map[i]]);
        }
        if (s->selected >= 0 && s->selected < s->career.mission_count) {
            const char *faction = s->career.missions[s->selected].faction;
            static const char *const names[] = {"COMSTAR", "DRACONIS", "FEDCOM", "FREERASS"};
            for (i = 0; i < COUNT(ui_contracts_faction_logos); i++) if (!strcmp(faction, names[i])) sprite(s, &ui_contracts_faction_logos[i]);
        }
        mercs_text_draw_rows(&s->body, s->art, ui_contracts_text_x, ui_contracts_text_y, ui_contracts_text_h, s->scroll, 22, s->reveal_rows);
    } else if (s->page == PAGE_DROPSHIP) {
        mercs_art_background(s->art, ui_dropship_cabin_background);
        for (i = 0; i < COUNT(ui_dropship_cabin_sprites); i++) sprite(s, &ui_dropship_cabin_sprites[i]);
        room_labels(s, ui_dropship_cabin_hotspots, COUNT(ui_dropship_cabin_hotspots), s->mode ? 3u : 2u);
    } else if (s->page == PAGE_COMMAND) {
        mercs_art_background(s->art, ui_dropship_computer_background);
        sprite(s, &ui_dropship_computer_sprites[0]);
        art_button(s, &ui_dropship_computer_sprites[1], &ui_dropship_computer_hotspots[8]);
        if (s->mode) {
            for (i = 0; i < 5; i++) art_hover(s, &ui_dropship_computer_sprites[2+i], &ui_dropship_computer_hotspots[i]);
            sprite(s, &ui_dropship_computer_sprites[7]); draw_funds(s, 15);
        } else for (i = 0; i < 3; i++) art_hover(s, &ui_dropship_computer_sprites[8+i], &ui_dropship_computer_hotspots[5+i]);
    } else if (s->page == PAGE_ROSTER) {
        mercs_art_background(s->art, ui_duty_roster_background);
        if (s->mode) { sprite(s, &ui_duty_roster_sprites[0]); draw_funds(s, 15); }
        art_button(s, &ui_duty_roster_sprites[1], &ui_duty_roster_hotspots[0]);
        art_button(s, &ui_duty_roster_sprites[2], &ui_duty_roster_hotspots[2]);
        art_button(s, &ui_duty_roster_sprites[3], &ui_duty_roster_hotspots[3]);
        mercs_roster_view_draw(&s->roster_view,s->art,s->campaign.document);
    } else if (s->page == PAGE_BRIEFING) {
        static const int map[] = {3, 1, 4, 6};
        const mercs_shell_mission *m = &s->career.missions[s->accepted];
        mercs_art_background(s->art, ui_mission_briefing_background);
        for (i = 0; i < COUNT(ui_mission_briefing_sprites); i++) {
            const mercs_ui_sprite *v = &ui_mission_briefing_sprites[i];
            if (i == 1) mercs_art_button(s->art, v->name, v->x, v->y, s->brief_tab);
            else art_button(s, v, &ui_mission_briefing_hotspots[map[i]]);
        }
        if (s->brief_video && m->briefing_smk[0]) {
            if (mercs_art_clip_flags(s->art, m->briefing_smk, 377, 26, 0x24u, s->portrait_restart) < 0)
                mercs_art_clip_flags(s->art, "nodata", 377, 26, 0x24u, s->portrait_restart);
        }
        s->portrait_restart = 0;
        mercs_text_draw_rows(&s->body, s->art, ui_mission_briefing_text_x, ui_mission_briefing_text_y,
                        ui_mission_briefing_text_h, s->scroll, 17, s->reveal_rows);
    } else if (s->page == PAGE_SAVE) {
        /* DOS save/load resource20 is zero-based DATABASE entry19. Ten slots and
         * saveload/savesave/saveexit SHPs retain their authored coordinates. */
        mercs_art_background(s->art, 19);
        if (can_load_slot(s)) mercs_art_button(s->art, "saveload", 402, 397, 0);
        if (can_save_slot(s)) mercs_art_button(s->art, "savesave", 529, 397, 0);
        mercs_art_button(s->art, "saveexit", 467, 440, 0);
        for (i = 0; i < 10; i++) {
            char description[258];size_t length;
            snprintf(description, sizeof description, "%s%s", s->save_titles[i],
                     s->save_editing && i == s->save_slot ? "_" : "");
            length=strlen(description);
            while(length&&mercs_art_width(s->art,5,description)>240)description[--length]=0;
            mercs_art_text(s->art, 5, 42, 92 + 35 * i, description,
                           i == s->save_slot ? 17 : 22);
        }
    } else if (s->page == PAGE_DEBRIEF && s->salvage_open) {
        mercs_salvage_view_draw(&s->salvage_view, s->art, &s->salvage_yard);
    } else if (s->page == PAGE_DEBRIEF) {
        mdv_salvage_button = salvage_available(s);
        mercs_debrief_view_draw_rows(&s->debrief_view, s->art, s->reveal_rows);
    } else if (s->page == PAGE_REPAIR && s->repair) {
        mercs_repair_view_draw(s->art, &s->repair_view, s->repair);
    } else if (s->page == PAGE_MECHLAB && s->mechlab) {
        mercs_mechlab_view_draw(&s->mechlab_view, s->art, s->mechlab);
    } else if (s->page == PAGE_FINANCE || s->page == PAGE_DROP_FINANCE) {
        mercs_finance_view_draw(&s->finance_view, s->art);
    } else if (s->page == PAGE_INVENTORY || s->page == PAGE_DROP_INVENTORY) {
        mercs_inventory_view_draw(&s->inventory_view, s->art, s->campaign.document);
    } else if (s->page == PAGE_PERSONNEL) {
        mercs_personnel_view_draw(&s->personnel_view, s->art, s->campaign.document);
    } else if (s->page == PAGE_TRADING) {
        mercs_trading_view_draw(&s->trading_view, s->art, s->campaign.document);
    } else if (s->page == PAGE_FACTORY) {
        mercs_factory_view_draw(&s->factory_view, s->art, s->campaign.document, s->mouse_x, s->mouse_y);
    } else if (s->page == PAGE_GROUPS) {
        mercs_groups_view_draw(&s->groups_view, s->art, s->campaign.document, s->mouse_x, s->mouse_y);
    } else if (s->page == PAGE_CREW) {
        mercs_crew_draw(s->art,s->mouse_x,s->mouse_y,!s->options.snd_size || mercs_options_get(&s->options,MO_SUBTITLES));
    } else if (s->page == PAGE_CREW_BIO) {
        mercs_crew_bio_draw(s->art,&s->body,s->scroll,s->reveal_rows,s->mouse_x,s->mouse_y);
    } else if (s->page == PAGE_MECHBAY) {
        mercs_mechbay_draw(s->art, s->room_restart, s->mouse_x, s->mouse_y,
                           !s->options.snd_size || mercs_options_get(&s->options, MO_SUBTITLES));
        s->room_restart = 0;
    } else if (s->page == PAGE_MECHOPS) {
        mercs_mechops_draw(s->art, s->campaign.document, s->room_restart, s->mouse_x, s->mouse_y);
        s->room_restart = 0;
    }
    if (s->transition >= 0) {
        int done = mercs_art_clip_flags(s->art, s->transition_clip, s->transition_x,
                                        s->transition_y, 0x44u, s->transition_restart);
        s->transition_restart = 0;
        if (done) {
            s->completed_transition = s->transition + 1;
            s->transition = -1;
        }
    }
    if (modal && !s->modal_background) s->modal_background = mercs_art_capture(s->art);
overlays:
    if (s->game_menu.active || s->menu_backdrop) {
        mercs_game_menu painted = s->game_menu;
        painted.active = 1;
        mercs_game_menu_draw(s->art, &painted);
    }
    if (s->dialog.active) mercs_dialog_draw(s->art, &s->dialog);
cursor:
    if (s->mouse_x >= 0 && s->mouse_y >= 0)
        mercs_art_shape(s->art, 3, 0, s->mouse_x, s->mouse_y);
    mercs_art_present(s->art);
}

static void draw_settings(shell *s)
{
    static const char *looks[] = {"ORIGINAL", "SMOOTH", "SCANLINES", "CRT", "CURVED CRT"};
    static const char *fits[] = {"4:3", "FILL SCREEN", "STRETCH"};
    int i, fullscreen = (SDL_GetWindowFlags(s->window) & SDL_WINDOW_FULLSCREEN) != 0;
    static const int look_x[] = {175, 275, 465}, look_w[] = {90, 180, 110};
    page_background(s, 26);
    box(s, 8, 70, 624, 362, 0x050a13, 255);
    centered(s, 320, 91, 18, 0xe6eaf1, "VIDEO OPTIONS");
    static const char *salvage[] = {"AUTO (TITANIUM)", "CONTRACT LIST", "SALVAGE YARD"};
    text(s, 65, 137, 12, 0xc0cde0, "DISPLAY");
    button(s, 316, 126, 250, 34, fullscreen ? "FULLSCREEN" : "WINDOWED", 0, 1);
    text(s, 65, 189, 12, 0xc0cde0, "MOVIE FILTER");
    button(s, 316, 178, 250, 34, looks[s->cfg->videos], 0, 1);
    text(s, 65, 241, 12, 0xc0cde0, "MOVIE SIZE");
    button(s, 316, 230, 250, 34, fits[s->cfg->video_fit], 0, 1);
    text(s, 65, 293, 12, 0xc0cde0, "SALVAGE");
    button(s, 316, 282, 250, 34, salvage[s->cfg->salvage >= 0 && s->cfg->salvage < PORTCFG_SALVAGE_COUNT ? s->cfg->salvage : 0], 0, 1);
    text(s, 65, 356, 11, 0xc0cde0, "GAME LOOK");
    for (i = 0; i < 3; i++)
        button(s, look_x[i], 346, look_w[i], 31, portcfg_mercs_label(shell_looks[i]),
               s->edition == shell_looks[i], s->look_available[i]);
    centered(s, 320, 397, 10, 0x9baec7, "Game look applies to your next mission. Alt+Enter changes fullscreen.");
    button(s, 24, 436, 180, 31, "REPLAY INTRO", 0, 1);
    button(s, 214, 436, 186, 31, "SAVE / LOAD (F5)", 0, s->repair == NULL);
    button(s, 410, 436, 206, 31, "BACK", 0, 1);
}

static void draw(shell *s)
{
    if (!s->viewport.renderer) mercs_viewport_init(&s->viewport, s->renderer);
    if (mercs_viewport_begin(&s->viewport)) {
        fprintf(stderr, "mercs shell: canvas failed: %s\n", SDL_GetError());
        s->running = 0; return;
    }
    SDL_SetRenderDrawBlendMode(s->renderer, SDL_BLENDMODE_BLEND);
    if (s->page == PAGE_SETTINGS) {
        SDL_ShowCursor();
        draw_settings(s);
        if (s->status[0]) {
            char clipped[110]; snprintf(clipped, sizeof clipped, "%.105s", s->status);
            centered(s, 320, 418, 9, 0xf0b9a6, clipped);
        }
    } else { SDL_HideCursor(); draw_original(s); }
    if (mercs_viewport_end(&s->viewport)) {
        fprintf(stderr, "mercs shell: presentation failed: %s\n", SDL_GetError());
        s->running = 0; return;
    }
    SDL_RenderPresent(s->renderer);
    /* Door completion can open a full movie. Finish the authored canvas first
     * so movies never inherit a half-composed target or a scaled viewport. */
    if (s->completed_transition && !s->dialog.active && !s->game_menu.active) {
        int destination = s->completed_transition - 1;
        s->completed_transition = 0;
        if (destination == PAGE_DROPSHIP) play_movie(s, "MTO");
        if (s->running) set_page(s, destination);
    }
}

static void settings(shell *s)
{
    char models[PORTCFG_PATH], textures[PORTCFG_PATH], kind[PORTCFG_PATH], sky[PORTCFG_PATH], fog[PORTCFG_PATH];
    int i;
    for (i = 0; i < 3; i++)
        s->look_available[i] = portcfg_edition(s->cfg, shell_looks[i], models, textures, kind, sky, fog) == 0;
    s->settings_return = s->page; set_page(s, PAGE_SETTINGS);
}

static void scroll_text(shell *s, int amount)
{
    if (s->page == PAGE_MECHLAB && s->mechlab) { mercs_mechlab_view_scroll(&s->mechlab_view, s->art, amount); return; }
    if (s->page == PAGE_DEBRIEF) { mercs_debrief_view_scroll(&s->debrief_view, amount * 12); return; }
    if (s->page == PAGE_FINANCE || s->page == PAGE_DROP_FINANCE) {
        mercs_finance_view_scroll(&s->finance_view, amount * 12); return;
    }
    if (s->page == PAGE_FACTORY) { mercs_factory_view_scroll(&s->factory_view, s->mouse_x, amount); return; }
    if (s->page == PAGE_PERSONNEL) { mercs_personnel_view_scroll(&s->personnel_view, amount * 12); return; }
    if (s->page == PAGE_INVENTORY || s->page == PAGE_DROP_INVENTORY) {
        mercs_inventory_view_scroll(&s->inventory_view, s->art, amount); return;
    }
    if (s->page == PAGE_TRADING) {
        mercs_trading_view_scroll(&s->trading_view, s->art, s->mouse_x < 450 ? 1 : 0, amount); return;
    }
    int height = s->page == PAGE_NEWS ? ui_news_text_h :
        s->page == PAGE_CONTRACTS || s->page == PAGE_CREW_BIO ? ui_contracts_text_h : ui_mission_briefing_text_h;
    int maximum = s->body.height > height ? ((s->body.height - height + 11) / 12) * 12 : 0;
    s->scroll += amount * 12;
    if (s->scroll < 0) s->scroll = 0;
    if (s->scroll > maximum) s->scroll = maximum;
}

static void unavailable(shell *s, const char *feature)
{ snprintf(s->status, sizeof s->status, "%s is not implemented yet in this port.", feature); }

static void transition(shell *s, const char *clip, int x, int y, int destination)
{
    s->transition_clip = clip; s->transition_x = x; s->transition_y = y;
    s->transition = destination; s->transition_restart = 1;
}

static void open_briefing(shell *s)
{
    if (s->accepted < 0 || s->accepted >= s->career.mission_count) return;
    s->selected = s->accepted; s->brief_tab = 0; set_page(s, PAGE_BRIEFING);
}

static void click(shell *s, int x, int y, int clicks)
{
    int at;
    (void)clicks;
    if (s->transition >= 0) return;
    if (s->page == PAGE_SETTINGS) {
        if (hit(x,y,316,126,250,34)) toggle_fullscreen(s);
        else if (hit(x,y,316,178,250,34)) { s->cfg->videos = (s->cfg->videos + 1) % PORTCFG_VID_COUNT; save_settings_fields(s, SAVE_MOVIE_LOOK); }
        else if (hit(x,y,316,230,250,34)) { s->cfg->video_fit = (s->cfg->video_fit + 1) % PORTCFG_FIT_COUNT; save_settings_fields(s, SAVE_MOVIE_FIT); }
        else if (hit(x,y,316,282,250,34)) { s->cfg->salvage = (s->cfg->salvage + 1) % PORTCFG_SALVAGE_COUNT; save_settings_fields(s, SAVE_SALVAGE); }
        else if (hit(x,y,175,346,90,31)) change_game_look(s, PORTCFG_ED_DOS);
        else if (hit(x,y,275,346,180,31)) change_game_look(s, PORTCFG_ED_D3D);
        else if (hit(x,y,465,346,110,31)) change_game_look(s, PORTCFG_ED_3DFX);
        else if (hit(x,y,24,436,180,31)) play_movie(s, "MINTRO");
        else if (hit(x,y,214,436,186,31)) open_save_load(s);
        else if (hit(x,y,410,436,206,31)) set_page(s, s->settings_return);
    } else if (s->page == PAGE_MENU) {
        at = clicked(ui_mainmenu_hotspots, 5, x, y);
        if (at == 0 || at == 1) {
            if (!play_cue(s, at == 0 ? MSA_CUE_MENU_COMMANDER : MSA_CUE_MENU_MERCENARY)) return;
            s->mode = at == 0; s->focus = 0; s->pilot[0] = 0;
            snprintf(s->unit, sizeof s->unit, "%s", s->mode ? "" : "HANSON'S ROUGHRIDERS");
            mercs_campaign_free(&s->campaign);
            memset(&s->career, 0, sizeof s->career); s->accepted = -1; s->letter_heard = 0; s->regular_unassigned = 0;
            /* both careers land first: MW2SHELL.EXE 0x6e038 (Commander) and 0x6e0ec (Mercenary) each play MLAND2 while
             * the transitions option is on ([0xb778c] == 1), then start their career (0x69074(0) / (1)). DOSBox:
             * MERCENARY plays the DropShip landing before the StarPort (the port had played it for the Commander only) */
            play_movie(s, "MLAND2");
            set_page(s, PAGE_HALL);
        } else if (at == 4) { if (play_cue(s, MSA_CUE_MENU_OPTIONS)) open_game_menu(s); }
        else if (at == 2) { if (play_cue(s, MSA_CUE_MENU_MERCNET)) unavailable(s, "MercNet multiplayer"); }
        else if (at == 3) { if (play_cue(s, MSA_CUE_MENU_INSTANT)) open_instant(s); }
    } else if (s->page == PAGE_INSTANT || s->page == PAGE_INSTANT_PARAMS) {
        instant_click(s,x,y);
    } else if (s->page == PAGE_REGISTER) {
        if (hit(x,y,330,306,230,28)) s->focus = 0;
        else if (hit(x,y,330,344,230,28)) s->focus = 1;
        else if (clicked(ui_registration_hotspots, 2, x, y) >= 0) {
            if (!s->focus && s->pilot[0]) s->focus = 1; else register_pilot(s);
        }
    } else if (s->page == PAGE_HALL) {
        at = clicked(ui_mainhall_hotspots, COUNT(ui_mainhall_hotspots), x, y);
        if (at == 1) {
            if (play_cue(s, MSA_CUE_HALL_TERMINAL)) set_page(s, s->career.registered ? PAGE_NEWS : PAGE_REGISTER);
        }
        else if (at == 2 && s->mode) {
            if (!s->career.registered) play_cue(s, MSA_CUE_HALL_UNREGISTERED);
            else { play_cue(s, MSA_CUE_HALL_OFFICE); transition(s, "sdoora", 95, 259, PAGE_OFFICE); }
        } else if (at == 0) {
            const uint8_t *header = mercs_save_header(s->campaign.document);
            if (!s->career.registered) play_cue(s, MSA_CUE_HALL_UNREGISTERED);
            else if (s->mode && (!header || header[0x239d])) play_cue(s, MSA_CUE_HALL_NEED_CONTRACT);
            else if (s->mode && s->career.owned_count == 0) play_cue(s, MSA_CUE_HALL_NO_MECH);
            else if (!accept_training_boarding(s)) {
                play_cue(s, MSA_CUE_HALL_BOARD); transition(s, "sdoorb", 438, 286, PAGE_DROPSHIP);
            }
        }
    } else if (s->page == PAGE_NEWS) {
        if (clicked(ui_news_hotspots, COUNT(ui_news_hotspots), x, y) == 0) {
            if (play_cue(s, MSA_CUE_NEWS_EXIT)) { s->letter_heard = 1; set_page(s, PAGE_HALL); }
        }
    } else if (s->page == PAGE_OFFICE) {
        at = clicked(ui_office_hotspots, COUNT(ui_office_hotspots), x, y);
        if (at == 3) { if (play_cue(s, MSA_CUE_OFFICE_COMPUTER)) set_page(s, PAGE_COMPUTER); }
        else if (at == 2) { play_cue(s, MSA_CUE_OFFICE_STARPORT); transition(s, "offdoora", 103, 194, PAGE_HALL); }
        else if (at == 1) { if (play_cue(s, MSA_CUE_OFFICE_MECHBAY)) set_page(s, PAGE_MECHBAY); }
    } else if (s->page == PAGE_COMPUTER) {
        at = clicked(ui_computer_hotspots, COUNT(ui_computer_hotspots), x, y);
        if (at >= 0 && !play_cue(s, at == 4 ? MSA_CUE_COMPUTER_EXIT : MSA_CUE_COMPUTER_SELECT)) return;
        if (at == 0) { s->selected = s->accepted >= 0 ? s->accepted : 0; set_page(s, PAGE_CONTRACTS); }
        else if (at == 4) set_page(s, PAGE_OFFICE);
        else if (at == 1) { s->personnel_return = PAGE_COMPUTER; set_page(s, PAGE_PERSONNEL); }
        else if (at == 2) set_page(s, PAGE_INVENTORY);
        else if (at == 3) set_page(s, PAGE_FINANCE);
    } else if (s->page == PAGE_CONTRACTS) {
        const uint8_t *header = mercs_save_header(s->campaign.document);
        at = clicked(ui_contracts_hotspots, COUNT(ui_contracts_hotspots), x, y);
        if (at >= 0 && at != 1 && (!header || !header[0x239d])) return;
        if (at >= 0 && !play_cue(s, at == 1 ? MSA_CUE_CONTRACT_EXIT :
                          at == 0 ? MSA_CUE_CONTRACT_ACCEPT : MSA_CUE_CONTRACT_SELECT)) return;
        if (at == 1) set_page(s, PAGE_COMPUTER);
        else if (at == 0 && s->selected >= 0 && s->selected < s->career.mission_count) {
            if (!s->career.missions[s->selected].launchable) snprintf(s->status, sizeof s->status, "%s", s->career.missions[s->selected].unavailable);
            else accept_contract(s);
        } else if (at == 2 && s->selected > 0) { s->selected--; prepare_lines(s); }
        else if (at == 3 && s->selected + 1 < s->career.mission_count) { s->selected++; prepare_lines(s); }
    } else if (s->page == PAGE_DROPSHIP) {
        at = clicked(ui_dropship_cabin_hotspots, COUNT(ui_dropship_cabin_hotspots), x, y);
        if (at == 1) { if (play_cue(s, MSA_CUE_CABIN_COMMAND)) set_page(s, PAGE_COMMAND); }
        else if (at == 0 && s->mode) { play_movie(s, "MLAND"); set_page(s, PAGE_HALL); }
    } else if (s->page == PAGE_COMMAND) {
        if (hit(x,y,512,426,107,46)) {
            if (!play_cue(s, MSA_CUE_COMMAND_EXIT)) return;
        } else if (hit(x,y,13,7,226,463) && !play_cue(s, MSA_CUE_COMMAND_SELECT)) return;
        if (hit(x,y,512,426,107,46)) set_page(s, PAGE_DROPSHIP);
        else if ((s->mode && hit(x,y,13,379,226,91)) || (!s->mode && hit(x,y,13,287,226,92))) open_briefing(s);
        else if (hit(x,y,13,192,226,95)) set_page(s, PAGE_ROSTER);
        else if (s->mode && hit(x,y,13,287,226,92)) open_repair(s);
        else if (s->mode && hit(x,y,13,7,226,91)) set_page(s, PAGE_DROP_FINANCE);
        else if (s->mode && hit(x,y,13,98,226,94)) set_page(s, PAGE_DROP_INVENTORY);
        else if (hit(x,y,13,7,226,372)) open_mechlab(s);
    } else if (s->page == PAGE_ROSTER) {
        size_t selected;
        int row=mercs_roster_view_hit(s->campaign.document,s->art,x,y,&selected);
        if(row){
            if(!play_cue(s,MSA_CUE_ROSTER_ROW))return;
            if(row==1){s->roster_view.selected=selected;s->roster_view.restart=1;}
            else roster_change(s,0,1);
            return;
        }
        at = clicked(ui_duty_roster_hotspots, COUNT(ui_duty_roster_hotspots), x, y);
        if (at == 0) {
            if (roster_ready(s) && play_cue(s, MSA_CUE_ROSTER_EXIT)) set_page(s, PAGE_COMMAND);
        }
        else if (at == 2 || at == 3) {
            if (!play_cue(s, MSA_CUE_ROSTER_SELECT)) return;
            roster_change(s, at == 3 ? 1 : -1, 0);
        }
    } else if (s->page == PAGE_BRIEFING) {
        at = clicked(ui_mission_briefing_hotspots, COUNT(ui_mission_briefing_hotspots), x, y);
        if (at == 6 && !play_cue(s, MSA_CUE_BRIEFING_EXIT)) return;
        if (at == 4 && !play_cue(s, MSA_CUE_BRIEFING_LAUNCH)) return;
        if (at == 6) set_page(s, PAGE_COMMAND);
        else if (at == 4) { s->selected = s->accepted; launch_mission(s); }
        else if (at == 3) { s->brief_video = 1; s->portrait_restart = 1; }
        else if (at == 1) { s->brief_tab = !s->brief_tab; prepare_lines(s); }
    } else if (s->page == PAGE_SAVE) {
        static const int top[] = {83,117,152,187,222,257,292,327,362,398};
        static const int bottom[] = {116,151,186,221,256,291,326,361,397,432};
        int i;
        for (i = 0; i < 10; i++) if (x >= 32 && x <= 297 && y >= top[i] && y <= bottom[i]) {
            if (!play_cue(s, MSA_CUE_SAVE_SELECT)) return;
            finish_save_description(s, 0);
            if (s->save_slot == i) edit_save_description(s);
            else s->save_slot = i;
            return;
        }
        finish_save_description(s, 0);
        if (hit(x,y,392,387,116,43)) {
            if (!can_load_slot(s)) return;
            if (!slot_has_save(s,s->save_slot) ||
                !career_dirty(s) ||
                game_question(s, "Lose current game?", 1)) load_slot(s, s->save_slot, 0);
        }
        else if (hit(x,y,508,387,111,43)) {
            if (!can_save_slot(s)) return;
            if (!slot_has_save(s,s->save_slot) ||
                game_question(s, "Replace Saved Game?", 1)) save_slot(s);
        }
        else if (hit(x,y,462,435,95,40) && play_cue(s, MSA_CUE_SAVE_EXIT)) {
            finish_save_load(s);
        }
    } else if (s->page == PAGE_REPAIR && s->repair) {
        repair_action(s, mercs_repair_view_click(s->art, &s->repair_view, s->repair, x, y));
    } else if (s->page == PAGE_MECHLAB && s->mechlab) {
        int value = 0;
        at = mercs_mechlab_view_hit(&s->mechlab_view, s->art, s->mechlab, x, y, &value);
        mechlab_action(s, at, value, clicks);
    } else if (s->page == PAGE_FINANCE || s->page == PAGE_DROP_FINANCE) {
        if (mercs_finance_view_exit(x,y) && play_cue(s, MSA_CUE_LEDGER_EXIT))
            set_page(s, s->page == PAGE_DROP_FINANCE ? PAGE_COMMAND : PAGE_COMPUTER);
    } else if (s->page == PAGE_INVENTORY || s->page == PAGE_DROP_INVENTORY) {
        int category; size_t row;
        at = mercs_inventory_view_hit(&s->inventory_view, s->art, x, y, &category, &row);
        if (at == MERCS_INVENTORY_EXIT && play_cue(s, MSA_CUE_INVENTORY_EXIT))
            set_page(s, s->page == PAGE_DROP_INVENTORY ? PAGE_COMMAND : PAGE_COMPUTER);
        else if (at == MERCS_INVENTORY_CATEGORY && play_cue(s, MSA_CUE_INVENTORY_CATEGORY)) {
            if (mercs_inventory_view_prepare(&s->inventory_view, s->campaign.document,
                    s->page == PAGE_DROP_INVENTORY, category))
                snprintf(s->status, sizeof s->status, "The inventory category could not be prepared.");
        } else if (at == MERCS_INVENTORY_MECH && play_cue(s, MSA_CUE_INVENTORY_MECH))
            mercs_inventory_view_select(&s->inventory_view, row);
    } else if (s->page == PAGE_PERSONNEL) {
        int list; size_t index;
        at = mercs_personnel_view_hit(&s->personnel_view, s->art, s->campaign.document,
                                       x, y, &list, &index);
        if (at == MERCS_PERSONNEL_VIEW_EXIT && play_cue(s, MSA_CUE_PERSONNEL_EXIT))
            set_page(s, s->personnel_return == PAGE_MECHOPS ? PAGE_MECHOPS : PAGE_COMPUTER);
        else if (at == MERCS_PERSONNEL_VIEW_SELECT && play_cue(s, MSA_CUE_PERSONNEL_SELECT)) {
            if (mercs_personnel_view_select(&s->personnel_view, s->art, s->campaign.document, list, index))
                snprintf(s->status, sizeof s->status, "The selected pilot biography could not be prepared.");
            else if (clicks > 1) economy_transaction(s, list & 1);
        } else if (at == MERCS_PERSONNEL_VIEW_HIRE || at == MERCS_PERSONNEL_VIEW_DISMISS) {
            if (play_cue(s, MSA_CUE_PERSONNEL_SELECT))
                economy_transaction(s, at == MERCS_PERSONNEL_VIEW_HIRE);
        }
    } else if (s->page == PAGE_TRADING) {
        int value; size_t group;
        at = mercs_trading_view_hit(&s->trading_view, s->art, x, y, &value, &group);
        if (at == MERCS_TRADING_EXIT && play_cue(s, MSA_CUE_TRADING_EXIT)) set_page(s, PAGE_MECHOPS);
        else if (at == MERCS_TRADING_CATEGORY && play_cue(s, MSA_CUE_TRADING_SELECT)) {
            if (mercs_trading_view_prepare(&s->trading_view, s->campaign.document, value))
                snprintf(s->status, sizeof s->status, "The trading category could not be prepared.");
        } else if (at == MERCS_TRADING_SELECT && play_cue(s, MSA_CUE_TRADING_SELECT)) {
            mercs_trading_view_select(&s->trading_view, value, group);
            if (clicks > 1) economy_transaction(s, value);
        } else if ((at == MERCS_TRADING_BUY || at == MERCS_TRADING_SELL) && play_cue(s, MSA_CUE_TRADING_SELECT))
            economy_transaction(s, at == MERCS_TRADING_BUY);
        else if (at == MERCS_TRADING_REPAIR && play_cue(s, MSA_CUE_TRADING_SELECT)) trading_repair(s);
    } else if (s->page == PAGE_FACTORY) {
        int side; size_t index;
        at = mercs_factory_view_hit(&s->factory_view, s->art, x, y, &side, &index);
        if (at == MERCS_FACTORY_EXIT && play_cue(s, MSA_CUE_FACTORY_EXIT)) set_page(s, PAGE_MECHOPS);
        else if (at == MERCS_FACTORY_SELECT && play_cue(s, MSA_CUE_FACTORY_MECH)) {
            mercs_factory_view_select(&s->factory_view, side, index);
        } else if (at && play_cue(s, MSA_CUE_FACTORY_SELECT)) {
            if (at == MERCS_FACTORY_BUY || at == MERCS_FACTORY_SELL) economy_transaction(s, at == MERCS_FACTORY_BUY);
            else if (at == MERCS_FACTORY_DETAILS) { s->factory_view.details = !s->factory_view.details; s->factory_view.restart = 1; }
            else if (at == MERCS_FACTORY_NEXT || at == MERCS_FACTORY_PREVIOUS)
                mercs_factory_view_step(&s->factory_view, at == MERCS_FACTORY_NEXT ? 1 : -1);
        }
    } else if (s->page == PAGE_GROUPS) {
        int slot = -1;
        at = mercs_groups_view_hit(&s->groups_view, s->art, s->campaign.document, x, y, &slot);
        if (at == MERCS_GROUPS_EXIT && play_cue(s, MSA_CUE_GROUPS_EXIT)) set_page(s, PAGE_MECHOPS);
        else if (at == MERCS_GROUPS_DEFAULT && play_cue(s, MSA_CUE_GROUPS_DEFAULT)) change_weapon_group(s, -1);
        else if (at == MERCS_GROUPS_WEAPON) change_weapon_group(s, slot);
        else if ((at == MERCS_GROUPS_PREVIOUS || at == MERCS_GROUPS_NEXT) &&
                 mercs_save_list_count(s->campaign.document, MERCS_SAVE_OWNED_MECHS) > 1 &&
                 play_cue(s, MSA_CUE_GROUPS_SELECT))
            mercs_groups_view_step(&s->groups_view, s->campaign.document, at == MERCS_GROUPS_NEXT ? 1 : -1);
    } else if (s->page == PAGE_MECHBAY) {
        at = mercs_mechbay_hit(x,y);
        if (at == MERCS_MECHBAY_OPS && play_cue(s, MSA_CUE_MECHBAY_OPS)) set_page(s, PAGE_MECHOPS);
        else if (at == MERCS_MECHBAY_OFFICE && play_cue(s, MSA_CUE_MECHBAY_OFFICE)) set_page(s, PAGE_OFFICE);
        else if (at == MERCS_MECHBAY_CREW) set_page(s, PAGE_CREW);
    } else if (s->page == PAGE_CREW) {
        at=mercs_crew_hit(x,y);
        if(at==16)set_page(s,PAGE_MECHBAY);
        else if(at>=0&&play_cue(s,MSA_CUE_CREW_SELECT)){s->crew_selected=at;set_page(s,PAGE_CREW_BIO);}
    } else if (s->page == PAGE_CREW_BIO) {
        if(mercs_crew_bio_exit(x,y)&&play_cue(s,MSA_CUE_CREW_BIO_EXIT))set_page(s,PAGE_CREW);
    } else if (s->page == PAGE_MECHOPS) {
        at = mercs_mechops_hit(x,y);
        if (at && !play_cue(s, at == MERCS_MECHOPS_EXIT ? MSA_CUE_MECHOPS_EXIT : MSA_CUE_MECHOPS_SELECT)) return;
        if (at == MERCS_MECHOPS_EXIT) set_page(s, PAGE_MECHBAY);
        else if (at == MERCS_MECHOPS_ARMS) set_page(s, PAGE_TRADING);
        else if (at == MERCS_MECHOPS_FACTORY) set_page(s, PAGE_FACTORY);
        else if (at == MERCS_MECHOPS_REPAIR) open_repair(s);
        else if (at == MERCS_MECHOPS_CUSTOMIZE) open_mechlab(s);
        else if (at == MERCS_MECHOPS_GROUPS) {
            if (mercs_save_list_count(s->campaign.document, MERCS_SAVE_OWNED_MECHS)) set_page(s, PAGE_GROUPS);
            else game_notice(s, "You have no mechs in inventory!#Ok");
        }
    } else if (s->page == PAGE_DEBRIEF && s->salvage_open) {
        salvage_click(s, x, y);
    } else if (s->page == PAGE_DEBRIEF) {
        mercs_debrief_view selected = s->debrief_view;
        mdv_salvage_button = salvage_available(s);
        at = mercs_debrief_view_click(&selected, x, y);
        if (at == MERCS_DEBRIEF_VIEW_SALVAGE) {
            if (play_cue(s, MSA_CUE_DEBRIEF_SELECT)) open_salvage(s);
            return;
        }
        if (at != MERCS_DEBRIEF_VIEW_NONE && !play_cue(s, MSA_CUE_DEBRIEF_SELECT)) return;
        if (at == MERCS_DEBRIEF_VIEW_TOGGLE) {
            s->debrief_view.content_mode = selected.content_mode;
            reset_text_reveal(s);
        }
        if (at == MERCS_DEBRIEF_VIEW_NEXT || at == MERCS_DEBRIEF_VIEW_CONTRACTS ||
            at == MERCS_DEBRIEF_VIEW_REPLAY) continue_debrief(s);
    }
}

static void key(shell *s, SDL_Keycode k, SDL_Keymod mods)
{
    if (k == SDLK_RETURN && (mods & SDL_KMOD_ALT)) { toggle_fullscreen(s); return; }
    if (k == SDLK_F10) { if (s->page == PAGE_SETTINGS) set_page(s, s->settings_return); else settings(s); return; }
    if (k == SDLK_F5) {
        if (s->page == PAGE_SAVE) finish_save_load(s);
        else open_save_load(s);
        return;
    }
    /* Mercenary -> Save/Load is the original Windows menu mnemonic. F5 is a
     * separate native convenience because this SDL shell has no menu bar. */
    if (k == SDLK_M && (mods & SDL_KMOD_ALT)) { s->menu_mnemonic = 1; return; }
    if (s->menu_mnemonic) {
        s->menu_mnemonic = 0;
        if (k == SDLK_S) { open_save_load(s); return; }
    }
    if (s->transition >= 0) return;
    if (s->page == PAGE_MECHLAB && s->mechlab_editing) {
        size_t n = strlen(s->mechlab_name);
        if (k == SDLK_ESCAPE) finish_mechlab_name(s, 1);
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) finish_mechlab_name(s, 0);
        else if (k == SDLK_A && (mods & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) s->select_all = 1;
        else if (k == SDLK_BACKSPACE) {
            if (s->select_all) s->mechlab_name[0] = 0; else if (n) s->mechlab_name[n-1] = 0;
            s->select_all = 0;
        }
        return;
    }
    if (s->page == PAGE_SAVE && s->save_editing) {
        char *input = s->save_titles[s->save_slot];
        size_t n = strlen(input);
        if (k == SDLK_ESCAPE) finish_save_description(s, 1);
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) finish_save_description(s, 0);
        else if (k == SDLK_A && (mods & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) s->select_all = 1;
        else if (k == SDLK_BACKSPACE) {
            if (s->select_all) input[0] = 0; else if (n) input[n-1] = 0;
            s->select_all = 0;
        }
        return;
    }
    if (k == SDLK_ESCAPE) {
        if (s->page == PAGE_SETTINGS) set_page(s, s->settings_return);
        else open_game_menu(s);
        return;
    }
    if (s->page == PAGE_REGISTER) {
        char *input = s->focus ? s->unit : s->pilot;
        size_t n = strlen(input);
        /* DOSBox: Return in the call sign moves to the unit name; Return in the unit name and Tab do nothing - the
         * ACCEPT button registers (the port's Return-to-accept and Tab toggle were additions) */
        if (k == SDLK_A && (mods & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) s->select_all = 1;
        else if (k == SDLK_BACKSPACE) { if (s->select_all) input[0] = 0; else if (n) input[n-1] = 0; s->select_all = 0; }
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { if (!s->focus && s->pilot[0]) s->focus = 1; }
    } else if (s->page == PAGE_CONTRACTS || s->page == PAGE_NEWS || s->page == PAGE_BRIEFING || s->page == PAGE_DEBRIEF ||
               s->page == PAGE_FINANCE || s->page == PAGE_DROP_FINANCE || s->page == PAGE_PERSONNEL ||
               s->page == PAGE_INVENTORY || s->page == PAGE_DROP_INVENTORY || s->page == PAGE_TRADING || s->page == PAGE_FACTORY ||
               s->page == PAGE_MECHLAB || s->page == PAGE_CREW_BIO) {
        if (k == SDLK_UP) scroll_text(s, -1);
        else if (k == SDLK_DOWN) scroll_text(s, 1);
        else if (k == SDLK_PAGEUP) scroll_text(s, -12);
        else if (k == SDLK_PAGEDOWN) scroll_text(s, 12);
        else if (s->page == PAGE_CONTRACTS && (k == SDLK_LEFT || k == SDLK_RIGHT)) {
            const uint8_t *header = mercs_save_header(s->campaign.document);
            int next = s->selected + (k == SDLK_RIGHT ? 1 : -1);
            if (header && header[0x239d] && next >= 0 && next < s->career.mission_count &&
                play_cue(s, MSA_CUE_CONTRACT_SELECT)) {
                s->selected = next; prepare_lines(s);
            }
        }
    }
}

static void text_input(shell *s, const char *value)
{
    int saving = s->page == PAGE_SAVE && s->save_editing;
    int naming = s->page == PAGE_MECHLAB && s->mechlab_editing;
    char *input;
    const unsigned char *p = (const unsigned char *)value;
    size_t n, maximum = naming ? 18u : saving ? 28u : 21u;
    int font = saving || naming ? 5 : MERCS_UI_FONT_INPUT;
    int width = naming ? 197 : saving ? 240 : 230;
    if (s->page != PAGE_REGISTER && !saving && !naming) return;
    input = naming ? s->mechlab_name : saving ? s->save_titles[s->save_slot] : s->focus ? s->unit : s->pilot;
    if (s->select_all) { input[0] = 0; s->select_all = 0; }
    n = strlen(input);
    for (; *p && n < maximum; p++) if (*p >= 32 && *p < 127) {
        input[n] = (char)*p; input[n+1] = 0;
        if (mercs_art_width(s->art, font, input) + mercs_art_width(s->art, font, "_") < width) n++;
        else input[n] = 0;
    }
    input[n] = 0; s->status[0] = 0;
}

static void pointer_event(shell *s, float raw_x, float raw_y)
{
    if (mercs_viewport_window_point(&s->viewport, s->window, raw_x, raw_y,
                                   &s->mouse_x, &s->mouse_y) != 1)
        s->mouse_x = s->mouse_y = -1;
}

static int wheel_rows(shell *s, float delta)
{
    double total = (double)s->wheel_remainder - (double)delta * 3.0;
    int rows;
    /* SDL3 wheel deltas can be fractional. Retain small trackpad movements
     * instead of truncating every event before it reaches an original row. */
    if (!isfinite(total)) { s->wheel_remainder = 0; return 0; }
    if (total >= INT_MAX) { s->wheel_remainder = 0; return INT_MAX; }
    if (total <= INT_MIN) { s->wheel_remainder = 0; return INT_MIN; }
    rows = (int)total;
    s->wheel_remainder = (float)(total - rows);
    return rows;
}

static int system_event(shell *s, const SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) { s->running = 0; return 1; }
    if (event->type == SDL_EVENT_RENDER_DEVICE_RESET || event->type == SDL_EVENT_RENDER_TARGETS_RESET) {
        invalidate_shell_textures(s); return 1;
    }
    if (event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST) {
        if (event->window.windowID && event->window.windowID != SDL_GetWindowID(s->window)) return 1;
        if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) s->running = 0;
        else if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            mercs_dialog_cancel_press(&s->dialog);
            s->combat.drag = -1;
            s->mouse_x = s->mouse_y = -1;
        } else if (event->type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
            s->mouse_x = s->mouse_y = -1;
        } else if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
                   event->type == SDL_EVENT_WINDOW_RESIZED ||
                   event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
                   event->type == SDL_EVENT_WINDOW_MOVED ||
                   event->type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
                   event->type == SDL_EVENT_WINDOW_MOUSE_ENTER) {
            float x, y;
            SDL_GetMouseState(&x, &y); pointer_event(s, x, y);
        }
        return 1;
    }
    return 0;
}

static void dispatch_event(shell *s, const SDL_Event *event)
{
    int left = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP;
    if (system_event(s, event)) return;
    if (event->type == SDL_EVENT_MOUSE_MOTION) pointer_event(s, event->motion.x, event->motion.y);
    else if (left) pointer_event(s, event->button.x, event->button.y);
    if (s->controls.active && !s->dialog.active) {
        mercs_controls_view *v = &s->controls;
        if (event->type == SDL_EVENT_KEY_DOWN) {
            SDL_Keycode k = event->key.key;
            if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && (event->key.mod & SDL_KMOD_ALT))
                toggle_fullscreen(s);
            else if (v->editing_name) {
                size_t n = strlen(v->config.name);
                if (k == SDLK_ESCAPE) { memcpy(v->config.name, v->old_name, 64); v->editing_name = 0; }
                else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) v->editing_name = 0;
                else if (k == SDLK_BACKSPACE && n) v->config.name[n-1] = 0;
                if (!v->editing_name) SDL_StopTextInput(s->window);
            } else if (k == SDLK_ESCAPE) v->active = 0;
            else if (v->editor && k == SDLK_PAGEUP) mercs_controls_scroll(v, -18);
            else if (v->editor && k == SDLK_PAGEDOWN) mercs_controls_scroll(v, 18);
        } else if (event->type == SDL_EVENT_TEXT_INPUT && v->editing_name) {
            const unsigned char *p = (const unsigned char *)event->text.text;
            size_t n = strlen(v->config.name);
            for (; *p && n < sizeof v->config.name - 1; p++) if (*p >= 32 && *p < 127) {
                v->config.name[n++] = (char)*p; v->config.name[n] = 0;
                if (mercs_art_width(s->art, 10, v->config.name) >= 150) v->config.name[--n] = 0;
            }
        } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                   (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT)) {
            v->editing_name = 0; SDL_StopTextInput(s->window);
            controls_action(s, mercs_controls_click(s->art, v, s->mouse_x, s->mouse_y,
                                                    event->button.button == SDL_BUTTON_RIGHT));
            if (v->editing_name) SDL_StartTextInput(s->window);
        } else if (event->type == SDL_EVENT_MOUSE_WHEEL && v->editor)
            mercs_controls_scroll(v, wheel_rows(s, event->wheel.y));
        return;
    }
    if (s->combat.active && !s->dialog.active) {
        int preview = 0;
        if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
            SDL_Keycode key = event->key.key;
            if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) &&
                (event->key.mod & SDL_KMOD_ALT)) toggle_fullscreen(s);
            else if (key == SDLK_ESCAPE) {
                if (s->combat.graphics) s->combat.graphics = 0;
                else s->combat.active = 0;
            } else if (key == SDLK_G) { s->combat.graphics = !s->combat.graphics; s->combat.drag = -1; }
            else if (s->combat.graphics) {
                if (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_TAB) {
                    int i, direction = key == SDLK_UP || (key == SDLK_TAB && (event->key.mod & SDL_KMOD_SHIFT)) ? -1 : 1;
                    for (i = 0; i < 6; i++) {
                        s->combat.graphics_focus = (s->combat.graphics_focus + direction + 6) % 6;
                        if (mercs_combat_graphics_enabled(&s->combat, s->combat.graphics_focus)) break;
                    }
                } else if (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE)
                    mercs_combat_graphics_step(&s->combat, &s->options, s->combat.graphics_focus, key == SDLK_LEFT ? -1 : 1);
            }
        } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_RIGHT) {
            if (s->combat.graphics) s->combat.graphics = 0;
            else s->combat.active = 0;
        }
        else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
            if (mercs_combat_graphics_button(s->art, s->mouse_x, s->mouse_y)) {
                s->combat.graphics = !s->combat.graphics; s->combat.drag = -1;
            } else if (s->combat.graphics) {
                int row = mercs_combat_graphics_hit(s->art, s->mouse_x, s->mouse_y);
                if (row >= 0) {
                    s->combat.graphics_focus = row;
                    mercs_combat_graphics_step(&s->combat, &s->options, row, 1);
                }
            } else {
                if (mercs_combat_gpu_hit(s->art, s->mouse_x, s->mouse_y))
                    mercs_combat_gpu_step(&s->combat);
                else {
                    int row = mercs_combat_hit(s->art, s->mouse_x, s->mouse_y);
                    if (row >= 13 && row < 16) {
                        s->combat.drag = row - 13;
                        s->combat.preview_value = mercs_options_volume(&s->options, mercs_combat_volume_offset[s->combat.drag]);
                        preview = mercs_combat_slide(&s->combat, &s->options, s->mouse_x);
                    } else mercs_combat_toggle(&s->combat, &s->options, row);
                }
            }
        } else if (event->type == SDL_EVENT_MOUSE_MOTION && !s->combat.graphics && s->combat.drag >= 0 && s->mouse_x >= 0)
            preview = mercs_combat_slide(&s->combat, &s->options, s->mouse_x);
        else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT)
            s->combat.drag = -1;
        if (preview && s->audio) {
            const mercs_shell_wave *wave = mercs_shell_audio_wave(s->shell_audio, 52);
            /* DOS0x4140c temporarily uses the chosen level for its effect
             * preview, regardless of which of the three sliders is moving. */
            if (wave) {
                sfx_stop_pcm(s->audio, (const unsigned char *)wave->samples);
                sfx_set_gains(s->audio, (float)s->combat.preview_value / 65536.0f, 1.0f);
                sfx_play_pcm_s16(s->audio, wave->samples, wave->frames, wave->channels, wave->rate, SFX_VOL(50), 0);
            }
        }
        return;
    }
    if (s->panel_mode) {
        if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
            if ((event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER) &&
                (event->key.mod & SDL_KMOD_ALT)) toggle_fullscreen(s);
            else s->panel_mode = 0;
        } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                  (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT))
            s->panel_mode = 0;
        return;
    }
    if (s->dialog.active || s->game_menu.active) {
        if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
            SDL_Keycode k = event->key.key;
            if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && (event->key.mod & SDL_KMOD_ALT))
                toggle_fullscreen(s);
            else if (s->dialog.active) mercs_dialog_key(&s->dialog, k);
            else mercs_game_menu_key(&s->game_menu, k);
        } else if (event->type == SDL_EVENT_MOUSE_MOTION ||
                   (left && event->button.button == SDL_BUTTON_LEFT)) {
            int action = event->type == SDL_EVENT_MOUSE_MOTION ? MERCS_DIALOG_MOVE :
                         event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ? MERCS_DIALOG_PRESS : MERCS_DIALOG_RELEASE;
            if (s->dialog.active) mercs_dialog_mouse(&s->dialog, s->mouse_x, s->mouse_y, action);
            else mercs_game_menu_mouse(&s->game_menu, s->mouse_x, s->mouse_y, action);
        } else if (s->game_menu.active && event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                   event->button.button == SDL_BUTTON_RIGHT)
            mercs_game_menu_mouse(&s->game_menu, s->mouse_x, s->mouse_y, MERCS_GAME_MENU_RIGHT_PRESS);
        return; /* Modal text, wheel and shortcuts cannot reach the room. */
    }
    if (event->type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode k = event->key.key;
        if (!event->key.repeat || k == SDLK_BACKSPACE || k == SDLK_UP || k == SDLK_DOWN ||
            k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_PAGEUP || k == SDLK_PAGEDOWN)
            key(s, k, (SDL_Keymod)event->key.mod);
    }
    else if (event->type == SDL_EVENT_TEXT_INPUT) text_input(s, event->text.text);
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT &&
             s->mouse_x >= 0 && s->mouse_y >= 0)
        click(s, s->mouse_x, s->mouse_y, event->button.clicks);
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_RIGHT)
        open_game_menu(s);
    else if (event->type == SDL_EVENT_MOUSE_WHEEL && (s->page == PAGE_BRIEFING || s->page == PAGE_NEWS ||
             s->page == PAGE_CONTRACTS || s->page == PAGE_DEBRIEF || s->page == PAGE_FINANCE ||
             s->page == PAGE_DROP_FINANCE || s->page == PAGE_PERSONNEL || s->page == PAGE_INVENTORY ||
             s->page == PAGE_DROP_INVENTORY || s->page == PAGE_TRADING || s->page == PAGE_FACTORY ||
             s->page == PAGE_CREW_BIO)) scroll_text(s, wheel_rows(s, event->wheel.y));
}

static void close_modal_background(shell *s)
{
    if (s->menu_backdrop) return;
    mercs_art_snapshot_free(s->modal_background); s->modal_background = NULL;
    if (s->running && (s->page == PAGE_REGISTER || (s->page == PAGE_SAVE && s->save_editing)))
        SDL_StartTextInput(s->window);
}

static int run_dialog(shell *s)
{
    SDL_Event event;
    int answer;
    SDL_StopTextInput(s->window);
    while (s->running && s->dialog.active) {
        draw(s);
        if (s->running && SDL_WaitEventTimeout(&event, 16)) dispatch_event(s, &event);
    }
    answer = s->running ? s->dialog.answer : MERCS_DIALOG_PENDING;
    s->dialog.active = 0;
    close_modal_background(s);
    return answer;
}

static int play_cue(shell *s, mercs_shell_audio_cue cue)
{
    SDL_Event event;
    if (mercs_shell_audio_trigger(s->shell_audio, cue)) {
        SDL_StopTextInput(s->window);
        while (s->running && mercs_shell_audio_busy(s->shell_audio)) {
            mercs_shell_audio_tick(s->shell_audio);
            draw(s);
            if (s->running && SDL_WaitEventTimeout(&event, 16)) {
                if (!system_event(s, &event) && event.type == SDL_EVENT_MOUSE_MOTION)
                    pointer_event(s, event.motion.x, event.motion.y);
                /* The original blocking cue finishes before its action; room
                 * clicks/typing during it cannot activate a second action. */
            }
        }
        if (s->running && (s->page == PAGE_REGISTER || s->save_editing || s->mechlab_editing)) SDL_StartTextInput(s->window);
    }
    return s->running;
}

static int career_dirty(const shell *s)
{
    const uint8_t *h = mercs_save_header(s->campaign.document);
    return s->career.registered && ((h && h[0] == 1) ||
           (s->repair && mercs_repair_dirty(s->repair) == 1) ||
           (s->mechlab && !s->mechlab_instant && mercs_mechlab_dirty(s->mechlab)));
}

static void discard_repair_for_menu(shell *s)
{
    mercs_repair_free(s->repair); s->repair = NULL; s->repair_finished = 0;
    release_mechlab(s);
}

static void dismiss_menu_background(shell *s)
{
    s->menu_backdrop = 0;
    close_modal_background(s);
}

static int prepare_original_panel(shell *s, int which)
{
    unsigned char *bytes = NULL;
    size_t length = 0;
    char path[PORTCFG_PATH];
    if (!s->panels) {
        mercs_panels *data = malloc(sizeof *data);
        if (!data || dp_read_file("MW2SHELL.EXE", &bytes, &length) ||
            mercs_panels_decode(bytes, length, data)) {
            free(data); free(bytes);
            snprintf(s->status, sizeof s->status,
                     "The original DOS MW2SHELL.EXE data for this screen could not be read.");
            return -1;
        }
        free(bytes); s->panels = data;
    }
    if (which == 1) {
        char overlay[PORTCFG_PATH];
        const char *fallback=NULL;
        if (!dp_resolve(s->panels->scores_filename,path,sizeof path)) fallback=path;
        if (portcfg_overlay_dir(s->cfg,"retail",overlay,sizeof overlay,1) ||
            mercs_scores_load(overlay,fallback,&s->panels->defaults,&s->lords,s->status,sizeof s->status)) return -1;
    }
    return 0;
}

static void original_panel(shell *s, int which)
{
    SDL_Event event;
    if (prepare_original_panel(s, which)) {
        char message[sizeof s->status];
        memcpy(message, s->status, sizeof message); s->status[0] = 0;
        game_notice(s, message); return;
    }
    s->panel_mode = which; s->panel_polls = 0;
    SDL_StopTextInput(s->window);
    while (s->running && s->panel_mode) {
        draw(s);
        if (s->panel_polls < 2u * (460u + 20u * MERCS_CIRCUIT_ROWS + 1u)) s->panel_polls++;
        if (s->running && SDL_WaitEventTimeout(&event, 16)) dispatch_event(s, &event);
    }
    s->panel_mode = 0;
    close_modal_background(s);
}

static void apply_shell_volumes(shell *s)
{
    sfx_set_gains(s->audio, (float)mercs_options_volume(&s->options, MO_EFFECTS) / 65536.0f,
                           (float)mercs_options_volume(&s->options, MO_VOICE) / 65536.0f);
}

static void original_combat_variables(shell *s)
{
    SDL_Event event;
    char error[384];
    mercs_options loaded;
    portcfg saved;
    if (mercs_options_load(&loaded, s->cfg->install, s->cfg->cd, error, sizeof error)) {
        game_notice(s, error); return;
    }
    s->options = loaded;
    /* Mission menu changes can arrive after this shell snapshot was created. */
    if (!portcfg_load(NULL, &saved)) {
        s->cfg->texture_filter = saved.texture_filter;
        s->cfg->dos_sky_smooth = saved.dos_sky_smooth;
        s->cfg->hud_original = saved.hud_original;
        s->cfg->gpu = saved.gpu;
    }
    mercs_combat_open(&s->combat, s->cfg);
    mercs_combat_graphics_availability(&s->combat, s->cfg);
    SDL_StopTextInput(s->window);
    mercs_shell_audio_suspend(s->shell_audio);
    for (;;) {
        while (s->running && s->combat.active) {
            draw(s);
            if (s->running && SDL_WaitEventTimeout(&event, 16)) dispatch_event(s, &event);
        }
        if (!mercs_options_save(&s->options, s->cfg->install, error, sizeof error)) {
            mercs_options preferences = s->options;
            error[0] = 0;
            if (s->combat.edition_changed && s->combat.edition != s->edition &&
                change_game_look_options(s, s->combat.edition, &preferences, s->combat.resolution_changed))
                snprintf(error, sizeof error, "%s", s->status);
            if (!error[0]) {
                unsigned fields = SAVE_GRAPHICS | (s->combat.edition_changed ? SAVE_GAME_LOOK : 0);
                s->cfg->texture_filter = s->combat.texture_filter;
                s->cfg->dos_sky_smooth = s->combat.dos_shading;
                s->cfg->hud_original = s->combat.hud_original;
                if (s->combat.gpu_changed) {
                    s->cfg->gpu = s->combat.gpu; fields |= SAVE_GPU;
                }
                if (s->combat.resolution_changed) {
                    mercs_combat_resolution(&s->combat, s->cfg); fields |= SAVE_RENDER_SIZE;
                }
                save_settings_fields(s, fields);
                snprintf(error, sizeof error, "%s", s->status);
            }
            s->status[0] = 0;
            if (!error[0]) break;
        }
        if (!s->running) { fprintf(stderr, "mercs shell: %s\n", error); break; }
        s->combat.active = 1;
        game_notice(s, error);
    }
    s->combat.active = 0; s->combat.drag = -1;
    {
        const mercs_shell_wave *wave = mercs_shell_audio_wave(s->shell_audio, 52);
        if (wave) sfx_stop_pcm(s->audio, (const unsigned char *)wave->samples);
    }
    apply_shell_volumes(s);
    mercs_shell_audio_resume(s->shell_audio);
    close_modal_background(s);
}

static void controls_action(shell *s, int action)
{
    mercs_controls_view *v = &s->controls;
    mercs_control_config next = v->config;
    char error[384] = {0};
    if (action == MCV_NONE) return;
    if (action == MCV_CUSTOM || action == MCV_DEFAULTS ||
        (action == MCV_ACCEPT && !v->editor && v->changed)) {
        if (mercs_controls_defaults(&next, v->selected)) {
            game_notice(s, "The original device defaults could not be loaded from GIDDI."); return;
        }
    }
    if (action == MCV_CUSTOM || action == MCV_DEFAULTS) {
        v->config = next; v->editor = 1; v->level = 0; v->slot = 0;
        v->control = -1; v->half = 0; return;
    }
    if (action == MCV_ACCEPT) {
        if (!v->editor && !v->changed) { v->active = 0; return; }
        if (mercs_controls_apply(&next, s->cfg->install, error, sizeof error)) game_notice(s, error);
        else { v->config = next; v->active = 0; }
    } else if (action == MCV_NOVICE) {
        mercs_options options = s->options;
        int novice = !v->novice;
        if ((novice && mercs_controls_load(&next, "NOVICE.CPC")) ||
            (!novice && mercs_controls_defaults(&next, v->selected))) {
            game_notice(s, "The original control defaults could not be loaded from GIDDI."); return;
        }
        mercs_options_set(&options, 0x50, (uint32_t)novice);
        if (mercs_options_save_sound(&options, s->cfg->install, error, sizeof error)) { game_notice(s,error); return; }
        if (mercs_controls_apply(&next, s->cfg->install, error, sizeof error)) {
            char rollback[384];
            if (mercs_options_save_sound(&s->options, s->cfg->install, rollback, sizeof rollback))
                fprintf(stderr,"mercs controls: could not restore prior novice option: %s\n",rollback);
            game_notice(s,error); return;
        }
        s->options = options; v->config = next; v->novice = novice;
        mercs_controls_selected(&v->config, v->selected); v->changed = 0;
    } else if (action >= MCV_LOAD_1 && action < MCV_LOAD_1+4) {
        int slot = action-MCV_LOAD_1+1;
        char name[24]; snprintf(name,sizeof name,"CONFIG%02d.CPC",slot);
        if (mercs_controls_load(&next,name)) { game_notice(s,"This custom control configuration could not be loaded."); return; }
        v->config = next; v->slot = slot; v->level = 0; v->control = -1; v->half = 0;
        mercs_controls_selected(&v->config,v->selected);
        if (!v->selected[v->device]) { v->device=0; v->scroll=0; }
    } else if (action >= MCV_SAVE_1 && action < MCV_SAVE_1+4) {
        int slot = action-MCV_SAVE_1+1;
        if (mercs_controls_save_slot(&next,s->cfg->install,slot,error,sizeof error)) game_notice(s,error);
        else v->slot = slot;
    }
}

static void original_cockpit_controls(shell *s)
{
    SDL_Event event;
    if (mercs_controls_open(&s->controls,mercs_options_get(&s->options,0x50)!=0)) {
        game_notice(s,"The original Cockpit Controls data could not be loaded from GIDDI."); return;
    }
    SDL_StopTextInput(s->window); mercs_shell_audio_suspend(s->shell_audio);
    while (s->running && s->controls.active) {
        draw(s);
        if (s->running && SDL_WaitEventTimeout(&event,16)) dispatch_event(s,&event);
    }
    s->controls.active = 0; s->controls.editing_name = 0;
    SDL_StopTextInput(s->window); mercs_shell_audio_resume(s->shell_audio);
    close_modal_background(s);
}

static void open_game_menu(shell *s)
{
    SDL_Event event;
    int selected = 0, again = 1;
    if (s->dialog.active || s->game_menu.active || s->transition >= 0 || s->completed_transition) return;
    if (s->page == PAGE_SETTINGS) set_page(s, s->settings_return);
    finish_save_description(s, 0);
    finish_mechlab_name(s, 0);
    while (s->running && again) {
        int answer;
        again = 0;
        mercs_game_menu_open(&s->game_menu); s->game_menu.selected = selected;
        SDL_StopTextInput(s->window);
        while (s->running && s->game_menu.active) {
            draw(s);
            if (s->running && SDL_WaitEventTimeout(&event, 16)) dispatch_event(s, &event);
        }
        answer = s->running ? s->game_menu.answer : MERCS_GAME_MENU_CANCEL;
        selected = s->game_menu.selected; s->game_menu.active = 0;
        if (!s->running || answer == MERCS_GAME_MENU_CANCEL) break;
        s->menu_backdrop = 1;
        if (answer == MERCS_GAME_MENU_NEW) {
            if (career_dirty(s) && game_question(s, "Save Game?", 0)) {
                dismiss_menu_background(s);
                open_save_load(s);
                if (s->page == PAGE_SAVE) s->after_save = 1;
            } else if (s->running) {
                uint8_t *header = mercs_save_header_edit(s->campaign.document);
                dismiss_menu_background(s);
                discard_repair_for_menu(s);
                if (header) header[0] = 0;
                s->after_save = 0; set_page(s, PAGE_MENU);
            }
        } else if (answer == MERCS_GAME_MENU_QUIT) {
            if (!game_question(s, "Sell Out?", 1)) { again = s->running; continue; }
            if (career_dirty(s) && game_question(s, "Save Game Before Exiting?", 0)) {
                dismiss_menu_background(s);
                open_save_load(s);
                if (s->page == PAGE_SAVE) s->after_save = 2;
            } else if (s->running) { dismiss_menu_background(s); s->running = 0; }
        } else if (answer == MERCS_GAME_MENU_SAVE) {
            dismiss_menu_background(s);
            if (s->page != PAGE_SAVE) {
                open_save_load(s); s->after_save = 0;
            }
        } else if (answer == MERCS_GAME_MENU_LORDS || answer == MERCS_GAME_MENU_CIRCUIT) {
            original_panel(s, answer == MERCS_GAME_MENU_LORDS ? 1 : 2);
            again = s->running;
        } else if (answer == MERCS_GAME_MENU_COMBAT) {
            original_combat_variables(s);
            again = s->running;
        } else if (answer == MERCS_GAME_MENU_COCKPIT) {
            original_cockpit_controls(s);
            again = s->running;
        } else {
            again = s->running;
        }
    }
    s->game_menu.active = 0;
    dismiss_menu_background(s);
}

static void start_shell(shell *s)
{
    /* Cold launch enters the original five-button menu after MINTRO. The
     * working generation is a transaction/recovery checkpoint, not a request
     * to replay its last debrief. Saved careers are selected through the
     * original Load/Save screen; a live mission child still returns through
     * launch_mission's normal result path. Do not mutate either kind of save
     * merely because the executable was opened. */
    set_page(s, PAGE_MENU);
    if (!getenv("MERCS_SHELL_SKIP_INTRO")) play_movie(s, "MINTRO");
}

static const char *shell_renderer_drivers(const portcfg *cfg)
{
#if defined(_WIN32)
    int gpu = cfg->gpu;
    const char *override = getenv("MERCS_GPU");
    if (!override || !override[0]) override = getenv("MW2_GPU");
    if (override) {
        if (!SDL_strcasecmp(override, "vulkan")) gpu = PORTCFG_GPU_VULKAN;
        else if (!SDL_strcasecmp(override, "d3d12")) gpu = PORTCFG_GPU_D3D12;
    }
    return gpu == PORTCFG_GPU_VULKAN ? "vulkan,direct3d12,software" : "direct3d12,vulkan,software";
#elif defined(__APPLE__)
    (void)cfg; return "metal";
#else
    (void)cfg; return "vulkan,software";
#endif
}

static SDL_Renderer *create_shell_renderer(SDL_Window *window, const portcfg *cfg)
{
    const char *drivers = shell_renderer_drivers(cfg);
    SDL_Renderer *renderer;
    /* SDL's software renderer can otherwise build a default accelerated window
     * framebuffer of its own. Both paths must obey the no-OpenGL policy. */
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, drivers, SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0", SDL_HINT_OVERRIDE);
    renderer = SDL_CreateRenderer(window, drivers);
    if (!renderer) {
        char message[768];
        snprintf(message, sizeof message,
            "The Mercenaries menu renderer could not start.\n\n"
#if defined(_WIN32)
            "Tried Direct3D 12, Vulkan and the software renderer. "
            "Update your graphics driver and check that a desktop display is available.\n\n"
#elif defined(__APPLE__)
            "A Metal-capable Mac and macOS 10.14 or newer are required. "
            "Check that a desktop display is available.\n\n"
#else
            "Tried Vulkan and the software renderer. "
            "Check your Vulkan graphics driver and desktop display.\n\n"
#endif
            "SDL: %s", SDL_GetError());
        mercs_gui_error(message);
    } else fprintf(stderr, "mercs shell: renderer=%s (requested %s)\n", SDL_GetRendererName(renderer), drivers);
    return renderer;
}

int mercs_shell_run(const char *exe, portcfg *cfg, int edition)
{
    shell *s = calloc(1, sizeof *s);
    char path[PORTCFG_PATH], error[256], models[PORTCFG_PATH], textures[PORTCFG_PATH], kind[PORTCFG_PATH], sky[PORTCFG_PATH], fog[PORTCFG_PATH];
    SDL_Event event;
    int rc = 1, i, initialized = 0;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (!s) return 1;
    s->cfg = cfg; s->exe = exe; s->edition = edition; s->mode = 1; s->running = 1; s->accepted = -1; s->transition = -1; s->mouse_x = -1; s->mouse_y = -1;
    fprintf(stderr, "mercs shell: startup-look=%s\n", portcfg_mercs_name(edition));
    if (cfg->videos < 0 || cfg->videos >= PORTCFG_VID_COUNT) cfg->videos = PORTCFG_VID_ORIGINAL;
    if (cfg->video_fit < 0 || cfg->video_fit >= PORTCFG_FIT_COUNT) cfg->video_fit = PORTCFG_FIT_43;
    if (cfg->salvage < 0 || cfg->salvage >= PORTCFG_SALVAGE_COUNT) cfg->salvage = PORTCFG_SALVAGE_AUTO;
    if (dp_resolve("DATABASE.MW2", path, sizeof path) || shelldb_open(path, &s->database) || s->database.count != 120) {
        mercs_gui_error("The retail Mercenaries menu data could not be opened. Choose the complete Mercs-game folder."); goto cleanup;
    }
    if (portcfg_edition(cfg, edition, models, textures, kind, sky, fog) || !(s->archive = prj_open(models, error, sizeof error))) {
        mercs_gui_error("The Mercenaries mission archive could not be opened."); goto cleanup;
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) { mercs_gui_error(SDL_GetError()); goto cleanup; }
    initialized = 1;
    if (cfg->mode != 1) flags |= SDL_WINDOW_FULLSCREEN;
    s->window = SDL_CreateWindow("MechWarrior 2: Mercenaries", 1280, 960, flags);
    if (!s->window) { mercs_gui_error(SDL_GetError()); goto cleanup; }
    mercs_set_window_icon(s->window);
    if (cfg->mode == 1) SDL_SetWindowPosition(s->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_SetWindowMinimumSize(s->window, SW, SH);
    s->renderer = create_shell_renderer(s->window, cfg);
    if (!s->renderer) goto cleanup;
    SDL_SetRenderVSync(s->renderer, 1);
    mercs_viewport_init(&s->viewport, s->renderer);
    ttext_init(SDL_GetBasePath());
    { long n = shelldb_entry(&s->database, 5, &s->fallback_font); s->fallback_size = n > 0 ? (size_t)n : 0; }
    if (!background(s, 23) || !background(s, 32) || !background(s, 26)) {
        mercs_gui_error("The Mercenaries registration or mission artwork could not be decoded."); goto cleanup;
    }
    s->art = mercs_art_create(&s->database, s->renderer);
    if (!s->art) { mercs_gui_error("Could not create the original shell compositor."); goto cleanup; }
    {   /* Titanium-only shell clips (the arms dealer's repair button for
         * salvaged damaged items) are looked up after the game's own */
        char database[PORTCFG_PATH * 2], dir[PORTCFG_PATH];
        if (!salvage_assets(s, database, dir)) mercs_art_set_extra_dir(s->art, dir);
    }
    s->audio = sfx_create(NULL); mercs_art_set_audio(s->art, s->audio);
    s->shell_audio = mercs_shell_audio_create(s->audio, &s->database);
    if (mercs_options_load(&s->options, cfg->install, cfg->cd, s->status, sizeof s->status)) {
        fprintf(stderr, "mercs shell: %s\n", s->status);
        mercs_options_defaults(&s->options); s->status[0] = 0;
    }
    apply_shell_volumes(s);
    mercs_shell_audio_seed(s->shell_audio, (uint32_t)SDL_GetTicks());
    if (!s->audio || !s->shell_audio)
        fprintf(stderr, "mercs shell: original audio unavailable: %s\n", SDL_GetError());
    if (portcfg_overlay_dir(cfg, "retail", path, sizeof path, 1) ||
        mercs_store_directory(path, s->save_dir, sizeof s->save_dir)) {
        mercs_gui_error("The career save folder could not be created in the game installation."); goto cleanup;
    }
    start_shell(s);
    SDL_ShowCursor();
    while (s->running) {
        poll_shell(s);
        if (s->status[0] && s->page != PAGE_SETTINGS) {
            char message[sizeof s->status];
            memcpy(message, s->status, sizeof message);
            s->status[0] = 0;
            game_notice(s, message);
        }
        if (!s->running) break;
        draw(s);
        if (!SDL_WaitEventTimeout(&event, 33)) continue;
        do {
            dispatch_event(s, &event);
        } while (s->running && SDL_PollEvent(&event));
    }
    rc = 0;
cleanup:
    if (s->window) SDL_StopTextInput(s->window);
    for (i = 0; i < TEXT_CACHE; i++) SDL_DestroyTexture(s->text[i].texture);
    for (i = 0; i < 120; i++) SDL_DestroyTexture(s->backgrounds[i]);
    mercs_text_free(&s->body);
    mercs_debrief_view_free(&s->debrief_view);
    mercs_finance_view_free(&s->finance_view);
    mercs_personnel_view_free(&s->personnel_view);
    mercs_trading_view_free(&s->trading_view);
    mercs_inventory_view_free(&s->inventory_view);
    mercs_repair_free(s->repair);
    mercs_instant_free(&s->instant);
    release_mechlab(s);
    mercs_campaign_free(&s->campaign);
    free(s->panels);
    mercs_art_snapshot_free(s->modal_background);
    mercs_viewport_reset(&s->viewport);
    mercs_art_destroy(s->art);
    mercs_shell_audio_destroy(s->shell_audio);
    sfx_destroy(s->audio);
    free(s->fallback_font);
    if (s->archive) prj_close(s->archive);
    shelldb_close(&s->database);
    SDL_DestroyRenderer(s->renderer); SDL_DestroyWindow(s->window);
    if (initialized) SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    free(s); return rc;
}
