#include "sdl3_test.h"
/* Asset-backed compositor boundary checks. The audio sink is intercepted here
 * to compare the exact decoded samples and ownership/stop order; the real
 * signed16 mixing arithmetic is covered by the mixer tests.
 * Usage: test_mercs_art DATABASE.MW2 CD_DIRECTORY */
#define sfx_play_pcm_s16 fixture_play_s16
#define sfx_stop_pcm fixture_stop_pcm
#include "../src/mercsart.c"
#undef sfx_play_pcm_s16
#undef sfx_stop_pcm
#include <assert.h>

static const int16_t *expected_pcm;
static size_t expected_samples;
static int expected_channels, expected_rate;
static const void *active[8];
static int plays, stops, clips_checked, snapshot_checks, clipping_checks;

int fixture_play_s16(sfx *audio, const int16_t *pcm, int frames, int channels,
                    int rate, float volume, int loop)
{
    int i;
    assert(audio && expected_pcm && pcm && frames > 0);
    assert(channels == expected_channels && rate == expected_rate && volume == 1.0f && !loop);
    assert((size_t)frames * (size_t)channels == expected_samples);
    assert(!memcmp(pcm, expected_pcm, expected_samples * sizeof *pcm));
    for (i = 0; i < 8 && active[i]; i++) {}
    assert(i < 8); active[i] = pcm; plays++;
    return 0;
}

void fixture_stop_pcm(sfx *audio, const unsigned char *pcm)
{
    int i;
    assert(audio && pcm);
    /* Access while stopping: under ASan this also detects stop-after-free. */
    { volatile int16_t first_sample = *(const int16_t *)pcm; (void)first_sample; }
    for (i = 0; i < 8; i++) if (active[i] == pcm) active[i] = NULL;
    stops++;
}

static int active_count(void)
{
    int i, n = 0;
    for (i = 0; i < 8; i++) if (active[i]) n++;
    return n;
}

static void check_clip(mercs_art *art, const char *name, int channels)
{
    char path[2048], relative[64];
    smk *decoder;
    int16_t *pcm = NULL, *retained;
    size_t samples = 0, i, precise = 0, different = 0;
    int rate, decoded_channels, before;
    snprintf(relative, sizeof relative, "SMK\\%s.SMK", name);
    assert(!dp_resolve(relative, path, sizeof path));
    decoder = smk_open_file(path); assert(decoder);
    assert(!smk_audio(decoder, &pcm, &samples, &rate, &decoded_channels));
    assert(samples && decoded_channels == channels && rate == 22050);
    for (i = 0; i < samples; i++) if ((uint16_t)pcm[i] & 255u) precise++;
    if (channels == 2) for (i = 0; i + 1 < samples; i += 2) if (pcm[i] != pcm[i+1]) different++;
    assert(precise && (channels == 1 || different));
    expected_pcm = pcm; expected_samples = samples; expected_channels = channels; expected_rate = rate;
    before = plays;
    assert(mercs_art_clip_flags(art, name, 0, 0, 0x24, 1) >= 0);
    assert(plays == before + 1 && active_count() == 1);
    retained = art->clips[art->clip_count - 1].pcm;
    assert(retained && retained != pcm);
    assert(art->clips[art->clip_count - 1].pcm_frames == samples / (size_t)channels);
    /* Restart reuses decoded ownership, stopping the old play first. */
    assert(mercs_art_clip_flags(art, name, 0, 0, 0x24, 1) >= 0);
    assert(plays == before + 2 && active_count() == 1 && art->clips[art->clip_count - 1].pcm == retained);
    mercs_art_begin_page(art); assert(!active_count());
    assert(mercs_art_clip_flags(art, name, 0, 0, 0x24, 0) >= 0);
    assert(plays == before + 3 && active_count() == 1 && art->clips[art->clip_count - 1].pcm == retained);
    printf("art clip %s: %zu signed16 samples, %d channels, %d Hz, %zu low-byte samples preserved\n",
           name, samples, channels, rate, precise);
    mercs_art_begin_page(art); assert(!active_count());
    expected_pcm = NULL; free(pcm); smk_close(decoder); clips_checked++;
}

