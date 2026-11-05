/* Exercise the real msim capture functions using independent original-code
 * snapshots and full writer bytes. This checks representation/transport and
 * the specified MTBL initialization/aggregation boundary, not full missions. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "msim.h"
#include "mercsresult.h"
#include "../tests/fixtures/result-audit/result_format_original_fixtures.h"
#include "../tests/fixtures/result-audit/result_lifecycle_original_fixtures.h"
#include "../tests/fixtures/result-audit/result_rearm_original_fixtures.h"

/* msim contains the complete mission table set; keep it off the test stack. */
static msim sim;

static void compare(const char *name, const uint8_t *got,
                    const uint8_t *expected, size_t length)
{
    size_t i;
    for (i = 0; i < length; i++) if (got[i] != expected[i]) {
        fprintf(stderr, "%s: byte%zu native=%02x original=%02x\n", name, i, got[i], expected[i]);
        assert(got[i] == expected[i]);
    }
}

static int32_t milliseconds(int32_t seconds)
{
    if (seconds == -1) return -1;
    assert(seconds >= 0 && seconds <= (INT32_MAX - 999) / 1000);
    /* Fractional native milliseconds must not become rounded seconds. */
    return seconds * 1000 + 999;
}

static int native_compatible(const mercs_msn_star *s)
{
    size_t i;
    if (s->started_seconds < -1 || s->resolved_seconds < -1 ||
        s->started_seconds > (INT32_MAX - 999) / 1000 ||
        s->resolved_seconds > (INT32_MAX - 999) / 1000) return 0;
    for (i = 0; i < s->node_count; i++) {
        const mercs_msn_node *n = &s->nodes[i];
        if (n->objective_value > 1 || n->started_seconds < -1 || n->resolved_seconds < -1 ||
            n->started_seconds > (INT32_MAX - 999) / 1000 ||
            n->resolved_seconds > (INT32_MAX - 999) / 1000) return 0;
    }
    return 1;
}

/* Construct native inputs from the declared ORIGINAL runtime snapshot.
 * No expected output bytes are read while doing this conversion. */
static void seed_table(msim *s, int index, const mercs_msn_star *in)
{
    mtbl_table *t = &s->logic.tables[index];
    size_t i;
    assert(native_compatible(in));
    memset(t, 0, sizeof *t);
    t->index = index;
    t->node_count = (int)in->node_count;
    s->logic.start_time[index] = milliseconds(in->started_seconds);
    s->logic.result_time[index] = milliseconds(in->resolved_seconds);
    s->logic.result[index] = in->outcome;
    s->logic.current[index] = -1;
    if (s->logic.table_count <= index) s->logic.table_count = index + 1;
    for (i = 0; i < in->node_count; i++) {
        const mercs_msn_node *src = &in->nodes[i];
        mtbl_node *dst = &t->nodes[i];
        dst->kind = MTBL_K_TIMER;
        dst->delay = UINT32_MAX;
        dst->visible = src->visible != 0;
        dst->objective = src->objective_value ? 'M' : 'O';
        dst->priority = src->priority;
        /* require_all=0 and no dependencies keeps this boundary's declared
         * node states inert during the separate star aggregation tests. */
        strcpy(dst->text, src->text);
        s->logic.shown[index][i] = src->visible;
        s->logic.state[index][i] = src->runtime_state;
        s->logic.started[index][i] = milliseconds(src->started_seconds);
        s->logic.ended[index][i] = milliseconds(src->resolved_seconds);
    }
}

static void capture_equals(const char *name, const uint8_t *expected)
{
    mercs_msn_star captured;
    uint8_t bytes[MERCS_MSN_BYTES];
    size_t written;
    assert(msim_capture_result_star(&sim, &captured) == 0);
    assert(mercs_msn_encode_selected(&captured, 1, 0, bytes, sizeof bytes, &written) == MERCS_RESULT_OK);
    assert(written == sizeof bytes);
    compare(name, bytes, expected, sizeof bytes);
}

