#include "sdl3_test.h"
/* Golden glyph positions come from executing the supplied retail shell's
 * 1004f9c0 layout and 10040330 text cursor routines with its original fonts.
 * The test reads the user's PRJ and DATABASE fixtures; no game text is bundled.
 * Arguments: retail MW2.PRJ DATABASE.MW2. No display server is required. */
#include "mercstext.h"
#include "prj.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static const unsigned char *chunk(const unsigned char *p, size_t n, size_t start,
                                   const char *tag, size_t *length)
{
    size_t at;
    for (at = start; at < n;) {
        size_t size;
        assert(n - at >= 8);
        size = rd32(p + at + 4);
        assert(size >= 8 && size <= n - at);
        if (!memcmp(p + at, tag, 4)) { *length = size - 8; return p + at + 8; }
        at += size;
    }
    return NULL;
}

static char *fixture_text(prj_archive *a, const char *name, const char *tag)
{
    prj_record r;
    const unsigned char *p;
    size_t n, length = 0;
    char *out;
    assert(prj_read_named(a, "BWD", name, &r) == PRJ_OK);
    assert(r.size >= 12 && !memcmp(r.data, "BWD", 4));
    if (!strcmp(tag, "ORDR")) p = chunk(r.data, r.size, 12, tag, &n);
    else {
        p = chunk(r.data, r.size, 12, "CPGN", &n); assert(p && n >= 0x98);
        p = chunk(p, n, 0x98, tag, &n);
    }
    assert(p);
    while (length < n && p[length]) length++;
    out = malloc(length + 1); assert(out);
    memcpy(out, p, length); out[length] = 0;
    prj_record_free(&r);
    return out;
}

static uint64_t word_hash(uint64_t h, uint32_t value)
{
    int i;
    for (i = 0; i < 4; i++) {
        h = (h ^ (value & 255)) * UINT64_C(1099511628211);
        value >>= 8;
    }
    return h;
}

static uint64_t glyph_hash(mercs_art *art, const mercs_text_layout *layout, int *count)
{
    size_t i;
    uint64_t h = UINT64_C(14695981039346656037);
    *count = 0;
    for (i = 0; i < layout->count; i++) {
        const mercs_text_run *r = &layout->runs[i];
        const unsigned char *p = (const unsigned char *)r->text;
        int x = r->x;
        for (; *p; p++) {
            char ch[2] = { (char)*p, 0 };
            int w = mercs_art_width(art, r->font, ch);
            if (w) {
                h = word_hash(h, (uint32_t)x);
                h = word_hash(h, (uint32_t)r->y);
                h = word_hash(h, (uint32_t)r->font);
                h = word_hash(h, *p);
                h = word_hash(h, (uint32_t)(r->ink < 0 ? 22 : r->ink));
                (*count)++;
            }
            x += w;
        }
    }
    return h;
}

