/* Mercenaries initial shell data, reconstructed from the user's original files.
 * MW2SHELL.DLL: new career 0x10039270; CPGN 0x10038710; registration
 * 0x10041b10; launch assignment 0x1001d650; chassis row70 at 0x10083d8c.
 * Also imports deployed MEKs through the separately verified DOS shell record
 * conversion. Full SHELLMER saves and contract settlement remain separate. */
#include "mercsshell_data.h"
#include "mercsrmg.h"
#include "mercsreturn.h"
#include <time.h>
#include "bwd.h"
#include "datapath.h"
#include "mercsfile.h"
#include "lance.h"
#include "mek.h"
#include "mercsshell_chassis.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    prj_record record;
    int kind, year, month;
    int32_t success_pay;
    char scene[16], briefing[16], faction[16];
    char next[MERCS_SHELL_MAX_MISSIONS][16];
    int next_count;
    const unsigned char *contract, *news, *starter;
    size_t contract_size, news_size, starter_size;
    const unsigned char *available_mechs;
    size_t available_mechs_size;
} campaign;

static int fail(char *error, size_t n, const char *fmt, ...)
{
    va_list ap;
    if (error && n) {
        va_start(ap, fmt);
        vsnprintf(error, n, fmt, ap);
        va_end(ap);
    }
    return -1;
}

