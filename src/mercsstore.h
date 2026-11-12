#ifndef MERCS_STORE_H
#define MERCS_STORE_H
#include "mercscampaign.h"

/* Each generation contains genuine DOS SHELLMER/mersave bytes and a separate
 * native resume companion. Only the small slot index is atomically replaced;
 * a partial new generation cannot damage the previous selected generation. */
int mercs_store_directory(const char *retail_overlay,char *out,size_t capacity);
int mercs_store_write(const char *directory,int slot,
                       const mercs_campaign *campaign,const mercs_shell_state *view,
                       char *error,size_t error_size);
/* Native-only attachment: Titanium's wreck list (salvage.mw2 bytes) kept in
 * the same generation, so a career saved during its debrief keeps its
 * Salvage Yard. The original save bytes are unchanged. */
int mercs_store_write_attached(const char *directory,int slot,
                       const mercs_campaign *campaign,const mercs_shell_state *view,
                       const uint8_t *salvage,size_t salvage_size,
                       char *error,size_t error_size);
/* The selected generation's attachment: 1 none, 0 read, -1 invalid. */
int mercs_store_read_salvage(const char *directory,int slot,uint8_t *out,size_t capacity,size_t *size);
/* slot -1 is the working career; slots0..9 are the original ten user slots.
 * Returns1 when absent,0 on success,-1 for invalid/failed data. */
int mercs_store_read(const char *directory,int slot,prj_archive *archive,
                      mercs_campaign *campaign,mercs_shell_state *view,
                      char *error,size_t error_size);
/* Original header+2 slot description; an empty original description stays
 * empty. Uses the same absent/invalid return convention as read. */
int mercs_store_title(const char *directory,int slot,char *out,size_t capacity);

/* Standalone original DOS manual saves. Reads require regular bounded files;
 * the native generation remains selected until the frontend has validated
 * and durably installed the import. The source file is never modified. */
int mercs_store_original_title(const char *path,char *out,size_t capacity);
int mercs_store_import_original(const char *path,prj_archive *archive,
                                 mercs_campaign *campaign,mercs_shell_state *view,
                                 uint32_t *original_event,char *error,size_t error_size);
/* Copy the selected validated generation's exact original payload to the
 * conventional mersaveN.sav in the writable retail overlay. Each copy is
 * atomic; an export error does not invalidate its already committed native
 * generation. Caller reports that distinction and can retry. */
int mercs_store_export_original(const char *directory,int slot,const char *retail_overlay,
                                 char *error,size_t error_size);
/* 0 same bytes,1 different or no native generation,-1 unreadable. Used only
 * to offer an explicit in-game choice when a loose file differs from a slot. */
int mercs_store_original_differs(const char *directory,int slot,const char *path);

#endif
