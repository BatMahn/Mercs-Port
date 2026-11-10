#include "mercsvideo.h"
#include "mercsviewport.h"
#include "smk.h"
#include "vidfx.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SDL_Texture *target;
    SDL_Rect viewport, clip;
    bool clipping, viewport_set;
    SDL_RendererLogicalPresentation logical_mode;
    int logical_w, logical_h;
    float scale_x, scale_y;
    Uint8 r, g, b, a;
    SDL_BlendMode blend;
} renderer_state;

typedef struct {
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    vidfx *filter;
    smk *decoder;
    uint8_t *doubled;
    uint8_t *rgb;          /* a 24-bit frame for a renderer without 24-bit textures (SDL's Vulkan and GPU renderers) */
    size_t rgb_size;
    int rgba;              /* the texture is 32-bit: frames are drawn into rgb and widened */
    int width, height, raw_height, texture_w, texture_h, look, fit;
} movie;

static void save_renderer(SDL_Renderer *r, renderer_state *s)
{
    memset(s, 0, sizeof *s);
    s->target = SDL_GetRenderTarget(r);
    SDL_GetRenderViewport(r, &s->viewport);
    s->viewport_set = SDL_RenderViewportSet(r);
    SDL_GetRenderClipRect(r, &s->clip);
    s->clipping = SDL_RenderClipEnabled(r);
    SDL_GetRenderLogicalPresentation(r, &s->logical_w, &s->logical_h, &s->logical_mode);
    SDL_GetRenderScale(r, &s->scale_x, &s->scale_y);
    SDL_GetRenderDrawColor(r, &s->r, &s->g, &s->b, &s->a);
    SDL_GetRenderDrawBlendMode(r, &s->blend);
}

static void restore_renderer(SDL_Renderer *r, const renderer_state *s)
{
    SDL_SetRenderTarget(r, s->target);
    /* SDL3 keeps these states independent on each target. Restoring an unset
     * viewport as NULL lets the window follow an in-movie resize/fullscreen
     * change; an explicitly authored viewport retains its exact coordinates. */
    SDL_SetRenderLogicalPresentation(r, s->logical_w, s->logical_h, s->logical_mode);
    SDL_SetRenderScale(r, s->scale_x, s->scale_y);
    SDL_SetRenderViewport(r, s->viewport_set ? &s->viewport : NULL);
    SDL_SetRenderClipRect(r, s->clipping ? &s->clip : NULL);
    SDL_SetRenderDrawColor(r, s->r, s->g, s->b, s->a);
    SDL_SetRenderDrawBlendMode(r, s->blend);
}

static int prepare_window_renderer(SDL_Renderer *r)
{
    if (mercs_renderer_pixels(r) < 0 || !SDL_SetRenderDrawColor(r, 0, 0, 0, 255) ||
        !SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE)) return -1;
    return 0;
}

static void drop_texture(movie *m)
{
    SDL_DestroyTexture(m->texture);
    m->texture = NULL;
    m->texture_w = m->texture_h = 0;
}

static double picture_aspect(const movie *m)
{
    /* Preserve the full encoded picture. 320x200 cinematic pixels use the GBL
     * presentation's 4:3 VGA aspect. Mercs MINTRO is only 144 rows high: retain
     * that relative height instead of stretching the short image to 4:3. This
     * centred framing is a port presentation policy, not a claim that the
     * original Windows shell used these destination coordinates. Other clips
     * (including embedded briefing panels) keep their encoded pixel aspect. */
    if (m->width == 320 && m->height <= 200)
        return (double)m->width * 5.0 / ((double)m->height * 6.0);
    return (double)m->width / (double)m->height;
}

static SDL_Rect movie_rectangle(const movie *m, int w, int h)
{
    double box_w = w, box_h = h, aspect = picture_aspect(m), draw_w, draw_h;
    SDL_Rect d;
    if (m->fit == PORTCFG_FIT_STRETCH) {
        d.x = d.y = 0; d.w = w; d.h = h;
        return d;
    }
    if (m->fit == PORTCFG_FIT_43) {
        if (box_w * 3.0 > box_h * 4.0) box_w = box_h * 4.0 / 3.0;
        else box_h = box_w * 3.0 / 4.0;
    }
    draw_w = box_w;
    draw_h = box_w / aspect;
    if ((m->fit == PORTCFG_FIT_FILL && draw_h < box_h) ||
        (m->fit != PORTCFG_FIT_FILL && draw_h > box_h)) {
        draw_h = box_h;
        draw_w = box_h * aspect;
    }
    d.w = (int)lround(draw_w); d.h = (int)lround(draw_h);
    if (d.w < 1) d.w = 1;
    if (d.h < 1) d.h = 1;
    d.x = (w - d.w) / 2; d.y = (h - d.h) / 2;
    return d;
}

