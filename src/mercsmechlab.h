#ifndef MERCS_MECHLAB_H
#define MERCS_MECHLAB_H

#include "mercssave.h"

/* Original DOS MW2SHELL Mech Lab (0x30990..0x36960). Each session owns a
 * private full career document. Successful edits stage original mech bytes
 * and actual owned inventory together; failed edits change neither. The
 * caller persists the document returned by finish as one career transaction.
 */
typedef struct mercs_mechlab_session mercs_mechlab_session;
enum mercs_mechlab_status {
    MERCS_MECHLAB_OK = 0,
    MERCS_MECHLAB_NO_SPACE = 1,
    MERCS_MECHLAB_NO_ITEM = 2,
    MERCS_MECHLAB_LOCATION = 3,
    MERCS_MECHLAB_FIXED = 4,
    MERCS_MECHLAB_DUPLICATE = 5,
    MERCS_MECHLAB_LIMIT = 6,
    MERCS_MECHLAB_HEATSINK_TYPE = 7,
    MERCS_MECHLAB_OVERWEIGHT = 8,
    MERCS_MECHLAB_FUNDS = 9,
    MERCS_MECHLAB_PLACEMENT = 10,
    MERCS_MECHLAB_OPEN_ENGINE = 11,
    MERCS_MECHLAB_OPEN_ARMOR = 12,
    MERCS_MECHLAB_INVALID = -1,
    MERCS_MECHLAB_NOMEM = -2
};
enum mercs_mechlab_control {
    MERCS_MECHLAB_ENGINE,
    MERCS_MECHLAB_XL,
    MERCS_MECHLAB_ARMOR_WEIGHT,
    MERCS_MECHLAB_FRONT_ARMOR,
    MERCS_MECHLAB_REAR_ARMOR
};
int mercs_mechlab_begin(const mercs_save_state *source, size_t mech_index,
                         mercs_mechlab_session **out);
/* Instant Action owns six independent designs, not career mechs. This
 * adapter creates an isolated mode2 design using the original MEK converter.
 * filename supplies the chassis prefix used by that original converter. */
int mercs_mechlab_begin_mek(const uint8_t *mek, size_t length,
                             const char *filename, mercs_mechlab_session **out);
void mercs_mechlab_free(mercs_mechlab_session *session);
const mercs_save_state *mercs_mechlab_document(const mercs_mechlab_session *session);
const mercs_career_mech *mercs_mechlab_current(const mercs_mechlab_session *session);
const mercs_career_mech *mercs_mechlab_backup(const mercs_mechlab_session *session);
size_t mercs_mechlab_index(const mercs_mechlab_session *session);
int mercs_mechlab_dirty(const mercs_mechlab_session *session);
int32_t mercs_mechlab_cost(const mercs_mechlab_session *session);
/* Returns the number of mandatory Endo/Ferro placements still outstanding.
 * component, when non-NULL, receives original critical id9000 or9500. */
int mercs_mechlab_pending(const mercs_mechlab_session *session, int *component);
int mercs_mechlab_select_location(mercs_mechlab_session *session, int location);
/* Original item type0=weapon,1=ammunition,2=equipment. Equipment0 and9
 * return OPEN_ENGINE/OPEN_ARMOR without changing the document. Other items
 * are drawn from actual owned stock, except the original instant-action
 * mode2 unlimited catalog. id7/8 begin the original manual critical placement.
 */
int mercs_mechlab_add(mercs_mechlab_session *session, unsigned type, int id);
int mercs_mechlab_remove(mercs_mechlab_session *session, int slot);
int mercs_mechlab_place(mercs_mechlab_session *session, int location);
/* direction is-1 or1; XL toggles once. Armor increments are original
 * one-point front/rear changes or50 hundredths-of-ton weight changes. */
int mercs_mechlab_adjust(mercs_mechlab_session *session,
                          enum mercs_mechlab_control control, int direction);
int mercs_mechlab_rename(mercs_mechlab_session *session, const char *name);
/* Reset to the entry document, preserving the selected location. */
int mercs_mechlab_rollback(mercs_mechlab_session *session);
/* accept0 returns an unchanged copy of the entry career. accept1 validates
 * all critical placements, weight and regular-mode funds, applies the
 * original critical/armor charge, replaces the identity-matched reference
 * and returns a new owned document. Failure leaves the session open. The
 * caller owns *out; the session can then be freed. No file I/O or UI occurs.
 */
int mercs_mechlab_finish(mercs_mechlab_session *session, int accept,
                         mercs_save_state **out);
int mercs_mechlab_finish_mek(mercs_mechlab_session *session, int accept,
                              uint8_t *out, size_t capacity, size_t *written);

/* Original weight recalculation and armor-condition baselines on one bare
 * career mech (used by the Titanium Salvage Yard). */
int mercs_mechlab_recalculate(uint8_t *mech);
void mercs_mechlab_armor_condition(uint8_t *mech);

#endif
