/* Golden PCM hashes were obtained by executing the supplied original Mercs
 * decoder 0x100396a4 in an isolated x86 emulator, not from this implementation.
 * Optional argument2 is the supplied CD's KEATING folder. No audio is bundled. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/sfx.c"

static uint64_t hash_pcm(const unsigned char *p, size_t n)
{
    uint64_t h = UINT64_C(14695981039346656037);
    size_t i;
    for (i = 0; i < n; i++) h = (h ^ p[i]) * UINT64_C(1099511628211);
    return h;
}
int main(int argc, char **argv)
{
    static const unsigned char raw[] = {83,70,76,88,29,0,0,0,5,0,0,0,8,0,
        5,9,20,31,42,53,64,75,86,69,101,88,55,20,1,133,150,19,0};
    static const unsigned char expected[] = {9,20,31,42,53,64,75,86,
        101,94,88,71,55,37,20,36,101,94,88,71,55,37,20,36,
        150,117,84,51,19,36,53,70,128,128,128,128,128,128,128,128};
    static const struct { int id, length; uint64_t hash; } golden[] = {
        {1,11264,UINT64_C(0x84af1bd3279bd8af)}, {2,14592,UINT64_C(0xdca04a69f9ba457b)},
        {365,13568,UINT64_C(0x396b68c688ea8a7c)}, {476,23552,UINT64_C(0xd70cb89fe73d6466)},
        {569,11264,UINT64_C(0xf910735673edfbc0)}, {570,8832,UINT64_C(0x368f5f79e16c4e74)},
        {583,28800,UINT64_C(0x0c1f2ff0540a5525)}, {607,75776,UINT64_C(0x50194946977ab88b)}};
    static const struct { const char *name; int length; uint64_t hash; } external[] = {
        {"TRA1001S",34816,UINT64_C(0x3007a14f85e4510e)},
        {"TRA1002S",240640,UINT64_C(0xbeaf81dfc02285d4)},
        {"TRA4012S",93824,UINT64_C(0xdf7d920c10782dc6)}};
    char error[256];
    prj_archive *a;
    unsigned char *pcm = NULL;
    sfx *s;
    int n, i, channel = -1;
    assert(argc == 2 || argc == 3);
    n = sfx_decode_sflx(raw, sizeof raw, &pcm);
    assert(n == (int)sizeof expected && !memcmp(pcm, expected, sizeof expected)); free(pcm);
    assert(sfx_decode_sflx(raw, sizeof raw - 1, &pcm) == -1 && !pcm);
    assert(sfx_decode_sflx((const unsigned char *)"RIFFbad", 7, &pcm) == -1 && !pcm);
    a = prj_open(argv[1], error, sizeof error); assert(a);
    for (i = 0; i < (int)(sizeof golden / sizeof golden[0]); i++) {
        n = sfx_decode(a, golden[i].id, &pcm);
        assert(n == golden[i].length && hash_pcm(pcm, (size_t)n) == golden[i].hash);
        free(pcm);
    }
    if (argc == 3) for (i = 0; i < 3; i++) {
        char path[1200]; FILE *f; long length; unsigned char *packed;
        snprintf(path, sizeof path, "%s/%s.SFL", argv[2], external[i].name);
        f = fopen(path, "rb"); assert(f);
        assert(fseek(f, 0, SEEK_END) == 0); length = ftell(f); assert(length > 0);
        rewind(f); packed = malloc((size_t)length); assert(packed);
        assert(fread(packed, 1, (size_t)length, f) == (size_t)length); fclose(f);
        n = sfx_decode_sflx(packed, (size_t)length, &pcm); free(packed);
        assert(n == external[i].length && hash_pcm(pcm, (size_t)n) == external[i].hash);
        free(pcm);
    }
    assert(SDL_Init(SDL_INIT_AUDIO));
    s = sfx_create(a); assert(s);
    sfx_play_voice(s, 569, 1.0f);
    SDL_LockAudioStream(s->stream);
    for (i = 0; i < SFX_VOICES; i++) if (s->v[i].c && s->v[i].index == 569) channel = i;
    assert(channel >= 0 && s->v[channel].is_voice);
    assert(s->v[channel].step == 11025.0 / (double)s->rate);
    assert(s->cache[569].len == 11264);
    SDL_UnlockAudioStream(s->stream);
    sfx_destroy(s); SDL_Quit(); prj_close(a);
    puts("PASS: original-decoder PCM goldens, stateful interpolation/repeat, malformed rejection and11025-Hz voice mixer path.");
    return 0;
}
