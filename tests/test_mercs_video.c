#include "sdl3_test.h"
/* Focused native SDL movie checks. Include the presenter so the same frame
 * routine can be captured before SDL_RenderPresent invalidates the backbuffer.
 * Usage: test_mercs_video ACTUAL_MINTRO.SMK EMPTY_OUTPUT_DIRECTORY
 * For a headless native run: SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy
 *                          LIBGL_ALWAYS_SOFTWARE=1
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static bool capture_present(SDL_Renderer *renderer);
#define SDL_RenderPresent capture_present
#include "../src/mercsvideo.c"
#undef SDL_RenderPresent

static int failures, presentations;
static const char *capture_path;
static SDL_Window *resize_on_present;
static void check(int ok, const char *label)
{
    if (!ok) { fprintf(stderr, "FAIL: %s (%s)\n", label, SDL_GetError()); failures++; }
}

static bool capture_present(SDL_Renderer *renderer)
{
    bool result;
    presentations++;
    if (capture_path) {
        int w, h;
        SDL_Surface *surface;
        SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
        surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
        check(surface != NULL, "capture surface");
        if (surface) {
            check(mercs_test_read_pixels(renderer, NULL, surface->format, surface->pixels, surface->pitch) == 0,
                  "native renderer readback");
            check(SDL_SaveBMP(surface, capture_path), "movie frame screenshot");
            SDL_DestroySurface(surface);
        }
    }
    result = SDL_RenderPresent(renderer);
    if (resize_on_present) {
        SDL_SetWindowSize(resize_on_present, 800, 700);
        resize_on_present = NULL;
    }
    return result;
}

static void put32(uint8_t *out, uint32_t n)
{
    int i;
    for (i = 0; i < 4; i++) out[i] = (uint8_t)(n >> (i * 8));
}

static void fixture(const char *path, uint32_t flags)
{
    /* Four 20ms frames, an all-red palette index zero, empty Huffman trees,
     * and valid four-byte packets. No bundled original assets are needed for
     * end-of-file, skip, quit, vertical-doubling or renderer-lifetime tests. */
    uint8_t bytes[140] = {0};
    FILE *f;
    int i;
    memcpy(bytes, "SMK2", 4);
    put32(bytes + 4, 4); put32(bytes + 8, 4);
    put32(bytes + 12, 4); put32(bytes + 16, 20); put32(bytes + 20, flags);
    for (i = 0; i < 4; i++) put32(bytes + 104 + i * 4, 4);
    bytes[120] = 1;
    bytes[124] = 1; bytes[125] = 63; bytes[126] = 10; bytes[127] = 5;
    f = fopen(path, "wb");
    check(f != NULL, "fixture opened");
    if (f) { check(fwrite(bytes, 1, sizeof bytes, f) == sizeof bytes, "fixture written"); fclose(f); }
}

static int same_state(const renderer_state *a, const renderer_state *b)
{
    return a->target == b->target && SDL_RectsEqual(&a->viewport, &b->viewport) &&
           SDL_RectsEqual(&a->clip, &b->clip) && a->clipping == b->clipping &&
           a->viewport_set == b->viewport_set && a->logical_mode == b->logical_mode &&
           a->logical_w == b->logical_w && a->logical_h == b->logical_h &&
           a->scale_x == b->scale_x && a->scale_y == b->scale_y && a->r == b->r && a->g == b->g &&
           a->b == b->b && a->a == b->a && a->blend == b->blend;
}

typedef struct { Uint32 window_id; int phase; } toggle_sequence;
static Uint32 SDLCALL toggle_then_quit(void *opaque, SDL_TimerID timer_id, Uint32 interval)
{
    toggle_sequence *sequence = opaque;
    SDL_Event event;
    (void)timer_id; (void)interval;
    SDL_zero(event);
    if (!sequence->phase++) {
        event.type = SDL_EVENT_KEY_DOWN; event.key.windowID = sequence->window_id;
        event.key.key = SDLK_RETURN; event.key.mod = SDL_KMOD_ALT;
        SDL_PushEvent(&event);
        return 40;
    }
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
    return 0;
}

static Uint32 SDLCALL skip_movie(void *opaque, SDL_TimerID timer_id, Uint32 interval)
{
    SDL_Event event;
    (void)timer_id; (void)interval; (void)opaque;
    SDL_zero(event); event.type = SDL_EVENT_KEY_DOWN; event.key.key = SDLK_SPACE;
    SDL_PushEvent(&event);
    return 0;
}

