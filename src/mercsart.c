/* Faithful bitmap composition for the original Mercenaries shell resources.
 * Asset layouts are decoded by the existing, mature PRJ/PCX/SHP/Smacker readers.
 * The control coordinates and screen state machine live in mercsshell.c. */
#include "mercsart.h"
#include "datapath.h"
#include "shp.h"
#include "smk.h"

#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ART_W = 640, ART_H = 480, ART_ENTRIES = 120, ART_CLIPS = 96 };
typedef struct {
    unsigned char *data;
    size_t length;
    int loaded;
    shell_image image;
} art_entry;
typedef struct {
    char name[20];
    smk *movie;
    shp_frame shape;
    shp_frame active_shape;
    int have_shape, have_active_shape;
    shp_frame extra_shape[5];
    unsigned extra_shapes;
    int frame, finished, audio_loaded, rate, channels;
    unsigned page;
    int16_t *pcm;
    size_t pcm_frames;
    Uint64 last_tick;
} art_clip;
struct mercs_art {
    const shelldb *db;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Uint32 frame[ART_W * ART_H];
    Uint32 palette[256];
    art_entry entries[ART_ENTRIES];
    art_clip clips[ART_CLIPS];
    int clip_count;
    unsigned page;
    sfx *audio;
    char extra_dir[2048];   /* a second folder for named clips (Titanium's shell art), searched after the game's */
};
struct mercs_art_snapshot {
    Uint32 frame[ART_W * ART_H];
    Uint32 palette[256];
};

static uint32_t read32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static art_entry *entry(mercs_art *a, int index)
{
    art_entry *e;
    long length;
    if (!a || index < 0 || index >= ART_ENTRIES) return NULL;
    e = &a->entries[index];
    if (!e->loaded) {
        e->loaded = 1;
        length = shelldb_entry(a->db, index, &e->data);
        if (length > 0) e->length = (size_t)length;
        if (e->length > 0 && e->data[0] == 10)
            (void)shell_pcx(e->data, e->length, &e->image, NULL);
    }
    return e->length ? e : NULL;
}

static Uint32 rgba(const unsigned char *rgb)
{ return 0xff000000u | (Uint32)rgb[0] << 16 | (Uint32)rgb[1] << 8 | rgb[2]; }

mercs_art *mercs_art_create(const shelldb *database, SDL_Renderer *renderer)
{
    mercs_art *a;
    if (!database || !renderer) return NULL;
    a = calloc(1, sizeof *a);
    if (a) { a->db = database; a->renderer = renderer; }
    return a;
}

void mercs_art_reset_device(mercs_art *a)
{
    if (!a) return;
    SDL_DestroyTexture(a->texture); a->texture = NULL;
}

void mercs_art_set_audio(mercs_art *a, sfx *audio)
{
    int i;
    if (!a || a->audio == audio) return;
    if (a->audio) for (i = 0; i < a->clip_count; i++)
        if (a->clips[i].pcm) sfx_stop_pcm(a->audio, (const unsigned char *)a->clips[i].pcm);
    a->audio = audio;
}

mercs_art_snapshot *mercs_art_capture(const mercs_art *a)
{
    mercs_art_snapshot *snapshot;
    if (!a) { SDL_SetError("Mercs art capture needs a compositor"); return NULL; }
    snapshot = malloc(sizeof *snapshot);
    if (!snapshot) { SDL_OutOfMemory(); return NULL; }
    memcpy(snapshot->frame, a->frame, sizeof snapshot->frame);
    memcpy(snapshot->palette, a->palette, sizeof snapshot->palette);
    return snapshot;
}

void mercs_art_restore(mercs_art *a, const mercs_art_snapshot *snapshot)
{
    if (!a || !snapshot) return;
    memcpy(a->frame, snapshot->frame, sizeof a->frame);
    memcpy(a->palette, snapshot->palette, sizeof a->palette);
}

void mercs_art_snapshot_free(mercs_art_snapshot *snapshot)
{ free(snapshot); }

void mercs_art_begin_page(mercs_art *a)
{
    int i;
    if (!a) return;
    if (a->audio) for (i = 0; i < a->clip_count; i++)
        if (a->clips[i].pcm) sfx_stop_pcm(a->audio, (const unsigned char *)a->clips[i].pcm);
    a->page++;
}

