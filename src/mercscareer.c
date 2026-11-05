/* Bounded retail DOS MW2SHELL mech ownership/return adapter.
 * Original entry points and execution goldens: MERCS_CAREER_CONSUMER_AUDIT.md.
 * The native bounds checks are deliberate; the original parser was unchecked.
 */
#include "mercscareer.h"
#include "mercscareer_data.h"
#include <limits.h>
#include <string.h>

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static int32_t i32(const uint8_t *p)
{ uint32_t v=u32(p); return v<=INT32_MAX?(int32_t)v:-(int32_t)(~v)-1; }
static void w16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void w32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static int r(const mercs_career_mech *m,size_t p) { return i32(m->bytes+p); }
static void w(mercs_career_mech *m,size_t p,int v) { w32(m->bytes+p,(uint32_t)v); }
static int32_t low_signed16(int32_t v)
{ uint32_t low=(uint32_t)v&65535;return low<=INT16_MAX?(int32_t)low:(int32_t)low-65536; }
static size_t zlen(const char *s,size_t bound)
{ size_t n=0; if(!s)return bound; while(n<bound&&s[n])++n; return n; }
static int filename_ok(const char *s)
{
    size_t i,n=zlen(s,MERCS_CAREER_FILENAME_BYTES);
    if(n<3||n>=MERCS_CAREER_FILENAME_BYTES)return 0;
    for(i=0;i<n;i++) {
        unsigned char c=(unsigned char)s[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'))return 0;
    }
    return s[0]!='.';
}
static int chassis_index(const char *filename)
{
    size_t i,j;
    for(i=0;i<sizeof career_chassis/sizeof career_chassis[0];i++) {
        for(j=0;j<3;j++) {
            unsigned char c=(unsigned char)filename[j];
            if(c>='A'&&c<='Z')c=(unsigned char)(c+32);
            if(c!=(unsigned char)career_chassis[i].prefix[j])break;
        }
        if(j==3)return (int)i;
    }
    return -1;
}

/* DOS 0x583ac, 0x5849c, 0x5858c, 0x587e4, 0x58558.
 * The editor-global scratch affected by excessive jump-jet removal is outside
 * this pure record API. The passed record retains its critical slots exactly.
 */
static int recalculate(mercs_career_mech *m)
{
    int i,e=r(m,0x20c),tons=r(m,0x200),walk,jump,gyro,weight=0,ammo=0;
    int64_t total;
    if(e<0||e%10000>=79||e>=20000||tons<1000||tons>10000)return MERCS_CAREER_INVALID;
    for(i=0;i<14;i++) {
        int id=r(m,0x2f0+8*i);
        if(id==-1)continue;
        if(id<0||id/100>=66)return MERCS_CAREER_UNSUPPORTED;
        weight+=career_weapon_weight[id/100];
    }
    for(i=0;i<25;i++)if(r(m,0x28c+4*i)!=-1)ammo+=100;
    w(m,0x210,career_engine[e%10000][0]);
    w(m,0x214,career_engine[e%10000][1]/(e>=10000?2:1));
    w(m,0x218,e>=10000?7:0);w(m,0x21c,tons-r(m,0x214));
    walk=r(m,0x210)*100/tons;
    jump=r(m,0x288);if(jump<0||jump>1000)return MERCS_CAREER_UNSUPPORTED;
    if(jump>walk)jump=walk;
    w(m,0x280,walk);w(m,0x284,(walk*3+2)/2);w(m,0x288,jump);
    w(m,0x234,tons<=5500?50:tons<=8500?100:200);
    w(m,0x238,r(m,0x234)*jump);w(m,0x260,weight);w(m,0x268,ammo);
    /* Original 53-bit x87 products can truncate just below the rating, but
     * rounding to100 yields this same final weight for all79 engine rows.
     * Integer arithmetic avoids a dependency on host long-double precision. */
    gyro=r(m,0x210);
    if(gyro%100)gyro=gyro+100-gyro%100;
    w(m,0x220,gyro);w(m,0x224,300);
    w(m,0x244,tons/(r(m,0x240)?20:10));
    total=(int64_t)r(m,0x214)+gyro+300+r(m,0x22c)+r(m,0x238)+
          r(m,0x244)+r(m,0x250)+weight+ammo+r(m,0x270);
    if(total<INT32_MIN||total>INT32_MAX||tons-total<INT32_MIN||tons-total>INT32_MAX)
        return MERCS_CAREER_INVALID;
    w(m,0x204,(int)total);w(m,0x208,tons-(int)total);
    return MERCS_CAREER_OK;
}

static int raw_count(const uint8_t *mek,unsigned id,int only_location)
{
    int l,s,n=0;
    for(l=0;l<8;l++) {
        const uint8_t *p=mek+24+40*l;
        if(only_location>=0&&l!=only_location)continue;
        for(s=0;s<u16(p+36);s++)if(u16(p+12+2*s)==id)++n;
    }
    return n;
}

/* Original loader 0x58e94, with exact size/table bounds established first. */
static int load_mech(mercs_career_mech *m,const uint8_t *mek,size_t length,
                     const char *name,uint32_t identity,int32_t association)
{
    uint32_t weapons,ammo,tons,walk,jump,heat;
    size_t config_at,n;
    int i,j,ci,engine,armor=0,armor_weight,hs_count,rc;
    int raw_ammo[25]={0};
    if(!m||!mek||length<394||!filename_ok(name))return MERCS_CAREER_INVALID;
    weapons=u32(mek+16);ammo=u32(mek+20);
    if(weapons>14||ammo>25)return MERCS_CAREER_UNSUPPORTED;
    config_at=344+(size_t)(weapons+ammo)*8;
    if(length!=config_at+50&&length!=config_at+58)return MERCS_CAREER_INVALID;
    n=zlen((const char *)mek+config_at,30);
    if(n==30)return MERCS_CAREER_UNSUPPORTED;
    ci=chassis_index(name);
    /* AeroTech support uses the same original0x58e94/0x59c04 conversion.
     * It remains a temporary deployment, not an owned roster mech. */
    if(ci<0||career_chassis[ci].kind>2)return MERCS_CAREER_UNSUPPORTED;
    tons=u32(mek);walk=u32(mek+4);jump=u32(mek+8);heat=u32(mek+12);
    if(tons<10||tons>100||tons%5||walk>1000||jump>1000||heat>1000)
        return MERCS_CAREER_UNSUPPORTED;
    for(i=0;i<8;i++) {
        const uint8_t *p=mek+24+40*i;
        if(u16(p+36)>12)return MERCS_CAREER_INVALID;
        for(j=0;j<3;j++)if(i32(p+4*j)<INT16_MIN||i32(p+4*j)>INT16_MAX)
            return MERCS_CAREER_UNSUPPORTED;
        armor+=i32(p)+i32(p+4);
    }
    for(i=0;i<(int)weapons;i++) {
        int id=i32(mek+344+8*i);
        if(id!=-1&&(id<0||id/100>=66))return MERCS_CAREER_UNSUPPORTED;
    }
    for(i=0;i<(int)ammo;i++) {
        const uint8_t *p=mek+344+8*(weapons+i);
        if(u32(p)>65535||i32(p+4)<0||i32(p+4)>=6600)return MERCS_CAREER_UNSUPPORTED;
    }
    memset(m,0,sizeof *m);
    memcpy(m->bytes,career_chassis[ci].name,strlen(career_chassis[ci].name)+1);
    memcpy(m->bytes+0x100,mek+config_at,n+1);
    memcpy(m->bytes+0x81c,name,3);memcpy(m->bytes+0x823,name,strlen(name)+1);
    for(i=0;i<78;i++)w(m,0x4f0+8*i,-1);
    for(i=0;i<14;i++) {
        if(i<(int)weapons)memcpy(m->bytes+0x2f0+8*i,mek+344+8*i,8);
        else w(m,0x2f0+8*i,-1);
    }
    for(i=0;i<25;i++)w(m,0x28c+4*i,-1);
    /* Original 0x599c4 scans still-zero criticals during this loader phase.
     * Consequently same-type bins receive the same internal ID, a retained
     * original behavior, even though deployment assigns separate raw IDs. */
    for(i=0;i<(int)ammo;i++) {
        const uint8_t *p=mek+344+8*(weapons+i);
        raw_ammo[i]=i32(p);w(m,0x28c+4*i,i32(p+4)*100+10001);
    }
    for(i=0;i<8;i++) {
        const uint8_t *p=mek+24+40*i;
        int col=i==2?0:(i==1||i==3)?1:(i==4||i==5)?2:3;
        for(j=0;j<12;j++) {
            int v=-1,k;
            if(j<u16(p+36)) {
                v=u16(p+12+2*j);
                if(v>9999)for(k=0;k<25;k++)if(raw_ammo[k]==v){v=r(m,0x28c+4*k);break;}
            }
            w(m,0x368+48*i+4*j,v);
        }
        w(m,0x760+18*i,i32(p+8));
        w(m,0x764+18*i,i==0?9:career_internal[(tons-10)/5][col]*2);
        w(m,0x768+18*i,i32(p));
        w(m,0x76c+18*i,(i>=1&&i<=3)?i32(p+4):-1);
        w16(m->bytes+0x770+18*i,u16(p+38));
    }
    w(m,0x200,(int)tons*100);w(m,0x280,(int)walk);w(m,0x288,(int)jump);
    w(m,0x22c,((int)heat-10)*100);
    engine=((int)tons*(int)walk*5+4)/5;
    if(engine<10)engine=10;
    if(engine>400)engine=400;
    w(m,0x20c,(engine-10)/5+(raw_count(mek,7850,1)?10000:0));
    w(m,0x240,raw_count(mek,9001,-1)?1:0);
    w(m,0x24c,raw_count(mek,9501,-1)?1:0);
    w(m,0x258,armor);w(m,0x25c,armor);
    /* Within the validated sum of signed16 location values, these signed
     * ratios give the same final rounded weight as original53-bit x87.
     * Intermediate truncations can differ. See numeric-slice goldens. */
    armor_weight=(int)((int64_t)armor*(r(m,0x24c)?125:25)/(r(m,0x24c)?24:4));
    armor_weight=armor_weight*2+50;
    armor_weight=(armor_weight-armor_weight%100)/2;
    w(m,0x250,armor_weight);
    w(m,0x808,(((int)tons*100/25)+50)/100);
    if(raw_count(mek,7001,-1)){w(m,0x270,r(m,0x808)*100);w(m,0x7f4,1);}
    rc=recalculate(m);if(rc)return rc;
    hs_count=raw_count(mek,8001,-1);
    if(hs_count)w(m,0x228,hs_count>1?2:1);
    else w(m,0x228,r(m,0x204)>r(m,0x200)&&heat>19&&!(heat&1)?2:1);
    if(r(m,0x228)==2){w(m,0x22c,r(m,0x22c)/2-500);rc=recalculate(m);if(rc)return rc;}
    w(m,0x228,r(m,0x228)-1);
    /* 0x582c0 is the external heat-sink count updater. Its old value is -1
     * here, so no editor-global critical-list operation is executed. */
    hs_count=r(m,0x22c)/100+10-r(m,0x210)/25;
    w(m,0x230,hs_count<0?0:hs_count);
    w(m,0x80c,association);w32(m->bytes+0x810,identity);
    m->bytes[0x84f]=(uint8_t)career_chassis[ci].kind;
    return MERCS_CAREER_OK;
}

int mercs_career_pair_init(mercs_career_mech_pair *out,const uint8_t *mek,size_t length,
                          const char *name,uint32_t identity,int32_t association)
{
    mercs_career_mech_pair next;int rc;
    if(!out)return MERCS_CAREER_INVALID;
    rc=load_mech(&next.current,mek,length,name,identity,association);
    if(rc)return rc;
    next.reference=next.current;*out=next;return MERCS_CAREER_OK;
}

int mercs_career_import_return(mercs_career_mech_pair *pair,int restore,
                              const uint8_t *mek,size_t length,
                              mercs_career_import_result *result)
{
    mercs_career_mech returned,next;
    mercs_career_import_result status;
    int rc;
    if(!pair||!result)return MERCS_CAREER_INVALID;
    memset(&status,0,sizeof status);
    if(mercs_career_filename(&pair->current,status.filename,sizeof status.filename))
        return MERCS_CAREER_INVALID;
    next=pair->current;
    if(restore==1)status.action=MERCS_CAREER_IMPORT_RESTORE;
    else if(next.bytes[0x84b]!=1) { *result=status;return MERCS_CAREER_OK; }
    else if(!mek) {
        if(length)return MERCS_CAREER_INVALID;
        status.action=MERCS_CAREER_IMPORT_MISSING;
    } else {
        rc=load_mech(&returned,mek,length,status.filename,0,-1);
        if(rc)return rc;
        memcpy(next.bytes+0x28c,returned.bytes+0x28c,25*4);
        memcpy(next.bytes+0x2f0,returned.bytes+0x2f0,14*8);
        memcpy(next.bytes+0x368,returned.bytes+0x368,8*12*4);
        memcpy(next.bytes+0x760,returned.bytes+0x760,8*18);
        memcpy(next.bytes+0x200,returned.bytes+0x200,12);
        memcpy(next.bytes+0x220,returned.bytes+0x220,0x60);
        /* Original DOS/Windows selective merge copies endo into ferro. */
        w(&next,0x24c,r(&returned,0x240));
        rc=recalculate(&next);if(rc)return rc;
        status.action=MERCS_CAREER_IMPORT_APPLIED;status.request_delete=1;
    }
    next.bytes[0x84b]=0;pair->current=next;*result=status;
    return MERCS_CAREER_OK;
}

int mercs_career_deployment_bytes(const mercs_career_mech *m,uint8_t *out,
                                 size_t capacity,size_t *written)
{
    uint8_t bytes[MERCS_CAREER_DEPLOY_MAX_BYTES]={0};
    uint8_t weapons[14*8]={0},ammo[25*8]={0};
    int i,j,k,nw=0,na=0,tons,heat,sink;size_t size,at,n;
    if(!m||!out||!written)return MERCS_CAREER_INVALID;
    if(!filename_ok((const char *)m->bytes+0x823))return MERCS_CAREER_INVALID;
    n=zlen((const char *)m->bytes+0x100,30);if(n==30)return MERCS_CAREER_UNSUPPORTED;
    tons=r(m,0x200);sink=r(m,0x228);
    if(tons<1000||tons>10000||r(m,0x280)<0||r(m,0x280)>40||
       r(m,0x288)<0||r(m,0x288)>40||sink<0||sink>1||
       r(m,0x22c)<-1000||r(m,0x22c)>100000)return MERCS_CAREER_UNSUPPORTED;
    for(i=0;i<14;i++)if(r(m,0x2f0+8*i)!=-1) {
        int id=r(m,0x2f0+8*i);
        if(id<0||id/100>=66)return MERCS_CAREER_UNSUPPORTED;
        memcpy(weapons+8*nw++,m->bytes+0x2f0+8*i,8);
    }
    for(i=0;i<25;i++)if(r(m,0x28c+4*i)!=-1) {
        int id=r(m,0x28c+4*i),raw;
        if(id<10000||id>=670000)return MERCS_CAREER_UNSUPPORTED;
        raw=((id-10000)/10000)*100+10001;
        for(j=0;j<25&&i32(ammo+8*j)!=0;j++)if(i32(ammo+8*j)==raw)++raw;
        w32(ammo+8*na,(uint32_t)raw);w32(ammo+8*na+4,(uint32_t)((id-10000)/100));++na;
    }
    for(i=0;i<8;i++) {
        uint8_t *p=bytes+24+40*i;
        unsigned flags=u16(m->bytes+0x770+18*i);
        w16(p+38,(uint16_t)(flags|(m->bytes[0x84f]==0?0x100:0)));
        w16(p+36,(uint16_t)(i==0||i>=6?6:12));
        for(j=0;j<12;j++) {
            int id=r(m,0x368+48*i+4*j),raw=0;
            if(id!=-1&&id<10000)raw=id;
            else if(id>=10000)for(k=0;k<25;k++)if(r(m,0x28c+4*k)==id){raw=i32(ammo+8*k);break;}
            w16(p+12+2*j,(uint16_t)raw);
        }
        /* Original writer deliberately sign-extends the low 16 bits. */
        w32(p,(uint32_t)low_signed16(r(m,0x768+18*i)));
        w32(p+8,(uint32_t)low_signed16(r(m,0x760+18*i)));
        if(i>=1&&i<=3)w32(p+4,(uint32_t)low_signed16(r(m,0x76c+18*i)));
    }
    heat=((r(m,0x22c)+50)/100+10)*(sink+1);
    w32(bytes,(uint32_t)((tons+50)/100));w32(bytes+4,(uint32_t)r(m,0x280));
    w32(bytes+8,(uint32_t)r(m,0x288));w32(bytes+12,(uint32_t)heat);
    w32(bytes+16,(uint32_t)nw);w32(bytes+20,(uint32_t)na);
    at=344;memcpy(bytes+at,weapons,(size_t)nw*8);at+=(size_t)nw*8;
    memcpy(bytes+at,ammo,(size_t)na*8);at+=(size_t)na*8;
    memcpy(bytes+at,m->bytes+0x100,n+1);w32(bytes+at+34,(uint32_t)sink);
    size=at+58;
    if(capacity<size)return MERCS_CAREER_NO_SPACE;
    memcpy(out,bytes,size);*written=size;return MERCS_CAREER_OK;
}

void mercs_career_set_deployed(mercs_career_mech *m,int deployed)
{ if(m)m->bytes[0x84b]=(uint8_t)(deployed?1:0); }
int mercs_career_deployed(const mercs_career_mech *m)
{ return m&&m->bytes[0x84b]==1; }
int mercs_career_filename(const mercs_career_mech *m,char *out,size_t capacity)
{
    size_t n;
    if(!m||!out)return MERCS_CAREER_INVALID;
    n=zlen((const char *)m->bytes+0x823,32);if(n==32)return MERCS_CAREER_INVALID;
    if(n+1>capacity)return MERCS_CAREER_NO_SPACE;
    memcpy(out,m->bytes+0x823,n+1);return MERCS_CAREER_OK;
}
uint32_t mercs_career_instance_id(const mercs_career_mech *m)
{ return m?u32(m->bytes+0x810):0; }
int32_t mercs_career_association_id(const mercs_career_mech *m)
{ return m?r(m,0x80c):-1; }
void mercs_career_set_association(mercs_career_mech *m,int32_t id)
{ if(m)w(m,0x80c,id); }
void mercs_career_set_acquisition(mercs_career_mech *m,int32_t value,int32_t year,int32_t threshold)
{ if(m){w(m,0x818,value);w(m,0x843,year);w(m,0x847,threshold);} }
int mercs_career_mech_lost(const mercs_career_mech *m)
{
    return m&&((r(m,0x760)==0&&r(m,0x768)==0&&r(m,0x76c)==0)||
               (r(m,0x784)==0&&r(m,0x78c)==0&&r(m,0x790)==0));
}
