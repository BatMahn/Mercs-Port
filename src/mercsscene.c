/* Shared authored player validation for shell launch and simulator return. */
#include "mercsscene.h"
#include "mek.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int fail(char *error, size_t size, const char *format, ...)
{
    va_list ap;
    if (error && size) {
        va_start(ap,format);vsnprintf(error,size,format,ap);va_end(ap);
    }
    return -1;
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

/* Validate the archive plus the current native-generated graph. Loose files
 * from an earlier mission never make an incomplete contract launchable. */
int mercs_shell_scene_player(prj_archive *a, const char *scene,
                              bwd_gps *stock_player, char *error, size_t n)
{
    int ids[512], count = 1, head, bwd_type, mek_type, userstar = 0, players = 0;
    bwd_gps selected = {0};
    if (!a || !scene || !scene[0]) return fail(error, n, "Missing mission scene");
    bwd_type = prj_find_type(a, "BWD"); mek_type = prj_find_type(a, "MEK");
    if (bwd_type < 0 || mek_type < 0 || (ids[0] = prj_find_id(a, bwd_type, scene)) < 0)
        return fail(error, n, "Missing scene %s", scene);
    for (head = 0; head < count; head++) {
        prj_record r;
        bwd_chunk *chunks = NULL;
        int i, chunks_count;
        if (prj_read(a, bwd_type, ids[head], &r) != PRJ_OK)
            return fail(error, n, "Cannot read BWD id %d", ids[head]);
        chunks_count = chunks_of(&r, &chunks, error, n);
        if (chunks_count < 0) { prj_record_free(&r); return -1; }
        for (i = 0; i < chunks_count; i++) {
            bwd_chunk *c = &chunks[i];
            if (!strcmp(c->tag, "INCL")) {
                uint16_t id;
                char name[11];
                int j;
                if (bwd_incl(c, &id, name)) { fail(error, n, "Invalid INCL in %s", r.name); goto bad; }
                if (id == BWD_EXTERNAL_ID) {
                    if (!strcasecmp(name, "USERSTAR")) {userstar++;continue;}
                    j=prj_find_id(a,bwd_type,name);
                    if(j<0){fail(error,n,"Missing generated or authored external %s",name);goto bad;}
                    id=(uint16_t)j;
                }
                if(id==0xffff){j=prj_find_id(a,bwd_type,name);if(j<0){fail(error,n,"Missing named BWD %s",name);goto bad;}id=(uint16_t)j;}
                if (prj_find_id(a, bwd_type, name) != id) {
                    fail(error, n, "BWD reference %s does not match id %u", name, id); goto bad;
                }
                for (j = 0; j < count && ids[j] != id; j++) { }
                if (j == count) {
                    if (count == 512) { fail(error, n, "Mission include graph is too large"); goto bad; }
                    ids[count++] = id;
                }
            } else if (!strcmp(c->tag, "GPS")) {
                bwd_gps gps;
                prj_record mech;
                mek_def def;
                int parsed;
                if (bwd_gps_decode(c, &gps) ||
                    (gps.skeleton_id!=0xffff&&prj_find_id(a, bwd_type, gps.skeleton) != gps.skeleton_id) ||
                    (gps.mek_id!=0xffff&&prj_find_id(a, mek_type, gps.loadout) != gps.mek_id) ||
                    prj_find_id(a,bwd_type,gps.skeleton)<0) {
                    fail(error, n, "Unsupported actor reference in %s", r.name); goto bad;
                }
                if ((gps.mek_id==0xffff?prj_read_named(a,"MEK",gps.loadout,&mech):prj_read(a,mek_type,gps.mek_id,&mech)) != PRJ_OK) {
                    fail(error, n, "Cannot read actor loadout %s", gps.loadout); goto bad;
                }
                parsed = mek_parse(mech.data, mech.size, &def);
                prj_record_free(&mech);
                if (parsed) { fail(error, n, "Unsupported actor loadout %s", gps.loadout); goto bad; }
                if (gps.is_player) {
                    /* DOS0x5f26d loads STD designs from the archive without
                     * setting the owned-return flag (also verified on Windows).
                     * Require exact archive identities at this boundary. */
                    if (++players != 1 || strlen(gps.loadout) != 8 ||
                        strcasecmp(gps.loadout + 5, "STD") ||
                        prj_find_id(a, bwd_type, gps.skeleton) != gps.skeleton_id ||
                        prj_find_id(a, mek_type, gps.loadout) != gps.mek_id) {
                        fail(error, n, "Unsupported authored player reference in %s", r.name); goto bad;
                    }
                    selected = gps;
                }
            }
        }
        free(chunks); prj_record_free(&r);
        continue;
bad:
        free(chunks); prj_record_free(&r); return -1;
    }
    if (userstar == 1 && !players) return 0;
    if (!userstar && players == 1) {
        if (stock_player) *stock_player = selected;
        return 1;
    }
    return fail(error, n, "Mission needs one USERSTAR reference or one authored stock player");
}

