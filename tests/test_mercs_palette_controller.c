/* Captured original palette state/RGB gate; no game or rendering execution. */
#include "fx.h"
#include "original-palette-fixtures.h"
#include "original-shared-palette-fixtures.h"
#include <stdio.h>
#include <string.h>

static int checks, failures, submissions, phase_one_frames, shared_operations;
static uint8_t bank[FX_PALETTE_SLOTS][FX_PALETTE_COMPONENTS];
static int valid[FX_PALETTE_SLOTS];
static const char *case_name, *phase;
static int frame;

static void check(int good, const char *what)
{
    checks++;
    if (!good) {
        failures++;
        if (failures < 25) fprintf(stderr, "%s %s %d: %s\n", case_name, phase, frame, what);
    }
}

static void state(const fx_world *w, const original_palette_state *e)
{
    const fx_palette *p = &w->palette;
    check(p->current == e->current, "current");
    check(p->dirty == e->dirty, "dirty");
    check(p->want == e->want, "want");
    check(p->back_slot == e->back_slot, "back slot");
    check(w->palette_back == e->back, "canonical back counter");
    check(w->palette_left == e->left, "canonical left counter");
    check(p->frames == e->blend_frames, "blend frame count");
    check(p->step == e->blend_step, "blend step");
    check(p->buffer_present == e->buffer_present, "buffer lifetime");
}

static void shared_state(const fx_world *w, const original_shared_palette_state *e)
{
    original_palette_state base = {e->current, e->dirty, e->want, e->back_slot,
        e->back, e->left, e->blend_frames, e->blend_step, e->buffer_present};
    state(w, &base);
    check(w->palette.base == e->base_slot, "relative-effect base slot");
    check(w->palette.queued == e->queued_slot, "queued base slot");
    if (e->buffer_present) {
        check(!memcmp(w->palette.rgb, e->rgb6, FX_PALETTE_COMPONENTS), "current RGB6 buffer");
        check(!memcmp(w->palette.delta, e->delta, FX_PALETTE_COMPONENTS), "signed8 delta buffer");
        check(!memcmp(w->palette.remainder, e->remainder, sizeof w->palette.remainder), "signed16 remainder buffer");
    }
}

typedef struct {
    fx_world *world;
    const original_palette_frame *expected_frame;
    const original_shared_palette_operation *expected_operation;
    int count;
} observer;

static void submit(void *user, const uint8_t *rgb6)
{
    observer *o = user;
    phase = "at submission";
    o->count++; submissions++;
    if (o->expected_frame) {
        state(o->world, &o->expected_frame->at_submission);
        check(!memcmp(rgb6, o->expected_frame->rgb6, FX_PALETTE_COMPONENTS), "all 768 submitted RGB6 components");
    } else if (o->expected_operation && o->expected_operation->submission_count > 0) {
        shared_state(o->world, &o->expected_operation->at_submission);
        check(!memcmp(rgb6, o->expected_operation->submitted_rgb6, FX_PALETTE_COMPONENTS), "all 768 submitted RGB6 components");
    } else check(0, "unexpected palette submission");
}

static void cases_phase_one(void)
{
    size_t ci, fi;
    for (ci = 0; ci < ORIGINAL_PALETTE_CASES_COUNT; ci++) {
        const original_palette_case *c = &original_palette_cases[ci];
        fx_world world = {0}; observer o = {&world, NULL, NULL, 0};
        case_name = c->name; phase = "before producer"; frame = -1;
        check(fx_palette_bind(&world, bank, valid, c->initial_slot, submit, &o), "bind actual resource bank");
        world.palette_left = c->before.left; world.palette_back = c->before.back;
        state(&world, &c->before);
        fx_set_timing(c->profile == 0);
        check(fx_duration(22) * 2 == c->nuclear_duration_ticks, "actual profile nuclear duration");
        check(fx_palette_request(&world, 18, fx_duration(22) * 2, 2, c->raw_ticks) == !c->initially_busy,
              "mode2 admission");
        phase = "after producer"; state(&world, &c->after_producer);
        for (fi = 0; fi < c->frame_count; fi++) {
            frame = (int)fi; o.expected_frame = &c->frames[fi]; o.count = 0;
            fx_palette_step(&world, c->raw_ticks);
            phase = "after update";
            check(o.count == 1, "exactly one submission per captured update");
            state(&world, &c->frames[fi].after_update);
            phase_one_frames++;
        }
    }
}

static void cases_shared(void)
{
    size_t ci, oi;
    for (ci = 0; ci < ORIGINAL_SHARED_PALETTE_SEQUENCE_COUNT; ci++) {
        const original_shared_palette_sequence *c = &original_shared_palette_sequences[ci];
        fx_world world = {0}; observer o = {&world, NULL, NULL, 0};
        case_name = c->name; phase = "bind"; frame = -1;
        check(fx_palette_bind(&world, bank, valid, c->initial_slot, submit, &o), "bind actual resource bank");
        fx_palette_queue(&world, c->initial_base_slot);
        fx_set_timing(c->profile == 0);
        for (oi = 0; oi < c->operation_count; oi++) {
            const original_shared_palette_operation *op = &c->operations[oi];
            frame = (int)oi; phase = "before operation";
            shared_state(&world, &op->before);
            o.expected_operation = op; o.count = 0;
            switch (op->operation) {
            case 0: fx_palette_request(&world, 18, fx_duration(22) * 2, 2, op->raw_ticks); break;
            case 1: check(fx_palette_request(&world, op->slot, op->duration, op->mode, op->raw_ticks) == op->return_value,
                          "absolute begin admission result"); break;
            case 2: fx_palette_step(&world, op->raw_ticks); break;
            case 3: fx_palette_queue(&world, op->slot); break;
            case 4: fx_palette_flush(&world); break;
            default: check(0, "unknown original operation"); break;
            }
            phase = "after operation";
            check(o.count == op->submission_count, "submission count");
            shared_state(&world, &op->after);
            shared_operations++;
        }
    }
}

int main(void)
{
    size_t i;
    for (i = 0; i < ORIGINAL_PALETTE_RESOURCES_COUNT; i++) {
        const original_palette_resource *r = &original_palette_resources[i];
        memcpy(bank[r->slot], r->rgb6, FX_PALETTE_COMPONENTS); valid[r->slot] = 1;
    }
    cases_phase_one(); cases_shared();
    printf("{\"checks\":%d,\"failures\":%d,\"phase_one_cases\":%zu,\"phase_one_updates\":%d,"
           "\"shared_sequences\":%zu,\"shared_operations\":%d,\"submissions_compared\":%d,"
           "\"submitted_rgb6_components_compared\":%d,\"screen_or_gamma_claim\":false}\n",
           checks, failures, ORIGINAL_PALETTE_CASES_COUNT, phase_one_frames,
           ORIGINAL_SHARED_PALETTE_SEQUENCE_COUNT, shared_operations, submissions, submissions * 768);
    return failures ? 1 : 0;
}