static uint32_t u32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int fixed_name(char *out, size_t capacity, const unsigned char *p, size_t n)
{
    size_t i = 0, len = 0;
    while (len < n && p[len]) len++;
    while (len && p[len - 1] == ' ') len--;
    if (!len || len >= capacity) return -1;
    for (i = 0; i < len; i++) {
        unsigned char c = p[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return -1;
        out[i] = (char)toupper(c);
    }
    out[len] = 0;
    return 0;
}

static int owned_basename(char *name)
{
    size_t i,len=strlen(name);
    if(len==12&&!strcasecmp(name+8,".MEK"))name[8]=0;
    if(strlen(name)!=8)return -1;
    for(i=0;i<8;i++) {
        if(!isalnum((unsigned char)name[i])&&name[i]!='_')return -1;
        name[i]=(char)toupper((unsigned char)name[i]);
    }
    return 0;
}

static int bwd_name(char *out,size_t capacity,const unsigned char *p,size_t n)
{
    size_t len=0;while(len<n&&p[len])len++;
    if(len>4&&!strncasecmp((const char *)p+len-4,".BWD",4))len-=4;
    return fixed_name(out,capacity,p,len);
}

static int chunks_of(const prj_record *r, bwd_chunk **chunks,
                     char *error, size_t n)
{
    int count = bwd_chunks(r->data, r->size, NULL, 0);
    if (count < 0 || count > 4096) return fail(error, n, "Malformed BWD resource %s", r->name);
    *chunks = calloc((size_t)(count ? count : 1), sizeof **chunks);
    if (!*chunks) return fail(error, n, "Out of memory reading %s", r->name);
    if (bwd_chunks(r->data, r->size, *chunks, count) != count) {
        free(*chunks); *chunks = NULL;
        return fail(error, n, "Malformed BWD resource %s", r->name);
    }
    return count;
}

static void campaign_free(campaign *c) { prj_record_free(&c->record); }

static int campaign_read(prj_archive *a, const char *name, campaign *c,
                         char *error, size_t n)
{
    bwd_chunk *chunks = NULL;
    const unsigned char *p = NULL;
    size_t size = 0, off;
    int i, count, seen = 0;
    memset(c, 0, sizeof *c);
    if (prj_read_named(a, "BWD", name, &c->record) != PRJ_OK)
        return fail(error, n, "Missing campaign resource %s", name);
    count = chunks_of(&c->record, &chunks, error, n);
    if (count < 0) goto bad;
    for (i = 0; i < count; i++) if (!strcmp(chunks[i].tag, "CPGN")) {
        p = chunks[i].data; size = chunks[i].size; seen++;
    }
    free(chunks); chunks = NULL;
    if (seen != 1 || size < 0x98) { fail(error, n, "%s has no valid CPGN header", name); goto bad; }
    c->kind = (int32_t)u32(p);
    c->year = (int32_t)u32(p + 4); c->month = (int32_t)u32(p + 8);
    c->success_pay = (int32_t)u32(p + 92);
    if (bwd_name(c->scene, sizeof c->scene, p + 12, 16) ||
        ((p[28]||!c->record.generated||strncasecmp(name,"RND",3))&&
         fixed_name(c->briefing, sizeof c->briefing, p + 28, 16))) {
        fail(error, n, "%s has an invalid scene or briefing name", name); goto bad;
    }
    if (p[136] && fixed_name(c->faction, sizeof c->faction, p + 136, 16)) {
        fail(error, n, "%s has an invalid faction emblem name", name); goto bad;
    }
    /* The original advances over a 0xa0-byte full CPGN header, then the
     * tagged subchunks. Their lengths include their own eight-byte headers. */
    for (off = 0x98; off < size;) {
        const unsigned char *q;
        size_t length, payload;
        if (size - off < 8 || (length = u32(p + off + 4)) < 8 || length > size - off) {
            /* Same exact-payload policy as the durable campaign loader: the
             * supplied BET4's incomplete final EQUP has no complete row. */
            if ((size_t)(p + off - c->record.data) == 1812 &&
                prj_known_mercs_campaign_tail(&c->record)) break;
            fail(error, n, "%s has a malformed CPGN subchunk", name); goto bad;
        }
        q = p + off + 8; payload = length - 8;
        if (!memcmp(p + off, "SLST", 4)) {
            if (payload % 16 || payload / 16 > MERCS_SHELL_MAX_MISSIONS || c->next_count) {
                fail(error, n, "%s has an unsupported contract list", name); goto bad;
            }
            c->next_count = (int)(payload / 16);
            for (i = 0; i < c->next_count; i++)
                if (fixed_name(c->next[i], sizeof c->next[i], q + i * 16, 16)) {
                    fail(error, n, "%s has an invalid contract reference", name); goto bad;
                }
        } else if (!memcmp(p + off, "CTNT", 4)) {
            c->contract = q; c->contract_size = payload;
        } else if (!memcmp(p + off, "NEWS", 4)) {
            c->news = q; c->news_size = payload;
        } else if (!memcmp(p + off, "SMHL", 4)) {
            if (payload % 28) { fail(error, n, "%s has malformed SMHL entries", name); goto bad; }
            c->starter = q; c->starter_size = payload;
        } else if (!memcmp(p + off, "MHLT", 4)) {
            if (payload % 24) { fail(error, n, "%s has malformed MHLT entries", name); goto bad; }
            c->available_mechs = q; c->available_mechs_size = payload;
        }
        off += length;
    }
    return 0;
bad:
    free(chunks);
    campaign_free(c);
    return -1;
}

static int append(char *out, size_t capacity, size_t *used, const char *s, size_t n)
{
    if (n >= capacity - *used) return -1;
    memcpy(out + *used, s, n); *used += n; out[*used] = 0;
    return 0;
}

static int raw_text(char *out, size_t capacity, const unsigned char *p, size_t n)
{
    size_t length = 0;
    while (length < n && p[length]) length++;
    if (length >= capacity) return -1;
    if (length) memcpy(out, p, length);
    out[length] = 0;
    return 0;
}

/* The original embeds physical line wrapping alongside backslash formatting.
 * Keep the text, expand name/tab/newline/page codes, and remove alignment codes.
 * This is plain-text presentation, not a reconstruction of the rich-text UI. */
static int plain_text(char *out, size_t capacity, const unsigned char *p,
                      size_t n, const char *pilot)
{
    size_t i, used = 0;
    out[0] = 0;
    for (i = 0; i < n && p[i]; i++) {
        unsigned char c = p[i];
        if (c == '\\' && i + 1 < n && p[i + 1]) {
            unsigned char code = p[++i];
            if (code == 'c' || code == 'l' || code == 'r') continue;
            if (code == 'n' || code == 's') {
                while (used && out[used - 1] == ' ') out[--used] = 0;
                if (append(out, capacity, &used, code == 's' ? "\n\n" : "\n", code == 's' ? 2 : 1)) return -1;
                continue;
            }
            if (code == 't') { if (append(out, capacity, &used, "    ", 4)) return -1; continue; }
            if (code == 'p') { if (append(out, capacity, &used, pilot, strlen(pilot))) return -1; continue; }
            if (append(out, capacity, &used, "\\", 1)) return -1;
            c = code;
        }
        if (c == '\r' || c == '\n') {
            if (!used || out[used - 1] == '\n' || out[used - 1] == ' ') continue;
            c = ' ';
        }
        if (c == 0x91 || c == 0x92) c = '\'';
        if (c == 0x93 || c == 0x94) c = '"';
        if (c == 0x96 || c == 0x97) c = '-';
        if (c < 32 && c != '\t') continue;
        if (c >= 128) c = '?';
        if (append(out, capacity, &used, (const char *)&c, 1)) return -1;
    }
    while (used && (out[used - 1] == ' ' || out[used - 1] == '\n')) out[--used] = 0;
    return 0;
}

static int briefing_read(prj_archive *a, const char *scene, const char *pilot,
                         char *out, size_t capacity, char *raw, size_t raw_capacity,
                         char *error, size_t n)
{
    prj_record r;
    bwd_chunk *chunks = NULL;
    char name[9] = "____BRF1";
    size_t used = 0, raw_used = 0;
    int i, count, found = 0;
    out[0] = raw[0] = 0;
    /* DOS0x622d8 derives Situation text from the scene's first four
     * characters, stopping at a dot and padding with underscores. CPGN's
     * briefing field names the separate AV movie (SIG2 uses NODATA). */
    for (i=0;i<4 && scene[i] && scene[i]!='.';i++)
        name[i]=(char)toupper((unsigned char)scene[i]);
    if (prj_read_named(a, "BWD", name, &r) != PRJ_OK) return fail(error, n, "Missing briefing %s", name);
    count = chunks_of(&r, &chunks, error, n);
    if (count < 0) { prj_record_free(&r); return -1; }
    for (i = 0; i < count; i++) if (!strcmp(chunks[i].tag, "ORDR")) {
        if ((found && (append(out, capacity, &used, "\n\n", 2) ||
                       append(raw, raw_capacity, &raw_used, "\\n\\n", 4))) ||
            plain_text(out + used, capacity - used, chunks[i].data, chunks[i].size, pilot) ||
            raw_text(raw + raw_used, raw_capacity - raw_used, chunks[i].data, chunks[i].size)) {
            fail(error, n, "Briefing %s exceeds the text buffer", name); goto bad;
        }
        used = strlen(out); raw_used = strlen(raw); found++;
    }
    free(chunks); prj_record_free(&r);
    return found ? 0 : fail(error, n, "Briefing %s has no ORDR text", name);
bad:
    free(chunks); prj_record_free(&r); return -1;
}

static int mission_check(prj_archive *a, const char *scene, char *error, size_t n)
{ return mercs_shell_scene_player(a, scene, NULL, error, n) < 0 ? -1 : 0; }

static int user_name(char out[22], const char *s, int uppercase,
                     char *error, size_t n, const char *label)
{
    size_t i, len;
    if (!s || !(len = strlen(s)) || len > 21) return fail(error, n, "%s must contain 1 to 21 characters", label);
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 32 || c > 126) return fail(error, n, "%s currently supports printable ASCII characters", label);
        out[i] = uppercase ? (char)toupper(c) : (char)c;
    }
    out[len] = 0;
    if (strspn(out, " ") == len) return fail(error, n, "%s cannot be blank", label);
    return 0;
}

/* Original MW2SHELL.DLL chassis table0x10083d8c, rows70/16/20. The names
 * below resolve the exact three authored NOVICE1 designs; availability and
 * order still come from that campaign's MHLT, never from this lookup table. */
static const mercs_shell_mech training_chassis[] = {
    {"PLY00STD", "PLY00MER", "COMMANDO", "Commando"},
    {"JEN00STD", "JEN00MER", "ISJENNER", "Jenner"},
    {"PNT00STD", "PNT00MER", "PANTHER", "Panther"}
};

