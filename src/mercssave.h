#ifndef MERCS_SAVE_H
#define MERCS_SAVE_H

#include <stddef.h>
#include <stdint.h>
#include "prj.h"
#include "mercscareer.h"

/* Original SHELLMER format, not a C-structure dump or native receipt.
 * DOS mech rows have0x850 bytes; Windows rows add three opaque bytes. The
 * document retains its decoded wire layout and every extension byte. Runtime
 * accessors expose the independently verified common0x850-byte mech prefix. */
#define MERCS_SAVE_HEADER_BYTES 0x2dfb
#define MERCS_SAVE_PILOT_BYTES 0x2066
#define MERCS_SAVE_TECH_BYTES 0x205e
#define MERCS_SAVE_ITEM_BYTES 0x1a
#define MERCS_SAVE_MAX_BYTES (32u * 1024u * 1024u)
#define MERCS_SAVE_MAX_LIST_RECORDS 4096u
#define MERCS_SAVE_WINDOWS_EXTENSION_BYTES 3u

enum mercs_save_list {
    MERCS_SAVE_SALVAGE_MECHS,
    MERCS_SAVE_SALVAGE_ITEMS,
    MERCS_SAVE_SHOP_MECHS,
    MERCS_SAVE_SHOP_REFERENCES,
    MERCS_SAVE_SHOP_PILOTS,
    MERCS_SAVE_SHOP_TECHS,
    MERCS_SAVE_SHOP_WEAPONS,
    MERCS_SAVE_SHOP_AMMO,
    MERCS_SAVE_OWNED_MECHS,
    MERCS_SAVE_OWNED_REFERENCES,
    MERCS_SAVE_OWNED_PILOTS,
    MERCS_SAVE_OWNED_TECHS,
    MERCS_SAVE_OWNED_WEAPONS,
    MERCS_SAVE_OWNED_AMMO,
    MERCS_SAVE_LIST_COUNT
};

enum mercs_save_status {
    MERCS_SAVE_OK = 0,
    MERCS_SAVE_INVALID = -1,
    MERCS_SAVE_NOMEM = -2,
    MERCS_SAVE_UNSUPPORTED = -3
};

typedef struct mercs_save_state mercs_save_state;

mercs_save_state *mercs_save_create(void);
mercs_save_state *mercs_save_clone(const mercs_save_state *source);
void mercs_save_free(mercs_save_state *state);
/* Exchange ownership of two documents; neither document may be NULL. */
void mercs_save_swap(mercs_save_state *left, mercs_save_state *right);
const uint8_t *mercs_save_header(const mercs_save_state *state);
uint8_t *mercs_save_header_edit(mercs_save_state *state);
size_t mercs_save_list_stride(enum mercs_save_list list);
size_t mercs_save_list_count(const mercs_save_state *state, enum mercs_save_list list);
int mercs_save_windows(const mercs_save_state *state);
/* NULL for DOS/non-mech/invalid rows. Explicit record moves and whole-record
 * reference replacement copy this opaque tail along with the common prefix.
 * Ordinary edits and set_list/set_owned preserve tails by original identity;
 * newly allocated Windows records begin with zero extension bytes. */
const uint8_t *mercs_save_mech_extension(const mercs_save_state *state,
                                         enum mercs_save_list list,size_t index);
int mercs_save_set_mech_extension(mercs_save_state *state,enum mercs_save_list list,
                                  size_t index,const uint8_t extension[3]);
const uint8_t *mercs_save_list_record(const mercs_save_state *state,
                                     enum mercs_save_list list, size_t index);
uint8_t *mercs_save_list_record_edit(mercs_save_state *state,
                                    enum mercs_save_list list, size_t index);
/* Copies caller bytes. Failure leaves the list unchanged. Paired mech lists
 * must have equal counts before encoding; set_owned updates both atomically. */
int mercs_save_set_list(mercs_save_state *state, enum mercs_save_list list,
                       const uint8_t *records, size_t count);
int mercs_save_set_owned(mercs_save_state *state,
                        const mercs_career_mech_pair *pairs, size_t count);

/* Successful decode allocates a new owned document in *out. It never replaces
 * or frees the caller's old pointer. Unknown header/list bytes are preserved.
 * Exact length and nonnegative bounded counts are required. DOS and retail
 * Windows layouts are identified by the exact encoded array lengths. */
int mercs_save_decode(const uint8_t *bytes, size_t length, mercs_save_state **out);
/* Allocates *bytes; free it with free(). In original_writer mode, applies
 * DOS0x694b4's header output edits to a private copy: dirty=0, previous screen
 * =current screen, counts=current lists. Otherwise requires consistent header
 * counts and encodes losslessly. No mode changes the input state. */
int mercs_save_encode(const mercs_save_state *state, int original_writer,
                     uint8_t **bytes, size_t *length);
/* The original restore loader clears dirty and every current-mech deployed
 * marker. This bounded helper does not regenerate procedural contracts, write
 * result files, import transient MEKs, or run debrief/settlement. */
void mercs_save_restore_markers(mercs_save_state *state);

/* Original authored new-career initialization and registration, backed by
 * CPGN/PILT resources. Returns a complete supported native initial document,
 * including the original shop lists. Newly allocated undefined padding is
 * zeroed; imported saves retain it. Only regular/training modes are supported. */
int mercs_save_initialize(prj_archive *archive, int mode,
                          const char *pilot, const char *unit,
                          mercs_save_state **out, char *error, size_t error_size);

/* Apply the original CPGN fields and authored shop/candidate lists to a clone.
 * Owned lists persist. Native caller owns campaign eligibility/advancement.
 * This is useful when accepting a verified authored contract; it does not
 * choose that contract, transfer salvage or charge funds. */
int mercs_save_load_campaign(prj_archive *archive, mercs_save_state *state,
                             const char *campaign, char *error, size_t error_size);

/* DOS0x5a938/0x5a8c4/0x5aa98: retain owned current/reference pairs whose
 * +0x814 is positive, replace the remaining training roster with shop MHLT,
 * hire PTLT using the shared ID counter, and transfer shop weapons/ammo.
 * Transactional; does not change the player's assigned-mech field. */
int mercs_save_transfer_training(mercs_save_state *state);

/* Successful terminal training-contract cleanup, DOS0x3996c/0x39a70:
 * retain only owned mechs with signed+0x814>0 and their identity-matched
 * references; keep only the player pilot and clear player+0x20. Shop/item
 * lists remain unchanged. This is NOT the type1 chained-mission advance. */
int mercs_save_training_cleanup(mercs_save_state *state);

/* A player is the unique owned-pilot row with byte+0x2064 == 1. Returns an
 * error for missing/duplicate players or an unterminated callsign. Name may
 * be NULL. Assignment updates ONLY player+0x20; caller synchronizes mechs. */
int mercs_save_player(const mercs_save_state *state, size_t *index,
                      uint32_t *identity, char *name, size_t name_size);
int mercs_save_player_assignment(mercs_save_state *state, int32_t mech_identity);

#endif
