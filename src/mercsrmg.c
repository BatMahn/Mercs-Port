/* Mercenaries DOS shell procedural mission generator, recovered from
 * 0x201b0, 0x21508, 0x219f8, 0x21f88 and 0x22158. See the RMG audit and
 * original-instruction corpus. No commercial executable/assets are embedded. */
#include "mercsrmg.h"
#include "datapath.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct { unsigned char *exe; } catalog;
typedef struct { unsigned char bytes[16384];size_t size;uint32_t largest;int bad; } builder;
typedef struct { char name[11];int32_t width,height;int required; } area;
typedef struct { uint32_t value;int invalid; } random_state;
static uint16_t u16(const uint8_t *p){return (uint16_t)(p[0]|p[1]<<8);}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void w16(uint8_t *p,uint16_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void w32(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}
static int fail(char *e,size_t n,const char *fmt,...)
{va_list a;if(e&&n){va_start(a,fmt);vsnprintf(e,n,fmt,a);va_end(a);}return -1;}

/* Exact retail image identity prevents silently applying one executable's
 * offsets to another version. All accepted data lies in its mapped data
 * object; the pointers there are object-relative LE fixups. */
static int catalog_open(catalog *c,char *e,size_t n)
{
    size_t i,size;uint64_t h=UINT64_C(14695981039346656037);unsigned char *raw=NULL;
    memset(c,0,sizeof *c);
    if(dp_read_file("MW2SHELL.EXE",&raw,&size)&&dp_read_file("MERCS/MW2SHELL.EXE",&raw,&size))
        return fail(e,n,"Procedural contracts need the original DOS MW2SHELL.EXE and CD/RMG data in the selected game folder.");
    if(size==962175)for(i=0;i<size;i++)h=(h^raw[i])*UINT64_C(1099511628211);
    if(size!=962175||h!=UINT64_C(0x5c4bc66f9129d76c)||memcmp(raw,"MZ",2)||memcmp(raw+0x2e10,"LE",2)){
        free(raw);return fail(e,n,"The DOS shell's procedural data tables are an unrecognized version (expected the supplied retail DOS MW2SHELL.EXE).");
    }
    c->exe=raw;return 0;
}
static const uint8_t *data(const catalog *c,uint32_t va){return c->exe+va+0x25600u;}
static const char *str(const catalog *c,uint32_t va){return (const char *)data(c,va);}
static const char *ptr(const catalog *c,uint32_t va){return str(c,0xa0000u+u32(data(c,va)));}

int mercs_rmg_available(char *e,size_t n)
{
    catalog c;unsigned char *b;size_t size;
    if(catalog_open(&c,e,n))return -1;
    free(c.exe);
    if(dp_read_file("RMG/MIS0SCN1.BWD",&b,&size))return fail(e,n,"The original CD/RMG mission templates are missing from the selected game data.");
    free(b);return 0;
}

static unsigned draw(random_state *s)
{s->value=s->value*UINT32_C(1103515245)+12345u;return (s->value>>16)&32767u;}
static int range(random_state *s,int low,int high)
{
    /* Original runtime CW=0x127f selects 53-bit x87 arithmetic. Keep each
     * operation rounded to binary64 on Intel and ARM (long double differs).
     * rand()==32767 can produce high+1; callers bound-check those original
     * exceptional draws instead of indexing past an asset table. */
    volatile double v=(double)draw(s)*(1.0/32767.0);
    v=v*(high-low+1);v=v+low;
    if((int)v<low||(int)v>high){s->invalid=1;return low;}
    return (int)v;
}
static uint8_t *chunk(builder *b,const char tag[4],size_t size)
{
    uint8_t *p;if(size<8||size>sizeof b->bytes-b->size){b->bad=1;return NULL;}
    p=b->bytes+b->size;memset(p,0,size);memcpy(p,tag,4);w32(p+4,(uint32_t)size);
    b->size+=size;if(size>b->largest)b->largest=(uint32_t)size;return p;
}
static void begin(builder *b)
{
    uint8_t *p;memset(b,0,sizeof *b);b->size=12;memcpy(b->bytes,"BWD\0",4);
    p=chunk(b,"REV\0",12);memcpy(p+8,"1.22",4);chunk(b,"DTBL",28);
    /* Original max-chunk counter is reset after the common file prefix. */
    b->largest=12;
}
static int add(mercs_rmg_bundle *b,const char *name,const char *tag,const void *bytes,size_t size)
{
    prj_record *r;size_t i;
    if(b->count>=MERCS_RMG_MAX_FILES||!name[0]||strlen(name)>16||size>8u*1024u*1024u)return -1;
    for(i=0;i<b->count;i++)if(!strcasecmp(b->files[i].name,name)&&!strcmp(b->files[i].tag,tag))return -1;
    r=&b->files[b->count];memset(r,0,sizeof *r);r->data=malloc(size?size:1);if(!r->data)return -1;
    memcpy(r->data,bytes,size);r->size=size;r->generated=1;
    for(i=0;name[i];i++)r->name[i]=(char)toupper((unsigned char)name[i]);
    snprintf(r->tag,sizeof r->tag,"%s",tag);snprintf(r->filename,sizeof r->filename,"%s.%s",name,tag);
    b->count++;return 0;
}
static int finish(mercs_rmg_bundle *out,builder *b,const char *name)
{
    if(b->bad)return -1;
    if(b->largest>out->writer_max_chunk)out->writer_max_chunk=b->largest;
    w32(b->bytes+4,(uint32_t)b->size);w32(b->bytes+8,out->writer_max_chunk);
    return add(out,name,"BWD",b->bytes,b->size);
}
static int incl(builder *b,const char *name,int id)
{
    size_t len=strlen(name),size=(len+14u)&~3u;uint8_t *p=chunk(b,"INCL",size);
    if(!p)return -1;
    w16(p+8,(uint16_t)id);memcpy(p+10,name,len+1);return 0;
}
static void block(builder *b,int x,int z)
{
    uint8_t *p=chunk(b,"BLKX",44);if(!p)return;
    w32(p+8,1);w32(p+12,1);w32(p+16,1);w32(p+32,(uint32_t)x);w32(p+40,(uint32_t)z);
    p=chunk(b,"BLK\0",32);if(!p)return;w32(p+20,1);w32(p+24,1);w32(p+28,1);
}
static int copy_template(mercs_rmg_bundle *out,const char *source,const char *dest,char *e,size_t n)
{
    unsigned char *raw;size_t size;char path[64];int rc;
    snprintf(path,sizeof path,"RMG/%s.BWD",source);
    if(dp_read_file(path,&raw,&size))return fail(e,n,"Missing original RMG template %s",path);
    if(size<12||memcmp(raw,"BWD\0",4)||u32(raw+4)!=size){free(raw);return fail(e,n,"Invalid original RMG template %s",path);}
    rc=add(out,dest,"BWD",raw,size);free(raw);return rc;
}
static int place(const area *a,int r[4],int *x,int *z)
{
    int i;
    for(i=0;i<64;i++){
        int oldx,oldz,right,top;
        r[3]=r[1]+a->height;
        if(r[3]>125000){int dz=r[0]-r[2]+25000;r[3]+=-125000-r[1];r[1]=-125000;r[2]+=dz;r[0]=r[2];}
        oldx=r[1];oldz=r[2];right=r[3];top=r[2]+a->height;if(top>r[0])r[0]=top;
        *x=oldx+a->width/2;*z=oldz+a->height/2;r[1]+=a->width+25000;r[3]+=a->width+25000;
        if(!(oldx<0&&right>0&&oldz<0&&top>0))return r[2]<125000;
    }
    return 0;
}
static void reset_bounds(int b[4]){b[0]=b[1]=b[2]=b[3]=-125000;}

static void number_text(int value,char out[32])
{
    char buf[32];size_t i,j=0,len;snprintf(buf,sizeof buf,"%d",value);len=strlen(buf);
    for(i=0;i<len;i++){out[j++]=buf[i];if(buf[i]!='-'&&i+1<len&&(len-i-1)%3==0)out[j++]=',';}out[j]=0;
}
/* Only substitute the known %s/%d fields. Text is an asset, never a native
 * printf format; malformed formats cannot interpret pointers or write memory. */
static int format(char *out,size_t cap,const char *fmt,const char *const *args,size_t argc)
{
    size_t j=0,k=0,i;
    while(*fmt){const char *p;size_t len;if(*fmt!='%'){if(j+1>=cap)return -1;out[j++]=*fmt++;continue;}
        fmt++;if(*fmt!='s'&&*fmt!='d')return -1;fmt++;if(k==argc)return -1;p=args[k++];len=strlen(p);
        if(len>=cap-j)return -1;
        for(i=0;i<len;i++)out[j++]=p[i];}
    if(k!=argc)return -1;
    out[j]=0;return 0;
}
static int text_file(mercs_rmg_bundle *out,builder *b,const char *name,const char *text)
{
    size_t len=strlen(text),size=(len+12u)&~3u;uint8_t *p;begin(b);p=chunk(b,"ORDR",size);
    if(!p)return -1;
    memcpy(p+8,text,len+1);return finish(out,b,name);
}

static uint32_t enemy_model(const catalog *c,random_state *rng,int year)
{
    static const uint32_t base[4]={0xb00d4,0xb0234,0xb0374,0xb0474};
    static const int counts[4]={11,10,8,18};int group=3,index;
    (void)c;
    if(year<3050){group=(year-3044)/2;if(range(rng,0,1000)>=750)group=range(rng,0,group);}
    if(group<0||group>3)return 0;
    index=range(rng,0,counts[group]*10-1)/10;if(index<0||index>=counts[group])return 0;
    return base[group]+(uint32_t)index*32;
}
static int stars(prj_archive *archive,const catalog *c,random_state *rng,mercs_rmg_bundle *out,int count,char *e,size_t n)
{
    int star,j,skill=out->config.skill==1?6:out->config.skill==3?1:5;
    const char *abbr=ptr(c,0xaff04+(uint32_t)out->opponent*14);builder b;char name[32],display[80],stock[32];
    for(star=1;star<=count;star++,skill++){
        int pilots[3]={-1,-1,-1},num=range(rng,0,out->config.kind==2?19:29)/10+1;
        if(num<1||num>3)return fail(e,n,"Original RMG produced an invalid star size");
        if(skill>8)skill=8;
        begin(&b);out->enemy_count+=num;
        for(j=0;j<num;j++){
            uint32_t model=enemy_model(c,rng,out->config.year);int pilot,k,limit=0;uint8_t *p;prj_record stock_record;
            const char *skeleton,*pilot_name;const uint8_t *stats;
            if(!model)return fail(e,n,"Original RMG mech group is invalid");
            do{pilot=range(rng,0,129)/10;for(k=0;k<j&&pilots[k]!=pilot;k++){}if(++limit>4096)return -1;}while(k<j);
            if(pilot<0||pilot>12)return -1;
            pilots[j]=pilot;
            skeleton=ptr(c,model+8);pilot_name=ptr(c,0xafff0+(uint32_t)pilot*8);
            snprintf(stock,sizeof stock,"%s00std",ptr(c,model+4));snprintf(display,sizeof display,"%s %s",abbr,ptr(c,model+12));
            if(prj_read_named(archive,"MEK",stock,&stock_record)!=PRJ_OK)return fail(e,n,"RMG needs the original loadout %s",stock);
            prj_record_free(&stock_record);
            p=chunk(&b,"GPS\0",100);if(!p)return -1;
            w16(p+8,0xffff);w16(p+10,0xffff);p[12]=(uint8_t)star;p[13]=(uint8_t)(j==0);p[14]=2;
            stats=data(c,0xb0054+(uint32_t)skill*14);memcpy(p+16,stats,10);
            w16(p+32,6);w16(p+34,0x1000);
            memcpy(p+36,skeleton,strlen(skeleton)<8?strlen(skeleton):8);memcpy(p+45,stock,strlen(stock)<8?strlen(stock):8);
            memcpy(p+54,display,strlen(display)<21?strlen(display):21);memcpy(p+76,pilot_name,strlen(pilot_name)<21?strlen(pilot_name):21);
        }
        snprintf(name,sizeof name,"en%02dstar",star);if(finish(out,&b,name))return -1;
    }
    return 0;
}

void mercs_rmg_free(mercs_rmg_bundle *b)
{size_t i;if(b){for(i=0;i<b->count;i++)prj_record_free(&b->files[i]);memset(b,0,sizeof *b);}}
int mercs_rmg_install(prj_archive *a,const mercs_rmg_bundle *b)
{return !b?-1:prj_set_generated(a,b->files,b->count);}

int mercs_rmg_generate(prj_archive *a,const mercs_rmg_config *cfg,mercs_rmg_bundle *result,char *e,size_t n)
{
    catalog c;mercs_rmg_bundle out;random_state rng;area areas[49];int selected[49],used[49]={0},selected_count=0;
    int template_index,required,kind,i,j,x,z,bounds[4],tries=0,terrain,owner,other,planet=-1;builder b;
    char name[32],source[32],scene[32],text[4096],pay[32],enemy_count[32];const char *duty,*employer,*opponent;
    if(e&&n)e[0]=0;
    if(!a||!cfg||!result||cfg->number<0||cfg->number>9||(cfg->year!=-1&&cfg->year<3044)||cfg->year>32767||cfg->kind<0||cfg->kind>3||
       cfg->skill<0||cfg->skill>3||!memchr(cfg->planet,0,sizeof cfg->planet))return fail(e,n,"Invalid procedural mission parameters");
    if(catalog_open(&c,e,n))return -1;
    memset(&out,0,sizeof out);out.config=*cfg;rng.value=cfg->seed;rng.invalid=0;
    /* Instant Action asks the same generator for year=-1. DOS0x201b0 draws
     * its3044..3054 year before choosing a mission type or placement. */
    if(out.config.year==-1)out.config.year=range(&rng,3044,3054);
    if(!out.config.kind)out.config.kind=range(&rng,1,3);
    if(!out.config.skill)out.config.skill=2;
    kind=out.config.kind;
    if(cfg->planet[0]){for(i=0;i<15;i++)if(!strcasecmp(cfg->planet,ptr(&c,0xaff70+(uint32_t)i*8))){planet=i;break;}
        if(planet<0){fail(e,n,"Unknown original RMG planet %s",cfg->planet);goto bad;}}
    do{template_index=range(&rng,0,29);if(++tries>4096||template_index<0||template_index>=30)goto bad;
        required=template_index%3+1;j=template_index/3;i=j<7?(j+2)%3:(j+1)%3;
    }while(required>2||i!=kind-1);
    out.template_index=template_index;
    for(i=0;i<49;i++){const uint8_t *p=data(&c,0xaf903+(uint32_t)i*23);memcpy(areas[i].name,p,11);
        areas[i].name[10]=0;areas[i].width=(int32_t)u32(p+11);areas[i].height=(int32_t)u32(p+19);areas[i].required=0;}
    tries=0;while(selected_count<required){i=range(&rng,0,25);if(i<0||i>=26||++tries>16384)goto bad;
        if(!used[i]){used[i]=1;areas[i].required=1;selected[selected_count++]=i;}}
    while(selected_count<=23){i=range(&rng,26,48);if(i<26||i>=49||++tries>16384)goto bad;
        if(!used[i]){used[i]=1;selected[selected_count++]=i;}}
    tries=0;
    for(;;){int placed=0,good=1;
        for(i=0;i<required;i++){int tmp;j=range(&rng,0,selected_count-1);if(j<0||j>=selected_count)goto bad;tmp=selected[j];selected[j]=selected[i];selected[i]=tmp;}
        reset_bounds(bounds);
        for(i=0;i<selected_count;i++){const area *ar=&areas[selected[i]];if(!place(ar,bounds,&x,&z)){if(placed<required){good=0;break;}}
            else if(ar->required)placed++;}
        if(good)break;
        if(++tries>4096){fail(e,n,"Original RMG placement did not converge");goto bad;}
    }
    if(planet<0){terrain=range(&rng,0,54);owner=range(&rng,0,2);planet=range(&rng,0,14);
        if(planet<0||planet>=15||terrain<0||terrain>=55)goto bad;
        snprintf(out.config.planet,sizeof out.config.planet,"%s",ptr(&c,0xaff70+(uint32_t)planet*8));
    }else{owner=u16(data(&c,0xaff74+(uint32_t)planet*8));terrain=u16(data(&c,0xaff76+(uint32_t)planet*8));}
    tries=0;do{other=range(&rng,0,2);if(++tries>4096)goto bad;}while(other==owner);
    out.employer=kind==2?owner:other;out.opponent=kind==2?other:owner;
    if(out.config.year>=3050)out.opponent=range(&rng,3,6);
    if(out.employer<0||out.employer>6||out.opponent<0||out.opponent>6)goto bad;
    snprintf(out.scene,sizeof out.scene,"RAN%dSCN1",cfg->number);snprintf(out.world,sizeof out.world,"RAN%dWLD1",cfg->number);
    snprintf(out.companion,sizeof out.companion,"RND%dCAM1",cfg->number);
    begin(&b);snprintf(name,sizeof name,"%splt1",ptr(&c,0xafe24+(uint32_t)terrain*4));incl(&b,name,-1);
    for(i=1;i<=required;i++){snprintf(name,sizeof name,"en%02dstar.bwd",i);incl(&b,name,-2);}
    reset_bounds(bounds);j=1;
    for(i=0;i<selected_count;i++){
        const area *ar=&areas[selected[i]];int number=j;
        if(ar->required){snprintf(name,sizeof name,"mis%dare%d",template_index/3,j++);
            if(copy_template(&out,ar->name,name,e,n))goto bad;}
        if(!place(ar,bounds,&x,&z))break;
        block(&b,x,z);
        if(ar->required){snprintf(name,sizeof name,"mis%dstp%d",template_index/3,number);incl(&b,name,-1);
            snprintf(name,sizeof name,"mis%dare%d",template_index/3,number);incl(&b,name,-2);
            snprintf(name,sizeof name,"randnav%d",number);incl(&b,name,-2);
        }else incl(&b,ar->name,-1);chunk(&b,"ENDB",8);
    }
    if(finish(&out,&b,out.world)||add(&out,"RANDWLD1","BWD",b.bytes,b.size))goto bad;
    for(i=1;i<=required;i++){
        static const char *navnames[3]={"Nav Alpha","Nav Beta","Nav Gamma"};uint8_t *p;
        begin(&b);p=chunk(&b,"NAVP",80);if(!p)goto bad;w16(p+24,1);w16(p+32,100);w16(p+34,0x100);
        memcpy(p+36,navnames[i-1],strlen(navnames[i-1]));snprintf(name,sizeof name,"randnav%d",i);
        if(finish(&out,&b,name))goto bad;
    }
    if(stars(a,&c,&rng,&out,required,e,n))goto bad;
    snprintf(source,sizeof source,"mis%dscn%d",template_index/3,required);
    if(copy_template(&out,source,out.scene,e,n))goto bad;
    number_text(cfg->success_pay,pay);snprintf(enemy_count,sizeof enemy_count,"%d",out.enemy_count);
    duty=ptr(&c,0xaff60+(uint32_t)kind*4);employer=ptr(&c,0xaff00+(uint32_t)out.employer*14);opponent=ptr(&c,0xaff00+(uint32_t)out.opponent*14);
    {const char *args[]={employer,duty,opponent,out.config.planet,out.config.planet,pay,enemy_count,duty,duty};
        if(format(text,sizeof text,str(&c,0xa4a10),args,8))goto bad;}
    snprintf(name,sizeof name,"ran%dbrf1",cfg->number);if(text_file(&out,&b,name,text))goto bad;
    snprintf(name,sizeof name,"ran%ddbff",cfg->number);if(text_file(&out,&b,name,str(&c,0xa4cb0)))goto bad;
    {const char *args[]={pay};if(format(text,sizeof text,str(&c,0xa4d2c),args,1))goto bad;}
    snprintf(name,sizeof name,"ran%ddbfs",cfg->number);if(text_file(&out,&b,name,text))goto bad;
    {const char *args[]={employer,employer,duty,out.config.planet,opponent,
         str(&c,out.enemy_count<4?0xa4dbc:out.enemy_count<6?0xa4dc4:0xa4dcc),employer,out.config.planet,duty,pay,enemy_count};
        /* The retail sprintf passes a final unused enemy-count argument. */
        if(format(text,sizeof text,str(&c,0xa4dd4),args,10))goto bad;}
    begin(&b);{size_t len=strlen(text),ts=(len+12u)&~3u;uint8_t *p=chunk(&b,"CPGN",160);
        if(!p)goto bad;
        w32(p+4,(uint32_t)(160+ts));if(160+ts>b.largest)b.largest=(uint32_t)(160+ts);
        snprintf(scene,sizeof scene,"ran%dscn1.bwd",cfg->number);memcpy(p+20,scene,strlen(scene)+1);
        snprintf((char *)p+144,16,"%s",ptr(&c,0xaff0a+(uint32_t)out.employer*14));
        p=chunk(&b,"CTNT",ts);if(!p)goto bad;memcpy(p+8,text,len+1);if(finish(&out,&b,out.companion))goto bad;}
    for(i=0;i<=required;i++){
        prj_record logo;int id=u16(data(&c,0xaff08+(uint32_t)(i?out.opponent:out.employer)*14));
        if(prj_read(a,prj_find_type(a,"CEL"),id,&logo)!=PRJ_OK){fail(e,n,"Missing original RMG faction logo %d",id);goto bad;}
        snprintf(name,sizeof name,"mw2logo%d",i);j=logo.size<4098?-1:add(&out,name,"CEL",logo.data,4098);prj_record_free(&logo);if(j)goto bad;
    }
    if(rng.invalid)goto bad;
    free(c.exe);*result=out;return 0;
bad:
    mercs_rmg_free(&out);free(c.exe);
    if(rng.invalid){fail(e,n,"This original procedural seed exceeds an original table; choose a new preview.");return MERCS_RMG_BAD_SEED;}
    if(e&&n&&!e[0])fail(e,n,"Cannot generate the original procedural mission");
    return -1;
}

int mercs_rmg_campaign(prj_archive *a,const char *name,int year,uint32_t seed,
                       mercs_rmg_bundle *out,char *e,size_t n)
{
    mercs_rmg_config cfg;prj_record r;size_t p;int found=0;
    if(!name||strncasecmp(name,"RAN",3)||strlen(name)<4)return fail(e,n,"Invalid procedural campaign name");
    memset(&cfg,0,sizeof cfg);cfg.seed=seed;cfg.year=year;cfg.number=isdigit((unsigned char)name[3])?name[3]-'0':0;
    if(prj_read_named(a,"BWD",name,&r)!=PRJ_OK)return fail(e,n,"Missing procedural campaign %s",name);
    for(p=12;p+8<=r.size;){uint32_t z=u32(r.data+p+4);if(z<8||z>r.size-p)break;
        if(!memcmp(r.data+p,"CPGN",4)&&z>=160){cfg.success_pay=(int32_t)u32(r.data+p+8+0x5c);cfg.failure_pay=(int32_t)u32(r.data+p+8+0x60);found=1;break;}p+=z;}
    prj_record_free(&r);if(!found)return fail(e,n,"Missing original procedural campaign terms");
    {int rc=mercs_rmg_generate(a,&cfg,out,e,n);if(rc)return rc;}
    /* Retail preview uses atoi(letter)==0, but 0x67fc8 later asks for the
     * literal RND letter. Keep both names pointing to the same generated
     * contract so accepting a later contract preserves the shown terms. */
    if(!isdigit((unsigned char)name[3])){
        char alias[17];size_t i;
        if(strlen(name)>16){mercs_rmg_free(out);return fail(e,n,"Procedural campaign name is too long");}
        snprintf(alias,sizeof alias,"RND%s",name+3);
        for(i=0;i<out->count;i++)if(!strcasecmp(out->files[i].name,out->companion)){
            if(add(out,alias,"BWD",out->files[i].data,out->files[i].size)){
                mercs_rmg_free(out);return fail(e,n,"Cannot preserve generated contract identity");}
            break;
        }
    }
    return 0;
}
