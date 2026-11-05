/* Expected bytes and post-serialization critical slots come only from
 * executing original Mercs DOS and Windows code, independently of this C. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "mercsreturn.h"
#include "../src/combat.c"
#include "../tests/fixtures/result-audit/return_adapter_original_fixtures.h"
#include "../tests/fixtures/result-audit/return_edition_stock_fixtures.h"

/* Call the actual loader helpers with independently executed original inputs.
 * Full combat_init transport is checked again on the four authored loadouts. */
static void original_init_cases(void)
{
    size_t i;
    for (i = 0; i < MERCS_RETURN_FLAG_INIT_FIXTURE_COUNT; i++) {
        const mercs_return_flag_init_fixture *f = &mercs_return_flag_init_fixtures[i];
        mek_loc loc;
        memset(&loc, 0, sizeof loc);
        loc.armor = (uint32_t)f->armor;
        loc.rear_armor = (uint32_t)f->rear;
        loc.internal = (uint32_t)f->internal;
        loc.flags = f->input_flags;
        assert(return_location_flags(&loc) == f->expected_flags);
    }
    for (i = 0; i < MERCS_RETURN_WALK_INIT_FIXTURE_COUNT; i++) {
        const mercs_return_walk_init_fixture *f = &mercs_return_walk_init_fixtures[i];
        assert(return_walk_raw(f->walk_mp) == f->expected_raw);
    }
    printf("PASS: %zu original location-flag and %zu original raw-walk initialization cases\n",
           MERCS_RETURN_FLAG_INIT_FIXTURE_COUNT, MERCS_RETURN_WALK_INIT_FIXTURE_COUNT);
}

static void compare(const mercs_return_original_fixture *f, const uint8_t *got, size_t length)
{
    size_t i;
    if (length != f->expected_size) {
        fprintf(stderr, "%s: length %zu, original %zu\n", f->name, length, f->expected_size);
        assert(length == f->expected_size);
    }
    for (i = 0; i < length; i++) if (got[i] != f->expected[i]) {
        fprintf(stderr, "%s: byte %zu native%02x original%02x\n", f->name, i, got[i], f->expected[i]);
        assert(got[i] == f->expected[i]);
    }
}

static void original_encode_cases(void)
{
    size_t i, l;
    for (i = 0; i < sizeof mercs_return_original_fixtures / sizeof mercs_return_original_fixtures[0]; i++) {
        const mercs_return_original_fixture *f = &mercs_return_original_fixtures[i];
        mercs_return_state state = f->input;
        uint8_t bytes[MERCS_RETURN_MAX_BYTES + 8];
        size_t length = 0;
        int rc;
        memset(bytes, 0xa5, sizeof bytes);
        rc = mercs_return_encode(&state, bytes, MERCS_RETURN_MAX_BYTES, &length);
        assert(rc == (f->original_return ? MERCS_RETURN_OK : MERCS_RETURN_NOT_OWNED));
        compare(f, bytes, length);
        for (l = 0; l < 8; l++) {
            assert(!memcmp(state.locations[l].slots, f->post_slots[l], sizeof f->post_slots[l]));
            assert(state.locations[l].slot_count == f->input.locations[l].slot_count);
        }
        for (l = length; l < sizeof bytes; l++) assert(bytes[l] == 0xa5);
    }
    printf("PASS: %zu complete returned-MEK byte streams and depleted-slot side effects match both originals\n", i);
}

/* Construct a native representation from declared ORIGINAL INPUT snapshots,
 * not from expected output bytes. Shared-pool cases use a native-representable
 * contiguous pool with two mounts; other oracle cases deliberately include
 * layouts not present in native combat and exercise the encoder directly. */