void mercs_art_destroy(mercs_art *a)
{
    int i;
    if (!a) return;
    mercs_art_reset_device(a);
    for (i = 0; i < ART_ENTRIES; i++) {
        shell_image_free(&a->entries[i].image);
        free(a->entries[i].data);
    }
    for (i = 0; i < a->clip_count; i++) {
        if (a->audio && a->clips[i].pcm) sfx_stop_pcm(a->audio, (const unsigned char *)a->clips[i].pcm);
        free(a->clips[i].pcm);
        smk_close(a->clips[i].movie);
        shp_frame_free(&a->clips[i].shape);
        shp_frame_free(&a->clips[i].active_shape);
        {
            int frame;
            for (frame = 0; frame < 5; frame++) shp_frame_free(&a->clips[i].extra_shape[frame]);
        }
    }
    free(a);
}

int mercs_art_background(mercs_art *a, int index)
{
    art_entry *e = entry(a, index);
    int x, y, i;
    if (!e || !e->image.pix) return -1;
    for (i = 0; i < 256; i++) a->palette[i] = rgba(e->image.pal[i]);
    for (y = 0; y < ART_H; y++) for (x = 0; x < ART_W; x++)
        a->frame[y * ART_W + x] = x < e->image.w && y < e->image.h ?
            a->palette[e->image.pix[y * e->image.w + x]] : 0xff000000u;
    return 0;
}

int mercs_art_background_image(mercs_art *a, const shell_image *image)
{
    int x, y, i;
    if (!a || !image || !image->pix) return -1;
    for (i = 0; i < 256; i++) a->palette[i] = rgba(image->pal[i]);
    for (y = 0; y < ART_H; y++) for (x = 0; x < ART_W; x++)
        a->frame[y * ART_W + x] = x < image->w && y < image->h ?
            a->palette[image->pix[y * image->w + x]] : 0xff000000u;
    return 0;
}

void mercs_art_set_extra_dir(mercs_art *a, const char *dir)
{
    if (a) snprintf(a->extra_dir, sizeof a->extra_dir, "%s", dir ? dir : "");
}

static const unsigned char *glyph(art_entry *e, unsigned c, int *width, int *height, int *key)
{
    uint32_t off, count;
    if (!e || e->length < 16 || memcmp(e->data, "1.", 2)) return NULL;
    count = read32(e->data + 4);
    if (c >= count || 16u + (size_t)c * 4u + 4u > e->length) return NULL;
    off = read32(e->data + 16u + c * 4u);
    if ((size_t)off + 4u > e->length) return NULL;
    *width = (int)read32(e->data + off); *height = (int)read32(e->data + 8);
    *key = e->data[12];
    if (*width < 0 || *width > 128 || *height <= 0 || *height > 128 ||
        (size_t)off + 4u + (size_t)*width * (size_t)*height > e->length) return NULL;
    return e->data + off + 4;
}

int mercs_art_width(mercs_art *a, int font, const char *text)
{
    art_entry *e = entry(a, font);
    int total = 0, w = 0, h = 0, key = 0;
    if (!text) return 0;
    while (*text) {
        if (glyph(e, (unsigned char)*text++, &w, &h, &key)) total += w;
    }
    return total;
}

int mercs_art_height(mercs_art *a, int font)
{
    art_entry *e = entry(a, font);
    if (!e || e->length < 16 || memcmp(e->data, "1.", 2)) return 0;
    return (int)read32(e->data + 8);
}

void mercs_art_text_clipped(mercs_art *a, int font, int x, int y, const char *text,
                            int remap, int top, int bottom)
{
    art_entry *e = entry(a, font);
    if (!e || !text) return;
    if (top < 0) top = 0;
    if (bottom > ART_H) bottom = ART_H;
    if (bottom <= top) return;
    while (*text) {
        int w = 0, h = 0, key = 0, xx, yy;
        const unsigned char *g = glyph(e, (unsigned char)*text++, &w, &h, &key);
        if (!g) continue;
        for (yy = 0; yy < h; yy++) for (xx = 0; xx < w; xx++) {
            int px = x + xx, py = y + yy, ink = g[yy * w + xx];
            if (px < 0 || px >= ART_W || py < top || py >= bottom || ink == key) continue;
            /* Original screen constructors set remap[0] to 255 (transparent)
             * and remap[1] to their active/normal palette ink. */
            if (remap >= 0 && ink == 0) continue;
            if (remap >= 0 && ink == 1) ink = remap & 255;
            a->frame[py * ART_W + px] = a->palette[ink];
        }
        x += w;
    }
}

void mercs_art_text(mercs_art *a, int font, int x, int y, const char *text, int remap)
{ mercs_art_text_clipped(a, font, x, y, text, remap, 0, ART_H); }

void mercs_art_center(mercs_art *a, int font, int x, int y, const char *text, int remap)
{ mercs_art_text(a, font, x - mercs_art_width(a, font, text) / 2, y, text, remap); }

