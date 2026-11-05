#ifndef MERCS_DEBRIEF_H
#define MERCS_DEBRIEF_H

#include <stddef.h>
#include <stdint.h>
#include "mercscareer.h"
#include "mercsresult.h"

/* Pure reconstructed DOS shell transitions. The frontend owns movie playback,
 * original room art, file commitment and exactly-once state application. */
enum { MERCS_DEBRIEF_OK=0, MERCS_DEBRIEF_SKIPPED=1, MERCS_DEBRIEF_INVALID=-1,
       MERCS_DEBRIEF_UNSUPPORTED=-2, MERCS_DEBRIEF_NO_SPACE=-3 };
enum { MERCS_DEBRIEF_MOVIE_NONE=0, MERCS_DEBRIEF_MOVIE_EJECT=1,
       MERCS_DEBRIEF_MOVIE_DEATH=2 };
typedef struct {
    int movie;
    int remove_player_mech; /* after MEJECT, regular only; paired reference too */
    int unassign_player;
    int career_end;         /* after MDEATH, regular only; no report follows */
    int show_report;
    int restore_training_mechs; /* fresh novice report resets current from ref */
} mercs_debrief_entry;

int mercs_debrief_enter(int mode, unsigned restore_flag,
                        const mercs_car_record *car,
                        mercs_debrief_entry *out);

/* Authored career fields, supplied explicitly rather than guessed from mission
 * names. Expenses are quoted after the entry transition removes an ejected
 * player's mech. All arithmetic follows the original 32-bit wrapping domain. */
typedef struct {
    int mode;                       /* 0 novice, 1 regular */
    unsigned restore_flag;          /* original C+0x23b7 byte */
    int32_t campaign_type;          /* C+0x1fe */
    int32_t funds;                  /* C+0x12a */
    int32_t success_pay, failure_pay; /* C+0x1e6, +0x1ea */
    int32_t target_pay, bonus_pay, bounty_pay; /* C+0x1ee, +0x1f2, +0x1f6 */
    int32_t expense_factor;         /* C+0x1fa */
    int32_t per_mech_a, per_mech_b;  /* C+0x23a3, +0x23a7 */
    int32_t fixed_a, fixed_b, fixed_c; /* C+0x23ab, +0x23af, +0x23b3 */
    int32_t pilot_pay, aero_pay;     /* exact eligible list sums */
    uint32_t owned_mech_count;
    char success_next[160];         /* C+0x202 */
    char failure_next[160];         /* C+0x2a2 */
} mercs_debrief_terms;

typedef struct {
    int success;
    uint32_t outcome, row_count, confirmed_kills;
    int32_t row_reward[MERCS_MSN_ROWS];
    uint32_t target_count, bonus_count, bounty_count;
    int32_t objective_income, kill_income, contract_income;
    int32_t income, expense, balance;
    int select_salvage; /* regular success and restore_flag != 1 */
} mercs_debrief_report;

int32_t mercs_debrief_expense(const mercs_debrief_terms *terms);
int mercs_debrief_build(const mercs_debrief_terms *terms,
                        const mercs_car_record *car,
                        const mercs_msn_record *msn,
                        mercs_debrief_report *out);

enum {
    MERCS_DEBRIEF_NEXT_STAY=0,
    MERCS_DEBRIEF_NEXT_CAMPAIGN=1,
    MERCS_DEBRIEF_NEXT_CONTRACT_END=2,
    MERCS_DEBRIEF_NEXT_CAREER_END=3,
    MERCS_DEBRIEF_NEXT_TRAINING_CLEANUP=4
};
typedef struct {
    int next;
    char campaign[160];
    int commit_funds, cleanup_lost, transfer_salvage;
    int32_t funds_after;
} mercs_debrief_commit;

/* Original Continue branches. Entry's regular-death branch must already have
 * ended the career and must not call this. This function does not mutate state
 * or imply that a requested successor resource has loaded successfully. */
int mercs_debrief_continue(const mercs_debrief_terms *terms,
                           const mercs_debrief_report *report,
                           mercs_debrief_commit *out);

