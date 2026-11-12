/* combat.c - see combat.h (what is from the game and what is assumed is listed there). */
#include "combat.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wtable.h"
#include "wtable_titanium.h"

static unsigned rnd(unsigned *s) { *s = *s * 1103515245u + 12345u; return (*s >> 16) & 0x7FFF; }
static float frand(unsigned *s) { return (float)rnd(s) / 32768.0f; }

void combat_fall_begin(combat_unit *u, int32_t gaussian)
{
    u->fall_active = 1;
    /* 2f59c multiplies in 32 bits before signed division by1024. */
    u->fall_velocity = (int32_t)((uint32_t)gaussian * 65536u) / 1024;
}
static void fall_integrate(int32_t *angle, int32_t *velocity, int32_t acceleration, int32_t ticks)
{
    int32_t dv = (int32_t)((uint32_t)acceleration * (uint32_t)ticks);
    int32_t mid = (int32_t)((uint32_t)*velocity + (uint32_t)(dv >> 1));
    *velocity = (int32_t)((uint32_t)*velocity + (uint32_t)dv);
    *angle = (int32_t)((uint32_t)*angle + (uint32_t)(((int64_t)mid * ticks) >> 16));
}
int combat_fall_frame(combat_unit *u, int32_t ticks, int grounded,
                      int32_t clearance, int thng_contact)
{
    if (!u || !u->fall_active) return 0;
    if (grounded || llabs((long long)clearance) <= 100) {
        if (thng_contact) {
            if (u->fall_contact) return 0;
            u->fall_contact = 1; return 4;
        } else {
            int32_t a = u->fall_angle, v = u->fall_velocity;
            fall_integrate(&a, &v, 100 * 65536, ticks);
            a %= 360 * 65536;
            if (!((v > 0 && u->fall_velocity > 0) || (v < 0 && u->fall_velocity < 0) ||
                  (u->fall_angle > -5 * 65536 && u->fall_angle < 5 * 65536))) return 0;
            u->fall_angle = a; u->fall_velocity = v; return 1;
        }
    }
    if (!grounded && u->fall_angle != 0) {
        if (u->fall_velocity >= 0) u->fall_velocity = -25 * 65536;
        if (u->fall_angle <= 0) u->fall_angle = 0;
        else fall_integrate(&u->fall_angle, &u->fall_velocity, -100 * 65536, ticks);
        u->fall_contact = 0; return 2;
    }
    return 0;
}
static int32_t fall_gaussian(unsigned *rng)
{
    /* Original5c6e0 builds each signed-table value from30 uniform draws.
     * Use that distribution with the native mission RNG; the shared
     * original127-entry pool's global consumption order is not claimed. */
    unsigned sum = 0; int i;
    for (i = 0; i < 30; ++i) sum += rnd(rng);
    return (int32_t)(sqrt(90.0) * (2.0 * sum / (32767.0 * 30.0) - 1.0) * 1024.0);
}

static combat_planet g_planet = {1.0f, 0, 100000.0f, 0};
static int g_weapon_timing_dos = 1;
static const sim_weapon *g_weapons = sim_weapons;

void combat_set_weapon_profile(int profile)
{
    g_weapon_timing_dos = profile == COMBAT_PROFILE_DOS;
    g_weapons = profile == COMBAT_PROFILE_TITANIUM ? sim_weapons_titanium : sim_weapons;
}
void combat_set_weapon_timing(int dos)
{ combat_set_weapon_profile(dos ? COMBAT_PROFILE_DOS : COMBAT_PROFILE_RETAIL); }
float combat_lock_acquire_ticks(void)
{
    /* Mercs DOS0x670ac / retail Windows0x100639ad initialize and decay
     * acquisition against this edition's duration. State ordering is shared. */
    return g_weapon_timing_dos ? 364.0f : 362.0f;
}
float combat_projectile_ground_margin(void)
{
    /* Titanium0x10011723..31 compares nextY against support-25cm.
     * DOS, retail Windows and dedicated3dfx use support without a margin. */
    return g_weapons == sim_weapons_titanium ? 25.0f : 0.0f;
}

int combat_is_titanium(void) { return g_weapons == sim_weapons_titanium; }

int combat_projectile_backoff(const float start[3], const float end[3],
                              const float contact[3], float out[3])
{
    double d[3], length;
    int k;
    memcpy(out, contact, 3 * sizeof *out);
    for (k = 0; k < 3; ++k)
        if (!isfinite(start[k]) || !isfinite(end[k]) || !isfinite(contact[k])) return 0;
    if (combat_is_titanium()) {
        /* Titanium10051992 stores double endpoint differences, calls sqrt,
         * then stores FLOAT unit components. 100110b3 uses those components
         * in C-100*n before the final float store, with no distance clamp. */
        for (k = 0; k < 3; ++k) d[k] = (double)end[k] - (double)start[k];
        length = sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
        if (length > 0) for (k = 0; k < 3; ++k) {
            float direction = (float)(d[k] / length);
            out[k] = (float)((double)contact[k] - 100.0 * (double)direction);
        }
        return 1;
    } else {
        int64_t delta[3], absolute[3], largest = 0, sum = 0, approximation;
        /* Native geometry remains float. The integer-engine bridge is an
         * explicit truncation of EACH endpoint, not truncation of their
         * difference. C is never moved or quantized for direct damage.
         * Reject values outside the proved, non-overflow signed domain;
         * retaining C there avoids undefined float-to-int conversions. */
        for (k = 0; k < 3; ++k) {
            if ((double)start[k] < -2147483648.0 || (double)start[k] > 2147483647.0 ||
                (double)end[k] < -2147483648.0 || (double)end[k] > 2147483647.0) return 0;
            delta[k] = (int64_t)(int32_t)end[k] - (int64_t)(int32_t)start[k];
            if (delta[k] < -2147483647 || delta[k] > 2147483647) return 0;
            absolute[k] = delta[k] < 0 ? -delta[k] : delta[k];
            if (absolute[k] > largest) largest = absolute[k];
            sum += absolute[k];
        }
        if (3 * largest + sum > 2147483647) return 0;
        approximation = (3 * largest + sum) / 4;
        if (approximation == 0) return 1;
        length = approximation > 3000 ?
                 sqrt((double)delta[0]*(double)delta[0] + (double)delta[1]*(double)delta[1] +
                      (double)delta[2]*(double)delta[2]) : (double)approximation;
        for (k = 0; k < 3; ++k) {
            int32_t direction = approximation > 3000 ?
                (int32_t)((double)delta[k] / length * 65536.0) :
                (int32_t)((delta[k] * 65536) / approximation);
            int64_t q = (int64_t)direction * 100 + 32768;
            /* Original signed IMUL / SHRD16 / ADC0: negative half values
             * round toward +infinity too. Avoid implementation-defined >>. */
            int64_t rounded = q >= 0 ? q / 65536 : -((-q + 65535) / 65536);
            out[k] = contact[k] - (float)rounded;
        }
        return 1;
    }
}
static int weapon_time(int w, int field)
{
    if (w < 0 || w >= SIM_WEAPON_COUNT) return 0;
    if (g_weapon_timing_dos) return sim_weapon_dos_time[w][field];
    return field == 0 ? g_weapons[w].refire : field == 1 ? g_weapons[w].interval : g_weapons[w].life;
}
int combat_weapon_refire(int w) { return weapon_time(w, 0); }
int combat_weapon_interval(int w) { return weapon_time(w, 1); }
float combat_weapon_heat(int w) { return w >= 0 && w < SIM_WEAPON_COUNT ? g_weapons[w].heat : 0; }
int combat_weapon_ams(int w) { return w == 29 || w == 62; }

void combat_set_planet(const combat_planet *p) { g_planet = *p; if (g_planet.jet_damping <= 0) g_planet.jet_damping = 100000.0f; }
static int g_auto_eject;   /* DAT_1024bfbc */
void combat_set_auto_eject(int on) { g_auto_eject = on != 0; }
int  combat_auto_eject(void) { return g_auto_eject; }

static int rand_n(unsigned *s, int n) { return n > 0 ? (int)(rnd(s) % (unsigned)n) : 0; }   /* engine 0x1003d910: table[i] % n */

static float range_cm(int w) { return (float)g_weapons[w].range; }   /* Mercs table +0x50 */

/* MERCSW.DLL 0x1000bb66..0x1000bdd5: an integer Q16.16 coefficient,
 * multiplied by MEK heat_sinking. OUT3's -101 C uses the new fivefold
 * extreme-cold branch. Truncation after each difficulty multiplier matters. */
static float sink_factor(int difficulty)
{
    int q = difficulty == 0 ? 70 : difficulty == 2 ? 45 : 50;
    if (g_planet.temperature <= -100) q *= 5;
    else if (g_planet.temperature < -30) q = difficulty == 2 ? q * 3 / 2 : q * 2;
    else if (g_planet.temperature > 50 && difficulty != 0) q = difficulty == 2 ? q * 4 / 5 : q * 9 / 10;
    return (float)q / 65536.0f;
}

/* Mercs rating consumer 0x1000bf23..0x1000c399 retains the cumulative
 * location calculation but uses equipment thresholds 7000..7900, 8000,
 * 8500, 9000, 9500 and fourteen item records. The first 32 weapon rating
 * values remain unchanged. UNRESOLVED: the Windows retail consumer indexes
 * beyond that 32-entry table into diagnostic strings. DOS 0x5fe54 likewise
 * indexes beyond its table at 0xbc85c, into mutable globals (MLAS index 56
 * reads the live GPS unit pointer) and relocated code pointers. A snapshot
 * of that adjacent data cannot represent both runtime behaviors. Weapon
 * rating contributions for IDs >=32 stay zero pending a compatibility policy;
 * this affects rating-based group selection, not damage/heat/range. */
