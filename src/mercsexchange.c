#include "mercsexchange.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

/* Explicit little-endian encoding keeps this private protocol independent of
 * compiler padding, pointer size and host endian. CRC detects partial/corrupt
 * files; the token provides launch identity, not an authentication boundary. */
#define EX_CONTEXT (8 + 32 + 17 + 12 + 4 + MERCS_EXCHANGE_UNITS * 37)
#define EX_MAX_PACKET (EX_CONTEXT + MERCS_CAR_BYTES + MERCS_MSN_BYTES + 4 + \
                       MERCS_EXCHANGE_UNITS * (33 + MERCS_EXCHANGE_MEK_BYTES) + 4)

static void put32(uint8_t *p, uint32_t v)
{ p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static uint32_t checksum(const uint8_t *b, size_t n)
{
    uint32_t v=0xffffffffu;
    size_t i;
    int j;
    for(i=0;i<n;i++) { v^=b[i]; for(j=0;j<8;j++) v=(v>>1)^((0u-(v&1u))&0xedb88320u); }
    return ~v;
}
static int token_ok(const char *s)
{
    size_t i;
    if(!s || strlen(s)!=32) return 0;
    for(i=0;i<32;i++) if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f'))) return 0;
    return 1;
}
static int name_ok(const char *s, size_t capacity, int dots)
{
    size_t i;
    if(!s || !s[0]) return 0;
    for(i=0;i<capacity;i++) {
        unsigned char c=(unsigned char)s[i];
        if(!c) return i>0;
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||(dots&&c=='.'))) return 0;
    }
    return 0;
}
static int launch_ok(const mercs_exchange_launch *l)
{
    size_t i,j;
    if(!l || !memchr(l->token,0,sizeof l->token) || !token_ok(l->token) ||
       !name_ok(l->scene,sizeof l->scene,0) || !memchr(l->edition,0,sizeof l->edition) ||
       (strcmp(l->edition,"dos") && strcmp(l->edition,"3dfx") && strcmp(l->edition,"direct3d")) ||
       !l->unit_count || l->unit_count>MERCS_EXCHANGE_UNITS) return 0;
    for(i=0;i<l->unit_count;i++) {
        const mercs_exchange_unit *u=&l->units[i];
        if(u->actor_index < -3 || u->actor_index>=256 || !(u->controller_flags&2) ||
           !name_ok(u->name,sizeof u->name,1) || u->name[0]=='.') return 0;
        for(j=0;j<i;j++) if(u->actor_index==l->units[j].actor_index || !strcmp(u->name,l->units[j].name)) return 0;
    }
    return 1;
}
static int encode_launch(const mercs_exchange_launch *l, uint8_t *b)
{
    size_t i,at=73;
    if(!launch_ok(l)) return MERCS_EXCHANGE_INVALID;
    memset(b,0,EX_CONTEXT);
    memcpy(b,"MCRSCTX1",8); memcpy(b+8,l->token,32);
    memcpy(b+40,l->scene,strlen(l->scene)); memcpy(b+57,l->edition,strlen(l->edition));
    put32(b+69,(uint32_t)l->unit_count);
    for(i=0;i<l->unit_count;i++,at+=37) {
        const mercs_exchange_unit *u=&l->units[i];
        put32(b+at,(uint32_t)u->actor_index); put32(b+at+4,u->controller_flags);
        put32(b+at+8,(uint32_t)u->alliance); memcpy(b+at+12,u->name,strlen(u->name));
    }
    return 0;
}
static int decode_launch(const uint8_t *b, mercs_exchange_launch *out)
{
    mercs_exchange_launch l;
    uint8_t check[EX_CONTEXT];
    size_t i,at=73;
    if(memcmp(b,"MCRSCTX1",8)) return MERCS_EXCHANGE_INVALID;
    memset(&l,0,sizeof l);
    memcpy(l.token,b+8,32); memcpy(l.scene,b+40,17); memcpy(l.edition,b+57,12);
    if(!memchr(l.scene,0,sizeof l.scene) || !memchr(l.edition,0,sizeof l.edition)) return MERCS_EXCHANGE_INVALID;
    l.unit_count=get32(b+69);
    if(l.unit_count>MERCS_EXCHANGE_UNITS) return MERCS_EXCHANGE_INVALID;
    for(i=0;i<l.unit_count;i++,at+=37) {
        mercs_exchange_unit *u=&l.units[i];
        if(get32(b+at+4)>65535) return MERCS_EXCHANGE_INVALID;
        u->actor_index=(int32_t)get32(b+at); u->controller_flags=(uint16_t)get32(b+at+4);
        u->alliance=(int32_t)get32(b+at+8); memcpy(u->name,b+at+12,25);
    }
    if(encode_launch(&l,check) || memcmp(check,b,EX_CONTEXT)) return MERCS_EXCHANGE_INVALID;
    *out=l; return 0;
}
static int real_directory(const char *path)
{
#ifdef _WIN32
    DWORD a=GetFileAttributesA(path);
    return a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY) && !(a&FILE_ATTRIBUTE_REPARSE_POINT);
