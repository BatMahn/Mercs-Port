/* Controller placement comparisons against executed Mercs DOS/Windows tails,
 * followed by complete resource-model/representation pose checks. */
#include "mech3d.h"
#include "bwdplace.h"
#include "bwd.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../tests/fixtures/pose-contact/original-pose.h"
#include "../tests/fixtures/pose-contact/original-camera.h"

static unsigned checks;
#define CHECK(c) do { ++checks; if(!(c)){fprintf(stderr,"pose check failed: %s:%d: %s\n",__FILE__,__LINE__,#c);abort();} } while(0)

static void original_cases(void)
{
    size_t i;int k;
    for(i=0;i<sizeof original_pose/sizeof original_pose[0];i++) {
        bwd_transform t;int32_t d[3],p=original_pose[i].pitch;
        for(k=0;k<3;k++)d[k]=original_pose[i].endpoint[k]-original_pose[i].arm_position[k];
        if(original_pose[i].nodes&2)CHECK(mech3d_arm_pitch(p,d)==original_pose[i].arm_pitch);
        if(original_pose[i].nodes&1) {
            bwd_transform_torso(&t,original_pose[i].torso[0],original_pose[i].torso[1]);
            for(k=0;k<9;k++)CHECK(t.m[k]==original_pose[i].matrices[0][k]);
        }
    }
    /* Native protection for a point exactly on the arm, which has no
     * direction and would make the original integer division fault. */
    {int32_t d[3]={0};CHECK(mech3d_arm_pitch(-1,d)==1);}
}

static void original_cameras_check(void)
{
    size_t i;int k;
    for(i=0;i<sizeof original_cameras/sizeof original_cameras[0];i++) {
        bwd_transform t;int32_t a[3],point[3]={0,original_cameras[i].offset,0};
        mech3d m={0};
        memcpy(t.m,original_cameras[i].matrix,sizeof t.m);
        bwd_transform_angles(&t,a);
        if(original_cameras[i].mode==1)
            for(k=0;k<3;k++)CHECK(a[k]==original_cameras[i].camera[k]);
        if(original_cameras[i].mode==2)continue;
        bwd_transform_point(&t,point);
        for(k=0;k<3;k++)CHECK(point[k]==original_cameras[i].camera[3+k]);
        m.eye_ok=1;m.fall_active=original_cameras[i].fall;m.fall_pitch=original_cameras[i].body_pitch;
        m.heading=original_cameras[i].body_yaw/65536.0f;m.twist=original_cameras[i].twist/65536.0f;
        for(k=0;k<9;k++)m.eye_rot[k]=(float)t.m[k]/536870912.0f;
        if(original_cameras[i].mode==0) {
            CHECK(mech3d_cockpit_angles(&m,-original_cameras[i].pitch/65536.0f,a)==0);
            for(k=0;k<3;k++)CHECK(abs(a[k]-original_cameras[i].camera[k])<=2);
        }
        CHECK(mech3d_aim_direction(&m,-original_cameras[i].pitch/65536.0f,a)==0);
        for(k=0;k<3;k++) {
            int32_t origin=original_cameras[i].matrix[9+k]+(k==1?original_cameras[i].offset:0);
            int32_t original_endpoint=origin+(int32_t)(((int64_t)original_cameras[i].direction[k]*50000+32768)>>16);
            CHECK(abs(a[k]-original_cameras[i].direction[k])<=2);
            CHECK(original_endpoint==original_cameras[i].reticle[k]);
        }
    }
}

static void same_pose(const mech3d *full,const mech3d *other,const char *name,int repr,int state)
{
    int i,j,k;
    if(!full->eye_ok||!other->eye_ok)fprintf(stderr,"missing eye %s repr%d state%d\n",name,repr,state);
    CHECK(full->eye_ok&&other->eye_ok);
    CHECK(!memcmp(full->eye_pos,other->eye_pos,sizeof full->eye_pos));
    CHECK(!memcmp(full->eye_rot,other->eye_rot,sizeof full->eye_rot));
    CHECK(full->mount_ok==other->mount_ok);
    for(k=0;k<8;k++)if(full->mount_ok&(1u<<k)) {
        CHECK(!memcmp(full->mount_pos[k],other->mount_pos[k],sizeof full->mount_pos[k]));
        CHECK(!memcmp(full->mount_rot[k],other->mount_rot[k],sizeof full->mount_rot[k]));
    }
    for(i=0;i<other->part_count;i++) {
        for(k=0;k<9;k++)CHECK(isfinite(other->parts[i].rot[k]));
        for(j=0;j<full->part_count;j++)if(full->parts[j].node==other->parts[i].node) {
            CHECK(!memcmp(full->parts[j].pos,other->parts[i].pos,sizeof full->parts[j].pos));
            CHECK(!memcmp(full->parts[j].rot,other->parts[i].rot,sizeof full->parts[j].rot));
            break;
        }
    }
}

