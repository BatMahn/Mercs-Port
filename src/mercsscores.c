#include "mercsscores.h"
#include "mercsdebrief.h"
#include "mercsfile.h"
#include "mercslock.h"
#include <ctype.h>
#include <inttypes.h>

enum { SCORE_BYTES=MERCS_LORDS_FILE_BYTES, JOURNAL_BYTES=1510,
       J_TOKEN=8,J_CANDIDATE=40,J_PRESENT=109,J_CHANGED=110,
       J_BEFORE=114,J_AFTER=808,J_CHECKSUM=1502 };
static const char score_name[]="mw2high.cfg";

static uint32_t score_u32(const unsigned char *b)
{return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static int32_t score_signed(uint32_t u)
{int32_t s;memcpy(&s,&u,4);return s;}
static void score_p32(unsigned char *b,uint32_t u)
{unsigned i;for(i=0;i<4;i++)b[i]=(unsigned char)(u>>(i*8));}
static uint64_t score_hash(const unsigned char *b,size_t n)
{uint64_t h=UINT64_C(14695981039346656037);size_t i;for(i=0;i<n;i++)h=(h^b[i])*UINT64_C(1099511628211);return h;}
static int score_error(char *e,size_t n,const char *message)
{if(e&&n)snprintf(e,n,"%s",message);return -1;}
static int score_token(const char *token)
{size_t i;if(!token||strlen(token)!=32)return 0;for(i=0;i<32;i++)if(!isxdigit((unsigned char)token[i]))return 0;return 1;}
static void score_row_bytes(const mercs_lords_row *row,unsigned char out[69])
{memcpy(out,row->pilot,32);memcpy(out+32,row->group,32);score_p32(out+64,(uint32_t)row->wealth);out[68]=0;}

int mercs_scores_document_token(const mercs_save_state *career,char token[33])
{
    unsigned char *bytes=NULL;size_t size,i;uint64_t a,b=UINT64_C(0x73636f72652d656e);
    if(!token||mercs_save_encode(career,1,&bytes,&size))return -1;
    a=score_hash(bytes,size);
    for(i=size;i>0;i--)b=(b^bytes[i-1])*UINT64_C(1099511628211);
    free(bytes);snprintf(token,33,"%016" PRIx64 "%016" PRIx64,a,b);return 0;
}

int mercs_scores_earned(const mercs_save_state *career,mercs_lords_row *out)
{
    const uint8_t *h=mercs_save_header(career);
    mercs_lords_row row;
    uint32_t value;
    size_t i,j;
    int32_t year;
    if(!h||!out||!memchr(h+0x102,0,32))return -1;
    memset(&row,0,sizeof row);
    if(mercs_save_player(career,NULL,NULL,row.pilot,sizeof row.pilot))return -1;
    memcpy(row.group,h+0x102,32);year=score_signed(score_u32(h+0x122));
    value=score_u32(h+0x12a);
    for(i=0;i<mercs_save_list_count(career,MERCS_SAVE_OWNED_MECHS);i++) {
        mercs_career_mech mech;int32_t price;
        memcpy(mech.bytes,mercs_save_list_record(career,MERCS_SAVE_OWNED_MECHS,i),sizeof mech.bytes);
        if(mercs_debrief_mech_value(&mech,year,&price))return -1;
        value+=(uint32_t)price;
    }
    for(j=0;j<2;j++) {
        enum mercs_save_list list=j?MERCS_SAVE_OWNED_AMMO:MERCS_SAVE_OWNED_WEAPONS;
        for(i=0;i<mercs_save_list_count(career,list);i++)
            value+=(uint32_t)mercs_debrief_item_value(mercs_save_list_record(career,list,i));
    }
    row.wealth=score_signed(value);*out=row;return 0;
}

/*0 present,1 absent,-1 unreadable. Extra/truncated data never partly loads. */
static int score_read(const char *directory,const char *name,unsigned char *out,size_t size)
{
    char path[2048];FILE *f;size_t n;int tail,bad;
    if(dp_resolve_in(directory,name,path,sizeof path))return 1;
    f=fopen(path,"rb");if(!f)return -1;
    n=fread(out,1,size,f);tail=fgetc(f);bad=ferror(f);if(fclose(f))bad=1;
    return n==size&&tail==EOF&&!bad?0:-1;
}
static int score_fallback(const char *path,const mercs_lords_table *defaults,
                            mercs_lords_table *out)
{
    unsigned char raw[SCORE_BYTES];FILE *f;size_t n;int tail,bad;
    if(!path||!path[0]){if(mercs_lords_encode(defaults,raw))return -1;*out=*defaults;return 0;}
    f=fopen(path,"rb");if(!f)return -1;n=fread(raw,1,sizeof raw,f);tail=fgetc(f);
    bad=ferror(f);if(fclose(f))bad=1;
    return n==sizeof raw&&tail==EOF&&!bad?mercs_lords_decode(raw,sizeof raw,out):-1;
}
int mercs_scores_load(const char *overlay,const char *fallback,const mercs_lords_table *defaults,
                       mercs_lords_table *out,char *e,size_t n)
{
    unsigned char raw[SCORE_BYTES];mercs_lords_table next;int rc;
    if(!overlay||!out)return score_error(e,n,"The Merc Lords score folder is unavailable.");
    rc=score_read(overlay,score_name,raw,sizeof raw);
    if(rc<0||(rc==0&&mercs_lords_decode(raw,sizeof raw,&next))||
       (rc==1&&score_fallback(fallback,defaults,&next)))
        return score_error(e,n,"The Merc Lords score file is incomplete or unreadable.");
    *out=next;return 0;
}

static void journal_seal(unsigned char b[JOURNAL_BYTES])
{uint64_t h=score_hash(b,J_CHECKSUM);unsigned i;for(i=0;i<8;i++)b[J_CHECKSUM+i]=(unsigned char)(h>>(8*i));}
static int journal_valid(const unsigned char b[JOURNAL_BYTES])
{
    unsigned char copy[JOURNAL_BYTES];mercs_lords_table t;char token[33];
    memcpy(copy,b,sizeof copy);journal_seal(copy);memcpy(token,b+J_TOKEN,32);token[32]=0;
    return !memcmp(b,"MSCORE01",8)&&score_token(token)&&b[J_PRESENT]<=1&&b[J_CHANGED]<=1&&
           !b[111]&&!b[112]&&!b[113]&&!memcmp(b+J_CHECKSUM,copy+J_CHECKSUM,8)&&
           memchr(b+J_CANDIDATE,0,32)&&memchr(b+J_CANDIDATE+32,0,32)&&
           !mercs_lords_decode(b+J_BEFORE,SCORE_BYTES,&t)&&!mercs_lords_decode(b+J_AFTER,SCORE_BYTES,&t);
}

static int score_recover(const char *overlay,const char *receipts,char *e,size_t n)
{
    unsigned char journal[JOURNAL_BYTES],current[SCORE_BYTES],receipt[JOURNAL_BYTES];
    char token[33],name[40],path[2048];int rc,present,done;
    rc=score_read(receipts,"PENDING.DAT",journal,sizeof journal);
    if(rc==1)return 0;
    if(rc||!journal_valid(journal))return score_error(e,n,"The pending Merc Lords result is incomplete or invalid.");
    memcpy(token,journal+J_TOKEN,32);token[32]=0;snprintf(name,sizeof name,"%s.DAT",token);
    done=score_read(receipts,name,receipt,sizeof receipt);
    if(done<0||(done==0&&memcmp(receipt,journal,sizeof receipt)))
        return score_error(e,n,"The Merc Lords result receipt is incomplete or mismatched.");
    if(done==1) {
        present=score_read(overlay,score_name,current,sizeof current);
        if(present<0)return score_error(e,n,"The Merc Lords score file could not be read during recovery.");
        if(journal[J_CHANGED]) {
            int applied=present==0&&!memcmp(current,journal+J_AFTER,SCORE_BYTES);
            int before=journal[J_PRESENT]?(present==0&&!memcmp(current,journal+J_BEFORE,SCORE_BYTES)):present==1;
            if(!applied&&!before)return score_error(e,n,"The Merc Lords scores changed while a result was pending; the existing scores were preserved.");
            if(!applied&&mercs_atomic_write(overlay,score_name,journal+J_AFTER,SCORE_BYTES,e,n))return -1;
        }
        if(mercs_atomic_write(receipts,name,journal,sizeof journal,e,n))return -1;
    }
    if(dp_resolve_in(receipts,"PENDING.DAT",path,sizeof path)||remove(path))
        return score_error(e,n,"The completed Merc Lords result could not be finalized. Try again.");
    return 0;
}

int mercs_scores_commit(const char *overlay,const char *fallback,const mercs_lords_table *defaults,
                         const char *token,const mercs_lords_row *earned,char *e,size_t n)
{
    char receipts[2048],name[40];unsigned char journal[JOURNAL_BYTES],candidate[69],raw[SCORE_BYTES];
    mercs_launch_lock *lock=NULL;mercs_lords_table table;int rc=-1,present,changed,done;
    if(!score_token(token)||!earned||!memchr(earned->pilot,0,32)||!memchr(earned->group,0,32))
        return score_error(e,n,"The earned Merc Lords result has no valid career-ending identity.");
    if(mercs_file_directory(overlay,"MERCS-SCORES",receipts,sizeof receipts,e,n))return -1;
    if(mercs_launch_lock_acquire(receipts,&lock,e,n))
        return score_error(e,n,"Another Mercenaries window is updating Merc Lords. Try again when it finishes.");
    if(score_recover(overlay,receipts,e,n))goto finish;
    score_row_bytes(earned,candidate);snprintf(name,sizeof name,"%s.DAT",token);
    done=score_read(receipts,name,journal,sizeof journal);
    if(done==0) {
        if(journal_valid(journal)&&!memcmp(journal+J_TOKEN,token,32)&&!memcmp(journal+J_CANDIDATE,candidate,69))rc=0;
        else score_error(e,n,"This career-ending identity already belongs to a different Merc Lords result.");
        goto finish;
    }
    if(done<0){score_error(e,n,"The Merc Lords result receipt is unreadable.");goto finish;}
    present=score_read(overlay,score_name,raw,sizeof raw);
    if(present<0){score_error(e,n,"The Merc Lords score file is incomplete or unreadable.");goto finish;}
    if(mercs_scores_load(overlay,fallback,defaults,&table,e,n))goto finish;
    memset(journal,0,sizeof journal);memcpy(journal,"MSCORE01",8);memcpy(journal+J_TOKEN,token,32);
    memcpy(journal+J_CANDIDATE,candidate,69);journal[J_PRESENT]=(unsigned char)(present==0);
    if(mercs_lords_encode(&table,journal+J_BEFORE))goto finish;
    changed=mercs_lords_submit(&table,earned);if(changed<0)goto finish;
    journal[J_CHANGED]=(unsigned char)changed;
    if(changed){if(mercs_lords_encode(&table,journal+J_AFTER))goto finish;}
    else memcpy(journal+J_AFTER,journal+J_BEFORE,SCORE_BYTES);
    journal_seal(journal);
    if(mercs_atomic_write(receipts,"PENDING.DAT",journal,sizeof journal,e,n))goto finish;
    rc=score_recover(overlay,receipts,e,n);
finish:
    mercs_launch_lock_release(lock);return rc;
}