static void blit_shape(mercs_art *a, const shp_frame *shape, int x, int y)
{
    int xx, yy;
    if (!shape->pix || !shape->mask) return;
    for (yy = 0; yy < shape->h; yy++) for (xx = 0; xx < shape->w; xx++) {
        int px = x + shape->left + xx, py = y + shape->top + yy;
        if (px >= 0 && px < ART_W && py >= 0 && py < ART_H && shape->mask[yy * shape->w + xx])
            a->frame[py * ART_W + px] = a->palette[shape->pix[yy * shape->w + xx]];
    }
}

int mercs_art_shape(mercs_art *a, int index, int frame, int x, int y)
{
    art_entry *e = entry(a, index);
    shp_frame shape;
    if (!e || shp_decode(e->data, e->length, frame, &shape)) return -1;
    blit_shape(a, &shape, x, y); shp_frame_free(&shape);
    return 0;
}

static art_clip *open_clip(mercs_art *a, const char *name)
{
    art_clip *clip;
    char path[2048], relative[64];
    int i;
    FILE *file;
    for (i = 0; i < a->clip_count; i++) if (!strcmp(a->clips[i].name, name)) return &a->clips[i];
    if (a->clip_count >= ART_CLIPS || strlen(name) >= sizeof a->clips[0].name) return NULL;
    clip = &a->clips[a->clip_count++];
    clip->page = a->page - 1;
    snprintf(clip->name, sizeof clip->name, "%s", name);
    snprintf(relative, sizeof relative, "SMK\\%s.SMK", name);
    if (!dp_resolve(relative, path, sizeof path) ||
        (a->extra_dir[0] && !dp_resolve_in(a->extra_dir, relative, path, sizeof path))) clip->movie = smk_open_file(path);
    if (clip->movie) {
        smk_next(clip->movie); clip->last_tick = SDL_GetTicks();
        return clip;
    }
    snprintf(relative, sizeof relative, "SMK\\%s.SHP", name);
    if ((!dp_resolve(relative, path, sizeof path) ||
         (a->extra_dir[0] && !dp_resolve_in(a->extra_dir, relative, path, sizeof path))) && (file = fopen(path, "rb"))) {
        long length;
        unsigned char *data = NULL;
        if (!fseek(file, 0, SEEK_END) && (length = ftell(file)) > 0 && length < 16 * 1024 * 1024 && !fseek(file, 0, SEEK_SET)) {
            data = malloc((size_t)length);
            if (data && fread(data, 1, (size_t)length, file) == (size_t)length && !shp_decode(data, (size_t)length, 0, &clip->shape)) {
                clip->have_shape = 1;
                if (shp_count(data, (size_t)length) > 1 &&
                    !shp_decode(data, (size_t)length, 1, &clip->active_shape)) clip->have_active_shape = 1;
                {
                    int frame, count = shp_count(data, (size_t)length);
                    for (frame = 2; frame < count && frame < 7; frame++)
                        if (!shp_decode(data, (size_t)length, frame, &clip->extra_shape[frame-2]))
                            clip->extra_shapes |= 1u << (unsigned)(frame-2);
                }
            }
        }
        free(data); fclose(file);
    }
    return clip;
}

static void clip_audio(mercs_art *a, art_clip *clip)
{
    int16_t *samples = NULL;
    size_t count = 0;
    int channels = 1;
    if (!clip->audio_loaded) {
        clip->audio_loaded = 1;
        if (!smk_audio(clip->movie, &samples, &count, &clip->rate, &channels) && samples && count &&
            (channels == 1 || channels == 2) && clip->rate > 0 &&
            !(count % (size_t)channels) && count / (size_t)channels <= INT_MAX) {
            /* smk_audio already supplies native-endian signed16 PCM. Preserve
             * both stereo channels and all sample bits; the mixer resamples
             * at the file's original rate without an 8-bit mono intermediate. */
            clip->pcm = samples; samples = NULL;
            clip->pcm_frames = count / (size_t)channels;
            clip->channels = channels;
        }
        free(samples);
    }
    if (a->audio && clip->pcm) {
        sfx_stop_pcm(a->audio, (const unsigned char *)clip->pcm);
        sfx_play_pcm_s16(a->audio, clip->pcm, (int)clip->pcm_frames, clip->channels, clip->rate, 1.0f, 0);
    }
}

