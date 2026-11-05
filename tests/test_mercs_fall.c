#include "combat.h"
#include "mech3d.h"
#include "msim.h"
#include "bwd.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../tests/fixtures/combat-completion/original-fall.h"

static int checks;
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"fall check failed at %s:%d: %s\n",__FILE__,__LINE__,#c); abort(); } } while(0)
static void original_frames(void)
{
    size_t i;
    for(i=0;i<sizeof original_falls/sizeof original_falls[0];++i) {
        const original_fall_case *c=&original_falls[i]; combat_unit u={0};
        int contact=0,k,result; int32_t center=c->center,xyz[3],delta[3];
        memcpy(xyz,c->xyz,sizeof xyz);
        u.fall_angle=c->angle;u.fall_velocity=c->velocity;u.fall_active=(c->flags&4)!=0;u.fall_contact=(c->flags&8)!=0;
        for(k=0;k<2;k++)if((c->nodes&(1<<k))&&c->node_y[k]-(c->radius>>1)<=c->node_ground[k])contact=1;
        result=combat_fall_frame(&u,c->ticks,c->grounded,c->xyz[1]-c->ground-c->center,contact);
        if(result&3) {
            mech3d_fall_geometry(c->angle,u.fall_angle,c->base,c->heading,&center,delta);
            if(result&1)for(k=0;k<3;k++)xyz[k]=(int32_t)((uint32_t)xyz[k]+(uint32_t)delta[k]);
        }
        if(u.fall_angle!=c->want_angle||u.fall_velocity!=c->want_velocity||center!=c->want_center||memcmp(xyz,c->want_xyz,sizeof xyz))
            fprintf(stderr,"original fall case %zu: native angle %d velocity %d center %d xyz %d,%d,%d\n",i,u.fall_angle,u.fall_velocity,center,xyz[0],xyz[1],xyz[2]);
        CHECK(u.fall_angle==c->want_angle);CHECK(u.fall_velocity==c->want_velocity);CHECK(center==c->want_center);
        CHECK(!memcmp(xyz,c->want_xyz,sizeof xyz));CHECK((u.fall_active*4+u.fall_contact*8)==c->want_flags);
        CHECK(((result&4)!=0)==c->sound);
    }
    for(i=0;i<sizeof original_fall_begin/sizeof original_fall_begin[0];i++) {
        combat_unit u={0};u.fall_angle=12345;u.fall_contact=1;u.narc_beacon=1;
        combat_fall_begin(&u,original_fall_begin[i][0]);
        CHECK(u.fall_angle==original_fall_begin[i][1]);CHECK(u.fall_velocity==original_fall_begin[i][2]);
        CHECK(u.narc_beacon+u.fall_active*4+u.fall_contact*8==original_fall_begin[i][3]);
    }
    {
        combat_unit u={0};int32_t center=550,xyz[3]={1000,550,3000};u.fall_active=1;
        for(i=0;i<sizeof original_fall_sequence/sizeof original_fall_sequence[0];i++) {
            int32_t old=u.fall_angle,delta[3];int result,k;
            if(i>=80)xyz[1]=2000;
            result=combat_fall_frame(&u,6,i<80,xyz[1]-center,i>=60);
            if(result&3) {
                mech3d_fall_geometry(old,u.fall_angle,550,0,&center,delta);
                if(result&1)for(k=0;k<3;k++)xyz[k]=(int32_t)((uint32_t)xyz[k]+(uint32_t)delta[k]);
            }
            CHECK(u.fall_angle==original_fall_sequence[i][0]);CHECK(u.fall_velocity==original_fall_sequence[i][1]);
            CHECK(center==original_fall_sequence[i][2]);CHECK(u.fall_active*4+u.fall_contact*8==original_fall_sequence[i][3]);
            CHECK(!memcmp(xyz,original_fall_sequence[i]+4,sizeof xyz));CHECK(((result&4)!=0)==original_fall_sequence[i][7]);
        }
    }
}