static const short rating_mult[21] = { 1, 1, -60, -60, -50, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 2, 3, 2, 1, 1 };
static const short rating_weapon[32] = {   /* 0x10259f60 */
    183, 137, 91, 46, 51, 34, 17, 74, 49, 25, 2, 228, 42, 82, 123, 157,
    74, 144, 247, 329, 4, 228, 166, 51, 21, 177, 74, 16, 50, 40, 0, 0 };
static int rating_band_min(int b) { return b <= 18 ? MEK_EQ_MASC + 50 * b : MEK_EQ_HEAT_SINK + 500 * (b - 19); }
uint16_t combat_mek_rating(const mek_def *d)
{
    int count[23] = {0}, total = 0, l, k, b, n;
    int32_t armor_sum = 0;
    int16_t r = (int16_t)d->tonnage, tons = (int16_t)d->tonnage;
    for (l = 0; l < MEK_LOC_COUNT; l++) {
        const mek_loc *L = &d->loc[l];
        int16_t nslots = (int16_t)L->slot_count;
        if (nslots > 0) {
            armor_sum += (int32_t)(L->armor + L->rear_armor) * nslots;
            for (k = 0; k < nslots && k < MEK_SLOTS; k++)
                for (b = 22; b >= 0; b--)
                    if (rating_band_min(b) < (int)L->slots[k]) { count[b]++; total++; break; }
        }
        for (b = 20; b >= 0; b--)
            if (count[b]) r = (int16_t)(rating_mult[b] < 0 ? r - rating_mult[b] * count[b] : r + rating_mult[b] * count[b] * tons);
        r = (int16_t)(r + (count[21] ? total * 2 : total));
        r = (int16_t)(r + (int16_t)armor_sum + (int16_t)L->internal);
    }
    n = d->item_count < COMBAT_MAX_WEAPONS ? d->item_count : COMBAT_MAX_WEAPONS;
    for (k = 0; k < n; k++) {
        unsigned idx = d->items[k].id / 100;
        r = (int16_t)(r + (idx < 32 ? rating_weapon[idx] : 0));
    }
    return (uint16_t)r;
}

/* Mercs DOS 0x5f5d5: original signed division, with the 32-bit addition
 * performed before division. The two nibbles describe the initial location,
 * before difficulty armor scaling, and remain attached to its return data. */
static int32_t signed_word(uint32_t value)
{
    return value <= INT32_MAX ? (int32_t)value :
           (int32_t)(-1 - (int64_t)(UINT32_MAX - value));
}
static uint16_t return_location_flags(const mek_loc *loc)
{
    int32_t front, rear;
    if (loc->flags & 0x0800) return loc->flags;
    front = signed_word(loc->armor + loc->internal) / 5;
    rear = signed_word(loc->rear_armor + loc->internal) / 5;
    if (front < 0) front = 0; else if (front > 15) front = 15;
    if (rear < 0) rear = 0; else if (rear > 15) rear = 15;
    return (uint16_t)((loc->flags & 0xff00) | 0x0800 | front | (rear << 4));
}
static int64_t return_walk_raw(int32_t walk_mp)
{
    int64_t product = (int64_t)walk_mp * 0x697e98 + 0x8000;
    return product >= 0 ? product / 65536 : -((-product + 65535) / 65536);
}

int combat_init(combat_unit *u, prj_archive *a, const char *loadout)
{
    mek_def d;
    int i, l, k, j;
    memset(u, 0, sizeof *u);
    snprintf(u->loadout, sizeof u->loadout, "%s", loadout);
    u->speed_gain = 1.0f;
    u->unit_class = 1;
    u->rng = 0x9E3779B9u;
    u->armor_mult = 1.0f;
    u->lock_target = -2;
    if (mek_load(a, loadout, &d) != 0) return -1;   /* the archive, else a user design MEK\\<name>.MEK */
    memcpy(u->return_config, d.config_raw, sizeof u->return_config);
    /* Mercs DOS loader0x5fb11 and controller init0x30ac0 retain this raw
     * integer. Actuator damage changes speed_gain, not the return field.
     * This uses the same scalar path as msim_top_speed; full fixed-point
     * locomotion remains separate. Invalid/unrepresentable input disables
     * return capture instead of manufacturing a stock replacement. */
    if (d.walk_mp <= INT32_MAX && isfinite(g_planet.gravity_g) &&
        g_planet.gravity_g > 0 && g_planet.gravity_g <= 32767.0f) {
        int64_t g16 = (int64_t)lrintf(g_planet.gravity_g * 65536.0f);
        int64_t grav = (g16 * 0x794 + 0x8000) >> 16;
        int64_t ratio = (grav * 65536) / 0x794;
        int64_t raw = return_walk_raw((int32_t)d.walk_mp);
        if (ratio > 0) {
            raw = (raw * 65536) / ratio;
            if (raw >= INT32_MIN && raw <= INT32_MAX) {
                u->return_walk_speed = (int32_t)raw;
                u->return_valid = 1;
            }
        }
    }
    /* DOS0x33180 obtains a fresh controller from a zero-backed pool.
     * The loader copies the name through its NUL, then five DWORDs; stock
     * Panther files contain nonzero source bytes after that NUL. Those bytes
     * are not controller data. Reused original allocation is a separate case. */
    for (i = 0; i < 30 && d.config_raw[i]; i++) {}
    if (i == 30) u->return_valid = 0;
    else memset(u->return_config + i + 1, 0, (size_t)(29 - i));
    if (getenv("MERCS_DEBUG_LOADOUT")) fprintf(stderr, "loadout %s: %d items, %d ammo bins, '%s'\n", loadout, d.item_count, d.link_count, d.config_name);
    /* engine 0x10041410: the MEK locations copied as they are (armour, rear, structure made floats; the item
     * list and its count +0x24 kept) */
    for (l = 0; l < MEK_LOC_COUNT; l++) {
        u->armor[l] = (float)d.loc[l].armor;
        u->rear[l] = (float)d.loc[l].rear_armor;
        u->internal[l] = (float)d.loc[l].internal;
        u->armor_max[l] = (int)d.loc[l].armor;
        u->rear_max[l] = (int)d.loc[l].rear_armor;
        u->slot_count[l] = d.loc[l].slot_count <= MEK_SLOTS ? d.loc[l].slot_count : MEK_SLOTS;
        if (d.loc[l].slot_count > MEK_SLOTS) u->return_valid = 0;
        u->return_flags[l] = return_location_flags(&d.loc[l]);
        for (k = 0; k < MEK_SLOTS; k++) u->slots[l][k] = d.loc[l].slots[k];
    }
    u->sink_per_tick = (float)d.heat_sinking * sink_factor(1);
    u->rating = combat_mek_rating(&d);
    u->tons = (int)d.tonnage;   /* controller +0xe4 (MEK +0x00): the head-hit ratio in 0x1000c1f0 */
    u->jets = (int)d.jump_mp;
    u->jet_ddy = d.walk_mp ? (float)d.jump_mp / (float)d.walk_mp * 0.1184f : 0;
    u->jet_fuel = u->jets > 0 ? combat_jet_fuel_max : 0;
    /* DOS0x5f6d2 / Windows0x1000b34d builds every ammo type pool BEFORE
     * matching mounts. A destroyed gun can leave live ammunition in the
     * next deployment; neither its pool nor its bins depend on a mount.
     * Pools retain first-link order. Native bins are contiguous per pool;
     * return_bin_order preserves the original physical link ordering. */
    for (j = 0; j < d.link_count && j < COMBAT_MAX_BINS && d.item_count + j < MEK_MAX_ITEMS; j++) {
        const mek_item *item = &d.items[d.item_count + j];
        int wi = item->linked_id / 100;
        if (item->linked_id < 0 || wi >= SIM_WEAPON_COUNT) { u->return_valid = 0; continue; }
        for (k = 0; k < u->ammo_pool_count; k++) if (u->ammo_pools[k].weapon == wi) break;
        if (k == u->ammo_pool_count) {
            if (k == COMBAT_MAX_POOLS) { u->return_valid = 0; continue; }
            u->ammo_pools[u->ammo_pool_count++].weapon = wi;
        }
    }
    for (k = 0; k < u->ammo_pool_count; k++) {
        combat_ammo_pool *pool = &u->ammo_pools[k];
        int wi = pool->weapon;
        pool->bin_first = u->bin_count;
        for (j = 0; j < d.link_count && j < COMBAT_MAX_BINS && d.item_count + j < MEK_MAX_ITEMS; j++) {
            const mek_item *item = &d.items[d.item_count + j];
            combat_bin *b;
            if (item->linked_id < 0 || item->linked_id / 100 != wi) continue;
            if (u->bin_count == COMBAT_MAX_BINS) { u->return_valid = 0; break; }
            u->return_bin_order[j] = u->bin_count;
            b = &u->bins[u->bin_count++];
            b->id = item->id; b->weapon = wi; b->pool = k;
            b->rounds = (int)(short)(g_weapons[wi].per_volley * g_weapons[wi].ammo_per_ton);
            /* Original constructor and loader do not assign bin+0x12.
             * Fresh zero-backed controllers therefore retain critical pool0,
             * even for later type pools. See original orphan-ammo probes. */
            b->critical_pool = 0;
            pool->rounds += b->rounds;
            pool->bin_n++;
        }
        if (pool->bin_n > COMBAT_POOL_BINS) u->return_valid = 0;
    }
    /* Fourteen mounts share the independent type pools above. */
    for (i = 0; i < d.item_count && u->weapon_count < COMBAT_MAX_WEAPONS; i++) {
        const mek_weapon *w = mek_weapon_for(d.items[i].id);
        combat_weapon *cw;
        int wi, loc = MEK_CT;
        if (!w) continue;
        wi = (int)(w - mek_weapons);
        if (wi < 0 || wi >= SIM_WEAPON_COUNT || g_weapons[wi].per_volley <= 0) continue;
        for (l = 0; l < MEK_LOC_COUNT; l++)
            for (k = 0; k < MEK_SLOTS; k++)
                if (d.loc[l].slots[k] == d.items[i].id) { loc = l; l = MEK_LOC_COUNT; break; }
        cw = &u->weapons[u->weapon_count];
        cw->weapon = wi;
        cw->item = d.items[i].id;
        cw->ammo = g_weapons[wi].ammo_per_ton != -1 ? 0 : -1;
        cw->ammo_pool = -1;
        cw->location = loc;
        /* Original Mercs loader retains a separate descriptor: group from
         * bits8..11 (values>4 become0), and the LAST matching critical
         * location, with legs mapped to their torso mount positions. The
         * inherited firing location is not used as evidence for this field. */
        cw->return_group = (int)(((uint32_t)d.items[i].linked_id >> 8) & 15u);
        if (cw->return_group > 4) cw->return_group = 0;
        cw->return_location = -1; /* DOS0x33254 initializes every mount to no location. */
        for (l = 0; l < MEK_LOC_COUNT; l++)
            for (j = 0; j < MEK_SLOTS; j++)
                if (d.loc[l].slots[j] == cw->item) cw->return_location = l;
        if (cw->return_location == MEK_RL) cw->return_location = MEK_RT;
        if (cw->return_location == MEK_LL) cw->return_location = MEK_LT;
        cw->state = CW_READY;
        cw->homing = -2;
        for (k = 0; k < u->ammo_pool_count; k++) {
            const combat_ammo_pool *pool = &u->ammo_pools[k];
            if (pool->weapon != wi) continue;
            cw->ammo_pool = k;
            cw->bin_first = pool->bin_first;
            cw->bin_n = pool->bin_n;
            cw->bin_cur = pool->bin_cur;
            cw->ammo = pool->rounds;
            break;
        }
        if (cw->ammo == 0) cw->state = -1;
        u->weapon_count++;
    }
    /* Reject partial/malformed state, while retaining valid unmounted bins. */
    if (u->bin_count != d.link_count || u->weapon_count != d.item_count)
        u->return_valid = 0;
    return 0;
}

