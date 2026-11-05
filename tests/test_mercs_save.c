#include "mercssave.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
static uint8_t *read_file(const char *path,size_t *size)
{
    FILE *f=fopen(path,"rb");long length;uint8_t *data;
    assert(f);assert(!fseek(f,0,SEEK_END));length=ftell(f);assert(length>0);
    assert(!fseek(f,0,SEEK_SET));data=(uint8_t *)malloc((size_t)length+1);assert(data);
    assert(fread(data,1,(size_t)length,f)==(size_t)length);assert(!fclose(f));
    *size=(size_t)length;return data;
}
static void same_encoding(const mercs_save_state *s,const uint8_t *expected,size_t size)
{
    uint8_t *data=NULL;size_t length=0;
    assert(!mercs_save_encode(s,0,&data,&length));assert(length==size);
    assert(!memcmp(data,expected,size));free(data);
}
static void check_codec(const char *directory)
{
    char path[1024];uint8_t *input,*expected,*restored,*encoded,*copy,*before,*after;
    size_t input_size,size,restored_size,length,before_size,after_size,at,i;mercs_save_state *s,*decoded=NULL,*clone,*sentinel;
    snprintf(path,sizeof path,"%s/writer-all-lists.input",directory);input=read_file(path,&input_size);
    snprintf(path,sizeof path,"%s/writer-all-lists.expected",directory);expected=read_file(path,&size);
    snprintf(path,sizeof path,"%s/restore-all-lists.expected",directory);restored=read_file(path,&restored_size);
    assert(size==input_size);s=mercs_save_create();assert(s);at=MERCS_SAVE_HEADER_BYTES;
    for(i=0;i<MERCS_SAVE_LIST_COUNT;i++){
        assert(!mercs_save_set_list(s,(enum mercs_save_list)i,input+at,1));at+=mercs_save_list_stride((enum mercs_save_list)i);
    }
    assert(at==size);memcpy(mercs_save_header_edit(s),input,MERCS_SAVE_HEADER_BYTES);
    assert(!mercs_save_encode(s,1,&encoded,&length));assert(length==size);
    assert(!memcmp(encoded,expected,size));assert(!memcmp(mercs_save_header(s),input,MERCS_SAVE_HEADER_BYTES));free(encoded);
    /* Pure encoding must reject inconsistent raw header counts. */
    assert(mercs_save_encode(s,0,&encoded,&length)==MERCS_SAVE_INVALID);
    assert(!mercs_save_decode(expected,size,&decoded));same_encoding(decoded,expected,size);
    clone=mercs_save_clone(decoded);assert(clone);same_encoding(clone,expected,size);
    mercs_save_header_edit(clone)[0x12a]^=0x80;
    mercs_save_list_record_edit(clone,MERCS_SAVE_OWNED_REFERENCES,0)[0x200]^=0x40;
    same_encoding(decoded,expected,size);mercs_save_free(clone);

    copy=(uint8_t *)malloc(size+1);assert(copy);memcpy(copy,expected,size);copy[size]=1;
    sentinel=decoded;
    assert(mercs_save_decode(copy,size+1,&sentinel)==MERCS_SAVE_INVALID);assert(sentinel==decoded);
    for(i=0;i<size;i++){
        assert(mercs_save_decode(copy,i,&sentinel)==MERCS_SAVE_INVALID);assert(sentinel==decoded);
    }
    put32(copy+0x12e,UINT32_MAX);
    assert(mercs_save_decode(copy,size,&sentinel)==MERCS_SAVE_INVALID);assert(sentinel==decoded);
    memcpy(copy,expected,size);put32(copy+0x146,MERCS_SAVE_MAX_LIST_RECORDS+1);
    assert(mercs_save_decode(copy,size,&sentinel)==MERCS_SAVE_INVALID);
    memcpy(copy,expected,size);put32(copy+0x146,2);
    assert(mercs_save_decode(copy,size,&sentinel)==MERCS_SAVE_INVALID);
    assert(mercs_save_set_list(decoded,MERCS_SAVE_OWNED_PILOTS,NULL,1)==MERCS_SAVE_INVALID);
    same_encoding(decoded,expected,size);
    assert(mercs_save_set_owned(decoded,NULL,1)==MERCS_SAVE_INVALID);same_encoding(decoded,expected,size);
    assert(mercs_save_set_list(decoded,MERCS_SAVE_OWNED_MECHS,NULL,0)==MERCS_SAVE_OK);
    assert(mercs_save_encode(decoded,1,&encoded,&length)==MERCS_SAVE_INVALID);
    mercs_save_free(decoded);decoded=NULL;assert(!mercs_save_decode(expected,size,&decoded));

    /* Reload is lossless and never pays money, consumes results, replaces
     * references, or replays settlement. Marker restoration is idempotent. */
    assert(!mercs_save_encode(decoded,0,&before,&before_size));
    mercs_save_restore_markers(decoded);assert(!mercs_save_encode(decoded,0,&after,&after_size));
    assert(before_size==after_size);at=MERCS_SAVE_HEADER_BYTES;
    for(i=0;i<MERCS_SAVE_OWNED_MECHS;i++)at+=mercs_save_list_stride((enum mercs_save_list)i);
    before[0]=0;before[at+0x84b]=0;assert(!memcmp(before,after,before_size));
    assert(restored_size==after_size&&!memcmp(restored,after,after_size));
    mercs_save_restore_markers(decoded);same_encoding(decoded,after,after_size);
    for(i=0;i<3;i++){
        clone=NULL;assert(!mercs_save_decode(after,after_size,&clone));same_encoding(clone,after,after_size);mercs_save_free(clone);
    }
    free(before);free(after);free(copy);free(input);free(expected);free(restored);mercs_save_free(decoded);mercs_save_free(s);
}

