#include "mercsstore.h"
#include "mercslock.h"
#include "mercsinterchange.h"
#include "mercsfile.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

static int fail(char *e,size_t n,const char *message)
{if(e&&n)snprintf(e,n,"%s",message);return -1;}
static uint32_t u32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int directory(const char *path)
{
#ifdef _WIN32
    DWORD a=GetFileAttributesA(path);
    return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT);
#else
    struct stat st;return !lstat(path,&st)&&S_ISDIR(st.st_mode)&&!S_ISLNK(st.st_mode);
#endif
}
static int mkdir_private(const char *path)
{
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path,0700);
#endif
}
static int path_join(const char *dir,const char *name,char *out,size_t cap)
{int n=snprintf(out,cap,"%s/%s",dir,name);return n<0||(size_t)n>=cap?-1:0;}
int mercs_store_directory(const char *overlay,char *out,size_t cap)
{
    if(!overlay||!out||!directory(overlay)||path_join(overlay,"MERCS-CAREERS",out,cap))return -1;
    if(mkdir_private(out)&&errno!=EEXIST)return -1;
    return directory(out)?0:-1;
}
static FILE *regular_file(const char *path,int create)
{
#ifdef _WIN32
    HANDLE h;BY_HANDLE_FILE_INFORMATION info;int fd;
    h=CreateFileA(path,create?GENERIC_WRITE:GENERIC_READ,FILE_SHARE_READ,NULL,
                 create?CREATE_NEW:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(h==INVALID_HANDLE_VALUE){DWORD e=GetLastError();errno=e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND?ENOENT:EIO;return NULL;}
    if(!GetFileInformationByHandle(h,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){CloseHandle(h);errno=EINVAL;return NULL;}
    fd=_open_osfhandle((intptr_t)h,create?(_O_WRONLY|_O_BINARY):(_O_RDONLY|_O_BINARY));
    if(fd<0){CloseHandle(h);return NULL;}
    {FILE *f=_fdopen(fd,create?"wb":"rb");if(!f)_close(fd);return f;}
#else
    int flags=create?O_WRONLY|O_CREAT|O_EXCL:O_RDONLY,fd;struct stat st;FILE *f;
#ifdef O_NOFOLLOW
    flags|=O_NOFOLLOW;
#endif
#ifdef O_CLOEXEC
    flags|=O_CLOEXEC;
#endif
    fd=open(path,flags|O_NONBLOCK,0600);if(fd<0)return NULL;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||fcntl(fd,F_SETFD,FD_CLOEXEC)<0){close(fd);errno=EINVAL;return NULL;}
    f=fdopen(fd,create?"wb":"rb");if(!f)close(fd);return f;
#endif
}
static int write_new(const char *path,const uint8_t *bytes,size_t n)
{
    FILE *f=regular_file(path,1);int bad=0;if(!f)return -1;
    if(fwrite(bytes,1,n,f)!=n||fflush(f))bad=1;
#ifdef _WIN32
    if(!bad&&_commit(_fileno(f)))bad=1;
#else
    if(!bad&&fsync(fileno(f)))bad=1;
#endif
    if(fclose(f))bad=1;
    return bad?-1:0;
}
static int read_all(const char *path,size_t limit,uint8_t **out,size_t *length)
{
    FILE *f=regular_file(path,0);long n;uint8_t *b;int bad;
    if(!f)return errno==ENOENT?1:-1;
    if(fseek(f,0,SEEK_END)||(n=ftell(f))<0||(size_t)n>limit||fseek(f,0,SEEK_SET)){fclose(f);return -1;}
    b=malloc(n?(size_t)n:1);if(!b){fclose(f);return -1;}
    bad=fread(b,1,(size_t)n,f)!=(size_t)n||fgetc(f)!=EOF||ferror(f);if(fclose(f))bad=1;
    if(bad){free(b);return -1;}*out=b;*length=(size_t)n;return 0;
}
static int sync_dir(const char *path)
{
#ifndef _WIN32
    int fd=open(path,O_RDONLY
#ifdef O_DIRECTORY
                |O_DIRECTORY
#endif
               );
    if(fd>=0){int rc=fsync(fd);if(rc)fprintf(stderr,"mercs save: directory sync: %s\n",strerror(errno));close(fd);return rc;}
    return -1;
#else
    (void)path;return 0;
#endif
}
static int slot_names(int slot,char *index,size_t ni,char *payload,size_t np)
{
    if(slot < -1||slot>9)return -1;
    if(slot<0){snprintf(index,ni,"AUTOSAVE.REF");snprintf(payload,np,"SHELLMER.CFG");}
    else{snprintf(index,ni,"SLOT%d.REF",slot);snprintf(payload,np,"mersave%d.sav",slot);}
    return 0;
}
static int token_ok(const uint8_t *p,size_t n)
{size_t i;if(n!=33||p[32]!='\n')return 0;
 for(i=0;i<32;i++)if(!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f')))return 0;
 return 1;}

int mercs_store_write(const char *dir,int slot,const mercs_campaign *c,
                       const mercs_shell_state *v,char *e,size_t n)
{return mercs_store_write_attached(dir,slot,c,v,NULL,0,e,n);}

int mercs_store_write_attached(const char *dir,int slot,const mercs_campaign *c,
                       const mercs_shell_state *v,const uint8_t *salvage,size_t salvage_size,
                       char *e,size_t n)
{
    mercs_launch_lock *lock=NULL;
    mercs_campaign snapshot;
    char index[32],payload[32],token[33],generation[4096],path[4096],temp[4096],leaf[80],index_path[4096];
    uint8_t *original=NULL,resume[1024],ref[33];size_t size;
    static uint32_t sequence;
    uint32_t seconds=(uint32_t)time(NULL),fraction,pid;
    int rc=-1,attempt;
    memset(&snapshot,0,sizeof snapshot);if(e&&n)e[0]=0;
    if(!dir||!directory(dir)||slot_names(slot,index,sizeof index,payload,sizeof payload))return fail(e,n,"Invalid save location");
    if(mercs_campaign_clone(c,&snapshot))return fail(e,n,"Out of memory staging saved career");
    if(mercs_campaign_sync(&snapshot,v,e,n))goto done;
    if(mercs_save_encode(snapshot.document,1,&original,&size)||mercs_campaign_encode_resume(&snapshot,v,original,size,resume)){
        fail(e,n,"Cannot encode this career for saving");goto done;}
    if(mercs_launch_lock_acquire(dir,&lock,e,n)!=MERCS_LOCK_OK)goto done;
#ifdef _WIN32
    {FILETIME t;GetSystemTimeAsFileTime(&t);fraction=t.dwLowDateTime;pid=(uint32_t)_getpid();}
#else
    {struct timespec t={0,0};clock_gettime(CLOCK_REALTIME,&t);fraction=(uint32_t)t.tv_nsec;pid=(uint32_t)getpid();}
#endif
    for(attempt=0;attempt<16;attempt++){
        snprintf(token,sizeof token,"%08x%08x%08x%08x",seconds,fraction,pid,++sequence);
        if(path_join(dir,token,generation,sizeof generation))goto done;
        if(!mkdir_private(generation))break;
        if(errno!=EEXIST)goto done;
    }
    if(attempt==16)goto done;
    if(path_join(generation,payload,path,sizeof path)||write_new(path,original,size)||
       path_join(generation,"NATIVE.DAT",path,sizeof path)||write_new(path,resume,sizeof resume))goto done;
    if(salvage&&salvage_size&&(path_join(generation,"SALVAGE.DAT",path,sizeof path)||write_new(path,salvage,salvage_size)))goto done;
    if(sync_dir(generation))goto done;
    memcpy(ref,token,32);ref[32]='\n';snprintf(leaf,sizeof leaf,"%s.%s.TMP",index,token);
    if(path_join(dir,leaf,temp,sizeof temp)||write_new(temp,ref,sizeof ref)||path_join(dir,index,index_path,sizeof index_path))goto done;
    /* Refuse replacing a symlink/device/directory; absent and regular indexes
     * are the only supported targets. Generation contents are immutable. */
    {FILE *f=regular_file(index_path,0);if(f)fclose(f);else if(errno!=ENOENT)goto done;}
#ifdef _WIN32
    if(!MoveFileExA(temp,index_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))goto done;
#else
    if(rename(temp,index_path))goto done;
#endif
    /* Index replacement is the commitment point. Keep the new in-memory state
     * even if this filesystem cannot provide an additional directory flush. */
    (void)sync_dir(dir);rc=0;
done:
    if(rc&&(!e||!n||!e[0]))fail(e,n,"Could not finish saving the career; the previous save remains selected");
    mercs_launch_lock_release(lock);free(original);mercs_campaign_free(&snapshot);return rc;
}

static int read_generation(const char *dir,int slot,uint8_t **original,size_t *size,uint8_t **resume,size_t *rsize)
{
    char index[32],payload[32],path[4096],generation[4096],token[33];uint8_t *ref=NULL;size_t n;int rc;
    if(!dir||!directory(dir)||slot_names(slot,index,sizeof index,payload,sizeof payload)||path_join(dir,index,path,sizeof path))return -1;
    rc=read_all(path,33,&ref,&n);if(rc)return rc;
    if(!token_ok(ref,n)){free(ref);return -1;}memcpy(token,ref,32);token[32]=0;free(ref);
    if(path_join(dir,token,generation,sizeof generation)||!directory(generation)||
       path_join(generation,payload,path,sizeof path))return -1;
    if(read_all(path,MERCS_SAVE_MAX_BYTES,original,size))return -1;
    if(path_join(generation,"NATIVE.DAT",path,sizeof path)||read_all(path,1024,resume,rsize)){
        free(*original);*original=NULL;return -1;}
    return 0;
}
int mercs_store_read(const char *dir,int slot,prj_archive *a,mercs_campaign *c,
                      mercs_shell_state *v,char *e,size_t n)
{
    uint8_t *original=NULL,*resume=NULL;size_t size,rsize;int rc;
    rc=read_generation(dir,slot,&original,&size,&resume,&rsize);
    if(rc<0)return fail(e,n,"The selected saved career is incomplete or invalid");
    if(rc==1)return 1;
    rc=mercs_campaign_decode_resume(a,original,size,resume,rsize,c,v,e,n);
    free(original);free(resume);return rc;
}
int mercs_store_read_salvage(const char *dir,int slot,uint8_t *out,size_t capacity,size_t *size)
{
    char index[32],payload[32],path[4096],generation[4096];uint8_t *ref=NULL,*bytes=NULL;size_t n;int rc;
    if(size)*size=0;
    if(!out||!size||!dir||!directory(dir)||slot_names(slot,index,sizeof index,payload,sizeof payload)||
       path_join(dir,index,path,sizeof path))return -1;
    rc=read_all(path,33,&ref,&n);if(rc)return rc;
    if(!token_ok(ref,n)){free(ref);return -1;}ref[32]=0;
    rc=path_join(dir,(const char *)ref,generation,sizeof generation);free(ref);
    if(rc||!directory(generation)||path_join(generation,"SALVAGE.DAT",path,sizeof path))return -1;
    rc=read_all(path,capacity,&bytes,&n);if(rc)return rc;
    memcpy(out,bytes,n);*size=n;free(bytes);return 0;
}
int mercs_store_title(const char *dir,int slot,char *out,size_t capacity)
{
    uint8_t *original=NULL,*resume=NULL;size_t size,rsize;mercs_save_state *d=NULL;int rc;
    if(!out||!capacity)return -1;
    out[0]=0;
    rc=read_generation(dir,slot,&original,&size,&resume,&rsize);if(rc)return rc;
    if(rsize!=1024||memcmp(resume,"MRCSRES2",8)||u32(resume+8)!=size||
       u32(resume+12)!=mercs_campaign_crc(original,size)||
       u32(resume+1020)!=mercs_campaign_crc(resume,1020)||mercs_save_decode(original,size,&d)||
       !memchr(mercs_save_header(d)+2,0,256))rc=-1;
    /* DOS0x6ad36 reads the saved description, not a synthesized pilot label. */
    else snprintf(out,capacity,"%s",(const char *)mercs_save_header(d)+2);
    mercs_save_free(d);free(original);free(resume);return rc;
}

int mercs_store_original_title(const char *path,char *out,size_t capacity)
{
    uint8_t *bytes=NULL;size_t size;mercs_save_state *d=NULL;int rc;
    if(!path||!out||!capacity)return -1;
    out[0]=0;rc=read_all(path,MERCS_SAVE_MAX_BYTES,&bytes,&size);if(rc)return rc;
    if(mercs_save_decode(bytes,size,&d)||!memchr(mercs_save_header(d)+2,0,256))rc=-1;
    else snprintf(out,capacity,"%s",(const char*)mercs_save_header(d)+2);
    free(bytes);mercs_save_free(d);return rc;
}

int mercs_store_import_original(const char *path,prj_archive *a,mercs_campaign *c,
                                 mercs_shell_state *v,uint32_t *event,char *e,size_t n)
{
    uint8_t *bytes=NULL;size_t size;int rc;
    if(!path)return fail(e,n,"Missing original saved-game filename");
    rc=read_all(path,MERCS_SAVE_MAX_BYTES,&bytes,&size);
    if(rc<0)return fail(e,n,"The original saved-game file is unreadable or invalid");
    if(rc)return rc;
    rc=mercs_interchange_import(a,bytes,size,c,v,event,e,n);free(bytes);return rc;
}

int mercs_store_export_original(const char *dir,int slot,const char *overlay,char *e,size_t n)
{
    uint8_t *original=NULL,*resume=NULL;size_t size,rsize;mercs_save_state *d=NULL;
    char name[32];int rc=-1;
    if(slot<0||slot>9)return fail(e,n,"Only manual slots can be exported to the original game");
    if(read_generation(dir,slot,&original,&size,&resume,&rsize)||rsize!=1024||memcmp(resume,"MRCSRES2",8)||
       u32(resume+8)!=size||u32(resume+12)!=mercs_campaign_crc(original,size)||
       u32(resume+1020)!=mercs_campaign_crc(resume,1020)||mercs_save_decode(original,size,&d)||
       !mercs_interchange_screen_valid(u32(mercs_save_header(d)+0x23c7))){
        fail(e,n,"The selected native generation cannot be exported as an original saved game");goto done;
    }
    snprintf(name,sizeof name,"mersave%d.sav",slot);
    rc=mercs_atomic_write(overlay,name,original,size,e,n);
done:free(original);free(resume);mercs_save_free(d);return rc;
}

int mercs_store_original_differs(const char *dir,int slot,const char *path)
{
    uint8_t *original=NULL,*resume=NULL,*loose=NULL;size_t size,rsize,lsize;int rc;
    rc=read_generation(dir,slot,&original,&size,&resume,&rsize);free(resume);
    if(rc)return rc<0?-1:1;
    if(read_all(path,MERCS_SAVE_MAX_BYTES,&loose,&lsize)){free(original);return -1;}
    rc=lsize!=size||memcmp(original,loose,size)!=0;free(original);free(loose);return rc;
}