static void destroy_location(combat_unit *t, int loc, unsigned *rng);
static void crit_item(combat_unit *t, int loc, int idx, int silent);
static void cook_off(combat_unit *u);

/* Cached HUD ammunition and current-bin positions mirror the shared runtime
 * pool. On exhaustion the retail engine disables every mount of this type
 * (0x10062918..0x1006296b), including simultaneous volleys. */
static void sync_pool(combat_unit *u, const combat_weapon *source)
{
    int p = source->ammo_pool;
    if (p >= 0 && p < u->ammo_pool_count && u->ammo_pools[p].weapon == source->weapon) {
        u->ammo_pools[p].rounds = source->ammo;
        u->ammo_pools[p].bin_cur = source->bin_cur;
    }
}

static void sync_ammo(combat_unit *u, const combat_weapon *source)
{
    int k;
    sync_pool(u, source);
    for (k = 0; k < u->weapon_count; k++) {
        combat_weapon *cw = &u->weapons[k];
        if (cw->weapon != source->weapon) continue;
        cw->ammo = source->ammo;
        cw->bin_cur = source->bin_cur;
        if (cw->ammo == 0) { cw->state = -1; cw->shots = 0; }
    }
}

/* engine 0x10043cc0: one round leaves the weapon's count and its current bin (an empty bin moves the pointer on to
 * the next without taking a round); the count reaching 0 marks the weapon spent (state -1) and that projectile
 * still flies but adds no heat. Returns 0 then. Unlimited ammunition (Combat Variables +0) takes nothing. */
static int take_round(combat_unit *u, combat_weapon *cw)
{
    if (cw->ammo == -1 || u->unlimited_ammo) return 1;
    cw->ammo--;
    if (cw->bin_n > 0) {
        combat_bin *b = &u->bins[cw->bin_first + cw->bin_cur];
        if (b->rounds == 0) { if (cw->bin_cur + 1 < cw->bin_n) cw->bin_cur++; }
        else b->rounds--;
    }
    sync_ammo(u, cw);
    if (cw->ammo == 0) { cw->state = -1; return 0; }
    return 1;
}

/* engine 0x100437a0, state 2: release every projectile due (`interval` ticks apart, the time carried over) */
static void release_due(combat_unit *u, combat_weapon *cw)
{
    const sim_weapon *sw = &g_weapons[cw->weapon];
    const int interval = combat_weapon_interval(cw->weapon);
    while (cw->state == CW_FIRING && cw->shots > 0 && cw->t >= (float)interval) {
        int ok = take_round(u, cw);
        /* 0x100437a0 calls the spawn (0x10043cc0) before it takes the interval off: the projectile's life is the
         * table's +0x4c less slot +0x10 as it stands - the volley timer before this frame's ticks for the first
         * release of the frame, the remainder after the last release for the next (DOS 0x52757: the same) */
        if (cw->pulses_due < 24) cw->pulse_age[cw->pulses_due] = cw->t10;
        cw->pulses_due++;
        if (!ok) { if (cw->shots == sw->per_volley) cw->volley_start = 0; cw->shots = 0; break; }   /* last round: no heat, no sound */
        cw->t -= (float)interval;
        cw->t10 = cw->t;
        cw->shots--;
        u->heat += combat_test_wheat[cw->weapon] > 0 ? combat_test_wheat[cw->weapon] : sw->heat;   /* TEST ONLY override */
        if (cw->shots == 0 && sw->repeat && (u->fire_held & (1u << (cw - u->weapons)))) {
            /* a repeating weapon with its trigger still held waits refire ticks and fires again */
            cw->state = CW_REPEAT;
            cw->shots = sw->per_volley;
            cw->t = 0;
            break;
        }
    }
}

/* engine 0x100437a0 / 0x10044440: every trigger path starts the volley with the weapon's timer at 0, and the release
 * loop runs in the same frame - the first projectile leaves once `interval` ticks have passed, counting this frame */
static void start_volley(combat_unit *u, combat_weapon *cw)
{
    cw->state = CW_FIRING;
    cw->shots = g_weapons[cw->weapon].per_volley;
    cw->t = u->last_ticks;
    cw->t10 = 0;   /* the trigger sets +0x10 = 0; the release loop adds this frame's ticks to it */
    cw->volley_start = 1;
    cw->homing = u->lock_target;   /* 0x100437a0: slot +0x18 / +0x1c = the target if locked, at the trigger */
    release_due(u, cw);
}

void combat_tick(combat_unit *u, float dt)
{
    float ticks = dt * COMBAT_TICKS_PER_S, net;
    int i;
    if (u->destroyed) return;
    u->last_ticks = ticks;
    u->fire_edge = u->fire_held & ~u->fire_prev;
    u->fire_prev = u->fire_held;
    u->heat_warning = 0;
    u->seq_alarm = 0;
    u->auto_shutdown = 0;
    u->restarted = 0;
    for (i = 0; i < u->weapon_count; i++) {
        combat_weapon *cw = &u->weapons[i];
        const int refire = combat_weapon_refire(cw->weapon);
        if (combat_weapon_ams(cw->weapon)) continue; /* custom Mercs pre/fire callbacks; combat_ams_step */
        if (cw->state < 0) continue;   /* destroyed by a critical hit, or out of ammunition */
        /* 0x100437a0 runs only while the mech is up (state 2): a volley's timer stands still while shut down; the
         * recycle and repeat waits are measured from a start time, so they run on */
        if ((u->shutdown || u->offline) && cw->state == CW_FIRING) continue;
        cw->t10 = cw->t;
        cw->t += ticks;
        if (u->shutdown || u->offline) continue;
        if (cw->state == CW_RECYCLE && cw->t >= (float)refire) { cw->state = CW_READY; cw->t = 0; }
        else if (cw->state == CW_REPEAT && cw->t >= (float)refire) {
            /* still held: the next volley (counted from 0); released: ready */
            if (u->fire_held & (1u << i)) { cw->state = CW_FIRING; cw->volley_start = 1; } else cw->state = CW_READY;
            cw->t = 0;
        }
        else if (cw->state == CW_FIRING) {
            if (cw->shots < 1) { cw->state = CW_RECYCLE; cw->shots = 0; cw->t = 0; continue; }   /* the frame after the last */
            release_due(u, cw);
        }
    }
    /* walking heat, engine 0x1001aac8: (c13 - 1/64) x dissipation x 6.4 added once per frame (not per tick):
     * c13 - 1/64 = throttle / 64; 30 frames a second assumed */
    if (!u->shutdown && u->throttle_frac > 0)
        u->heat += u->throttle_frac * 0.015625f * u->sink_per_tick * 6.4f * (dt * 30.0f);
    /* These represent differences from the original absolute mission clock,
     * which continues even while the player disables heat tracking. */
    u->seq_ticks += ticks;
    u->warn_ticks += ticks;
    if (u->no_heat) { u->heat = 0; u->heat_mark = 0; return; }   /* Combat Variables: heat tracking off */
    net = (u->heat - u->heat_mark) - u->sink_per_tick * (u->shutdown ? 2.0f : 1.0f) * ticks;   /* this frame's gain */
    u->dhdt_net += (u->heat - u->heat_mark) - u->sink_per_tick * ticks;   /* (DOS 0x311f0: +0x9c x ticks, not doubled) */
    u->heat = u->heat_mark + net;
    if (u->heat < 0) u->heat = 0;
    {
        /* Mercs DOS 0x2ed2c / retail Windows 0x1000465a, compared by executing
         * both original routines. Both classify the WHOLE heat value and use
         * this branch order. The earlier MW2 DOS path differs materially.
         * The retail builds use 182/181-tick constants; accelerated engine
         * parity beyond these retail consumers remains a separate audit. */
        const float second = combat_heat_dos ? 182.0f : 181.0f;
        int h = (int)u->heat;
        if (u->shutdown_pending && !u->override && !u->shutdown && u->seq_ticks > 6.0f * second) {
            u->shutdown = 1;
            u->seq_ticks = 0;
            u->auto_shutdown = 1;
        }
        if (h > 100) {
            if (!u->invulnerable) {
                if (!u->override) {
                    /* DOS 0x2eee0 / Windows 0x1000489d: 20 seconds, not the
                     * older MW2 10-second ammo roll / 25-second destruction. */
                    if (u->seq_ticks > 20.0f * second) u->destroyed = u->down_in_update = 1;
                } else if (ticks >= 1.0f) {
                    int bound = 4000 / (int)ticks;
                    /* Original integer rand(4000 / frame_ticks) < 3. The port
                     * still has its own RNG stream, not the original table. */
                    if (bound > 0 && rand_n(&u->rng, bound) < 3) {
                        cook_off(u);
                        if (u->destroyed) u->down_in_update = 1;
                    }
                }
            }
        } else if (h > 80) {
            if (!u->shutdown_pending) {
                u->shutdown_pending = 1; u->seq_ticks = 0;
                if (u->human && !u->alarm_latch) { u->alarm_latch = 1; u->seq_alarm = 1; }
            }
        } else if (h > 65) {
            if (u->human && !u->warn_latch && net > 0) { u->warn_latch = 1; u->warn_ticks = 0; u->heat_warning = 1; }
        } else if (!u->shutdown_pending) {
            /* DOS 0x2f083 / Windows constant 0x100a1040: below 40, with the
             * four-second latch timeout expired. Cooling only to 50 is not
             * sufficient to re-arm the next critical-heat warning. */
            if (u->human && u->warn_latch && u->warn_ticks > 4.0f * second && h < 40) u->warn_latch = 0;
            u->alarm_latch = 0;
        } else {
            if (u->shutdown && u->seq_ticks > 4.0f * second) { u->shutdown = 0; u->shutdown_pending = 0; u->restarted = 1; u->manual_down = 0; }
            if (u->override) { u->override = 0; u->shutdown_pending = 0; }
        }
    }
    u->heat_mark = u->heat;
}

