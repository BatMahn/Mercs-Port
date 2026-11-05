/* Includes the real mixer solely to inspect its buffer identity and deterministic
 * output while SDL's dummy callback is paused. Original WAV remains external. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/sfx.c"
#include "../src/mercsshell_audio.h"
#include "fixtures/mercs_shell_audio_waves.h"

static void u16(uint8_t *p, unsigned v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void u32(uint8_t *p, uint32_t v) { u16(p,v&65535); u16(p+2,v>>16); }
static uint64_t pcm_hash(const int16_t *p, size_t samples)
{
    size_t i; uint64_t h=UINT64_C(14695981039346656037);
    for (i=0;i<samples;i++) {
        unsigned v=(uint16_t)p[i];
        h=(h^(v&255u))*UINT64_C(1099511628211);
        h=(h^(v>>8))*UINT64_C(1099511628211);
    }
    return h;
}

/* Deliberately includes an odd-length unknown chunk; data does not start at44. */
static size_t wave_bytes(uint8_t *out, int bits, int channels, const uint8_t *p, size_t n)
{
    size_t size=54+n+(n&1u);
    memset(out,0,size);memcpy(out,"RIFF",4);u32(out+4,(uint32_t)size-8);memcpy(out+8,"WAVE",4);
    memcpy(out+12,"JUNK",4);u32(out+16,1);out[20]=99;
    memcpy(out+22,"fmt ",4);u32(out+26,16);u16(out+30,1);u16(out+32,(unsigned)channels);
    u32(out+34,11025);u32(out+38,(uint32_t)(11025*channels*(bits/8)));
    u16(out+42,(unsigned)(channels*(bits/8)));u16(out+44,(unsigned)bits);
    memcpy(out+46,"data",4);u32(out+50,(uint32_t)n);memcpy(out+54,p,n);
    return size;
}

static void decode_tests(void)
{
    uint8_t bytes[128]; size_t n; mercs_shell_wave w;
    const uint8_t eight[]={0,128,255};
    const uint8_t sixteen[]={0,128,255,127,123,0,56,254};
    n=wave_bytes(bytes,8,1,eight,sizeof eight);
    assert(mercs_shell_wave_decode(bytes,n,&w)==0);
    assert(w.frames==3&&w.channels==1&&w.rate==11025&&w.original_bits==8);
    assert(w.samples[0]==-32768&&w.samples[1]==0&&w.samples[2]==32512);
    mercs_shell_wave_free(&w);assert(!w.samples&&!w.frames);
    assert(mercs_shell_wave_decode(bytes,n-1,&w)==-1&&!w.samples);
    n=wave_bytes(bytes,16,2,sixteen,sizeof sixteen);
    assert(mercs_shell_wave_decode(bytes,n,&w)==0);
    assert(w.frames==2&&w.channels==2&&w.original_bits==16);
    assert(w.samples[0]==-32768&&w.samples[1]==32767&&w.samples[2]==123&&w.samples[3]==-456);
    mercs_shell_wave_free(&w);
    u16(bytes+42,2);assert(mercs_shell_wave_decode(bytes,n,&w)==-1); /* incomplete stereo alignment */
    u16(bytes+42,4);u16(bytes+30,2);assert(mercs_shell_wave_decode(bytes,n,&w)==-1); /* unsupported codec */
    u16(bytes+30,1);u32(bytes+34,0);assert(mercs_shell_wave_decode(bytes,n,&w)==-1);
    u32(bytes+34,11025);u32(bytes+50,7);assert(mercs_shell_wave_decode(bytes,n,&w)==-1);
    assert(mercs_shell_wave_decode(NULL,12,&w)==-1&&!w.samples);
    assert(mercs_shell_wave_decode(bytes,8,&w)==-1&&!w.samples);
}

static int find_pcm(sfx *s, const void *p)
{
    int i;
    for (i=0;i<SFX_VOICES;i++)
        if (s->v[i].c==&s->v[i].own&&s->v[i].own.data==p) return i;
    return -1;
}
static int active_count(sfx *s)
{
    int n=0,i;for(i=0;i<SFX_VOICES;i++)n+=s->v[i].c!=NULL;return n;
}
static void advance(sfx *s, int frames)
{
    float out[512*2];
    while(frames>0) { int n=frames>512?512:frames;mix(s,(Uint8 *)out,n*2*(int)sizeof(float));frames-=n; }
}
static void near_float(float a,float b) { assert(fabsf(a-b)<0.000002f); }
static int music_calls;
static void test_music(void *user,float *out,int frames)
{
    int i;assert(user==&music_calls);music_calls++;
    for(i=0;i<frames*2;i++)out[i]+=0.03125f;
}

