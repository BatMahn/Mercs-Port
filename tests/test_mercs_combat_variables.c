#include "sdl3_test.h"
/* Actual shell menu/event/render/settings integration. Requires a PRIVATE game
 * folder with external original assets. Never run against the user's saves.
 * SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy ./test_mercs_combat_variables
 *   PRIVATE_GAME [CAPTURE_DIRECTORY]
 * The bounded x86 reference probe lives in tests/fixtures/combat-variables/. */
#include <SDL3/SDL.h>
static bool combat_present(SDL_Renderer *renderer);
#define SDL_RenderPresent combat_present
#include "../tools/mercsshell.c"
#undef SDL_RenderPresent
#include <assert.h>
#include <sys/stat.h>
#include "../tests/fixtures/combat-variables/original-widget-fixtures.h"

static shell *observed;
static const char *captures;
static int active_frames, saw_original, saw_changed, preview_played;
static int graphics_seen[3][PORTCFG_FILTER_COUNT];

static void capture(SDL_Renderer *renderer, const char *name)
{
    SDL_Surface *surface;
    int w, h;
    char path[2048];
    if (!captures) return;
    assert(SDL_GetCurrentRenderOutputSize(renderer, &w, &h));
    surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888); assert(surface);
    assert(!mercs_test_read_pixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,surface->pixels,surface->pitch));
    snprintf(path,sizeof path,"%s/%s.bmp",captures,name);
    assert(SDL_SaveBMP(surface,path)); SDL_DestroySurface(surface);
}

static bool combat_present(SDL_Renderer *renderer)
{
    shell *s = observed;
    if (s && s->combat.active) {
        active_frames++;
        if (mercs_shell_audio_resource_playing(s->shell_audio,52)) preview_played=1;
        assert(!s->game_menu.active && !s->dialog.active);
        if (s->combat.graphics) {
            int row = s->combat.edition == PORTCFG_ED_D3D ? 1 : s->combat.edition == PORTCFG_ED_3DFX ? 2 : 0;
            if (!graphics_seen[row][s->combat.texture_filter]++) {
                char name[80];
                snprintf(name, sizeof name, "combat-graphics-%s-%s", portcfg_mercs_name(s->combat.edition), PORTCFG_FILTER_NAME[s->combat.texture_filter]);
                capture(renderer, name);
            }
        }
        if (!saw_original++) capture(renderer,"combat-original-values");
        if (!saw_changed && !mercs_options_get(&s->options,MO_SUBTITLES) &&
            mercs_options_volume(&s->options,MO_MUSIC)==0 &&
            mercs_options_volume(&s->options,MO_EFFECTS)==32768 &&
            mercs_options_volume(&s->options,MO_VOICE)==16384) {
            saw_changed=1; capture(renderer,"combat-edited-values");
        }
    }
    return SDL_RenderPresent(renderer);
}

static void push_key(SDL_Keycode key)
{
    SDL_Event e;
    SDL_zero(e); e.type=SDL_EVENT_KEY_DOWN; e.key.key=key; e.key.down=true;
    assert(SDL_PushEvent(&e)==1);
}

static void coords(shell *s,int x,int y,int *rx,int *ry)
{
    SDL_Rect dst;
    int ww,wh,ow,oh;
    SDL_GetWindowSize(s->window,&ww,&wh);
    assert(SDL_GetCurrentRenderOutputSize(s->renderer,&ow,&oh));
    assert(mercs_viewport_fit(ow,oh,&dst)==1);
    *rx=(int)lround((dst.x+(x+0.5)*(double)dst.w/640)*ww/ow);
    *ry=(int)lround((dst.y+(y+0.5)*(double)dst.h/480)*wh/oh);
}

static void push_mouse(shell *s,Uint32 type,int button,int x,int y)
{
    SDL_Event e;
    int rx,ry;
    coords(s,x,y,&rx,&ry); SDL_zero(e); e.type=type;
    if(type==SDL_EVENT_MOUSE_MOTION){e.motion.x=(float)rx;e.motion.y=(float)ry;e.motion.state=SDL_BUTTON_LMASK;}
    else {e.button.button=(Uint8)button;e.button.x=(float)rx;e.button.y=(float)ry;e.button.clicks=1;e.button.down=type==SDL_EVENT_MOUSE_BUTTON_DOWN;}
    assert(SDL_PushEvent(&e)==1);
}