static int training_choices(prj_archive *a, const campaign *c,
                             mercs_shell_mech out[MERCS_SHELL_TRAINING_MECHS],
                             int *out_count, char *error, size_t error_size)
{
    int count = (int)(c->available_mechs_size / 24), i, j;
    *out_count = 0;
    if (count < 1 || count > MERCS_SHELL_TRAINING_MECHS || c->available_mechs_size % 24)
        return fail(error, error_size, "Unsupported training mech list in %s", c->record.name);
    for (i = 0; i < count; i++) {
        char stock[9];
        prj_record r;
        mek_def def;
        int parsed;
        if (fixed_name(stock, sizeof stock, c->available_mechs + i * 24, 16))
            return fail(error, error_size, "Invalid training mech reference in %s", c->record.name);
        for (j = 0; j < MERCS_SHELL_TRAINING_MECHS; j++)
            if (!strcmp(stock, training_chassis[j].stock)) break;
        if (j == MERCS_SHELL_TRAINING_MECHS)
            return fail(error, error_size, "Training design %s needs a verified chassis mapping", stock);
        out[i] = training_chassis[j];
        for (j = 0; j < i; j++) if (!strcmp(stock, out[j].stock))
            return fail(error, error_size, "Duplicate training design %s", stock);
        if (prj_find_id(a, prj_find_type(a, "BWD"), out[i].skeleton) < 0)
            return fail(error, error_size, "Missing training chassis %s", out[i].skeleton);
        if (prj_read_named(a, "MEK", stock, &r) != PRJ_OK)
            return fail(error, error_size, "Missing training design %s", stock);
        parsed = mek_parse(r.data, r.size, &def);
        prj_record_free(&r);
        if (parsed) return fail(error, error_size, "Unsupported training design %s", stock);
    }
    *out_count = count;
    return 0;
}

int mercs_shell_register(prj_archive *a, mercs_shell_state *state,
                         int mode, const char *pilot, const char *unit,
                         char *error, size_t error_size)
{
    campaign initial;
    mercs_shell_state *s;
    int i;
    if (error && error_size) error[0] = 0;
    if (!a || !state || (mode != 0 && mode != 1)) return fail(error, error_size, "Unsupported registration mode");
    s = calloc(1, sizeof *s);
    if (!s) return fail(error, error_size, "Out of memory registering pilot");
    s->mode = mode;
    s->assigned_index = -1;
    s->owned_index = -1;
    if (user_name(s->pilot, pilot, 1, error, error_size, "Call Sign") ||
        user_name(s->unit, unit, 0, error, error_size, "Unit Name")) goto bad_state;
    snprintf(s->initial_campaign, sizeof s->initial_campaign, "%s", mode ? "CAMP" : "NOVICE");
    if (campaign_read(a, s->initial_campaign, &initial, error, error_size)) goto bad_state;
    /* Original loading order consumes one shared identity per MHLT/SMHL
     * design, then registration creates the player. PTLT initially uses-1.
     * CAMP:11 MHLT,1 SMHL ->mech11/player12. NOVICE:18+1 ->player19;
     * NOVICE1 then loads PLY20/JEN21/PNT22. Do not silently reuse these IDs
     * if a different authored initial campaign changes that allocation. */
    if (initial.available_mechs_size != (size_t)(mode ? 11 : 18) * 24 ||
        initial.starter_size != 28) {
        fail(error, error_size, "Unsupported initial owned-mech identity allocation"); goto bad_campaign;
    }
    s->pilot_instance_id = mode ? 12u : 19u;
    s->year = initial.year; s->month = initial.month;
    s->funds = mode ? initial.success_pay : 0;
    if (plain_text(s->news, sizeof s->news, initial.news, initial.news_size, s->pilot) ||
        raw_text(s->news_raw, sizeof s->news_raw, initial.news, initial.news_size)) {
        fail(error, error_size, "Initial campaign news is too long"); goto bad_campaign;
    }
    if (mode) {
        prj_record starter;
        mek_def def;
        int rc;
        /* New-career regular transfers CAMP's SMHL into ownership. Row70 of
         * the original chassis table maps the PLY prefix to COMMANDO. */
        if (initial.starter_size != 28 ||
            fixed_name(s->starter_stock, sizeof s->starter_stock, initial.starter, 16) ||
            strcmp(s->starter_stock, "PLY00STD")) {
            fail(error, error_size, "Unsupported original initial-mech allocation"); goto bad_campaign;
        }
        if (prj_read_named(a, "MEK", s->starter_stock, &starter) != PRJ_OK) {
            fail(error, error_size, "Missing initial mech %s", s->starter_stock); goto bad_campaign;
        }
        rc = mek_parse(starter.data, starter.size, &def);
        if (!rc) rc = mercs_career_pair_init(&s->owned[0], starter.data, starter.size,
                                             "PLY00MER", 11, -1);
        prj_record_free(&starter);
        if (rc) { fail(error, error_size, "Unsupported initial mech %s", s->starter_stock); goto bad_campaign; }
        strcpy(s->starter_loadout, "PLY00MER");
        strcpy(s->starter_skeleton, "COMMANDO");
        strcpy(s->starter_name, "Commando");
        mercs_career_set_acquisition(&s->owned[0].current, (int32_t)u32(initial.starter + 20),
                                     (int32_t)u32(initial.starter + 24), (int32_t)u32(initial.starter + 16));
        s->owned[0].reference = s->owned[0].current;
        mercs_career_set_association(&s->owned[0].current, (int32_t)s->pilot_instance_id);
        s->owned_count = 1; s->owned_index = 0;
        if (prj_find_id(a, prj_find_type(a, "BWD"), s->starter_skeleton) < 0) {
            fail(error, error_size, "Missing initial Commando skeleton"); goto bad_campaign;
        }
    } else strcpy(s->starter_name, "Unassigned");
    s->mission_count = initial.next_count;
    for (i = 0; i < s->mission_count; i++) {
        campaign next;
        mercs_shell_mission *m = &s->missions[i];
        const char *line;
        size_t len;
        strcpy(m->campaign, initial.next[i]);
        if (campaign_read(a, m->campaign, &next, error, error_size)) goto bad_campaign;
        strcpy(m->scene, next.scene); strcpy(m->briefing_smk, next.briefing);
        strcpy(m->faction, next.faction);
        m->campaign_type = next.kind; m->year = next.year; m->month = next.month;
        if (plain_text(m->contract, sizeof m->contract, next.contract, next.contract_size, s->pilot) ||
            raw_text(m->contract_raw, sizeof m->contract_raw, next.contract, next.contract_size)) {
            campaign_free(&next); fail(error, error_size, "Contract %s is too long", m->campaign); goto bad_campaign;
        }
        if (!mode && !strcmp(m->campaign, "NOVICE1") &&
            training_choices(a, &next, s->training_mechs, &s->training_mech_count, error, error_size)) {
            campaign_free(&next); goto bad_campaign;
        }
        if (!mode && !strcmp(m->campaign, "NOVICE1")) {
            int choice;
            for (choice = 0; choice < s->training_mech_count; choice++) {
                prj_record stock;
                const unsigned char *entry = next.available_mechs + choice * 24;
                if (prj_read_named(a, "MEK", s->training_mechs[choice].stock, &stock) != PRJ_OK) {
                    campaign_free(&next); fail(error, error_size, "Missing initial training mech"); goto bad_campaign;
                }
                if (mercs_career_pair_init(&s->owned[choice], stock.data, stock.size,
                                           s->training_mechs[choice].loadout, (uint32_t)(20 + choice), -1)) {
                    prj_record_free(&stock); campaign_free(&next);
                    fail(error, error_size, "Cannot create the original training mech record"); goto bad_campaign;
                }
                prj_record_free(&stock);
                mercs_career_set_acquisition(&s->owned[choice].current, (int32_t)u32(entry + 16),
                                             (int32_t)u32(entry + 20), 0);
                s->owned[choice].reference = s->owned[choice].current;
            }
            s->owned_count = s->training_mech_count;
        }
        campaign_free(&next);
        line = m->contract + strspn(m->contract, " \n\t");
        len = strcspn(line, "\n");
        if (len >= sizeof m->title) len = sizeof m->title - 1;
        memcpy(m->title, line, len); m->title[len] = 0;
        if (!m->title[0]) strcpy(m->title, m->campaign);
        if (!strncasecmp(m->campaign,"RAN",3)) {
            mercs_shell_mission loaded;
            if(mercs_shell_mission_load(a,m->campaign,s->pilot,&loaded,error,error_size))goto bad_campaign;
            *m=loaded;
            continue;
        }
        if (briefing_read(a, m->scene, s->pilot, m->briefing, sizeof m->briefing,
                          m->briefing_raw, sizeof m->briefing_raw,
                          m->unavailable, sizeof m->unavailable)) continue;
        if (!mode) {
            if (mission_check(a, m->scene, m->unavailable, sizeof m->unavailable)) continue;
            strcpy(m->unavailable, "Assign your pilot a 'Mech with PREV or NEXT in the Duty Roster before launching.");
            continue;
        }
        if (m->campaign_type < 0 || m->campaign_type > 4) {
            strcpy(m->unavailable, "This resource is a campaign transition rather than a mission."); continue;
        }
        if (mission_check(a, m->scene, m->unavailable, sizeof m->unavailable)) continue;
        m->launchable = 1;
    }
    campaign_free(&initial);
    s->registered = 1;
    *state = *s;
    free(s);
    return 0;
bad_campaign:
    campaign_free(&initial);
bad_state:
    free(s); return -1;
}