static void mixer_tests(sfx *s)
{
    int16_t stereo[]={-32768,32767,16384,-8192};
    unsigned char legacy[]={192,192};
    unsigned char spoken[256];
    float out[16];int ch;
    assert(sfx_play_pcm_s16(s,stereo,2,2,s->rate,1,1)==0);
    assert(sfx_pcm_playing(s,stereo));mix(s,(Uint8 *)out,4*2*(int)sizeof(float));
    near_float(out[0],tanhf(-0.6f));near_float(out[1],tanhf((32767.0f/32768.0f)*0.6f));
    near_float(out[2],tanhf(0.3f));near_float(out[3],tanhf(-0.15f));
    near_float(out[4],out[0]);near_float(out[5],out[1]);
    sfx_stop_pcm(s,(const unsigned char *)stereo);assert(!sfx_pcm_playing(s,stereo));
    /* A reused S16 channel must not interpret subsequent 8-bit simulation PCM as S16. */
    assert(sfx_play_pcm(s,legacy,2,s->rate,1,1)==0);
    ch=find_pcm(s,legacy);assert(ch>=0&&!s->v[ch].own.s16);
    mix(s,(Uint8 *)out,2*2*(int)sizeof(float));near_float(out[0],tanhf(0.3f));near_float(out[1],out[0]);
    sfx_stop_pcm(s,legacy);
    /* Half-rate signed interpolation treats stereo channels independently. */
    assert(sfx_play_pcm_s16(s,stereo,2,2,s->rate/2,1,1)==0);
    mix(s,(Uint8 *)out,2*2*(int)sizeof(float));
    near_float(out[2],tanhf(-0.25f*0.6f));
    near_float(out[3],tanhf(((32767.0f/32768.0f)-0.25f)*0.5f*0.6f));
    sfx_pcm_level(s,(const unsigned char *)stereo,0.5f,1.0f);
    ch=find_pcm(s,stereo);assert(ch>=0);near_float(s->v[ch].l,0);near_float(s->v[ch].r,0.5f);
    sfx_pcm_rate(s,(const unsigned char *)stereo,11025);assert(s->v[ch].step==11025.0/s->rate);
    sfx_stop_loops(s);assert(!sfx_pcm_playing(s,stereo));
    assert(sfx_play_pcm_s16(s,stereo,2,3,s->rate,1,0)==-1);
    assert(sfx_play_pcm_s16(s,stereo,-1,2,s->rate,1,0)==-1);
    memset(spoken,128,sizeof spoken);sfx_play_pcm_voice(s,spoken,(int)sizeof spoken,1);
    assert(sfx_play_pcm_s16(s,stereo,2,2,s->rate,1,1)==0);
    assert(sfx_play_pcm(s,legacy,2,s->rate,1,0)==0);
    sfx_pause(s,1);assert(!sfx_pcm_playing(s,legacy)&&sfx_pcm_playing(s,stereo)&&sfx_voice_busy(s));
    ch=find_pcm(s,stereo);double pos=s->v[ch].pos;
    mix(s,(Uint8 *)out,(int)sizeof out);assert(s->v[ch].pos==pos);
    for(int i=0;i<16;i++)assert(out[i]==0);
    sfx_pause(s,0);sfx_stop_pcm(s,spoken);sfx_stop_pcm(s,(const unsigned char *)stereo);
    assert(active_count(s)==0);
}

static void cue_level(sfx *s,mercs_shell_audio *a,int id,int volume,int loop)
{
    const mercs_shell_wave *w=mercs_shell_audio_wave(a,id);
    int ch=find_pcm(s,w->samples);assert(ch>=0);
    assert(s->v[ch].own.s16&&s->v[ch].own.channels==w->channels);
    assert(s->v[ch].own.len==w->frames&&s->v[ch].loop==loop);
    assert(s->v[ch].step==(double)w->rate/s->rate);
    near_float(s->v[ch].l,SFX_VOL(volume));near_float(s->v[ch].r,SFX_VOL(volume));
}