static void push_click(shell *s,int x,int y)
{
    push_mouse(s,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,x,y);
    push_mouse(s,SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,x,y);
}

static Uint32 SDLCALL watchdog(void *unused, SDL_TimerID timer_id, Uint32 interval)
{
    SDL_Event e;(void)timer_id; (void)interval;(void)unused;
    SDL_zero(e);e.type=SDL_EVENT_QUIT;SDL_PushEvent(&e);
    fputs("FAIL: Combat Variables modal stalled\n",stderr);return 0;
}

static void write_raw(const char *dir,const char *name,const uint8_t *bytes,size_t n)
{
    char path[2048];FILE *f;
    snprintf(path,sizeof path,"%s/%s",dir,name);f=fopen(path,"wb");assert(f);
    assert(fwrite(bytes,1,n,f)==n);assert(!fclose(f));
}

static void check_original_widgets(mercs_art *art)
{
    size_t i;
    mercs_options options;
    mercs_combat_view view;
    portcfg cfg;
    portcfg_defaults(&cfg);
    for (i=0;i<sizeof mercs_combat_original_rects/sizeof *mercs_combat_original_rects;i++) {
        const mercs_combat_original_rect *r=&mercs_combat_original_rects[i];
        assert(mercs_combat_hit(art,r->x,r->y)==r->row);
        assert(mercs_combat_hit(art,r->x+r->w-1,r->y+r->h-1)==r->row);
    }
    for (i=0;i<sizeof mercs_combat_original_hits/sizeof *mercs_combat_original_hits;i++) {
        const mercs_combat_original_hit *h=&mercs_combat_original_hits[i];
        assert(mercs_combat_hit(art,h->x,h->y)==h->hit);
    }
    for (i=0;i<sizeof mercs_combat_original_slides/sizeof *mercs_combat_original_slides;i++) {
        const mercs_combat_original_slide *s=&mercs_combat_original_slides[i];
        mercs_options_defaults(&options);
        mercs_combat_open(&view,&cfg);
        view.drag=s->slider;
        view.preview_value=s->initial;
        mercs_options_set(&options,mercs_combat_volume_offset[view.drag],(uint32_t)s->initial);
        assert(mercs_combat_slide(&view,&options,s->x)==s->preview);
        assert(mercs_options_volume(&options,mercs_combat_volume_offset[view.drag])==s->value);
        assert(view.preview_value==(s->preview?s->value:s->initial));
    }
    for (i=0;i<sizeof mercs_combat_original_texts/sizeof *mercs_combat_original_texts;i++) {
        const mercs_combat_original_text *t=&mercs_combat_original_texts[i];
        int width=mercs_art_width(art,6,t->text);
        assert(width==t->width && 423-width/2==t->x);
        assert(mercs_art_height(art,6)==t->height && mercs_combat_y[i]==t->y);
    }
    puts("PASS: original x86-derived Combat Variables rectangles,96 edge cases,48 slider/preview cases,13 original-font text layouts.");
}

