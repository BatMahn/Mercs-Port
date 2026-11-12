#ifndef MERCS_EXCHANGE_H
#define MERCS_EXCHANGE_H

#include <stddef.h>
#include <stdint.h>
#include "mercsresult.h"
#include "mercsreturn.h"
#include "mercscareer.h"

/* Native process transport, NOT an original Mercs save format. A private,
 * unique launch directory binds complete original-format result payloads to
 * the shell invocation that produced them. No CD data or owned MEK is written
 * by this module. Callers hold the edition launch lock for the entire exchange.
 * This bounded manifest can carry16 explicitly identified owned units
 * without inferring ownership from team. */
#define MERCS_EXCHANGE_UNITS 16
#define MERCS_EXCHANGE_TOKEN 33
#define MERCS_EXCHANGE_NAME 25
/* A fixed-player scene can leave a deployed MEK's complete 58-byte trailer
 * unchanged. Ordinary simulation returns use the shorter 50-byte trailer. */
#define MERCS_EXCHANGE_MEK_BYTES \
    (MERCS_CAREER_DEPLOY_MAX_BYTES > MERCS_RETURN_MAX_BYTES ? \
     MERCS_CAREER_DEPLOY_MAX_BYTES : MERCS_RETURN_MAX_BYTES)

typedef struct {
    int32_t actor_index;             /* -1 player; -2/-3 USERSTAR slots1/2;
                                     * otherwise explicit native actor index.
                                     * Negative slot keys survive the return. */
    uint16_t controller_flags;
    int32_t alliance;
    char name[MERCS_EXCHANGE_NAME];   /* original controller filename */
} mercs_exchange_unit;

typedef struct {
    char token[MERCS_EXCHANGE_TOKEN];
    char scene[17], edition[12];
    size_t unit_count;
    mercs_exchange_unit units[MERCS_EXCHANGE_UNITS];
} mercs_exchange_launch;

typedef struct {
    int32_t actor_index;
    char name[MERCS_EXCHANGE_NAME];
    size_t size;
    uint8_t bytes[MERCS_EXCHANGE_MEK_BYTES];
} mercs_exchange_mek;

typedef struct {
    uint8_t career[MERCS_CAR_BYTES], mission[MERCS_MSN_BYTES];
    size_t mech_count;
    mercs_exchange_mek mechs[MERCS_EXCHANGE_UNITS];
} mercs_exchange_result;

enum {
    MERCS_EXCHANGE_OK = 0,
    MERCS_EXCHANGE_PENDING = 1,
    MERCS_EXCHANGE_APPLIED = 2,
    MERCS_EXCHANGE_INVALID = -1,
    MERCS_EXCHANGE_IO = -2
};

/* Begin assigns a unique token and commits the explicit launch manifest.
 * Before begin, token must be empty. No existing launch is overwritten.
 * load validates token syntax and the complete, checksummed manifest. */
int mercs_exchange_begin(const char *overlay, mercs_exchange_launch *launch);
int mercs_exchange_load(const char *overlay, const char *token,
                        mercs_exchange_launch *launch);

/* Publish writes and flushes one complete packet to an exclusive temporary
 * file, then renames it. A crash/partial write leaves no readable result.
 * Read checks the launch identity, exact lengths, checksum and every returned
 * owned identity. It leaves output unchanged on pending/error/already-applied.
 * Original payloads are retained verbatim inside this native envelope. */
int mercs_exchange_publish(const char *overlay, const mercs_exchange_launch *launch,
                           const mercs_exchange_result *result);
int mercs_exchange_read(const char *overlay, const mercs_exchange_launch *launch,
                        mercs_exchange_result *result);
/* Call only after the caller has committed the staged import. Records remain
 * available for diagnosis; no recursive cleanup or original-file deletion. */
int mercs_exchange_acknowledge(const char *overlay,
                               const mercs_exchange_launch *launch);

/* Titanium SALVAGE.MW2 beside the result; read returns PENDING when absent. */
int mercs_exchange_publish_salvage(const char *overlay, const mercs_exchange_launch *launch,
                                   const uint8_t *bytes, size_t size);
int mercs_exchange_read_salvage(const char *overlay, const mercs_exchange_launch *launch,
                                uint8_t *bytes, size_t capacity, size_t *size);

#endif
