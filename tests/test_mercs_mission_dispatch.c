/* Original-backed scheduler snapshots and declared native integration inputs.
 * No saved career, original full mission, or campaign victory is exercised. */
#ifndef MERCS_MSIM_UNDER_TEST
#define MERCS_MSIM_UNDER_TEST "../src/msim.c"
#endif
#include MERCS_MSIM_UNDER_TEST
#ifndef MERCS_ORDER_FIXTURES
#define MERCS_ORDER_FIXTURES "../tests/fixtures/mission-order-dispatch/original/mission-order-fixtures.h"
#endif
#include MERCS_ORDER_FIXTURES

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
static unsigned checks, failures, scheduler_steps, native_steps, scene_loads, bindings;
static const char *case_name;
static void equal(double got, double want, const char *what)
{
    checks++;
    if (!isfinite(got) || !isfinite(want) || fabs(got - want) > 0.001) {
        if (failures < 35) fprintf(stderr, "FAIL %s: %s got %.7g want %.7g\n", case_name, what, got, want);
        failures++;
    }
}
static int milliseconds(int seconds) { return seconds < 0 ? seconds : seconds * 1000; }
static void load_table(mtbl_logic *m, int t, const order_table_input *f)
{
    int n;
    mtbl_table *b = &m->tables[t]; b->index = t; b->node_count = f->count;
    m->current[t] = f->current; m->result[t] = f->result;
    m->start_time[t] = milliseconds(f->start); m->result_time[t] = milliseconds(f->end);
    for (n = 0; n < f->count; n++) {
        const order_node_input *x = &f->nodes[n]; mtbl_node *y = &b->nodes[n];
        y->kind = (uint32_t)x->kind; y->objective = x->main ? 'M' : 'N'; y->delay = (uint32_t)x->delay;
        y->require_all = x->all; y->other_table = x->other_table; y->other_node = x->other_node;
        if (x->dep) { y->item_count = 1; y->items[0].type = x->dep; y->items[0].table = x->dep_table; y->items[0].node = x->dep_node; }
        m->state[t][n] = (unsigned char)x->state; m->started[t][n] = milliseconds(x->start); m->ended[t][n] = milliseconds(x->end);
    }
}
static void compare_table(const mtbl_logic *m, int t, const order_table_expected *w)
{
    int n;
    equal(m->current[t], w->current, "published current"); equal(m->result[t], w->result, "table result");
    if (w->has_times) { equal(m->start_time[t], milliseconds(w->start), "table start"); equal(m->result_time[t], milliseconds(w->end), "table end"); }
    for (n = 0; n < w->count; n++) {
        equal(m->state[t][n], w->state[n], "node state");
        equal(m->started[t][n], milliseconds(w->started[n]), "node start");
        equal(m->ended[t][n], milliseconds(w->ended[n]), "node end");
    }
}
static void scheduler(void)
{
    int c,t,j;
    for (c = 0; c < COUNT(original_order_fixtures); c++) {
        const order_fixture *f = &original_order_fixtures[c]; mtbl_logic *m = calloc(1,sizeof *m);
        if (!m) exit(2);
        case_name = f->name; m->table_count = f->table_count;
        for (t = 0; t < f->table_count; t++) load_table(m,t,&f->tables[t]);
        for (j = 0; j < 3; j++) {
            mtbl_logic_step(m,f->steps[j].seconds*1000,NULL); scheduler_steps++;
            for (t = 0; t < f->table_count; t++) if (f->compare_mask & (1<<t)) compare_table(m,t,&f->steps[j].tables[t]);
        }
        free(m);
    }
}