static void shell_tests(sfx *s,const char *path)
{
    shelldb db;mercs_shell_audio *a,*no_device;size_t i;int stereo=0;float out[16];
    const unsigned char unrelated[]={128,192,64,128};
    assert(shelldb_open(path,&db)==0);assert(db.count==120);
    a=mercs_shell_audio_create(s,&db);assert(a);
    for(i=0;i<sizeof shell_wave_golden/sizeof shell_wave_golden[0];i++) {
        const mercs_shell_wave *w=mercs_shell_audio_wave(a,shell_wave_golden[i].id);
        int nonzero=0,j;assert(w);
        assert(w->frames==shell_wave_golden[i].frames&&w->channels==shell_wave_golden[i].channels);
        assert(w->rate==shell_wave_golden[i].rate&&w->original_bits==shell_wave_golden[i].bits);
        assert(pcm_hash(w->samples,(size_t)w->frames*(size_t)w->channels)==shell_wave_golden[i].hash);
        for(j=0;j<w->frames*w->channels;j++)nonzero+=w->samples[j]!=0;
        /* Authored resource118 is a real silent PCM asset, not a decoder error. */
        assert((nonzero==0)==(shell_wave_golden[i].id==118));stereo+=w->channels==2;
    }
    assert(i==73&&stereo==4);
    sfx_set_music(s,test_music,&music_calls);
    assert(sfx_play_pcm(s,unrelated,4,s->rate,0.1f,1)==0);
    mercs_shell_audio_enter(a,MSA_PAGE_REGISTER,0,MSA_PAGE_HALL);
    cue_level(s,a,62,5,1);cue_level(s,a,79,50,0);assert(!mercs_shell_audio_busy(a));
    assert(mercs_shell_audio_trigger(a,MSA_CUE_REGISTER_ACCEPT));cue_level(s,a,56,50,0);
    assert(mercs_shell_audio_wave(a,56)->frames==15730);
    assert(mercs_shell_audio_trigger(a,MSA_CUE_NEWS_EXIT)); /* original blocking gate swallows another click */
    advance(s,(int)(s->rate*1.5));assert(!mercs_shell_audio_busy(a));
    mercs_shell_audio_enter(a,MSA_PAGE_OFFICE,1,MSA_PAGE_HALL);
    assert(!mercs_shell_audio_resource_playing(a,62)&&!mercs_shell_audio_resource_playing(a,79));
    cue_level(s,a,60,30,1);cue_level(s,a,66,40,0);
    int before=active_count(s);mercs_shell_audio_suspend(a);
    assert(!mercs_shell_audio_resource_playing(a,60)&&sfx_pcm_playing(s,unrelated));
    assert(!mercs_shell_audio_trigger(a,MSA_CUE_OFFICE_COMPUTER));
    mercs_shell_audio_resume(a);cue_level(s,a,60,30,1);assert(!mercs_shell_audio_resource_playing(a,66));
    mercs_shell_audio_resume(a);assert(active_count(s)==before-1); /* no duplicated loop */
    mercs_shell_audio_enter(a,MSA_PAGE_COMPUTER,1,MSA_PAGE_OFFICE);
    cue_level(s,a,60,30,1);assert(!mercs_shell_audio_resource_playing(a,54));
    assert(mercs_shell_audio_trigger(a,MSA_CUE_COMPUTER_SELECT));cue_level(s,a,52,50,0);
    advance(s,s->rate);assert(!mercs_shell_audio_busy(a));
    assert(mercs_shell_audio_trigger(a,MSA_CUE_COMPUTER_EXIT));cue_level(s,a,56,50,0);
    advance(s,s->rate*2);assert(!mercs_shell_audio_busy(a));
    mercs_shell_audio_enter(a,MSA_PAGE_CONTRACTS,1,MSA_PAGE_COMPUTER);
    cue_level(s,a,72,50,0);cue_level(s,a,60,30,1);assert(!mercs_shell_audio_resource_playing(a,54));
    assert(!mercs_shell_audio_trigger(a,MSA_CUE_CONTRACT_SELECT));cue_level(s,a,52,50,0);
    assert(mercs_shell_audio_trigger(a,MSA_CUE_CONTRACT_ACCEPT));cue_level(s,a,56,50,0);
    advance(s,s->rate*2);assert(!mercs_shell_audio_busy(a));
    assert(mercs_shell_audio_trigger(a,MSA_CUE_CONTRACT_EXIT));cue_level(s,a,56,50,0);
    mercs_shell_audio_enter(a,MSA_PAGE_ROSTER,1,MSA_PAGE_COMMAND);
    assert(!mercs_shell_audio_trigger(a,MSA_CUE_ROSTER_REJECT));cue_level(s,a,56,30,0);
    assert(mercs_shell_audio_trigger(a,MSA_CUE_ROSTER_EXIT));cue_level(s,a,56,30,0);
    mercs_shell_audio_enter(a,MSA_PAGE_HALL,1,MSA_PAGE_OFFICE);
    mercs_shell_audio_trigger(a,MSA_CUE_HALL_NEED_CONTRACT);cue_level(s,a,70,40,0);
    advance(s,128);int ch=find_pcm(s,mercs_shell_audio_wave(a,70)->samples);double at=s->v[ch].pos;
    mercs_shell_audio_trigger(a,MSA_CUE_HALL_NEED_CONTRACT);assert(s->v[ch].pos==at);
    mercs_shell_audio_trigger(a,MSA_CUE_HALL_UNREGISTERED);cue_level(s,a,69,50,0);
    advance(s,128);mercs_shell_audio_trigger(a,MSA_CUE_HALL_UNREGISTERED);
    ch=find_pcm(s,mercs_shell_audio_wave(a,69)->samples);assert(s->v[ch].pos==0);
    mercs_shell_audio_enter(a,MSA_PAGE_MENU,0,MSA_PAGE_HALL);
    assert(mercs_shell_audio_trigger(a,MSA_CUE_MENU_COMMANDER));cue_level(s,a,52,50,0);cue_level(s,a,81,50,0);
    advance(s,s->rate*4);assert(!mercs_shell_audio_busy(a));
    assert(mercs_shell_audio_trigger(a,MSA_CUE_MENU_MERCNET));cue_level(s,a,52,50,0);
    assert(!mercs_shell_audio_resource_playing(a,53));
    advance(s,s->rate);assert(mercs_shell_audio_busy(a));cue_level(s,a,53,50,0);
    advance(s,s->rate);assert(!mercs_shell_audio_busy(a));
    mercs_shell_audio_enter(a,MSA_PAGE_NEWS,1,MSA_PAGE_REGISTER);
    assert(!mercs_shell_audio_resource_playing(a,49));mercs_shell_audio_text_reveal(a,1);
    cue_level(s,a,49,10,1);before=active_count(s);mercs_shell_audio_text_reveal(a,1);assert(active_count(s)==before);
    mercs_shell_audio_text_reveal(a,0);assert(!mercs_shell_audio_resource_playing(a,49));
    /* The original idle table deliberately uses a silent WAV as a timed gap. */
    mercs_shell_audio_enter(a,MSA_PAGE_HALL,1,MSA_PAGE_NONE);
    mercs_shell_audio_seed(a,3);mercs_shell_audio_tick(a);cue_level(s,a,118,15,0);
    advance(s,s->rate);mercs_shell_audio_tick(a);
    ch=find_pcm(s,mercs_shell_audio_wave(a,118)->samples);assert(ch>=0&&s->v[ch].pos>0);
    mercs_shell_audio_idle_enabled(a,0);assert(!mercs_shell_audio_resource_playing(a,118));
    before=active_count(s);mercs_shell_audio_tick(a);assert(active_count(s)==before);
    /* Hall idle waits for its unregistered greeting; NEWS can suppress idle
     * for the initial MLETTER object's entire lifetime. */
    mercs_shell_audio_enter(a,MSA_PAGE_HALL,0,MSA_PAGE_NONE);
    mercs_shell_audio_seed(a,0);mercs_shell_audio_tick(a);assert(!mercs_shell_audio_resource_playing(a,105));
    advance(s,s->rate*4);mercs_shell_audio_tick(a);cue_level(s,a,105,15,0);
    mercs_shell_audio_enter(a,MSA_PAGE_NEWS,1,MSA_PAGE_REGISTER);mercs_shell_audio_seed(a,0);
    mercs_shell_audio_idle_enabled(a,0);mercs_shell_audio_tick(a);assert(!mercs_shell_audio_resource_playing(a,105));
    mercs_shell_audio_idle_enabled(a,1);mercs_shell_audio_tick(a);cue_level(s,a,105,15,0);
    music_calls=0;mix(s,(Uint8 *)out,(int)sizeof out);assert(music_calls==1);
    assert(s->music==test_music&&s->music_user==&music_calls);
    mercs_shell_audio_destroy(a);assert(sfx_pcm_playing(s,unrelated)&&active_count(s)==1);
    assert(s->music==test_music);sfx_set_music(s,NULL,NULL);sfx_stop_pcm(s,unrelated);
    no_device=mercs_shell_audio_create(NULL,&db);assert(no_device);
    mercs_shell_audio_enter(no_device,MSA_PAGE_REGISTER,0,MSA_PAGE_HALL);
    assert(!mercs_shell_audio_trigger(no_device,MSA_CUE_REGISTER_ACCEPT)&&!mercs_shell_audio_busy(no_device));
    mercs_shell_audio_destroy(no_device);shelldb_close(&db);
}

int main(int argc,char **argv)
{
    sfx *s;
    assert(argc==2);decode_tests();
    assert(SDL_Init(SDL_INIT_AUDIO));s=sfx_create(NULL);assert(s);
    assert(SDL_PauseAudioStreamDevice(s->stream));mixer_tests(s);shell_tests(s,argv[1]);
    sfx_destroy(s);SDL_Quit();
    puts("PASS:73 original WAV PCM goldens; signed16 mono/stereo and legacy8 mixing; exact registration/menu/hall cues; waits, single loops, isolated FMV/child suspension and music ownership.");
    return 0;
}
