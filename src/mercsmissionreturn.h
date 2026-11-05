#ifndef MERCS_MISSION_RETURN_H
#define MERCS_MISSION_RETURN_H

#include <stddef.h>
#include <stdint.h>
#include "msim.h"
#include "mercsreturn.h"
#include "mercsresult.h"

/* Portable bound for the explicit mapping, not a claim about original unit
 * capacity. DOS 0x63c80 independently considers only original indices 0..99. */
#define MERCS_MISSION_RETURN_MAX_UNITS 256
#define MERCS_MISSION_RETURN_NAME 25
#define MERCS_MISSION_RETURN_PATH 34
#define MERCS_MISSION_RETURN_SALVAGE_LIMIT 100

typedef struct {
    int actor_index;                  /* -1 = native player; otherwise exact msim actor index */
    uint16_t controller_flags;        /* explicit original +0x442; mask2, never inferred from alliance */
    char controller_name[MERCS_MISSION_RETURN_NAME]; /* original +0x470, including any extension */
    int alliance;                     /* explicit serializer alliance context */
    uint8_t controller_state;         /* original +0x3d4, for salvage diagnostics only */
    int controller_state_known;       /* 0 makes relevant salvage diagnostics incomplete */
} mercs_mission_return_binding;

/* Called only for a mask2 unit while owned returns are enabled. Path spelling
 * follows the original logical DOS path: mek\\name, adding .mek only when
 * no dot occurs. Resolve only within the explicit launch overlay; do not
 * search the archive or substitute a stock file. 1 = the original rb guard
 * succeeds, 0 = absent/unreadable, negative = callback/context failure. */
typedef int (*mercs_mission_return_existing)(void *user, const char *logical_path);

typedef struct {
    int owned_returns_enabled;        /* original DOS global0xbcbe8, retained from launch/loading */
    const mercs_mission_return_binding *bindings;
    size_t binding_count;             /* explicit known deployment subset, or complete original unit order */
    int complete_unit_order;          /* 1 requires every actor/player exactly once; 0 leaves salvage unknown */
    mercs_mission_return_existing existing_file;
    void *existing_user;
    const mercs_car_record *career;   /* all214 bytes explicitly supplied; no80-byte padding default */
    const mercs_msn_star *stars;
    size_t star_count, selected_star; /* explicit selected-star producer inputs, not exit-time guesses */
} mercs_mission_return_context;

enum mercs_mission_return_decision {
    MERCS_MISSION_UNIT_DISABLED = 0,
    MERCS_MISSION_UNIT_NOT_OWNED = 1,
    MERCS_MISSION_UNIT_FILE_MISSING = 2,
    MERCS_MISSION_UNIT_STAGED = 3
};

typedef struct {
    size_t original_unit_index;       /* SIZE_MAX when only a deployment subset was supplied */
    int actor_index;
    char path[MERCS_MISSION_RETURN_PATH];
    uint8_t bytes[MERCS_RETURN_MAX_BYTES];
    size_t size;
    uint32_t slots_before[8][12], slots_after[8][12];
} mercs_mission_return_mek;

typedef struct {
    uint8_t career[MERCS_CAR_BYTES], mission[MERCS_MSN_BYTES];
    size_t career_size, mission_size;
    mercs_mission_return_mek *mechs;
    size_t mech_count, binding_count;
    uint8_t decisions[MERCS_MISSION_RETURN_MAX_UNITS];
    size_t salvage_indices[MERCS_MISSION_RETURN_SALVAGE_LIMIT];
    size_t salvage_count;
    int salvage_complete;
    /* Diagnostics only: no salvage.mw2 bytes are manufactured. The original
     * collector excludes mask2, but its serializer requires mask2; its128-byte
     * header also contains an unspecified stack tail. */
} mercs_mission_return_bundle;

enum {
    MERCS_MISSION_RETURN_OK = 0,
    MERCS_MISSION_RETURN_INVALID = -1,
    MERCS_MISSION_RETURN_NO_MEMORY = -2,
    MERCS_MISSION_RETURN_EXISTING_ERROR = -3,
    MERCS_MISSION_RETURN_CAPTURE_ERROR = -4,
    MERCS_MISSION_RETURN_STALE = -5
};

/* Initialize out to all zeroes before its first build, or free it before
 * reuse. On failure out and every live combat unit remain unchanged. On
 * success bytes and depleted-slot side effects are staged in memory only.
 * A subset maps only known deployment identities, without classifying omitted
 * actors as unowned. A complete mapping must include every actor/player once;
 * original order is supplied rather than inferred. Normal outcome, abort, ejection
 * and death do not add filters to the original owned-unit eligibility loop. */
int mercs_mission_return_build(const msim *sim,
                               const mercs_mission_return_context *context,
                               mercs_mission_return_bundle *out);

/* Optional original serializer side-effect commit after the outer transaction
 * succeeds. Caller must keep simulation frozen between build and commit.
 * Check every recorded critical-slot array first; a conflict commits none. */
int mercs_mission_return_commit_slots(msim *sim,
                                      const mercs_mission_return_bundle *bundle);
void mercs_mission_return_free(mercs_mission_return_bundle *bundle);

#endif