static void config_tests(const char *directory)
{
    char path[2048];
    portcfg c, loaded;
    FILE *f;
    int look, fit;
    snprintf(path, sizeof path, "%s/movie-settings.cfg", directory);
    SDL_setenv_unsafe("MERCS_CONFIG", path, 1);
    portcfg_defaults(&c);
    check(c.videos == PORTCFG_VID_ORIGINAL && c.video_fit == PORTCFG_FIT_43, "original and 4:3 defaults");
    for (look = 0; look < PORTCFG_VID_COUNT; look++) for (fit = 0; fit < PORTCFG_FIT_COUNT; fit++) {
        c.videos = look; c.video_fit = fit;
        check(portcfg_save(NULL, &c) == 0 && portcfg_load(NULL, &loaded) == 0 &&
              loaded.videos == look && loaded.video_fit == fit, "all movie option pairs survive config roundtrip");
    }
    c.videos = -10; c.video_fit = 999;
    check(portcfg_save(NULL, &c) == 0 && portcfg_load(NULL, &loaded) == 0 &&
          loaded.videos == PORTCFG_VID_ORIGINAL && loaded.video_fit == PORTCFG_FIT_43, "invalid in-memory config bounded");
    f = fopen(path, "w");
    if (f) { fputs("videos=999\nvideo_fit=-1\nvideos=crt_curved\nvideos=nonsense\n", f); fclose(f); }
    check(f && portcfg_load(NULL, &loaded) == 0 && loaded.videos == PORTCFG_VID_CRT_CURVED &&
          loaded.video_fit == PORTCFG_FIT_43, "unknown option values ignored defensively");
    unlink(path);
}

static void filter_tests(void)
{
    uint8_t pixels[16] = {0}, palette[256][3] = {{0}}, out[8 * 8 * 3], guard[8 * 8 * 3];
    vidfx *filter = vidfx_new();
    int i, look;
    palette[0][0] = 220; palette[0][1] = 160; palette[0][2] = 80;
    check(filter != NULL, "filter allocated");
    for (look = 0; look < VIDFX_COUNT; look++) {
        memset(out, 0, sizeof out);
        check(vidfx_render(filter, look, pixels, 4, 4, (const uint8_t (*)[3])palette, out, 8, 8, 24) == 0,
              "all filters render a valid tiny movie frame");
        for (i = 0; i < (int)sizeof out && !out[i]; i++) {}
        check(i != (int)sizeof out, "filter does not turn a valid frame entirely black");
    }
    memset(out, 0x5a, sizeof out); memcpy(guard, out, sizeof guard);
    check(vidfx_render(filter, VIDFX_CRT, pixels, 4, 4, (const uint8_t (*)[3])palette, out, 8, 8, 1) == -1 &&
          memcmp(out, guard, sizeof out) == 0, "bad destination pitch rejected without writing");
    vidfx_free(filter);
}

static void actual_frame_tests(SDL_Renderer *renderer, const char *path, const char *directory)
{
    movie m;
    char screenshot[2048];
    int frame, rate = 0, channels = 0;
    int16_t *pcm = NULL;
    size_t samples = 0;
    memset(&m, 0, sizeof m);
    m.renderer = renderer; m.fit = PORTCFG_FIT_43; m.decoder = smk_open_file(path);
    check(m.decoder != NULL, "actual supplied MINTRO decoded");
    if (!m.decoder) return;
    m.width = smk_width(m.decoder); m.height = m.raw_height = smk_height(m.decoder);
    check(m.width == 320 && m.height == 144 && smk_frames(m.decoder) == 1492, "actual intro dimensions and frame count");
    check(smk_audio(m.decoder, &pcm, &samples, &rate, &channels) == 0 &&
          rate == 22050 && channels == 2 && samples > 0, "actual signed-16 stereo intro soundtrack decoded");
    printf("MINTRO: %dx%d, %d frames, %.2f ms/frame, %d Hz, %d channels, %zu samples\n",
           m.width, m.height, smk_frames(m.decoder), smk_frame_ms(m.decoder), rate, channels, samples);
    free(pcm);
    for (frame = 0; frame <= 600; frame++) check(smk_next(m.decoder) == frame, "actual movie frame sequence");
    prepare_window_renderer(renderer);
    snprintf(screenshot, sizeof screenshot, "%s/mintro-original-frame600.bmp", directory);
    capture_path = screenshot;
    check(present_movie(&m) == 0, "actual movie original presentation");
    drop_texture(&m);
    m.look = PORTCFG_VID_CRT; m.filter = vidfx_new();
    snprintf(screenshot, sizeof screenshot, "%s/mintro-crt-frame600.bmp", directory);
    check(present_movie(&m) == 0, "actual movie CRT presentation");
    capture_path = NULL;
    drop_texture(&m); vidfx_free(m.filter); smk_close(m.decoder);
}

