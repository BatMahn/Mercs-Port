#include "mercsrepair.h"
#include <stdlib.h>
#include <string.h>

struct repair_items { uint8_t *bytes; size_t count, capacity; };
struct mercs_repair_session {
    mercs_career_mech current, reference, backup;
    struct repair_items lists[4];
    int32_t funds;
    uint8_t dirty;
    int active;
};

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int32_t signed32(uint32_t v)
{ return v <= INT32_MAX ? (int32_t)v : (int32_t)(-1 - (int64_t)(UINT32_MAX - v)); }
static int32_t i32(const uint8_t *p) { return signed32(u32(p)); }
static void put32(uint8_t *p, uint32_t v)
{ p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static int32_t sub32(int32_t a, int32_t b) { return signed32((uint32_t)a-(uint32_t)b); }
static int32_t add32(int32_t a, int32_t b) { return signed32((uint32_t)a+(uint32_t)b); }

/* DOS0xb8a54 / Windows0x10082be8: ID, string pointer, cost, 23 rows.
 * Pointer/name columns are not native record fields. See original-repairs.json. */
static const struct { int id, cost; } equipment[] = {
    {7000,10000},{7050,50000},{7100,10000},{7150,10000},{7200,10000},
    {7250,10000},{7300,10000},{7350,10000},{7400,10000},{7450,10000},
    {7500,10000},{7550,10000},{7600,10000},{7650,10000},{7700,100000},
    {7750,200000},{7800,300000},{7850,200000},{7900,50000},{8000,2000},
    {8500,10000},{9000,1600},{9500,20000}
};

static int classify(int32_t id)
{ return id == -1 || id == 0 ? 3 : id < 7000 ? 0 : id < 10000 ? 2 : 1; }
static int32_t item_id(int kind, int32_t id)
{
    int k;
    if (kind == 0) return id / 100;
    if (kind == 1) return sub32(id,10000) / 10000;
    if (kind == 2) for (k = 22; k >= 0; k--)
        if (equipment[k].id / 50 == id / 50) return k;
    return -1;
}

int mercs_repair_begin(const mercs_career_mech_pair *pair,
                       const uint8_t *weapons, size_t nw,
                       const uint8_t *ammo, size_t na,
                       int32_t funds, uint8_t dirty, mercs_repair_session **out)
{
    mercs_repair_session *s;
    size_t i;
    if (!pair || !out || (nw && !weapons) || (na && !ammo) ||
        nw > MERCS_REPAIR_MAX_ITEMS || na > MERCS_REPAIR_MAX_ITEMS) return MERCS_REPAIR_INVALID;
    s = calloc(1,sizeof *s);
    if (!s) return MERCS_REPAIR_NOMEM;
    s->current = s->backup = pair->current; s->reference = pair->reference;
    s->funds = funds; s->dirty = dirty; s->active = 1;
    for (i = 0; i < 4; i++) {
        size_t n = i & 1 ? na : nw;
        s->lists[i].capacity = n;
        if (n) {
            s->lists[i].bytes = malloc(n * MERCS_REPAIR_ITEM_BYTES);
            if (!s->lists[i].bytes) { mercs_repair_free(s); return MERCS_REPAIR_NOMEM; }
            if (i < 2) {
                memcpy(s->lists[i].bytes,i & 1 ? ammo : weapons,n * MERCS_REPAIR_ITEM_BYTES);
                s->lists[i].count = n;
            }
        }
    }
    *out = s;
    return MERCS_REPAIR_OK;
}

void mercs_repair_free(mercs_repair_session *s)
{
    int i;
    if (!s) return;
    for (i = 0; i < 4; i++) free(s->lists[i].bytes);
    free(s);
}
const mercs_career_mech *mercs_repair_current(const mercs_repair_session *s) { return s ? &s->current : NULL; }
const mercs_career_mech *mercs_repair_reference(const mercs_repair_session *s) { return s ? &s->reference : NULL; }
const mercs_career_mech *mercs_repair_backup(const mercs_repair_session *s) { return s ? &s->backup : NULL; }
int32_t mercs_repair_funds(const mercs_repair_session *s) { return s ? s->funds : 0; }
uint8_t mercs_repair_dirty(const mercs_repair_session *s) { return s ? s->dirty : 0; }
int mercs_repair_select_location(mercs_repair_session *s, int location)
{
    if (!s || !s->active || location < 0 || location >= 8) return MERCS_REPAIR_INVALID;
    put32(s->current.bytes+0x364,(uint32_t)location);
    return MERCS_REPAIR_OK;
}
const uint8_t *mercs_repair_items(const mercs_repair_session *s, enum mercs_repair_list list, size_t *count)
{
    if (count) *count = 0;
    if (!s || list < 0 || list > 3) return NULL;
    if (count) *count = s->lists[list].count;
    return s->lists[list].bytes;
}

static void changed(mercs_repair_session *s, mercs_career_mech *m)
{ m->bytes[0x84c] = 1; s->dirty = 1; }

static int32_t armor_internal(mercs_repair_session *s, mercs_career_mech *m,
                              int loc, int part, int quote)
{
    size_t p = 0x760u + (size_t)loc * 18u;
    int32_t diff, cost;
    if (part == MERCS_REPAIR_ARMOR) {
        diff = add32(sub32(i32(s->reference.bytes+p+8),i32(m->bytes+p+8)),
                     sub32(i32(s->reference.bytes+p+12),i32(m->bytes+p+12)));
        cost = signed32((uint32_t)diff * 625u);
        if (!quote) memcpy(m->bytes+p+8,s->reference.bytes+p+8,8);
    } else {
        diff = sub32(i32(s->reference.bytes+p),i32(m->bytes+p));
        cost = signed32((uint32_t)diff * 200u);
        if (!quote) memcpy(m->bytes+p,s->reference.bytes+p,4);
    }
    if (!quote && diff > 0) changed(s,m);
    return cost;
}

/* DOS0x64f7c: the first matching owned ID/type record moves to reservation.
 * Cost is zero for an actual inventory replacement; built-ins use the table.
 * Keep the literal 9550 hundred-base comparison, even though it is unreachable
 * for positive integer IDs. Do not silently substitute 9500 or a shop item. */
static int replacement(mercs_repair_session *s, int32_t id, int reserve, int32_t *cost)
{
    int kind = classify(id), list;
    int32_t ident = item_id(kind,id);
    size_t k;
    struct repair_items *owned, *reserved;
    *cost = 0;
    if (kind == 0) list = 0;
    else if (kind == 1) list = 1;
    else if (kind == 2 && ident >= 0) {
        int32_t base = id / 100 * 100;
        list = 0;
        if (base == 8000) ident = 1;
        else if (base == 8500) ident = 6;
        else if (base == 9550) ident = 5;
        else { *cost = equipment[ident].cost; return 1; }
    } else return 0;
    owned = &s->lists[list]; reserved = &s->lists[list+2];
    for (k = 0; k < owned->count; k++) {
        uint8_t *p = owned->bytes + k * MERCS_REPAIR_ITEM_BYTES;
        if (i32(p) != ident || p[8] != kind) continue;
        if (reserve) {
            /* Both arrays were sized to the original owned count. Moving
             * existing rows conserves that total and needs no allocation. */
            memcpy(reserved->bytes+reserved->count*MERCS_REPAIR_ITEM_BYTES,p,MERCS_REPAIR_ITEM_BYTES);
            reserved->count++;
            memmove(p,p+MERCS_REPAIR_ITEM_BYTES,(owned->count-k-1)*MERCS_REPAIR_ITEM_BYTES);
            owned->count--;
        }
        return 1;
    }
    return 0;
}

int mercs_repair_has_replacement(mercs_repair_session *s, int32_t id, int32_t *cost)
{
    if (!s || !cost) return MERCS_REPAIR_INVALID;
    return replacement(s,id,0,cost);
}

static void restore_group(mercs_repair_session *s, mercs_career_mech *m,
                           int loc, int slot, int32_t id)
{
    int kind = classify(id), k, last = -1;
    size_t p;
    if (kind == 0) {
        for (k = 0; k < 14; k++) if (i32(s->current.bytes+0x2f0+8*k) == -1) last = k;
        if (last < 0) last = 13;
        put32(m->bytes+0x2f0+8*last,(uint32_t)id);
    } else if (kind == 1) {
        for (k = 0; k < 25; k++) if (i32(s->current.bytes+0x28c+4*k) == -1) last = k;
        if (last < 0) last = 24;
        put32(m->bytes+0x28c+4*last,(uint32_t)id);
    } else if (kind == 2 && (id == 7800 || id == 7850)) return;
    for (k = 0; k < 96; k++) {
        p = 0x368u + (size_t)k * 4u;
        if (k != loc*12+slot && i32(s->reference.bytes+p) == id) put32(m->bytes+p,(uint32_t)id);
    }
}

static int32_t critical(mercs_repair_session *s, mercs_career_mech *m,
                        int loc, int slot, int quote)
{
    size_t p = 0x368u + (size_t)(loc*12+slot)*4u;
    int32_t id = i32(s->reference.bytes+p), old = i32(m->bytes+p), cost;
    int kind = classify(id), oldkind = classify(old);
    if (id < 0 || old == id) return 0;
    if (kind == 1 && kind == oldkind && item_id(kind,id) == item_id(oldkind,old)) return 0;
    if (!replacement(s,id,0,&cost)) return -1;
    if (!quote) { put32(m->bytes+p,(uint32_t)id); changed(s,m); }
    if ((kind == 2 && id/100*100 == 8000) || (kind != 2 && !quote)) {
        restore_group(s,m,loc,slot,id); changed(s,m);
    }
    if (!quote) { replacement(s,id,1,&cost); changed(s,m); }
    return cost;
}

static int32_t full(mercs_repair_session *s, mercs_career_mech *m, int quote)
{
    int loc, slot;
    int32_t total = 0;
    m->bytes[0x84d] = 1;
    for (loc = 0; loc < 8; loc++) {
        total = add32(total,armor_internal(s,m,loc,MERCS_REPAIR_ARMOR,quote));
        total = add32(total,armor_internal(s,m,loc,MERCS_REPAIR_INTERNAL,quote));
        for (slot = 0; slot < 12; slot++) {
            size_t p = 0x368u + (size_t)(loc*12+slot)*4u;
            if (i32(m->bytes+p) != i32(s->reference.bytes+p)) {
                int32_t cost = critical(s,m,loc,slot,quote);
                if (cost > 0) total = add32(total,cost);
            }
        }
    }
    return total;
}

static int valid_part(const mercs_repair_session *s, int loc, int part, int slot, const int32_t *amount)
{
    return s && s->active && amount && loc >= 0 && loc < 8 && part >= 0 && part <= 2 &&
           (part != MERCS_REPAIR_CRITICAL || (slot >= 0 && slot < 12));
}
int mercs_repair_component(mercs_repair_session *s, int loc, enum mercs_repair_part part,
                           int slot, int quote, int32_t *amount)
{
    if (!valid_part(s,loc,part,slot,amount) || (quote != 0 && quote != 1)) return MERCS_REPAIR_INVALID;
    *amount = part == MERCS_REPAIR_CRITICAL ? critical(s,&s->current,loc,slot,quote) :
              armor_internal(s,&s->current,loc,part,quote);
    return part == MERCS_REPAIR_CRITICAL && *amount == -1 ? MERCS_REPAIR_NO_REPLACEMENT : MERCS_REPAIR_OK;
}
int mercs_repair_all(mercs_repair_session *s, int quote, int32_t *amount)
{
    if (!s || !s->active || !amount || (quote != 0 && quote != 1)) return MERCS_REPAIR_INVALID;
    *amount = full(s,&s->current,quote);
    return MERCS_REPAIR_OK;
}

int mercs_repair_selected(mercs_repair_session *s, int loc, enum mercs_repair_part part,
                          int slot, int32_t *amount)
{
    int32_t id, cost;
    size_t p;
    if (!valid_part(s,loc,part,slot,amount)) return MERCS_REPAIR_INVALID;
    put32(s->current.bytes+0x364,(uint32_t)loc);
    *amount = 0;
    p = 0x760u + (size_t)loc*18u;
    if (part != MERCS_REPAIR_INTERNAL &&
        sub32(i32(s->reference.bytes+p),i32(s->current.bytes+p)) > 0) return MERCS_REPAIR_INTERNALS_FIRST;
    if (part != MERCS_REPAIR_CRITICAL) {
        *amount = armor_internal(s,&s->current,loc,part,1);
        if (s->funds < *amount) return MERCS_REPAIR_INSUFFICIENT_FUNDS;
        *amount = armor_internal(s,&s->current,loc,part,0);
        return MERCS_REPAIR_OK;
    }
    p = 0x368u + (size_t)(loc*12+slot)*4u;
    id = i32(s->reference.bytes+p);
    if (id == -1 || i32(s->current.bytes+p) == id) return MERCS_REPAIR_NOT_DAMAGED;
    if (!replacement(s,id,0,&cost)) return MERCS_REPAIR_NO_REPLACEMENT;
    if (cost > 0) changed(s,&s->current);
    put32(s->current.bytes+p,(uint32_t)id);
    restore_group(s,&s->current,loc,slot,id);
    replacement(s,id,1,&cost);
    if (cost > 0) changed(s,&s->current);
    *amount = cost;
    return MERCS_REPAIR_OK;
}

int mercs_repair_owed(mercs_repair_session *s, int32_t *amount)
{
    int32_t before, after;
    if (!s || !s->active || !amount) return MERCS_REPAIR_INVALID;
    before = full(s,&s->backup,1); after = full(s,&s->current,1);
    *amount = sub32(before,after);
    return MERCS_REPAIR_OK;
}
int mercs_repair_rollback(mercs_repair_session *s)
{
    int k;
    if (!s || !s->active) return MERCS_REPAIR_INVALID;
    for (k = 0; k < 2; k++) {
        struct repair_items *owned = &s->lists[k], *reserved = &s->lists[k+2];
        if (reserved->count) {
            memcpy(owned->bytes+owned->count*MERCS_REPAIR_ITEM_BYTES,reserved->bytes,
                   reserved->count*MERCS_REPAIR_ITEM_BYTES);
            owned->count += reserved->count;
            reserved->count = 0;
        }
    }
    s->current = s->backup;
    return MERCS_REPAIR_OK;
}
int mercs_repair_finish(mercs_repair_session *s, int accept, int undo, int *leave)
{
    int32_t owed = 0;
    if (!s || !s->active || !leave || (accept != 0 && accept != 1) || (undo != 0 && undo != 1)) return MERCS_REPAIR_INVALID;
    *leave = 1;
    if (!s->current.bytes[0x84c]) { s->active = 0; return MERCS_REPAIR_OK; }
    if (!accept) { mercs_repair_rollback(s); s->active = 0; return MERCS_REPAIR_OK; }
    mercs_repair_owed(s,&owed);
    if (owed < 0) owed = 0;
    if (s->funds < owed) {
        if (undo) { mercs_repair_rollback(s); s->active = 0; }
        else *leave = 0;
        return MERCS_REPAIR_INSUFFICIENT_FUNDS;
    }
    if (owed > 0) { s->funds = sub32(s->funds,owed); s->current.bytes[0x84c] = 0; }
    s->active = 0;
    return MERCS_REPAIR_OK;
}