#else
    struct stat s;
    return !lstat(path,&s) && S_ISDIR(s.st_mode) && !S_ISLNK(s.st_mode);
#endif
}
static int make_directory(const char *path)
{
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path,0700);
#endif
}
static int directory_path(const char *overlay, const char *token, char *path, size_t capacity)
{
    char parent[4096];
    int n;
    if(!overlay || !real_directory(overlay) || !token_ok(token)) return MERCS_EXCHANGE_INVALID;
    n=snprintf(parent,sizeof parent,"%s/MERCS-RETURNS",overlay);
    if(n<0 || (size_t)n>=sizeof parent || !real_directory(parent)) return MERCS_EXCHANGE_IO;
    n=snprintf(path,capacity,"%s/%s",parent,token);
    if(n<0 || (size_t)n>=capacity || !real_directory(path)) return MERCS_EXCHANGE_IO;
    return 0;
}
static int path_for(const char *dir,const char *name,char *path,size_t capacity)
{
    int n=snprintf(path,capacity,"%s/%s",dir,name);
    return n>=0 && (size_t)n<capacity ? 0 : MERCS_EXCHANGE_INVALID;
}

/* Open only ordinary files. No inherited descriptor, symlink, FIFO, or device
 * can turn a local return check into an external write or blocking read. */
