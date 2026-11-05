/* mek.c - see mek.h. Mercenaries retail weapon/equipment data from MW2SHELL.DLL. */
#include "mek.h"
#include "prj.h"
#include "datapath.h"
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

const mek_weapon mek_weapons[MEK_WEAPON_COUNT] = {
    /* Retail MW2SHELL.DLL file 0x83958, 66 x 64-byte records: first nine
     * signed integers, then the full-name pointer (+0x24). */
    {"LRM 20 (C)", 6, -1, -1, 7, 14, 1000, 500, 4, 6},
    {"LRM 15 (C)", 5, -1, -1, 7, 14, 1000, 350, 2, 8},
    {"LRM 10 (C)", 4, -1, -1, 7, 14, 1000, 250, 1, 12},
    {"LRM 5 (C)", 2, -1, -1, 7, 14, 1000, 100, 1, 24},
    {"SRM 6 (C)", 4, -2, -1, 3, 6, 497, 150, 1, 15},
    {"SRM 4 (C)", 3, -2, -1, 3, 6, 497, 100, 1, 25},
    {"SRM 2 (C)", 2, -2, -1, 3, 6, 497, 50, 1, 50},
    {"Streak SRM-6 (C)", 4, -2, -1, 4, 8, 497, 300, 2, 15},
    {"Streak SRM-4 (C)", 3, -2, -1, 4, 8, 497, 200, 1, 25},
    {"Streak SRM-2 (C)", 2, -2, -1, 4, 8, 497, 100, 1, 50},
    {"Machine Gun (C)", 0, 2, -1, 1, 2, 175, 25, 1, 200},
    {"Gauss Rifle (C)", 1, 15, 2, 7, 15, 1820, 1200, 6, 8},
    {"LB 2-X AC (C)", 1, 2, 4, 10, 20, 800, 500, 3, 45},
    {"LB 5-X AC (C)", 1, 5, 3, 8, 15, 700, 700, 4, 20},
    {"LB 10-X AC (C)", 2, 10, -1, 6, 12, 600, 1000, 5, 10},
    {"LB 20-X AC (C)", 6, 20, -1, 4, 8, 450, 1200, 9, 5},
    {"Ultra AC/2 (C)", 1, 2, 2, 9, 18, 700, 500, 2, 45},
    {"Ultra AC/5 (C)", 1, 5, -1, 7, 14, 600, 700, 3, 20},
    {"Ultra AC/10 (C)", 3, 10, -1, 6, 12, 500, 1000, 4, 10},
    {"Ultra AC/20 (C)", 7, 20, -1, 4, 8, 400, 1200, 8, 5},
    {"Flamer (C)", 3, 2, -1, 2, 4, 10, 50, 1, 0},
    {"ER PPC (C)", 15, 15, -1, 7, 14, 746, 600, 2, 0},
    {"ER LLaser (C)", 12, 10, -1, 8, 15, 1019, 400, 1, 0},
    {"ER MLaser (C)", 5, 7, -1, 5, 10, 510, 100, 1, 0},
    {"ER SLaser (C)", 2, 5, -1, 2, 4, 255, 50, 1, 0},
    {"Pulse LLaser (C)", 10, 10, -1, 6, 14, 815, 600, 2, 0},
    {"Pulse MLaser (C)", 4, 7, -1, 4, 8, 408, 200, 1, 0},
    {"Pulse SLaser (C)", 2, 3, -1, 2, 4, 204, 100, 1, 0},
    {"NARC (C)", 0, 0, -1, 4, 8, 450, 200, 1, 6},
    {"AMS (C)", 1, 2, -1, -1, -1, -1, 50, 1, 20},
    {"Nuke (C)", 0, 0, -1, -1, -1, -1, 2000, 5, 1},
    {"Inferno SRM-2 (C)", 2, -2, -1, 3, 6, 497, 50, 1, 50},
    {"Torpedo (C)", 4, -2, -1, 4, 8, 497, 300, 2, 15},
    {"Inferno SSRM-2 (C)", 2, -2, -1, 4, 8, 497, 100, 1, 50},
    {"ARROW IV (C)", 10, -20, -1, -1, -1, 1000, 1200, 12, 5},
    {"LRM 20", 6, -1, 6, 7, 14, 630, 1000, 5, 6},
    {"LRM 15", 5, -1, 6, 7, 14, 630, 700, 3, 8},
    {"LRM 10", 4, -1, 6, 7, 14, 630, 500, 2, 12},
    {"LRM 5", 2, -1, 6, 7, 14, 630, 200, 1, 24},
    {"SRM 6", 4, -2, -1, 3, 6, 270, 300, 2, 15},
    {"SRM 4", 3, -2, -1, 3, 6, 270, 200, 1, 25},
    {"SRM 2", 2, -2, -1, 3, 6, 270, 100, 1, 50},
    {"Streak SRM 2", 2, -2, -1, 3, 6, 270, 150, 1, 50},
    {"Machine Gun", 0, 2, -1, 1, 2, 90, 50, 1, 200},
    {"Gauss Rifle", 1, 15, 2, 7, 15, 660, 1500, 7, 8},
    {"Autocannon/2", 1, 2, 4, 8, 16, 720, 600, 1, 45},
    {"Autocannon/5", 1, 5, 3, 6, 12, 540, 800, 4, 20},
    {"Autocannon/10", 3, 10, -1, 5, 10, 450, 1200, 7, 10},
    {"Autocannon/20", 7, 20, -1, 3, 6, 270, 1400, 10, 5},
    {"Ultra AC/5", 1, 5, 2, 6, 13, 600, 900, 5, 20},
    {"LB 10-X AC", 2, 10, -1, 6, 12, 540, 1100, 6, 10},
    {"Flamer", 3, 2, -1, 1, 2, 90, 100, 1, -1},
    {"ER PPC", 15, 10, -1, 7, 14, 690, 700, 3, -1},
    {"PPC", 10, 10, 3, 6, 12, 540, 700, 3, -1},
    {"ER LLaser", 12, 8, -1, 7, 14, 570, 500, 2, -1},
    {"Laser (Large)", 8, 8, -1, 5, 10, 450, 500, 2, -1},
    {"Laser (Medium)", 3, 5, -1, 3, 6, 270, 100, 1, -1},
    {"Laser (Small)", 1, 3, -1, 1, 2, 90, 50, 1, -1},
    {"Pulse LLaser", 10, 9, -1, 3, 7, 300, 700, 2, -1},
    {"Pulse MLaser", 4, 6, -1, 2, 4, 180, 200, 1, -1},
    {"Pulse SLaser", 2, 3, -1, 1, 2, 90, 100, 1, -1},
    {"NARC", 0, 0, -1, 3, 6, 270, 300, 2, 6},
    {"AMS", 1, 2, -1, -1, -1, -1, 50, 1, 12},
    {"Inferno SRM 2", 2, -2, -1, 3, 6, 270, 100, 1, 50},
    {"Inferno Strk SRM 2", 2, -2, -1, 3, 6, 270, 150, 1, 50},
    {"ARROW IV", 10, -20, -1, -1, -1, 3000, 1500, 15, 5},
};
const int mek_weapon_count = (int)(sizeof mek_weapons / sizeof mek_weapons[0]);

