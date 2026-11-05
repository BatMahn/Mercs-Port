#include "mercsreturn.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Defined arithmetic shift, including negative fixed-point inputs. */
static int32_t whole_q16(int32_t value)
{
    return value >= 0 ? value / 65536 :
           (int32_t)(-((-(int64_t)value + 65535) / 65536));
}

static int armor_divisor(const mercs_return_state *s)
{
    if (s->player || s->difficulty == 2) return 4;
    return s->alliance == 0 || s->difficulty == 1 ? 3 : 1;
}

int mercs_return_encode(mercs_return_state *state, uint8_t *out, size_t capacity,
                        size_t *written)
{
    mercs_return_state next;
    mek_def result;
    uint8_t bytes[MERCS_RETURN_MAX_BYTES];
    int i, l, k, observed, emitted, divisor;
    size_t length;
    if (written) *written = 0;
    if (!state || !out || !written) return MERCS_RETURN_INVALID;
    if (!(state->flags & 2)) return MERCS_RETURN_NOT_OWNED;
    if (state->difficulty < 0 || state->difficulty > 2 ||
        state->weapon_count < 0 || state->weapon_count > MERCS_RETURN_MOUNTS ||
        state->bin_count < 0 || state->bin_count > MERCS_RETURN_BINS ||
        state->pool_count < 0 || state->pool_count > MERCS_RETURN_POOLS)
        return MERCS_RETURN_INVALID;
    observed = 0;
    for (i = 0; i < MERCS_RETURN_MOUNTS; i++) if (state->mounts[i].weapon >= 0) observed++;
    if (observed < state->weapon_count) return MERCS_RETURN_INVALID;
    /* Validate every index before doing the original mutation or serializing. */
    emitted = 0;
    for (i = 0; i < state->pool_count; i++) {
        const mercs_return_pool *p = &state->pools[i];
        if (p->bin_count < 0 || p->bin_count > MERCS_RETURN_POOL_BINS ||
            p->current_bin < 0 || p->current_bin > p->bin_count)
            return MERCS_RETURN_INVALID;
        for (k = 0; k < p->bin_count; k++)
            if (p->bin_indices[k] < 0 || p->bin_indices[k] >= state->bin_count)
                return MERCS_RETURN_INVALID;
        if (p->rounds) emitted += p->bin_count - p->current_bin;
    }
    if (emitted > MERCS_RETURN_BINS) return MERCS_RETURN_INVALID;

    next = *state;
    memset(&result, 0, sizeof result);
    divisor = armor_divisor(&next);
    /* DOS 0x63817: scan all twelve slots, shift once, then advance. Adjacent
     * duplicate ammo IDs can therefore leave one occurrence. Preserve that
     * original iteration order rather than substituting a filter operation. */
    for (i = 0; i < next.bin_count; i++) if (next.bins[i].rounds <= 0) {
        int32_t id = next.bins[i].id;
        for (l = 0; l < 8; l++) for (k = 0; k < 12; k++)
            if ((int32_t)next.locations[l].slots[k] == id) {
                memmove(&next.locations[l].slots[k], &next.locations[l].slots[k + 1],
                        (size_t)(11 - k) * sizeof next.locations[l].slots[0]);
                next.locations[l].slots[11] = 0;
            }
    }

    result.tonnage = (uint32_t)next.tonnage;
    result.walk_mp = (uint32_t)(((int64_t)next.walk_speed_raw * 65536) / 0x697e98);
    result.jump_mp = (uint32_t)next.jump_mp;
    result.heat_sinking = read32(next.config + 34) ? 20u : 10u;
    for (l = 0; l < 8; l++) {
        const mercs_return_location *src = &next.locations[l];
        mek_loc *dst = &result.loc[l];
        int32_t internal = whole_q16(src->internal_q16);
        dst->slot_count = src->slot_count;
        dst->flags = (uint16_t)((src->flags & 0x0fff) | 0x0800);
        if (internal <= 0) continue;
        dst->internal = (uint32_t)internal;
        dst->armor = (uint32_t)(whole_q16(src->armor_q16) / divisor);
        dst->rear_armor = (uint32_t)(whole_q16(src->rear_q16) / divisor);
        for (k = 0; k < 12; k++) {
            dst->slots[k] = src->slots[k];
            if (src->slots[k] == 8000) result.heat_sinking++;
        }
    }
    /* Original loops over physical slots, counting active types even when
     * their independent destroyed flag prevents their emission. */
    observed = 0;
    for (i = 0; i < MERCS_RETURN_MOUNTS; i++) {
        const mercs_return_mount *m = &next.mounts[i];
        if (m->weapon >= 0) {
            if (!m->destroyed) {
                mek_item *dst = &result.items[result.item_count++];
                dst->id = m->item;
                dst->linked_id = (int32_t)(((uint32_t)m->group & 15u) << 8 |
                                          (uint32_t)m->location);
            }
            observed++;
        }
        if (observed >= next.weapon_count) break;
    }
    for (i = 0; i < next.pool_count; i++) {
        const mercs_return_pool *p = &next.pools[i];
        if (!p->rounds) continue;
        for (k = p->current_bin; k < p->bin_count; k++) {
            const mercs_return_bin *b = &next.bins[p->bin_indices[k]];
            mek_item *dst = &result.items[result.item_count + result.link_count++];
            dst->id = (uint32_t)(int32_t)b->id;
            dst->linked_id = (int32_t)b->weapon * 100;
        }
    }
    memcpy(result.config_raw, next.config, MERCS_RETURN_CONFIG);
    /* zeroed unknown16 and has_config_extension reproduce the original
     * dword ammo count and the exact 50-byte simulation trailer. */
    length = mek_write(&result, bytes, sizeof bytes);
    if (!length) return MERCS_RETURN_INVALID;
    if (capacity < length) return MERCS_RETURN_NO_SPACE;
    memcpy(out, bytes, length);
    *state = next;
    *written = length;
    return MERCS_RETURN_OK;
}