/* Mercs DOS 0x2ebd0 / Windows 0x100044c6: scan standing locations,
 * then their critical slots, then matching bins. The pool selected by the
 * bin's separate +0x12 field is the gate, not the bin's remaining rounds.
 * A fresh original loader retains0 in that field even for later type pools.
 * The normal critical handler deliberately retains the retail pool-index
 * guard below, so this consumes the selected slot without inventing a blast. */
static void cook_off(combat_unit *u)
{
    int l, k, b;
    if (!combat_damage_live) return;
    if (u->bin_count > 0 && u->ammo_pool_count > 0)
        for (l = 0; l < MEK_LOC_COUNT; l++) {
            if (u->loc_gone[l]) continue;
            for (k = 0; k < u->slot_count[l]; k++) {
                uint32_t id = u->slots[l][k];
                if (id <= 10000) continue;
                for (b = 0; b < u->bin_count; b++) {
                    const combat_bin *bin = &u->bins[b];
                    /* This original field is separate from type ownership.
                     * Bound it before dereferencing, including orphan pools. */
                    if (bin->id != id || bin->critical_pool < 0 || bin->critical_pool >= u->ammo_pool_count) continue;
                    if (u->ammo_pools[bin->critical_pool].rounds > 0) {
                        crit_item(u, l + 1, k, 0);
                        return;
                    }
                }
            }
        }
    if (!(u->human && u->invulnerable)) u->destroyed = 1;
}

/* engine location numbers 1-8 -> MEK index 0-7 (same order) */
static void destroy_location(combat_unit *t, int loc, unsigned *rng) /* DOS2f59c / retail Win10005228 */
{
    int n;
    if (loc < 1 || loc > 8 || t->loc_gone[loc - 1]) return;
    t->loc_gone[loc - 1] = 1;
    t->internal[loc - 1] = 0;            /* only the structure is zeroed; the armour stays (display) */
    /* 0x10016600: every item in the list takes a silent critical (always entry 0: each one removes itself) */
    for (n = t->slot_count[loc - 1]; n > 0 && t->slot_count[loc - 1] > 0; n--) crit_item(t, loc, 0, 1);
    switch (loc) {
    case 2: destroy_location(t, 5, rng); t->loc_gone[1] = 0; return;
    case 4: destroy_location(t, 6, rng); t->loc_gone[3] = 0; return;
    case 5: case 6: return;
    case 3:
        destroy_location(t, 5, rng); destroy_location(t, 2, rng); destroy_location(t, 4, rng); destroy_location(t, 6, rng);
        destroy_location(t, 1, rng);
        break;
    case 7: case 8:
        if (!t->immobile) {
            /* Mercs retains the damaged first leg in95% of normal requests:
             * internal1, gone cleared and speed Q16=0x3333. Items are already
             * critically removed. Non-mechs always take the sever branch. */
            int roll = rand_n(rng, 100);
            if (roll < 95 && t->unit_class == 1) {
                t->internal[loc - 1] = 1; t->loc_gone[loc - 1] = 0;
                t->speed_gain = 13107.0f / 65536.0f;
                return;
            }
            t->immobile = 1; t->speed_gain = 0;
            if (t->unit_class == 1) combat_fall_begin(t, fall_gaussian(rng));
            return;
        }
        destroy_location(t, 3, rng);
        break;
    default:
        if (rand_n(rng, 100) >= 20) { t->internal[loc - 1] = 1; t->loc_gone[loc - 1] = 0; return; }
        break;
    }
    if (!(t->human && t->invulnerable)) t->destroyed = 1;
}
void combat_destroy_location(combat_unit *t, int loc, unsigned *rng)
{
    if (t) destroy_location(t, loc, rng ? rng : &t->rng);
}

static int loc_side_level(float armour, float internal, int mult, int nibble)
{
    int64_t a, s, lv;
    if (nibble <= 0) return 0;
    a = (int64_t)llroundf(armour * 65536.0f);
    s = (int64_t)llroundf(internal * 65536.0f);
    lv = 15 - ((s + a / mult) * 3) / ((int64_t)nibble << 16);   /* C division truncates, as the originals' idiv */
    return lv < 1 ? 0 : lv > 15 ? 15 : (int)lv;
}
int combat_loc_level(const combat_unit *u, int loc)
{
    int mult, f, r;
    if (!u || loc < 0 || loc >= MEK_LOC_COUNT) return 0;
    mult = (int)(u->armor_mult + 0.5f);
    if (mult < 1) mult = 1;
    /* DOS 0x3af08 grades both sides of every location (Windows 0x100117f2
     * only the torsos' rear); a limb's rear (no armour, same structure, a
     * baseline no larger) never exceeds its front, so the result agrees. */
    f = loc_side_level(u->armor[loc], u->internal[loc], mult, u->return_flags[loc] & 0xf);
    r = loc_side_level(u->rear[loc], u->internal[loc], mult, (u->return_flags[loc] >> 4) & 0xf);
    return f > r ? f : r;
}

/* engine 0x100167b0: entry `idx` of location `loc`'s item list takes a critical (silent: a location being destroyed);
 * the entry is then removed from the list */
static void crit_item(combat_unit *t, int loc, int idx, int silent)
{
    int i = loc - 1, k;
    if (loc < 1 || loc > 8) return;
    for (;;) {
        uint32_t id, cls, eq;
        if (idx < 0 || idx >= t->slot_count[i]) return;
        id = t->slots[i][idx];
        cls = id / 100;
        if (cls < MEK_WEAPON_COUNT) {                                       /* a weapon (or an empty entry): that instance is gone */
            for (k = 0; k < t->weapon_count; k++)
                if (t->weapons[k].item == id) {
                    if (!silent && t->human && t->weapons[k].state >= 0 && t->crit_said_n < 8) t->crit_said[t->crit_said_n++] = id;   /* 0x10016996 */
                    int l, in, out;
                    t->weapons[k].destroyed = 1;
                    t->weapons[k].state = -1; t->weapons[k].shots = 0; t->weapons[k].pulses_due = 0;
                    /* Mercs DOS 0x2fa99..0x2faff / Windows
                     * 0x100058d3..0x10005990 remove EVERY slot belonging to
                     * this weapon instance, across all eight locations. This
                     * matters to later critical selection and returned damage. */
                    for (l = 0; l < MEK_LOC_COUNT; l++) {
                        for (in = out = 0; in < t->slot_count[l]; in++)
                            if (t->slots[l][in] != id) t->slots[l][out++] = t->slots[l][in];
                        t->slot_count[l] = out;
                        while (out < MEK_SLOTS) t->slots[l][out++] = 0;
                    }
                    return;
                }
            break;
        }
        if (cls > 100) {
            /* Mercs DOS 0x2fb44..0x2fb5d / Windows 0x100059f0..0x10005a08
             * leave ammunition processing when bin.pool < pool_count. Every
             * valid native bin meets that guard. The common tail still
             * removes this critical slot, but the pool, bin and structure
             * remain unchanged (silent and audible paths alike).
             * Do not emulate the original out-of-range memory path for
             * corrupt pool references; it has no defined native counterpart. */
            break;
        }
        eq = (id / 10) * 10;                                   /* equipment */
        if (!silent && t->human && t->crit_said_n < 8 &&      /* announced (0x100167b0): not MASC, the cockpit or endo / ferro */
            ((eq >= MEK_EQ_SHOULDER && eq <= MEK_EQ_LIFE_SUPPORT && eq % 50 == 0 && eq != MEK_EQ_COCKPIT) || eq == MEK_EQ_JUMP_JET || (eq == MEK_EQ_HEAT_SINK && t->sink_per_tick != 0)))
            t->crit_said[t->crit_said_n++] = id;
        if (eq == MEK_EQ_HIP || eq == MEK_EQ_UPPER_LEG || eq == MEK_EQ_LOWER_LEG || eq == MEK_EQ_FOOT || eq == MEK_EQ_GYRO || eq == MEK_EQ_ENGINE) {
            t->speed_gain -= 0.100006103515625f;               /* hip, leg actuators, gyro, engine */
            if (t->speed_gain < 0) t->speed_gain = 0;
            if (eq == MEK_EQ_GYRO) { t->jet_fuel = -2; t->jet_ddy = 0; t->jets = 0; t->no_jump = 1; }   /* gyro: no jets */
        } else if (eq == MEK_EQ_HEAT_SINK) {
            /* heat sink: 0.000763 less dissipation while it is positive (it can go below 0), else 0 */
            if (t->sink_per_tick > 0) t->sink_per_tick -= 0.000762939453125f; else t->sink_per_tick = 0;
        } else if (eq == MEK_EQ_COCKPIT) t->destroyed = 1;              /* cockpit */
        else if (eq == MEK_EQ_LIFE_SUPPORT) { if (g_planet.hostile) t->destroyed = 1; }   /* life support */
        else if (eq == MEK_EQ_JUMP_JET) {                                 /* jump jet */
            if (t->jets < 1) t->jet_ddy = 0; else t->jet_ddy -= t->jet_ddy / (float)t->jets;
            if (t->jets > 0) t->jets--;
            if (t->jets == 0) t->jet_fuel = -2;
        }
        else if (eq >= MEK_EQ_SHOULDER && eq <= MEK_EQ_HAND) t->display_hits++;   /* cockpit displays: 0x1001e670(flag 0), each instrument 2 in 10 */
        else if (eq == MEK_EQ_SENSORS) t->display_hits5++;                /* 0x10016e33: 0x1001e670(flag 1), each instrument 5 in 10 */
        else if ((eq == MEK_EQ_ENDO || eq == MEK_EQ_FERRO) && !silent) {      /* endo steel / ferro: another entry takes it, silently */
            idx = rand_n(&t->rng, t->slot_count[i]);
            silent = 1;
            continue;
        }
        if (fabsf(t->speed_gain) >= 1e-7f && t->speed_gain < 0.4f) t->speed_gain = 0.4f;   /* floor 0.4 (0x1001710b) */
        break;
    }
    if (t->slot_count[i] <= 0) return;                          /* already emptied (the location went with it) */
    for (k = idx; k < t->slot_count[i] - 1; k++) t->slots[i][k] = t->slots[i][k + 1];
    t->slots[i][k] = 0;
    t->slot_count[i]--;
}

