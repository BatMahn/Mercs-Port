#ifndef MERCS_RETURN_H
#define MERCS_RETURN_H

#include <stddef.h>
#include <stdint.h>
#include "combat.h"

/* Mercs DOS MW2.EXE 0x636a0 / retail MERCSW.DLL 0x1006b810.
 * These are portable values at the original serializer boundary, not packed
 * copies of the original controller. They also make the original-code oracle
 * independent of the native combat representation. No file I/O or campaign
 * state changes occur here. */
#define MERCS_RETURN_MOUNTS 14
#define MERCS_RETURN_BINS 25
#define MERCS_RETURN_POOLS 14
#define MERCS_RETURN_POOL_BINS 10
#define MERCS_RETURN_CONFIG 50
#define MERCS_RETURN_MAX_BYTES (24 + 8 * 40 + (14 + 25) * 8 + 50)

typedef struct {
    int32_t armor_q16, rear_q16, internal_q16;
    uint16_t slots[12], slot_count, flags;
} mercs_return_location;

typedef struct {
    int32_t weapon, group, location, destroyed;
    uint32_t item;
} mercs_return_mount;

typedef struct {
    uint16_t weapon;
    int16_t rounds, id;
} mercs_return_bin;

typedef struct {
    int32_t rounds;
    int bin_count, current_bin;
    int bin_indices[MERCS_RETURN_BINS];
} mercs_return_pool;

typedef struct {
    uint16_t flags;                  /* mask0x0002: eligible owned/deployed controller */
    int difficulty, player, alliance;
    int32_t tonnage, walk_speed_raw, jump_mp;
    uint8_t config[MERCS_RETURN_CONFIG]; /* includes original sink flag at +34 */
    mercs_return_location locations[8];
    mercs_return_mount mounts[MERCS_RETURN_MOUNTS];
    int weapon_count, bin_count, pool_count;
    mercs_return_bin bins[MERCS_RETURN_BINS];
    mercs_return_pool pools[MERCS_RETURN_POOLS];
} mercs_return_state;

enum {
    MERCS_RETURN_OK = 0,
    MERCS_RETURN_NOT_OWNED = 1,
    MERCS_RETURN_INVALID = -1,
    MERCS_RETURN_NO_SPACE = -2
};

/* Encode the original 50-byte-tail result format. On success, removal of
 * depleted ammo criticals is committed to state, matching the original side
 * effect. Slot counts are intentionally not reduced by that removal. Invalid
 * state or insufficient capacity leaves both state and output untouched.
 * written is zero on every non-success. Unsupported/unbounded original input
 * is rejected rather than emulating the original stack overrun behavior. */
int mercs_return_encode(mercs_return_state *state, uint8_t *out, size_t capacity,
                        size_t *written);

/* Capture current native combat plus retained loadout metadata. Independent
 * ammunition pools survive missing/destroyed mounts. Every bin's type, pool,
 * complete order mapping and every mounted cache must remain consistent.
 * The supplied
 * context is the original caller's ownership/difficulty/alliance context.
 * This conversion does not turn native float combat into fixed-point combat;
 * it quantizes the current values at the serializer boundary. */
int mercs_return_capture(const combat_unit *unit, int owned, int difficulty,
                         int player, int alliance, mercs_return_state *out);

/* Native capture + encode, committing depleted-slot removal to the live unit
 * only after successful encoding. This is the verified producer boundary;
 * calling it does not enable the unfinished shell/debrief/save transaction. */
int mercs_return_unit_bytes(combat_unit *unit, int owned, int difficulty,
                            int player, int alliance, uint8_t *out,
                            size_t capacity, size_t *written);

#endif