static void check_graphics(shell *s)
{
    portcfg cfg, saved;
    mercs_options destination, readback;
    mercs_combat_view availability;
    SDL_Event event;
    SDL_TimerID timer = SDL_AddTimer(30000, watchdog, NULL);
    char overlay[PORTCFG_PATH], error[384], pilot[64];
    int i, funds, owned, phase;
    assert(timer);
    /* Real registered career, not just an unregistered settings mock. */
    assert(!mercs_campaign_new(s->archive, 1, "GRAPHICS PILOT", "GRAPHICS UNIT", &s->campaign, &s->career, error, sizeof error));
    s->mode = s->career.mode; s->selected = s->campaign.selected; s->accepted = s->campaign.accepted;
    snprintf(pilot, sizeof pilot, "%s", s->career.pilot);
    funds = s->career.funds; owned = s->career.owned_count;
    for (i = 1; i < 3; i++) {
        assert(!portcfg_overlay_dir(s->cfg, portcfg_mercs_overlay(mercs_combat_editions[i]), overlay, sizeof overlay, 1));
        mercs_options_defaults(&destination);
        destination.snd_size = 109; destination.dif_size = 35;
        destination.snd[0x47] = (uint8_t)(0x80 + i);
        destination.snd[0x50] = (uint8_t)(0x90 + i); destination.dif[8] = (uint8_t)(0xa0 + i);
        memset(destination.snd + MERCS_SND_BYTES, 0xb0 + i, destination.snd_size - MERCS_SND_BYTES);
        memset(destination.dif + MERCS_DIF_BYTES, 0xc0 + i, destination.dif_size - MERCS_DIF_BYTES);
        assert(!mercs_options_save(&destination, overlay, error, sizeof error));
    }
    for (i = 0; i < 3; i++) {
        int expected = mercs_combat_editions[(i + 1) % 3];
        while (SDL_PollEvent(&event)) {}
        if (!i) push_click(s, 320, 447); /* discoverable original-screen link */
        else push_key(SDLK_G);
        push_key(SDLK_RIGHT); /* DOS -> Titanium -> dedicated3dfx -> DOS */
        push_key(SDLK_DOWN); push_key(SDLK_RIGHT); /* Original -> nearest -> smooth -> Original */
        if (i != 1) {
            push_key(SDLK_DOWN); push_key(SDLK_RIGHT); /* object textures off, then on */
            push_key(SDLK_DOWN); push_key(SDLK_RIGHT); /* terrain textures off, then on */
            push_key(SDLK_DOWN); push_key(SDLK_RIGHT); /* HUD on accelerated; DOS shading on DOS */
        }
        push_key(SDLK_ESCAPE); push_key(SDLK_ESCAPE);
        original_combat_variables(s);
        assert(!s->status[0] && s->running && !s->combat.active);
        assert(s->edition == expected && s->cfg->renderer == expected);
        assert(s->cfg->texture_filter == (i + 1) % PORTCFG_FILTER_COUNT);
        assert(!portcfg_load(NULL, &saved) && saved.renderer == expected && saved.texture_filter == s->cfg->texture_filter);
        assert(!strcmp(s->career.pilot, pilot) && s->career.funds == funds && s->career.owned_count == owned);
        assert(!mercs_options_load(&readback, s->cfg->install, s->cfg->cd, error, sizeof error));
        assert(mercs_options_get(&readback, MO_OBJECT_TEXTURES) == (unsigned)(i == 2));
        assert(mercs_options_get(&readback, MO_TERRAIN_TEXTURES) == (unsigned)(i == 2));
        if (i < 2) {
            assert(readback.snd_size == 109 && readback.dif_size == 35);
            assert(readback.snd[0x47] == 0x81 + i && readback.snd[0x50] == 0x91 + i && readback.dif[8] == 0xa1 + i);
            assert(readback.snd[108] == 0xb1 + i && readback.dif[34] == 0xc1 + i);
        }
    }
    assert(s->cfg->dos_sky_smooth && s->cfg->hud_original);
    /* A single available profile does not cycle to missing archives. The
     * missing alternate source is simulated in config; original files stay untouched. */
    cfg = *s->cfg; cfg.titanium[0] = 0; cfg.no3d = 1;
    mercs_combat_open(&availability, &cfg);
    mercs_combat_graphics_availability(&availability, &cfg);
    assert(availability.available[0] && !availability.available[1] && !availability.available[2]);
    mercs_combat_graphics_step(&availability, &s->options, 0, 1);
    assert(availability.edition == PORTCFG_ED_DOS);
    mercs_combat_graphics_step(&availability, &s->options, 1, -1);
    assert(availability.texture_filter == PORTCFG_FILTER_LINEAR);
    /* A saved next-mission choice is applied after return, retaining the
     * career and current native controls. A pending deployment remains pinned. */
    saved.renderer = PORTCFG_ED_D3D; saved.texture_filter = PORTCFG_FILTER_LINEAR;
    saved.hud_original = 0; saved.dos_sky_smooth = 0;
    assert(!portcfg_save(NULL, &saved));
    refresh_mission_graphics(s);
    assert(!s->status[0] && s->edition == PORTCFG_ED_D3D && s->cfg->texture_filter == PORTCFG_FILTER_LINEAR);
    assert(!s->cfg->hud_original && !s->cfg->dos_sky_smooth);
    assert(!strcmp(s->career.pilot, pilot) && s->career.funds == funds && s->career.owned_count == owned);
    assert(mercs_options_get(&s->options, MO_OBJECT_TEXTURES) && mercs_options_get(&s->options, MO_TERRAIN_TEXTURES));
    phase = s->campaign.phase; s->campaign.phase = MERCS_CAMPAIGN_DEPLOYED;
    saved.renderer = PORTCFG_ED_DOS; assert(!portcfg_save(NULL, &saved));
    refresh_mission_graphics(s); assert(s->edition == PORTCFG_ED_D3D);
    while (SDL_PollEvent(&event)) {}
    push_key(SDLK_G); push_key(SDLK_ESCAPE); push_key(SDLK_ESCAPE);
    original_combat_variables(s);
    assert(!portcfg_load(NULL, &saved) && saved.renderer == PORTCFG_ED_DOS);
    assert(s->edition == PORTCFG_ED_D3D && s->campaign.phase == MERCS_CAMPAIGN_DEPLOYED);
    s->campaign.phase = phase; refresh_mission_graphics(s); assert(s->edition == PORTCFG_ED_DOS);
    /* Original keyboard and right-button return behavior still works from
     * the second page, and reopening reads the latest child preferences. */
    while (SDL_PollEvent(&event)) {}
    push_key(SDLK_G);
    push_mouse(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_RIGHT, 100, 100);
    push_key(SDLK_ESCAPE); original_combat_variables(s);
    assert(!s->combat.active && s->combat.texture_filter == PORTCFG_FILTER_LINEAR && !s->combat.hud_original);
    SDL_RemoveTimer(timer);
    puts("PASS: original-art Combat Variables graphics page; keyboard/mouse cycling across DOS, Titanium Direct3D and dedicated3dfx; three texture filters; live native texture toggles; persistent reopen; known options carried with unknown destination bytes retained; active career and pending-return profile preserved; absent profile skipped.");
}

