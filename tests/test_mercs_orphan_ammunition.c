/* Full native load/action/return comparisons against independently executed
 * DOS/Windows instructions. Temporary MEKs are isolated, never game assets. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mercsreturn.h"
#include "datapath.h"
#include "../src/combat.c"
#include "fixtures/mercs_orphan_ammunition_original.h"

static int nibble(char c)
{ return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }

static void load_case(combat_unit *u, const mercs_orphan_ammunition_fixture *f, const char *root)
{
    char path[512];
    size_t i;
    FILE *file;
    snprintf(path, sizeof path, "%s/MEK/ORP00USR.MEK", root);
    file = fopen(path, "wb"); assert(file);
    for (i = 0; f->mek_hex[i]; i += 2) {
        int a = nibble(f->mek_hex[i]), b = nibble(f->mek_hex[i + 1]);
        assert(a >= 0 && b >= 0); assert(fputc(a * 16 + b, file) != EOF);
    }
    assert(!fclose(file));
    assert(!combat_init(u, NULL, "ORP00USR"));
    combat_set_armor_mult(u, combat_armor_mult(1, 0, 1));
    u->human = 1;
    assert(u->return_valid);
}

static void check_capture(const combat_unit *u, const mercs_return_state *want)
{
    mercs_return_state got;
    int i, k, total = 0;
    assert(!mercs_return_capture(u, 1, 1, 1, 0, &got));
    assert(got.flags == want->flags && got.difficulty == want->difficulty && got.player == want->player);
    assert(got.alliance == want->alliance && got.tonnage == want->tonnage);
    assert(got.walk_speed_raw == want->walk_speed_raw && got.jump_mp == want->jump_mp);
    assert(!memcmp(got.config, want->config, sizeof got.config));
    assert(got.weapon_count == want->weapon_count && got.bin_count == want->bin_count && got.pool_count == want->pool_count);
    for (i = 0; i < 8; i++) {
        const mercs_return_location *a = &got.locations[i], *b = &want->locations[i];
        assert(a->armor_q16 == b->armor_q16 && a->rear_q16 == b->rear_q16 && a->internal_q16 == b->internal_q16);
        assert(a->slot_count == b->slot_count && a->flags == b->flags);
        assert(!memcmp(a->slots, b->slots, sizeof a->slots));
    }
    for (i = 0; i < got.weapon_count; i++) {
        const mercs_return_mount *a = &got.mounts[i], *b = &want->mounts[i];
        assert(a->weapon == b->weapon && a->group == b->group && a->location == b->location);
        assert(a->destroyed == b->destroyed && a->item == b->item);
    }
    for (i = 0; i < got.bin_count; i++) {
        assert(got.bins[i].weapon == want->bins[i].weapon);
        assert(got.bins[i].rounds == want->bins[i].rounds && got.bins[i].id == want->bins[i].id);
    }
    for (i = 0; i < got.pool_count; i++) {
        const mercs_return_pool *a = &got.pools[i], *b = &want->pools[i];
        assert(a->rounds == b->rounds && a->bin_count == b->bin_count && a->current_bin == b->current_bin);
        for (k = 0; k < a->bin_count; k++) assert(a->bin_indices[k] == b->bin_indices[k]);
        if (b->rounds > 0) total += b->rounds;
    }
    assert(combat_ammo_total(u) == total);
}

static void original_cases(const char *root)
{
    size_t n;
    for (n = 0; n < sizeof mercs_orphan_ammunition_fixtures / sizeof mercs_orphan_ammunition_fixtures[0]; n++) {
        const mercs_orphan_ammunition_fixture *f = &mercs_orphan_ammunition_fixtures[n];
        combat_unit u;
        uint8_t bytes[MERCS_RETURN_MAX_BYTES];
        size_t written = 0;
        int i, l, k;
        printf("case %s\n", f->name); fflush(stdout);
        load_case(&u, f, root);
        check_capture(&u, &f->before);
        for (i = 0; i < u.bin_count; i++) assert(u.bins[u.return_bin_order[i]].critical_pool == f->critical_pools[i]);
        for (i = 0; i < u.weapon_count; i++) assert(u.weapons[i].state == f->before_states[i]);
        if (f->action == 1) crit_item(&u, 2, 0, 0);
        if (f->action == 3 || f->action == 4) assert(combat_jettison(&u, 0) == f->action_return);
        if (f->action == 2 || f->action == 4) cook_off(&u);
        assert(u.destroyed == f->destroyed);
        check_capture(&u, &f->after);
        for (i = 0; i < u.weapon_count; i++) assert(u.weapons[i].state == f->after_states[i]);
        assert(!mercs_return_unit_bytes(&u, 1, 1, 1, 0, bytes, sizeof bytes, &written));
        assert(written == f->expected_size && !memcmp(bytes, f->expected, written));
        for (l = 0; l < 8; l++) for (k = 0; k < 12; k++) assert(u.slots[l][k] == f->post_slots[l][k]);
    }
    printf("PASS: %zu native loader/action/complete-return cases match both original engines, including the actual MCC3 deployment\n", n);
}

static void rejects_inconsistent_pools(const char *root)
{
    combat_unit base, u, before;
    uint8_t bytes[MERCS_RETURN_MAX_BYTES], previous[MERCS_RETURN_MAX_BYTES];
    size_t written;
    int which;
    load_case(&base, &mercs_orphan_ammunition_fixtures[0], root);
    memset(previous, 0xa7, sizeof previous);
    for (which = 0; which < 8; which++) {
        u = base;
        if (which == 0) u.bins[1].pool = -1;
        if (which == 1) u.ammo_pools[1].weapon = u.ammo_pools[0].weapon;
        if (which == 2) u.ammo_pools[1].bin_n = 0;
        if (which == 3) u.ammo_pools[1].bin_first = u.bin_count;
        if (which == 4) u.weapons[1].ammo = 5; /* Energy mount, no matching pool. */
        if (which == 5) u.weapons[0].ammo++;
        if (which == 6) u.bins[1].weapon = 37;
        if (which == 7) u.ammo_pool_count = 0;
        before = u; memcpy(bytes, previous, sizeof bytes); written = 99;
        assert(mercs_return_unit_bytes(&u, 1, 1, 1, 0, bytes, sizeof bytes, &written) == MERCS_RETURN_INVALID);
        assert(!written && !memcmp(&u, &before, sizeof u) && !memcmp(bytes, previous, sizeof bytes));
    }
    puts("PASS: eight incomplete/inconsistent pool captures reject atomically");
}