static int hit_location(const combat_unit *t, unsigned *rng)
{
    float total = 0, r;
    int l;
    for (l = 0; l < 8; l++) total += t->hit_w[l];
    if (total <= 0) {
        static const float fallback[8] = {2, 14, 26, 14, 10, 10, 12, 12};   /* unknown geometry */
        int i;
        float ft = 0;
        for (i = 0; i < 8; i++) ft += fallback[i];
        r = frand(rng) * ft;
        for (i = 0; i < 8; i++) { if (r < fallback[i]) return i + 1; r -= fallback[i]; }
        return 3;
    }
    r = frand(rng) * total;
    for (l = 0; l < 8; l++) { if (r < t->hit_w[l]) return l + 1; r -= t->hit_w[l]; }
    return 3;
}

static void apply_damage_at(combat_unit *t, float dmg, int rear, int loc, unsigned *rng);

static void apply_damage(combat_unit *t, float dmg, int rear, unsigned *rng)
{
    apply_damage_at(t, dmg, rear, hit_location(t, rng), rng);
}

int combat_damage_live = 1;
/* the full jet tank, c[0x30] (controller +0xc0) in ticks: 3Dfx 0x712 = 1810 (0x10041beb at load, 0x1001a5ad / 0x1001a63c,
 * the Jets bar 0x1001c783); DOS MW2.EXE 0x71c = 1820 everywhere (0x4ce84 at load, 0x27b84 / 0x27c57 / 0x27c7d, the bar
 * 0x314b9). The front end sets it by edition */
float combat_jet_fuel_max = 1810.0f;
/* the refill with the jets released, both 0x1001a5ad (3Dfx) and DOS 0x27c63: below the full tank c[0x30] += the frame's
 * whole ticks >> 2 (no cap that frame), else = full. The 3D looks keep ticks / 4 (unchanged); the DOS look takes the
 * DOS rule at the agreed 30 fps: 182 / 30 = 6.07 ticks a frame - the clock's whole ticks 6 or 7, >> 2 = 1 a frame,
 * 30 a second (ticks / 4 is 45.5). The front end sets it by edition */
int combat_jet_refill_dos = 0;
int combat_ct_split = 3;   /* Mercs DOS 0x30482 / Windows 0x1000669d: rand(3), both retail builds. */
float combat_test_wheat[MEK_WEAPON_COUNT];   /* TEST ONLY */
int combat_heat_dos = 0;   /* retail DOS/Windows heat timer constants, same state machine */
const char *combat_damage_src = "weapon";   /* TEST ONLY: what the next damage is, for MW2_DAMAGE_LOG */
static void apply_damage_at(combat_unit *t, float dmg, int rear, int loc, unsigned *rng)
{
    int i, r, n;
    float *arm;
    static int dlog = -1;
    if (dlog < 0) dlog = getenv("MERCS_DAMAGE_LOG") != NULL;
    if (dlog && combat_damage_live && !t->invulnerable && dmg > 1e-7f)   /* TEST ONLY: every damage, by source ("wreck": a destroyed unit) */
        fprintf(stderr, "damage %s %s %s loc %d%s %.3f\n", combat_damage_src, t->destroyed ? "wreck" : t->human ? "player" : "ai", t->loadout, loc, rear ? "r" : "", (double)dmg);
    /* engine 0x100171d0: nothing for an invulnerable player, damage of 1e-7 or less, or a location outside 1-8 - and
     * nothing at all before the player's first start-up is done (DAT_1024c570; DOSBox WHITSCN1 / PLUMSCN1: the dropped
     * player lands hard, MECMTNHD, the damage display stays blue) */
    if (!combat_damage_live || t->invulnerable || !(dmg > 1e-7f) || loc < 1 || loc > 8) return;
    /* Mercs DOS 0x30482..0x304ba / Windows 0x10006693..0x10006701
     * both choose rand(3) -> LT / CT / RT. The old two-way mode remains only
     * for historical MW2 fixtures; no Mercs frontend selects it. */
    if (loc == 3) loc = 2 + rand_n(rng, combat_ct_split == 2 ? 2 : 3);
    i = loc - 1;
    t->damage_taken += (int)dmg;
    if (t->loc_gone[i]) return;                              /* already gone: no transfer (nothing more can happen) */
    arm = (rear && (loc == 2 || loc == 3 || loc == 4)) ? &t->rear[i] : &t->armor[i];
    *arm -= dmg;
    if (*arm >= 1e-7f) return;
    /* 0x1001732e: the first time a location's armour is used up (flag 0x4000, front and rear share it) the player, up
     * (state 2), hears MECRDWA1 0xec */
    if (!(t->breach_latch & (1u << i))) {
        t->breach_latch |= 1u << i;
        /* Mercs0x100065c4: player controller state2 only, once per
         * location. Starting, shut down and ejected states stay silent. */
        if (t->human && !t->offline && !t->shutdown && !t->destroyed && !t->intact_out)
            t->breach_sound = 1;
    }
    /* breached (armour at or below 0): the rest into this location's structure */
    t->internal[i] += *arm;
    *arm = 0;
    /* 0x10017383: the player's head (1) or centre torso (3, after the CT remap) breached by a hit of more than 2 points,
     * not yet destroyed (state 4): the red flash (DAT_1024cce0) */
    if (t->human && (loc == 1 || loc == 3) && !t->destroyed && dmg > 2.0f) t->red_flash = 1;
    if (t->internal[i] < 1e-7f) { destroy_location(t, loc, rng); return; }
    /* critical hits: rand(5) rolls (re-drawn each pass); rand(100) < 20 with items in the list is one: 12 destroys the
     * location, else rand(count) picks the entry */
    for (n = 0; n < rand_n(rng, 5); n++) {
        r = rand_n(rng, 100);
        if (r < 20 && t->slot_count[i] > 0) {
            if (r == 12) { destroy_location(t, loc, rng); return; }
            crit_item(t, loc, rand_n(rng, t->slot_count[i]), 0);
            if (t->destroyed || t->loc_gone[i]) return;
        }
    }
}

int combat_fire(combat_unit *u, combat_unit *t, float distance, float rel_angle, unsigned *rng)
{
    int rear;
    while (rel_angle > 180.0f) rel_angle -= 360.0f;
    while (rel_angle < -180.0f) rel_angle += 360.0f;
    rear = rel_angle > -90.0f && rel_angle < 90.0f;
    int i, dealt = 0;
    if (u->destroyed || t->destroyed) return 0;
    for (i = 0; i < u->weapon_count; i++) {
        combat_weapon *cw = &u->weapons[i];
        if (combat_weapon_ams(cw->weapon)) continue;
        const sim_weapon *sw = &g_weapons[cw->weapon];
        float range = range_cm(cw->weapon), p;
        /* trigger */
        if (cw->state == CW_READY && !u->shutdown && u->internal[cw->location] > 0 && distance <= range &&
            !(u->hold_fire_above > 0 && u->heat > u->hold_fire_above)) {
            start_volley(u, cw);
        }
        /* projectiles released since the last call (assumed: instant hit, range-based odds) */
        p = 0.85f - 0.35f * (distance / range);
        if (distance > range) p = 0;
        while (cw->pulses_due > 0) {
            cw->pulses_due--;
            if (frand(rng) < p) {
                apply_damage(t, sw->damage, rear, rng);
                dealt += (int)(sw->damage + 0.5f);
                t->heat += sw->target_heat;   /* PPC / flamer heat the target (engine 0x10044f60) */
            }
            if (t->destroyed) { cw->pulses_due = 0; break; }
        }
    }
    return dealt;
}

void combat_override(combat_unit *u)
{
    if (u->shutdown_pending && !u->shutdown) u->override = 1;
}

