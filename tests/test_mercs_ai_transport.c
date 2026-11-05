/* GPS -> world_actor -> msim's regular ai_view -> linked ai_step.
 * Observe the real caller without replacing its view construction or AI.
 * Two authored scenes, with controlled combat/mind/RNG preconditions; this
 * is a data-transport test, not an uninterrupted mission-playthrough test.
 */
#include "msim.h"
static void observe_ai_step(const ai_library *, ai_mind *, const ai_view *, float);
static void observe_missile_warning(ai_mind *, const ai_view *, int,
                                    int (*)(void *, int), void *);
#define ai_step observe_ai_step
#define ai_missile_warning observe_missile_warning
#include "../src/msim.c"
#undef ai_step
#undef ai_missile_warning
#include <assert.h>
#include "../tests/fixtures/ai-validation/mercs_ai_transport_cases.h"

static msim *observed_sim;
static int observed_actor, step_calls, missile_calls;
static ai_view delivered, missile_delivered;
static int input_state, result_forced, result_queue;
static unsigned result_rng;

static void observe_ai_step(const ai_library *lib, ai_mind *m,
                            const ai_view *view, float dt)
{
    int selected = observed_sim && m == &observed_sim->minds[observed_actor];
    if (selected) { delivered = *view; input_state = m->state; step_calls++; }
    ai_step(lib, m, view, dt); /* actual linked implementation, unchanged view */
    if (selected) {
        result_forced = m->forced_man;
        result_queue = m->queue5;
        result_rng = m->rng;
    }
}

static void observe_missile_warning(ai_mind *m, const ai_view *view, int level,
                                    int (*side)(void *, int), void *ctx)
{
    if (observed_sim && m == &observed_sim->minds[observed_actor]) {
        missile_delivered = *view;
        missile_calls++;
    }
    ai_missile_warning(m, view, level, side, ctx);
}

static int find_actor(const world_actor *actors, int count,
                       const mercs_ai_transport_fixture *f)
{
    int i, matching = 0;
    for (i = 0; i < count; i++)
        if (actors[i].table == f->table && strcmp(actors[i].name, f->name) == 0 &&
            strcasecmp(actors[i].skel, f->skeleton) == 0 && matching++ == f->ordinal)
            return i;
    return -1;
}

static void verify_authored_fields(prj_archive *archive, const bwd_mission *mission,
                                   const world_actor *actor,
                                   const mercs_ai_transport_fixture *f)
{
    int r, k, found = 0, matching = 0, class_found = 0, n;
    prj_record skeleton;
    bwd_chunk *chunks;
    for (r = 0; r < mission->record_count; r++) {
        const bwd_record *record = &mission->records[r];
        if (strcasecmp(record->name, f->record) != 0) continue;
        assert(!record->external); /* this is the authored archive entry */
        for (k = 0; k < record->chunk_count; k++) {
            const bwd_chunk *chunk = &record->chunks[k];
            bwd_gps gps;
            if (strcmp(chunk->tag, "GPS") || bwd_gps_decode(chunk, &gps)) continue;
            if (gps.table != f->table || strcmp(gps.name, f->name) ||
                strcasecmp(gps.skeleton, f->skeleton)) continue;
            if (matching++ != f->ordinal) continue;
            assert(gps.control == 2 && !gps.is_player);
            assert(chunk->size >= 20);
            /* Direct raw bytes cross-check the decoder as well as loader. */
            assert((unsigned)f->raw == (unsigned)(chunk->data[18] | chunk->data[19] << 8));
            assert((uint16_t)gps.attributes[5] == (unsigned)f->raw);
            assert(actor->ai_gps5 == (uint8_t)gps.attributes[5]);
            assert(actor->ai_level == gps.pilot_level && gps.pilot_level == f->level);
            assert(strcasecmp(actor->loadout, gps.loadout) == 0);
            found++;
        }
    }
    assert(found == 1);
    /* A skeleton class is different from the GPS objective-class word. */
    assert(prj_read_named(archive, "BWD", f->skeleton, &skeleton) == PRJ_OK);
    n = bwd_chunks(skeleton.data, skeleton.size, NULL, 0);
    assert(n > 0);
    chunks = calloc((size_t)n, sizeof *chunks); assert(chunks);
    assert(bwd_chunks(skeleton.data, skeleton.size, chunks, n) == n);
    for (k = 0; k < n; k++) if (strcmp(chunks[k].tag, "GP") == 0 && chunks[k].size >= 4) {
        int cls = (int16_t)(chunks[k].data[2] | chunks[k].data[3] << 8);
        assert(actor->unit_class == cls && cls == f->unit_class);
        class_found++;
    }
    assert(class_found == 1);
    free(chunks); prj_record_free(&skeleton);
}