static void writer_transport_cases(void)
{
    size_t i, j, count = 0, excluded = 0;
    for (i = 0; i < MERCS_MSN_ORIGINAL_FIXTURE_COUNT; i++) {
        const mercs_msn_original_fixture *f = &mercs_msn_original_fixtures[i];
        int compatible = 1;
        for (j = 0; j < f->star_count; j++) compatible &= native_compatible(&f->stars[j]);
        if (!compatible) { excluded++; continue; }
        memset(&sim, 0, sizeof sim);
        for (j = 0; j < f->star_count; j++) seed_table(&sim, (int)j, &f->stars[j]);
        sim.player_table = (int)f->selected_star;
        sim.now = 987654;
        sim.ended_at = 876543;
        sim.outcome = 99;  /* these exit/display values must not override the star */
        capture_equals(f->name, f->expected);
        count++;
    }
    /* The pure codec tests cover the one deliberately unrepresentable native
     * fixture: arbitrary runtime DWORD and signed-second extremes. */
    assert(excluded == 1);
    printf("PASS: %zu native selected-star captures match complete original writer streams; one pure-only range fixture\n", count);
}

static void lifecycle_transport_cases(void)
{
    size_t i;
    for (i = 0; i < MERCS_RESULT_LIFECYCLE_FIXTURE_COUNT; i++) {
        const mercs_result_lifecycle_fixture *f = &mercs_result_lifecycle_fixtures[i];
        memset(&sim, 0, sizeof sim);
        seed_table(&sim, 3, &f->after);
        sim.player_table = 3;
        sim.now = 999999;
        sim.ended_at = sim.now;
        sim.outcome = 255;
        capture_equals(f->name, f->expected);
    }
    printf("PASS: %zu captured original lifecycle states survive native millisecond/selected-star transport\n", i);
}

static void aggregation_cases(void)
{
    size_t i, count = 0;
    for (i = 0; i < MERCS_RESULT_LIFECYCLE_FIXTURE_COUNT; i++) {
        const mercs_result_lifecycle_fixture *f = &mercs_result_lifecycle_fixtures[i];
        if (f->operation != 2 || f->forced_success) continue;
        memset(&sim, 0, sizeof sim);
        seed_table(&sim, 2, &f->before);
        sim.player_table = 2;
        sim.logic.tables[2].time_limit = (uint32_t)f->time_limit;
        /* Same fractional whole-second inputs as the original aggregation
         * captures. Their per-node evaluation was explicitly intercepted. */
        mtbl_logic_step(&sim.logic, milliseconds(f->now_seconds), NULL);
        capture_equals(f->name, f->expected);
        count++;
    }
    printf("PASS: %zu actual native star aggregation cases match the original boundary (forced-success global excluded)\n", count);
}

static void started_callback(void *user, int table, int node)
{
    int *count = user;
    assert(table == 0 && node == 0);
    (*count)++;
}

