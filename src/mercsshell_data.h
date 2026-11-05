#ifndef MERCS_SHELL_DATA_H
#define MERCS_SHELL_DATA_H

#include <stddef.h>
#include <stdint.h>
#include "prj.h"
#include "mercsscene.h"
#include "mercscareer.h"
#include "mercsexchange.h"
#include "mercssave.h"

/* Authored contract lists have ten16-byte entries in the retail career
 * block. The full save/campaign layer owns settlement and durable state;
 * these views also support standalone resource/deployment inspection. */
#define MERCS_SHELL_MAX_MISSIONS 10
#define MERCS_SHELL_TEXT 8192
#define MERCS_SHELL_TRAINING_MECHS 3
/* Original owned lists are dynamically sized. Use the codec's explicit
 * corruption/resource bound, not the former arbitrary 32-mech fleet cap.
 * Shell states are heap objects; do not allocate this structure on a stack. */
#define MERCS_SHELL_MAX_OWNED ((int)MERCS_SAVE_MAX_LIST_RECORDS)

typedef struct {
    char stock[9];                  /* authored MHLT MEK resource */
    char loadout[9];                /* original owned filename, no .MEK */
    char skeleton[9];
    char name[40];
} mercs_shell_mech;

typedef struct {
    char campaign[16];
    char scene[16];
    char title[160];
    char briefing_smk[16];
    char faction[16];
    char contract[MERCS_SHELL_TEXT];
    char briefing[MERCS_SHELL_TEXT];
    char contract_raw[MERCS_SHELL_TEXT]; /* original alignment/tab/page controls */
    char briefing_raw[MERCS_SHELL_TEXT];
    int campaign_type;
    int year, month;
    int launchable;
    uint32_t procedural_seed;
    int procedural_year;
    char unavailable[192];
} mercs_shell_mission;


/* Original procedural preview is reproducible from its saved seed. This
 * stages generated resources for validation and restores the current graph. */
int mercs_shell_mission_load_seeded(prj_archive *archive,const char *campaign,
                                   const char *pilot,int year,uint32_t seed,
                                   mercs_shell_mission *out,char *error,size_t error_size);

typedef struct {
    int registered;
    int mode;                       /* 0 novice; 1 regular */
    char pilot[22];
    char unit[22];
    char initial_campaign[16];
    int year, month;
    int32_t funds;
    char starter_stock[9];          /* authored MEK resource */
    char starter_loadout[9];        /* original owned-mech filename, no .MEK */
    char starter_skeleton[9];
    char starter_name[40];
    mercs_shell_mech training_mechs[MERCS_SHELL_TRAINING_MECHS];
    int training_mech_count;
    int assigned_index;            /* Training: -1 until an explicit Assign action */
    int owned_count, owned_index;
    uint32_t pilot_instance_id;
    mercs_career_mech_pair owned[MERCS_SHELL_MAX_OWNED];
    /* Complete original simulation result payloads, retained independently
     * from outcome rewards, salvage selection and campaign progression. */
    int last_return_valid, last_return_mission;
    char last_return_token[MERCS_EXCHANGE_TOKEN];
    mercs_car_record last_car;
    mercs_msn_record last_msn;
    char news[MERCS_SHELL_TEXT];
    char news_raw[MERCS_SHELL_TEXT];
    int mission_count;
    mercs_shell_mission missions[MERCS_SHELL_MAX_MISSIONS];
} mercs_shell_state;

/* Read the original initial campaign and its contract list. No files are
 * written. The caller retains ownership of the open archive and state. */
int mercs_shell_register(prj_archive *a, mercs_shell_state *state,
                         int mode, const char *pilot, const char *unit,
                         char *error, size_t error_size);

/* Assign one of the selected training contract's already owned MHLT mechs. This
 * performs no purchase, hire, or implicit default selection. The original
 * regular registration's automatic assignment remains unchanged. */
int mercs_shell_assign(prj_archive *a, mercs_shell_state *state,
                       int mission_index, int choice,
                       char *error, size_t error_size);
/* Original roster PREV/NEXT also includes the unassigned sentinel. */
void mercs_shell_unassign(mercs_shell_state *state, int mission_index);