/* Retail shell file 0x813e8, 24 x {id, name pointer, cost}; engine
 * critical dispatch 0x10006412..0x10006536 confirms the active IDs. */
static const struct { uint32_t id; const char *name; } equipment[] = {
    {7000, "MASC"},
    {7050, "Targeting Computer"},
    {7100, "ECM"},
    {7150, "Artemis IV"},
    {7200, "Beagle Active Probe"},
    {7250, "TAG"},
    {7300, "Shoulder"},
    {7350, "Upper Arm Actuator"},
    {7400, "Lower Arm Actuator"},
    {7450, "Hand Actuator"},
    {7500, "Hip"},
    {7550, "Upper Leg Actuator"},
    {7600, "Lower Leg Actuator"},
    {7650, "Foot Actuator"},
    {7700, "Sensors"},
    {7750, "Cockpit"},
    {7800, "Gyro"},
    {7850, "Engine"},
    {7900, "Life Support"},
    {8000, "Heat Sink"},
    {8500, "Jump Jet"},
    {9000, "Endo Steel"},
    {9500, "Ferro-Fibrous"},
    {9550, "CASE"},
};
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

#define MEK_HEADER 0x18
#define MEK_LOC_SIZE 40
#define MEK_ITEMS_AT (MEK_HEADER + MEK_LOC_COUNT * MEK_LOC_SIZE)
#define MEK_NAME_LEN 50

