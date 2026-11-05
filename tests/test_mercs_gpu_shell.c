/* Asset-free check of the production shell renderer and its failure message.
 * SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./test_mercs_gpu_shell
 * Linux/Windows also check the real software fallback with Vulkan unavailable.
 * The final all-renderers failure is injected; it does not claim a GPU test. */
#include "sdl3_test.h"
#include <assert.h>

static SDL_Renderer *checked_create_renderer(SDL_Window *window, const char *name);
static int captured_gui_error(const char *message);
#define SDL_CreateRenderer checked_create_renderer
#define mercs_gui_error captured_gui_error
#include "../tools/mercsshell.c"
#undef SDL_CreateRenderer
#undef mercs_gui_error

static int force_failure, error_count;
static char error_text[1024];

static SDL_Renderer *checked_create_renderer(SDL_Window *window, const char *name)
{
    assert(name && !strstr(name, "opengl") && !strstr(name, "opengles"));
    if (force_failure) {
        SDL_SetError("controlled test: all allowed rendering drivers are unavailable");
        return NULL;
    }
    return SDL_CreateRenderer(window, name);
}

static int captured_gui_error(const char *message)
{
    error_count++;
    snprintf(error_text, sizeof error_text, "%s", message);
    /* Production mercs_gui_error prints this and opens a message box. Capture
     * the box text here so unattended tests never need somebody to dismiss it. */
    fprintf(stderr, "captured error dialog: %s\n", message);
    return 0;
}

static void check_no_gl_libraries(void)
{
#if defined(__linux__)
    char line[1024];
    FILE *maps = fopen("/proc/self/maps", "r");
    assert(maps);
    while (fgets(line, sizeof line, maps)) {
        assert(!strstr(line, "/libGL") && !strstr(line, "/libEGL"));
        assert(!strstr(line, "/libOpenGL") && !strstr(line, "/libGLES"));
    }
    fclose(maps);
#endif
}

int main(void)
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Surface *surface;
    portcfg cfg;
    assert(SDL_Init(SDL_INIT_VIDEO));
    portcfg_defaults(&cfg);
    window = SDL_CreateWindow("Mercs GPU policy test", 64, 48, SDL_WINDOW_HIDDEN);
    assert(window);
    /* External SDL preferences cannot turn the shell's software fallback into
     * an OpenGL framebuffer. Both hints are intentionally hostile inputs. */
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "opengl", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_FRAMEBUFFER_ACCELERATION, "opengl", SDL_HINT_OVERRIDE);
    renderer = create_shell_renderer(window, &cfg);
    assert(renderer && !error_count);
    assert(!strcmp(SDL_GetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION), "0"));
    assert(!strcmp(SDL_GetHint(SDL_HINT_RENDER_DRIVER), shell_renderer_drivers(&cfg)));
    assert(!strstr(SDL_GetRendererName(renderer), "opengl"));
#ifndef __APPLE__
    if (!strcmp(SDL_GetCurrentVideoDriver(), "dummy"))
        assert(!strcmp(SDL_GetRendererName(renderer), "software"));
#endif
    assert(SDL_SetRenderDrawColor(renderer, 17, 33, 65, 255));
    assert(SDL_RenderClear(renderer));
    surface = SDL_RenderReadPixels(renderer, NULL);
    assert(surface && surface->w == 64 && surface->h == 48);
    SDL_DestroySurface(surface);
    assert(SDL_RenderPresent(renderer));
    check_no_gl_libraries();
    SDL_DestroyRenderer(renderer);
    force_failure = 1;
    renderer = create_shell_renderer(window, &cfg);
    assert(!renderer && error_count == 1);
    assert(strstr(error_text, "The Mercenaries menu renderer could not start."));
    assert(strstr(error_text, "all allowed rendering drivers are unavailable"));
#if defined(_WIN32)
    assert(strstr(error_text, "Direct3D 12") && strstr(error_text, "Vulkan") && strstr(error_text, "software"));
#elif defined(__APPLE__)
    assert(strstr(error_text, "Metal") && strstr(error_text, "macOS 10.14"));
#else
    assert(strstr(error_text, "Vulkan") && strstr(error_text, "software"));
#endif
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("PASS: production shell renderer, hostile GL hints overridden, render/readback/present, and injected unavailable-driver error dialog.");
    return 0;
}
