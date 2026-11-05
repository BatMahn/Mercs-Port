#ifndef MERCS_PANELS_H
#define MERCS_PANELS_H

#include <stddef.h>
#include <stdint.h>

/* Original Merc Lords and First Circuit tables.
 * The original executable remains external game data. No machine code is
 * executed by this reader, and no original credits or score table is embedded
 * in the port. Only the supplied retail DOS MW2SHELL.EXE layout is supported.
 */
enum {
    MERCS_PANELS_OK = 0,
    MERCS_PANELS_INVALID = -1,
    MERCS_PANELS_UNSUPPORTED = -2,
    MERCS_CIRCUIT_ROWS = 422,
    MERCS_CIRCUIT_LINE_BYTES = 96,
    MERCS_LORDS_ROWS = 10,
    MERCS_LORDS_FILE_BYTES = 694
};
typedef struct mercs_lords_row {
    char pilot[32], group[32];
    int32_t wealth;
    unsigned char highlighted;
} mercs_lords_row;
typedef struct mercs_lords_table {
    unsigned count;
    mercs_lords_row rows[MERCS_LORDS_ROWS];
} mercs_lords_table;
typedef struct mercs_panels {
    /* Empty credit rows preserve the original NULL table entries and spacing.
     * Leading '<', '>', '~' select the original rendering styles. */
    char credits[MERCS_CIRCUIT_ROWS][MERCS_CIRCUIT_LINE_BYTES];
    char headings[3][32];
    char scores_filename[16];
    mercs_lords_table defaults;
} mercs_panels;

/* Decode atomically: failure leaves output unchanged. The complete recognized
 * file length and a 64-bit version fingerprint are checked before using the
 * original LE data-object offsets. This is version identification, not a
 * security/authenticity claim or a general-purpose LE loader. */
int mercs_panels_decode(const unsigned char *exe, size_t length, mercs_panels *out);

/* Original mw2high.cfg is a count followed by ten fixed 69-byte rows. Native
 * bounds reject truncated/extra data, count >10 and unterminated active names.
 * Read order and the original flag byte are preserved; do not re-sort an
 * existing file. Missing file uses defaults; corrupt file is a visible error.
 * All ten rows, including inactive bytes, survive decoding. */
int mercs_lords_decode(const unsigned char *bytes, size_t length, mercs_lords_table *out);
int mercs_lords_encode(const mercs_lords_table *table, unsigned char out[MERCS_LORDS_FILE_BYTES]);
/* DOS0x46e84. Clears all highlight flags in memory, replaces the first strict
 * minimum below500,000,000 only when earned wealth is greater, then executes
 * the original small-sort order. Return1 means the original writes the table;
 * return0 leaves the original disk file unchanged. Negative is invalid input. */
int mercs_lords_submit(mercs_lords_table *table, const mercs_lords_row *earned);

#endif
