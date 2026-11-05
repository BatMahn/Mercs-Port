#include "mercsinstant.h"
#include "mercsshell_chassis.h"
#include "mercsfile.h"
#include "lance.h"
#include "bwd.h"
#include <time.h>
#include <stdarg.h>
#include <ctype.h>

static const char *const planets[]={"Glen Garry","Whittington","Kansu","Mississauga","Woodstock",
    "Alegedi","Tok Do","Lands End","Corsica Nueva","Bad News","Orestes","Al Hillah","Grumium","Karbala","Ueda"};
static int fail(char *e,size_t n,const char *fmt,...)
{va_list a;if(e&&n){va_start(a,fmt);vsnprintf(e,n,fmt,a);va_end(a);}return -1;}
static uint32_t u32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void w32(uint8_t *p,uint32_t v)
{p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static int signed32(uint32_t v){return v<=INT32_MAX?(int)v:(int)(-1-(int64_t)(UINT32_MAX-v));}
static uint32_t crc(const uint8_t *p,size_t n)
{uint32_t c=UINT32_MAX;size_t i;int k;for(i=0;i<n;i++){c^=p[i];for(k=0;k<8;k++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;}
static int edition_ok(const char *p)
{return p&&(!strcmp(p,"dos")||!strcmp(p,"direct3d")||!strcmp(p,"3dfx"));}
static int token_ok(const char *p)
{size_t i;if(!p||strlen(p)!=32)return 0;for(i=0;i<32;i++)if(!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f')))return 0;return 1;}
static int chassis_ok(int slot,int chassis)
{return slot>=0&&slot<6&&chassis>=(slot<3?-1:-2)&&chassis<62;}
const char *mercs_instant_chassis_name(int c){return c>=0&&c<62?mercs_shell_chassis[c].name:NULL;}
const char *mercs_instant_prefix(int c){return c>=0&&c<62?mercs_shell_chassis[c].prefix:NULL;}
const char *mercs_instant_planet(int p){return p>=0&&p<15?planets[p]:NULL;}
int mercs_instant_planet_index(const char *s)
{int i;if(s)for(i=0;i<15;i++)if(!strcasecmp(s,planets[i]))return i;return -1;}
static void stock(mercs_instant_slot *p)
{p->loadout[0]=0;if(p->chassis>=0)snprintf(p->loadout,sizeof p->loadout,"%s00STD",mercs_instant_prefix(p->chassis));}
void mercs_instant_init(mercs_instant *s)
{int i;if(!s)return;memset(s,0,sizeof *s);s->parameters.year=-1;s->restart=1;for(i=0;i<6;i++){s->slots[i].chassis=i==0?0:i<3?-1:-2;stock(&s->slots[i]);}}
void mercs_instant_free(mercs_instant *s)
{if(s){mercs_rmg_free(&s->world);memset(s,0,sizeof *s);}}
int mercs_instant_cycle_chassis(int slot,int chassis,int step)
{
    int next;if(!chassis_ok(slot,chassis)||(step!=-1&&step!=1))return chassis;next=chassis+step;
    /* DOS0x5fb60: first enemy cannot become empty when cycling. */
    if(slot==3&&next==-1)next=step<0?61:0;
    if(next<(slot<3?-1:-2))next=61;else if(next>61)next=slot<3?-1:-2;
    return next;
}
void mercs_instant_cycle_variant(int *active,int count,int *index,int step)
{
    if(!active||!index||!step)return;
    if(count<=0){*active=0;*index=0;return;}
    if(!*active){*active=1;*index=0;return;}
    *index+=step;if(*index<0)*index=0;if(*index>=count)*index=count-1;
}
void mercs_instant_choose(mercs_instant *s,int slot,int step)
{
    mercs_instant_slot *p;int next;if(!s||slot<0||slot>=6)return;
    s->selected=slot;s->restart=1;if(!step)return;p=&s->slots[slot];
    next=mercs_instant_cycle_chassis(slot,p->chassis,step);memset(p,0,sizeof *p);p->chassis=next;stock(p);
}
int mercs_instant_generate(prj_archive *a,mercs_instant *s,uint32_t seed,char *e,size_t n)
{
    mercs_rmg_bundle b={0};mercs_rmg_config cfg;int rc;
    if(!a||!s)return fail(e,n,"Instant Action is not initialized.");
    cfg=s->parameters;cfg.seed=seed;cfg.number=0;cfg.year=-1;cfg.success_pay=cfg.failure_pay=0;
    rc=mercs_rmg_generate(a,&cfg,&b,e,n);if(rc)return rc;
    mercs_rmg_free(&s->world);s->world=b;s->parameters=b.config;s->request=cfg;s->ready=1;s->restart=1;return 0;
}

static int read_mek(const char *overlay,const char *name,uint8_t *bytes,size_t capacity,size_t *size)
{
    char path[2048],logical[32];FILE *f;size_t got;int bad,extra;mek_def m;
    snprintf(logical,sizeof logical,"MEK/%s.MEK",name);
    if(dp_resolve_in(overlay,logical,path,sizeof path)||(f=fopen(path,"rb"))==NULL)return -1;
    got=fread(bytes,1,capacity,f);extra=fgetc(f);bad=ferror(f);fclose(f);
    if(bad||extra!=EOF||mek_parse(bytes,got,&m))return -1;
    *size=got;return 0;
}
int mercs_instant_variant(mercs_instant *s,int step,const char *overlay,char *e,size_t n)
{
    mercs_instant_slot *p;int i,index,active;uint8_t bytes[MEK_MAX_SERIALIZED_BYTES];size_t size;
    if(!s||s->selected<0||s->selected>=6||!overlay||(step!=-1&&step!=1))return -1;
    p=&s->slots[s->selected];if(p->chassis<0)return 0;
    if(!p->variant_active){
        p->variant_count=0;
        /* Original0x5f73c counts only PREFIX??USR files. Its archive STD
         * lookup does not increase the count, so stock variants are not in
         * this selector. Numeric order replaces the DOS directory's order. */
        for(i=0;i<100;i++){
            char name[9];snprintf(name,sizeof name,"%s%02dUSR",mercs_instant_prefix(p->chassis),i);
            if(!read_mek(overlay,name,bytes,sizeof bytes,&size))strcpy(p->variants[p->variant_count++],name);
        }
    }
    active=p->variant_active;index=p->variant_index;mercs_instant_cycle_variant(&active,p->variant_count,&index,step);
    if(!active)return 0;
    if(read_mek(overlay,p->variants[index],bytes,sizeof bytes,&size))return fail(e,n,"The selected custom Mech could not be read.");
    p->variant_active=active;p->variant_index=index;strcpy(p->loadout,p->variants[index]);
    memcpy(p->custom,bytes,size);p->custom_size=size;return 0;
}
int mercs_instant_loadout(prj_archive *a,const mercs_instant *s,int slot,uint8_t *bytes,size_t cap,size_t *size)
{
    const mercs_instant_slot *p;prj_record r;mek_def m;int rc;
    if(size)*size=0;
    if(!a||!s||slot<0||slot>=6||!bytes||!size)return -1;
    p=&s->slots[slot];if(p->chassis<0)return -1;
    if(p->custom_size){if(p->custom_size>cap)return -1;memcpy(bytes,p->custom,p->custom_size);*size=p->custom_size;return 0;}
    if(prj_read_named(a,"MEK",p->loadout,&r))return -1;
    rc=r.size>cap||mek_parse(r.data,r.size,&m);if(!rc){memcpy(bytes,r.data,r.size);*size=r.size;}prj_record_free(&r);return rc?-1:0;
}
int mercs_instant_design(mercs_instant *s,const uint8_t *bytes,size_t size,const char *overlay,char *e,size_t n)
{
    mercs_instant_slot *p;char dir[2048],name[16],path[2048],base[9];int i;mek_def m;
    if(!s||s->selected<0||s->selected>=6||!bytes||size>MEK_MAX_SERIALIZED_BYTES||mek_parse(bytes,size,&m))return fail(e,n,"Invalid Instant Action MechLab design.");
    p=&s->slots[s->selected];if(p->chassis<0)return fail(e,n,"Select a Mech type first.");
    if(mercs_file_directory(overlay,"MEK",dir,sizeof dir,e,n))return -1;
    for(i=0;i<100;i++){
        snprintf(base,sizeof base,"%s%02dUSR",mercs_instant_prefix(p->chassis),i);snprintf(name,sizeof name,"%s.MEK",base);
        if(dp_resolve_in(dir,name,path,sizeof path))break;
    }
    if(i==100)return fail(e,n,"Too many Mechs of this variant. Remove an unused custom design first.");
    if(mercs_atomic_write(dir,name,bytes,size,e,n))return -1;
    memcpy(p->custom,bytes,size);p->custom_size=size;strcpy(p->loadout,base);
    p->variant_count=0;p->variant_index=0;p->variant_active=0;s->restart=1;return 0;
}

static int request_ok(const mercs_rmg_config *c)
{return c->number==0&&c->year==-1&&!c->success_pay&&!c->failure_pay&&c->kind>=0&&c->kind<=3&&c->skill>=0&&c->skill<=3&&
    memchr(c->planet,0,sizeof c->planet)&&(!c->planet[0]||mercs_instant_planet_index(c->planet)>=0);}
static int launch_ok(const mercs_instant_launch *l)
{
    int i;mek_def m;
    if(!l||!memchr(l->edition,0,sizeof l->edition)||!edition_ok(l->edition)||!request_ok(&l->request)||l->difficulty<0||l->difficulty>2)return 0;
    for(i=0;i<6;i++){
        int c=l->slots[i].chassis;const char *p=l->slots[i].loadout;size_t j;
        if(!chassis_ok(i,c)||!memchr(p,0,9)||l->slots[i].size>MEK_MAX_SERIALIZED_BYTES)return 0;
        if(c<0){if(p[0]||l->slots[i].size)return 0;continue;}
        if(strlen(p)!=8||strncasecmp(p,mercs_instant_prefix(c),3)||p[3]<'0'||p[3]>'9'||p[4]<'0'||p[4]>'9')return 0;
        for(j=0;j<8;j++)if(!isalnum((unsigned char)p[j]))return 0;
        if(l->slots[i].size){if(strcasecmp(p+5,"USR")||mek_parse(l->slots[i].bytes,l->slots[i].size,&m))return 0;}
        else if(strcasecmp(p+5,"STD"))return 0;
    }
    return l->slots[0].chassis>=0;
}
int mercs_instant_prepare(const mercs_instant *s,const char *edition,int difficulty,mercs_instant_launch *out,char *e,size_t n)
{
    mercs_instant_launch l;int i;if(!s||!s->ready||!out||!edition_ok(edition))return fail(e,n,"Instant Action mission is not ready.");
    if(s->slots[0].chassis<0)return fail(e,n,"You must choose a mech for yourself.");
    if(s->slots[3].chassis==-1&&s->slots[4].chassis==-1&&s->slots[5].chassis==-1)return fail(e,n,"You must select at least 1 enemy Mech.");
    memset(&l,0,sizeof l);strcpy(l.edition,edition);l.request=s->request;l.difficulty=difficulty;
    for(i=0;i<6;i++){
        if(s->slots[i].custom_size>MEK_MAX_SERIALIZED_BYTES||!memchr(s->slots[i].loadout,0,9))return fail(e,n,"Invalid Instant Action loadout.");
        l.slots[i].chassis=s->slots[i].chassis;strcpy(l.slots[i].loadout,s->slots[i].loadout);
        l.slots[i].size=s->slots[i].custom_size;memcpy(l.slots[i].bytes,s->slots[i].custom,l.slots[i].size);}
    if(!launch_ok(&l))return fail(e,n,"Invalid Instant Action roster.");
    *out=l;return 0;
}

static int add_record(prj_record *r,size_t *count,const char *tag,const char *name,const uint8_t *bytes,size_t size)
{
    size_t i;uint8_t *copy=malloc(size?size:1);if(!copy)return -1;memcpy(copy,bytes,size);
    for(i=0;i<*count;i++)if(!strcasecmp(r[i].name,name)&&!strcmp(r[i].tag,tag))break;
    if(i==*count){if(*count>=64){free(copy);return -1;}(*count)++;}else free(r[i].data);
    memset(&r[i],0,sizeof r[i]);strcpy(r[i].name,name);strcpy(r[i].tag,tag);r[i].data=copy;r[i].size=size;r[i].generated=1;return 0;
}
int mercs_instant_install(prj_archive *a,const mercs_instant_launch *l,char *e,size_t n)
{
    mercs_rmg_bundle b={0};prj_record records[64];lance_mercs_slot friendly[3]={{0}},enemy[3]={{0}};
    char random[3][9]={{0}};size_t count=0,i,size;int k,rc=-1,skill;uint8_t star[LANCE_MERCS_STAR_MAX_BYTES];
    memset(records,0,sizeof records);if(!a||!launch_ok(l))return fail(e,n,"Invalid Instant Action launch.");
    if(mercs_rmg_generate(a,&l->request,&b,e,n))return -1;
    for(i=0;i<b.count;i++){
        const prj_record *p=&b.files[i];
        if(!strcasecmp(p->name,"EN01STAR")){size_t at;int slot=0;
            for(at=12;at+8<=p->size;){uint32_t z=u32(p->data+at+4);if(z<8||z>p->size-at)break;
                if(!memcmp(p->data+at,"GPS",3)&&z>=100&&slot<3){memcpy(random[slot++],p->data+at+45,8);}at+=z;}}
        if(add_record(records,&count,p->tag,p->name,p->data,p->size))goto done;
    }
    for(k=0;k<6;k++){
        int chassis=l->slots[k].chassis;const char *name=l->slots[k].loadout;lance_mercs_slot *slot=k<3?&friendly[k]:&enemy[k-3];
        if(chassis==-1)continue;
        if(chassis==-2){name=random[k-3];if(!name[0])continue;
            for(chassis=0;chassis<62;chassis++)if(!strncasecmp(name,mercs_instant_prefix(chassis),3))break;
            if(chassis==62){fail(e,n,"The original random enemy has an unknown chassis.");goto done;}}
        slot->active=1;strcpy(slot->skeleton,mercs_shell_chassis[chassis].skeleton);strcpy(slot->loadout,name);
        if(!k)strcpy(slot->name,"MechWarrior");
        else snprintf(slot->name,sizeof slot->name,k<3?"Friend %d":"Enemy %d",k<3?k:k-2);
        if(l->slots[k].size){size_t j;
            for(j=0;j<count;j++)if(!strcmp(records[j].tag,"MEK")&&!strcasecmp(records[j].name,name))break;
            if(j<count&&(records[j].size!=l->slots[k].size||memcmp(records[j].data,l->slots[k].bytes,l->slots[k].size))){fail(e,n,"Two Instant Action Mechs have conflicting custom design identities.");goto done;}
            if(add_record(records,&count,"MEK",name,l->slots[k].bytes,l->slots[k].size))goto done;
        }else{prj_record r;mek_def m;if(prj_read_named(a,"MEK",name,&r)){fail(e,n,"Missing Instant Action loadout %s.",name);goto done;}
            rc=mek_parse(r.data,r.size,&m);prj_record_free(&r);if(rc)goto done;rc=-1;}
    }
    if(!enemy[0].active&&!enemy[1].active&&!enemy[2].active){fail(e,n,"You must select at least 1 enemy Mech.");goto done;}
    /* DOS0x5e560 resets skill for EACH file. It does not inherit Mech2's
     * decrement-per-star rule. Its MW2DIF difficulty is independent of the
     * RMG mission-parameter skill byte. */
    skill=l->difficulty==0?7:l->difficulty==2?5:6;
    for(k=0;k<6;k++){
        char name[17];if(k)snprintf(name,sizeof name,"EN%02dSTAR",k);else strcpy(name,"USERSTAR");
        if(lance_mercs_star_bytes(a,k?enemy:friendly,3,k,k?skill:0,star,sizeof star,&size)||add_record(records,&count,"BWD",name,star,size))goto done;
    }
    rc=prj_set_generated(a,records,count);
done:
    for(i=0;i<count;i++)prj_record_free(&records[i]);
    mercs_rmg_free(&b);
    if(rc&&e&&n&&!e[0])fail(e,n,"Could not construct the original Instant Action launch.");
    return rc;
}

/* Explicit byte layout: no pointer/padding/size_t is serialized. */
#define IA_HEADER 80u
#define IA_SLOT_HEADER 17u
#define IA_MAX_BYTES (IA_HEADER+6u*(IA_SLOT_HEADER+MEK_MAX_SERIALIZED_BYTES)+4u)
static int encode(const mercs_instant_launch *l,uint8_t *out,size_t *size)
{
    size_t at=IA_HEADER;int i;if(!launch_ok(l)||!token_ok(l->token))return -1;
    memset(out,0,IA_MAX_BYTES);memcpy(out,"MCRSIA01",8);memcpy(out+8,l->token,32);memcpy(out+40,l->edition,strlen(l->edition));
    w32(out+52,l->request.seed);out[56]=(uint8_t)l->request.kind;out[57]=(uint8_t)l->request.skill;out[58]=(uint8_t)l->difficulty;
    memcpy(out+60,l->request.planet,strlen(l->request.planet));
    for(i=0;i<6;i++){w32(out+at,(uint32_t)l->slots[i].chassis);memcpy(out+at+4,l->slots[i].loadout,9);w32(out+at+13,(uint32_t)l->slots[i].size);at+=17;
        memcpy(out+at,l->slots[i].bytes,l->slots[i].size);at+=l->slots[i].size;}
    w32(out+at,crc(out,at));*size=at+4;return 0;
}
static int decode(const uint8_t *p,size_t size,mercs_instant_launch *out)
{
    mercs_instant_launch l;uint8_t check[IA_MAX_BYTES];size_t at=IA_HEADER,n;int i;
    if(size<IA_HEADER+6*IA_SLOT_HEADER+4||size>IA_MAX_BYTES||memcmp(p,"MCRSIA01",8)||u32(p+size-4)!=crc(p,size-4))return -1;
    memset(&l,0,sizeof l);memcpy(l.token,p+8,32);memcpy(l.edition,p+40,12);l.request.year=-1;
    l.request.seed=u32(p+52);l.request.kind=p[56];l.request.skill=p[57];l.difficulty=p[58];memcpy(l.request.planet,p+60,17);
    for(i=0;i<6;i++){if(size-4-at<17)return -1;l.slots[i].chassis=signed32(u32(p+at));memcpy(l.slots[i].loadout,p+at+4,9);l.slots[i].size=u32(p+at+13);at+=17;
        if(l.slots[i].size>MEK_MAX_SERIALIZED_BYTES||l.slots[i].size>size-4-at)return -1;
        memcpy(l.slots[i].bytes,p+at,l.slots[i].size);at+=l.slots[i].size;}
    if(at!=size-4||encode(&l,check,&n)||n!=size||memcmp(check,p,n))return -1;
    *out=l;return 0;
}
static int realdir(const char *p)
{
#ifdef _WIN32
    DWORD a=GetFileAttributesA(p);return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT);
#else
    struct stat s;return !lstat(p,&s)&&S_ISDIR(s.st_mode)&&!S_ISLNK(s.st_mode);
#endif
}
static int child_dir(const char *overlay,const char *token,char *dir,size_t cap)
{
    char parent[2048];int n;if(!overlay||!realdir(overlay)||!token_ok(token))return -1;
    n=snprintf(parent,sizeof parent,"%s/MERCS-INSTANT",overlay);if(n<0||(size_t)n>=sizeof parent||!realdir(parent))return -1;
    n=snprintf(dir,cap,"%s/%s",parent,token);return n<0||(size_t)n>=cap||!realdir(dir)?-1:0;
}
int mercs_instant_begin(const char *overlay,mercs_instant_launch *l,char *e,size_t n)
{
    static uint32_t sequence;mercs_instant_launch staged;char parent[2048],dir[2048];uint8_t data[IA_MAX_BYTES];size_t size;unsigned tries,pid;int k;
    if(!l||l->token[0]||!launch_ok(l)||!overlay||!realdir(overlay))return fail(e,n,"Invalid Instant Action launch context.");
    if(mercs_file_directory(overlay,"MERCS-INSTANT",parent,sizeof parent,e,n))return -1;
    staged=*l;
#ifdef _WIN32
    pid=(unsigned)GetCurrentProcessId();
#else
    pid=(unsigned)getpid();
#endif
    for(tries=0;tries<256;tries++){
        snprintf(staged.token,sizeof staged.token,"%08x%08x%08x%08x",(unsigned)time(NULL),pid,(unsigned)clock(),(unsigned)++sequence);
        k=snprintf(dir,sizeof dir,"%s/%s",parent,staged.token);if(k<0||(size_t)k>=sizeof dir)return fail(e,n,"Instant Action path is too long.");
#ifdef _WIN32
        if(CreateDirectoryA(dir,NULL))break;
        if(GetLastError()!=ERROR_ALREADY_EXISTS)return fail(e,n,"Could not create the Instant Action launch.");
#else
        if(!mkdir(dir,0700))break;
        if(errno!=EEXIST)return mercs_file_failure(e,n,"Instant Action launch");
#endif
    }
    if(tries==256||encode(&staged,data,&size))return fail(e,n,"Could not identify the Instant Action launch.");
    if(mercs_atomic_write(dir,"LAUNCH.DAT",data,size,e,n))return -1;
    *l=staged;return 0;
}
int mercs_instant_read(const char *overlay,const char *token,const char *edition,mercs_instant_launch *l,char *e,size_t n)
{
    char dir[2048],path[2080];uint8_t data[IA_MAX_BYTES];size_t got;mercs_instant_launch staged;FILE *f;int fd,bad,extra;
    if(!l||!edition_ok(edition)||child_dir(overlay,token,dir,sizeof dir))return fail(e,n,"Instant Action launch identity is missing.");
    snprintf(path,sizeof path,"%s/LAUNCH.DAT",dir);
#ifdef _WIN32
    {HANDLE h=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);BY_HANDLE_FILE_INFORMATION info;
     if(h==INVALID_HANDLE_VALUE)return fail(e,n,"Instant Action launch could not be read.");
     if(!GetFileInformationByHandle(h,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){CloseHandle(h);return fail(e,n,"Invalid Instant Action launch file.");}
     fd=_open_osfhandle((intptr_t)h,_O_RDONLY|_O_BINARY);if(fd<0){CloseHandle(h);return -1;}f=_fdopen(fd,"rb");if(!f)_close(fd);}
#else
    {struct stat st;int flags=O_RDONLY|O_NONBLOCK;
#ifdef O_NOFOLLOW
     flags|=O_NOFOLLOW;
#endif
     fd=open(path,flags);if(fd<0)return mercs_file_failure(e,n,"Instant Action launch");
     if(fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);return fail(e,n,"Invalid Instant Action launch file.");}
     f=fdopen(fd,"rb");if(!f)close(fd);}
#endif
    if(!f)return mercs_file_failure(e,n,"Instant Action launch");
    got=fread(data,1,sizeof data,f);extra=fgetc(f);bad=ferror(f);fclose(f);
    if(bad||extra!=EOF||decode(data,got,&staged)||strcmp(staged.token,token)||strcmp(staged.edition,edition))return fail(e,n,"Instant Action launch is incomplete or does not match this edition.");
    *l=staged;return 0;
}