int mercs_art_clip_flags(mercs_art *a, const char *name, int x, int y,
                         unsigned flags, int restart)
{
    art_clip *clip;
    if (!a || !name || !(clip = open_clip(a, name))) return -1;
    if (clip->movie) {
        const unsigned char *pixels;
        const uint8_t (*palette)[3];
        int xx, yy, w = smk_width(clip->movie), h = smk_height(clip->movie), steps = 0;
        Uint64 now = SDL_GetTicks();
        double duration = smk_frame_ms(clip->movie);
        Uint64 interval = duration >= 1.0 ? (Uint64)duration : 1;
        if (restart || clip->page != a->page) {
            smk_rewind(clip->movie); clip->frame = smk_next(clip->movie);
            clip->last_tick = now; clip->finished = 0; clip->page = a->page;
            clip_audio(a, clip);
            fprintf(stderr, "mercs shell: original-clip=%s frames=%d pcm_frames=%zu rate=%d channels=%d flags=0x%x\n",
                    name, smk_frames(clip->movie), clip->pcm_frames, clip->rate, clip->channels, flags);
        }
        /* A suspended window does not decode an unbounded backlog of frames. */
        if (now - clip->last_tick > interval * 8u) clip->last_tick = now - interval;
        while (!(flags & 1u) && !clip->finished && now - clip->last_tick >= interval && steps++ < 8) {
            if (!(flags & 8u) && clip->frame + 1 >= smk_frames(clip->movie)) clip->finished = 1;
            else clip->frame = smk_next(clip->movie);
            clip->last_tick += interval;
        }
        /* The original dirty-rectangle compositor retains the last painted
         * pixels when a non-looping object is disposed. Our page is recomposed
         * every frame, so keep painting that image until the page changes.
         * This is visible in the completed RCOMSTAR registration logo. */
        pixels = smk_pixels(clip->movie); palette = smk_palette(clip->movie);
        for (yy = 0; yy < h; yy++) for (xx = 0; xx < w; xx++) {
            int px = x + xx, py = y + yy;
            if (px >= 0 && px < ART_W && py >= 0 && py < ART_H)
                a->frame[py * ART_W + px] = rgba(palette[pixels[yy * w + xx]]);
        }
        return clip->finished;
    }
    if (clip->have_shape) { blit_shape(a, &clip->shape, x, y); return 0; }
    return -1;
}

int mercs_art_clip(mercs_art *a, const char *name, int x, int y, int restart)
{ return mercs_art_clip_flags(a, name, x, y, 8u, restart); }

int mercs_art_button(mercs_art *a, const char *name, int x, int y, int active)
{
    art_clip *clip = a && name ? open_clip(a, name) : NULL;
    if (!clip || !clip->have_shape) return -1;
    blit_shape(a, active && clip->have_active_shape ? &clip->active_shape : &clip->shape, x, y);
    return 0;
}

int mercs_art_named_shape(mercs_art *a, const char *name, int frame, int x, int y)
{
    art_clip *clip = a && name ? open_clip(a, name) : NULL;
    const shp_frame *shape;
    if (!clip || frame < 0 || frame > 6) return -1;
    if (frame == 0 && clip->have_shape) shape = &clip->shape;
    else if (frame == 1 && clip->have_active_shape) shape = &clip->active_shape;
    else if (frame >= 2 && (clip->extra_shapes & (1u << (unsigned)(frame-2))))
        shape = &clip->extra_shape[frame-2];
    else return -1;
    blit_shape(a, shape, x, y);
    return 0;
}

void mercs_art_rect(mercs_art *a, int x, int y, int w, int h, int index)
{
    int xx, yy;
    if (!a) return;
    for (yy = y < 0 ? 0 : y; yy < y + h && yy < ART_H; yy++)
        for (xx = x < 0 ? 0 : x; xx < x + w && xx < ART_W; xx++)
            a->frame[yy * ART_W + xx] = a->palette[index & 255];
}

void mercs_art_line(mercs_art *a, int x0, int y0, int x1, int y1, int index)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, error = dx + dy;
    if (!a) return;
    for (;;) {
        int e2 = 2 * error;
        if (x0 >= 0 && x0 < ART_W && y0 >= 0 && y0 < ART_H) a->frame[y0 * ART_W + x0] = a->palette[index & 255];
        if (x0 == x1 && y0 == y1) break;
        if (e2 >= dy) { error += dy; x0 += sx; }
        if (e2 <= dx) { error += dx; y0 += sy; }
    }
}

int mercs_art_present(mercs_art *a)
{
    if (!a) return -1;
    if (!a->texture) {
        a->texture = SDL_CreateTexture(a->renderer, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING, ART_W, ART_H);
        if (!a->texture) return -1;
        SDL_SetTextureScaleMode(a->texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(a->texture, SDL_BLENDMODE_NONE);
    }
    if (!SDL_UpdateTexture(a->texture, NULL, a->frame, ART_W * 4) ||
        !SDL_SetRenderDrawColor(a->renderer, 0, 0, 0, 255) ||
        !SDL_RenderClear(a->renderer) ||
        !SDL_RenderTexture(a->renderer, a->texture, NULL, NULL)) return -1;
    return 0;
}