int mek_parse(const uint8_t *d, size_t len, mek_def *m)
{
    uint32_t items;
    uint16_t links;
    size_t need, i, k;

    memset(m, 0, sizeof *m);
    if (len < MEK_ITEMS_AT + MEK_NAME_LEN) return -1;
    items = rd32(d + 0x10);
    links = rd16(d + 0x14);
    if (items > MEK_MAX_ITEMS || links > MEK_MAX_ITEMS - items) return -1;
    need = MEK_ITEMS_AT + (size_t)(items + links) * 8 + MEK_NAME_LEN;
    /* Mercs has both legacy 50-byte and shell-generated 58-byte config areas.
     * Do not mistake the extra eight bytes for an uncounted weapon/ammo link.
     * The original shell reads/writes 0x3a bytes at 0x1000ae80/0x1000b9a0. */
    if (len == need + sizeof m->config_extension) {
        m->has_config_extension = 1;
        memcpy(m->config_extension, d + need, sizeof m->config_extension);
        need += sizeof m->config_extension;
    }
    if (len < need && (need - len) % 8 == 0 && (need - len) / 8 <= links) {
        /* two 3D-edition records (GRG03STD, STM04STD) are one entry shorter than their counts: the trailing ammo
         * link(s) taken as missing (ASSUMED) */
        links = (uint16_t)(links - (need - len) / 8);
        need = len;
    }
    if (len != need) return -1;

    m->tonnage = rd32(d);
    m->walk_mp = rd32(d + 4);
    m->jump_mp = rd32(d + 8);
    m->heat_sinking = rd32(d + 12);
    for (i = 0; i < MEK_LOC_COUNT; i++) {
        const uint8_t *p = d + MEK_HEADER + i * MEK_LOC_SIZE;
        mek_loc *l = &m->loc[i];
        l->armor = rd32(p);
        l->rear_armor = rd32(p + 4);
        l->internal = rd32(p + 8);
        for (k = 0; k < MEK_SLOTS; k++) l->slots[k] = rd16(p + 12 + k * 2);
        l->slot_count = rd16(p + 36);
        l->flags = rd16(p + 38);
        if (l->slot_count > MEK_SLOTS) return -1;
    }
    for (i = 0; i < items + links; i++) {
        m->items[i].id = rd32(d + MEK_ITEMS_AT + i * 8);
        m->items[i].linked_id = (int32_t)rd32(d + MEK_ITEMS_AT + i * 8 + 4);
    }
    m->item_count = (int)items;
    m->link_count = links;
    m->unknown16 = rd16(d + 0x16);
    memcpy(m->config_raw, d + MEK_ITEMS_AT + (size_t)(items + links) * 8, MEK_NAME_LEN);
    memcpy(m->config_name, m->config_raw, MEK_NAME_LEN);
    m->config_name[MEK_NAME_LEN] = '\0';
    return 0;
}