static int sounds;
static void sound(void *user,int snd,const float p[3])
{(void)user;CHECK(snd==0xee);CHECK(p!=NULL);++sounds;}
static void simulation_adapter(void)
{
    msim *s=calloc(1,sizeof *s);mech3d m={0};mech3d_part parts[2];int32_t center,delta[3];
    memset(parts,0,sizeof parts);
    CHECK(s!=NULL);m.centre_h=550;m.parts=parts;m.part_count=2;m.fall_node[0]=12;m.fall_node[1]=21;
    parts[0].node=12;parts[1].node=21;
    parts[0].sphere_ok=parts[1].sphere_ok=1;parts[0].hull_radius=parts[1].hull_radius=200;
    parts[0].pos[1]=parts[1].pos[1]=800;
    s->player_centre_h=550;s->player_unit.fall_active=1;s->on_sound=sound;
    m.origin[0]=1000;m.origin[2]=3000;
    CHECK(msim_fall_step(s,-1,&m,6)==1);CHECK(s->player_unit.fall_angle==1800);
    CHECK(s->player_unit.y==0);CHECK(m.fall_active&&m.fall_center==550);
    parts[1].pos[1]=100;
    CHECK(msim_fall_step(s,-1,&m,6)==4);CHECK(sounds==1);
    CHECK(msim_fall_step(s,-1,&m,6)==0);CHECK(sounds==1);
    s->player_unit.fall_angle=60*65536;s->player_unit.fall_velocity=65536;
    s->player_centre_h=275;s->player_unit.y=725;
    mech3d_fall_geometry(60*65536,60*65536-1950,550,0,&center,delta);
    CHECK(msim_fall_step(s,-1,&m,6)==2);
    CHECK(s->player_unit.y+s->player_centre_h==1000); /* unchanged original body center */
    CHECK(!s->player_unit.fall_contact);CHECK(s->player_centre_h==center);
    s->player_unit.fall_angle=-60*65536;s->player_unit.fall_velocity=-65536;s->player_unit.y=1000;
    CHECK(msim_fall_step(s,-1,&m,6)==2);CHECK(s->player_unit.fall_angle==0);CHECK(s->player_centre_h==550);
    free(s);
}