int combat_health(const combat_unit *u)
{
    int l;
    float h = 0;
    for (l = 0; l < MEK_LOC_COUNT; l++) h += u->armor[l] + u->rear[l] + u->internal[l];
    return (int)h;
}

int combat_out_of_weapons(const combat_unit *u)
{
    /* 0x10020880: 1 only when the unit HAS weapon slots (+0xa8 != 0) and none is usable (slot +8 != -1 and +0x20 != 0;
     * here a weapon left empty is state -1). An unarmed unit (a vehicle with no weapons, MARO's limousine) is not
     * "out of weapons" - it never takes the forced flee (0x1001ea40) */
    int i;
    if (u->weapon_count <= 0) return 0;
    for (i = 0; i < u->weapon_count; i++) if (u->weapons[i].state != -1) return 0;
    return 1;
}

float combat_max_range(const combat_unit *u)
{
    int i;
    float r = 0;
    for (i = 0; i < u->weapon_count; i++) if (range_cm(u->weapons[i].weapon) > r) r = range_cm(u->weapons[i].weapon);
    return r;
}

void combat_trigger(combat_unit *u, float distance) { combat_trigger_mask(u, distance, 0xFFFFFFFFu); }

void combat_trigger_mask(combat_unit *u, float distance, uint32_t mask)
{
    int i;
    if (u->destroyed || u->shutdown || u->offline) return;
    /* a human pilot (0x100437a0 / 0x10044440): a weapon fires on the press of its trigger, not while it is held (a
     * repeating weapon re-fires from the volley's end, see release_due), and with no range check */
    if (u->human) mask &= u->fire_edge;
    for (i = 0; i < u->weapon_count; i++) {
        combat_weapon *cw = &u->weapons[i];
        if (combat_weapon_ams(cw->weapon)) continue;
        if (i < 32 && !(mask & (1u << i))) continue;
        const sim_weapon *sw = &g_weapons[cw->weapon];
        if (cw->state == CW_READY && cw->ammo != 0 && u->internal[cw->location] > 0 && !u->loc_gone[cw->location] &&
            (u->human || distance <= range_cm(cw->weapon)) &&
            /* AI (0x100232b0): fire a weapon only if heat + its heat stays under 65 */
            !(u->hold_fire_above > 0 && u->heat + sw->heat >= u->hold_fire_above))
            start_volley(u, cw);
    }
}

/* 0x10043f80: the selection moves to the next weapon that is still there */
static void ai_next_weapon(combat_unit *u)
{
    int k, n = u->weapon_count, i = u->ai_sel;
    for (k = 1; k <= n; k++) {
        int j = (i + k) % n;
        if (!combat_weapon_ams(u->weapons[j].weapon) && u->weapons[j].state >= 0 && !u->loc_gone[u->weapons[j].location] && u->internal[u->weapons[j].location] > 0) { u->ai_sel = j; break; }
    }
}

/* engine 0x100232b0: the weapon that passes stays selected (+0xac), returned; -1 none */
static int ai_choose(combat_unit *u, float distance, unsigned (*rng)(void *ctx, unsigned n), void *ctx)
{
    int tries, n = u->weapon_count;
    if (u->destroyed || u->shutdown || u->offline || n <= 0) return -1;
    if (u->ai_sel < 0 || u->ai_sel >= n) u->ai_sel = 0;
    for (tries = 0; tries < n; tries++) {
        int i = u->ai_sel;
        combat_weapon *cw = &u->weapons[i];
        const sim_weapon *sw = &g_weapons[cw->weapon];
        int usable = !combat_weapon_ams(cw->weapon) && cw->state >= 0 && u->internal[cw->location] > 0 && !u->loc_gone[cw->location];
        /* 0x100151a0: 3 inside the minimum range (+0x3c), 2 inside the range, 1 at it, 0 beyond: fires on 1-2 */
        if (usable && distance >= (float)sw->field3c && distance <= range_cm(cw->weapon) &&
            sw->heat + u->heat < 65.0f && cw->state == CW_READY && cw->ammo != 0 &&
            rng(ctx, (unsigned)(combat_weapon_refire(cw->weapon) / 90 + 1)) == 0)
            return i;
        ai_next_weapon(u);
    }
    return -1;
}

int combat_ai_fire(combat_unit *u, float distance, unsigned (*rng)(void *ctx, unsigned n), void *ctx)
{
    int i = ai_choose(u, distance, rng, ctx);
    if (i >= 0) start_volley(u, &u->weapons[i]);
    return i;
}

int combat_ai_select(combat_unit *u, float distance, unsigned (*rng)(void *ctx, unsigned n), void *ctx)
{
    return ai_choose(u, distance, rng, ctx);
}

void combat_ai_trigger(combat_unit *u)
{
    /* engine 0x100437a0 (unit state 2), a unit other than the player: no group triggers (+0x27..+0x2a), so +0x25 alone:
     * pressed with the latch (+0x10c bit 1) clear - latch, and the selected weapon (+0xac) starts its volley if ready
     * (state 1: +0x24 = per volley, state 2, +0x10 = 0; the release loop then counts this frame's ticks, combat_tick);
     * released with the latch set - unlatch and 0x10043f80(0): the next weapon is selected */
    int n = u->weapon_count;
    if (u->destroyed || u->shutdown || u->offline || n <= 0) return;
    if (u->ai_sel < 0 || u->ai_sel >= n) u->ai_sel = 0;
    if (u->ai_trigger) {
        if (!u->ai_latch) {
            combat_weapon *cw = &u->weapons[u->ai_sel];
            u->ai_latch = 1;
            if (cw->state == CW_READY && !combat_weapon_ams(cw->weapon)) {
                cw->state = CW_FIRING;
                cw->shots = g_weapons[cw->weapon].per_volley;
                cw->t = 0;
                cw->volley_start = 1;
                cw->homing = u->lock_target;
            }
        }
    } else if (u->ai_latch) {
        u->ai_latch = 0;
        ai_next_weapon(u);
    }
    /* the volley's end (release_due) and the repeat's end (combat_tick) test the trigger: down -> state 3 / the
     * selected weapon fires again (0x100437a0 states 2 / 3) */
    u->fire_held = u->ai_trigger ? 1u << u->ai_sel : 0;
}

int combat_next_projectile_ex(combat_unit *u, int *first) { return combat_next_projectile_at(u, first, NULL); }
int combat_next_projectile_at(combat_unit *u, int *first, int *loc) { return combat_next_projectile_h(u, first, loc, NULL); }
float combat_last_age;
int combat_next_projectile_h(combat_unit *u, int *first, int *loc, int *homing)
{
    int i, k;
    for (i = 0; i < u->weapon_count; i++)
        if (u->weapons[i].pulses_due > 0) {
            combat_weapon *cw = &u->weapons[i];
            combat_last_age = cw->pulse_age[0];   /* first due, first out */
            for (k = 1; k < cw->pulses_due && k < 24; k++) cw->pulse_age[k - 1] = cw->pulse_age[k];
            u->weapons[i].pulses_due--;
            if (first) *first = u->weapons[i].volley_start;
            u->weapons[i].volley_start = 0;
            if (loc) *loc = u->weapons[i].location;
            if (homing) *homing = u->weapons[i].homing;
            return u->weapons[i].weapon;
        }
    return -1;
}

int combat_next_projectile(combat_unit *u) { return combat_next_projectile_ex(u, NULL); }
int combat_weapon_kind(int w) { return (w >= 0 && w < SIM_WEAPON_COUNT) ? g_weapons[w].type : -1; }
int combat_weapon_has_object(int w) { return w >= 0 && w < SIM_WEAPON_COUNT && !(g_weapons[w].type == 7 && g_weapons[w].visual < 0); }   /* +0x14: 0 only for the flamer (3D edition table) */
float combat_weapon_damage(int w) { return (w >= 0 && w < SIM_WEAPON_COUNT) ? g_weapons[w].damage : 0; }
int combat_weapon_sound(int w) { return (w >= 0 && w < SIM_WEAPON_COUNT) ? g_weapons[w].sound : 0; }
int combat_weapon_visual(int w) { return (w >= 0 && w < SIM_WEAPON_COUNT) ? g_weapons[w].visual : -1; }

void combat_damage_at(combat_unit *t, float dmg, int loc, unsigned *rng)
{
    const char *was = combat_damage_src;
    combat_damage_src = "area";
    apply_damage_at(t, dmg, 0, loc, rng);
    combat_damage_src = was;
}

int combat_hit(combat_unit *t, int w, int loc, float rel_angle, unsigned *rng)
{
    int rear;
    /* Mercs projectile flags+4 bit0x80, before the damage gate. The marker
     * persists even when ordinary direct damage is zero or disabled. */
    if (w == 28 || w == 61) t->narc_beacon = 1;
    if (t->destroyed || w < 0 || w >= SIM_WEAPON_COUNT) return 0;
    while (rel_angle > 180.0f) rel_angle -= 360.0f;
    while (rel_angle < -180.0f) rel_angle += 360.0f;
    rear = rel_angle > -90.0f && rel_angle < 90.0f;
    apply_damage_at(t, g_weapons[w].damage, rear, loc, rng);
    t->heat += g_weapons[w].target_heat;
    return (int)(g_weapons[w].damage + 0.5f);
}

int combat_crit_ammo(combat_unit *u)
{
    int l, k, b;
    if (u->destroyed) return 0;
    for (b = 0; b < u->bin_count; b++) {
        if (u->bins[b].id / 100 <= 100 || u->bins[b].rounds < 1) continue;   /* LRM 20 ammunition never explodes (0x1001680a) */
        for (l = 0; l < 8; l++) {
            if (u->loc_gone[l]) continue;
            for (k = 0; k < u->slot_count[l]; k++)
                if (u->slots[l][k] == u->bins[b].id) { crit_item(u, l + 1, k, 0); return 1; }
        }
    }
    return 0;
}