typedef struct { msim *s; int count, refuse, type, instance; unsigned generation; float pos[3]; int member_state; } capture;
static void effect_capture(void *opaque, int type, const float p[3])
{
    capture *c = opaque;
    if (type != 22) return;
    c->count++; c->type = type; memcpy(c->pos,p,sizeof c->pos); c->member_state = c->s->minds[1].state;
    c->s->fx_refused = c->refuse; c->s->fx_instance = c->instance; c->s->fx_generation = c->generation;
}
static void healthy(combat_unit *u)
{
    int i; memset(u,0,sizeof *u); u->armor_mult = u->speed_gain = 1; u->lock_target = -2;
    for (i = 0; i < 8; i++) u->armor[i] = u->internal[i] = 100;
}
static msim *synthetic(capture *c, int table, const int p[3])
{
    int i,t; msim *s = calloc(1,sizeof *s); if (!s) exit(2);
    s->actor_count = 2; s->player_table = 0; s->player_target = s->player_lock_target = s->player_sel_w = -1;
    s->player_aim_free = 1; s->no_player_muzzle = 1; s->no_col_lod = 1; s->rng = 1;
    s->player[0] = s->player[2] = -500000; healthy(&s->player_unit); s->player_unit.human = 1;
#define ALLOC(field) s->field = calloc(2,sizeof *s->field); if (!s->field) exit(2)
    ALLOC(actors); ALLOC(units); ALLOC(armed); ALLOC(minds); ALLOC(seen_node); ALLOC(powered_down);
    ALLOC(height); ALLOC(centre_h); ALLOC(radius); ALLOC(cmd); ALLOC(engage); ALLOC(online_at);
    ALLOC(ai_focus); ALLOC(ai_who); ALLOC(cmd_target); ALLOC(mate_target); ALLOC(defend_leg);
#undef ALLOC
    s->ai_ok = 1; s->ai.count = 1; s->ai.progs[0].state_count = 4;
    s->ai.progs[0].states[0]=0; s->ai.progs[0].states[1]=5; s->ai.progs[0].states[2]=10; s->ai.progs[0].states[3]=11;
    s->logic.table_count = table+1;
    for (t = 0; t < table; t++) { s->logic.current[t] = -1; s->logic.result[t] = 2; s->logic.result_time[t] = 0; }
    /* Keep the separate fixture player table unresolved; no success is supplied
     * or counted. It has one unlimited HOLD with a true empty ALL condition. */
    s->logic.result[0] = 0; s->logic.result_time[0] = -1; s->logic.tables[0].node_count = 1;
    s->logic.tables[0].nodes[0].kind = MTBL_K_HOLD; s->logic.tables[0].nodes[0].delay = UINT32_MAX;
    s->logic.tables[0].nodes[0].require_all = 1; s->logic.tables[0].nodes[0].objective = 'M';
    /* Original WOL1 first/second mine input graph is already in the generated
     * scheduler fixtures. Copy its declared runtime node input, not a saved game. */
    load_table(&s->logic,table,&original_order_fixtures[table==6 ? 1 : 0].tables[table]);
    snprintf(s->logic.tables[table].nodes[1].target,10,"WOL1NUK%d",table==6?2:1);
    s->logic.tables[table].nodes[1].need = 2; s->logic.tables[table].nodes[1].lock = 2;
    for (i = 0; i < 2; i++) {
        world_actor *a = &s->actors[i];
        a->table = table; a->leader = i==0; a->friendly = 1; a->alliance = 0;
        a->unit_class = 1; a->obj_class = 0x1006; a->gps_control = 2;
        snprintf(a->group,sizeof a->group,"WOL1NUK%d",table==6?2:1);
        a->mech.origin[0] = p[0] + (i==0?1000:0); a->mech.origin[2] = p[2];
        healthy(&s->units[i]); s->units[i].weapon_count = 1;
        s->units[i].weapons[0].weapon = 21; s->units[i].weapons[0].state = CW_READY; s->units[i].weapons[0].ammo = -1;
        s->armed[i] = 1; s->height[i] = 2.0f*(float)p[1]; s->centre_h[i] = (float)p[1]; s->radius[i] = 10;
        ai_init(&s->minds[i],-1); s->minds[i].state = AI_REST; s->minds[i].lock_bits = 1;
        s->seen_node[i] = 2; s->powered_down[i] = 1; s->ai_focus[i] = s->ai_who[i] = -2;
        s->cmd_target[i] = s->mate_target[i] = -1;
    }
    memset(c,0,sizeof *c); c->s = s; c->instance = 186; c->generation = 7;
    s->shot_user = c; s->on_effect = effect_capture; s->fx_radius[22] = 100;
    return s;
}
static void destroy_synthetic(msim *s)
{
    world_actor *actors = s->actors; msim_free(s); free(actors); free(s);
}
static void published_instant(msim *s, int table)
{
    int i; s->logic.current[table] = 1; s->logic.state[table][2] = MTBL_OK;
    s->logic.ended[table][2] = table==6 ? 286000 : 141000;
    s->now = table==6 ? 286000 : 141000;
    for (i = 0; i < 2; i++) { s->seen_node[i] = 1; s->minds[i].state = i ? AI_FOLLOW : AI_IDLE; s->powered_down[i] = 0; }
}
static void dispatch_fixtures(void)
{
    int c,k;
    for (c = 0; c < COUNT(original_dispatch_fixtures); c++) {
        const order_dispatch_fixture *f = &original_dispatch_fixtures[c]; capture got;
        msim *s = synthetic(&got,f->table,f->position); case_name = f->name; published_instant(s,f->table);
        if (f->mode==1) s->minds[1].state = AI_IDLE;
        if (f->mode==2) for (k=0;k<2;k++) { s->units[k].destroyed=1; s->units[k].intact_out=1; }
        if (f->mode==3) s->player_table=f->table;
        if (f->mode==4) { s->logic.state[f->table][2]=MTBL_FAILED; s->logic.current[f->table]=3; for(k=0;k<2;k++)s->seen_node[k]=3; }
        msim_step(s,33); native_steps++;
        equal(got.count,f->requests,"original effect request count");
        if (f->requests && got.count) {
            equal(got.type,22,"effect type");
            for (k=0;k<3;k++) equal(got.pos[k],f->position[k],"selected member body point");
            equal(s->area_count,1,"existing area producer");
            if (s->area_count) { equal(s->area[0].owner,-2,"original source identity"); equal(s->area[0].attached,0,"unattached effect"); equal(s->area[0].instance,186,"frontend instance"); equal(s->area[0].generation,7,"frontend generation"); }
        }
        destroy_synthetic(s);
    }
}
static void handover_and_refusal(void)
{
    static const int dts[]={1,6,33,100,250}; const int p[3]={79000,3200,19300}; int dos,j,frame,refused;
    for (dos=0;dos<2;dos++) for (j=0;j<COUNT(dts);j++) for(refused=0;refused<2;refused++) {
        capture got; msim *s=synthetic(&got,5,p); int dt=dts[j];
        case_name=refused?"full-step instant allocation refusal":"full-step hold/instant/hold handover";
        combat_set_weapon_timing(dos); fx_set_timing(dos); got.refuse=refused; s->now=141000-dt;
        for(frame=0;frame<4;frame++) {
            msim_step(s,dt); native_steps++;
            equal(got.count,frame?1:0,"one request only, independent of frame size");
            equal(s->logic.current[5],frame?3:1,"current-order phase");
            if (!frame) { equal(s->minds[0].state,AI_IDLE,"leader awakened"); equal(s->minds[1].state,AI_FOLLOW,"member awakened"); }
            equal(s->area_count,frame&&!refused?1:0,"refusal controls gameplay area");
        }
        if (!refused && s->area_count) { equal(s->area[0].duration,dos?1820:1810,"profile effect duration"); equal(s->area[0].owner,-2,"no player kill ownership"); }
        equal(s->logic.result[0],0,"fixture is not a campaign win");
        destroy_synthetic(s);
    }
}
static void poststate_controls(void)
{
    const int p[3]={79000,3200,19300}; int mode;
    for(mode=0;mode<3;mode++) {
        capture got; msim *s=synthetic(&got,5,p); published_instant(s,5);
        case_name=mode==0?"native post-state AIT rejection":mode==1?"native pushed post-state":"native both members powered down";
        if(mode==0) { s->ai.progs[0].state_count=1; s->ai.progs[0].states[0]=5; }
        if(mode==1) s->minds[1].counter=1;
        if(mode==2) s->powered_down[0]=s->powered_down[1]=1;
        node_assign(s,5,0);
        equal(got.count,mode==2?0:1,"request precedes post-state handling");
        if(mode!=2) { equal(s->minds[1].state,AI_FOLLOW,"rejected/pushed state preserved"); equal(s->area_count,1,"area exists before inspecting owner"); if(s->area_count) equal(s->area[0].owner,-2,"source preserved"); }
        destroy_synthetic(s);
    }
}
static void transition_controls(void)
{
    const int p[3]={79000,3200,19300}; int c,dos,i;
    for(dos=0;dos<2;dos++) for(c=0;c<COUNT(original_transition_fixtures);c++) {
        const order_transition_fixture *f=&original_transition_fixtures[c]; capture got;
        msim *s=synthetic(&got,5,p); case_name=f->name; s->now=140967;
        combat_set_weapon_timing(dos); fx_set_timing(dos);
        if(!f->admit) { s->ai.progs[0].state_count=2; s->ai.progs[0].states[0]=10; s->ai.progs[0].states[1]=11; }
        for(i=0;i<2;i++) s->minds[i].counter=f->pushed;
        msim_step(s,33); native_steps++;
        equal(s->logic.current[5],1,"instant current still publishes"); equal(got.count,0,"transition itself does not emit");
        for(i=0;i<2;i++) {
            equal(s->minds[i].state,f->states[i],"original admitted/rejected/pushed transition state");
            equal(s->powered_down[i],f->states[i]==10,"original REST power state");
            equal(s->minds[i].lock_bits&3,1,"new order lock published even on state rejection");
        }
        msim_step(s,33); native_steps++;
        equal(got.count,f->requests,"following leader obeys transition admission");
        equal(s->logic.current[5],3,"instant leaves current on next pass");
        destroy_synthetic(s);
    }
}
static void authored_bindings(const char *path)
{
    char error[256]; int t,q,n; prj_archive *a=prj_open(path,error,sizeof error);
    world_actor *actors=NULL; int count=0; msim *s=calloc(1,sizeof *s);
    if(!a||!s) { fprintf(stderr,"archive/setup: %s\n",a?"allocation failed":error); exit(2); }
    if(world3d_actors(a,"WOL1SCN1",&actors,&count)||msim_init(s,a,"WOL1SCN1",actors,count)) exit(2);
    scene_loads++; case_name="actual WOL1 archive initialization (no steps)";
    for(t=5;t<=6;t++) {
        int code[40]; const mtbl_node *node=&s->logic.tables[t].nodes[1];
        equal(node->kind,0x1000,"authored instant kind"); equal(node->need,2,"authored member requirement"); equal(node->delay,3,"stored delay");
        equal(node->item_count,1,"single prerequisite"); equal(node->items[0].type,'S',"successful hold prerequisite"); equal(node->items[0].table,t,"same-table prerequisite"); equal(node->items[0].node,2,"later-index prerequisite");
        n=node_target_list(s,node,code,40); equal(n,2,"actual expanded helper targets"); bindings+=(unsigned)n;
        for(q=0;q<n;q++) {
            int i=(code[q]&0xffff)-1; equal(code[q]>>16,2,"actor target class");
            if(i<0||i>=count) { equal(0,1,"valid actor index"); continue; }
            equal(actors[i].obj_class,0x1006,"authored helper object class"); equal(actors[i].gps_control,2,"AI control");
            equal(s->armed[i],1,"native actual loadout available"); equal(combat_out_of_weapons(&s->units[i]),0,"actual helper has usable weapons");
            equal(ai_allows(&s->ai,&s->minds[i],AI_IDLE),1,"actual AIT admits idle"); equal(ai_allows(&s->ai,&s->minds[i],AI_FOLLOW),1,"actual AIT admits follow");
        }
    }
    msim_free(s); world3d_free_actors(actors,count); free(s); prj_close(a);
}
int main(int argc,char **argv)
{
    int i;
#if !defined(_WIN32)
    unsetenv("MERCS_INSTALL_DIR"); unsetenv("MERCS_CD_DIR"); unsetenv("MERCS_CD_IMAGE"); unsetenv("MERCS_OF"); unsetenv("MERCS_OE");
#endif
    dp_clear_roots(); combat_set_weapon_profile(COMBAT_PROFILE_RETAIL);
    scheduler(); dispatch_fixtures(); handover_and_refusal(); poststate_controls(); transition_controls();
    for(i=1;i<argc;i++) authored_bindings(argv[i]);
    printf("{\"summary\":true,\"checks\":%u,\"failures\":%u,\"original_scheduler_cases\":%d,\"scheduler_updates\":%u,\"original_dispatch_cases\":%d,\"original_transition_cases\":%d,\"native_ms_steps\":%u,\"scene_initializations\":%u,\"authored_actor_bindings\":%u,\"scope\":\"executed-original runtime snapshots plus declared native integration; archive initialization only; no campaign wins\"}\n",checks,failures,COUNT(original_order_fixtures),scheduler_steps,COUNT(original_dispatch_fixtures),COUNT(original_transition_fixtures),native_steps,scene_loads,bindings);
    return failures?1:0;
}
