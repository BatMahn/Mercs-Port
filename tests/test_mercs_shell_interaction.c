#include "sdl3_test.h"
/* Real shell dispatch/render integration. The runner must supply a PRIVATE
 * writable game folder whose CD/art files refer to the read-only originals.
 * Controlled UI states below are not an autonomous campaign/playthrough.
 * Usage: test_mercs_shell_interaction PRIVATE_GAME_DIRECTORY [CAPTURE_DIRECTORY]
 * Linux headless: SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy.
 * Capture only intercepts presentation; all application handlers are real. */
#include <SDL3/SDL.h>
static bool interaction_present(SDL_Renderer *renderer);
#define SDL_RenderPresent interaction_present
#include "../tools/mercsshell.c"
#undef SDL_RenderPresent
#include <assert.h>
#include <inttypes.h>
#include "shp.h"

static shell *observed_shell;
static const char *capture_directory, *capture_name;
static uint64_t last_hash;
static int hash_next, captures, modal_frames, menu_frames, dispatches, gates;
static int backdrop_probe, backdrop_questions, backdrop_returns, save_art_cases, reveal_cases;
static int debrief_wait_probe, debrief_old_mode, debrief_wait_frames;
static Uint32 *menu_pixels;
static mercs_art_snapshot *menu_snapshot;
static SDL_Rect question_bounds;

static void read_canvas(shell *s, Uint32 *pixels)
{
    SDL_Texture *previous = SDL_GetRenderTarget(s->renderer);
    assert(s->viewport.canvas && SDL_SetRenderTarget(s->renderer, s->viewport.canvas));
    assert(!mercs_test_read_pixels(s->renderer, NULL, SDL_PIXELFORMAT_ARGB8888, pixels, SW * 4));
    assert(SDL_SetRenderTarget(s->renderer, previous));
}

static bool interaction_present(SDL_Renderer *renderer)
{
    if (observed_shell) {
        if (observed_shell->dialog.active) modal_frames++;
        if (observed_shell->game_menu.active) menu_frames++;
        if (debrief_wait_probe) {
            assert(observed_shell->page == PAGE_DEBRIEF &&
                   observed_shell->debrief_view.content_mode == debrief_old_mode);
            debrief_wait_frames++;
        }
        if (backdrop_probe) {
            shell *s = observed_shell;
            if (s->game_menu.active && s->game_menu.selected == MERCS_GAME_MENU_QUIT) {
                if (!menu_snapshot) {
                    read_canvas(s, menu_pixels); menu_snapshot = s->modal_background; assert(menu_snapshot);
                } else if (backdrop_questions) {
                    Uint32 *pixels = malloc((size_t)SW * SH * 4); assert(pixels);
                    read_canvas(s, pixels);
                    assert(menu_snapshot == s->modal_background && !memcmp(pixels, menu_pixels, (size_t)SW * SH * 4));
                    free(pixels); backdrop_returns++;
                }
            } else if (s->dialog.active) {
                Uint32 *pixels = malloc((size_t)SW * SH * 4);
                int x, y;
                assert(pixels && menu_snapshot && s->menu_backdrop && s->modal_background == menu_snapshot);
                read_canvas(s, pixels);
                for (y = 0; y < SH; y++) for (x = 0; x < SW; x++)
                    if (x < question_bounds.x || x >= question_bounds.x + question_bounds.w ||
                        y < question_bounds.y || y >= question_bounds.y + question_bounds.h)
                        assert(pixels[y * SW + x] == menu_pixels[y * SW + x]);
                free(pixels);
                if (!backdrop_questions) capture_name = "interaction-menu-owned-question";
                backdrop_questions++;
            }
        }
    }
    if (capture_name || hash_next) {
        int w, h, x, y;
        SDL_Surface *surface;
        uint64_t hash = UINT64_C(14695981039346656037);
        assert(SDL_GetCurrentRenderOutputSize(renderer, &w, &h) && w > 0 && h > 0);
        surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888); assert(surface);
        assert(!mercs_test_read_pixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch));
        for (y = 0; y < h; y++) for (x = 0; x < w * 4; x++)
            hash = (hash ^ ((const Uint8 *)surface->pixels)[(size_t)y * (size_t)surface->pitch + (size_t)x]) * UINT64_C(1099511628211);
        last_hash = hash;
        if (capture_name) {
            printf("CAPTURE %s %dx%d fnv1a64=%016" PRIx64 "\n", capture_name, w, h, hash);
            if (capture_directory) {
                char path[4096];
                int length = snprintf(path, sizeof path, "%s/%s.bmp", capture_directory, capture_name);
                assert(length > 0 && (size_t)length < sizeof path && SDL_SaveBMP(surface, path));
            }
            captures++;
        }
        SDL_DestroySurface(surface); capture_name = NULL; hash_next = 0;
    }
    return SDL_RenderPresent(renderer);
}

