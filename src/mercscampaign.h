#ifndef MERCS_CAMPAIGN_H
#define MERCS_CAMPAIGN_H

#include "mercssave.h"
#include "mercsshell_data.h"
#include "mercsdebrief.h"

enum { MERCS_CAMPAIGN_READY, MERCS_CAMPAIGN_DEPLOYED,
       MERCS_CAMPAIGN_RETURN, MERCS_CAMPAIGN_DEBRIEF, MERCS_CAMPAIGN_ENDED };

/* Native continuation context is kept separate from the exact DOS save.
 * In particular, launch tokens and the CAR tail are not invented SHELLMER fields. */
typedef struct {
    mercs_save_state *document;
    int phase, active, accepted, selected, resume_page;
    uint32_t rng_state;
    int procedural_year;            /* year used by the accepted RMG preview */
    uint32_t procedural_seed;       /* standalone original restore history seed */
    int procedural_seed_override;  /* preserve header162 when history differs */
    char campaign[16];
    char launch_token[MERCS_EXCHANGE_TOKEN], launch_edition[12];
    mercs_debrief_entry entry;
    mercs_debrief_terms terms;
    mercs_debrief_report report;
} mercs_campaign;

void mercs_campaign_free(mercs_campaign *campaign);
int mercs_campaign_clone(const mercs_campaign *source, mercs_campaign *out);
int mercs_campaign_new(prj_archive *archive, int mode, const char *pilot,
                        const char *unit, mercs_campaign *out,
                        mercs_shell_state *view, char *error, size_t error_size);
int mercs_campaign_hydrate(prj_archive *archive, mercs_campaign *campaign,
                            mercs_shell_state *view, char *error, size_t error_size);
int mercs_campaign_sync(mercs_campaign *campaign, const mercs_shell_state *view,
                         char *error, size_t error_size);
int mercs_campaign_accept(prj_archive *archive, mercs_campaign *campaign,
                           mercs_shell_state *view, int mission_index,
                           char *error, size_t error_size);
/* Restore the accepted seed's graph in a newly opened/changed archive before
 * mission launch. Authored contracts clear old generated resources. */
int mercs_campaign_prepare_resources(prj_archive *archive,const mercs_campaign *campaign,
                                     char *error,size_t error_size);
/* Called after the already-validated damaged-MEK/result import. The entry
 * action is separated so the original movie can finish before its mutations. */
int mercs_campaign_return(mercs_campaign *campaign, mercs_shell_state *view,
                           char *error, size_t error_size);
int mercs_campaign_enter_debrief(prj_archive *archive, mercs_campaign *campaign,
                                  mercs_shell_state *view, char *error, size_t error_size);
int mercs_campaign_continue(prj_archive *archive, mercs_campaign *campaign,
                              mercs_shell_state *view, char *error, size_t error_size);
int mercs_campaign_report(mercs_campaign *campaign,const mercs_shell_state *view,
                           char *error,size_t error_size);
/* Original bankruptcy/ending/no-mech gates precede all settlement. This
 * read-only query returns the original movie names for the frontend. Continue
 * enforces the same gate again and applies its state changes atomically. */
int mercs_campaign_preflight(const mercs_campaign *campaign,
                              const mercs_shell_state *view,
                              mercs_debrief_gate *gate,
                              char *error,size_t error_size);

/* Native resume companion, bound to one complete original SHELLMER payload.
 * Decoder leaves out/view unchanged on failure. Resources are rehydrated from
 * the selected graphics archive; owned condition comes from the saved bytes.
 * PRV1 at offset532 retains all cached RAN seeds and generation years. Older
 * zero-extension companions remain readable; an unstored, uncached old offer
 * must be generated once and is retained by the next save. */
#define MERCS_CAMPAIGN_RESUME_BYTES 1024
int mercs_campaign_encode_resume(const mercs_campaign *campaign,
                                  const mercs_shell_state *view,
                                  const uint8_t *original, size_t original_size,
                                  uint8_t out[MERCS_CAMPAIGN_RESUME_BYTES]);
int mercs_campaign_decode_resume(prj_archive *archive,
                                  const uint8_t *original, size_t original_size,
                                  const uint8_t *resume, size_t resume_size,
                                  mercs_campaign *out, mercs_shell_state *view,
                                  char *error, size_t error_size);
uint32_t mercs_campaign_crc(const uint8_t *bytes,size_t length);

#endif
