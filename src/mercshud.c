#include "mercshud.h"
#include "mercsfile.h"
#include <string.h>

static int32_t hud_i32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 |
                     (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

int mercs_hud_damage_load(prj_archive *a, int set, mercs_hud_damage *out)
{
    prj_record r;
    int i, j, id, type, found = 0;
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!a || set < 0 || set > 2 || prj_read_named(a, "HUD", "MW2HUD", &r) != PRJ_OK) return -1;
    if (r.size < 13u * 4u + 15u * 16u) { prj_record_free(&r); return -1; }
    id = hud_i32(r.data + 40);
    type = prj_find_type(a, "SHP");
    if (id >= 0 && id <= 65533 && type >= 0)
        for (i = 0; i < prj_symbol_count(a, type); i++)
            if (prj_symbol_id(a, type, i) == id + set) {
                const char *name = prj_symbol_name(a, type, i);
                if (name) { strncpy(out->shape, name, PRJ_NAME_LEN); found = 1; }
                break;
            }
    /* Original HUD loader 0x1004770a..0x1004784f: fifteen rectangles after
     * ten coordinates and three shape IDs; an invalid component clears its
     * whole rectangle. The drawing loop's sixteenth slot is empty. */
    for (i = 0; i < 15; i++) {
        int valid = 1;
        for (j = 0; j < 4; j++) {
            int v = hud_i32(r.data + 52 + i * 16 + j * 4);
            out->regions[i][j] = v;
            if (v < 0 || v > 100) valid = 0;
        }
        if (!valid) memset(out->regions[i], 0, sizeof out->regions[i]);
    }
    prj_record_free(&r);
    return found ? 0 : -1;
}

const mercs_hud_window *mercs_hud_instrument(int instrument)
{
    /* Windows 22, 26, 32, 33 and 16.16 text references. The dH/dT
     * renderer uses window25's reference (0, 0.5 of its 52-unit height),
     * evaluated separately by the caller; its own reference is (0,0). */
    static const mercs_hud_window w[MERCS_HUD_INSTRUMENT_COUNT] = {
        {258, 95, 309, 129, 0, 0x4000},
        {10, 95, 61, 129, 0x3333, 0x4000},
        {132, 145, 183, 179, 0, 0},
        {193, 145, 244, 179, 0, 0}
    };
    return instrument >= 0 && instrument < MERCS_HUD_INSTRUMENT_COUNT ? &w[instrument] : NULL;
}

static int compass_pixels(int width, int degrees)
{
    int64_t product;
    if (width <= 0) return 0;
    /* Original IMUL / SHRD 16 / ADC 0 rounds ties toward positive infinity,
     * including negative bearings. Use division with an explicit floor so
     * this does not rely on the host's signed-right-shift behavior. */
    product = ((int64_t)width * 65536 / 360) * degrees + 32768;
    return (int)(product >= 0 ? product / 65536 : -((-product + 65535) / 65536));
}

int mercs_hud_compass_tape_x(int center, int sprite_width, int heading_degrees)
{
    int heading = heading_degrees % 360;
    if (heading < 0) heading += 360;
    return center - compass_pixels(sprite_width, heading);
}

int mercs_hud_compass_marker_x(int center, int sprite_width, int relative_degrees)
{
    return center + compass_pixels(sprite_width, relative_degrees);
}

/* Left arm/torso/leg weapons fill windows 3-9 in record order; right mounts
 * fill 10-16. A full column continues in the other column's available rows.
 * Centre torso and head weapons alternate between the next free windows. */
int mercs_weapon_window(const combat_unit *pu, int wi)
{
    int pos[HUD_WEAPON_COUNT], k, L = HUD_WEAPON_FIRST, R, nright = 0, b = 1;
    int n = pu->weapon_count < HUD_WEAPON_COUNT ? pu->weapon_count : HUD_WEAPON_COUNT;
    int ty[HUD_WEAPON_COUNT];
    for (k = 0; k < n; k++) {
        int loc = pu->weapons[k].location;
        ty[k] = loc == MEK_RL ? MEK_RT : loc == MEK_LL ? MEK_LT : loc;
        pos[k] = HUD_WEAPON_FIRST + (k % 2) * HUD_WEAPON_ROWS + k / 2;
        if (ty[k] == MEK_RA || ty[k] == MEK_RT) nright++;
    }
    R = HUD_WEAPON_RIGHT + nright;
    for (k = 0; k < n; k++) if (ty[k] == MEK_LA || ty[k] == MEK_LT) { pos[k] = L++; if (L == HUD_WEAPON_RIGHT) L = R; }
    R = HUD_WEAPON_RIGHT;
    for (k = 0; k < n; k++) if (ty[k] == MEK_RA || ty[k] == MEK_RT) { pos[k] = R++; if (R > HUD_WEAPON_LAST) R = L; }
    for (k = 0; k < n; k++) {
        int nl = L, nr = R;
        if (ty[k] != MEK_CT && ty[k] != MEK_HEAD) continue;
        if (b && L < R) { if (R == HUD_TARGET_VIEW) nl = L + 1; else { nr = R + 1; L = R; } b = 0; }
        else { if (L == HUD_WEAPON_RIGHT) { L = R; nr = R + 1; } else nl = L + 1; b = 1; }
        pos[k] = L;
        L = nl; R = nr;
    }
    k = wi >= 0 && wi < n ? pos[wi] : HUD_WEAPON_FIRST + (wi % 2) * HUD_WEAPON_ROWS + wi / 2;
    return k < HUD_WEAPON_FIRST ? HUD_WEAPON_FIRST : k > HUD_WEAPON_LAST ? HUD_WEAPON_LAST : k;
}