int main(int argc, char **argv)
{
    static const struct {
        const char *name, *tag;
        int width, rows, glyphs;
        size_t next;
        uint64_t hash;
    } golden[] = {
        {"CAMP", "NEWS", 429, 21, 769, 826, UINT64_C(0xb35bf74d4b7e71cd)},
        {"NOVICE", "NEWS", 429, 23, 1007, 1065, UINT64_C(0xf3ec325a65cd4ca2)},
        {"TRA1CAM1", "CTNT", 549, 17, 744, 823, UINT64_C(0x0eb56cbe10aacb06)},
        {"BET1CAM1", "CTNT", 549, 18, 736, 814, UINT64_C(0xbabe3935c00856e0)},
        {"RAN1CAM1", "CTNT", 549, 4, 105, 35, UINT64_C(0x30fde6741a2f5016)},
        {"TRA1BRF1", "ORDR", 301, 27, 779, 880, UINT64_C(0x86e8e422fdb92e2a)},
        {"BET1BRF1", "ORDR", 301, 61, 2395, 2508, UINT64_C(0xc9189dc72122eaa9)},
        {"OUT3BRF1", "ORDR", 301, 30, 1000, 1077, UINT64_C(0x28979925aac4a93e)}
    };
    mercs_text_layout layout = {0};
    shelldb database;
    SDL_Surface *surface;
    SDL_Renderer *renderer;
    mercs_art *art;
    prj_archive *a;
    char error[256];
    int i, count;
    assert(argc == 3);
    a = prj_open(argv[1], error, sizeof error); assert(a);
    assert(!shelldb_open(argv[2], &database));
    assert(SDL_Init(0));
    surface = SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_ARGB8888); assert(surface);
    renderer = SDL_CreateSoftwareRenderer(surface); assert(renderer);
    art = mercs_art_create(&database, renderer); assert(art);
    assert(mercs_art_height(art, 10) == 10 && mercs_art_height(art, 6) == 16);
    for (i = 0; i < (int)(sizeof golden / sizeof golden[0]); i++) {
        char *raw = fixture_text(a, golden[i].name, golden[i].tag);
        uint64_t hash;
        assert(!mercs_text_prepare_names(&layout, art, raw, "HAWKES", "HAWKES MERCS", "", 10, golden[i].width));
        hash = glyph_hash(art, &layout, &count);
        if (hash != golden[i].hash)
            fprintf(stderr, "%s/%s layout hash: %016llx\n", golden[i].name, golden[i].tag, (unsigned long long)hash);
        assert(hash == golden[i].hash && count == golden[i].glyphs);
        assert(layout.rows == golden[i].rows && mercs_text_rows(&layout) == golden[i].rows);
        assert(layout.line_height == 12 && layout.next_offset == golden[i].next);
        assert(layout.height >= layout.rows * 12 && !layout.unsupported_controls);
        free(raw);
    }
    assert(!mercs_text_prepare(&layout, art, "\\cTitle\\n\\l\\cNEWS HEADLINE\\n", "", 10, 429));
    assert(glyph_hash(art, &layout, &count) == UINT64_C(0x98b5152ef96587dc) && count == 117);
    assert(!mercs_text_prepare(&layout, art, "A\\tB\\t\\tC\\n", "", 10, 301));
    assert(glyph_hash(art, &layout, &count) == UINT64_C(0x2493b7b360367661) && count == 3);
    assert(layout.count == 3 && layout.runs[0].x == 0 && layout.runs[1].x == 36 && layout.runs[2].x == 108);
    assert(!mercs_text_prepare(&layout, art, "First\\n\n\\n\r\nSecond line\\n", "", 10, 301));
    assert(glyph_hash(art, &layout, &count) == UINT64_C(0x1b8efd7d7ad49e51) && layout.rows == 4);
    assert(!mercs_text_prepare(&layout, art, "one\\n\\ssecond\\n", "", 10, 301));
    assert(layout.stopped && layout.next_offset == 7 && layout.rows == 1);
    assert(glyph_hash(art, &layout, &count) == UINT64_C(0x6b022dae59269287));
    assert(!mercs_text_prepare(&layout, art, layout.expanded + layout.next_offset, "", 10, 301));
    assert(!layout.stopped && layout.count == 1 && !strcmp(layout.runs[0].text, "second"));
    assert(!mercs_text_prepare_names(&layout, art, "\\P \\M \\Q \\R\\n", "PILOT", "UNIT", "EXTRA", 10, 301));
    assert(!strcmp(layout.expanded, "PILOT UNIT EXTRA \\n"));
    mercs_text_free(&layout);
    mercs_art_destroy(art); SDL_DestroyRenderer(renderer); SDL_DestroySurface(surface); SDL_Quit();
    shelldb_close(&database); prj_close(a);
    puts("PASS: original-executable glyph-position goldens for letters, contracts and orders; font switches, tabs, centering, substitutions and page continuation.");
    return 0;
}
