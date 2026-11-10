/* Test the real PRJ read boundary against supplied ASA00STD/JEN50STD records, then
 * temporary miniature archives with mutations. No game payload is embedded
 * and original files are opened read-only. */
#include "prj.h"
#include "mek.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned rd16(const unsigned char *p) { return p[0] | (unsigned)p[1] << 8; }
static uint32_t rd32(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void wr16(unsigned char *p, unsigned x) { p[0]=(unsigned char)x; p[1]=(unsigned char)(x>>8); }
static void wr32(unsigned char *p, uint32_t x)
{
    p[0]=(unsigned char)x; p[1]=(unsigned char)(x>>8);
    p[2]=(unsigned char)(x>>16); p[3]=(unsigned char)(x>>24);
}
static uint32_t checksum(const unsigned char *record, size_t size)
{
    uint32_t sum=0;
    size_t i;
    for (i=12; i<size; i++) sum+=record[i];
    return sum;
}

static void original_record(const char *path,unsigned id,const char *name,
                             size_t size,unsigned char *out)
{
    FILE *fp=fopen(path,"rb");
    unsigned char header[26], entry[24], slot[8];
    unsigned t, count;
    int found=0;
    assert(fp && fread(header,1,sizeof header,fp)==sizeof header);
    assert(memcmp(header,"PROJ",4)==0);
    count=rd16(header+24);
    for (t=0; t<count; t++) {
        assert(fseek(fp,26+(long)t*24,SEEK_SET)==0);
        assert(fread(entry,1,sizeof entry,fp)==sizeof entry);
        if (memcmp(entry,"MEK\0",4)) continue;
        assert(fseek(fp,(long)rd32(entry+4)+22+(long)id*8,SEEK_SET)==0);
        assert(fread(slot,1,sizeof slot,fp)==sizeof slot && rd32(slot+4)==size);
        assert(fseek(fp,(long)rd32(slot),SEEK_SET)==0);
        assert(fread(out,1,size,fp)==size);
        assert(memcmp(out,"DATA",4)==0 && rd16(out+24)==id);
        assert(memcmp(out+30,name,strlen(name)+1)==0);
        found++;
    }
    assert(found==1);
    fclose(fp);
}

static void mini_archive(const unsigned char *record,size_t size,unsigned id,const char tag[4],
                         int wanted, int compat, const char *label)
{
    /* One type, enough INDX slots for the original id, one SYMB entry. */
    enum { INDEX_AT=50, SYMBOL_SIZE=40 };
    unsigned index_size=22+(id+1)*8,symbol_at=INDEX_AT+index_size,data_at=symbol_at+SYMBOL_SIZE;
    unsigned char data[50+22+249*8+40+504]={0};
    char path[]="/tmp/mercs-prj-checksum-XXXXXX", error[128];
    prj_record decoded;
    prj_archive *archive;
    int fd=mkstemp(path), rc;
    FILE *fp;
    assert(fd>=0 && size<=504 && id<=248);
    memcpy(data,"PROJ",4); wr32(data+4,(uint32_t)(data_at+size-8));
    memcpy(data+12,"DDIT",4); wr16(data+24,1);
    memcpy(data+26,tag,4); wr32(data+30,INDEX_AT); wr32(data+34,index_size);
    wr32(data+38,symbol_at); wr32(data+42,SYMBOL_SIZE);
    memcpy(data+INDEX_AT,"INDX",4); wr32(data+INDEX_AT+4,index_size-8);
    memcpy(data+INDEX_AT+12,tag,4); wr16(data+INDEX_AT+16,id+1); wr16(data+INDEX_AT+18,1);
    wr32(data+INDEX_AT+22+id*8,data_at); wr32(data+INDEX_AT+26+id*8,(uint32_t)size);
    memcpy(data+symbol_at,"SYMB",4); wr32(data+symbol_at+4,SYMBOL_SIZE-8);
    memcpy(data+symbol_at+12,tag,4); wr16(data+symbol_at+16,1); wr16(data+symbol_at+18,1);
    memcpy(data+symbol_at+22,record+30,16); wr16(data+symbol_at+38,id);
    memcpy(data+data_at,record,size);
    fp=fdopen(fd,"wb"); assert(fp);
    assert(fwrite(data,1,data_at+size,fp)==data_at+size && fclose(fp)==0);
    archive=prj_open(path,error,sizeof error);
    if (!archive) fprintf(stderr,"%s: cannot open miniature archive: %s\n",label,error);
    assert(archive);
    rc=prj_read(archive,0,(int)id,&decoded);
    if (rc!=wanted) fprintf(stderr,"%s: result %d, expected %d\n",label,rc,wanted);
    assert(rc==wanted);
    if (rc==PRJ_OK) {
        assert(decoded.checksum==rd32(record+8));
        assert(decoded.computed_checksum==checksum(record,size));
        assert(decoded.checksum_compat==compat);
        assert(decoded.size==size-PRJ_RECORD_HEADER);
        assert(memcmp(decoded.data,record+PRJ_RECORD_HEADER,decoded.size)==0);
        prj_record_free(&decoded);
    }
    prj_close(archive); assert(unlink(path)==0);
}

static void check_record(int argc,char **argv,const char *name,unsigned id,
                          size_t size,uint32_t stored,uint32_t computed,
                          int items,int links)
{
    unsigned char retail[504]={0}, accelerated[504]={0}, candidate[504]={0};
    int i, mutations=0;
    original_record(argv[1],id,name,size,retail);
    original_record(argv[2],id,name,size,accelerated);
    assert(checksum(retail,size)==rd32(retail+8));
    assert(rd32(accelerated+8)==stored && checksum(accelerated,size)==computed);
    if(id==6)assert(memcmp(retail+62,accelerated+62,size-62)==0);
    else assert(memcmp(retail+62,accelerated+62,size-62)!=0);
    for (i=1; i<argc; i++) {
        char error[128];
        prj_archive *archive=prj_open(argv[i],error,sizeof error);
        prj_record record;
        mek_def mech;
        assert(archive);
        original_record(argv[i],id,name,size,candidate);
        assert(memcmp(candidate,i==1 ? retail : accelerated,size)==0);
        assert(prj_read_named(archive,"MEK",name,&record)==PRJ_OK);
        assert(record.checksum_compat==(i>1));
        assert(record.checksum==rd32(candidate+8));
        assert(record.computed_checksum==checksum(candidate,size));
        assert(record.size==size-62 && memcmp(record.data,candidate+62,size-62)==0);
        assert(mek_parse(record.data,record.size,&mech)==0);
        assert(mech.item_count==items && mech.link_count==links);
        if(id==248){
            assert(mech.loc[MEK_HEAD].armor==(uint32_t)(i==1?7:0));
            assert(mech.loc[MEK_RL].armor==(uint32_t)(i==1?0:6));
            assert(mech.loc[MEK_LL].armor==(uint32_t)(i==1?0:6));
            assert(mech.loc[MEK_RL].internal==(uint32_t)(i==1?0:8));
            assert(mech.loc[MEK_LL].internal==(uint32_t)(i==1?0:8));
        }
        prj_record_free(&record); prj_close(archive);
    }
    mini_archive(retail,size,id,"MEK\0",PRJ_OK,0,"valid retail");
    mini_archive(accelerated,size,id,"MEK\0",PRJ_OK,1,"known accelerated record");

#define MUTATE(label, change, length, tag, expected) do { \
    memcpy(candidate,accelerated,size); change; \
    mini_archive(candidate,length,id,tag,expected,0,label); mutations++; \
} while (0)
    MUTATE("payload one byte",candidate[62]++,size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("payload equal-sum pair",candidate[62]++;candidate[66]--,size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("payload equal-sum last block",candidate[size-2]++;candidate[size-1]--,size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("stamp equal-sum pair",candidate[16]++;candidate[17]--,size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("name equal-sum pair",candidate[30]++;candidate[31]--,size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("filename equal-sum pair",candidate[46]++;candidate[47]--,size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("name NUL boundary",candidate[38]++;candidate[16]--,size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("name padded last byte",candidate[45]++;candidate[16]--,size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("filename padded last byte",candidate[61]++;candidate[16]--,size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("unused header equal-sum pair",candidate[28]++;candidate[16]--,size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("wrong stored checksum",wr32(candidate+8,stored-1),size,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("different record id",wr16(candidate+24,id+1),size,"MEK\0",PRJ_ERR_FORMAT);
    MUTATE("wrong record size",wr32(candidate+4,(uint32_t)size-9),size-1,"MEK\0",PRJ_ERR_CHECKSUM);
    MUTATE("other resource type",memcpy(candidate+12,"BWD\0",4),size,"BWD\0",PRJ_ERR_CHECKSUM);
    MUTATE("invalid declared size",wr32(candidate+4,(uint32_t)size-7),size,"MEK\0",PRJ_ERR_FORMAT);
#undef MUTATE
    /* Ordinary checksum-valid records remain valid; the special fingerprint
     * is not a new restriction on unrelated records or corrected headers. */
    memcpy(candidate,accelerated,size);
    wr32(candidate+8,checksum(candidate,size));
    mini_archive(candidate,size,id,"MEK\0",PRJ_OK,0,"valid recomputed checksum");
    candidate[62]++;
    wr32(candidate+8,checksum(candidate,size));
    mini_archive(candidate,size,id,"MEK\0",PRJ_OK,0,"different checksum-valid payload");
    printf("PASS: %d original %s records load unchanged; exact known checksum anomaly marked, %d mutations rejected, ordinary valid checksums retained\n",
           argc-1,name,mutations);
}
int main(int argc,char **argv)
{
    if(argc<3){fprintf(stderr,"usage: %s retail-MW2.PRJ accelerated-archive [accelerated-archive ...]\n",argv[0]);return 2;}
    check_record(argc,argv,"ASA00STD",6,496,0x41f3,0x41f4,3,2);
    check_record(argc,argv,"JEN50STD",248,504,0x460c,0x4763,5,1);
    {   /* every indexed record of every supplied edition loads (the Titanium/3Dfx MON00STD stopped IVA1) */
        int i,t,id,records=0,compat=0,fails=0,broken=0;char e[256];
        for(i=1;i<argc;i++){
            prj_archive *a=prj_open(argv[i],e,sizeof e);const char *tag;assert(a);
            for(t=0;(tag=prj_type_tag(a,t))!=NULL;t++)
                for(id=1;id<0xf000;id++){
                    prj_record r;int rc;
                    if(!prj_record_size(a,t,id))continue;
                    rc=prj_read(a,t,id,&r);
                    /* the 3Dfx and Titanium archives' BLD01STD and SCDCHRCC slots point at other data (no DATA
                     * header); nothing else in any archive names them, and the port refuses them as malformed */
                    if(rc==PRJ_ERR_FORMAT&&(prj_find_id(a,t,"BLD01STD")==id||prj_find_id(a,t,"SCDCHRCC")==id)){broken++;continue;}
                    if(rc){fprintf(stderr,"%s: %s %d does not load (%d)\n",argv[i],tag,id,rc);fails++;continue;}
                    records++;compat+=r.checksum_compat;prj_record_free(&r);
                }
            prj_close(a);
        }
        if(fails)return 1;
        printf("PASS: %d indexed records load (%d shipped with a known wrong checksum); %d known broken slots refused\n",records,compat,broken);
    }
    return 0;
}