int combat_jettison(combat_unit *u, int wi)
{
    combat_weapon *cw;
    if (!u || wi < 0 || wi >= u->weapon_count) return 0;
    cw = &u->weapons[wi];
    if (g_weapons[cw->weapon].per_volley <= 0 || cw->ammo <= 0) return 0;   /* an energy weapon (-1) or empty */
    /* Mercs 0x100632e4: every bin linked to this TYPE POOL is emptied as
     * well. Leaving them live corrupts the returned ammunition loadout. */
    {
        int b;
        for (b = 0; b < u->bin_count; b++) {
            if (u->bins[b].weapon == cw->weapon)
                u->bins[b].rounds = 0;
        }
    }
    cw->ammo = 0;
    sync_ammo(u, cw);
    cw->state = -1; cw->shots = 0;
    return 1;
}

int combat_toggle_ams(combat_unit *u)
{
    int i, changed = 0;
    if (!u) return 0;
    for (i = 0; i < u->weapon_count; i++) {
        combat_weapon *w = &u->weapons[i];
        if (w->weapon != 29) continue;
        if (w->state == CW_READY) { w->state = CW_DISABLED; changed = 2; }
        else if (w->state == CW_DISABLED) { w->state = CW_READY; changed = 1; }
    }
    return changed;
}

static int ams_rounds(combat_unit *u, combat_weapon *w, unsigned (*random)(void *, unsigned), void *ctx)
{
    int n = 2, k;
    if (w->ammo == 0) return 0;
    if (w->ammo != -1 && !(u->human && u->unlimited_ammo)) {
        n = (int)random(ctx, 5) + 1;
        if (n > w->shots) n = w->shots;
        w->ammo -= n;
        if (w->ammo < 0) w->ammo = 0;
        /* Original AMS removes 1 bin round even if it spends up to5 pool
         * rounds. An already empty bin only advances the current pointer. */
        if (w->bin_first >= 0 && w->bin_cur >= 0 && w->bin_cur < w->bin_n &&
            w->bin_first + w->bin_cur < u->bin_count) {
            combat_bin *b = &u->bins[w->bin_first + w->bin_cur];
            if (b->rounds == 0) { if (w->bin_cur + 1 < w->bin_n) w->bin_cur++; }
            else b->rounds--;
        }
        /* The custom AMS callback only disables the releasing mount, unlike
         * the ordinary projectile callback's all-mount exhaustion loop. */
        for (k = 0; k < u->weapon_count; k++) if (u->weapons[k].weapon == w->weapon) {
            u->weapons[k].ammo = w->ammo; u->weapons[k].bin_cur = w->bin_cur;
        }
        sync_pool(u, w);
        if (w->ammo == 0) { w->state = -1; n = 0; }
    }
    return n;
}

int combat_ams_step(combat_unit *u, int slot, float ticks, int incoming,
                    unsigned (*random)(void *, unsigned), void *ctx,
                    void (*release)(void *, int, int))
{
    combat_weapon *w;
    int count = 0, interval;
    if (!u || !random || slot < 0 || slot >= u->weapon_count || ticks < 0 || u->destroyed) return 0;
    w = &u->weapons[slot];
    if (!combat_weapon_ams(w->weapon) || w->state < 0 || w->state == CW_DISABLED) return 0;
    if (u->shutdown || u->offline) { if (w->state != CW_FIRING) w->t += ticks; return 0; }
    /* The original pre-callback executes before the per-state switch. */
    if (w->state == CW_READY && incoming) {
        w->state = CW_FIRING; w->shots = g_weapons[w->weapon].per_volley;
        w->t = 0; w->homing = -2;
    }
    if (w->state == CW_RECYCLE || w->state == CW_REPEAT) {
        w->t += ticks;
        if (w->t >= combat_weapon_refire(w->weapon)) { w->state = CW_READY; w->t = 0; }
    } else if (w->state == CW_FIRING) {
        if (w->shots < 1) { w->state = CW_RECYCLE; w->t = 0; return 0; }
        w->t += ticks;
        interval = combat_weapon_interval(w->weapon);
        while (interval > 0 && w->t >= interval && w->shots > 0) {
            int result, have_pool = w->ammo != 0;
            result = ams_rounds(u, w, random, ctx);
            if (have_pool) { if (release) release(ctx, slot, result); count++; }
            if (!result) { w->shots = 0; break; }
            w->shots -= result; w->t -= interval;
        }
    }
    return count;
}

static int32_t original_distance(const int32_t v[3])
{
    int64_t a = llabs((int64_t)v[0]), b = llabs((int64_t)v[1]), c = llabs((int64_t)v[2]), t;
    if (a < b) { t = a; a = b; b = t; }
    if (a < c) { t = a; a = c; c = t; }
    t = (a * 4 + b + c) / 4;
    return t > INT32_MAX ? INT32_MAX : (int32_t)t;
}
static int ams_kind(int k) { return k == 3 || k == 4 || k == 9 || k == 10 || k == 12; }
static int ams_approach(const int32_t d[3], const int32_t v[3])
{
    uint32_t q = (uint32_t)d[0] * (uint32_t)v[0] + (uint32_t)d[1] * (uint32_t)v[1] + (uint32_t)d[2] * (uint32_t)v[2];
    return q > 0 && q < 0x80000000u;
}
int combat_ams_threat(int kind, int enemy, int active, const int32_t d[3], const int32_t v[3])
{
    return active && enemy && ams_kind(kind) && original_distance(d) <= 50000 && ams_approach(d, v);
}
int combat_ams_intercept(int kind, int enemy, int active, const int32_t d[3], const int32_t v[3],
                         int *distance, unsigned (*random)(void *, unsigned), void *ctx)
{
    int range, room;
    if (!distance || !random || !active || !enemy || !ams_kind(kind)) return 0;
    range = original_distance(d);
    if (range > 50000) return 0;
    if (*distance < 0) {
        room = range - 2500;
        if (room < 1) *distance = 0;
        else if (ams_approach(d, v)) {
            if (random(ctx, 100) < 76) *distance = room < 100 ? 2500 : (int)random(ctx, (unsigned)(room / 100)) * 100 + 2500;
            else *distance = 0;
        }
    }
    return *distance > 0 && range <= *distance;
}
void combat_guidance(const int32_t d[3], const int32_t v[3], int32_t a[3], int ticks, int torpedo, int dos)
{
    int i;
    int32_t dl = original_distance(d), vl = original_distance(v);
    int32_t factor = ticks > 0 ? (int32_t)((int64_t)ticks * 65536 / (dos ? 728 : 724)) : 0;
    /* Original divides by zero at an exactly coincident target. The native
     * caller has already set proximity; retain acceleration for that invalid
     * arithmetic case instead of introducing undefined behavior. */
    if (dl <= 0) return;
    for (i = 0; i < 3; i++) {
        int32_t desired = (int32_t)((int64_t)d[i] * 65536 / dl) - (vl > 0 ? (int32_t)((int64_t)v[i] * 65536 / vl) : v[i]);
        if (torpedo) {
            /* IMUL; SHRD16; ADC0 rounds the shifted product using bit15. */
            int64_t q = (int64_t)(desired - a[i]) * factor + 32768;
            int32_t step = q >= 0 ? (int32_t)(q / 65536) : (int32_t)(-((-q + 65535) / 65536));
            a[i] += step;
        } else a[i] = desired;
    }
}

int combat_ammo_total(const combat_unit *u)
{
    int i, total = 0;
    for (i = 0; i < u->ammo_pool_count; i++)
        if (u->ammo_pools[i].rounds > 0) total += u->ammo_pools[i].rounds;
    return total;
}

int combat_has_masc(const combat_unit *u)
{
    int l, k;
    for (l = 0; l < 8; l++) for (k = 0; k < MEK_SLOTS; k++) if (u->slots[l][k] >= MEK_EQ_MASC && u->slots[l][k] < MEK_EQ_TARGETING) return 1;
    return 0;
}

float combat_weapon_range(int w) { return (w >= 0 && w < SIM_WEAPON_COUNT) ? range_cm(w) : 0; }
float combat_weapon_speed(int w) { return (w >= 0 && w < SIM_WEAPON_COUNT) ? (float)g_weapons[w].speed : 0; }
int   combat_weapon_life(int w) { return weapon_time(w, 2); }
int   combat_weapon_drop(int w) { return (w >= 0 && w < SIM_WEAPON_COUNT) ? g_weapons[w].drop : 0; }

void combat_jets_forward(combat_unit *u, int on, float dt, float *hvel)
{
    u->jet_forward = on;
    if (on && u->jets > 0 && u->jet_fuel > 0 && !u->no_jump && !u->shutdown) *hvel += u->jet_ddy * dt * COMBAT_TICKS_PER_S;
}