enum { MERCS_DEBRIEF_GATE_NONE=0, MERCS_DEBRIEF_GATE_BANKRUPT=1,
       MERCS_DEBRIEF_GATE_ENDING=2, MERCS_DEBRIEF_GATE_NO_MECHS=3 };
typedef struct {
    int block_settlement, career_end, reason;
    unsigned movie_count;
    char movies[2][16];
    char campaign[16]; /* no-mech fallback; set success=0, fresh=1, active=0 */
} mercs_debrief_gate;

/* Original leave-handler gates run BEFORE Continue/funds/loss/salvage commit.
 * asset_value is the original0x5bf6c total, not a guessed market value; it is
 * needed only for regular type4 ending. no_mech_campaign is C+0x1d6 and must
 * be a terminated16-byte resource field. Empty fallback fails safely only if
 * that branch is reached. Movies are original resource identifiers. */
int mercs_debrief_preflight(const mercs_debrief_terms *terms,
                            const mercs_debrief_report *report,
                            const mercs_car_record *car, int32_t asset_value,
                            const char no_mech_campaign[16],
                            mercs_debrief_gate *out);

/* Ending asset value (DOS0x5bf6c): wrap-sum one result per owned current mech
 * plus the stored value+4 of every owned weapon/equipment and ammo record.
 * The mech helper includes original date adjustment, remaining armor/internal,
 * equipment criticals and the original weapon/ammo duplicate scan. */
int mercs_debrief_mech_value(const mercs_career_mech *mech, int32_t current_year,
                             int32_t *out);
int32_t mercs_debrief_item_value(const uint8_t item[26]);
int32_t mercs_debrief_age_value(int32_t current_year, int32_t acquired_year,
                               int32_t base_value);

/* Original fresh novice debrief replacement: reference's association is set
 * to current's association, then the entire updated reference becomes current.
 * Both original identities must match. No deployment/file I/O is performed. */
int mercs_debrief_restore_training_pair(mercs_career_mech_pair *pair);

/* DOS Watcom rand(): original0x7d0f0, unsigned state at0xc40fc. The caller owns
 * initialization and the saved RNG state; the module never seeds from time. */
uint32_t mercs_debrief_rand(uint32_t *state);
typedef struct { int32_t threshold; uint8_t selected; } mercs_salvage_choice;
int mercs_debrief_roll_salvage(unsigned restore_flag,
                              mercs_salvage_choice *items, size_t item_count,
                              mercs_salvage_choice *mechs, size_t mech_count,
                              uint32_t *rng_state);

/* Original salvage ownership operation for one already-selected mech. The
 * caller supplies the reference loaded from chassis00STD and a unique name
 * allocated against the existing owned list. Returned current condition is
 * retained; only identity/name and eight original location IDs are adjusted.
 * The caller must stage list capacity, candidate removal and persistence. */
int mercs_debrief_salvage_pair(const mercs_career_mech *candidate,
                               const mercs_career_mech *stock_reference,
                               const char *unique_owned_filename,
                               mercs_career_mech_pair *out);

enum { MERCS_DEBRIEF_INVENTORY_WEAPON=0, MERCS_DEBRIEF_INVENTORY_AMMO=1 };
/* DOS selected SWNL record normalization: exact26 bytes. Numeric type0 and2
 * enter the weapon/equipment list; type1 enters ammo. Unselected/unknown types
 * return SKIPPED without touching output. Table-unsafe IDs fail atomically. */
int mercs_debrief_salvage_item(const uint8_t candidate[26],
                               uint8_t owned[26], int *destination);

/* Match the original signed time comparator including its both-negative
 * asymmetry. Use order_rows, not a host libc qsort, for original tie order. */
int mercs_debrief_compare_time(int32_t a, int32_t b);

/* DOS0x7b45a specialized to the original four-byte pointer array and comparator
 * 0x38800. Output indexes are independent of host pointer width/libc. Original
 * non-progressing partitions (possible with >=16 negative-time rows) return
 * UNSUPPORTED atomically instead of repeating forever or exhausting its stack. */
int mercs_debrief_order_rows(const mercs_msn_record *msn,
                              uint32_t order[MERCS_MSN_ROWS]);

#endif
