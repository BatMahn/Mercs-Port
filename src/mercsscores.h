#ifndef MERCS_SCORES_H
#define MERCS_SCORES_H
#include "mercspanels.h"
#include "mercssave.h"

/* DOS0x46f58/0x5bf6c: current owned asset value plus current funds, before
 * final settlement. Uses the unique saved player and original unit name. */
int mercs_scores_earned(const mercs_save_state *career,mercs_lords_row *out);
/* An ending caused by a sale has no new mission token. Derive a stable native
 * receipt key from that complete staged original-format document so retrying
 * the same sale/end commit remains idempotent. This is not authentication. */
int mercs_scores_document_token(const mercs_save_state *career,char token[33]);
/* Shared retail overlay for all three graphics choices. A supplied fallback
 * is read only when no native mw2high.cfg exists; NULL uses original defaults.
 * No writes occur while merely opening the Merc Lords screen. */
int mercs_scores_load(const char *retail_overlay,const char *fallback,
                       const mercs_lords_table *defaults,mercs_lords_table *out,
                       char *error,size_t error_size);
/* A career-ending return token is recorded at most once, including interrupted
 * writes. The external score file retains the original694-byte format. A
 * separate native pending record/receipt makes its commit recoverable before
 * the caller commits the career's ended generation. */
int mercs_scores_commit(const char *retail_overlay,const char *fallback,
                         const mercs_lords_table *defaults,const char *ending_token,
                         const mercs_lords_row *earned,char *error,size_t error_size);
#endif