static FILE *open_regular(const char *path,int write_new)
{
#ifdef _WIN32
    HANDLE h;
    BY_HANDLE_FILE_INFORMATION info;
    int fd;
    h=CreateFileA(path,write_new?GENERIC_WRITE:GENERIC_READ,FILE_SHARE_READ,NULL,
                 write_new?CREATE_NEW:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(h==INVALID_HANDLE_VALUE) {
        DWORD e=GetLastError(); errno=(e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND)?ENOENT:EIO; return NULL;
    }
    if(!GetFileInformationByHandle(h,&info) || (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) {
        CloseHandle(h); errno=EINVAL; return NULL;
    }
    fd=_open_osfhandle((intptr_t)h,write_new?(_O_WRONLY|_O_BINARY):(_O_RDONLY|_O_BINARY));
    if(fd<0) { CloseHandle(h); return NULL; }
    { FILE *f=_fdopen(fd,write_new?"wb":"rb"); if(!f) _close(fd); return f; }
#else
    int flags=write_new?(O_WRONLY|O_CREAT|O_EXCL):O_RDONLY,fd;
    struct stat s;
    FILE *f;
#ifdef O_NOFOLLOW
    flags|=O_NOFOLLOW;
#endif
#ifdef O_CLOEXEC
    flags|=O_CLOEXEC;
#endif
    fd=open(path,flags|O_NONBLOCK,0600);
    if(fd<0) return NULL;
    if(fstat(fd,&s) || !S_ISREG(s.st_mode)) { close(fd); errno=EINVAL; return NULL; }
    if(fcntl(fd,F_SETFD,FD_CLOEXEC)<0) { close(fd); return NULL; }
    f=fdopen(fd,write_new?"wb":"rb"); if(!f) close(fd); return f;
#endif
}
static int file_present(const char *path)
{
#ifdef _WIN32
    DWORD a=GetFileAttributesA(path);
    if(a!=INVALID_FILE_ATTRIBUTES) return 1;
    return GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND ? 0 : -1;
#else
    struct stat s;
    if(!lstat(path,&s)) return 1;
    return errno==ENOENT ? 0 : -1;
#endif
}
static int read_bytes(const char *path,uint8_t *bytes,size_t capacity,size_t *size)
{
    FILE *f=open_regular(path,0);
    size_t n;
    int extra,bad;
    if(!f) return errno==ENOENT ? MERCS_EXCHANGE_PENDING : MERCS_EXCHANGE_IO;
    n=fread(bytes,1,capacity,f); extra=fgetc(f); bad=ferror(f); if(fclose(f)) bad=1;
    if(bad || extra!=EOF) return MERCS_EXCHANGE_IO;
    *size=n; return 0;
}
static int publish_bytes(const char *dir,const char *name,const uint8_t *bytes,size_t size)
{
    char path[4096],temporary[4096],tempname[64];
    FILE *f;
    int bad=0;
    if(path_for(dir,name,path,sizeof path)) return MERCS_EXCHANGE_INVALID;
    if(file_present(path)!=0) return MERCS_EXCHANGE_INVALID;
    snprintf(tempname,sizeof tempname,"%s.TMP",name);
    if(path_for(dir,tempname,temporary,sizeof temporary)) return MERCS_EXCHANGE_INVALID;
    f=open_regular(temporary,1);
    if(!f) return MERCS_EXCHANGE_IO;
    if(fwrite(bytes,1,size,f)!=size || fflush(f)) bad=1;
#ifdef _WIN32
    if(!bad && _commit(_fileno(f))) bad=1;
#else
    if(!bad && fsync(fileno(f))) bad=1;
#endif
    if(fclose(f)) bad=1;
    if(bad) return MERCS_EXCHANGE_IO; /* leave partial staging evidence */
#ifdef _WIN32
    if(!MoveFileExA(temporary,path,MOVEFILE_WRITE_THROUGH)) return MERCS_EXCHANGE_IO;
#else
    if(rename(temporary,path)) return MERCS_EXCHANGE_IO;
    {
        int fd=open(dir,O_RDONLY);
        if(fd<0) return MERCS_EXCHANGE_IO;
        bad=fsync(fd); close(fd);
        if(bad) return MERCS_EXCHANGE_IO;
    }
#endif
    return 0;
}

int mercs_exchange_begin(const char *overlay,mercs_exchange_launch *launch)
{
    static uint32_t sequence;
    mercs_exchange_launch staged;
    char parent[4096],dir[4096];
    uint8_t bytes[EX_CONTEXT+4];
    unsigned attempt;
    int n,rc;
    uint32_t pid;
    if(!launch || launch->token[0] || !overlay || !real_directory(overlay)) return MERCS_EXCHANGE_INVALID;
    staged=*launch;
    strcpy(staged.token,"00000000000000000000000000000000");
    if(!launch_ok(&staged)) return MERCS_EXCHANGE_INVALID;
    n=snprintf(parent,sizeof parent,"%s/MERCS-RETURNS",overlay);
    if(n<0 || (size_t)n>=sizeof parent) return MERCS_EXCHANGE_INVALID;
    if(make_directory(parent) && errno!=EEXIST) return MERCS_EXCHANGE_IO;
    if(!real_directory(parent)) return MERCS_EXCHANGE_IO;
#ifdef _WIN32
    pid=(uint32_t)_getpid();
#else
    pid=(uint32_t)getpid();
#endif
    for(attempt=0;attempt<256;attempt++) {
        snprintf(staged.token,sizeof staged.token,"%08x%08x%08x%08x",(unsigned)time(NULL),
                 (unsigned)pid,(unsigned)clock(),(unsigned)++sequence);
        n=snprintf(dir,sizeof dir,"%s/%s",parent,staged.token);
        if(n<0 || (size_t)n>=sizeof dir) return MERCS_EXCHANGE_INVALID;
        if(!make_directory(dir)) break;
        if(errno!=EEXIST) return MERCS_EXCHANGE_IO;
    }
    if(attempt==256) return MERCS_EXCHANGE_IO;
    if(encode_launch(&staged,bytes)) return MERCS_EXCHANGE_INVALID;
    put32(bytes+EX_CONTEXT,checksum(bytes,EX_CONTEXT));
    rc=publish_bytes(dir,"LAUNCH.DAT",bytes,sizeof bytes);
    if(!rc) *launch=staged;
    return rc;
}

int mercs_exchange_load(const char *overlay,const char *token,mercs_exchange_launch *launch)
{
    char dir[4096],path[4096];
    uint8_t bytes[EX_CONTEXT+4];
    mercs_exchange_launch staged;
    size_t n;
    int rc;
    if(!launch) return MERCS_EXCHANGE_INVALID;
    rc=directory_path(overlay,token,dir,sizeof dir);
    if(rc) return rc;
    if(path_for(dir,"LAUNCH.DAT",path,sizeof path)) return MERCS_EXCHANGE_INVALID;
    rc=read_bytes(path,bytes,sizeof bytes,&n);
    if(rc) return rc;
    if(n!=sizeof bytes || get32(bytes+EX_CONTEXT)!=checksum(bytes,EX_CONTEXT) || decode_launch(bytes,&staged) || strcmp(staged.token,token)) return MERCS_EXCHANGE_INVALID;
    *launch=staged; return 0;
}
static int result_ok(const mercs_exchange_launch *launch,const mercs_exchange_result *result)
{
    mercs_msn_record msn;
    size_t i,j,k;
    if(!result || result->mech_count>launch->unit_count || mercs_msn_parse(result->mission,MERCS_MSN_BYTES,&msn)) return 0;
    for(i=0;i<result->mech_count;i++) {
        const mercs_exchange_mek *m=&result->mechs[i];
        if(!name_ok(m->name,sizeof m->name,1) || m->size<24+8*40+50 || m->size>MERCS_EXCHANGE_MEK_BYTES) return 0;
        for(j=0;j<launch->unit_count;j++)
            if(m->actor_index==launch->units[j].actor_index && !strcmp(m->name,launch->units[j].name)) break;
        if(j==launch->unit_count) return 0;
        for(k=0;k<i;k++) if(m->actor_index==result->mechs[k].actor_index) return 0;
    }
    return 1;
}

int mercs_exchange_publish(const char *overlay,const mercs_exchange_launch *launch,
                           const mercs_exchange_result *result)
{
    uint8_t bytes[EX_MAX_PACKET],check[EX_CONTEXT];
    mercs_exchange_launch saved;
    char dir[4096];
    size_t i,at=EX_CONTEXT;
    if(encode_launch(launch,bytes) || !result_ok(launch,result) ||
       mercs_exchange_load(overlay,launch->token,&saved) || encode_launch(&saved,check) ||
       memcmp(bytes,check,EX_CONTEXT) || directory_path(overlay,launch->token,dir,sizeof dir)) return MERCS_EXCHANGE_INVALID;
    memcpy(bytes+at,result->career,MERCS_CAR_BYTES); at+=MERCS_CAR_BYTES;
    memcpy(bytes+at,result->mission,MERCS_MSN_BYTES); at+=MERCS_MSN_BYTES;
    put32(bytes+at,(uint32_t)result->mech_count); at+=4;
    for(i=0;i<result->mech_count;i++) {
        const mercs_exchange_mek *m=&result->mechs[i];
        put32(bytes+at,(uint32_t)m->actor_index); memset(bytes+at+4,0,25);
        memcpy(bytes+at+4,m->name,strlen(m->name)); put32(bytes+at+29,(uint32_t)m->size); at+=33;
        memcpy(bytes+at,m->bytes,m->size); at+=m->size;
    }
    put32(bytes+at,checksum(bytes,at)); at+=4;
    return publish_bytes(dir,"RESULT.DAT",bytes,at);
}

int mercs_exchange_read(const char *overlay,const mercs_exchange_launch *launch,
                        mercs_exchange_result *result)
{
    uint8_t bytes[EX_MAX_PACKET],expected[EX_CONTEXT];
    mercs_exchange_result staged;
    char dir[4096],path[4096];
    size_t size,at=EX_CONTEXT,i;
    int rc,present;
    if(!result || encode_launch(launch,expected)) return MERCS_EXCHANGE_INVALID;
    rc=directory_path(overlay,launch->token,dir,sizeof dir);
    if(rc) return rc;
    if(path_for(dir,"APPLIED.DAT",path,sizeof path)) return MERCS_EXCHANGE_INVALID;
    present=file_present(path);
    if(present<0) return MERCS_EXCHANGE_IO;
    if(present) return MERCS_EXCHANGE_APPLIED;
    if(path_for(dir,"RESULT.DAT",path,sizeof path)) return MERCS_EXCHANGE_INVALID;
    rc=read_bytes(path,bytes,sizeof bytes,&size);
    if(rc) return rc;
    if(size<EX_CONTEXT+MERCS_CAR_BYTES+MERCS_MSN_BYTES+8 ||
       get32(bytes+size-4)!=checksum(bytes,size-4) || memcmp(bytes,expected,EX_CONTEXT)) return MERCS_EXCHANGE_INVALID;
    memset(&staged,0,sizeof staged);
    memcpy(staged.career,bytes+at,MERCS_CAR_BYTES); at+=MERCS_CAR_BYTES;
    memcpy(staged.mission,bytes+at,MERCS_MSN_BYTES); at+=MERCS_MSN_BYTES;
    staged.mech_count=get32(bytes+at); at+=4;
    if(staged.mech_count>MERCS_EXCHANGE_UNITS) return MERCS_EXCHANGE_INVALID;
    for(i=0;i<staged.mech_count;i++) {
        mercs_exchange_mek *m=&staged.mechs[i];
        if(size-4-at<33) return MERCS_EXCHANGE_INVALID;
        m->actor_index=(int32_t)get32(bytes+at); memcpy(m->name,bytes+at+4,25);
        m->size=get32(bytes+at+29); at+=33;
        if(m->size>MERCS_EXCHANGE_MEK_BYTES || m->size>size-4-at) return MERCS_EXCHANGE_INVALID;
        memcpy(m->bytes,bytes+at,m->size); at+=m->size;
    }
    if(at!=size-4 || !result_ok(launch,&staged)) return MERCS_EXCHANGE_INVALID;
    *result=staged; return 0;
}

int mercs_exchange_acknowledge(const char *overlay,const mercs_exchange_launch *launch)
{
    uint8_t bytes[EX_CONTEXT+4];
    char dir[4096];
    mercs_exchange_result *result;
    int rc;
    if(encode_launch(launch,bytes) || directory_path(overlay,launch->token,dir,sizeof dir)) return MERCS_EXCHANGE_INVALID;
    result=malloc(sizeof *result);
    if(!result) return MERCS_EXCHANGE_IO;
    rc=mercs_exchange_read(overlay,launch,result); free(result);
    if(rc) return rc;
    put32(bytes+EX_CONTEXT,checksum(bytes,EX_CONTEXT));
    return publish_bytes(dir,"APPLIED.DAT",bytes,sizeof bytes);
}

/* Titanium's wreck file beside the result (published first, so a readable
 * RESULT.DAT always has its SALVAGE.MW2 when the mission recorded one). */
int mercs_exchange_publish_salvage(const char *overlay,const mercs_exchange_launch *launch,
                                   const uint8_t *bytes,size_t size)
{
    mercs_exchange_launch saved;
    char dir[4096];
    if(!launch||!bytes||!size||mercs_exchange_load(overlay,launch->token,&saved)||
       directory_path(overlay,launch->token,dir,sizeof dir)) return MERCS_EXCHANGE_INVALID;
    return publish_bytes(dir,"SALVAGE.MW2",bytes,size);
}

int mercs_exchange_read_salvage(const char *overlay,const mercs_exchange_launch *launch,
                                uint8_t *bytes,size_t capacity,size_t *size)
{
    char dir[4096],path[4096];
    int rc;
    if(!launch||!bytes||!size) return MERCS_EXCHANGE_INVALID;
    rc=directory_path(overlay,launch->token,dir,sizeof dir);
    if(rc) return rc;
    if(path_for(dir,"SALVAGE.MW2",path,sizeof path)) return MERCS_EXCHANGE_INVALID;
    return read_bytes(path,bytes,capacity,size);
}