int mercs_shell_assign(prj_archive *a, mercs_shell_state *state,
                       int mission_index, int choice,
                       char *error, size_t error_size)
{
    mercs_shell_mission *m;
    if (error && error_size) error[0] = 0;
    if (!a || !state || !state->registered || state->mode != 0 ||
        mission_index < 0 || mission_index >= state->mission_count ||
        mission_index >= MERCS_SHELL_MAX_MISSIONS)
        return fail(error, error_size, "Select a registered training assignment first");
    if (choice < 0 || choice >= state->owned_count)
        return fail(error, error_size, "Select an owned training mech in the Duty Roster");
    m = &state->missions[mission_index];
    if (mission_check(a, m->scene, error, error_size)) return -1;
    /* Full career progression has already transferred the actual MHLT rows.
     * Assign by retained ownership, including later training contracts and
     * customized designs, rather than rebuilding the NOVICE1 stock trio. */
    if (mercs_shell_assign_owned(a,state,choice,error,error_size)) return -1;
    m->launchable = 1; m->unavailable[0] = 0;
    return 0;
}

void mercs_shell_unassign(mercs_shell_state *state, int mission_index)
{
    mercs_shell_mission *m;
    if (!state || !state->registered || state->mode != 0 || mission_index < 0 ||
        mission_index >= state->mission_count || mission_index >= MERCS_SHELL_MAX_MISSIONS) return;
    m = &state->missions[mission_index];
    if (state->owned_index >= 0 && state->owned_index < state->owned_count)
        mercs_career_set_association(&state->owned[state->owned_index].current, -1);
    state->owned_index = -1;
    state->assigned_index = -1;
    state->starter_stock[0] = state->starter_loadout[0] = state->starter_skeleton[0] = 0;
    strcpy(state->starter_name, "Unassigned");
    m->launchable = 0;
    snprintf(m->unavailable, sizeof m->unavailable,
             "Assign your pilot a 'Mech with PREV or NEXT in the Duty Roster before launching.");
}

/* Both original engines share same-type ammunition in one 58-byte pool.
 * Index eleven overwrites its trailing pointer; later indices corrupt the
 * next pool. The original editor still admits that layout. Refuse it before
 * a native mission can run without a serializable return; keep editing and
 * ownership unchanged. See tests/fixtures/ammo-pool-capacity/original/. */
static int deployment_ammo_check(const unsigned char *bytes,size_t size,
                                 const char *name,char *error,size_t error_size)
{
    mek_def parsed;
    int i,j;
    if(mek_parse(bytes,size,&parsed))
        return fail(error,error_size,"Cannot read %s's deployed ammunition",name);
    /* Count every link, including ammunition without a surviving mount.
     * Comparing types directly avoids indexing with an untrusted linked ID. */
    for(i=0;i<parsed.link_count;i++){
        int type=parsed.items[parsed.item_count+i].linked_id/100,count=1;
        for(j=0;j<i;j++)
            if(parsed.items[parsed.item_count+j].linked_id/100==type)count++;
        if(count>MERCS_RETURN_POOL_BINS)
            return fail(error,error_size,
                "%s exceeds the limit of %d ammunition bins per weapon type. Remove excess bins in the Mech Lab before launching.",
                name,MERCS_RETURN_POOL_BINS);
    }
    return 0;
}

