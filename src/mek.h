/*
 * mek.h - Mercenaries mech/vehicle definitions (MEK resources in MW2.PRJ).
 *
 * Little-endian layout inherited from the MW2 port, with Mercenaries weapon
 * and equipment IDs and both verified Mercs configuration-trailer lengths.
 * The foundation's record counts and full-game validation do not describe
 * the larger Mercenaries archive.
 *
 *   0x00  u32 tonnage
 *   0x04  u32 walk_mp
 *   0x08  u32 jump_mp
 *   0x0C  u32 heat_sinking     (dissipation; 2 x double heat sinks on most mechs)
 *   0x10  u32 item_count
 *   0x14  u16 link_count       (ammo bins; each references its weapon)
 *   0x16  u16 unknown
 *   0x18  8 x 40-byte locations, in order H, RT, CT, LT, RA, LA, RL, LL:
 *           u32 armor, u32 rear_armor, u32 internal,
 *           u16 slots[12]      (equipment id per critical slot, 0 = empty)
 *           u16 slot_count     (6 for head and legs, 12 elsewhere)
 *           u16 flags          (meaning unconfirmed)
 *   0x158 (item_count + link_count) x { u32 id, i32 linked_id (-1 = none) }
 *   then  a 50-byte config area, or 58 bytes for Mercs shell-format designs.
 *         Retail MW2SHELL.DLL 0x1000ae80/0x1000b9a0 reads/writes 58 bytes;
 *         the archive contains both lengths. Preserve the extension verbatim.
 *
 * Equipment ids:
 *   weapons     index*100 + instance   (index into mek_weapons[], 0..65)
 *   ammo        10000 + weapon id
 *   equipment   7000-9999: base id + instance/side (+1/+2 on limbs)
 */
#ifndef MW2_MEK_H
#define MW2_MEK_H

#include <stddef.h>
#include <stdint.h>

/* location index = OBJL group - 1. Sides as MW2SHELL names them (0x7f57c) and the models confirm (TW1_RARM is group 5,
 * TW1RULEG group 7): 1 right torso, 3 left torso, 4 right arm, 5 left arm, 6 right leg, 7 left leg */
enum mek_location { MEK_HEAD, MEK_RT, MEK_CT, MEK_LT, MEK_RA, MEK_LA, MEK_RL, MEK_LL, MEK_LOC_COUNT };

#define MEK_SLOTS 12
#define MEK_MAX_ITEMS 64
#define MEK_WEAPON_COUNT 66
#define MEK_CONFIG_MAX_BYTES 58
#define MEK_MAX_SERIALIZED_BYTES (24 + MEK_LOC_COUNT * 40 + MEK_MAX_ITEMS * 8 + MEK_CONFIG_MAX_BYTES)

/* Retail MW2SHELL.DLL equipment table 0x10082be8; runtime critical dispatch
 * MERCSW.DLL 0x10006412. Keep serialized IDs intact. */
enum mek_equipment {
    MEK_EQ_MASC = 7000, MEK_EQ_TARGETING = 7050, MEK_EQ_ECM = 7100,
    MEK_EQ_ARTEMIS = 7150, MEK_EQ_PROBE = 7200, MEK_EQ_TAG = 7250,
    MEK_EQ_SHOULDER = 7300, MEK_EQ_UPPER_ARM = 7350,
    MEK_EQ_LOWER_ARM = 7400, MEK_EQ_HAND = 7450,
    MEK_EQ_HIP = 7500, MEK_EQ_UPPER_LEG = 7550,
    MEK_EQ_LOWER_LEG = 7600, MEK_EQ_FOOT = 7650,
    MEK_EQ_SENSORS = 7700, MEK_EQ_COCKPIT = 7750,
    MEK_EQ_GYRO = 7800, MEK_EQ_ENGINE = 7850, MEK_EQ_LIFE_SUPPORT = 7900,
    MEK_EQ_HEAT_SINK = 8000, MEK_EQ_JUMP_JET = 8500,
    MEK_EQ_ENDO = 9000, MEK_EQ_FERRO = 9500, MEK_EQ_CASE = 9550
};

typedef struct {
    uint32_t armor, rear_armor, internal;
    uint16_t slots[MEK_SLOTS];
    uint16_t slot_count;
    uint16_t flags;
} mek_loc;

typedef struct {
    uint32_t id;
    int32_t  linked_id;   /* for ammo: the weapon it feeds; -1 otherwise */
} mek_item;

typedef struct {
    uint32_t tonnage, walk_mp, jump_mp, heat_sinking;
    mek_loc  loc[MEK_LOC_COUNT];
    mek_item items[MEK_MAX_ITEMS];
    int      item_count;     /* weapons + equipment */
    int      link_count;     /* ammo entries, stored after the items */
    char     config_name[51];
    uint16_t unknown16;      /* 0x16, kept for an exact round trip */
    uint8_t  config_raw[50]; /* the name field as stored (bytes after the NUL included) */
    uint8_t  config_extension[8]; /* additional Mercs shell-format bytes, uninterpreted */
    uint8_t  has_config_extension; /* serialize the original 58-byte rather than 50-byte area */
} mek_def;

typedef struct {
    const char *name;
    int heat;
    int damage;          /* per shot; negative = per missile (LRM -1, SRM -2) */
    int unknown;         /* -1 for most; set on Gauss and some autocannon (meaning unconfirmed) */
    int short_range;     /* BattleTech range brackets, hexes */
    int medium_range;
    int max_range_m;     /* in-game range, metres */
    int weight_x100;     /* tons x 100 */
    int crits;
    int ammo_per_ton;    /* 0 for energy weapons */
} mek_weapon;

extern const mek_weapon mek_weapons[];
extern const int mek_weapon_count;

/* Parse a MEK payload. Returns 0 on success, -1 if malformed. */
int mek_parse(const uint8_t *data, size_t len, mek_def *out);
/* Load a loadout by name: the archive's MEK record, else a user design MEK\<name>.MEK in the install / disc folders
 * (datapath roots; the Mech Lab saves them there). Returns 0 and fills *out. */
struct prj_archive;
int mek_load(struct prj_archive *a, const char *name, mek_def *out);
/* Serialise (the exact inverse of mek_parse: every retail record round-trips byte for byte). Returns the length
 * written, or 0 if cap is too small. Set config_name and clear config_raw to name a new design. */
size_t mek_write(const mek_def *m, uint8_t *out, size_t cap);

/* Human-readable name for any equipment id (weapons, ammo, internals). */
const char *mek_item_name(uint32_t id, char *buf, size_t buflen);

/* Weapon record for a weapon id, or NULL if the id isn't a weapon. */
const mek_weapon *mek_weapon_for(uint32_t id);

const char *mek_location_name(int loc);

#endif