static void viewport_return_tests(SDL_Window *window, SDL_Renderer *renderer, const char *path)
{
    static const int sizes[][2] = {{2048, 1330}, {1280, 960}, {800, 700}, {960, 600}};
    mercs_viewport view;
    portcfg cfg;
    SDL_Event event;
    SDL_Rect clip = {9, 17, 57, 41}, viewport;
    int i, w, h, logical_w, logical_h, point_x, point_y;
    float scale_x, scale_y;
    mercs_viewport_init(&view, renderer);
    portcfg_defaults(&cfg);
    for (i = 0; i < 4; i++) {
        SDL_SetWindowSize(window, sizes[i][0], sizes[i][1]);
        while (SDL_PollEvent(&event)) {}
        SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetRenderScale(renderer, 2.0f, 2.0f);
        SDL_SetRenderClipRect(renderer, &clip);
        check(mercs_viewport_begin(&view) == 0, "shell canvas begins after stale logical/clip state");
        SDL_SetRenderDrawColor(renderer, 50, 90, 130, 255); SDL_RenderClear(renderer);
        check(mercs_viewport_end(&view) == 0, "shell canvas presented before movie");
        if (i == 1) resize_on_present = window;
        check(mercs_video_play(window, renderer, &cfg, path) == 0, "movie completes between authored canvas frames");
        SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, NULL);
        SDL_GetRenderScale(renderer, &scale_x, &scale_y);
        SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
        SDL_GetRenderViewport(renderer, &viewport);
        check(!SDL_GetRenderTarget(renderer) && !logical_w && !logical_h &&
              scale_x == 1.0f && scale_y == 1.0f && !viewport.x && !viewport.y &&
              viewport.w == w && viewport.h == h && !SDL_RenderClipEnabled(renderer),
              "movie return preserves raw window points and full drawable coordinates");
        check(mercs_viewport_begin(&view) == 0 && mercs_viewport_end(&view) == 0,
              "shell canvas refits after movie resize/return");
        SDL_GetWindowSize(window, &w, &h);
        check(mercs_viewport_window_point(&view, window, w / 2, h / 2, &point_x, &point_y) == 1 &&
              point_x >= 319 && point_x <= 320 && point_y >= 239 && point_y <= 240,
              "settings and modal center coordinates survive movie return");
        if (i == 2) {
            check(SDL_SetWindowFullscreen(window, true),
                  "enter fullscreen between movie and shell frames");
            while (SDL_PollEvent(&event)) {}
            check(mercs_viewport_begin(&view) == 0 && mercs_viewport_end(&view) == 0,
                  "shell canvas refits fullscreen drawable");
            check(SDL_SetWindowFullscreen(window, false), "return to windowed shell");
            while (SDL_PollEvent(&event)) {}
            check(mercs_viewport_begin(&view) == 0 && mercs_viewport_end(&view) == 0,
                  "shell canvas refits after leaving fullscreen");
        }
        mercs_viewport_reset(&view);
    }
}