int mercs_shell_prepare_roster(prj_archive *a,mercs_shell_state *state,
                              int mission_index,const char *overlay,
                              const mercs_shell_deploy_pilot *pilots,size_t pilot_count,
                              const mercs_shell_deploy_aircraft *aircraft,
                              mercs_exchange_launch *manifest,char *error,size_t error_size)
{
    const mercs_shell_mission *m;
    lance_mercs_slot slots[4];
    mercs_exchange_unit units[3];
    char mech_path[32],mech_dir[2048],names[4][MERCS_CAREER_FILENAME_BYTES];
    unsigned char star[LANCE_MERCS_STAR_MAX_BYTES],mech[4][MERCS_CAREER_DEPLOY_MAX_BYTES];
    size_t star_size,mech_size[4],i,j,total=pilot_count+(aircraft!=NULL);
    int stock_player;
    if(error&&error_size)error[0]=0;
    if(!a||!state||!state->registered||!overlay||!overlay[0]||!pilots||!manifest||
       pilot_count<1||pilot_count>3||mission_index<0||mission_index>=state->mission_count||
       mission_index>=MERCS_SHELL_MAX_MISSIONS)
        return fail(error,error_size,"No registered pilot/mission selected");
    m=&state->missions[mission_index];
    if(!m->launchable)return fail(error,error_size,"%s",m->unavailable);
    if((state->mode!=0&&state->mode!=1)||state->owned_index<0||
       state->owned_index>=state->owned_count||state->owned_count>MERCS_SHELL_MAX_OWNED||
       pilots[0].owned_index!=state->owned_index||pilots[0].pilot_instance_id!=state->pilot_instance_id)
        return fail(error,error_size,"Assign your pilot an owned mech before launching");
    stock_player=mercs_shell_scene_player(a,m->scene,NULL,error,error_size);
    if(stock_player<0)return -1;
    memset(slots,0,sizeof slots);memset(units,0,sizeof units);
    for(i=0;i<pilot_count;i++){
        mercs_shell_mech design;const mercs_career_mech *current;
        if(pilots[i].owned_index<0||pilots[i].owned_index>=state->owned_count||
           pilots[i].pilot_instance_id>INT32_MAX||!memchr(pilots[i].name,0,sizeof pilots[i].name)||!pilots[i].name[0])
            return fail(error,error_size,"The selected pilot identity is invalid");
        for(j=0;j<i;j++)if(pilots[j].owned_index==pilots[i].owned_index||pilots[j].pilot_instance_id==pilots[i].pilot_instance_id)
            return fail(error,error_size,"Two deployed pilots share an assignment");
        if(mercs_shell_describe_owned(a,&state->owned[pilots[i].owned_index],&design,error,error_size))return -1;
        current=&state->owned[pilots[i].owned_index].current;
        if(mercs_career_association_id(current)!=(int32_t)pilots[i].pilot_instance_id)
            return fail(error,error_size,"The selected pilot and mech assignment disagree");
        if(!i&&(strcmp(state->starter_stock,design.stock)||strcmp(state->starter_loadout,design.loadout)||
                strcmp(state->starter_skeleton,design.skeleton)))
            return fail(error,error_size,"The player assignment is inconsistent");
        if(mercs_career_filename(current,names[i],sizeof names[i])||owned_basename(names[i])||strcmp(names[i],design.loadout))
            return fail(error,error_size,"The assigned mech identity is inconsistent");
        for(j=0;j<i;j++)if(!strcmp(names[j],names[i]))return fail(error,error_size,"Deployed mech filenames collide");
        if(mercs_career_deployment_bytes(current,mech[i],sizeof mech[i],&mech_size[i]))
            return fail(error,error_size,"Cannot serialize the assigned mech's current condition");
        /* Fixed-player scenes never instantiate these owned files. Their
         * existing untouched deployment/import path must remain available. */
        if(!stock_player&&deployment_ammo_check(mech[i],mech_size[i],design.name,error,error_size))return -1;
        slots[i].active=slots[i].has_attributes=1;
        strcpy(slots[i].skeleton,design.skeleton);strcpy(slots[i].loadout,design.loadout);strcpy(slots[i].name,pilots[i].name);
        for(j=0;j<8;j++)slots[i].attributes[j]=pilots[i].attributes[j];
        units[i].actor_index=-(int32_t)i-1;units[i].controller_flags=2;units[i].alliance=0;
        strcpy(units[i].name,design.loadout);
    }
    if(aircraft){
        prj_record raw={0};mercs_career_mech_pair temporary;char filename[16];int rc;
        if(!memchr(aircraft->stock,0,sizeof aircraft->stock)||strlen(aircraft->stock)!=8||
           strncasecmp(aircraft->stock,"ARO",3)||strcasecmp(aircraft->stock+5,"STD")||
           !memchr(aircraft->name,0,sizeof aircraft->name)||!aircraft->name[0])
            return fail(error,error_size,"The selected AeroTech aircraft identity is invalid");
        snprintf(names[pilot_count],sizeof names[pilot_count],"%.5sMER",aircraft->stock);
        if(owned_basename(names[pilot_count]))return fail(error,error_size,"The selected AeroTech filename is invalid");
        for(i=0;i<pilot_count;i++)if(!strcmp(names[i],names[pilot_count]))
            return fail(error,error_size,"The AeroTech and owned-mech filenames collide");
        snprintf(filename,sizeof filename,"%.8s.MEK",names[pilot_count]);
        if(prj_read_named(a,"MEK",aircraft->stock,&raw))
            return fail(error,error_size,"The selected AeroTech stock is unavailable");
        rc=mercs_career_pair_init(&temporary,raw.data,raw.size,filename,aircraft->temporary_instance_id,-1);
        prj_record_free(&raw);
        if(rc||mercs_career_deployment_bytes(&temporary.current,mech[pilot_count],sizeof mech[pilot_count],&mech_size[pilot_count]))
            return fail(error,error_size,"Cannot serialize the selected AeroTech aircraft");
        slots[pilot_count].active=slots[pilot_count].has_attributes=1;
        /* DOS0xb8bf0's first ARO entry is STUKA, as in0x5e250. */
        strcpy(slots[pilot_count].skeleton,"STUKA");strcpy(slots[pilot_count].loadout,names[pilot_count]);
        strcpy(slots[pilot_count].name,aircraft->name);
        for(i=0;i<8;i++)slots[pilot_count].attributes[i]=aircraft->attributes[i];
    }
    if(lance_mercs_star_bytes(a,slots,(int)total,0,0,star,sizeof star,&star_size))
        return fail(error,error_size,"Cannot encode Mercenaries USERSTAR");
    if(dp_set_write_root(overlay))return fail(error,error_size,"Cannot use launch overlay %s",overlay);
    if(mercs_file_directory(overlay,"MEK",mech_dir,sizeof mech_dir,error,error_size))return -1;
    for(i=0;i<total;i++){
        snprintf(mech_path,sizeof mech_path,"%.8s.MEK",names[i]);
        if(mercs_atomic_write(mech_dir,mech_path,mech[i],mech_size[i],error,error_size))return -1;
    }
    /* Each prevalidated record is replaced through an exclusive same-directory
     * temporary file. The edition lock prevents a child observing half a lance. */
    if(mercs_atomic_write(overlay,"USERSTAR.BWD",star,star_size,error,error_size))return -1;
    for(i=0;i<pilot_count;i++)mercs_career_set_deployed(&state->owned[pilots[i].owned_index].current,1);
    manifest->unit_count=pilot_count;memcpy(manifest->units,units,pilot_count*sizeof *units);
    return 0;
}

