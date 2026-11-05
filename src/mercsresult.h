#ifndef MERCS_RESULT_H
#define MERCS_RESULT_H

#include <stddef.h>
#include <stdint.h>

/* Canonical Mercs DOS writers: CAR 0x65a10 -> 0x5ec04; MSN 0x1e0bc.
 * Retail Windows counterparts: 0x1004bf99 -> 0x10047c9e; 0x1004fafb.
 * This module performs no file I/O and does not decide mission outcomes. */
#define MERCS_CAR_BYTES 214
#define MERCS_CAR_PLAYER_STATUS_OFFSET 0x1d
#define MERCS_CAR_RESULT_INDEX_OFFSET 0xd2
#define MERCS_MSN_BYTES 2516
#define MERCS_MSN_ROWS 48
#define MERCS_MSN_TEXT_BYTES 32
#define MERCS_MSN_SOURCE_TEXT_BYTES 64

enum {
    MERCS_RESULT_OK = 0,
    MERCS_RESULT_INVALID = -1,
    MERCS_RESULT_NO_SPACE = -2
};

/* Unknown counters and the multiplayer tail remain explicit original bytes.
 * An 80-byte record is not a complete Mercs simulation CAR record. */
typedef struct {
    uint8_t bytes[MERCS_CAR_BYTES];
} mercs_car_record;

int mercs_car_parse(const uint8_t *bytes, size_t length, mercs_car_record *out);
int mercs_car_encode(const mercs_car_record *record, uint8_t *out,
                     size_t capacity, size_t *written);

/* Fresh canonical DOS simulation record: 214 cleared bytes and final DWORD
 * -1. DOS main's early status=1 is subsequently cleared by 0x63d90. Retail
 * Windows instead sets status=1 after clearing; it is not the DOS default.
 * This is fresh simulation state, not an expansion of an 80-byte input. */
int mercs_car_init_dos(mercs_car_record *out);

enum {
    MERCS_CAR_STATUS_DOS_INITIAL = 0,
    MERCS_CAR_STATUS_WINDOWS_INITIAL = 1,
    MERCS_CAR_STATUS_EJECTED = 2,
    MERCS_CAR_STATUS_DEAD = 4
};

/* Only these four observed values may be assigned through this accessor;
 * parse/encode still preserve all unknown original bytes losslessly. */
int mercs_car_set_player_status(mercs_car_record *record, uint8_t status);
int mercs_car_player_status(const mercs_car_record *record);

typedef struct {
    int is_player;
    uint16_t actor_flags;
    int invulnerable;
    int32_t controller_state;
    int hazardous_ejection;
} mercs_car_loss_context;

/* The bounded original loss-status branch, DOS 0x2f0c0..0x2f266 / Windows
 * 0x10004b19..0x10004d58. Applies its guards, updates only the player-status
 * byte and converts hazardous ejection state5 to state4, also for AI.
 * Counters, network calls, audio and later actor/damage changes are outside
 * this helper; this is not the complete original destruction routine. */
int mercs_car_note_loss(mercs_car_record *record, mercs_car_loss_context *loss);

/* Original single-player statistic producers. Every counter increment wraps
 * as an unsigned16 field. Alliance is the raw original byte: 0 friendly,
 * 1 enemy, 2 neutral; other values do not increment alliance triples. */
int mercs_car_count_initial(mercs_car_record *record, int is_thing, uint8_t alliance);
typedef struct {
    mercs_car_loss_context loss;
    int32_t unit_class;       /* GP runtime class, not the GPS objective mask */
    uint8_t alliance;
    int same_player_star;    /* exact original actor+8 equality */
    int killer_is_player;
} mercs_car_unit_loss;

/* DOS0x2f0c0..0x2f3ac / Windows0x10004b19..0x10004f45. Includes the
 * guarded same-star loss, player credit, flags0x1400 total, status and
 * hazardous-state conversion; excludes network and later wreck effects. */
int mercs_car_count_unit_loss(mercs_car_record *record, mercs_car_unit_loss *event);

/* DOS0x65748 / Windows0x1004bbc3: thing flags4 skips an already-destroyed
 * thing. Player credit and exact same-star credit are independent. These
 * fields count credited things, not every world object that disappears. */
int mercs_car_count_thing_loss(mercs_car_record *record, uint16_t thing_flags,
                             uint8_t alliance, int killer_is_player,
                             int killer_in_player_star);

/* The reached projectile-hit counter slice DOS0x642d0..0x64393. Caller owns
 * collision/weapon guards and identity; no damage quantity is inferred. */
int mercs_car_count_unit_hit(mercs_car_record *record, uint8_t victim_alliance,
                            int source_is_player, int source_in_player_star,
                            int victim_is_player, int victim_in_player_star);
/* Count an accepted projectile release at its existing simulation boundary.
 * Whole weapon trigger/volley scheduling remains a separate audit. */
int mercs_car_count_shot(mercs_car_record *record, int source_is_player,
                         int source_in_player_star);

/* Source values at the original runtime-star serializer boundary. Times are
 * signed simulation seconds; -1 is preserved. Outcome is copied unchanged:
 * 0 unresolved, 2 success, 3 failure, 4 timed out in the traced updater.
 * Abort, ejection and death do not themselves select a value in this writer. */
typedef struct {
    uint8_t visible, runtime_state, priority;
    int32_t started_seconds, resolved_seconds;
    uint32_t objective_value;
    char text[MERCS_MSN_SOURCE_TEXT_BYTES];
} mercs_msn_node;

typedef struct {
    int32_t started_seconds, resolved_seconds;
    uint8_t outcome;
    size_t node_count;
    mercs_msn_node nodes[MERCS_MSN_ROWS];
} mercs_msn_star;

/* The original writer uses a selected STAR INDEX, not a bitmask. It compacts
 * visible nodes in source order, writes (runtime_state == 5), and copies the
 * full objective DWORD. Authored objective strings have up to 63 characters,
 * although each physical result row has only 32 text bytes. The original
 * ordered strcpy can extend into following rows; subsequent visible rows
 * overwrite their own fields. Reproduce those bytes within the result buffer
 * without performing a C array overflow. Reject unterminated source text and
 * any copy beyond the complete 2516-byte payload (original stack corruption).
 * The buffer is initially zeroed, including unused rows. */
int mercs_msn_encode_selected(const mercs_msn_star *stars, size_t star_count,
                              size_t selected_star, uint8_t *out,
                              size_t capacity, size_t *written);

/* Lossless parsed representation: all 48 physical rows are retained, even
 * beyond row_count. Unknown DWORD values and non-NUL text are preserved;
 * this is distinct from constructing a safe result from runtime stars. */
typedef struct {
    uint32_t succeeded, priority;
    int32_t started_seconds, resolved_seconds;
    uint32_t objective_value;
    uint8_t text[MERCS_MSN_TEXT_BYTES];
} mercs_msn_row;

typedef struct {
    uint32_t row_count;
    int32_t started_seconds, resolved_seconds;
    uint32_t outcome;
    mercs_msn_row rows[MERCS_MSN_ROWS];
} mercs_msn_record;

int mercs_msn_parse(const uint8_t *bytes, size_t length, mercs_msn_record *out);
int mercs_msn_encode(const mercs_msn_record *record, uint8_t *out,
                     size_t capacity, size_t *written);

/* Every failed parse/encode leaves its destination untouched. Encoders set
 * *written=0 on failure and commit the entire output only on success. */
#endif