static void check_player_and_transfer(void)
{
    mercs_save_state *s=mercs_save_create(),*clone;mercs_career_mech_pair pairs[2];
    uint8_t pilots[MERCS_SAVE_PILOT_BYTES*2]={0},shop[MERCS_CAREER_MECH_BYTES]={0};
    uint8_t item[MERCS_SAVE_ITEM_BYTES]={0},*before,*after;size_t before_size,after_size,index;uint32_t id;char name[32];
    assert(s);memset(pairs,0,sizeof pairs);
    memcpy(pilots,"PLAYER",7);pilots[0x2064]=1;put32(pilots+0x24,123);put32(pilots+0x20,7);
    memcpy(pilots+MERCS_SAVE_PILOT_BYTES,"NPC",4);
    assert(!mercs_save_set_list(s,MERCS_SAVE_OWNED_PILOTS,pilots,2));
    assert(!mercs_save_player(s,&index,&id,name,sizeof name));assert(index==0&&id==123&&!strcmp(name,"PLAYER"));
    assert(mercs_save_player(s,NULL,NULL,name,6)==MERCS_SAVE_INVALID);
    assert(!mercs_save_player_assignment(s,-1));
    assert(get32(mercs_save_list_record(s,MERCS_SAVE_OWNED_PILOTS,0)+0x20)==UINT32_MAX);
    assert(!memcmp(mercs_save_list_record(s,MERCS_SAVE_OWNED_PILOTS,1),pilots+MERCS_SAVE_PILOT_BYTES,MERCS_SAVE_PILOT_BYTES));
    mercs_save_list_record_edit(s,MERCS_SAVE_OWNED_PILOTS,1)[0x2064]=1;
    assert(mercs_save_player_assignment(s,9)==MERCS_SAVE_INVALID);
    mercs_save_list_record_edit(s,MERCS_SAVE_OWNED_PILOTS,1)[0x2064]=0;
    put32(pairs[0].current.bytes+0x810,7);put32(pairs[0].current.bytes+0x814,1);
    put32(pairs[1].current.bytes+0x810,8);put32(pairs[1].current.bytes+0x814,0);
    pairs[0].reference=pairs[0].current;pairs[1].reference=pairs[1].current;
    pairs[0].reference.bytes[0x200]=99;
    assert(!mercs_save_set_owned(s,pairs,2));
    put32(shop+0x810,9);shop[0x200]=55;
    assert(!mercs_save_set_list(s,MERCS_SAVE_SHOP_MECHS,shop,1));
    shop[0x200]=88;assert(!mercs_save_set_list(s,MERCS_SAVE_SHOP_REFERENCES,shop,1));
    memset(pilots,0,sizeof pilots);memcpy(pilots,"NEW PILOT",10);put32(pilots+0x24,UINT32_MAX);
    assert(!mercs_save_set_list(s,MERCS_SAVE_SHOP_PILOTS,pilots,1));
    item[8]=0;assert(!mercs_save_set_list(s,MERCS_SAVE_SHOP_WEAPONS,item,1));
    item[8]=1;assert(!mercs_save_set_list(s,MERCS_SAVE_SHOP_AMMO,item,1));
    put32(mercs_save_header_edit(s)+0x15e,124);
    clone=mercs_save_clone(s);assert(clone);
    /* Missing reference for retained identity is rejected atomically. */
    put32(mercs_save_list_record_edit(clone,MERCS_SAVE_OWNED_REFERENCES,0)+0x810,99);
    assert(!mercs_save_encode(clone,0,&before,&before_size));
    assert(mercs_save_transfer_training(clone)==MERCS_SAVE_INVALID);
    assert(!mercs_save_encode(clone,0,&after,&after_size));
    assert(before_size==after_size&&!memcmp(before,after,before_size));
    free(before);free(after);mercs_save_free(clone);
    assert(!mercs_save_transfer_training(s));
    assert(mercs_save_list_count(s,MERCS_SAVE_OWNED_MECHS)==2);
    assert(!memcmp(mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,0),pairs[0].current.bytes,MERCS_CAREER_MECH_BYTES));
    assert(!memcmp(mercs_save_list_record(s,MERCS_SAVE_OWNED_REFERENCES,0),pairs[0].reference.bytes,MERCS_CAREER_MECH_BYTES));
    assert(get32(mercs_save_list_record(s,MERCS_SAVE_OWNED_MECHS,1)+0x810)==9);
    assert(mercs_save_list_record(s,MERCS_SAVE_OWNED_REFERENCES,1)[0x200]==55);
    assert(mercs_save_list_count(s,MERCS_SAVE_OWNED_PILOTS)==3);
    assert(get32(mercs_save_list_record(s,MERCS_SAVE_OWNED_PILOTS,2)+0x24)==124);
    assert(get32(mercs_save_header(s)+0x15e)==125);
    assert(mercs_save_list_count(s,MERCS_SAVE_OWNED_WEAPONS)==1&&mercs_save_list_count(s,MERCS_SAVE_OWNED_AMMO)==1);
    assert(mercs_save_list_count(s,MERCS_SAVE_SHOP_MECHS)==0&&mercs_save_list_count(s,MERCS_SAVE_SHOP_REFERENCES)==0);
    assert(mercs_save_list_count(s,MERCS_SAVE_SHOP_PILOTS)==0&&mercs_save_list_count(s,MERCS_SAVE_SHOP_WEAPONS)==0&&mercs_save_list_count(s,MERCS_SAVE_SHOP_AMMO)==0);
    /* A transfer is an original campaign event, not a reload operation: do
     * not call it twice to recover. Reload above deliberately never does. */
    mercs_save_free(s);
}
static void check_original_cleanup(const char *directory)
{
    char path[1024];uint8_t *input,*expected,*output;size_t input_size,expected_size,output_size;
    mercs_save_state *s=NULL,*bad;uint8_t *before;size_t before_size;
    snprintf(path,sizeof path,"%s/training-cleanup.input",directory);input=read_file(path,&input_size);
    snprintf(path,sizeof path,"%s/training-cleanup.expected",directory);expected=read_file(path,&expected_size);
    assert(!mercs_save_decode(input,input_size,&s));
    bad=mercs_save_clone(s);assert(bad);
    mercs_save_list_record_edit(bad,MERCS_SAVE_OWNED_PILOTS,0)[0x2064]=1;
    assert(!mercs_save_encode(bad,0,&before,&before_size));
    assert(mercs_save_training_cleanup(bad)==MERCS_SAVE_INVALID);
    same_encoding(bad,before,before_size);free(before);mercs_save_free(bad);
    assert(!mercs_save_training_cleanup(s));assert(!mercs_save_encode(s,1,&output,&output_size));
    assert(output_size==expected_size&&!memcmp(output,expected,output_size));free(output);
    mercs_save_free(s);free(input);free(expected);
}
int main(int argc,char **argv)
{
    check_codec(argc>1?argv[1]:"tests/fixtures/mercs-save");
    check_player_and_transfer();
    check_original_cleanup(argc>1?argv[1]:"tests/fixtures/mercs-save");
    puts("Mercs DOS save codec, original writer/restore/cleanup bytes, bounds, lossless reload, marker idempotence, identity and training transfer passed");
    return 0;
}