int mercs_shell_prepare_lance(prj_archive *a,mercs_shell_state *state,
                              int mission_index,const char *overlay,
                              const mercs_shell_deploy_pilot *pilots,size_t pilot_count,
                              mercs_exchange_launch *manifest,char *error,size_t error_size)
{
    return mercs_shell_prepare_roster(a,state,mission_index,overlay,pilots,pilot_count,NULL,manifest,error,error_size);
}

int mercs_shell_prepare_launch(prj_archive *a,mercs_shell_state *state,
                               int mission_index,const char *overlay,char *error,size_t error_size)
{
    mercs_shell_deploy_pilot player;mercs_exchange_launch manifest={0};int i;
    if(!state)return fail(error,error_size,"No registered pilot/mission selected");
    memset(&player,0,sizeof player);player.owned_index=state->owned_index;player.pilot_instance_id=state->pilot_instance_id;
    memcpy(player.name,state->pilot,sizeof player.name);for(i=0;i<8;i++)player.attributes[i]=-1;
    return mercs_shell_prepare_lance(a,state,mission_index,overlay,&player,1,&manifest,error,error_size);
}

int mercs_shell_apply_return(mercs_shell_state *state,int mission_index,
                             const mercs_exchange_launch *launch,const mercs_exchange_result *result,
                             mercs_career_import_result *imported,char *error,size_t error_size)
{
    mercs_career_mech_pair pairs[3];mercs_career_import_result dispositions[3];
    mercs_car_record car;mercs_msn_record msn;
    int indices[3],matched[3]={-1,-1,-1};size_t i,j;int k;
    char owned_name[MERCS_CAREER_FILENAME_BYTES];
    if(error&&error_size)error[0]=0;
    if(!state||!launch||!result||!imported||!state->registered||
       !memchr(launch->token,0,sizeof launch->token)||strlen(launch->token)!=32||
       !memchr(launch->scene,0,sizeof launch->scene)||mission_index<0||mission_index>=state->mission_count||
       state->owned_index<0||state->owned_index>=state->owned_count||state->owned_count>MERCS_SHELL_MAX_OWNED||
       launch->unit_count<1||launch->unit_count>3||
       strcasecmp(launch->scene,state->missions[mission_index].scene)||!strcmp(launch->token,state->last_return_token))
        return fail(error,error_size,"This mission return does not match the active deployment");
    if(mercs_car_parse(result->career,sizeof result->career,&car)||
       mercs_msn_parse(result->mission,sizeof result->mission,&msn)||result->mech_count>launch->unit_count)
        return fail(error,error_size,"The mission's returned records are inconsistent");
    for(i=0;i<launch->unit_count;i++){
        const mercs_exchange_unit *unit=&launch->units[i];int found=-1;
        if(unit->actor_index!=-(int32_t)i-1||!(unit->controller_flags&2)||unit->alliance!=0||
           !memchr(unit->name,0,sizeof unit->name)||!unit->name[0])
            return fail(error,error_size,"The deployed roster identity is invalid");
        for(k=0;k<state->owned_count;k++){
            if(mercs_career_filename(&state->owned[k].current,owned_name,sizeof owned_name)||owned_basename(owned_name))continue;
            if(!strcasecmp(owned_name,unit->name)){
                if(found>=0)return fail(error,error_size,"Two owned mechs share a deployed filename");
                found=k;
            }
        }
        if(found<0||(!i&&found!=state->owned_index)||mercs_career_deployed(&state->owned[found].current)!=1||
           mercs_career_association_id(&state->owned[found].current)<0)
            return fail(error,error_size,"The returned owned mech does not match its deployment");
        for(j=0;j<i;j++)if(indices[j]==found)return fail(error,error_size,"A deployed owned mech appears twice");
        indices[i]=found;pairs[i]=state->owned[found];
    }
    for(i=0;i<result->mech_count;i++){
        const mercs_exchange_mek *mek=&result->mechs[i];
        if(!memchr(mek->name,0,sizeof mek->name)||mek->size>sizeof mek->bytes)
            return fail(error,error_size,"The returned mech record is invalid");
        for(j=0;j<launch->unit_count;j++)
            if(mek->actor_index==launch->units[j].actor_index&&!strcasecmp(mek->name,launch->units[j].name))break;
        if(j==launch->unit_count||matched[j]>=0)
            return fail(error,error_size,"The returned mech identity does not match its deployment");
        matched[j]=(int)i;
    }
    /* Every current/reference pair is staged. One malformed mate record cannot
     * leave the player's damage imported or any deployment flag cleared. */
    for(i=0;i<launch->unit_count;i++){
        const mercs_exchange_mek *mek=matched[i]<0?NULL:&result->mechs[matched[i]];
        if(mercs_career_import_return(&pairs[i],0,mek?mek->bytes:NULL,mek?mek->size:0,&dispositions[i]))
            return fail(error,error_size,"Cannot import the returned mech's damage and equipment");
    }
    for(i=0;i<launch->unit_count;i++)state->owned[indices[i]]=pairs[i];
    state->last_car=car;state->last_msn=msn;state->last_return_valid=1;state->last_return_mission=mission_index;
    snprintf(state->last_return_token,sizeof state->last_return_token,"%s",launch->token);
    *imported=dispositions[0];return 0;
}