static void healthy(shell *s)
{
    if (!s->running || s->status[0]) {
        fprintf(stderr, "interaction state: running=%d status=%s SDL=%s\n", s->running, s->status, SDL_GetError());
        abort();
    }
}

static void frame(shell *s, const char *name)
{
    capture_name = name;
    draw(s); healthy(s); assert(!capture_name);
}

static void discard_events(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
}

static SDL_Event key_event(shell *s, SDL_Keycode keycode, SDL_Keymod modifiers, int repeat)
{
    SDL_Event event;
    SDL_zero(event); event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(s->window);
    event.key.key = keycode; event.key.mod = modifiers; event.key.repeat = repeat != 0; event.key.down = true;
    return event;
}

static void send_event(shell *s, SDL_Event event)
{ dispatch_event(s, &event); dispatches++; healthy(s); }

/* Inverse geometry used only to place test input safely inside an authored
 * control. The product always receives raw window coordinates. */
static void window_point(shell *s, int authored_x, int authored_y, int *raw_x, int *raw_y)
{
    SDL_Rect destination;
    int ww, wh, ow, oh;
    SDL_GetWindowSize(s->window, &ww, &wh);
    assert(SDL_GetCurrentRenderOutputSize(s->renderer, &ow, &oh));
    assert(ww > 0 && wh > 0 && mercs_viewport_fit(ow, oh, &destination) == 1);
    *raw_x = (int)lround(((double)destination.x + ((double)authored_x + 0.5) * destination.w / SW) * ww / ow);
    *raw_y = (int)lround(((double)destination.y + ((double)authored_y + 0.5) * destination.h / SH) * wh / oh);
}

static SDL_Event raw_button(shell *s, Uint32 type, int button_number, int raw_x, int raw_y)
{
    SDL_Event event;
    SDL_zero(event); event.type = type; event.button.windowID = SDL_GetWindowID(s->window);
    event.button.button = (Uint8)button_number; event.button.clicks = 1;
    event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.x = (float)raw_x; event.button.y = (float)raw_y;
    return event;
}

static SDL_Event authored_button(shell *s, Uint32 type, int button_number, int x, int y)
{
    int raw_x, raw_y;
    window_point(s, x, y, &raw_x, &raw_y);
    return raw_button(s, type, button_number, raw_x, raw_y);
}

static void mouse_click(shell *s, int x, int y)
{
    send_event(s, authored_button(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, x, y));
    send_event(s, authored_button(s, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT, x, y));
}

static void text_event(shell *s, const char *value)
{
    SDL_Event event;
    SDL_zero(event); event.type = SDL_EVENT_TEXT_INPUT; event.text.windowID = SDL_GetWindowID(s->window);
    event.text.text = value; /* Synchronous dispatch retains the caller's text. */
    send_event(s, event);
}

static void enqueue(SDL_Event event)
{ assert(SDL_PushEvent(&event) == 1); }

static Uint32 SDLCALL stop_stalled_modal(void *unused, SDL_TimerID timer_id, Uint32 interval)
{
    SDL_Event event;
    (void)timer_id; (void)interval; (void)unused;
    fputs("FAIL: interaction modal exceeded its 30-second watchdog\n", stderr);
    SDL_zero(event); event.type = SDL_EVENT_QUIT; SDL_PushEvent(&event);
    return 0;
}

static void open_test_dialog(shell *s)
{
    assert(!s->dialog.active && !s->game_menu.active && !s->modal_background);
    assert(!mercs_dialog_open(&s->dialog, "Sell Out?#Yes|No", 1));
    SDL_StopTextInput(s->window);
    draw(s); healthy(s); assert(s->modal_background);
}

static void close_test_dialog(shell *s)
{
    assert(!s->dialog.active);
    close_modal_background(s); assert(!s->modal_background);
}