static void check_snapshot(mercs_art *art)
{
    mercs_art_snapshot *snapshot;
    Uint32 *original = malloc(sizeof art->frame), palette[256], *rendered = malloc(sizeof art->frame);
    unsigned page = art->page;
    assert(original && rendered && !mercs_art_background(art, 28));
    memcpy(original, art->frame, sizeof art->frame); memcpy(palette, art->palette, sizeof palette);
    snapshot = mercs_art_capture(art); assert(snapshot);
    assert(!mercs_art_shape(art, 3, 0, 80, 80));
    mercs_art_rect(art, 200, 200, 100, 40, 17);
    art->palette[17] ^= 0xffffff;
    assert(memcmp(original, art->frame, sizeof art->frame));
    mercs_art_begin_page(art); mercs_art_reset_device(art);
    mercs_art_restore(art, snapshot);
    assert(!memcmp(original, art->frame, sizeof art->frame));
    assert(!memcmp(palette, art->palette, sizeof palette) && art->page == page + 1);
    assert(!mercs_art_present(art));
    assert(!mercs_test_read_pixels(art->renderer, NULL, SDL_PIXELFORMAT_ARGB8888, rendered, ART_W * 4));
    assert(!memcmp(original, rendered, sizeof art->frame));
    mercs_art_snapshot_free(snapshot); mercs_art_snapshot_free(NULL);
    assert(!mercs_art_capture(NULL));
    free(original); free(rendered); snapshot_checks++;
}

static void check_clipped_text(mercs_art *art, int text_y, int top, int bottom)
{
    Uint32 *base = malloc(sizeof art->frame), *complete = malloc(sizeof art->frame);
    int x, y, visible = 0, discarded = 0;
    assert(base && complete && !mercs_art_background(art, 28));
    memcpy(base, art->frame, sizeof art->frame);
    mercs_art_text(art, 6, 240, text_y, "MERCENARIES", 17);
    memcpy(complete, art->frame, sizeof art->frame);
    memcpy(art->frame, base, sizeof art->frame);
    mercs_art_text_clipped(art, 6, 240, text_y, "MERCENARIES", 17, top, bottom);
    for (y = 0; y < ART_H; y++) for (x = 0; x < ART_W; x++) {
        size_t at = (size_t)y * ART_W + (size_t)x;
        if (y >= top && y < bottom) {
            assert(art->frame[at] == complete[at]);
            if (complete[at] != base[at]) visible++;
        } else {
            assert(art->frame[at] == base[at]);
            if (complete[at] != base[at]) discarded++;
        }
    }
    assert(visible && discarded);
    memcpy(art->frame, base, sizeof art->frame);
    mercs_art_text_clipped(art, 6, 240, text_y, "MERCENARIES", 17, 200, 100);
    assert(!memcmp(base, art->frame, sizeof art->frame));
    free(base); free(complete); clipping_checks++;
}

int main(int argc, char **argv)
{
    shelldb database;
    SDL_Surface *surface;
    SDL_Renderer *renderer;
    mercs_art *art;
    int audio_identity = 1, replacement_identity = 2, before;
    if (argc != 3) { fprintf(stderr, "Usage: %s DATABASE.MW2 CD_DIRECTORY\n", argv[0]); return 2; }
    assert(SDL_Init(0) && !shelldb_open(argv[1], &database) && !dp_add_root(argv[2]));
    surface = SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_ARGB8888); assert(surface);
    renderer = SDL_CreateSoftwareRenderer(surface); assert(renderer);
    art = mercs_art_create(&database, renderer); assert(art);
    check_snapshot(art);
    check_clipped_text(art, 118, 125, 460);
    check_clipped_text(art, 450, 125, 460);
    mercs_art_set_audio(art, (sfx *)&audio_identity);
    check_clip(art, "SPA1BRF1", 1);
    check_clip(art, "MLAND", 2);
    /* The actual sink lifecycle is independent of the snapshot/texture. */
    active[0] = art->clips[0].pcm; before = stops;
    mercs_art_set_audio(art, (sfx *)&replacement_identity);
    assert(!active_count() && stops == before + art->clip_count);
    active[0] = art->clips[0].pcm; before = stops;
    mercs_art_set_audio(art, NULL);
    assert(!active_count() && stops == before + art->clip_count);
    mercs_art_set_audio(art, (sfx *)&audio_identity);
    active[0] = art->clips[0].pcm; before = stops;
    mercs_art_destroy(art);
    assert(!active_count() && stops == before + 2);
    SDL_DestroyRenderer(renderer); SDL_DestroySurface(surface); shelldb_close(&database); dp_clear_roots(); SDL_Quit();
    printf("test_mercs_art: ok (%d real audio clips, %d exact PCM plays, %d snapshot, %d partial-row cases)\n",
           clips_checked, plays, snapshot_checks, clipping_checks);
    return 0;
}
