/* Declared geometry/AI fixtures for the Mercs pre-fire predicate. Actual retail Dragon pose and
 * native combat_step consumers; no scene, career, inputs, or game rules
 * are modified. Expected decisions follow the executed-original pre-fire
 * contract, not a claim that intercepted original world geometry ran. */
#define _POSIX_C_SOURCE 200809L
#ifndef AI_LOS_SOURCE
#define AI_LOS_SOURCE "../src/msim.c"
#endif
#include AI_LOS_SOURCE

typedef struct {
    msim *s;
    world_actor actors[3]; combat_unit units[3]; ai_mind minds[3]; msim_aim aim[3];
    int armed[3], cmd[3], cmd_target[3];
    float height[3], center[3], radius[3];
    mech3d world;
    int emitted;
    float emission[3];
} fixture;

static void emit(void *opaque, int w, int owner, int first, float x, float z)
{
    fixture *f = opaque;
    if (owner == 0) { f->emitted++; f->emission[0] = x; f->emission[2] = z; }
    (void)w; (void)first;
}
static void init_unit(combat_unit *u)
{
    int i;
    memset(u,0,sizeof *u); u->armor_mult = u->speed_gain = 1; u->lock_target = -2;
    for (i=0;i<8;i++) u->armor[i] = u->internal[i] = 100;
}
static float height(const mech3d *m)
{
    float h=0; int p,v;
    for (p=0;p<m->part_count;p++) if (m->parts[p].model.object_count)
        for (v=0;v<m->parts[p].model.objects[0].vert_count;v++) {
            float y=(float)(m->parts[p].model.objects[0].verts[v].y+m->parts[p].pos[1]-m->origin[1]);
            if (y>h) h=y;
        }
    return h;
}
static void actor(fixture *f, prj_archive *a, int i, const char *model, int x, int z)
{
    msim *s=f->s;
    if (mech3d_load(a,model,0,&f->actors[i].mech)) { fprintf(stderr,"load %s failed\n",model);exit(2); }
    f->height[i]=height(&f->actors[i].mech); f->center[i]=690; f->radius[i]=440;
    f->actors[i].mech.origin[0]=x; f->actors[i].mech.origin[2]=z;
    mech3d_pose(&f->actors[i].mech,NULL,0);
    f->actors[i].table=i+1; f->actors[i].gps_control=2; f->actors[i].unit_class=1;
    f->actors[i].alliance=i?0:1;
    init_unit(&f->units[i]); f->armed[i]=1; f->minds[i].tgt=-2;
    if (s->actor_count<=i) s->actor_count=i+1;
}
static fixture *init(prj_archive *a)
{
    fixture *f=calloc(1,sizeof *f); msim *s;
    unsigned seed;
    if (!f || !(f->s=calloc(1,sizeof *f->s))) exit(2);
    s=f->s;
    s->actors=f->actors; s->units=f->units; s->minds=f->minds; s->aim=f->aim;
    s->armed=f->armed; s->height=f->height; s->centre_h=f->center; s->radius=f->radius;
    s->cmd=f->cmd; s->cmd_target=f->cmd_target;
    s->ai_ok=1; s->difficulty=1; s->player_target=s->player_lock_target=s->player_sel_w=-1;
    s->player_radius=440; s->player_centre_h=690; s->player_unit_class=1;
    s->player[0]=30000; s->player[2]=30000; s->no_player_muzzle=1;
    s->on_shot=emit; s->shot_user=f;
    init_unit(&s->player_unit); s->player_unit.human=1;
    actor(f,a,0,"DRAGON",0,0);
    f->minds[0].state=AI_ATTACK; f->minds[0].wants_fire=1;
    f->units[0].weapon_count=1; f->units[0].ai_sel=0;
    f->units[0].weapons[0].weapon=56; f->units[0].weapons[0].ammo=-1;
    f->units[0].weapons[0].state=CW_READY; f->units[0].weapons[0].location=2;
    /* A declared deterministic seed whose unchanged production draw allows
     * this ready ML. The original refire/90+1 choice is never intercepted. */
    for (seed=1;;seed++) { unsigned copy=seed; if (!ms_roll(&copy,2)) break; }
    s->rng=seed;
    return f;
}
static int box(fixture *f,int x,int y,int z,int rx,int ry,int rz)
{
    static const unsigned short face[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{0,4,7,3},{1,2,6,5}};
    int n=f->world.part_count,v,k; mech3d_part *p; wtb_object *o;
    f->world.parts=realloc(f->world.parts,(size_t)(n+1)*sizeof *f->world.parts);
    if (!f->world.parts) exit(2);
    p=&f->world.parts[n]; memset(p,0,sizeof *p);
    p->pos[0]=x; p->pos[1]=y; p->pos[2]=z; p->parent_obj=-1;
    p->node=-1; p->obj_index=n; p->objtype=0; p->coll=0;
    snprintf(p->rec,sizeof p->rec,"LOSCASE");
    p->model.objects=calloc(1,sizeof *o); p->model.object_count=1;
    o=p->model.objects; o->verts=calloc(8,sizeof *o->verts);o->vert_count=8;
    o->polys=calloc(6,sizeof *o->polys);o->poly_count=6;
    if (!o->verts || !o->polys) exit(2);
    for(v=0;v<8;v++) {
        o->verts[v].x=(v==1||v==2||v==5||v==6)?rx:-rx;
        o->verts[v].y=(v==2||v==3||v==6||v==7)?ry:-ry;
        o->verts[v].z=v>=4?rz:-rz;
    }
    for(v=0;v<6;v++) { o->polys[v].n=4;for(k=0;k<4;k++)o->polys[v].idx[k]=face[v][k]; }
    f->world.part_count++; return n;
}
static int building(fixture *f,int part)
{
    msim *s=f->s; int b=s->bld_count++;
    s->bld=realloc(s->bld,(size_t)s->bld_count*sizeof *s->bld);
    if (!s->bld)exit(2);
    memset(&s->bld[b],0,sizeof s->bld[b]); s->bld[b].intact=part;s->bld[b].destroyed=-1;s->bld[b].hp=10000;
    if (part_box(&f->world.parts[part],s->bld[b].mn,s->bld[b].mx))exit(2);
    return b;
}
static void target_actor(fixture *f,prj_archive *a)
{
    actor(f,a,1,"DRAGON",0,10000);f->minds[0].tgt=1;
}
static void target_thing(fixture *f,int b)
{
    f->minds[0].tgt=-2;f->minds[0].node_tgt=0x40000|b;
}
static void install_world(fixture *f)
{
    /* Declared GT fixtures already installed. Same-pointer setter keeps
     * them and uses the actual terrain/collision/thing mapping builders. */
    f->s->world=&f->world; msim_set_world(f->s,&f->world);
}
static void destroy(fixture *f)
{
    int i; msim *s=f->s;
    for(i=0;i<s->actor_count;i++)mech3d_free(&f->actors[i].mech);
    if(s->pl_mech_ok)mech3d_free(&s->pl_mech);
    terrain_free(s->terrain);free(s->obstacles);free(s->obstacle_part);free(s->clist_seq);free(s->bld);
    mech3d_free(&f->world);free(s);free(f);
}

