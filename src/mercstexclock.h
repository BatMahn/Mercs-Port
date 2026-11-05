/* Mercs texture-slot clock: DOS0x452d8 / retail Windows0x1005f2f6.
 * Slots sharing a frame group have independent clocks; objects that reference
 * the same slot observe the same clock, including resets by another object.
 * This header also serves animated scenery. Original instruction fixtures are
 * in tests/fixtures/fidelity-followup/. */
#ifndef MERCS_TEXCLOCK_H
#define MERCS_TEXCLOCK_H
#include <stdint.h>
#include <string.h>

typedef struct {
    int frame, rate, mode, state;
    int32_t stamp;
} mercs_texclock;

static inline void mercs_texclock_mode(mercs_texclock *c, int mode)
{
    if (c->state != -2) c->mode = (int16_t)mode;
    if (c->rate == 0) c->rate = 45;
}

static inline void mercs_texclock_reset(mercs_texclock *c, const int cel[32], unsigned frame)
{
    frame = (uint16_t)frame;
    if (frame > 31) frame = 0;
    if (c->state >= 0 && cel[frame] > 0) { c->frame = (int)frame; c->stamp = 0; }
}

static inline void mercs_texclock_step(mercs_texclock *c, const int cel[32], int32_t now)
{
    int32_t delta, steps;
    int frame, old, next[32], square[32], i;
    unsigned left;
    if (c->state < 0 || c->mode == 0 || (uint16_t)c->rate == 0) return;
    if (c->stamp == 0) c->stamp = now;
    delta = (int32_t)((uint32_t)now - (uint32_t)c->stamp);
    steps = delta / (int)(uint16_t)c->rate;
    if (steps <= 0) return;
    old = c->frame;
    frame = old & 31;
    for (i = 0; i < 32; ++i) {
        int n = (i + 1) & 31;
        next[i] = cel[n] > 0 ? n : 0;
    }
    /* Exact composition of the original 32-state transition avoids billions
     * of iterations after a long suspension, including non-contiguous runs. */
    left = (unsigned)steps;
    while (left) {
        if (left & 1u) frame = next[frame];
        left >>= 1;
        if (!left) break;
        for (i = 0; i < 32; ++i) square[i] = next[next[i]];
        memcpy(next, square, sizeof next);
    }
    if (c->mode == 2 && (unsigned)frame < (unsigned)old) { c->mode = 0; frame = 0; }
    c->frame = frame;
    c->stamp = (int32_t)((uint32_t)c->stamp + (uint32_t)(uint16_t)c->rate * (uint32_t)steps);
}
#endif