static void check_gpu_settings(shell *s)
{
    portcfg saved;
    mercs_combat_view view;
    mercs_options options = s->options;
    SDL_Event event;
    int gpu, expected, resolution = s->combat.resolution;
    int funds = s->career.funds, owned = s->career.owned_count, phase = s->campaign.phase;
    const char *drivers;
    unsetenv("MERCS_GPU"); unsetenv("MW2_GPU");
    assert(mercs_art_width(s->art, 5, mercs_combat_gpu_notice) <= 608);
    assert(427 + mercs_art_height(s->art, 5) <= 442);
    for (gpu = 0; gpu < PORTCFG_GPU_COUNT; gpu++) {
        s->cfg->gpu = gpu;
        drivers = shell_renderer_drivers(s->cfg);
#if defined(_WIN32)
        assert(!strcmp(drivers, gpu == PORTCFG_GPU_VULKAN ? "vulkan,direct3d12,software" : "direct3d12,vulkan,software"));
#elif defined(__APPLE__)
        assert(!strcmp(drivers, "metal"));
#else
        assert(!strcmp(drivers, "vulkan,software"));
#endif
        mercs_combat_open(&view, s->cfg);
        assert(view.gpu == gpu && !view.gpu_changed);
#if defined(_WIN32)
        assert(mercs_combat_gpu_hit(s->art, 484, 325));
        assert(mercs_combat_gpu_hit(s->art, 627, 340));
        assert(!mercs_combat_gpu_hit(s->art, 483, 325) && !mercs_combat_gpu_hit(s->art, 628, 325));
        assert(!mercs_combat_gpu_hit(s->art, 484, 324) && !mercs_combat_gpu_hit(s->art, 484, 341));
        mercs_combat_gpu_step(&view);
        assert(view.gpu == (gpu + 1) % PORTCFG_GPU_COUNT && view.gpu_changed);
#else
        assert(!mercs_combat_gpu_hit(s->art, 584, 329));
        mercs_combat_gpu_step(&view);
        assert(view.gpu == gpu && !view.gpu_changed);
#ifdef __APPLE__
        assert(!strcmp(mercs_combat_gpu_text(&view), "METAL"));
#else
        assert(!strcmp(mercs_combat_gpu_text(&view), "VULKAN"));
#endif
#endif
        assert(!view.resolution_changed);
        assert(mercs_art_width(s->art, 5, mercs_combat_gpu_text(&view)) <= 88);
    }
    if (captures) {
        mercs_combat_view previous = s->combat;
        /* Preview the changed-state notice on every test host; selection is
         * still disabled on Linux/macOS and this never commits a setting. */
        mercs_combat_open(&s->combat, s->cfg); s->combat.gpu_changed = 1;
        draw(s); capture(s->renderer, "combat-gpu-change-notice");
        s->combat = previous;
    }
#ifdef _WIN32
    s->cfg->gpu = PORTCFG_GPU_D3D12;
    setenv("MW2_GPU", "vulkan", 1);
    assert(!strcmp(shell_renderer_drivers(s->cfg), "vulkan,direct3d12,software"));
    setenv("MERCS_GPU", "D3D12", 1);
    assert(!strcmp(shell_renderer_drivers(s->cfg), "direct3d12,vulkan,software"));
    setenv("MERCS_GPU", "", 1);
    assert(!strcmp(shell_renderer_drivers(s->cfg), "vulkan,direct3d12,software"));
    unsetenv("MERCS_GPU"); unsetenv("MW2_GPU");
#endif
    /* A display or appearance edit from an older shell snapshot must not
     * overwrite a GPU choice made by the child mission. */
    assert(!portcfg_load(NULL, &saved)); saved.gpu = PORTCFG_GPU_VULKAN;
    saved.videos = PORTCFG_VID_CRT_CURVED; saved.video_fit = PORTCFG_FIT_FILL;
    assert(!portcfg_save(NULL, &saved));
    s->cfg->gpu = PORTCFG_GPU_AUTO;
    save_settings_fields(s, SAVE_DISPLAY | SAVE_GRAPHICS);
    assert(!s->status[0] && !portcfg_load(NULL, &saved) && saved.gpu == PORTCFG_GPU_VULKAN);
    assert(saved.videos == PORTCFG_VID_CRT_CURVED && saved.video_fit == PORTCFG_FIT_FILL);
    while (SDL_PollEvent(&event)) {}
    push_click(s, 584, 329); push_key(SDLK_ESCAPE);
    original_combat_variables(s);
    expected = mercs_combat_gpu_enabled() ? PORTCFG_GPU_AUTO : PORTCFG_GPU_VULKAN;
    assert(s->cfg->gpu == expected && !portcfg_load(NULL, &saved) && saved.gpu == expected);
    assert(s->combat.resolution == resolution && !s->combat.resolution_changed);
    assert(s->career.funds == funds && s->career.owned_count == owned && s->campaign.phase == phase);
    assert(s->options.snd_size == options.snd_size && s->options.dif_size == options.dif_size);
    assert(!memcmp(s->options.snd, options.snd, options.snd_size));
    assert(!memcmp(s->options.dif, options.dif, options.dif_size));
    assert(saved.videos == PORTCFG_VID_CRT_CURVED && saved.video_fit == PORTCFG_FIT_FILL);
    s->cfg->gpu = PORTCFG_GPU_D3D12; save_settings_fields(s, SAVE_GPU);
    assert(!s->status[0] && !portcfg_load(NULL, &saved) && saved.gpu == PORTCFG_GPU_D3D12);
    saved.gpu = PORTCFG_GPU_VULKAN; assert(!portcfg_save(NULL, &saved));
    refresh_mission_graphics(s); assert(s->cfg->gpu == PORTCFG_GPU_VULKAN);
    puts("PASS: platform GPU policy, original resolution hit box retained, fixed Metal/Vulkan UI or Windows selection, latest-file GPU merge/reopen, unrelated CRT/native options and career state retained.");
}