static void native_unit(const mercs_return_state *s, combat_unit *u)
{
    int i, l, k, next_bin = 0;
    memset(u, 0, sizeof *u);
    u->return_valid = 1;
    u->tons = s->tonnage;
    u->return_walk_speed = s->walk_speed_raw;
    u->jets = s->jump_mp;
    u->armor_mult = combat_armor_mult(s->player, s->alliance, s->difficulty);
    memcpy(u->return_config, s->config, sizeof u->return_config);
    u->weapon_count = s->weapon_count;
    u->bin_count = s->bin_count;
    u->ammo_pool_count = s->pool_count;
    for (i = 0; i < u->bin_count; i++) u->return_bin_order[i] = -1;
    for (i = 0; i < u->ammo_pool_count; i++) {
        const mercs_return_pool *src = &s->pools[i];
        combat_ammo_pool *pool = &u->ammo_pools[i];
        assert(src->bin_count > 0);
        pool->weapon = s->bins[src->bin_indices[0]].weapon;
        pool->rounds = src->rounds; pool->bin_first = next_bin;
        pool->bin_n = src->bin_count; pool->bin_cur = src->current_bin;
        for (k = 0; k < src->bin_count; k++) {
            int original = src->bin_indices[k];
            assert(next_bin < u->bin_count && u->return_bin_order[original] == -1);
            u->return_bin_order[original] = next_bin;
            u->bins[next_bin].id = (uint32_t)(int32_t)s->bins[original].id;
            u->bins[next_bin].rounds = s->bins[original].rounds;
            u->bins[next_bin].pool = i;
            u->bins[next_bin++].weapon = pool->weapon;
        }
    }
    for (l = 0; l < 8; l++) {
        u->armor[l] = (float)s->locations[l].armor_q16 / 65536.0f;
        u->rear[l] = (float)s->locations[l].rear_q16 / 65536.0f;
        u->internal[l] = (float)s->locations[l].internal_q16 / 65536.0f;
        u->slot_count[l] = s->locations[l].slot_count;
        u->return_flags[l] = s->locations[l].flags;
        for (k = 0; k < 12; k++) u->slots[l][k] = s->locations[l].slots[k];
    }
    for (i = 0; i < u->weapon_count; i++) {
        combat_weapon *cw = &u->weapons[i];
        const mercs_return_mount *m = &s->mounts[i];
        cw->weapon = m->weapon;
        cw->return_group = m->group;
        cw->return_location = m->location;
        cw->item = m->item;
        cw->destroyed = m->destroyed;
        cw->ammo_pool = -1;
        cw->ammo = -1;
        for (k = 0; k < u->ammo_pool_count; k++) {
            const combat_ammo_pool *pool = &u->ammo_pools[k];
            if (pool->weapon != m->weapon) continue;
            cw->ammo_pool = k;
            cw->ammo = pool->rounds;
            cw->bin_first = pool->bin_first;
            cw->bin_n = pool->bin_n;
            cw->bin_cur = pool->bin_cur;
            break;
        }
        cw->state = cw->destroyed || cw->ammo == 0 ? -1 : CW_READY;
    }
    assert(next_bin == u->bin_count);
}

static void native_capture_cases(void)
{
    size_t i, count = 0;
    for (i = 0; i < sizeof mercs_return_original_fixtures / sizeof mercs_return_original_fixtures[0]; i++) {
        const mercs_return_original_fixture *f = &mercs_return_original_fixtures[i];
        combat_unit u, before;
        uint8_t bytes[MERCS_RETURN_MAX_BYTES];
        size_t length;
        int l, k;
        if (strncmp(f->name, "shared-pool-", 12) && strcmp(f->name, "native-pool-order-current-bin")) continue;
        native_unit(&f->input, &u);
        before = u;
        assert(mercs_return_unit_bytes(&u, 1, f->input.difficulty, f->input.player,
                                       f->input.alliance, bytes, sizeof bytes, &length) == MERCS_RETURN_OK);
        compare(f, bytes, length);
        for (l = 0; l < 8; l++) for (k = 0; k < 12; k++) assert(u.slots[l][k] == f->post_slots[l][k]);
        /* Depleted-but-undestroyed mounts stay in the returned equipment;
         * destroyed mounts do not erase a separately live ammo pool. */
        for (l = 0; l < u.weapon_count; l++) {
            assert(u.weapons[l].destroyed == before.weapons[l].destroyed);
            assert(u.weapons[l].ammo == before.weapons[l].ammo);
        }
        count++;
    }
    assert(count == 4);
    puts("PASS: four native capture cases match complete original bytes, including shared pools, destruction and ammo order/cursor");
}

static void rejection_cases(void)
{
    mercs_return_state original = mercs_return_original_fixtures[0].input, state;
    uint8_t out[MERCS_RETURN_MAX_BYTES], before[MERCS_RETURN_MAX_BYTES];
    size_t length = 99;
    memset(before, 0x67, sizeof before);
    memcpy(out, before, sizeof out);
    state = original;
    assert(mercs_return_encode(&state, out, 1, &length) == MERCS_RETURN_NO_SPACE);
    assert(!length && !memcmp(&state, &original, sizeof state) && !memcmp(out, before, sizeof out));
    state.pools[0].bin_indices[0] = MERCS_RETURN_BINS;
    original = state;
    assert(mercs_return_encode(&state, out, sizeof out, &length) == MERCS_RETURN_INVALID);
    assert(!length && !memcmp(&state, &original, sizeof state) && !memcmp(out, before, sizeof out));
    state = mercs_return_original_fixtures[0].input;
    state.pools[0].bin_count = MERCS_RETURN_POOL_BINS + 1;
    original = state;
    assert(mercs_return_encode(&state, out, sizeof out, &length) == MERCS_RETURN_INVALID);
    assert(!length && !memcmp(&state, &original, sizeof state) && !memcmp(out, before, sizeof out));
    state = mercs_return_original_fixtures[0].input;
    state.flags = 0;
    original = state;
    assert(mercs_return_encode(&state, out, sizeof out, &length) == MERCS_RETURN_NOT_OWNED);
    assert(!length && !memcmp(&state, &original, sizeof state) && !memcmp(out, before, sizeof out));
    state = mercs_return_original_fixtures[0].input;
    state.weapon_count = MERCS_RETURN_MOUNTS;
    original = state;
    assert(mercs_return_encode(&state, out, sizeof out, &length) == MERCS_RETURN_INVALID);
    assert(!length && !memcmp(&state, &original, sizeof state) && !memcmp(out, before, sizeof out));
    puts("PASS: insufficient space, invalid indices, oversized pools and non-owned controllers leave state/output untouched");
}