int main(int argc,char **argv)
{
    prj_archive *a;char error[256];int id,fail=0,checks=0;
    if(argc!=2){fprintf(stderr,"usage: %s ARCHIVE.PRJ\n",argv[0]);return 2;}
    unsetenv("MERCS_INSTALL_DIR");unsetenv("MERCS_CD_DIR");unsetenv("MERCS_CD_IMAGE");dp_clear_roots();
    combat_set_weapon_profile(COMBAT_PROFILE_RETAIL);
    a=prj_open(argv[1],error,sizeof error);if(!a){fprintf(stderr,"%s\n",error);return 2;}
    puts("{\"scope\":\"private full-native combat_step consumers with actual Dragon poses and declared world boxes\",\"rows\":[");
    for(id=0;id<16;id++){
        fixture *f=init(a);msim *s=f->s;int expected=1,part,b,trigger,emitted,pre_state,post_state,failbefore=fail;
        float eye_y=f->actors[0].mech.eye_pos[1], proxy_y=f->height[0]*.7f, ray_hit=0;
        float eye[3],dv[3],normal[3],world_hit=-1;int ci,hitunit=-2,group;
        const char *name=""; unsigned before_rng=s->rng,decision_rng;
        switch(id){
        case 0:name="selected-thing-front-face";b=building(f,box(f,0,1500,10000,1000,1500,1000));target_thing(f,b);break;
        case 1:name="selected-unit-nearer-than-world";target_actor(f,a);box(f,0,1000,9950,1000,1000,10);break;
        case 2:name="world-nearer-than-selected-unit";target_actor(f,a);box(f,0,1000,4000,1000,1000,10);expected=0;break;
        case 3:name="selected-thing-behind-different-thing";b=building(f,box(f,0,1500,10000,1000,1500,1000));target_thing(f,b);building(f,box(f,0,1500,4000,1000,1500,100));expected=0;break;
        case 4:name="posed-eye-clear-proxy-blocked";target_actor(f,a);{
            float old_y=proxy_y+(f->height[1]*.55f-proxy_y)*.4f;
            int top=(int)floorf((old_y+eye_y)*.5f);
            if(!(eye_y>old_y+30)){fprintf(stderr,"expected Dragon eye/proxy separation\n");exit(2);}
            box(f,0,top/2,4000,1000,top/2,10);
        }break;
        case 5:name="posed-eye-blocked-proxy-clear";target_actor(f,a);box(f,0,(int)lrintf(eye_y),4000,1000,15,10);expected=0;break;
        case 6:name="self-hull-excluded";target_actor(f,a);break;
        case 7:name="friendly-unarmed-unit-blocks";target_actor(f,a);actor(f,a,2,"DRAGON",0,4000);f->armed[2]=0;expected=0;break;
        case 8:name="standing-player-wreck-blocks";target_actor(f,a);if(mech3d_load(a,"DRAGON",0,&s->pl_mech))return 2;s->pl_mech_ok=1;s->player[0]=0;s->player[2]=4000;s->player_unit.destroyed=1;expected=0;break;
        case 9:name="no-eye-fallback-unchanged";target_actor(f,a);box(f,0,1000,9950,1000,1000,10);f->actors[0].mech.eye_ok=0;expected=0;break;
        case 10:name="behind-body-half-plane";target_actor(f,a);f->actors[1].mech.origin[2]=-10000;mech3d_pose(&f->actors[1].mech,NULL,0);expected=0;break;
        case 11:name="commanded-thing-overrides-stale-node";building(f,box(f,8000,1500,10000,1000,1500,1000));b=building(f,box(f,0,1500,10000,1000,1500,1000));target_thing(f,0);f->cmd[0]=MSIM_CMD_ATTACK;f->cmd_target[0]=MSIM_TGT_THING(b);break;
        /* Original parent-before-child initial creation leaves each child with
         * its own direct identity. These two fixtures do not model later
         * recursive parent recreation while child geometry already exists. */
        case 12:name="startup-parent-gt-unbound-child-blocks";b=building(f,box(f,0,1500,10000,1000,1500,1000));part=box(f,0,1500,4000,1000,1500,100);f->world.parts[part].parent_obj=0;target_thing(f,b);expected=0;break;
        case 13:name="startup-overlapping-parent-child-gt-blocks";b=building(f,box(f,0,1500,10000,1000,1500,1000));part=box(f,0,1500,4000,1000,1500,100);f->world.parts[part].parent_obj=0;building(f,part);target_thing(f,b);expected=0;break;
        case 14:name="same-eye-ray-no-hit-accepts";target_actor(f,a);f->actors[1].mech.origin[0]=5000;mech3d_pose(&f->actors[1].mech,NULL,0);break;
        case 15:name="registered-detached-piece-blocks";target_actor(f,a);
            part=box(f,0,0,50000,1000,100,100);f->world.parts[part].hidden=1;
            s->debris_count=1;s->debris[0].kind=3;s->debris[0].part=part;s->debris[0].destructible=1;
            s->debris[0].p[1]=eye_y;s->debris[0].p[2]=4000;s->debris[0].health_q16=16*65536;s->debris[0].until=100000;
            expected=0;break;
        }
        install_world(f);
        /* Independently expose current native intersection distances along
         * the known horizontal Dragon eye, without replacing any consumer. */
        memcpy(eye,f->actors[0].mech.eye_pos,sizeof eye);dv[0]=dv[1]=0;dv[2]=10000;
        {float end[3]={eye[0],eye[1],eye[2]+10000};if(clist_seg(s,eye,end,0,&world_hit,normal,&ci)==0)world_hit=-1;}
        {float best=1;shot_units(s,0,eye,dv,&best,&hitunit,&group);if(hitunit!=-2)ray_hit=best*10000;}
        pre_state=f->units[0].weapons[0].state;
        combat_step(s,0);trigger=f->units[0].ai_trigger;decision_rng=s->rng;
        if(expected>=0 && (trigger!=expected||f->emitted!=0||f->units[0].weapons[0].state!=pre_state))fail++;
        {unsigned expected_rng=before_rng; (void)ms_roll(&expected_rng,2);if(decision_rng!=expected_rng)fail++;}
        /* One native frame consumes the previous decision. ML first pulse
         * interval is 60 ticks, so 61 ticks makes the actual projectile
         * observable. No fresh wants_fire decision is permitted in this
         * second step; this isolates the already-scheduled trigger. */
        f->minds[0].wants_fire=0;
        combat_step(s,61.0f/COMBAT_TICKS_PER_S);emitted=f->emitted;post_state=f->units[0].weapons[0].state;
        if(expected>=0 && (emitted>0)!=expected)fail++;
        checks+=expected>=0?3:1;
        printf("%s{\"name\":\"%s\",\"expected_clear\":%d,\"trigger\":%d,\"emitted\":%d,\"pre_state\":%d,\"post_state\":%d,\"eye_y\":%.9g,\"proxy_y\":%.9g,\"nearest_old_unit\":%d,\"unit_distance\":%.9g,\"world_distance\":%.9g,\"rng_before\":%u,\"rng_after_decision\":%u,\"pass\":%s}",id?",\n":"",name,expected,trigger,emitted,pre_state,post_state,eye_y,proxy_y,hitunit,ray_hit,world_hit,before_rng,decision_rng,expected<0?"null":fail==failbefore?"true":"false");
        destroy(f);
    }
    printf("\n],\"diagnostic_rows\":[");
    for (id=0;id<5;id++) {
        static const char *const names[]={"clear","world","unarmed-unit","aim-half-plane","invalid-actor"};
        static const int expected[]={1,-1,0,-3,-2};
        fixture *f=init(a); msim *s=f->s; int got; unsigned rng;
        s->player[0]=0;s->player[2]=id==3?-10000:10000;
        if(mech3d_load(a,"DRAGON",0,&s->pl_mech))return 2;
        s->pl_mech_ok=1;s->pl_mech.origin[2]=(int32_t)s->player[2];
        mech3d_pose(&s->pl_mech,NULL,0);f->minds[0].tgt=-1;
        if(id==1)box(f,0,1000,4000,1000,1000,10);
        if(id==2){actor(f,a,1,"DRAGON",0,4000);f->armed[1]=0;}
        install_world(f);rng=s->rng;
        got=msim_test_fire_line(s,id==4?-1:0);
        if(got!=expected[id]||s->rng!=rng)fail++;
        checks++;
        printf("%s{\"name\":\"%s\",\"expected\":%d,\"actual\":%d,\"rng_unchanged\":%s,\"pass\":%s}",
               id?",":"",names[id],expected[id],got,s->rng==rng?"true":"false",got==expected[id]&&s->rng==rng?"true":"false");
        destroy(f);
    }
    prj_close(a);printf("\n],\"graded_consumer_cases\":16,\"ungraded_ownership_observations\":0,\"diagnostic_cases\":5,\"checks\":%d,\"failures\":%d,\"pass\":%s}\n",checks,fail,fail?"false":"true");return fail?1:0;
}