static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
size_t mek_write(const mek_def *m, uint8_t *out, size_t cap)
{
    size_t config_at, n, i, k;
    int any_raw = 0;
    if (m->item_count < 0 || m->link_count < 0 || m->item_count > MEK_MAX_ITEMS ||
        m->link_count > MEK_MAX_ITEMS - m->item_count) return 0;
    config_at = MEK_ITEMS_AT + (size_t)(m->item_count + m->link_count) * 8;
    n = config_at + MEK_NAME_LEN + (m->has_config_extension ? sizeof m->config_extension : 0);
    if (cap < n) return 0;
    memset(out, 0, n);
    wr32(out, m->tonnage); wr32(out + 4, m->walk_mp); wr32(out + 8, m->jump_mp); wr32(out + 12, m->heat_sinking);
    wr32(out + 0x10, (uint32_t)m->item_count); wr16(out + 0x14, (uint16_t)m->link_count); wr16(out + 0x16, m->unknown16);
    for (i = 0; i < MEK_LOC_COUNT; i++) {
        uint8_t *p = out + MEK_HEADER + i * MEK_LOC_SIZE;
        const mek_loc *l = &m->loc[i];
        wr32(p, l->armor); wr32(p + 4, l->rear_armor); wr32(p + 8, l->internal);
        for (k = 0; k < MEK_SLOTS; k++) wr16(p + 12 + k * 2, l->slots[k]);
        wr16(p + 36, l->slot_count); wr16(p + 38, l->flags);
    }
    for (i = 0; i < (size_t)(m->item_count + m->link_count); i++) {
        wr32(out + MEK_ITEMS_AT + i * 8, m->items[i].id);
        wr32(out + MEK_ITEMS_AT + i * 8 + 4, (uint32_t)m->items[i].linked_id);
    }
    for (i = 0; i < MEK_NAME_LEN; i++) if (m->config_raw[i]) any_raw = 1;
    if (any_raw) memcpy(out + config_at, m->config_raw, MEK_NAME_LEN);
    else { size_t L = strlen(m->config_name); memcpy(out + config_at, m->config_name, L < MEK_NAME_LEN ? L : MEK_NAME_LEN - 1); }
    if (m->has_config_extension) memcpy(out + config_at + MEK_NAME_LEN, m->config_extension, sizeof m->config_extension);
    return n;
}

int mek_load(struct prj_archive *a, const char *name, mek_def *out)
{
    prj_record r;
    char path[64];
    unsigned char *data = NULL;
    size_t len = 0;
    int rc;
    if (a && prj_read_named((prj_archive *)a, "MEK", name, &r) == PRJ_OK) {
        rc = mek_parse(r.data, r.size, out);
        prj_record_free(&r);
        return rc;
    }
    snprintf(path, sizeof path, "MEK\\%s.MEK", name);
    if (dp_read_file(path, &data, &len) != 0) return -1;
    rc = mek_parse(data, len, out);
    free(data);
    return rc;
}

const mek_weapon *mek_weapon_for(uint32_t id)
{
    if (id >= 10000 || id % 100 == 0) return NULL;
    return (id / 100 < (uint32_t)mek_weapon_count) ? &mek_weapons[id / 100] : NULL;
}

const char *mek_item_name(uint32_t id, char *buf, size_t buflen)
{
    const mek_weapon *w;
    size_t i;

    if (id == 0) return "-";
    if (id >= 10000 && (w = mek_weapon_for(id - 10000)) != NULL) {
        snprintf(buf, buflen, "%s ammo", w->name);
        return buf;
    }
    if ((w = mek_weapon_for(id)) != NULL) return w->name;
    /* equipment: largest base id not above this one */
    for (i = sizeof equipment / sizeof equipment[0]; i-- > 0;) {
        if (id >= equipment[i].id) {
            uint32_t extra = id - equipment[i].id;
            if (extra == 0 || equipment[i].id >= MEK_EQ_HEAT_SINK) return equipment[i].name;
            snprintf(buf, buflen, "%s (%s)", equipment[i].name,
                     extra == 1 ? "L" : extra == 2 ? "R" : "?");
            return buf;
        }
    }
    snprintf(buf, buflen, "item %u", (unsigned)id);
    return buf;
}

const char *mek_location_name(int loc)
{
    static const char *const n[] = {"Head", "Right Torso", "Center Torso", "Left Torso",
                                    "Right Arm", "Left Arm", "Right Leg", "Left Leg"};
    return (loc >= 0 && loc < MEK_LOC_COUNT) ? n[loc] : "?";
}