int mercs_shell_keep_owned_state(mercs_shell_state *to, const mercs_shell_state *from,
                                  char *error, size_t error_size)
{
    int i, j, map[MERCS_SHELL_MAX_OWNED], return_index = -1;
    char a[MERCS_CAREER_FILENAME_BYTES], b[MERCS_CAREER_FILENAME_BYTES];
    if (error && error_size) error[0] = 0;
    if (!to || !from || !to->registered || !from->registered || to->mode != from->mode ||
        strcmp(to->pilot, from->pilot) || strcmp(to->unit, from->unit) ||
        to->pilot_instance_id != from->pilot_instance_id || to->owned_count != from->owned_count ||
        from->owned_count < 1 || from->owned_count > MERCS_SHELL_MAX_OWNED ||
        from->owned_index < -1 || from->owned_index >= from->owned_count)
        return fail(error, error_size, "The selected look has different ownership data");
    for (i = 0; i < from->owned_count; i++) {
        if (mercs_career_filename(&from->owned[i].current, a, sizeof a))
            return fail(error, error_size, "The owned mech identity is invalid");
        for (j = 0; j < to->owned_count; j++) {
            if (mercs_career_filename(&to->owned[j].current, b, sizeof b)) continue;
            if (!strcmp(a, b) && mercs_career_instance_id(&from->owned[i].current) ==
                                mercs_career_instance_id(&to->owned[j].current)) break;
        }
        if (j == to->owned_count) return fail(error, error_size, "An owned mech is unavailable with that look");
        { int k; for (k = 0; k < i; k++) if (map[k] == j)
            return fail(error, error_size, "The selected look contains duplicate owned identities"); }
        map[i] = j;
    }
    if (from->last_return_valid) {
        if (from->last_return_mission < 0 || from->last_return_mission >= from->mission_count)
            return fail(error, error_size, "The last mission identity is invalid");
        for (i = 0; i < to->mission_count; i++)
            if (!strcmp(to->missions[i].campaign, from->missions[from->last_return_mission].campaign)) {
                return_index = i; break;
            }
        if (return_index < 0) return fail(error, error_size, "The last contract is unavailable with that look");
    }
    for (i = 0; i < from->owned_count; i++) to->owned[map[i]] = from->owned[i];
    to->owned_index = from->owned_index < 0 ? -1 : map[from->owned_index];
    to->funds = from->funds; to->year = from->year; to->month = from->month;
    to->last_return_valid = from->last_return_valid; to->last_return_mission = return_index;
    to->last_car = from->last_car; to->last_msn = from->last_msn;
    memcpy(to->last_return_token, from->last_return_token, sizeof to->last_return_token);
    return 0;
}

int mercs_shell_mission_load_seeded(prj_archive *a, const char *name, const char *pilot,
                                    int year,uint32_t seed,mercs_shell_mission *out,char *error,size_t n)
{
    campaign c,random;campaign *display=&c;
    mercs_rmg_bundle bundle={0};prj_record *previous=NULL;size_t previous_count=0;int staged=0,retry=0;
    mercs_shell_mission *m;
    const char *line;
    size_t len;
    char normalized[16];
    if (!a || !name || !pilot || !out ||
        fixed_name(normalized, sizeof normalized, (const unsigned char *)name,
                   strlen(name))) return fail(error,n,"Invalid campaign reference");
    if (campaign_read(a,normalized,&c,error,n)) return -1;
    m = calloc(1,sizeof *m);
    if (!m) { campaign_free(&c); return fail(error,n,"Out of memory reading campaign"); }
    strcpy(m->campaign,normalized); strcpy(m->scene,c.scene);
    strcpy(m->briefing_smk,c.briefing); strcpy(m->faction,c.faction);
    m->campaign_type=c.kind; m->year=c.year; m->month=c.month;
    memset(&random,0,sizeof random);
    if(!strncmp(normalized,"RAN",3)){
        if(year<3044)year=c.year;
        m->procedural_seed=seed;m->procedural_year=year;
        int generated=mercs_rmg_campaign(a,normalized,year,seed,&bundle,m->unavailable,sizeof m->unavailable);
        if(generated){
            retry=generated==MERCS_RMG_BAD_SEED;
            /* Missing original data is shown inside the contract screen. */
        }else if(prj_get_generated(a,&previous,&previous_count)){
            snprintf(m->unavailable,sizeof m->unavailable,"Out of memory staging procedural mission resources.");
        }else if(mercs_rmg_install(a,&bundle)){
            prj_free_generated(previous,previous_count);previous=NULL;previous_count=0;
            snprintf(m->unavailable,sizeof m->unavailable,"Cannot stage the generated mission resources.");
        }else{
            staged=1;
            if(campaign_read(a,bundle.companion,&random,m->unavailable,sizeof m->unavailable)==0){
                display=&random;snprintf(m->scene,sizeof m->scene,"%.15s",bundle.scene);
                snprintf(m->briefing_smk,sizeof m->briefing_smk,"RAN%dBRF1",bundle.config.number);
                snprintf(m->faction,sizeof m->faction,"%s",random.faction);
            }
        }
    }
    if (plain_text(m->contract,sizeof m->contract,display->contract,display->contract_size,pilot) ||
        raw_text(m->contract_raw,sizeof m->contract_raw,display->contract,display->contract_size)) {
        if(staged)prj_restore_generated(a,previous,previous_count);
        mercs_rmg_free(&bundle);campaign_free(&random);campaign_free(&c);free(m);return fail(error,n,"Campaign contract text is too long");
    }
    line=m->contract+strspn(m->contract," \n\t");len=strcspn(line,"\n");
    if(len>=sizeof m->title)len=sizeof m->title-1;
    memcpy(m->title,line,len);m->title[len]=0;
    if(!m->title[0])strcpy(m->title,normalized);
    if(m->unavailable[0]){ }
    else if(briefing_read(a,m->scene,pilot,m->briefing,sizeof m->briefing,
                         m->briefing_raw,sizeof m->briefing_raw,m->unavailable,sizeof m->unavailable)) { }
    else if(c.kind<0||c.kind>4)
        snprintf(m->unavailable,sizeof m->unavailable,"This campaign resource is a transition rather than a launchable mission.");
    else if(!mission_check(a,m->scene,m->unavailable,sizeof m->unavailable))m->launchable=1;
    if(staged)prj_restore_generated(a,previous,previous_count);
    mercs_rmg_free(&bundle);campaign_free(&random);campaign_free(&c);*out=*m;free(m);return retry?MERCS_RMG_BAD_SEED:0;
}