int combat_jets(combat_unit *u, int jetting, float dt, unsigned *rng)
{
    const float g = 0.0296f * g_planet.gravity_g;
    float ticks = dt * COMBAT_TICKS_PER_S, a;
    /* engine 0x1001a58a: with the input held (and the mech up) the tank only drains - to 0, never refilling while
     * held; it refills a quarter tick per tick only with the input released; thrust (and jet heat) only while the
     * tank holds fuel after this frame's burn. A tank at -2 (no jets: gyro / last jet lost) does neither */
    int held = jetting && u->jets > 0 && !u->no_jump && !u->shutdown && !u->destroyed && u->jet_fuel >= 0;
    int jet;
    u->landed_vy = 0;
    if (u->body_detached) { u->vy = 0; u->jet_on = 0; return 0; }
    if (u->jets > 0 && u->jet_fuel >= 0) {
        if (held) {
            u->jet_fuel -= ticks;
            if (u->jet_fuel <= 0) u->jet_fuel = 0;
        } else if (combat_jet_refill_dos) {   /* DOS 0x27c57..0x27c7d, one step per 30 fps frame */
            int n;
            u->jet_refill_acc += dt * 30.0f;
            n = (int)u->jet_refill_acc;
            u->jet_refill_acc -= (float)n;
            while (n-- > 0) {
                if (u->jet_fuel < combat_jet_fuel_max) u->jet_fuel += (float)((int)(COMBAT_TICKS_PER_S / 30.0f) >> 2);   /* 6 >> 2 = 1 (7 >> 2 too) */
                else u->jet_fuel = combat_jet_fuel_max;
            }
        } else if (u->jet_fuel < combat_jet_fuel_max) {
            u->jet_fuel += ticks * 0.25f;
            if (u->jet_fuel > combat_jet_fuel_max) u->jet_fuel = combat_jet_fuel_max;
        }
    }
    jet = held && u->jet_fuel > 0;
    u->jet_on = jet;   /* 0x1001a180 (0x1001a679 / 0x1001a688): firing with fuel left clears object +0x80 bits 2 / 0 - the walk keys hold */
    if (jet) {
        u->heat += ticks * (float)u->jets * 0.005859375f;           /* 3/512 per jet per tick */
        if (u->no_heat) u->heat = 0;
    }
    /* standing / walking (engine 0x10019310 -> 0x10019e28): the unit stands only with its feet at or under the ground
     * (asked at its centre height, 0x10010080: the highest surface within 10 m of the centre, else the highest below) -
     * walked up a slope it is set on it; off anything lower, a downhill step too, it is airborne (+0x78 clear, 0x10019dde)
     * and falls at the planet's gravity until its feet reach the ground: no snap-down */
    if (!jet && fabsf(u->vy) < 1e-7f && u->y <= u->ground) { u->y = u->ground; u->vy = 0; return 0; }
    a = -g;
    if (jet) a = u->jet_ddy - g + (u->vy > 1e-7f ? (g - u->jet_ddy) / g_planet.jet_damping * u->vy * 3620.0f : 0);
    if (jet && u->jet_forward) a = u->vy > 1e-7f ? -g : 0;   /* forward thrust: no lift, level glide */
    u->y += (0.5f * a * ticks + u->vy) * ticks;
    u->vy += a * ticks;
    if (u->y <= u->ground) {
        if (u->vy < -16.154f && !u->no_collision_damage && !u->destroyed) {   /* fall damage to both legs (0x10019310: only with
                                                                          * Collision Damage on, DAT_1024ac6c + 3; a wreck lands
                                                                          * first - state 4 -> 0x1000c4d0 and return, 0x10019e71) */
            float dmg = (u->vy + 16.154f) * -1.5476f;
            combat_damage_src = "fall";
            apply_damage_at(u, dmg, 0, 8, rng);
            apply_damage_at(u, dmg, 0, 7, rng);
            combat_damage_src = "weapon";
        }
        u->landed_vy = u->vy;   /* the touch-down (0x10019e28..: sound and camera jolt when faster than 5.384 cm/tick) */
        u->y = u->ground;
        u->vy = 0;
        return 0;
    }
    return 1;
}

void combat_land(combat_unit *u, float vy, unsigned *rng)
{
    /* combat_jets' touch-down at vertical speed vy (cm/tick): fall damage to both legs beyond 16.154 cm/tick with Collision
     * Damage on, the landing speed kept for the sound (0x10019e54..0x10019ee0), stood on the ground */
    if (vy < -16.154f && !u->no_collision_damage && !u->destroyed) {
        float dmg = (vy + 16.154f) * -1.5476f;
        combat_damage_src = "fall";
        apply_damage_at(u, dmg, 0, 8, rng);
        apply_damage_at(u, dmg, 0, 7, rng);
        combat_damage_src = "weapon";
    }
    u->landed_vy = vy;
    u->y = u->ground;
    u->vy = 0;
}

int combat_jets_firing(const combat_unit *u, int jetting, float dt)
{
    int held = jetting && u->jets > 0 && !u->no_jump && !u->shutdown && !u->destroyed && u->jet_fuel >= 0;
    return held && u->jet_fuel - dt * COMBAT_TICKS_PER_S > 0;
}

float combat_jets_dy(const combat_unit *u, int jetting, float dt)
{
    /* combat_jets' vertical move for this frame without making it (engine 0x10019310: the move 0x1000b5e0 gets
     * (v + a ticks / 2) ticks in y with the x / z parts) - the same fuel, thrust and standing rules */
    const float g = 0.0296f * g_planet.gravity_g;
    float ticks = dt * COMBAT_TICKS_PER_S, a, fuel = u->jet_fuel;
    int held = jetting && u->jets > 0 && !u->no_jump && !u->shutdown && !u->destroyed && u->jet_fuel >= 0, jet;
    if (held && u->jets > 0 && fuel >= 0) { fuel -= ticks; if (fuel <= 0) fuel = 0; }
    jet = held && fuel > 0;
    if (!jet && fabsf(u->vy) < 1e-7f && u->y <= u->ground) return 0.0f;
    a = -g;
    if (jet) a = u->jet_ddy - g + (u->vy > 1e-7f ? (g - u->jet_ddy) / g_planet.jet_damping * u->vy * 3620.0f : 0);
    if (jet && u->jet_forward) a = u->vy > 1e-7f ? -g : 0;
    return (0.5f * a * ticks + u->vy) * ticks;
}

/* engine 0x1000c1f0: where a collision lands, by the contact normal (pointing from what was struck toward the unit, y up):
 * normal y below -0.866 (struck from above: the unit ran up into it) - the head, the damage x max(1, 2 x the other's
 * tonnage / its own) when the other is a unit; normal y above 0.7071 (ground-like, or on top of a unit) - half to each
 * leg; else by the bearing of the struck surface from the torso's facing: 45-135 degrees to a side that arm, else that
 * side's torso */
static void collide(combat_unit *u, float speed, float ny, float rel_deg, int other_tons, int latch, unsigned *rng)
{
    float dmg = (speed - 3.0693676f) * 0.050123077f * 3.0f;
    if (latch) u->blocked = 1;   /* terrain (0x10019d84 -> 0x1000c3c0) and a unit (0x10019d77 -> 0x1000c160) alike: every contact
                                  * frame - the impact cuts the velocity to a quarter (msim_world_impact), so pressing on stays
                                  * under the threshold; blocked = the contact (the player's impact-sound latch 0x1007c338) */
    if (dmg <= 0 || u->destroyed || u->invulnerable || u->no_collision_damage || getenv("MERCS_NO_COLLISION_DAMAGE")) return;   /* Combat Variables */
    if (ny < -0.8660254f) {                       /* from above: the head (0x1000c212) */
        if (other_tons > 0) {
            float k = 2.0f * (float)other_tons / (float)(u->tons > 0 ? u->tons : 1);
            if (k > 1.0f) dmg *= k;
        }
        apply_damage_at(u, dmg, 0, 1, rng);
        return;
    }
    if (ny > 0.70710677f) {                       /* ground-like: half to each leg */
        apply_damage_at(u, dmg * 0.5f, 0, 7, rng);
        apply_damage_at(u, dmg * 0.5f, 0, 8, rng);
        return;
    }
    while (rel_deg > 180.0f) rel_deg -= 360.0f;
    while (rel_deg < -180.0f) rel_deg += 360.0f;
    if (rel_deg < 0) apply_damage_at(u, dmg, 0, (rel_deg > -135.0f && rel_deg < -45.0f) ? 6 : 4, rng);   /* left arm / left torso (groups 6 / 4) */
    else apply_damage_at(u, dmg, 0, (rel_deg > 45.0f && rel_deg < 135.0f) ? 5 : 2, rng);                 /* right arm / right torso (groups 5 / 2) */
}

void combat_collision(combat_unit *u, float speed, float ny, float rel_deg, unsigned *rng)
{
    combat_damage_src = "terrain";
    collide(u, speed, ny, rel_deg, 0, 1, rng);
    combat_damage_src = "weapon";
}

void combat_collision_unit(combat_unit *u, float dv, const float n[3], float facing_deg, int other_tons, unsigned *rng)
{
    combat_damage_src = "unit";
    collide(u, dv, n[1], atan2f(-n[0], -n[2]) * 57.29578f - facing_deg, other_tons, 0, rng);
    combat_damage_src = "weapon";
}

float combat_armor_mult(int player, int alliance, int difficulty)
{
    /* engine 0x10041410 (0x1004144a..: DAT_1024bfc4 the player's, DAT_1024bfc0 another alliance's, local the player's
     * side's; applied at 0x100419b8..), DOS 0x4c760 (DAT_000961d8 / DAT_000961d4 / local): by Combat Variables
     * difficulty (DAT_1024ac6c + 5) 0 / 1 / 2. Another value leaves the statics as they were (4 / 4 as loaded): taken as 2 */
    static const float SIDE[3] = {3.0f, 3.0f, 4.0f}, OTHER[3] = {1.0f, 3.0f, 4.0f};
    if (difficulty < 0 || difficulty > 2) difficulty = 2;
    if (player) return 4.0f;
    return alliance == 0 ? SIDE[difficulty] : OTHER[difficulty];   /* 0x10019040: the unit's star's alliance */
}

void combat_set_armor_mult(combat_unit *u, float mult)
{
    /* engine 0x10041410 / DOS 0x4c760: after the MEK copy, armour and rear armour x the factor (16.16 in DOS);
     * the structure is not scaled. The damage display level is a ratio to the starting value (armor_max) */
    int l;
    for (l = 0; l < MEK_LOC_COUNT; l++) {
        u->armor[l] *= mult;
        u->rear[l] *= mult;
        u->armor_max[l] = (int)((float)u->armor_max[l] * mult);
        u->rear_max[l] = (int)((float)u->rear_max[l] * mult);
    }
    u->armor_mult *= mult;
}

void combat_set_player_difficulty(combat_unit *u, int difficulty)
{
    /* The unit was initialized with the AI / normal coefficient. */
    if (difficulty < 0 || difficulty > 2) difficulty = 1;
    u->sink_per_tick = u->sink_per_tick / sink_factor(1) * sink_factor(difficulty);
}
