/* Golden streams are captured from the supplied original DOS and Windows
 * executables. Inputs are the declared original snapshots, not output-derived
 * reconstructions. No original binaries or game files are needed here. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mercsresult.h"
#include "../tests/fixtures/result-audit/result_format_original_fixtures.h"

static void equal_bytes(const char *name, const uint8_t *got,
                        const uint8_t *expected, size_t size)
{
    size_t i;
    for (i = 0; i < size; i++) if (got[i] != expected[i]) {
        fprintf(stderr, "%s: byte%zu native=%02x original=%02x\n",
                name, i, got[i], expected[i]);
        assert(got[i] == expected[i]);
    }
}

static void unchanged(const void *a, const void *b, size_t size)
{
    assert(!memcmp(a, b, size));
}

static void original_car_cases(void)
{
    uint8_t out[MERCS_CAR_BYTES + 8];
    mercs_car_record parsed, initial;
    size_t i, j, written;
    for (i = 0; i < MERCS_CAR_ORIGINAL_FIXTURE_COUNT; i++) {
        const mercs_car_original_fixture *f = &mercs_car_original_fixtures[i];
        memset(out, 0xa5, sizeof out);
        assert(mercs_car_encode(&f->input, out, MERCS_CAR_BYTES, &written) == MERCS_RESULT_OK);
        assert(written == MERCS_CAR_BYTES);
        equal_bytes(f->name, out, f->expected, MERCS_CAR_BYTES);
        for (j = MERCS_CAR_BYTES; j < sizeof out; j++) assert(out[j] == 0xa5);
        assert(mercs_car_parse(out, written, &parsed) == MERCS_RESULT_OK);
        unchanged(&parsed, &f->input, sizeof parsed);
    }
    assert(mercs_car_init_dos(&initial) == MERCS_RESULT_OK);
    equal_bytes("fresh DOS initialization", initial.bytes, mercs_car_dos_initial_expected, MERCS_CAR_BYTES);
    /* The observed edition difference must not silently become the DOS default. */
    assert(mercs_car_player_status(&initial) == MERCS_CAR_STATUS_DOS_INITIAL);
    assert(mercs_car_windows_initial_expected[MERCS_CAR_PLAYER_STATUS_OFFSET] == 1);
    printf("PASS: %zu full 214-byte CAR streams and fresh DOS initialization match original code\n", i);
}

static void original_loss_cases(void)
{
    size_t i, j;
    for (i = 0; i < MERCS_CAR_LOSS_FIXTURE_COUNT; i++) {
        const mercs_car_loss_original_fixture *f = &mercs_car_loss_original_fixtures[i];
        mercs_car_record car, before;
        mercs_car_loss_context loss;
        for (j = 0; j < sizeof car.bytes; j++) car.bytes[j] = (uint8_t)(29 * j + 7);
        car.bytes[MERCS_CAR_PLAYER_STATUS_OFFSET] = f->prior_status;
        before = car;
        loss.is_player = f->is_player;
        loss.actor_flags = f->actor_flags;
        loss.invulnerable = f->invulnerable;
        loss.controller_state = f->controller_state;
        loss.hazardous_ejection = f->hazardous_ejection;
        assert(mercs_car_note_loss(&car, &loss) == MERCS_RESULT_OK);
        assert(mercs_car_player_status(&car) == f->expected_status);
        assert(loss.controller_state == f->expected_controller_state);
        /* This helper intentionally owns no counter increments or opaque bytes. */
        before.bytes[MERCS_CAR_PLAYER_STATUS_OFFSET] = f->expected_status;
        unchanged(&car, &before, sizeof car);
    }
    printf("PASS: %zu original guarded loss/status and hazardous-ejection state cases\n", i);
}

static void original_msn_cases(void)
{
    uint8_t out[MERCS_MSN_BYTES + 8], roundtrip[MERCS_MSN_BYTES];
    mercs_msn_record parsed;
    size_t i, j, written;
    for (i = 0; i < MERCS_MSN_ORIGINAL_FIXTURE_COUNT; i++) {
        const mercs_msn_original_fixture *f = &mercs_msn_original_fixtures[i];
        memset(out, 0xa5, sizeof out);
        assert(mercs_msn_encode_selected(f->stars, f->star_count, f->selected_star,
                                        out, MERCS_MSN_BYTES, &written) == MERCS_RESULT_OK);
        assert(written == MERCS_MSN_BYTES);
        equal_bytes(f->name, out, f->expected, MERCS_MSN_BYTES);
        for (j = MERCS_MSN_BYTES; j < sizeof out; j++) assert(out[j] == 0xa5);
        assert(mercs_msn_parse(out, written, &parsed) == MERCS_RESULT_OK);
        assert(mercs_msn_encode(&parsed, roundtrip, sizeof roundtrip, &written) == MERCS_RESULT_OK);
        equal_bytes(f->name, roundtrip, f->expected, MERCS_MSN_BYTES);
    }
    /* Parsed unknown values and unused physical rows round-trip unchanged. */
    for (j = 0; j < MERCS_MSN_BYTES; j++) out[j] = (uint8_t)(j * 97 + 251);
    memcpy(out, "MW2M", 4);
    memset(out + 4, 0, 4);
    assert(mercs_msn_parse(out, MERCS_MSN_BYTES, &parsed) == MERCS_RESULT_OK);
    assert(mercs_msn_encode(&parsed, roundtrip, sizeof roundtrip, &written) == MERCS_RESULT_OK);
    equal_bytes("opaque physical MSN rows", roundtrip, out, MERCS_MSN_BYTES);
    printf("PASS: %zu full 2516-byte selected-star streams match both originals; opaque parser round-trip\n", i);
}

