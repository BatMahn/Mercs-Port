#ifndef MERCS_REPAIR_H
#define MERCS_REPAIR_H

#include <stddef.h>
#include <stdint.h>
#include "mercscareer.h"

#define MERCS_REPAIR_ITEM_BYTES 26
#define MERCS_REPAIR_MAX_ITEMS 4096

enum mercs_repair_status {
    MERCS_REPAIR_OK = 0,
    MERCS_REPAIR_NO_REPLACEMENT = 1,
    MERCS_REPAIR_NOT_DAMAGED = 2,
    MERCS_REPAIR_INTERNALS_FIRST = 3,
    MERCS_REPAIR_INSUFFICIENT_FUNDS = 4,
    MERCS_REPAIR_INVALID = -1,
    MERCS_REPAIR_NOMEM = -2
};
enum mercs_repair_part {
    MERCS_REPAIR_ARMOR = 0,
    MERCS_REPAIR_INTERNAL = 1,
    MERCS_REPAIR_CRITICAL = 2
};
enum mercs_repair_list {
    MERCS_REPAIR_OWNED_WEAPONS = 0,
    MERCS_REPAIR_OWNED_AMMO = 1,
    MERCS_REPAIR_RESERVED_WEAPONS = 2,
    MERCS_REPAIR_RESERVED_AMMO = 3
};

typedef struct mercs_repair_session mercs_repair_session;

/* All state is privately staged. Input item rows are the literal original
 * 26-byte owned records, including original numeric type bytes. No stock item
 * is generated and no original caller record is changed by this module.
 * Caller commits the final current/inventory/funds together after leave==1. */
int mercs_repair_begin(const mercs_career_mech_pair *pair,
                       const uint8_t *owned_weapons, size_t weapon_count,
                       const uint8_t *owned_ammo, size_t ammo_count,
                       int32_t funds, uint8_t career_dirty,
                       mercs_repair_session **out);
void mercs_repair_free(mercs_repair_session *session);
const mercs_career_mech *mercs_repair_current(const mercs_repair_session *session);
const mercs_career_mech *mercs_repair_reference(const mercs_repair_session *session);
const mercs_career_mech *mercs_repair_backup(const mercs_repair_session *session);
const uint8_t *mercs_repair_items(const mercs_repair_session *session,
                                 enum mercs_repair_list list, size_t *count);
int32_t mercs_repair_funds(const mercs_repair_session *session);
uint8_t mercs_repair_dirty(const mercs_repair_session *session);
/* UI-only selection field+364 changes no dirty marker or funds. */
int mercs_repair_select_location(mercs_repair_session *session, int location);
/* Original replacement lookup with reservation disabled; returns1 if an
 * actual owned replacement or original built-in repair is available,0 if
 * unavailable,-1 for invalid input. This query has no record/list writes. */
int mercs_repair_has_replacement(mercs_repair_session *session,
                                 int32_t critical_id, int32_t *cost);

/* Original bounded component helpers. quote_only==1 follows their quote
 * branch; 0 applies. Critical checks restore whole matching component groups,
 * reserve an actual owned replacement, and retain partial same-type ammo.
 * Full quote retains the original +84d and heat-sink group side effects.
 * Amount is the signed original 32-bit result, including negative armor or
 * internal differences. A missing critical replacement returns status1 and
 * amount=-1; full repair skips that critical and continues available work. */
int mercs_repair_component(mercs_repair_session *session, int location,
                           enum mercs_repair_part part, int slot,
                           int quote_only, int32_t *amount);
int mercs_repair_all(mercs_repair_session *session, int quote_only, int32_t *amount);

/* Actual selected-repair UI action: internals first for armor/criticals;
 * armor/internal affordability is checked against current funds before work.
 * Critical selection follows its distinct original path (including full
 * selected ammo-bin replacement), with its original diagnostic status. */
int mercs_repair_selected(mercs_repair_session *session, int location,
                          enum mercs_repair_part part, int slot, int32_t *amount);

/* DOS0x63b70: quote(backup,reference)-quote(current,reference), wrapping32.
 * This can update the original quote metadata/group fields in both records. */
int mercs_repair_owed(mercs_repair_session *session, int32_t *amount);

/* DOS0x66af4: return reservations at the end of their original owned list,
 * clear reservations, restore every0x850 backup byte. Career dirty remains
 * as in the original. The session stays open (Clear All / Undo). */
int mercs_repair_rollback(mercs_repair_session *session);

/* DOS0x66bb4: if current+84c is already zero, leave immediately regardless
 * of accept. Otherwise accept==0 restores and leaves. Clamp owed below zero
 * to zero, debit an affordable positive amount, and leave. Insufficient funds
 * either keeps the staged session open (undo_if_insufficient==0, leave0) or
 * restores and leaves (==1). The same completed session cannot debit twice.
 * A port shortcut that guarantees cancellation calls rollback first, since
 * the original clean-byte branch also covers some zero-cost replacements. */
int mercs_repair_finish(mercs_repair_session *session, int accept,
                        int undo_if_insufficient, int *leave);

#endif
