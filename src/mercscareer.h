#ifndef MERCS_CAREER_H
#define MERCS_CAREER_H

#include <stddef.h>
#include <stdint.h>

/* Original retail DOS MW2SHELL record. This is a byte-preserving record,
 * not the Windows shell's different 0x853-byte record and not a full career.
 * All multibyte fields are accessed explicitly as little endian. */
#define MERCS_CAREER_MECH_BYTES 0x850
#define MERCS_CAREER_FILENAME_BYTES 32
#define MERCS_CAREER_DEPLOY_MAX_BYTES (24 + 8 * 40 + (14 + 25) * 8 + 58)

typedef struct {
    uint8_t bytes[MERCS_CAREER_MECH_BYTES];
} mercs_career_mech;

typedef struct {
    mercs_career_mech current;
    mercs_career_mech reference;
} mercs_career_mech_pair;

enum mercs_career_status {
    MERCS_CAREER_OK = 0,
    MERCS_CAREER_INVALID = -1,
    MERCS_CAREER_NO_SPACE = -2,
    MERCS_CAREER_UNSUPPORTED = -3
};

enum mercs_career_import_action {
    MERCS_CAREER_IMPORT_SKIPPED = 0,
    MERCS_CAREER_IMPORT_APPLIED = 1,
    MERCS_CAREER_IMPORT_MISSING = 2,
    MERCS_CAREER_IMPORT_RESTORE = 3
};

typedef struct {
    enum mercs_career_import_action action;
    int request_delete; /* only after a successful marked normal-return import */
    char filename[MERCS_CAREER_FILENAME_BYTES];
} mercs_career_import_result;

/* Initialize a supported MEK using original DOS shell conversion, then copy
 * current into reference as in original initial ownership. IDs and the owned
 * filename are supplied by the traced caller; this API allocates no identity.
 * All failures leave the pair unchanged. */
int mercs_career_pair_init(mercs_career_mech_pair *out,
                           const uint8_t *mek, size_t length,
                           const char *owned_filename, uint32_t instance_id,
                           int32_t association_id);

/* Normal load: exact marker 1 permits an import. A missing MEK clears that
 * marker but leaves current damage/loadout intact. restore_mode==1 clears the
 * marker without reading returned bytes. Reference/identity remain unchanged.
 * A malformed supplied MEK fails transactionally; no disk file is deleted. */
int mercs_career_import_return(mercs_career_mech_pair *pair, int restore_mode,
                              const uint8_t *mek, size_t length,
                              mercs_career_import_result *result);

/* Produce the original shell's next deployment MEK, including its 58-byte
 * trailer, from current state. Does not set the deployed marker or write files. */
int mercs_career_deployment_bytes(const mercs_career_mech *current,
                                 uint8_t *out, size_t capacity, size_t *written);

/* Mark only after a deployment file has been committed by the caller. */
void mercs_career_set_deployed(mercs_career_mech *current, int deployed);
int mercs_career_deployed(const mercs_career_mech *current);
int mercs_career_filename(const mercs_career_mech *current,
                          char *out, size_t capacity);
uint32_t mercs_career_instance_id(const mercs_career_mech *current);
int32_t mercs_career_association_id(const mercs_career_mech *current);
void mercs_career_set_association(mercs_career_mech *current, int32_t association_id);
/* CPGN MHLT/SMHL metadata, applied before making an initial reference copy. */
void mercs_career_set_acquisition(mercs_career_mech *current, int32_t value,
                                 int32_t year, int32_t threshold);

/* Original zero-head/zero-center-torso predicate only. Removal of a mech,
 * its reference or pilot requires the separate original cleanup guards. */
int mercs_career_mech_lost(const mercs_career_mech *current);

#endif