static void run_fixture(prj_archive *archive, const mercs_ai_transport_fixture *f)
{
    world_actor *actors = NULL;
    bwd_mission mission;
    msim *sim = calloc(1, sizeof *sim);
    int count = 0, selected, i, k, target = -1;
    ai_mind *mind;
    assert(sim);
    assert(bwd_mission_load(archive, f->scene, &mission) == 0);
    assert(world3d_actors(archive, f->scene, &actors, &count) == 0);
    assert(count == (strcmp(f->scene, "MCC1SCN1") == 0 ? 8 : 7));
    selected = find_actor(actors, count, f); assert(selected >= 0);
    verify_authored_fields(archive, &mission, &actors[selected], f);
    assert(msim_init(sim, archive, f->scene, actors, count) == 0);
    if (!(sim->ai_ok && sim->minds && sim->units && sim->armed[selected]))
        fprintf(stderr, "%s/%s: ai_ok=%d, selected=%d, loadout=%s, armed=%d\n",
                f->scene, f->name, sim->ai_ok, selected, actors[selected].loadout, sim->armed[selected]);
    assert(sim->ai_ok && sim->minds && sim->units && sim->armed[selected]);
    assert(sim->units[selected].weapon_count > 0 && actors[selected].speed > 0);

    /* Isolate the reaction from mission orders and AIT transitions. Keep
     * the loaded actor's trait, level, class, speed, name, model and position.
     * Other combatants remain real loaded targets but do not run AI/fire.
     */
    memset(&sim->ai, 0, sizeof sim->ai);
    sim->logic.table_count = 0;
    sim->player_online_at = 0;
    for (i = 0; i < count; i++) {
        sim->seen_node[i] = -1;
        sim->online_at[i] = 0;
        sim->powered_down[i] = i != selected;
        sim->units[i].heat = 0;
        sim->units[i].jets = 0;
        sim->units[i].no_jump = 1;
        sim->units[i].ai_trigger = 0;
        for (k = 0; k < sim->units[i].weapon_count; k++) sim->units[i].weapons[k].state = -1;
        if (i != selected && actors[i].friendly != actors[selected].friendly && sim->armed[i])
            target = i;
    }
    assert(target >= 0);
    mind = &sim->minds[selected];
    ai_init(mind, AIP_DEFLT);
    mind->progs[1] = -1;
    mind->state = AI_ATTACK;
    mind->target = 1;
    mind->tgt = target;
    mind->man = mind->prev_man = 0;
    mind->forced_man = -1;
    mind->man_limit = mind->next_fire = 1000000;
    mind->rng = f->seed;

    observed_sim = sim; observed_actor = selected;
    step_calls = missile_calls = 0;
    memset(&delivered, 0, sizeof delivered);
    msim_step(sim, 50);
    assert(step_calls == 1 && input_state == AI_ATTACK);
    assert(delivered.has_enemy && delivered.moving && delivered.out_of_weapons);
    assert(!delivered.can_jump && delivered.heat == 0);
    assert(delivered.pilot_level == f->level && delivered.unit_class == f->unit_class);
    assert(delivered.charge_flee_threshold == f->threshold);
    assert(result_forced == f->forced && result_queue == f->queue && result_rng == f->final_rng);
    /* The transport fields remain the authored fields after the actual step. */
    assert(actors[selected].ai_gps5 == f->raw && actors[selected].ai_level == f->level);
    assert(actors[selected].unit_class == f->unit_class);

    /* Exercise the separate missile-warning view construction too. This
     * checks transport only; the exhausted-weapons consumer is regular AI.
     */
    missile_warning(sim, selected, 1);
    assert(missile_calls == 1);
    assert(missile_delivered.pilot_level == f->level);
    assert(missile_delivered.unit_class == f->unit_class);
    assert(missile_delivered.charge_flee_threshold == f->threshold);
    printf("PASS: %s/%s[%d] %s: raw=%u class=%d level=%d threshold=%d roll=%d forced=%d rng=%u; regular+missile views\n",
           f->scene, f->record, f->ordinal, f->name, (unsigned)actors[selected].ai_gps5,
           delivered.unit_class, delivered.pilot_level, delivered.charge_flee_threshold,
           f->roll, result_forced, result_rng);
    observed_sim = NULL;
    msim_free(sim); free(sim);
    for (i = 0; i < count; i++) mech3d_free(&actors[i].mech);
    free(actors); bwd_mission_free(&mission);
}

int main(int argc, char **argv)
{
    char error[256];
    prj_archive *archive;
    size_t i;
    if (argc != 3) {
        fprintf(stderr, "usage: %s MW2.PRJ game-data-root\n", argv[0]);
        return 2;
    }
    archive = prj_open(argv[1], error, sizeof error);
    if (!archive) { fprintf(stderr, "%s\n", error); return 1; }
    dp_clear_roots(); dp_add_root(argv[2]);
    for (i = 0; i < sizeof mercs_ai_transport_fixtures / sizeof mercs_ai_transport_fixtures[0]; i++)
        run_fixture(archive, &mercs_ai_transport_fixtures[i]);
    prj_close(archive); dp_clear_roots();
    printf("PASS: %zu native GPS -> actor -> msim -> AI fixtures across 2 authored scenes; 8 missile-view captures\n", i);
    return 0;
}