static void original_arming_and_control(void)
{
    size_t i;
    for (i = 0; i < MERCS_RESULT_ARM_FIXTURE_COUNT; i++) {
        const mercs_result_arm_fixture *f = &mercs_result_arm_fixtures[i];
        mtbl_world world;
        int started = 0;
        memset(&sim, 0, sizeof sim);
        seed_table(&sim, 0, &f->before);
        sim.player_table = 0;
        sim.logic.tables[0].nodes[0].require_all = 1;
        memset(&world, 0, sizeof world);
        world.user = &started;
        world.started = started_callback;
        mtbl_logic_step(&sim.logic, milliseconds(f->now_seconds), &world);
        capture_equals(f->name, f->expected);
        /* This is the native newly-dated callback contract, matched to the
         * original new-start marker, not a full radio consumer simulation. */
        assert(started == f->new_start_marker);
    }
    for (i = 0; i < MERCS_RESULT_CONTROL_FIXTURE_COUNT; i++) {
        const mercs_result_control_fixture *f = &mercs_result_control_fixtures[i];
        mtbl_node *command;
        memset(&sim, 0, sizeof sim);
        seed_table(&sim, 0, &f->before_stars[0]);
        seed_table(&sim, 1, &f->before_stars[1]);
        sim.player_table = 1;
        command = &sim.logic.tables[0].nodes[0];
        command->kind = f->kind;
        command->require_all = 1;
        command->other_table = 1;
        command->other_node = 0;
        mtbl_logic_step(&sim.logic, milliseconds(f->command_seconds), NULL);
        capture_equals(f->name, f->command_expected);
        /* Later readiness arms state3/start-1 without changing the target
         * star's retained start. Original REARM command capture ends before
         * its unsafe announcement-index write; only target metadata is used. */
        sim.logic.tables[1].nodes[0].require_all = 1;
        mtbl_logic_step(&sim.logic, milliseconds(f->ready_seconds), NULL);
        capture_equals(f->name, f->ready_expected);
    }
    printf("PASS: %zu actual ready-node arming and %zu RESET/REARM metadata sequences match original captures\n",
           MERCS_RESULT_ARM_FIXTURE_COUNT, MERCS_RESULT_CONTROL_FIXTURE_COUNT);
}

static void write32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16); p[3] = (uint8_t)(value >> 24);
}

static void loader_and_activation(void)
{
    uint8_t scene[12 + sizeof mercs_result_loader_chunk];
    mtbl_table decoded;
    mercs_msn_star captured;
    const mercs_msn_star *original = &mercs_result_loader_expected;
    size_t i;
    assert(mtbl_decode(mercs_result_loader_chunk, sizeof mercs_result_loader_chunk, &decoded) == 0);
    assert(decoded.node_count == (int)original->node_count);
    for (i = 0; i < original->node_count; i++) {
        assert((decoded.nodes[i].objective == 'M') == (int)original->nodes[i].objective_value);
        assert(decoded.nodes[i].visible == original->nodes[i].visible);
        assert(decoded.nodes[i].priority == original->nodes[i].priority);
        assert(!strcmp(decoded.nodes[i].text, original->nodes[i].text));
    }
    memset(scene, 0, sizeof scene);
    memcpy(scene, "BWD", 3);
    write32(scene + 4, sizeof scene);
    memcpy(scene + 12, mercs_result_loader_chunk, sizeof mercs_result_loader_chunk);
    memset(&sim, 0, sizeof sim);
    assert(mtbl_logic_init(&sim.logic, scene, sizeof scene) == 0);
    sim.player_table = 0;
    assert(msim_capture_result_star(&sim, &captured) == 0);
    assert(captured.started_seconds == mercs_result_lifecycle_fixtures[0].after.started_seconds);
    assert(captured.resolved_seconds == original->resolved_seconds && captured.outcome == original->outcome);
    assert(captured.node_count == original->node_count);
    for (i = 0; i < original->node_count; i++) {
        int32_t start = i ? original->nodes[i].started_seconds :
                          mercs_result_lifecycle_fixtures[0].after.nodes[0].started_seconds;
        assert(captured.nodes[i].runtime_state == original->nodes[i].runtime_state);
        assert(captured.nodes[i].started_seconds == start);
        assert(captured.nodes[i].resolved_seconds == original->nodes[i].resolved_seconds);
        assert(captured.nodes[i].visible == original->nodes[i].visible);
        assert(captured.nodes[i].priority == original->nodes[i].priority);
        assert(captured.nodes[i].objective_value == original->nodes[i].objective_value);
        assert(!strcmp(captured.nodes[i].text, original->nodes[i].text));
    }
    puts("PASS: actual MTBL loader and initial activation retain original objective/visibility/priority/state/time fields");
}