int main(int argc, char **argv)
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *target;
    SDL_Rect viewport = {17, 19, 222, 137}, clip = {3, 5, 33, 21};
    renderer_state before, after;
    portcfg cfg;
    SDL_Event event;
    SDL_TimerID timer;
    SDL_AudioDeviceID shared_audio;
    SDL_AudioSpec shared_spec;
    toggle_sequence sequence;
    char path[2048];
    int before_presentations, output_w, output_h;
    if (argc != 3) { fprintf(stderr, "Usage: %s MINTRO.SMK OUTPUT_DIRECTORY\n", argv[0]); return 2; }
    SDL_SetMainReady();
    config_tests(argv[2]); filter_tests();
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 2; }
    window = SDL_CreateWindow("Mercs movie verification", 960, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    if (!renderer) { fprintf(stderr, "Renderer: %s\n", SDL_GetError()); return 2; }
    snprintf(path, sizeof path, "%s/movie-fixture.smk", argv[2]); fixture(path, 0);
    portcfg_defaults(&cfg);
    SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderScale(renderer, 1.25f, 0.75f); SDL_SetRenderViewport(renderer, &viewport);
    SDL_SetRenderClipRect(renderer, &clip); SDL_SetRenderDrawColor(renderer, 20, 40, 60, 80);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    save_renderer(renderer, &before); before_presentations = presentations;
    check(mercs_video_play(window, renderer, &cfg, path) == 0, "normal movie completion");
    check(presentations - before_presentations == 4, "all four timely frames presented");
    save_renderer(renderer, &after); check(same_state(&before, &after), "window renderer state restored exactly");
    check(!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO), "silent movies leave audio subsystem untouched");
    target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 256, 256);
    check(target != NULL && SDL_SetRenderTarget(renderer, target), "caller render target prepared");
    SDL_SetRenderLogicalPresentation(renderer, 128, 128, SDL_LOGICAL_PRESENTATION_LETTERBOX); SDL_SetRenderClipRect(renderer, &clip);
    save_renderer(renderer, &before);
    check(mercs_video_play(window, renderer, &cfg, path) == 0, "movie temporarily leaves caller target");
    save_renderer(renderer, &after); check(same_state(&before, &after), "caller texture target and state restored");
    SDL_SetRenderTarget(renderer, NULL); SDL_DestroyTexture(target);
    fixture(path, 2);
    check(mercs_video_play(window, renderer, &cfg, path) == 0, "vertically doubled movie rows handled safely");
    fixture(path, 0);
    SDL_zero(event); event.type = SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.windowID = SDL_GetWindowID(window);
    SDL_PushEvent(&event);
    check(mercs_video_play(window, renderer, &cfg, path) == 0, "mouse skip");
    sequence.window_id = SDL_GetWindowID(window); sequence.phase = 0;
    timer = SDL_AddTimer(5, toggle_then_quit, &sequence);
    check(timer != 0, "toggle input timer");
    check(mercs_video_play(window, renderer, &cfg, path) == 1, "Alt+Enter continues until later quit");
    SDL_RemoveTimer(timer); SDL_SetWindowFullscreen(window, false);
    prepare_window_renderer(renderer);
    resize_on_present = window;
    check(mercs_video_play(window, renderer, &cfg, path) == 0, "movie follows a live window resize");
    save_renderer(renderer, &after);
    check(SDL_GetRenderOutputSize(renderer, &output_w, &output_h), "physical output dimensions after resize");
    check(!after.viewport_set && after.viewport.x == 0 && after.viewport.y == 0 && after.viewport.w == output_w &&
          after.viewport.h == output_h, "full-window caller viewport follows resized drawable");
    viewport_return_tests(window, renderer, path);
    SDL_SetWindowSize(window, 960, 600);
    while (SDL_PollEvent(&event)) {}
    actual_frame_tests(renderer, argv[1], argv[2]);
    cfg.videos = PORTCFG_VID_CRT_CURVED;
    timer = SDL_AddTimer(400, skip_movie, NULL);
    check(mercs_video_play(window, renderer, &cfg, argv[1]) == 0, "actual intro plays and skips with private audio");
    SDL_RemoveTimer(timer);
    check(!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO), "movie releases only its own audio reference");
    check(SDL_InitSubSystem(SDL_INIT_AUDIO), "caller audio subsystem initialized");
    SDL_zero(shared_spec); shared_spec.freq = 22050; shared_spec.format = SDL_AUDIO_S16;
    shared_spec.channels = 2;
    shared_audio = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &shared_spec);
    check(shared_audio != 0, "caller audio device opened");
    /* SDL3's direct device API starts unpaused; the movie's separately owned
     * OpenAudioDeviceStream starts paused until its first video frame. */
    check(shared_audio && !SDL_AudioDevicePaused(shared_audio), "caller audio starts unpaused");
    timer = SDL_AddTimer(400, skip_movie, NULL);
    check(mercs_video_play(window, renderer, &cfg, argv[1]) == 0, "movie tolerates existing caller audio");
    SDL_RemoveTimer(timer);
    check((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0 &&
          shared_audio && SDL_GetAudioDeviceFormat(shared_audio, &shared_spec, NULL) &&
          !SDL_AudioDevicePaused(shared_audio),
          "caller audio device and subsystem survive movie cleanup");
    if (shared_audio) SDL_CloseAudioDevice(shared_audio);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    check(mercs_video_play(window, renderer, &cfg, "/not/a/mercs/movie.smk") == -1 && SDL_GetError()[0], "missing movie reports error");
    SDL_DestroyRenderer(renderer);
    renderer = SDL_CreateRenderer(window, NULL);
    check(renderer && mercs_video_play(window, renderer, &cfg, path) == 0, "movie works after renderer recreation");
    unlink(path);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    printf("test_mercs_video: %s (%d failures)\n", failures ? "FAILED" : "ok", failures);
    return failures != 0;
}