static void archive(const char *path)
{
    char error[256];prj_archive *a=prj_open(path,error,sizeof error);int bt,n,total=0,poses=0;
    CHECK(a!=NULL);bt=prj_find_type(a,"BWD");CHECK(bt>=0);
    for(n=0;n<prj_symbol_count(a,bt);n++) {
        const char *name=prj_symbol_name(a,bt,n);prj_record r;bwd_chunk *chunks;
        int count,i,reprs=0,eye=-1,torso=-1,has_anim=0,mode,state,j;
        mech3d models[5]={{0}};anim_set anim={0};
        if(prj_read_named(a,"BWD",name,&r)!=PRJ_OK)continue;
        count=bwd_chunks(r.data,r.size,NULL,0);
        if(count<=0){prj_record_free(&r);continue;}
        chunks=calloc((size_t)count,sizeof *chunks);CHECK(chunks!=NULL);
        bwd_chunks(r.data,r.size,chunks,count);
        for(i=0;i<count;i++) {
            if(!strcmp(chunks[i].tag,"REPR"))reprs++;
            if(!strcmp(chunks[i].tag,"EYEO")&&chunks[i].size>=2)eye=(int16_t)(chunks[i].data[0]|chunks[i].data[1]<<8);
            if(!strcmp(chunks[i].tag,"THNG")&&chunks[i].size>=2&&torso<0)torso=(int16_t)(chunks[i].data[0]|chunks[i].data[1]<<8);
        }
        free(chunks);prj_record_free(&r);
        if(reprs<5||eye<0)continue;
        for(i=0;i<5;i++) {
            CHECK(mech3d_load(a,name,i,&models[i])==0);
            if(i)mech3d_use_skeleton(&models[i],&models[0]);
            CHECK(models[i].twist_node==torso);
        }
        if(models[0].anim_id>0)has_anim=anim_load_id(a,models[0].anim_id,&anim)==0;
        for(mode=0;mode<2;mode++)for(state=0;state<72;state++) {
            static const int fall_degrees[]={0,45,90,-90};
            mech3d *m=&models[0];int pitch=state%3,twist=(state/3)%3,fall=(state/9)%4;
            float time=(float)((state/36)*6);
            int32_t delta[3],center;
            mech3d_dos_hips=mode;
            m->heading=27.0f;m->twist=(float)(twist-1)*45.0f;
            m->fall_active=fall!=0;m->fall_pitch=fall_degrees[fall]*65536;
            mech3d_fall_geometry(0,m->fall_pitch,m->centre_h,27*65536,&center,delta);
            m->fall_center=center;m->origin[0]=1000+delta[0];m->origin[1]=250;m->origin[2]=3000+delta[2];
            mech3d_set_aim(m,(float)(pitch-1)*40.0f,(state&1)?2000:50000);
            mech3d_pose(m,has_anim?&anim:NULL,time);
            for(j=1;j<5;j++) {
                mech3d_copy_pose(&models[j],m);
                mech3d_pose(&models[j],has_anim?&anim:NULL,time);
                same_pose(m,&models[j],name,j,state);
            }
            poses++;
        }
        for(i=0;i<5;i++)mech3d_free(&models[i]);
        total++;
    }
    prj_close(a);if(total!=65)fprintf(stderr,"archive model count %d\n",total);CHECK(total==65);
    printf("PASS %s: %d models, five representations, %d poses (aim, twist, fall, animation, both hip modes)\n",path,total,poses);
}

int main(int argc,char **argv)
{
    int i;original_cases();original_cameras_check();for(i=1;i<argc;i++)archive(argv[i]);
    printf("PASS Mercs controller pose: %u checks\n",checks);return 0;
}