static void resize_and_point_checks(shell *s)
{
    static const int sizes[][2] = {{2048,1330},{1280,960},{800,700},{900,1200},{2048,1330}};
    int i;
    for (i = 0; i < COUNT(sizes); i++) {
        SDL_Event event;
        SDL_Rect destination;
        int x, y, ww, wh, ow, oh, logical_w, logical_h;
        float scale_x, scale_y;
        char name[80];
        SDL_SetWindowSize(s->window, sizes[i][0], sizes[i][1]);
        while (SDL_PollEvent(&event)) send_event(s, event);
        /* A prior logical renderer or movie must not move this page. */
        SDL_SetRenderLogicalPresentation(s->renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetRenderScale(s->renderer, 2.0f, 1.5f);
        snprintf(name, sizeof name, "interaction-registration-%d-%dx%d", i, sizes[i][0], sizes[i][1]);
        frame(s, name);
        SDL_GetRenderLogicalPresentation(s->renderer, &logical_w, &logical_h, NULL);
        SDL_GetRenderScale(s->renderer, &scale_x, &scale_y);
        assert(!logical_w && !logical_h && scale_x == 1.0f && scale_y == 1.0f);
        assert(!SDL_GetRenderTarget(s->renderer));
        s->focus = 1; mouse_click(s, 445, 320); assert(!s->focus && s->page == PAGE_REGISTER);
        mouse_click(s, 445, 357); assert(s->focus == 1 && s->page == PAGE_REGISTER);
        SDL_zero(event); event.type = SDL_EVENT_MOUSE_MOTION; event.motion.windowID = SDL_GetWindowID(s->window);
        window_point(s, 445, 320, &x, &y);
        event.motion.x = (float)x; event.motion.y = (float)y;
        send_event(s, event);
        assert(abs(s->mouse_x - 445) <= 1 && abs(s->mouse_y - 320) <= 1);
        SDL_GetWindowSize(s->window, &ww, &wh);
        assert(SDL_GetCurrentRenderOutputSize(s->renderer, &ow, &oh));
        assert(mercs_viewport_fit(ow, oh, &destination) == 1);
        if (destination.x || destination.y) {
            x = destination.x ? 0 : ww / 2; y = destination.y ? 0 : wh / 2;
            send_event(s, raw_button(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, x, y));
            send_event(s, raw_button(s, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT, x, y));
            assert(s->mouse_x == -1 && s->mouse_y == -1 && s->focus == 1);
        }
    }
    /* A physical Retina drawable is unavailable on some runners. Its exact
     * 2x point/pixel transform is checked separately, not claimed as hardware. */
    {
        int x, y;
        assert(mercs_viewport_map_point(1024,665,2048,1330,512,333,&x,&y) == 1 && x == 320 && y == 240);
        assert(mercs_viewport_map_point(1024,665,2048,1330,68,333,&x,&y) == 0 && x == -1 && y == -1);
        assert(mercs_viewport_map_point(1024,665,2048,1330,955,333,&x,&y) == 0);
    }
    {
        SDL_Event event;
        SDL_zero(event); event.type = SDL_EVENT_RENDER_TARGETS_RESET;
        send_event(s, event); assert(!s->viewport.canvas);
        frame(s, "interaction-registration-after-target-reset"); assert(s->viewport.canvas);
    }
}

static void dialog_checks(shell *s)
{
    int choice, use_escape, frames_before;
    uint64_t frozen;
    char original_pilot[sizeof s->pilot], original_unit[sizeof s->unit];
    strcpy(original_pilot, s->pilot); strcpy(original_unit, s->unit);
    s->focus = 0; s->select_all = 0; discard_events();
    for (choice = 0; choice < 2; choice++) for (use_escape = 0; use_escape < 2; use_escape++) {
        enqueue(key_event(s, use_escape ? SDLK_ESCAPE : SDLK_RETURN, SDL_KMOD_NONE, 0));
        frames_before = modal_frames;
        assert(game_question(s, "Save Game?", choice) == (choice == 0));
        healthy(s); assert(modal_frames > frames_before && !s->dialog.active && !s->modal_background);
        assert(s->page == PAGE_REGISTER && !s->career.registered && SDL_TextInputActive(s->window));
    }
    enqueue(key_event(s, SDLK_N, SDL_KMOD_NONE, 0)); assert(!game_question(s, "Save Game?", 0));
    enqueue(key_event(s, SDLK_Y, SDL_KMOD_SHIFT, 0)); assert(game_question(s, "Sell Out?", 1));
    open_test_dialog(s);
    s->mouse_x = s->mouse_y = -1;
    frame(s, "interaction-original-dialog-wide"); frozen = last_hash;
    SDL_Delay(35); hash_next = 1; draw(s); healthy(s);
    assert(last_hash == frozen); /* The original animated page is frozen. */
    text_event(s, "LEAK");
    send_event(s, key_event(s, SDLK_A, SDL_KMOD_CTRL, 0));
    send_event(s, key_event(s, SDLK_TAB, SDL_KMOD_NONE, 0));
    assert(s->focus == 0 && !s->select_all && !strcmp(s->pilot, original_pilot) && !strcmp(s->unit, original_unit));
    mouse_click(s, 445, 357); /* underlying unit-name input, outside the modal */
    assert(s->dialog.active && s->focus == 0 && s->page == PAGE_REGISTER && !s->career.registered);
    send_event(s, authored_button(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, 290, 276));
    assert(s->dialog.active && s->dialog.held && s->dialog.pressed == 0);
    frame(s, "interaction-original-dialog-pressed");
    send_event(s, authored_button(s, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT, 351, 276));
    assert(s->dialog.active && !s->dialog.held); /* Different button cannot accept. */
    send_event(s, authored_button(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, 290, 276));
    {
        SDL_Event event;
        SDL_zero(event); event.type = SDL_EVENT_WINDOW_FOCUS_LOST; event.window.windowID = SDL_GetWindowID(s->window);
        send_event(s, event);
    }
    send_event(s, authored_button(s, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT, 290, 276));
    assert(s->dialog.active && !s->dialog.held); /* A focus-loss release is stale. */
    send_event(s, authored_button(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, 290, 276));
    send_event(s, raw_button(s, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT, 0, 200));
    assert(s->dialog.active && !s->dialog.held && s->mouse_x == -1); /* Release in a black bar. */
    mouse_click(s, 290, 276); assert(!s->dialog.active && s->dialog.answer == 0);
    assert(s->page == PAGE_REGISTER && !s->career.registered && !strcmp(s->pilot, original_pilot));
    close_test_dialog(s); assert(SDL_TextInputActive(s->window));
}

static void settings_geometry_check(shell *s)
{
    portcfg saved;
    int previous = s->cfg->videos;
    settings(s); assert(s->page == PAGE_SETTINGS && s->settings_return == PAGE_REGISTER);
    frame(s, "interaction-video-settings-wide");
    mouse_click(s, 440, 225);
    assert(s->cfg->videos == (previous + 1) % PORTCFG_VID_COUNT);
    assert(!portcfg_load(NULL, &saved) && saved.videos == s->cfg->videos);
    mouse_click(s, 510, 451);
    assert(s->page == PAGE_REGISTER);
}

static void menu_checks(shell *s)
{
    int before;
    char pilot[sizeof s->pilot], unit[sizeof s->unit];
    strcpy(pilot, s->pilot); strcpy(unit, s->unit); s->focus = 0;
    discard_events(); before = menu_frames;
    enqueue(key_event(s, SDLK_ESCAPE, SDL_KMOD_NONE, 1)); /* repeat cannot dismiss */
    enqueue(key_event(s, SDLK_ESCAPE, SDL_KMOD_NONE, 0));
    capture_name = "interaction-original-escape-menu";
    send_event(s, key_event(s, SDLK_ESCAPE, SDL_KMOD_NONE, 0));
    assert(menu_frames >= before + 2 && !s->game_menu.active && s->page == PAGE_REGISTER);
    assert(!s->modal_background && SDL_TextInputActive(s->window));
    discard_events(); before = menu_frames;
    enqueue(authored_button(s, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_RIGHT, 445, 320));
    enqueue(authored_button(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_RIGHT, 445, 320));
    send_event(s, authored_button(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_RIGHT, 445, 320));
    assert(menu_frames > before && !s->game_menu.active && s->page == PAGE_REGISTER && s->focus == 0);
    discard_events();
    {
        SDL_Event event;
        SDL_zero(event); event.type = SDL_EVENT_TEXT_INPUT; event.text.windowID = SDL_GetWindowID(s->window);
        event.text.text = "LEAK"; enqueue(event); /* Static storage survives the queue. */
    }
    enqueue(authored_button(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, 445, 357));
    send_event(s, key_event(s, SDLK_ESCAPE, SDL_KMOD_NONE, 0));
    assert(!s->game_menu.active && s->focus == 0 && s->page == PAGE_REGISTER);
    assert(!strcmp(pilot, s->pilot) && !strcmp(unit, s->unit) && !s->career.registered);
}

static void save_focus_checks(shell *s)
{
    char title[64];
    set_page(s, PAGE_SAVE); s->save_return = PAGE_HALL; s->save_slot = 0;
    strcpy(s->save_titles[0], "UNCHANGED DESCRIPTION");
    edit_save_description(s); assert(s->save_editing);
    open_test_dialog(s);
    text_event(s, "LEAK");
    send_event(s, key_event(s, SDLK_BACKSPACE, SDL_KMOD_NONE, 0));
    send_event(s, key_event(s, SDLK_A, SDL_KMOD_CTRL, 0));
    mouse_click(s, 560, 408); /* underlying Save button; modal must consume it */
    assert(s->dialog.active && s->save_editing && !s->select_all && s->page == PAGE_SAVE);
    assert(!strcmp(s->save_titles[0], "UNCHANGED DESCRIPTION"));
    assert(mercs_store_title(s->save_dir, 0, title, sizeof title) == 1);
    send_event(s, key_event(s, SDLK_ESCAPE, SDL_KMOD_NONE, 0));
    assert(!s->dialog.active && s->dialog.answer == 1); close_test_dialog(s);
    assert(SDL_TextInputActive(s->window));
    /* Opening the original menu commits only the current edit buffer. Its
     * outside click must not select the save-list row beneath that click. */
    discard_events();
    enqueue(authored_button(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, 100, 132));
    send_event(s, authored_button(s, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_RIGHT, 100, 100));
    assert(!s->game_menu.active && !s->save_editing && s->save_slot == 0 && s->page == PAGE_SAVE);
    assert(!strcmp(s->save_titles[0], "UNCHANGED DESCRIPTION") && !s->status[0]);
    assert(mercs_store_title(s->save_dir, 0, title, sizeof title) == 1);
    frame(s, "interaction-original-save-after-modal");
}

static void menu_backdrop_checks(shell *s)
{
    unsigned char *record = NULL;
    shp_frame shape;
    long length;
    int i;
    set_page(s, PAGE_REGISTER); s->mouse_x = s->mouse_y = -1;
    discard_events();
    length = shelldb_entry(&s->database, 0, &record);
    assert(length > 0 && !shp_decode(record, (size_t)length, 2, &shape));
    question_bounds = (SDL_Rect){172 + shape.left, 165 + shape.top, shape.w, shape.h};
    shp_frame_free(&shape); free(record);
    menu_pixels = malloc((size_t)SW * SH * 4); assert(menu_pixels);
    menu_snapshot = NULL; backdrop_probe = 1;
    for (i = 0; i < MERCS_GAME_MENU_QUIT; i++) enqueue(key_event(s, SDLK_DOWN, SDL_KMOD_NONE, 0));
    enqueue(key_event(s, SDLK_RETURN, SDL_KMOD_NONE, 0)); /* Menu-owned Sell Out? */
    enqueue(key_event(s, SDLK_ESCAPE, SDL_KMOD_NONE, 0)); /* Original default No. */
    enqueue(key_event(s, SDLK_ESCAPE, SDL_KMOD_NONE, 0)); /* Dismiss restored menu. */
    send_event(s, key_event(s, SDLK_ESCAPE, SDL_KMOD_NONE, 0));
    backdrop_probe = 0;
    assert(backdrop_questions && backdrop_returns && !s->menu_backdrop && !s->modal_background);
    assert(!s->game_menu.active && !s->dialog.active && s->page == PAGE_REGISTER);
    free(menu_pixels); menu_pixels = NULL; menu_snapshot = NULL;
}

static void save_reference(shell *s, const char *button_name, int x, int y, Uint32 *out)
{
    assert(!mercs_viewport_begin(&s->viewport) && !mercs_art_background(s->art, 19));
    if (button_name) assert(!mercs_art_button(s->art, button_name, x, y, 0));
    assert(!mercs_art_present(s->art)); read_canvas(s, out);
    assert(!mercs_viewport_end(&s->viewport));
}

static void same_region(const Uint32 *actual, const Uint32 *expected, SDL_Rect area)
{
    int y;
    for (y = area.y; y < area.y + area.h; y++)
        assert(!memcmp(actual + y * SW + area.x, expected + y * SW + area.x, (size_t)area.w * 4));
}

static void save_art_checks(shell *s)
{
    static const struct { int registered, origin, phase, intent, load, save; } cases[] = {
        {0,PAGE_HALL,MERCS_CAMPAIGN_READY,0,1,0},
        {1,PAGE_MENU,MERCS_CAMPAIGN_READY,0,1,0},
        {1,PAGE_HALL,MERCS_CAMPAIGN_ENDED,0,1,0},
        {1,PAGE_HALL,MERCS_CAMPAIGN_READY,1,0,1},
        {1,PAGE_HALL,MERCS_CAMPAIGN_READY,0,1,1}
    };
    SDL_Rect load_area = {392,387,116,43}, save_area = {508,387,111,43};
    Uint32 *blank = malloc((size_t)SW * SH * 4), *load = malloc((size_t)SW * SH * 4);
    Uint32 *save = malloc((size_t)SW * SH * 4), *actual = malloc((size_t)SW * SH * 4);
    int i;
    assert(blank && load && save && actual);
    save_reference(s, NULL, 0, 0, blank);
    save_reference(s, "saveload", 402, 397, load);
    save_reference(s, "savesave", 529, 397, save);
    assert(memcmp(blank, load, (size_t)SW * SH * 4) && memcmp(blank, save, (size_t)SW * SH * 4));
    for (i = 0; i < COUNT(cases); i++) {
        set_page(s, PAGE_SAVE); s->save_return = cases[i].origin;
        s->career.registered = cases[i].registered; s->campaign.phase = cases[i].phase;
        s->after_save = cases[i].intent; s->mouse_x = s->mouse_y = -1;
        draw(s); healthy(s); read_canvas(s, actual);
        same_region(actual, cases[i].load ? load : blank, load_area);
        same_region(actual, cases[i].save ? save : blank, save_area);
        if (!cases[i].save) mouse_click(s, 560, 408);
        if (!cases[i].load) mouse_click(s, 445, 408);
        assert(s->page == PAGE_SAVE && !s->dialog.active && !s->status[0]);
        save_art_cases++;
    }
    s->after_save = 0;
    free(blank); free(load); free(save); free(actual);
}

static void finish_reveal(shell *s, const char *label)
{
    int polls = 0, total = s->reveal_total;
    assert(total > 0 && s->reveal_active && mercs_shell_audio_resource_playing(s->shell_audio, 49));
    while (s->reveal_active) {
        int before = s->reveal_rows;
        poll_shell(s);
        assert(s->reveal_rows == before || s->reveal_rows == before + 1);
        assert(s->reveal_rows <= total && ++polls <= total + 1);
    }
    assert(s->reveal_rows == total && !mercs_shell_audio_resource_playing(s->shell_audio, 49));
    printf("REVEAL %s rows=%d polls=%d completed-and-stopped49\n", label, total, polls);
    reveal_cases++;
}

static void reveal_checks(shell *s)
{
    mercs_campaign next = {0};
    mercs_shell_state *career = calloc(1, sizeof *career);
    uint8_t *header;
    int i, bet = -1;
    assert(career && !mercs_campaign_new(s->archive, 1, "INTERACTION", "TEST COMPANY",
                                       &next, career, s->status, sizeof s->status));
    install_career(s, &next, career); free(career);
    s->letter_heard = 1; set_page(s, PAGE_NEWS);
    assert(!s->reveal_rows && s->reveal_total > 2);
    frame(s, "interaction-news-before-reveal");
    poll_shell(s); assert(s->reveal_rows == 1 && s->reveal_active);
    frame(s, "interaction-news-first-row");
    /* Modal hosts leave the original line counter unchanged. */
    open_test_dialog(s);
    poll_shell(s); poll_shell(s); assert(s->reveal_rows == 1);
    send_event(s, key_event(s, SDLK_ESCAPE, SDL_KMOD_NONE, 0)); close_test_dialog(s);
    finish_reveal(s, "news");
    set_page(s, PAGE_CONTRACTS); assert(s->career.mission_count > 1 && !s->reveal_rows);
    poll_shell(s); poll_shell(s); assert(s->reveal_rows == 2);
    send_event(s, key_event(s, SDLK_RIGHT, SDL_KMOD_NONE, 0));
    assert(s->selected == 1 && !s->reveal_rows && s->reveal_active);
    finish_reveal(s, "next-contract-key");
    mouse_click(s, 80, 400); assert(s->selected == 0 && !s->reveal_rows);
    finish_reveal(s, "previous-contract-click");
    header = mercs_save_header_edit(s->campaign.document); assert(header && header[0x239d]);
    header[0x239d] = 0; set_page(s, PAGE_CONTRACTS);
    assert(!mercs_shell_audio_resource_playing(s->shell_audio, 52));
    send_event(s, key_event(s, SDLK_RIGHT, SDL_KMOD_NONE, 0)); mouse_click(s, 180, 400);
    assert(s->selected == 0 && !mercs_shell_audio_resource_playing(s->shell_audio, 52));
    header[0x239d] = 1;
    for (i = 0; i < s->career.mission_count; i++) if (!strcmp(s->career.missions[i].campaign, "BET1CAM1")) bet = i;
    assert(bet >= 0); s->accepted = s->selected = bet;
    set_page(s, PAGE_BRIEFING); assert(!s->reveal_rows && !s->brief_tab);
    finish_reveal(s, "briefing");
    mouse_click(s, 185, 442); assert(s->brief_tab && !s->reveal_rows && s->reveal_active);
    finish_reveal(s, "briefing-tab");
    frame(s, "interaction-briefing-tab-revealed");
    /* Controlled report fields exercise the original DBFS/analysis UI only;
     * no simulator result, salvage roll or settlement is claimed here. */
    s->campaign.phase = MERCS_CAMPAIGN_DEBRIEF;
    memset(&s->campaign.terms, 0, sizeof s->campaign.terms);
    memset(&s->campaign.report, 0, sizeof s->campaign.report);
    s->campaign.terms.mode = 1; s->campaign.terms.campaign_type = 1;
    s->campaign.terms.funds = s->career.funds; s->campaign.terms.owned_mech_count = (uint32_t)s->career.owned_count;
    s->campaign.report.success = 1; s->campaign.report.balance = s->career.funds;
    s->career.last_msn.outcome = 2;
    set_page(s, PAGE_DEBRIEF); healthy(s);
    assert(s->debrief_view.ready && !s->debrief_view.content_mode && !s->reveal_rows);
    finish_reveal(s, "debrief-report");
    debrief_old_mode = 0; debrief_wait_probe = 1; debrief_wait_frames = 0;
    mouse_click(s, 185, 442);
    debrief_wait_probe = 0;
    assert(debrief_wait_frames > 0 && s->debrief_view.content_mode == 1 && !s->reveal_rows);
    finish_reveal(s, "debrief-analysis");
    frame(s, "interaction-debrief-analysis-revealed");
    set_page(s, PAGE_HALL); assert(!s->reveal_active && !mercs_shell_audio_resource_playing(s->shell_audio, 49));
}

static void hall_gate(shell *s, int registered, int needs_contract, int owned, int hotspot, int resource)
{
    uint8_t before[MERCS_SAVE_HEADER_BYTES], *header = mercs_save_header_edit(s->campaign.document);
    const mercs_ui_hotspot *h = &ui_mainhall_hotspots[hotspot];
    assert(header);
    s->career.registered = registered; s->career.owned_count = owned; s->mode = 1;
    header[1] = (uint8_t)registered; header[0x239d] = (uint8_t)needs_contract;
    set_page(s, PAGE_HALL); memcpy(before, header, sizeof before);
    mouse_click(s, (h->x1+h->x2)/2, (h->y1+h->y2)/2);
    assert(s->page == PAGE_HALL && s->transition == -1 && !s->completed_transition && s->accepted == -1);
    assert(!s->dialog.active && !s->game_menu.active && !memcmp(before, header, sizeof before));
    assert(mercs_shell_audio_resource_playing(s->shell_audio, resource));
    assert(resource == 69 || !mercs_shell_audio_resource_playing(s->shell_audio, 69));
    assert(resource == 70 || !mercs_shell_audio_resource_playing(s->shell_audio, 70));
    assert(resource == 71 || !mercs_shell_audio_resource_playing(s->shell_audio, 71));
    printf("GATE original_resource=%d registered=%d needs_contract=%d owned=%d hotspot=%d page=hall unchanged\n",
           resource, registered, needs_contract, owned, hotspot);
    gates++;
}

int main(int argc, char **argv)
{
    shell *s = calloc(1, sizeof *s);
    portcfg cfg;
    char path[PORTCFG_PATH], error[256], models[PORTCFG_PATH], textures[PORTCFG_PATH];
    char kind[PORTCFG_PATH], sky[PORTCFG_PATH], fog[PORTCFG_PATH];
    SDL_TimerID watchdog;
    int i;
    if ((argc != 2 && argc != 3) || !s) {
        fprintf(stderr, "Usage: %s PRIVATE_GAME_DIRECTORY [CAPTURE_DIRECTORY]\n", argv[0]); return 2;
    }
    capture_directory = argc == 3 ? argv[2] : NULL;
    snprintf(path, sizeof path, "%s/interaction-port.cfg", argv[1]); assert(!SDL_setenv_unsafe("MERCS_CONFIG", path, 1));
    portcfg_defaults(&cfg); assert(!portcfg_resolve(&cfg, NULL, argv[1])); cfg.mode = 1;
    s->cfg = &cfg; s->exe = "unused-interaction-child"; s->running = 1; s->mode = 1;
    s->edition = PORTCFG_ED_DOS; s->accepted = -1; s->transition = -1; s->mouse_x = s->mouse_y = -1;
    assert(!portcfg_overlay_dir(&cfg, "retail", path, sizeof path, 1));
    assert(!mercs_store_directory(path, s->save_dir, sizeof s->save_dir));
    assert(!SDL_setenv_unsafe("MERCS_INSTALL_DIR", path, 1) && !SDL_setenv_unsafe("MERCS_CD_DIR", cfg.cd, 1));
    dp_clear_roots(); dp_add_roots_from_env(); assert(!dp_set_write_root(path));
    assert(!dp_resolve("DATABASE.MW2", path, sizeof path) && !shelldb_open(path, &s->database));
    assert(!portcfg_edition(&cfg, s->edition, models, textures, kind, sky, fog));
    s->archive = prj_open(models, error, sizeof error); assert(s->archive);
    s->campaign.document = mercs_save_create(); assert(s->campaign.document);
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));
    watchdog = SDL_AddTimer(30000, stop_stalled_modal, NULL); assert(watchdog);
    s->window = SDL_CreateWindow("Mercs interaction verification", 2048, 1330, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    assert(s->window);
    s->renderer = SDL_CreateRenderer(s->window, "software"); assert(s->renderer);
    mercs_viewport_init(&s->viewport, s->renderer);
    s->art = mercs_art_create(&s->database, s->renderer); assert(s->art);
    s->audio = sfx_create(NULL); assert(s->audio); mercs_art_set_audio(s->art, s->audio);
    s->shell_audio = mercs_shell_audio_create(s->audio, &s->database); assert(s->shell_audio);
    { long n = shelldb_entry(&s->database, 10, &s->fallback_font); if (n > 0) s->fallback_size = (size_t)n; }
    observed_shell = s;
    set_page(s, PAGE_REGISTER); strcpy(s->pilot, "INTERACTION"); strcpy(s->unit, "TEST COMPANY");
    resize_and_point_checks(s);
    settings_geometry_check(s);
    dialog_checks(s);
    menu_checks(s);
    save_focus_checks(s);
    hall_gate(s, 0, 1, 1, 0, 69);
    hall_gate(s, 0, 1, 1, 2, 69);
    hall_gate(s, 1, 1, 1, 0, 70);
    hall_gate(s, 1, 0, 0, 0, 71);
    frame(s, "interaction-original-hall-gate");
    menu_backdrop_checks(s);
    save_art_checks(s);
    reveal_checks(s);
    healthy(s);
    SDL_RemoveTimer(watchdog);
    observed_shell = NULL;
    mercs_debrief_view_free(&s->debrief_view); mercs_text_free(&s->body);
    mercs_campaign_free(&s->campaign); mercs_art_snapshot_free(s->modal_background);
    mercs_viewport_reset(&s->viewport); mercs_art_destroy(s->art);
    mercs_shell_audio_destroy(s->shell_audio); sfx_destroy(s->audio);
    for (i = 0; i < TEXT_CACHE; i++) SDL_DestroyTexture(s->text[i].texture);
    for (i = 0; i < 120; i++) SDL_DestroyTexture(s->backgrounds[i]);
    free(s->fallback_font); prj_close(s->archive); shelldb_close(&s->database);
    SDL_DestroyRenderer(s->renderer); SDL_DestroyWindow(s->window); dp_clear_roots(); SDL_Quit(); free(s);
    printf("PASS: original shell interaction (%d raw dispatches, %d modal frames, %d menu frames, %d original hall gates, %d frame hashes); resize/DPI geometry, modal focus, original defaults, press/release and no click-through\n",
           dispatches, modal_frames, menu_frames, gates, captures);
    printf("PASS: nested-menu backdrop (%d question frames, %d restored menu frames), %d Save/Load artwork gates, %d original text reveal sequences\n",
           backdrop_questions, backdrop_returns, save_art_cases, reveal_cases);
    return 0;
}