static int make_texture(movie *m, int w, int h)
{
    SDL_Texture *tx;
    if (m->texture && m->texture_w == w && m->texture_h == h) return 0;
    tx = SDL_CreateTexture(m->renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, w, h);
    m->rgba = 0;
    if (!tx) {   /* SDL's Vulkan renderer has no 24-bit format (Windows under the Vulkan driver): RGBA, widened per frame */
        tx = SDL_CreateTexture(m->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
        if (!tx) return -1;
        if (m->rgb_size < (size_t)w * (size_t)h * 3) {
            uint8_t *b = realloc(m->rgb, (size_t)w * (size_t)h * 3);
            if (!b) { SDL_DestroyTexture(tx); return SDL_OutOfMemory(), -1; }
            m->rgb = b; m->rgb_size = (size_t)w * (size_t)h * 3;
        }
        m->rgba = 1;
    }
    SDL_SetTextureBlendMode(tx, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(tx, m->look == PORTCFG_VID_ORIGINAL ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
    drop_texture(m);
    m->texture = tx; m->texture_w = w; m->texture_h = h;
    return 0;
}

static void use_original(movie *m)
{
    SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Mercs movie filter unavailable; using original pixels: %s", SDL_GetError());
    m->look = PORTCFG_VID_ORIGINAL;
    drop_texture(m);
    vidfx_free(m->filter); m->filter = NULL;
}

static const uint8_t *movie_pixels(movie *m)
{
    const uint8_t *p = smk_pixels(m->decoder);
    int y;
    /* The foundation decoder exposes the doubled display height but stores raw
     * rows. Expand explicitly when a file requests vertical doubling, without
     * reading past its decoder-owned frame. All supplied retail clips use 0. */
    if (m->doubled) {
        for (y = 0; y < m->height; y++)
            memcpy(m->doubled + (size_t)y * (size_t)m->width,
                   p + (size_t)(y / 2) * (size_t)m->width, (size_t)m->width);
        p = m->doubled;
    }
    return p;
}

static int present_movie(movie *m)
{
    int w, h, tw, th, pitch, y, x;
    double scale;
    SDL_Rect dest;
    SDL_FRect draw;
    void *buffer;
    const uint8_t *pix = movie_pixels(m);
    const uint8_t (*pal)[3] = smk_palette(m->decoder);
    /* Resize/fullscreen/device events can alter presentation state. The movie
     * addresses the complete physical drawable afresh on every frame. */
    if (prepare_window_renderer(m->renderer) < 0) return -1;
    if (!SDL_GetRenderOutputSize(m->renderer, &w, &h)) return -1;
    if (w <= 0 || h <= 0) return 0; /* Minimized: keep timing and input alive. */
    dest = movie_rectangle(m, w, h);
    tw = m->width; th = m->height;
    if (m->look != PORTCFG_VID_ORIGINAL) {
        double max_w = m->look == PORTCFG_VID_CRT_CURVED ? 1920.0 : 2560.0;
        double max_h = m->look == PORTCFG_VID_CRT_CURVED ? 1200.0 : 1440.0;
        scale = fmin(1.0, fmin(max_w / dest.w, max_h / dest.h));
        tw = (int)ceil(dest.w * scale); th = (int)ceil(dest.h * scale);
        if (tw < m->width) tw = m->width;
        if (th < m->height) th = m->height;
        if (m->width > 640 || m->height > 480 || !m->filter) {
            SDL_SetError("source exceeds the optional filter's supported movie size");
            use_original(m); tw = m->width; th = m->height;
        }
    }
    if (make_texture(m, tw, th) < 0) {
        if (m->look == PORTCFG_VID_ORIGINAL) return -1;
        use_original(m);
        if (make_texture(m, m->width, m->height) < 0) return -1;
    }
    if (!SDL_LockTexture(m->texture, NULL, &buffer, &pitch)) return -1;
    if (m->rgba) {   /* draw the 24-bit frame aside, then widen it into the texture */
        void *tex = buffer;
        const int tpitch = pitch;
        buffer = m->rgb; pitch = m->texture_w * 3;
        if (m->look == PORTCFG_VID_ORIGINAL) {
            for (y = 0; y < m->height; y++)
                for (x = 0; x < m->width; x++)
                    memcpy(m->rgb + (size_t)y * (size_t)pitch + (size_t)x * 3, pal[pix[(size_t)y * (size_t)m->width + (size_t)x]], 3);
        } else if (vidfx_render(m->filter, m->look, pix, m->width, m->height, pal,
                                buffer, m->texture_w, m->texture_h, pitch) < 0) {
            SDL_UnlockTexture(m->texture);
            SDL_SetError("unable to allocate movie filter buffers");
            use_original(m);
            return present_movie(m);
        }
        for (y = 0; y < m->texture_h; y++) {
            const uint8_t *s3 = m->rgb + (size_t)y * (size_t)pitch;
            uint8_t *d4 = (uint8_t *)tex + (size_t)y * (size_t)tpitch;
            for (x = 0; x < m->texture_w; x++) { d4[x * 4] = s3[x * 3]; d4[x * 4 + 1] = s3[x * 3 + 1]; d4[x * 4 + 2] = s3[x * 3 + 2]; d4[x * 4 + 3] = 255; }
        }
    } else if (m->look == PORTCFG_VID_ORIGINAL) {
        for (y = 0; y < m->height; y++) {
            uint8_t *row = (uint8_t *)buffer + (size_t)y * (size_t)pitch;
            for (x = 0; x < m->width; x++) {
                const uint8_t *color = pal[pix[(size_t)y * (size_t)m->width + (size_t)x]];
                memcpy(row + (size_t)x * 3, color, 3);
            }
        }
    } else if (vidfx_render(m->filter, m->look, pix, m->width, m->height, pal,
                            buffer, m->texture_w, m->texture_h, pitch) < 0) {
        SDL_UnlockTexture(m->texture);
        SDL_SetError("unable to allocate movie filter buffers");
        use_original(m);
        return present_movie(m);
    }
    SDL_UnlockTexture(m->texture);
    /* Window events may reset the viewport. Always address the real drawable,
     * including high-DPI pixels, rather than the shell's logical canvas. */
    draw = (SDL_FRect){(float)dest.x, (float)dest.y, (float)dest.w, (float)dest.h};
    if (!SDL_RenderClear(m->renderer) ||
        !SDL_RenderTexture(m->renderer, m->texture, NULL, &draw) ||
        !SDL_RenderPresent(m->renderer)) return -1;
    return 0;
}

/* 2 means continue; other values are the public playback result. */
static int movie_event(movie *m, SDL_Window *window, const SDL_Event *e)
{
    SDL_WindowID id = SDL_GetWindowID(window);
    if (e->type == SDL_EVENT_QUIT) return 1;
    if (e->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && e->window.windowID == id) return 1;
    if (e->type == SDL_EVENT_KEY_DOWN && (e->key.windowID == id || !e->key.windowID)) {
        SDL_Keycode key = e->key.key;
        if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && (e->key.mod & SDL_KMOD_ALT)) {
            if (!e->key.repeat) {
                SDL_WindowFlags flags = SDL_GetWindowFlags(window);
                if (!SDL_SetWindowFullscreen(window, !(flags & SDL_WINDOW_FULLSCREEN)))
                    SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Mercs movie fullscreen change failed: %s", SDL_GetError());
            }
            return 2;
        }
        if (e->key.repeat || key == SDLK_LALT || key == SDLK_RALT || key == SDLK_LCTRL || key == SDLK_RCTRL ||
            key == SDLK_LSHIFT || key == SDLK_RSHIFT || key == SDLK_LGUI || key == SDLK_RGUI) return 2;
        return 0;
    } else if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && (e->button.windowID == id || !e->button.windowID)) return 0;
    else if (e->type == SDL_EVENT_RENDER_DEVICE_RESET || e->type == SDL_EVENT_RENDER_TARGETS_RESET) drop_texture(m);
    return 2;
}

static SDL_AudioStream *movie_audio(smk *decoder, int *audio_reference)
{
    int16_t *pcm = NULL;
    size_t samples = 0;
    int rate = 0, channels = 0;
    SDL_AudioStream *stream = NULL;
    SDL_AudioSpec desired;
    if (smk_audio(decoder, &pcm, &samples, &rate, &channels) < 0 || !samples) { free(pcm); return NULL; }
    if ((channels != 1 && channels != 2) || rate <= 0 || samples > INT_MAX / sizeof *pcm) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Mercs movie audio has unsupported dimensions");
        free(pcm); return NULL;
    }
    /* Take our own reference even if the caller already initialized audio. Its
     * existing devices and reference remain intact when ours is released. */
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) goto unavailable;
    *audio_reference = 1;
    SDL_zero(desired);
    desired.freq = rate; desired.format = SDL_AUDIO_S16;
    desired.channels = channels;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, NULL, NULL);
    if (!stream || !SDL_PutAudioStreamData(stream, pcm, (int)(samples * sizeof *pcm)) ||
        !SDL_FlushAudioStream(stream)) {
        SDL_DestroyAudioStream(stream);
        goto unavailable;
    }
    free(pcm);
    return stream; /* Still paused: start alongside the first presented frame. */