/* The weapon after `wi` in panel order: row by row, left then right (windows
 * 3-9 and 10-16; MERCSW.DLL 0x1005c74e..0x1005c8e6), skipping empty slots and
 * destroyed weapons (state -1); the port keeps the weapons in record order and places them by location
 * (weapon_window), so the step follows the windows. ready_only: the next ready weapon with ammunition from wi on
 * (chain fire), -1 if none. */
int mercs_weapon_next(const combat_unit *pu, int wi, int include_self, int ready_only)
{
    int n = pu->weapon_count < COMBAT_MAX_WEAPONS ? pu->weapon_count : COMBAT_MAX_WEAPONS, ord[COMBAT_MAX_WEAPONS], k, j, at = -1, step;
    if (n <= 0) return -1;
    for (k = 0; k < n; k++) ord[k] = k;
    for (k = 1; k < n; k++) {   /* by row, then column */
        int v = ord[k], wv = mercs_weapon_window(pu, v), kv = ((wv - HUD_WEAPON_FIRST) % HUD_WEAPON_ROWS) * 2 + (wv >= HUD_WEAPON_RIGHT);
        for (j = k - 1; j >= 0; j--) {
            int wj = mercs_weapon_window(pu, ord[j]), kj = ((wj - HUD_WEAPON_FIRST) % HUD_WEAPON_ROWS) * 2 + (wj >= HUD_WEAPON_RIGHT);
            if (kj <= kv) break;
            ord[j + 1] = ord[j];
        }
        ord[j + 1] = v;
    }
    for (k = 0; k < n; k++) if (ord[k] == wi) at = k;
    if (at < 0) at = n - 1;
    for (step = include_self ? 0 : 1; step <= n; step++) {
        const combat_weapon *cw = &pu->weapons[ord[(at + step) % n]];
        if (cw->state < 0 || (cw->location >= 0 && cw->location < 8 && pu->loc_gone[cw->location])) continue;
        if (ready_only && (cw->state != CW_READY || cw->ammo == 0)) continue;
        return ord[(at + step) % n];
    }
    return ready_only ? -1 : wi;
}

void mercs_hud_defaults(mercs_hud_settings *s)
{
    static const int32_t defaults[MERCS_MFD_COUNT] = {1, 8, 9, 10, 0, 12, 13};
    memcpy(s->mode, defaults, sizeof defaults);
    s->weapons = s->compass = s->radar = 1;
}

int mercs_hud_command(mercs_hud_settings *s, unsigned command)
{
    static const int order[7] = {MERCS_MFD_MAIN, MERCS_MFD_A, MERCS_MFD_B,
        MERCS_MFD_C, MERCS_MFD_D, MERCS_MFD_E, MERCS_MFD_F};
    int mode;
    if (command >= 2 && command <= 8) {
        int slot = order[command - 2];
        s->mode[slot] = (s->mode[slot] + 1) % MERCS_MFD_MODES;
        return 1;
    }
    if (command == 9) { mercs_hud_defaults(s); return 1; }
    if (command == 11) { s->compass = !s->compass; return 1; }
    if (command == 12) { s->weapons = !s->weapons; return 1; }
    switch (command) {
    case 13: mode = MERCS_MFD_HTAL; break;
    case 14: mode = MERCS_MFD_REAR; break;
    case 15: mode = MERCS_MFD_DOWN; break;
    case 16: mode = MERCS_MFD_WEAPON; break;
    case 17: mode = MERCS_MFD_DAMAGE; break;
    default: return 0;
    }
    s->mode[MERCS_MFD_MAIN] = s->mode[MERCS_MFD_MAIN] == mode ? MERCS_MFD_OFF : mode;
    return 1;
}

static int hud_settings_valid(const mercs_hud_settings *s)
{
    int i;
    for (i = 0; i < MERCS_MFD_COUNT; i++) if (s->mode[i] < 0 || s->mode[i] >= MERCS_MFD_MODES) return 0;
    return s->radar >= 0 && s->radar <= 4;
}

int mercs_hud_decode(mercs_hud_settings *out, const uint8_t *data, size_t size)
{
    mercs_hud_settings s;
    int i;
    if (!out || !data || (size != 37 && size != 40)) return -1;
    for (i = 0; i < MERCS_MFD_COUNT; i++) s.mode[i] = hud_i32(data + 4 * i);
    s.weapons = hud_i32(data + 28); s.compass = hud_i32(data + 32);
    s.radar = size == 37 ? data[36] : hud_i32(data + 36);
    if (!hud_settings_valid(&s)) return -1;
    *out = s; return 0;
}

size_t mercs_hud_encode(const mercs_hud_settings *s, int windows, uint8_t data[40])
{
    int i, k;
    if (!s || !data || !hud_settings_valid(s)) return 0;
    for (i = 0; i < 10; i++) {
        uint32_t v = (uint32_t)(i < 7 ? s->mode[i] : i == 7 ? s->weapons : i == 8 ? s->compass : s->radar);
        for (k = 0; k < 4; k++) data[4 * i + k] = (uint8_t)(v >> (8 * k));
    }
    return windows ? 40 : 37;
}

int mercs_hud_load_settings(mercs_hud_settings *s)
{
    unsigned char *data = NULL;
    size_t size = 0;
    int result;
    if (dp_read_file("MERCS.HUD", &data, &size)) return -1;
    result = mercs_hud_decode(s, data, size); free(data); return result;
}

int mercs_hud_save_settings(const mercs_hud_settings *s, const char *directory,
                           int windows, char *error, size_t error_size)
{
    uint8_t data[40];
    size_t size = mercs_hud_encode(s, windows, data);
    if (!size) { if (error && error_size) snprintf(error,error_size,"Invalid HUD configuration"); return -1; }
    return mercs_atomic_write(directory, "MERCS.HUD", data, size, error, error_size);
}