static void native_exit_and_career(void)
{
    mercs_car_record car, baseline;
    size_t i, j;
    const mercs_msn_original_fixture *unresolved = &mercs_msn_original_fixtures[0];
    static const char *const loss_names[] = {
        "player-state4-hazard0", "player-state5-hazard0", "player-state5-hazard1"
    };
    static const int native_ejected[] = {0, 1, 2};
    memset(&sim, 0, sizeof sim);
    assert(mercs_car_init_dos(&baseline) == MERCS_RESULT_OK);
    memcpy(sim.career, baseline.bytes, sizeof sim.career);
    assert(msim_capture_career(&sim, &car) == 0);
    compare("native initial CAR", car.bytes, mercs_car_dos_initial_expected, sizeof car.bytes);
    for (i = 0; i < MERCS_CAR_ORIGINAL_FIXTURE_COUNT; i++) {
        const mercs_car_original_fixture *f = &mercs_car_original_fixtures[i];
        memcpy(sim.career, f->input.bytes, sizeof sim.career);
        assert(msim_capture_career(&sim, &car) == 0);
        compare(f->name, car.bytes, f->expected, sizeof car.bytes);
    }
    /* Abort/end_now preserves unresolved star timing/outcome and alive DOS
     * status. This does not claim the full automatic death-end updater. */
    memset(&sim, 0, sizeof sim);
    seed_table(&sim, 1, &unresolved->stars[0]);
    sim.player_table = 1;
    memcpy(sim.career, baseline.bytes, sizeof sim.career);
    sim.now = 765432;
    msim_end_now(&sim);
    capture_equals("native explicit abort unresolved", unresolved->expected);
    assert(msim_capture_career(&sim, &car) == 0);
    compare("native abort CAR", car.bytes, mercs_car_dos_initial_expected, sizeof car.bytes);
    for (i = 0; i < sizeof loss_names / sizeof loss_names[0]; i++) {
        const mercs_car_loss_original_fixture *golden = NULL;
        for (j = 0; j < MERCS_CAR_LOSS_FIXTURE_COUNT; j++)
            if (!strcmp(loss_names[i], mercs_car_loss_original_fixtures[j].name))
                golden = &mercs_car_loss_original_fixtures[j];
        assert(golden);
        for (j = 0; j < sizeof sim.career; j++) sim.career[j] = (uint8_t)(j * 31 + 7);
        memcpy(baseline.bytes, sim.career, sizeof baseline.bytes);
        sim.player_unit.destroyed = 1;
        sim.player_unit.ejected = native_ejected[i];
        assert(msim_capture_career(&sim, &car) == 0);
        baseline.bytes[MERCS_CAR_PLAYER_STATUS_OFFSET] = golden->expected_status;
        /* Original same-star loss producer precedes hazardous conversion:
         * state5 increments +36 even when the final player status is dead. */
        {
            size_t off = native_ejected[i] ? 0x36 : 0x34;
            unsigned value = (unsigned)baseline.bytes[off] | (unsigned)baseline.bytes[off + 1] << 8;
            value = (value + 1) & 0xffffu;
            baseline.bytes[off] = (uint8_t)value;
            baseline.bytes[off + 1] = (uint8_t)(value >> 8);
        }
        compare(loss_names[i], car.bytes, baseline.bytes, sizeof car.bytes);
        /* Capture must not mutate the live accumulator or infer a new star outcome. */
        for (j = 0; j < sizeof sim.career; j++) assert(sim.career[j] == (uint8_t)(j * 31 + 7));
        capture_equals(loss_names[i], unresolved->expected);
    }
    puts("PASS: native full214 preservation, explicit abort, death, normal ejection and hazardous ejection capture");
}