static int quantize_q16(float value, int32_t *out)
{
    double q = (double)value * 65536.0;
    if (!isfinite(q) || q < INT32_MIN || q > INT32_MAX) return -1;
    *out = (int32_t)q;
    return 0;
}

int mercs_return_capture(const combat_unit *unit, int owned, int difficulty,
                         int player, int alliance, mercs_return_state *out)
{
    mercs_return_state state;
    int i, j, l, reverse[MERCS_RETURN_BINS], pooled[MERCS_RETURN_BINS] = {0};
    if (!unit || !out) return MERCS_RETURN_INVALID;
    if (!owned) return MERCS_RETURN_NOT_OWNED;
    if (!unit->return_valid || difficulty < 0 || difficulty > 2 ||
        unit->weapon_count < 0 || unit->weapon_count > MERCS_RETURN_MOUNTS ||
        unit->bin_count < 0 || unit->bin_count > MERCS_RETURN_BINS ||
        unit->ammo_pool_count < 0 || unit->ammo_pool_count > MERCS_RETURN_POOLS ||
        unit->armor_mult != combat_armor_mult(player, alliance, difficulty))
        return MERCS_RETURN_INVALID;
    memset(&state, 0, sizeof state);
    state.flags = 2;
    state.difficulty = difficulty;
    state.player = player != 0;
    state.alliance = alliance;
    state.tonnage = unit->tons;
    state.walk_speed_raw = unit->return_walk_speed;
    state.jump_mp = unit->jets;
    memcpy(state.config, unit->return_config, sizeof state.config);
    for (l = 0; l < 8; l++) {
        mercs_return_location *dst = &state.locations[l];
        if (quantize_q16(unit->armor[l], &dst->armor_q16) ||
            quantize_q16(unit->rear[l], &dst->rear_q16) ||
            quantize_q16(unit->internal[l], &dst->internal_q16) ||
            unit->slot_count[l] < 0 || unit->slot_count[l] > MEK_SLOTS)
            return MERCS_RETURN_INVALID;
        dst->slot_count = (uint16_t)unit->slot_count[l];
        dst->flags = unit->return_flags[l];
        for (j = 0; j < 12; j++) {
            if (unit->slots[l][j] > UINT16_MAX) return MERCS_RETURN_INVALID;
            dst->slots[j] = (uint16_t)unit->slots[l][j];
        }
    }
    state.weapon_count = unit->weapon_count;
    for (i = 0; i < MERCS_RETURN_MOUNTS; i++) state.mounts[i].weapon = -1;
    for (i = 0; i < unit->weapon_count; i++) {
        const combat_weapon *cw = &unit->weapons[i];
        mercs_return_mount *dst = &state.mounts[i];
        if (cw->weapon < 0 || cw->weapon >= MEK_WEAPON_COUNT ||
            cw->ammo_pool < -1 || cw->ammo_pool >= unit->ammo_pool_count ||
            cw->bin_first < 0 || cw->bin_n < 0 || cw->bin_n > MERCS_RETURN_POOL_BINS || cw->bin_cur < 0 ||
            cw->bin_cur > cw->bin_n || cw->bin_first > unit->bin_count ||
            cw->bin_n > unit->bin_count - cw->bin_first)
            return MERCS_RETURN_INVALID;
        if (cw->ammo_pool >= 0) {
            const combat_ammo_pool *pool = &unit->ammo_pools[cw->ammo_pool];
            if (pool->weapon != cw->weapon || pool->rounds != cw->ammo ||
                pool->bin_first != cw->bin_first || pool->bin_n != cw->bin_n || pool->bin_cur != cw->bin_cur)
                return MERCS_RETURN_INVALID;
        } else {
            if (cw->bin_n || cw->bin_cur || (cw->ammo != 0 && cw->ammo != -1)) return MERCS_RETURN_INVALID;
            for (j = 0; j < unit->ammo_pool_count; j++)
                if (unit->ammo_pools[j].weapon == cw->weapon) return MERCS_RETURN_INVALID;
        }
        dst->weapon = cw->weapon;
        dst->group = cw->return_group;
        dst->location = cw->return_location;
        dst->destroyed = cw->destroyed;
        dst->item = cw->item;
    }
    state.bin_count = unit->bin_count;
    for (i = 0; i < MERCS_RETURN_BINS; i++) reverse[i] = -1;
    for (i = 0; i < unit->bin_count; i++) {
        const combat_bin *src;
        int native_index = unit->return_bin_order[i];
        if (native_index < 0 || native_index >= unit->bin_count || reverse[native_index] != -1)
            return MERCS_RETURN_INVALID;
        reverse[native_index] = i;
        src = &unit->bins[native_index];
        if (src->weapon < 0 || src->weapon >= MEK_WEAPON_COUNT ||
            src->pool < 0 || src->pool >= unit->ammo_pool_count ||
            src->id > INT16_MAX || src->rounds < INT16_MIN || src->rounds > INT16_MAX)
            return MERCS_RETURN_INVALID;
        state.bins[i].weapon = (uint16_t)src->weapon;
        state.bins[i].id = (int16_t)src->id;
        state.bins[i].rounds = (int16_t)src->rounds;
    }
    state.pool_count = unit->ammo_pool_count;
    for (i = 0; i < state.pool_count; i++) {
        const combat_ammo_pool *src = &unit->ammo_pools[i];
        mercs_return_pool *pool = &state.pools[i];
        if (src->weapon < 0 || src->weapon >= MEK_WEAPON_COUNT ||
            src->bin_first < 0 || src->bin_first > unit->bin_count ||
            src->bin_n <= 0 || src->bin_n > MERCS_RETURN_POOL_BINS ||
            src->bin_n > unit->bin_count - src->bin_first || src->bin_cur < 0 || src->bin_cur > src->bin_n)
            return MERCS_RETURN_INVALID;
        for (j = 0; j < i; j++)
            if (unit->ammo_pools[j].weapon == src->weapon) return MERCS_RETURN_INVALID;
        pool->rounds = src->rounds;
        pool->bin_count = src->bin_n;
        pool->current_bin = src->bin_cur;
        for (j = 0; j < pool->bin_count; j++) {
            int index = reverse[src->bin_first + j];
            if (index < 0 || state.bins[index].weapon != src->weapon ||
                unit->bins[src->bin_first + j].pool != i) return MERCS_RETURN_INVALID;
            if (pooled[index]++) return MERCS_RETURN_INVALID;
            pool->bin_indices[j] = index;
        }
    }
    for (i = 0; i < state.bin_count; i++) if (!pooled[i]) return MERCS_RETURN_INVALID;
    *out = state;
    return MERCS_RETURN_OK;
}

int mercs_return_unit_bytes(combat_unit *unit, int owned, int difficulty,
                            int player, int alliance, uint8_t *out,
                            size_t capacity, size_t *written)
{
    mercs_return_state state;
    int result, l, k;
    if (written) *written = 0;
    if (!out || !written) return MERCS_RETURN_INVALID;
    result = mercs_return_capture(unit, owned, difficulty, player, alliance, &state);
    if (result != MERCS_RETURN_OK) return result;
    result = mercs_return_encode(&state, out, capacity, written);
    if (result != MERCS_RETURN_OK) return result;
    for (l = 0; l < 8; l++) for (k = 0; k < 12; k++) unit->slots[l][k] = state.locations[l].slots[k];
    return MERCS_RETURN_OK;
}