unavailable:
    SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Mercs movie audio unavailable: %s", SDL_GetError());
    free(pcm);
    return NULL;
}

int mercs_video_play(SDL_Window *window, SDL_Renderer *renderer, const portcfg *cfg, const char *movie_path)
{
    movie m;
    renderer_state previous, window_state;
    void *file_data = NULL;
    size_t file_size = 0;
    SDL_AudioStream *audio = NULL;
    Uint64 start = 0, frequency;
    double frame_ms;
    int result = -1, frame, saved = 0, audio_reference = 0, cursor = -1;
    char error[512] = "";
    memset(&m, 0, sizeof m);
    if (!window || !renderer || !movie_path || !movie_path[0] || SDL_GetRenderer(window) != renderer) {
        SDL_SetError("Mercs movie needs a matching window/renderer and a file path"); return -1;
    }
    m.renderer = renderer;
    m.look = cfg && cfg->videos >= 0 && cfg->videos < PORTCFG_VID_COUNT ? cfg->videos : PORTCFG_VID_ORIGINAL;
    m.fit = cfg && cfg->video_fit >= 0 && cfg->video_fit < PORTCFG_FIT_COUNT ? cfg->video_fit : PORTCFG_FIT_43;
    file_data = SDL_LoadFile(movie_path, &file_size);
    if (!file_data) goto done;
    m.decoder = smk_open_mem(file_data, file_size);
    if (!m.decoder) { SDL_SetError("Unable to decode Mercs Smacker movie: %s", movie_path); goto done; }
    m.width = smk_width(m.decoder); m.height = smk_height(m.decoder);
    m.raw_height = (int)SDL_Swap32LE(((const Uint32 *)file_data)[2]);
    if (m.height != m.raw_height) {
        m.doubled = malloc((size_t)m.width * (size_t)m.height);
        if (!m.doubled) { SDL_OutOfMemory(); goto done; }
    }
    frame_ms = smk_frame_ms(m.decoder);
    if (!isfinite(frame_ms) || frame_ms <= 0.0) { SDL_SetError("Invalid Mercs movie frame interval"); goto done; }
    if (m.look != PORTCFG_VID_ORIGINAL) m.filter = vidfx_new();
    save_renderer(renderer, &previous);
    if (!SDL_SetRenderTarget(renderer, NULL)) goto done;
    save_renderer(renderer, &window_state);
    saved = 1;
    if (prepare_window_renderer(renderer) < 0) goto done;
    cursor = SDL_CursorVisible();
    SDL_HideCursor();
    audio = movie_audio(m.decoder, &audio_reference);
    frequency = SDL_GetPerformanceFrequency();
    if (!frequency) { SDL_SetError("No SDL performance clock for movie playback"); goto done; }
    for (frame = 0; frame < smk_frames(m.decoder); frame++) {
        SDL_Event event;
        double elapsed, deadline;
        int event_result;
        while (SDL_PollEvent(&event)) {
            event_result = movie_event(&m, window, &event);
            if (event_result != 2) { result = event_result; goto done; }
        }
        if (smk_next(m.decoder) < 0) { SDL_SetError("Mercs movie frame %d failed to decode: %s", frame, movie_path); goto done; }
        elapsed = start ? (double)(SDL_GetPerformanceCounter() - start) * 1000.0 / (double)frequency : 0.0;
        /* Decode every frame so delta images and palettes remain valid. When
         * late, omit only presentation work; retain the audio's original pace. */
        if (!start || frame == smk_frames(m.decoder) - 1 || elapsed < ((double)frame + 2.0) * frame_ms) {
            if (present_movie(&m) < 0) goto done;
        }
        if (!start) {
            start = SDL_GetPerformanceCounter();
            if (audio && !SDL_ResumeAudioStreamDevice(audio))
                SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Mercs movie audio could not start: %s", SDL_GetError());
        }
        deadline = ((double)frame + 1.0) * frame_ms;
        for (;;) {
            int delay;
            elapsed = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 / (double)frequency;
            if (elapsed >= deadline) break;
            delay = (int)fmin(10.0, ceil(deadline - elapsed));
            if (SDL_WaitEventTimeout(&event, delay)) {
                event_result = movie_event(&m, window, &event);
                if (event_result != 2) { result = event_result; goto done; }
            }
        }
    }
    result = 0;
done:
    if (result < 0) SDL_strlcpy(error, SDL_GetError(), sizeof error);
    /* This stream owns its logical device; destruction stops playback and
     * releases any queued movie audio before our subsystem reference. */
    SDL_DestroyAudioStream(audio);
    if (audio_reference) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    drop_texture(&m);
    vidfx_free(m.filter);
    free(m.doubled);
    free(m.rgb);
    smk_close(m.decoder);
    SDL_free(file_data);
    if (saved) {
        restore_renderer(renderer, &window_state);
        if (previous.target) restore_renderer(renderer, &previous);
    }
    if (cursor >= 0) { if (cursor) SDL_ShowCursor(); else SDL_HideCursor(); }
    if (result < 0) SDL_SetError("%s", error[0] ? error : "Mercs movie playback failed");
    return result;
}