static void rejection_is_atomic(void)
{
    mercs_msn_star out, before;
    memset(&sim, 0, sizeof sim);
    seed_table(&sim, 0, &mercs_msn_original_fixtures[0].stars[0]);
    memset(&out, 0xa5, sizeof out);
    before = out;
    sim.player_table = -1;
    assert(msim_capture_result_star(&sim, &out) == -1);
    sim.player_table = sim.logic.table_count;
    assert(msim_capture_result_star(&sim, &out) == -1);
    sim.player_table = 0;
    sim.logic.tables[0].node_count = 49;
    assert(msim_capture_result_star(&sim, &out) == -1);
    sim.logic.tables[0].node_count = 1;
    sim.logic.tables[0].nodes[0].priority = 256;
    assert(msim_capture_result_star(&sim, &out) == -1);
    sim.logic.tables[0].nodes[0].priority = 1;
    memset(sim.logic.tables[0].nodes[0].text, 'A', sizeof sim.logic.tables[0].nodes[0].text);
    assert(msim_capture_result_star(&sim, &out) == -1);
    assert(!memcmp(&out, &before, sizeof out));
    puts("PASS: unsupported live result state is rejected without a partial capture");
}

/* Optional read-only archive sweep: this is a representation-domain check,
 * not mission completion. The second pass deliberately enables every row to
 * cover visibility toggles without pretending to advance objective states. */
static void authored_result_domain(const char *path)
{
    prj_archive *a;
    char error[256];
    int type, i, records = 0, tables = 0, nodes = 0, long_text = 0;
    int max_nodes = 0, max_text = 0;
    a = prj_open(path, error, sizeof error);
    if (!a) { fprintf(stderr, "%s\n", error); abort(); }
    type = prj_find_type(a, "BWD"); assert(type >= 0);
    for (i = 0; i < prj_symbol_count(a, type); i++) {
        prj_record r;
        int t, present = 0;
        assert(!prj_read(a, type, prj_symbol_id(a, type, i), &r));
        memset(&sim, 0, sizeof sim);
        if (mtbl_logic_init(&sim.logic, r.data, r.size)) { prj_record_free(&r); continue; }
        for (t = 0; t < sim.logic.table_count; t++) {
            mercs_msn_star captured;
            uint8_t bytes[MERCS_MSN_BYTES];
            size_t written;
            int k, pass;
            mtbl_table *table = &sim.logic.tables[t];
            if (!table->node_count) continue;
            present = 1; tables++; nodes += table->node_count;
            if (table->node_count > max_nodes) max_nodes = table->node_count;
            for (k = 0; k < table->node_count; k++) {
                int length = (int)strlen(table->nodes[k].text);
                if (length > max_text) max_text = length;
                if (length >= MERCS_MSN_TEXT_BYTES) long_text++;
            }
            sim.player_table = t;
            for (pass = 0; pass < 2; pass++) {
                int result;
                if (pass) for (k = 0; k < table->node_count; k++) sim.logic.shown[t][k] = 1;
                assert(!msim_capture_result_star(&sim, &captured));
                result = mercs_msn_encode_selected(&captured, 1, 0, bytes, sizeof bytes, &written);
                if (result) fprintf(stderr, "%s table%d visibility-pass%d cannot serialize (%d)\n", r.name, t, pass, result);
                assert(!result && written == sizeof bytes);
            }
        }
        records += present;
        prj_record_free(&r);
    }
    prj_close(a);
    assert(records > 0 && long_text > 0);
    printf("PASS: authored archive return domain: %d records, %d tables, %d nodes, %d text strings >=32 bytes; max nodes %d, max text %d; initial and all-visible representation passes\n",
           records, tables, nodes, long_text, max_nodes, max_text);
}

int main(int argc, char **argv)
{
    writer_transport_cases();
    lifecycle_transport_cases();
    loader_and_activation();
    native_exit_and_career();
    rejection_is_atomic();
    aggregation_cases();
    original_arming_and_control();
    for (int i = 1; i < argc; i++) authored_result_domain(argv[i]);
    return 0;
}
