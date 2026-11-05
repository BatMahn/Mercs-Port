#ifndef MERCS_INTERCHANGE_H
#define MERCS_INTERCHANGE_H
#include "mercscampaign.h"

/* DOS shell event numbers from the actual0x55e88 dispatch table. These are
 * original serialized values, deliberately independent of frontend enums. */
enum mercs_original_screen {
    MERCS_ORIGINAL_DEBRIEF=0xc00a, MERCS_ORIGINAL_REGISTER=0xc013,
    MERCS_ORIGINAL_DROPSHIP=0xc016, MERCS_ORIGINAL_MECH_ROOM=0xc018,
    MERCS_ORIGINAL_OFFICE=0xc01a, MERCS_ORIGINAL_MECHBAY=0xc01b,
    MERCS_ORIGINAL_HALL=0xc01c, MERCS_ORIGINAL_FACTORY=0xc01d,
    MERCS_ORIGINAL_TRADING=0xc01e, MERCS_ORIGINAL_INVENTORY=0xc01f,
    MERCS_ORIGINAL_FINANCE=0xc020, MERCS_ORIGINAL_COMPUTER=0xc021,
    MERCS_ORIGINAL_PERSONNEL=0xc022, MERCS_ORIGINAL_CONTRACTS=0xc024,
    MERCS_ORIGINAL_ROSTER=0xc025, MERCS_ORIGINAL_NEWS=0xc026,
    MERCS_ORIGINAL_REPAIR=0xc028, MERCS_ORIGINAL_MENU=0xc02b,
    MERCS_ORIGINAL_SAVE=0xc02e, MERCS_ORIGINAL_GROUPS=0xc030,
    MERCS_ORIGINAL_MECHLAB=0xc031, MERCS_ORIGINAL_MECHOPS=0xc032,
    MERCS_ORIGINAL_BRIEFING=0xc033, MERCS_ORIGINAL_COMMAND=0xc036,
    MERCS_ORIGINAL_CREW_BIO=0xc038
};
int mercs_interchange_screen_valid(uint32_t original_event);

/* Prepare a staged campaign for a standalone original manual save. This
 * writes recovered screen/restore/accepted-seed fields, never rebuilds lists.
 * Caller supplies the original event corresponding to the resumed room.
 * Failure leaves the campaign intact; normal store_write then emits the
 * same payload bound to its native continuation companion. */
int mercs_interchange_prepare(mercs_campaign *campaign,const mercs_shell_state *view,
                               uint32_t original_event,char *error,size_t error_size);

/* Import a complete standalone DOS or retail Windows manual save. Unknown bytes and every
 * list survive; only original restore markers/Save-New/Quit flags are reset.
 * CPGN identity is matched against original saved fields without reloading
 * shop/salvage/owned records. Embedded old results are resumed only when the
 * saved original event is the debrief and its restore guard is set.
 * The caller maps original_event to its own frontend resume_page before
 * saving the imported career. out/view/event remain untouched on failure.
 * The source wire layout is retained. Windows0x853 mech-row files preserve
 * all three extension bytes separately from the common0x850 runtime record. */
int mercs_interchange_import(prj_archive *archive,const uint8_t *bytes,size_t size,
                              mercs_campaign *out,mercs_shell_state *view,
                              uint32_t *original_event,char *error,size_t error_size);
#endif