static void oversized_links(const char *root)
{
    struct { uint32_t before[4]; combat_unit unit; uint32_t after[4]; } guarded;
    mek_def definition;
    uint8_t bytes[2048];
    char path[512];
    size_t length;
    int i;
    FILE *f;
    load_case(&guarded.unit, &mercs_orphan_ammunition_fixtures[1], root);
    assert(!mek_load(NULL, "ORP00USR", &definition));
    assert(definition.item_count == 1);
    definition.link_count = 26;
    for (i = 0; i < 26; i++) {
        definition.items[1 + i].id = (uint32_t)(14601 + i);
        definition.items[1 + i].linked_id = i ? 4601 : -1;
    }
    length = mek_write(&definition, bytes, sizeof bytes); assert(length);
    snprintf(path, sizeof path, "%s/MEK/ORP00USR.MEK", root);
    f = fopen(path, "wb"); assert(f); assert(fwrite(bytes, 1, length, f) == length); assert(!fclose(f));
    for (i = 0; i < 4; i++) guarded.before[i] = guarded.after[i] = 0x91a2b3c4u;
    assert(!combat_init(&guarded.unit, NULL, "ORP00USR"));
    assert(!guarded.unit.return_valid && guarded.unit.bin_count == 24 && guarded.unit.ammo_pool_count == 1);
    for (i = 0; i < 4; i++) assert(guarded.before[i] == 0x91a2b3c4u && guarded.after[i] == 0x91a2b3c4u);
    /* return_bin_order[25] would corrupt the following locomotion field even
     * before crossing the enclosing struct sentinel. Keep both checks. */
    assert(guarded.unit.loc_gone[0] == 0);
    puts("PASS: skipped invalid link plus25 valid links stays bounded and rejects incomplete return");
}

int main(void)
{
    char root[] = "/tmp/mercs-orphan-ammunition-XXXXXX", path[512];
    combat_planet planet = {1, 20, 50000, 0};
    assert(mkdtemp(root)); snprintf(path, sizeof path, "%s/MEK", root); assert(!mkdir(path, 0700));
    dp_clear_roots(); assert(!dp_add_root(root));
    combat_set_planet(&planet); combat_set_weapon_profile(COMBAT_PROFILE_DOS); combat_damage_live = 1;
    original_cases(root); rejects_inconsistent_pools(root); oversized_links(root);
    snprintf(path, sizeof path, "%s/MEK/ORP00USR.MEK", root); assert(!unlink(path));
    snprintf(path, sizeof path, "%s/MEK", root); assert(!rmdir(path)); assert(!rmdir(root));
    dp_clear_roots();
    return 0;
}