int main(int argc,char **argv)
{
    shell *s=calloc(1,sizeof *s);
    portcfg cfg, reread;
    mercs_options baseline, saved;
    SDL_Event e;SDL_TimerID timer;
    char path[PORTCFG_PATH*2],error[384];
    int i, j, changed[MERCS_OPTIONS_MAX]={0};
    assert(argc>=2 && s);captures=argc>2?argv[2]:NULL;
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO));
    portcfg_defaults(&cfg);cfg.mode=1;
    assert(!portcfg_resolve(&cfg,NULL,argv[1]));
    assert(!portcfg_overlay_dir(&cfg,"retail",cfg.install,sizeof cfg.install,1));
    assert(!portcfg_save(NULL,&cfg));
    assert(!dp_resolve_in(cfg.cd,"MERCS/DATABASE.MW2",path,sizeof path) ||
           !dp_resolve_in(cfg.cd,"DATABASE.MW2",path,sizeof path));
    assert(!shelldb_open(path,&s->database));
    dp_clear_roots();assert(!dp_add_root(cfg.cd));
    snprintf(path,sizeof path,"%s/MERCS",cfg.cd);dp_add_root(path);
    s->cfg=&cfg;s->running=1;s->page=PAGE_MENU;s->transition=-1;s->mouse_x=s->mouse_y=-1;
    s->edition=cfg.renderer;
    {
        char models[PORTCFG_PATH], textures[PORTCFG_PATH], kind[PORTCFG_PATH], sky[PORTCFG_PATH], fog[PORTCFG_PATH];
        assert(!portcfg_edition(&cfg, s->edition, models, textures, kind, sky, fog));
        s->archive=prj_open(models,error,sizeof error);assert(s->archive);
    }
    s->window=SDL_CreateWindow("Mercs original Combat Variables", 1000, 700, SDL_WINDOW_RESIZABLE);
    assert(s->window);s->renderer=create_shell_renderer(s->window, &cfg);
    assert(s->renderer);mercs_viewport_init(&s->viewport,s->renderer);
    assert(!strcmp(SDL_GetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION), "0"));
    assert(!strcmp(SDL_GetHint(SDL_HINT_RENDER_DRIVER), shell_renderer_drivers(&cfg)));
    assert(!strstr(SDL_GetRendererName(s->renderer), "opengl"));
    s->art=mercs_art_create(&s->database,s->renderer);assert(s->art);
    check_original_widgets(s->art);
    s->audio=sfx_create(NULL);assert(s->audio);
    s->shell_audio=mercs_shell_audio_create(s->audio,&s->database);assert(s->shell_audio);
    mercs_options_defaults(&baseline);
    baseline.snd_size=107;baseline.dif_size=39;
    for(i=MERCS_SND_BYTES;i<(int)baseline.snd_size;i++)baseline.snd[i]=(uint8_t)(i^0x5a);
    for(i=MERCS_DIF_BYTES;i<(int)baseline.dif_size;i++)baseline.dif[i]=(uint8_t)(i^0xa5);
    baseline.snd[0x50]=0x6b;baseline.dif[8]=0xa3;
    assert(!mercs_options_save(&baseline,cfg.install,error,sizeof error));
    s->options=baseline;observed=s;draw(s);
    while(SDL_PollEvent(&e)){}
    timer=SDL_AddTimer(20000,watchdog,NULL);assert(timer);
    push_key(SDLK_DOWN);push_key(SDLK_RETURN); /* original menu row1 */
    for(i=0;i<13;i++)push_click(s,423,mercs_combat_y[i]+4);
    /* Native ->320. Choose640 without affecting the window/desktop. */
    push_click(s,423,329);
    /* Drag across authored slider bounds, then to fractional levels. */
    push_mouse(s,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,480,121);
    push_mouse(s,SDL_EVENT_MOUSE_MOTION,0,300,121);
    push_mouse(s,SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,300,121);
    push_click(s,478,145);push_click(s,414,169);
    /* An unrelated key must not dismiss the original screen. */
    push_key(SDLK_A);push_key(SDLK_ESCAPE);push_key(SDLK_ESCAPE);
    open_game_menu(s);
    SDL_RemoveTimer(timer);
    assert(s->running && !s->combat.active && !s->game_menu.active && !s->dialog.active && !s->status[0]);
    assert(active_frames>=20 && saw_changed && preview_played);
    assert(!mercs_options_load(&saved,cfg.install,cfg.cd,error,sizeof error));
    assert(saved.snd_size==baseline.snd_size && saved.dif_size==baseline.dif_size);
    assert(saved.dif[5]==2 && !saved.dif[4]);
    assert(mercs_options_resolution(&saved)==1);
    assert(mercs_options_volume(&saved,MO_MUSIC)==0);
    assert(mercs_options_volume(&saved,MO_EFFECTS)==32768);
    assert(mercs_options_volume(&saved,MO_VOICE)==16384);
    for(i=0;i<13;i++)if(mercs_combat_offset[i]>=0){
        assert(!mercs_options_get(&saved,mercs_combat_offset[i]));
        for(j=0;j<4;j++)changed[mercs_combat_offset[i]+j]=1;
    }
    for(i=4;i<16;i++)changed[i]=1;
    for(i=MO_DRIVER;i<MO_DRIVER+15;i++)changed[i]=1;
    for(i=0;i<(int)saved.snd_size;i++)if(!changed[i])assert(saved.snd[i]==baseline.snd[i]);
    for(i=0;i<(int)saved.dif_size;i++)if(i!=4 && i!=5)assert(saved.dif[i]==baseline.dif[i]);
    assert(!portcfg_load(NULL,&reread) && reread.render_w==640 && reread.render_h==480);
    assert(cfg.mode==1 && cfg.render_w==640 && cfg.render_h==480);
    assert(!mercs_options_transition(&saved,"MLAND") && !mercs_options_transition(&saved,"MLAND2") &&
           !mercs_options_transition(&saved,"MTO") && mercs_options_transition(&saved,"MINTRO") &&
           mercs_options_transition(&saved,"MDEATH"));
    /* Actual reopened modal, right-click dismissal, retained edited settings. */
    while(SDL_PollEvent(&e)){}
    push_mouse(s,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_RIGHT,100,100);
    original_combat_variables(s);
    assert(!s->combat.active && s->options.dif[5]==2 && s->combat.resolution==2);
    /* Existing mission-menu subset commits must retain later shell fields. */
    mercs_options_set(&saved,MO_EFFECTS,65536);
    assert(!mercs_options_save_sound(&saved,cfg.install,error,sizeof error));
    assert(!mercs_options_load(&saved,cfg.install,cfg.cd,error,sizeof error));
    assert(!mercs_options_get(&saved,MO_SUBTITLES) && saved.snd[0x50]==0x6b && saved.dif[8]==0xa3);
    /* A short previous-port file is completed from defaults, never all-zeroed. */
    write_raw(cfg.install,"MW2SND.CFG",baseline.snd,16);
    assert(!mercs_options_load(&saved,cfg.install,cfg.cd,error,sizeof error));
    assert(saved.snd_size==84 && mercs_options_get(&saved,MO_PARTICLES)==1 && mercs_options_get(&saved,MO_SHADOWS)==1);
    /* Refuse oversized input instead of silently truncating its tail. */
    {uint8_t bytes[4097];memset(bytes,0x31,sizeof bytes);write_raw(cfg.install,"MW2SND.CFG",bytes,sizeof bytes);}
    assert(mercs_options_load(&saved,cfg.install,cfg.cd,error,sizeof error)==-1);
    assert(strstr(error,"MW2SND.CFG"));
    /* A failed reopen keeps the currently active shell settings and closes
     * its original error panel normally instead of partially applying defaults. */
    saved=s->options;
    while(SDL_PollEvent(&e)){}
    push_key(SDLK_RETURN);
    original_combat_variables(s);
    assert(!s->combat.active && !s->dialog.active);
    assert(saved.snd_size==s->options.snd_size && saved.dif_size==s->options.dif_size);
    assert(!memcmp(saved.snd,s->options.snd,saved.snd_size));
    assert(!memcmp(saved.dif,s->options.dif,saved.dif_size));
    assert(!mercs_options_save(&baseline,cfg.install,error,sizeof error));
    printf("PASS: original Combat Variables menu dispatch,13 controls,3 dragged sliders,preview audio,Esc/right-click persistence,reopen,resolution,unknown-byte preservation,short/oversized records; %d actual modal frames.\n",active_frames);
    check_graphics(s);
    check_gpu_settings(s);
    observed=NULL;mercs_shell_audio_destroy(s->shell_audio);sfx_destroy(s->audio);
    mercs_art_destroy(s->art);mercs_viewport_reset(&s->viewport);SDL_DestroyRenderer(s->renderer);
    SDL_DestroyWindow(s->window);shelldb_close(&s->database);mercs_campaign_free(&s->campaign);prj_close(s->archive);free(s);SDL_Quit();return 0;
}
