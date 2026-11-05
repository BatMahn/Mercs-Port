/* Execute native order adapters against original DOS branch fixtures, then
 * test the actual target, wake and return-nav consumers. */
#include "../src/msim.c"
#include <assert.h>
#include "../tests/fixtures/combat-completion/original-orders.h"

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
static int messages;
static void message(void *p, const char *text, int voice) { (void)p; (void)text; (void)voice; messages++; }
static msim *scene(void)
{
    int i;
    msim *s = calloc(1, sizeof *s); assert(s);
    s->actor_count = 4; s->player_table = 2; s->rng = 1;
#define ALLOC(field) s->field = calloc(4, sizeof *s->field); assert(s->field)
    ALLOC(actors); ALLOC(units); ALLOC(armed); ALLOC(minds); ALLOC(cmd); ALLOC(cmd_target);
    ALLOC(engage); ALLOC(mate_target); ALLOC(defend_leg); ALLOC(powered_down); ALLOC(online_at);
    ALLOC(ai_focus); ALLOC(height); ALLOC(bld);
#undef ALLOC
    s->bld_count = 4; s->nav_count = 5; s->player_nav = 0;
    s->on_message = message; messages = 0;
    s->ai_ok = 1; s->ai.count = 2;
    s->ai.progs[1].state_count = 1; s->ai.progs[1].states[0] = -1; /* any state; program0 rejects all */
    for (i = 0; i < 4; i++) {
        s->actors[i].table = i < 2 ? 2 : 1; s->actors[i].friendly = i < 2; s->actors[i].unit_class = 1;
        s->actors[i].mech.origin[0] = 2000 * (i + 1); s->actors[i].speed = 50;
        s->armed[i] = 1; s->units[i].armor_mult = 1; s->units[i].internal[1] = 100;
        ai_init(&s->minds[i], -1); s->minds[i].state = AI_FOLLOW; s->minds[i].progs[0] = 1;
        s->minds[i].lock_bits = 3; s->cmd_target[i] = s->mate_target[i] = -1; s->ai_focus[i] = -2;
        s->bld[i].hp = 100; s->bld[i].mn[0] = s->bld[i].mx[0] = 50000.0f;
    }
    for (i = 0; i < 6; i++) s->pilot_voice[i] = 0;
    for (i = 0; i < 8; i++) s->last_cmd[i] = 7;
    return s;
}
static void done(msim *s)
{
    free(s->actors); free(s->units); free(s->armed); free(s->minds); free(s->cmd); free(s->cmd_target);
    free(s->engage); free(s->mate_target); free(s->defend_leg); free(s->powered_down); free(s->online_at);
    free(s->ai_focus); free(s->height); free(s->bld); free(s);
}
static int native_target(int original)
{
    if (original & 0x200) return (original & 255) - 1;
    if (original & 0x400) return MSIM_TGT_THING(original & 255);
    if (original & 0x100) return MSIM_TGT_NAV(original & 255);
    return -1;
}
static void fixtures(void)
{
    int i, a;
    for (i = 0; i < COUNT(original_order_build); i++) {
        const original_order_build_case *f = &original_order_build[i];
        msim *s = scene();
        if (f->point == 4) { s->actors[2].friendly = 1; s->actors[2].table = 2; s->actors[2].unit_class = 2; }
        if (f->recipient == -1) for (a = 0; a < 4; a++) s->actors[a].friendly = 0;
        assert(msim_command(s, f->point, f->order, native_target(f->selected)) == f->result);
        assert(messages == (f->message >= 0));
        done(s);
    }
    for (i = 0; i < COUNT(original_order_consume); i++) {
        const original_order_consume_case *f = &original_order_consume[i];
        msim *s = scene();
        for (a = 0; a < 4; a++) {
            int unit = a + 1;
            s->actors[a].friendly = unit < f->count; s->actors[a].table = unit < f->count ? 2 : 1;
            s->actors[a].unit_class = unit < f->count && f->points[unit] == 4 ? 2 : 1;
            s->units[a].destroyed = (f->dead >> unit) & 1;
            s->minds[a].state = s->units[a].destroyed ? AI_DEAD : AI_FOLLOW;
            s->minds[a].progs[0] = ((f->denied >> unit) & 1) ? 0 : 1;
            s->engage[a] = f->engage;
        }
        assert(msim_command(s, f->selector == 0x4200 ? 0 : 4, f->order, native_target(f->target)) == 1);
        for (a = 0; a < f->count - 1; a++) {
            int accepted = f->flags[a + 1] != 0x43;
            assert(s->minds[a].state == f->state[a + 1]);
            assert(s->engage[a] == f->after_engage[a + 1]);
            assert(s->cmd[a] == (accepted && f->order != MSIM_CMD_ENGAGE ? f->order : 0));
            if (f->order == MSIM_CMD_ENGAGE) assert(s->minds[a].lock_bits == (accepted ? 0 : 3));
        }
        done(s);
    }
    puts("PASS: 64 original command construction and recipient-gate fixtures (including AIT, support slot and sticky group skip)");
}
static void targets_and_wake(void)
{
    msim *s = scene(); float x, z, y, facing, height; int who;
    assert(msim_resolve_point(s, 0) == 0);
    s->units[0].destroyed = 1; assert(msim_resolve_point(s, 0) == 2 && msim_resolve_point(s, 1) == -1);
    assert(msim_select_point(s, 0) == 2 && messages == 1);
    s->units[0].destroyed = 0;
    assert(msim_command(s, 1, MSIM_CMD_SHUTDOWN, -1) == 1 && s->powered_down[0]);
    assert(msim_command(s, 1, MSIM_CMD_JOIN, -1) == 1 && !s->powered_down[0] && s->online_at[0] > 0);
    assert(msim_command(s, 1, MSIM_CMD_ATTACK, MSIM_TGT_THING(0)) == 1);
    /* An old attacker reaction cannot override the explicit new order. */
    s->ai_focus[0] = 2;
    assert(pick_enemy(s, 0, &x, &z, &y, &facing, &height, &who) == 1 && who == -3 && x == 50000);
    assert(s->minds[0].node_tgt == 0x40000 && s->minds[0].tgt == -2);
    s->bld[0].flags |= 4; assert(command_target_destroyed(s, MSIM_TGT_THING(0))); s->bld[0].flags = 0;
    assert(msim_command(s, 1, MSIM_CMD_DEFEND, MSIM_TGT_THING(0)) == 1);
    assert(s->minds[0].guard_n >= 4 && s->minds[0].guard[0][0] == 25000);
    s->minds[0].guard_n = 0; s->navs[4].x = 777; s->navs[4].radius = 400;
    assert(msim_command(s, 1, MSIM_CMD_DEFEND, MSIM_TGT_NAV(4)) == 1);
    assert(s->minds[0].guard[0][0] == 377 && s->minds[0].counter == 0);
    assert(msim_command(s, 1, MSIM_CMD_ATTACK, MSIM_TGT_NAV(4)) == 0);
    assert(s->last_cmd[1] == 2); /* original wrapper changes menu even for rejected target */
    puts("PASS: selection, shutdown/restart, exact commanded target priority, structure attack and structure/nav defense");
    done(s);
}
static void return_nav(void)
{
    msim *s = scene(); mtbl_table *t = &s->logic.tables[2];
    s->logic.table_count = 3; t->node_count = 3;
    t->nodes[0].kind = 0x100; strcpy(t->nodes[0].target, "FIRST");
    t->nodes[1].kind = 0x20; strcpy(t->nodes[1].target, "RETURN");
    t->nodes[2].kind = 0x20; strcpy(t->nodes[2].target, "LATER");
    strcpy(s->navs[0].name, "FIRST"); s->navs[0].cls = 0x100;
    strcpy(s->navs[1].name, "RETURN"); s->navs[1].cls = 0x20;
    strcpy(s->navs[2].name, "LATER"); s->navs[2].cls = 0x20;
    assert(command_return_target(s, 2) == MSIM_TGT_NAV(1));
    assert(msim_command(s, 1, MSIM_CMD_DISENGAGE, -1) == 1 && s->cmd_target[0] == MSIM_TGT_NAV(1));
    s->player_nav = 2; assert(s->cmd_target[0] == MSIM_TGT_NAV(1));
    t->nodes[1].kind = t->nodes[2].kind = 1;
    assert(command_return_target(s, 2) == MSIM_TGT_NAV(0));
    t->node_count = 1; t->nodes[0].kind = 0x20; strcpy(t->nodes[0].target, "RETURN");
    assert(command_return_target(s, 2) == MSIM_TGT_NAV(1));
    puts("PASS: original first kind-0x20 return node / first-node fallback, independent of cockpit nav selection");
    done(s);
}
int main(void) { fixtures(); targets_and_wake(); return_nav(); return 0; }