static void failure_is_atomic(void)
{
    uint8_t out[MERCS_MSN_BYTES], before[MERCS_MSN_BYTES], bad[MERCS_MSN_BYTES];
    mercs_car_record car, car_before;
    mercs_msn_record record, record_before;
    mercs_msn_star star;
    size_t written = 99;
    memset(out, 0x5a, sizeof out);
    memcpy(before, out, sizeof out);
    memset(&car, 0x69, sizeof car);
    car_before = car;
    assert(mercs_car_parse(before, 80, &car) == MERCS_RESULT_INVALID);
    assert(mercs_car_parse(before, MERCS_CAR_BYTES + 1, &car) == MERCS_RESULT_INVALID);
    unchanged(&car, &car_before, sizeof car);
    assert(mercs_car_encode(&car, out, MERCS_CAR_BYTES - 1, &written) == MERCS_RESULT_NO_SPACE);
    assert(written == 0);
    unchanged(out, before, sizeof out);
    assert(mercs_car_set_player_status(&car, 3) == MERCS_RESULT_INVALID);
    unchanged(&car, &car_before, sizeof car);
    assert(mercs_car_set_player_status(&car, MERCS_CAR_STATUS_DEAD) == MERCS_RESULT_OK);
    car_before.bytes[MERCS_CAR_PLAYER_STATUS_OFFSET] = 4;
    unchanged(&car, &car_before, sizeof car);

    memset(&record, 0x96, sizeof record);
    record_before = record;
    memcpy(bad, mercs_msn_original_fixtures[0].expected, sizeof bad);
    bad[0] = 'X';
    assert(mercs_msn_parse(bad, sizeof bad, &record) == MERCS_RESULT_INVALID);
    bad[0] = 'M'; bad[4] = 49;
    assert(mercs_msn_parse(bad, sizeof bad, &record) == MERCS_RESULT_INVALID);
    assert(mercs_msn_parse(bad, sizeof bad - 1, &record) == MERCS_RESULT_INVALID);
    unchanged(&record, &record_before, sizeof record);
    memset(&record, 0, sizeof record);
    record.row_count = 49;
    assert(mercs_msn_encode(&record, out, sizeof out, &written) == MERCS_RESULT_INVALID);
    assert(written == 0);
    record.row_count = 0;
    assert(mercs_msn_encode(&record, out, sizeof out - 1, &written) == MERCS_RESULT_NO_SPACE);
    assert(written == 0);
    memset(&star, 0, sizeof star);
    assert(mercs_msn_encode_selected(&star, 1, 1, out, sizeof out, &written) == MERCS_RESULT_INVALID);
    star.node_count = 49;
    assert(mercs_msn_encode_selected(&star, 1, 0, out, sizeof out, &written) == MERCS_RESULT_INVALID);
    star.node_count = 1;
    star.nodes[0].visible = 1;
    memset(star.nodes[0].text, 'A', sizeof star.nodes[0].text);
    assert(mercs_msn_encode_selected(&star, 1, 0, out, sizeof out, &written) == MERCS_RESULT_INVALID);
    assert(written == 0);
    unchanged(out, before, sizeof out);

    /* The original's final physical row can overwrite its own stack locals.
     * That unsupported corruption must not escape our staged payload. */
    memset(&star, 0, sizeof star);
    star.node_count = MERCS_MSN_ROWS;
    for (size_t i = 0; i < MERCS_MSN_ROWS; i++) star.nodes[i].visible = 1;
    memset(star.nodes[MERCS_MSN_ROWS - 1].text, 'A', MERCS_MSN_TEXT_BYTES);
    assert(mercs_msn_encode_selected(&star, 1, 0, out, sizeof out, &written) == MERCS_RESULT_INVALID);
    assert(written == 0);
    unchanged(out, before, sizeof out);

    assert(mercs_car_parse(NULL, MERCS_CAR_BYTES, &car) == MERCS_RESULT_INVALID);
    assert(mercs_car_init_dos(NULL) == MERCS_RESULT_INVALID);
    assert(mercs_car_note_loss(&car, NULL) == MERCS_RESULT_INVALID);
    assert(mercs_car_player_status(NULL) == MERCS_RESULT_INVALID);
    assert(mercs_msn_parse(NULL, MERCS_MSN_BYTES, &record) == MERCS_RESULT_INVALID);
    assert(mercs_msn_encode_selected(NULL, 1, 0, out, sizeof out, &written) == MERCS_RESULT_INVALID);
    puts("PASS: truncated, malformed, out-of-range and overflowing inputs leave destinations unchanged");
}

int main(void)
{
    original_car_cases();
    original_loss_cases();
    original_msn_cases();
    failure_is_atomic();
    return 0;
}
