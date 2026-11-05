/* Focused Mercs dispatch tests. Synthetic GPS/actor fixtures test slot rules;
 * they are not evidence that hiring/career state has been implemented.
 * All expected voice IDs come from matching DOS and Windows original tables. */
#include <assert.h>
#include <stdio.h>
#include "../src/msim.c"

typedef struct { int count, ids[32]; char text[256]; } capture;
static void said(void *u, const char *text, int voice)
{
    capture *c = u;
    assert(c->count < 32);
    c->ids[c->count++] = voice;
    snprintf(c->text, sizeof c->text, "%s", text);
}
static void defaults(msim *s, capture *c)
{
    int i;
    memset(s, 0, sizeof *s); memset(c, 0, sizeof *c);
    for (i = 0; i < 4; i++) s->pilot_voice[i] = -1;
    s->pilot_voice[4] = s->pilot_voice[5] = 32;
    s->on_message = said; s->message_user = c;
}
static void test_bindings(prj_archive *a, msim *s, capture *cap)
{
    prj_record r;
    bwd_chunk chunks[5], *original;
    bwd_record record = {0};
    bwd_mission m = {0};
    unsigned char gps[5][92];
    int n, i, found = -1, btype = prj_find_type(a, "BWD");
    assert(prj_read_named(a, "BWD", "OUT3USER", &r) == PRJ_OK);
    n = bwd_chunks(r.data, r.size, NULL, 0); assert(n > 0);
    original = calloc((size_t)n, sizeof *original); assert(original);
    bwd_chunks(r.data, r.size, original, n);
    for (i = 0; i < n; i++) if (!strcmp(original[i].tag, "GPS")) found = i;
    assert(found >= 0 && original[found].size == 92);
    for (i = 0; i < 5; i++) {
        memcpy(gps[i], original[found].data, 92);
        gps[i][4] = 0; gps[i][6] = i == 0 ? 0 : 2;
        gps[i][22] = 0; gps[i][23] = 0;
        memcpy(chunks[i].tag, "GPS\0", 5);
        chunks[i].data = gps[i]; chunks[i].size = 92;
    }
    gps[1][22] = 1;      /* Crash, slot1 */
    memset(gps[2] + 28, 0, 9); memcpy(gps[2] + 28, "AIRCRFTC", 8);
    i = prj_find_id(a, btype, "AIRCRFTC"); assert(i > 0);
    gps[2][2] = (unsigned char)i; gps[2][3] = (unsigned char)(i >> 8);
    gps[2][22] = 30;     /* reserved support slot4 keeps original bank32 */
    gps[3][22] = 20;     /* Ice Dancer, slot2 (support did not consume it) */
    gps[4][4] = 1; gps[4][22] = 31;   /* another group cannot overwrite it */
    record.chunks = chunks; record.chunk_count = 5;
    m.records = &record; m.record_count = 1;
    defaults(s, cap); pilot_voices_load(s, a, &m, 0);
    assert(s->pilot_voice[1] == 1 && s->pilot_voice[2] == 20);
    assert(s->pilot_voice[3] == -1 && s->pilot_voice[4] == 32);
    msim_order_message(s, 0, 1);
    assert(cap->count == 1 && cap->ids[0] == (373 | 524 << 16));
    free(original); prj_record_free(&r);
}
int main(int argc, char **argv)
{
    char error[256];
    prj_archive *a;
    msim *s = calloc(1, sizeof *s);
    capture cap;
    world_actor actors[8] = {0};
    combat_unit units[8] = {0};
    unsigned char seen[8] = {0};
    int armed[8], i;
    const int expected[] = {65, 67, 68, 66, 67, 67};
    assert(argc == 2 && s);
    a = prj_open(argv[1], error, sizeof error); assert(a);
    defaults(s, &cap);
    msim_order_message(s, 0, 1); assert(cap.count == 0); /* absent pilots stay silent */
    s->pilot_voice[1] = 0; s->pilot_voice[2] = 1;
    msim_order_message(s, 1, 1);
    assert(cap.count == 1 && cap.ids[0] == 363 && !strcmp(cap.text, "Its going down."));
    msim_order_message(s, 0, 2);
    assert(cap.count == 2 && cap.ids[1] == (367 | 375 << 16));
    msim_order_message(s, 1, 10); assert(cap.count == 2); /* original dispatcher no-op */
    msim_pilot_message(s, 1, 0); assert(cap.ids[2] == 365);
    msim_pilot_message(s, 1, 0); assert(cap.ids[3] == 364);
    msim_pilot_message(s, 1, 0); assert(cap.ids[4] == 365);
    msim_pilot_message(s, 3, 2); assert(cap.count == 5); /* unsupported selector slot */
    s->pilot_voice[1] = 15; msim_order_message(s, 1, 1);
    assert(cap.ids[5] == 453); /* original Havoc/Death-Angel duplicate bank */
    msim_order_message(s, 2, 7); assert(cap.ids[6] == (288 | 49 << 16));
    msim_order_message(s, 1, 8); assert(cap.ids[7] == (287 | 4 << 16));
    test_bindings(a, s, &cap);
    defaults(s, &cap);
    s->actors = actors; s->actor_count = 8; s->units = units; s->armed = armed; s->msg_seen = seen;
    for (i = 0; i < 8; i++) {
        actors[i].unit_class = i + 1; actors[i].alliance = 1;
        units[i].destroyed = 1; armed[i] = 1;
    }
    unit_messages(s);
    assert(cap.count == 6);
    for (i = 0; i < 6; i++) assert(cap.ids[i] == expected[i]);
    unit_messages(s); assert(cap.count == 6); /* one announcement per death */
    units[0].destroyed = 0; seen[0] = 2;
    unit_messages(s); assert(cap.count == 7 && cap.ids[6] == 69);
    prj_close(a); free(s);
    puts("PASS: original pilot banks/order routing, check-in alternation, GPS slot binding, generic fallbacks and death/restart IDs.");
    return 0;
}