static void native_rejection_cases(void)
{
    size_t i;
    for (i = 0; i < MERCS_RETURN_ORIGINAL_FIXTURE_COUNT; i++) {
        const mercs_return_original_fixture *f = &mercs_return_original_fixtures[i];
        combat_unit base, unit, before;
        uint8_t bytes[MERCS_RETURN_MAX_BYTES], previous[MERCS_RETURN_MAX_BYTES];
        size_t length;
        int which;
        if (strcmp(f->name, "shared-pool-exhausted")) continue;
        native_unit(&f->input, &base);
        memset(previous, 0x67, sizeof previous);
        for (which = 0; which < 6; which++) {
            int want = MERCS_RETURN_INVALID;
            size_t capacity = sizeof bytes;
            unit = base;
            if (which == 0) { capacity = 1; want = MERCS_RETURN_NO_SPACE; }
            if (which == 1) unit.ammo_pool_count = 0;
            if (which == 2) unit.return_bin_order[1] = unit.return_bin_order[0];
            if (which == 3) unit.armor[0] = NAN;
            if (which == 4) unit.weapons[0].bin_n = MERCS_RETURN_POOL_BINS + 1;
            if (which == 5) {
                unit.weapons[0].bin_n = unit.weapons[1].bin_n = 1;
                unit.weapons[0].bin_cur = unit.weapons[1].bin_cur = 1;
            }
            before = unit;
            memcpy(bytes, previous, sizeof bytes);
            length = 99;
            assert(mercs_return_unit_bytes(&unit, 1, f->input.difficulty, f->input.player,
                                           f->input.alliance, bytes, capacity, &length) == want);
            assert(!length && !memcmp(&unit, &before, sizeof unit) && !memcmp(bytes, previous, sizeof bytes));
        }
        puts("PASS: six invalid/incomplete native captures preserve the entire unit and output, including pending depleted-slot edits");
        return;
    }
    assert(!"missing shared-pool rejection fixture");
}

static void actual_loadouts(const char *archive, const char *edition)
{
    const mercs_return_stock_init_fixture *stock = mercs_return_stock_init_fixtures;
    combat_planet planet = {1.0f, 0, 100000.0f, 0};
    prj_archive *a;
    char error[256];
    int i, l;
    if (!strcmp(edition, "3dfx")) stock = mercs_return_stock_3dfx_init_fixtures;
    else if (!strcmp(edition, "direct3d")) stock = mercs_return_stock_direct3d_init_fixtures;
    else assert(!strcmp(edition, "dos"));
    a = prj_open(archive, error, sizeof error);
    assert(a);
    combat_set_planet(&planet);
    for (i = 0; i < 4; i++) {
        const mercs_return_stock_init_fixture *f = &stock[i];
        combat_unit u;
        mek_def input, returned;
        mercs_return_state captured;
        uint8_t bytes[MERCS_RETURN_MAX_BYTES];
        size_t length;
        assert(mek_load(a, f->name, &input) == 0);
        assert(combat_init(&u, a, f->name) == 0 && u.return_valid);
        combat_set_armor_mult(&u, combat_armor_mult(1, 0, 1));
        assert(mercs_return_capture(&u, 1, 1, 1, 0, &captured) == MERCS_RETURN_OK);
        assert(!memcmp(captured.config, f->expected_config, sizeof captured.config));
        assert(captured.walk_speed_raw == f->expected_raw);
        for (l = 0; l < 8; l++) assert(captured.locations[l].flags == f->expected_flags[l]);
        /* Actual current damage, not the stock armor, reaches the encoder. */
        u.armor[MEK_HEAD] = 7.75f;
        u.internal[MEK_LA] = 0;
        u.speed_gain = 0.4f; /* actuator factor is not the retained base walk field */
        assert(mercs_return_unit_bytes(&u, 1, 1, 1, 0, bytes, sizeof bytes, &length) == MERCS_RETURN_OK);
        assert(mek_parse(bytes, length, &returned) == 0);
        assert(returned.loc[MEK_HEAD].armor == 1 && returned.loc[MEK_LA].internal == 0);
        for (l = 0; l < 12; l++) assert(returned.loc[MEK_LA].slots[l] == 0);
        assert(!returned.has_config_extension && !memcmp(returned.config_raw, f->expected_config, 50));
        printf("%s: current damage captured, %d mounts, %d returned bins, 50-byte trailer\n",
               f->name, returned.item_count, returned.link_count);
        u.return_valid = 0;
        assert(mercs_return_capture(&u, 1, 1, 1, 0, &captured) == MERCS_RETURN_INVALID);
    }
    prj_close(a);
}

int main(int argc, char **argv)
{
    assert(argc >= 1 && argc <= 3);
    original_init_cases();
    original_encode_cases();
    native_capture_cases();
    rejection_cases();
    native_rejection_cases();
    if (argc >= 2) actual_loadouts(argv[1], argc == 3 ? argv[2] : "dos");
    return 0;
}