static uint32_t preview_seed(const char *name)
{
    uint32_t seed=(uint32_t)((uint64_t)time(NULL)%256000000u);size_t i;
    if(name)for(i=0;name[i];i++)seed=seed*33u+(unsigned char)toupper((unsigned char)name[i]);
    return seed%256000000u;
}
static int preview_mission(prj_archive *a,const char *name,const char *pilot,int year,
                           mercs_shell_mission *out,char *e,size_t n)
{
    uint32_t seed=preview_seed(name);int i,rc;
    for(i=0;i<16;i++){
        rc=mercs_shell_mission_load_seeded(a,name,pilot,year,seed,out,e,n);
        if(rc!=MERCS_RMG_BAD_SEED)return rc;
        seed=(seed+104729u)%256000000u;
    }
    return 0; /* unavailable reason retained; no invalid graph can launch */
}
int mercs_shell_mission_load(prj_archive *a,const char *name,const char *pilot,
                              mercs_shell_mission *out,char *e,size_t n)
{
    return preview_mission(a,name,pilot,0,out,e,n);
}

int mercs_shell_news_text(mercs_shell_state *s,const uint8_t *bytes,size_t length,
                           char *error,size_t n)
{
    char plain[MERCS_SHELL_TEXT], raw[MERCS_SHELL_TEXT];
    if(!s||(!bytes&&length)||length>4096)
        return fail(error,n,"Invalid saved news text");
    if(plain_text(plain,sizeof plain,bytes,length,s->pilot)||
       raw_text(raw,sizeof raw,bytes,length))
        return fail(error,n,"Saved news text is too long");
    snprintf(s->news,sizeof s->news,"%s",plain);
    snprintf(s->news_raw,sizeof s->news_raw,"%s",raw);
    return 0;
}

int mercs_shell_set_missions(prj_archive *a, mercs_shell_state *s,
                              const char names[][16], size_t count,
                              char *error, size_t n)
{
    mercs_shell_mission *missions;
    size_t i;
    if(!a||!s||!s->registered||count>MERCS_SHELL_MAX_MISSIONS||(count&&!names))
        return fail(error,n,"Invalid available-contract list");
    missions=calloc(MERCS_SHELL_MAX_MISSIONS,sizeof *missions);
    if(!missions)return fail(error,n,"Out of memory reading available contracts");
    for(i=0;i<count;i++)
        if(!memchr(names[i],0,16)||preview_mission(a,names[i],s->pilot,
             s->year,&missions[i],error,n)) {
            free(missions);return -1;
        }
    memcpy(s->missions,missions,sizeof s->missions);s->mission_count=(int)count;
    free(missions);return 0;
}

int mercs_shell_describe_owned(prj_archive *a,const mercs_career_mech_pair *pair,
                                mercs_shell_mech *out,char *error,size_t n)
{
    mercs_shell_mech result;
    char name[MERCS_CAREER_FILENAME_BYTES];
    size_t i;
    if(!a||!pair||!out||mercs_career_filename(&pair->current,name,sizeof name))
        return fail(error,n,"Invalid owned-mech identity");
    if(owned_basename(name))return fail(error,n,"Unsupported owned-mech filename");
    for(i=0;i<sizeof mercs_shell_chassis/sizeof mercs_shell_chassis[0];i++)
        if(!strncmp(name,mercs_shell_chassis[i].prefix,3))break;
    if(i==sizeof mercs_shell_chassis/sizeof mercs_shell_chassis[0]||mercs_shell_chassis[i].kind>1)
        return fail(error,n,"Unsupported owned chassis %.3s",name);
    memset(&result,0,sizeof result);
    snprintf(result.stock,sizeof result.stock,"%.3s00STD",name);
    strcpy(result.loadout,name);
    snprintf(result.skeleton,sizeof result.skeleton,"%s",mercs_shell_chassis[i].skeleton);
    snprintf(result.name,sizeof result.name,"%s",mercs_shell_chassis[i].name);
    if(prj_find_id(a,prj_find_type(a,"BWD"),result.skeleton)<0)
        return fail(error,n,"Missing owned chassis %s",result.skeleton);
    if(mercs_career_instance_id(&pair->current)!=mercs_career_instance_id(&pair->reference))
        return fail(error,n,"Owned current/reference identities differ");
    *out=result;return 0;
}

int mercs_shell_assign_owned(prj_archive *a,mercs_shell_state *s,int index,
                              char *error,size_t n)
{
    mercs_shell_mech design;
    int i;
    if(!a||!s||!s->registered||s->owned_count<0||s->owned_count>MERCS_SHELL_MAX_OWNED||
       index < -1||index>=s->owned_count)return fail(error,n,"Invalid owned-mech selection");
    if(index>=0&&mercs_shell_describe_owned(a,&s->owned[index],&design,error,n))return -1;
    for(i=0;i<s->owned_count;i++)
        if(mercs_career_association_id(&s->owned[i].current)==(int32_t)s->pilot_instance_id)
            mercs_career_set_association(&s->owned[i].current,-1);
    s->owned_index=index;
    if(s->mode==0)s->assigned_index=index;
    s->starter_stock[0]=s->starter_loadout[0]=s->starter_skeleton[0]=0;
    strcpy(s->starter_name,"Unassigned");
    if(index>=0){
        mercs_career_set_association(&s->owned[index].current,(int32_t)s->pilot_instance_id);
        strcpy(s->starter_stock,design.stock);strcpy(s->starter_loadout,design.loadout);
        strcpy(s->starter_skeleton,design.skeleton);strcpy(s->starter_name,design.name);
    }
    return 0;
}