/* Materialize the selected mission's verified launch files in an existing,
 * caller-approved edition overlay. The frontend serializes this operation and
 * the child's use of that overlay with its edition launch lock. This is a
 * launch adapter, not a saved campaign/result writer. A failure explains the
 * unsupported resource/path. */
int mercs_shell_prepare_launch(prj_archive *a, mercs_shell_state *state,
                               int mission_index, const char *overlay,
                               char *error, size_t error_size);

/* Explicit original Duty Roster selection, player first followed by at most
 * two hired MechWarriors in roster order. Eight saved signed16 attributes
 * come from original pilot+0x28 (including voice identity at attribute7).
 * This adapter validates every assignment and serializes every current MEK
 * before writing. It commits deployment flags/manifest only after all files
 * are complete. The caller holds the edition lock through child completion. */
typedef struct mercs_shell_deploy_pilot {
    int owned_index;
    uint32_t pilot_instance_id;
    char name[22];
    int16_t attributes[8];
} mercs_shell_deploy_pilot;
int mercs_shell_prepare_lance(prj_archive *archive,mercs_shell_state *state,
                              int mission_index,const char *overlay,
                              const mercs_shell_deploy_pilot *pilots,size_t pilot_count,
                              mercs_exchange_launch *manifest,char *error,size_t error_size);
typedef struct mercs_shell_deploy_aircraft {
    char stock[9],name[22];
    uint32_t temporary_instance_id;
    int16_t attributes[8];
} mercs_shell_deploy_aircraft;
/* Optional hired AeroTech follows the owned mechs in USERSTAR. It is a
 * temporary support aircraft and is not added to the owned-return manifest.
 * The original DOS caller loses this fourth slot; the native adapter retains
 * it while preserving original ARO converter and eight-word pilot fields. */
int mercs_shell_prepare_roster(prj_archive *archive,mercs_shell_state *state,
                               int mission_index,const char *overlay,
                               const mercs_shell_deploy_pilot *pilots,size_t pilot_count,
                               const mercs_shell_deploy_aircraft *aircraft,
                               mercs_exchange_launch *manifest,char *error,size_t error_size);

/* One complete, matched mission return. Stages the original selective MEK
 * import and commits current/reference/result fields only after validation.
 * The caller handles the returned delete request and marks the exchange
 * applied while still holding its launch lock. No money, repair, salvage
 * transfer, mech removal, or campaign advancement is inferred here. */
int mercs_shell_apply_return(mercs_shell_state *state, int mission_index,
                             const mercs_exchange_launch *launch,
                             const mercs_exchange_result *result,
                             mercs_career_import_result *imported,
                             char *error, size_t error_size);

/* Graphics-profile refresh may re-read authored menus, but must retain the
 * same current/reference owned mechs, player identity and last result. Both
 * states must already represent the same registration and available mechs. */
int mercs_shell_keep_owned_state(mercs_shell_state *refreshed,
                                  const mercs_shell_state *previous,
                                  char *error, size_t error_size);

/* Authored progression and restored-career views use these same archive
 * readers. A nonlaunchable scene retains its original text and reason. */
int mercs_shell_mission_load(prj_archive *archive, const char *campaign,
                              const char *pilot, mercs_shell_mission *out,
                              char *error, size_t error_size);
int mercs_shell_set_missions(prj_archive *archive, mercs_shell_state *state,
                              const char names[][16], size_t count,
                              char *error, size_t error_size);
/* NEWS is retained in the original career header when a later CPGN omits it.
 * Decode that saved text without silently substituting registration news. */
int mercs_shell_news_text(mercs_shell_state *state, const uint8_t *bytes,
                           size_t length, char *error, size_t error_size);
/* Original DOS chassis-prefix table supplies skeleton and display name. */
int mercs_shell_describe_owned(prj_archive *archive,
                                const mercs_career_mech_pair *pair,
                                mercs_shell_mech *out,
                                char *error, size_t error_size);
int mercs_shell_assign_owned(prj_archive *archive, mercs_shell_state *state,
                              int owned_index, char *error, size_t error_size);

#endif