static void camera_point(const float r[9],const float eye[3],const int32_t p[3],float out[3])
{int i;for(i=0;i<3;i++)out[i]=r[i]*(p[0]-eye[0])+r[3+i]*(p[1]-eye[1])+r[6+i]*(p[2]-eye[2]);}
static int node_depth(const mech3d *m,int node)
{int depth,k;for(depth=0;depth<MECH3D_MAX_NODES;depth++){int next=-1;for(k=0;k<m->skel_count;k++)if(m->skel_index[k]==node)next=m->skel_parent[k];if(next<0||next==node)break;node=next;}return depth+1;}
static void archive_poses(const char *path)
{
    char error[256];prj_archive *a=prj_open(path,error,sizeof error);int bt,i,total=0;
    CHECK(a!=NULL);bt=prj_find_type(a,"BWD");CHECK(bt>=0);
    for(i=0;i<prj_symbol_count(a,bt);++i) {
        const char *name=prj_symbol_name(a,bt,i);prj_record r;bwd_chunk *chunks;int nc,k,n=0,node[2],dos,has_skeleton=0,unit_class=-1;
        if(prj_read_named(a,"BWD",name,&r)!=PRJ_OK)continue;
        nc=bwd_chunks(r.data,r.size,NULL,0);if(nc<=0){prj_record_free(&r);continue;}
        chunks=calloc((size_t)nc,sizeof *chunks);CHECK(chunks!=NULL);bwd_chunks(r.data,r.size,chunks,nc);
        for(k=0;k<nc;k++) {
            if(!strcmp(chunks[k].tag,"DTBL"))has_skeleton=1;
            if(!strcmp(chunks[k].tag,"GP")&&chunks[k].size>=4)unit_class=chunks[k].data[2]|chunks[k].data[3]<<8;
            if(n<2&&!strcmp(chunks[k].tag,"THNG")&&chunks[k].size>=2)node[n++]=(int16_t)(chunks[k].data[0]|chunks[k].data[1]<<8);
        }
        free(chunks);prj_record_free(&r);if(n<2||!has_skeleton||unit_class!=1)continue;
        for(dos=0;dos<2;dos++) {
            mech3d full={0},pit={0};float oldr[9],old_eye[3],base_points[MECH3D_MAX_NODES][3];int angle;
            { int loaded=mech3d_load(a,name,0,&full);if(loaded)fprintf(stderr,"fall archive model failed: %s\n",name);CHECK(!loaded); }
            CHECK(full.fall_node[0]==node[0]&&full.fall_node[1]==node[1]);
            if(mech3d_load(a,name,4,&pit)){mech3d_free(&full);continue;}
            mech3d_use_skeleton(&pit,&full);mech3d_dos_hips=dos;
            full.heading=pit.heading=27;full.twist=pit.twist=20;
            full.origin[0]=pit.origin[0]=1000;full.origin[2]=pit.origin[2]=3000;
            mech3d_pose(&full,NULL,0);mech3d_pose(&pit,NULL,0);
            if(!pit.eye_ok||pit.part_count>MECH3D_MAX_NODES){mech3d_free(&full);mech3d_free(&pit);continue;}
            CHECK(!memcmp(pit.eye_pos,full.eye_pos,sizeof pit.eye_pos));
            memcpy(old_eye,pit.eye_pos,sizeof old_eye);mech3d_fall_camera(&pit,112,12,oldr); /* 65-degree sideways glance */
            for(k=0;k<pit.part_count;k++)camera_point(oldr,old_eye,pit.parts[k].pos,base_points[k]);
            for(angle=0;angle<=75;angle+=15) {
                int32_t c,d[3];float camera[9];int j;
                mech3d_fall_geometry(0,angle*65536,full.centre_h,27*65536,&c,d);
                full.fall_active=pit.fall_active=1;full.fall_pitch=pit.fall_pitch=angle*65536;full.fall_center=pit.fall_center=c;
                full.origin[0]=pit.origin[0]=1000+d[0];full.origin[2]=pit.origin[2]=3000+d[2];
                mech3d_pose(&full,NULL,0);mech3d_pose(&pit,NULL,0);
                CHECK(!memcmp(pit.eye_pos,full.eye_pos,sizeof pit.eye_pos));
                mech3d_fall_camera(&pit,112,12,camera);
                for(k=0;k<pit.part_count;k++) {
                    float now[3];camera_point(camera,pit.eye_pos,pit.parts[k].pos,now);
                    /* DOS rounds each ancestor position to whole cm.
                     * Each endpoint can differ by <sqrt(3)/2 per ancestor
                     * at each of the two orientations; preserve that bound. */
                    float bound=dos?1.733f*(float)(node_depth(&pit,pit.parts[k].node)+node_depth(&pit,pit.eye_node)):2.0f;
                    for(j=0;j<3;j++)CHECK(fabsf(now[j]-base_points[k][j])<bound);
                    /* Every shared cockpit part stays on the full source
                     * skeleton through whole-body pitch at both hip modes. */
                    for(j=0;j<full.part_count;j++)if(full.parts[j].node==pit.parts[k].node) {
                        CHECK(!memcmp(full.parts[j].pos,pit.parts[k].pos,sizeof pit.parts[k].pos));
                        CHECK(!memcmp(full.parts[j].rot,pit.parts[k].rot,sizeof pit.parts[k].rot));break;
                    }
                }
            }
            mech3d_free(&full);mech3d_free(&pit);++total;
        }
    }
    prj_close(a);CHECK(total>20);printf("PASS %d archive cockpit poses: six fall pitches, sideways glance, both placement modes\n",total);
}
int main(int argc,char **argv)
{
    original_frames();simulation_adapter();if(argc>1)archive_poses(argv[1]);
    printf("PASS Mercs fall: %d checks against original frames, contacts, center arc and cockpit pose\n",checks);return 0;
}
