#ifndef MERCS_HUD_BACKEND_FIXTURE_H
#define MERCS_HUD_BACKEND_FIXTURE_H
/* Test-only snapshots of submitted RGBA, cache identity and upload accounting.
 * Actual hud.c and the active render backend run; final RGB is read from the GPU. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "src/rb.h"
#include "src/rb_shaders.h"
static unsigned upload_calls;
static uint64_t upload_bytes;
static uint64_t upload_started,upload_ns;
static size_t refused_size;
static unsigned refused_allocations;
static rb_tex bound_texture;
static rb_hud_draw draw_uniforms;
typedef struct {rb_tex tex;int w,h;unsigned char *rgba;} submitted_texture;
static submitted_texture *submitted;
static size_t submitted_count,submitted_cap;
static submitted_texture *texture_record(rb_tex tex,int create)
{
    size_t i;
    for(i=0;i<submitted_count;i++)if(submitted[i].tex==tex)return &submitted[i];
    if(!create||!tex)return NULL;
    if(submitted_count==submitted_cap){
        size_t cap=submitted_cap?submitted_cap*2:64;
        submitted_texture *p=realloc(submitted,cap*sizeof *p);
        if(!p)abort();
        submitted=p;submitted_cap=cap;
    }
    memset(&submitted[submitted_count],0,sizeof submitted[submitted_count]);
    submitted[submitted_count].tex=tex;return &submitted[submitted_count++];
}
static void *counted_malloc(size_t n)
{
    if(refused_allocations&&n==refused_size){refused_allocations--;return NULL;}
    if(!upload_started)upload_started=SDL_GetTicksNS();
    return malloc(n);
}
static rb_tex counted_tex_new(void)
{
    rb_tex tex=rb_tex_new();(void)texture_record(tex,1);return tex;
}
static void counted_tex_delete(rb_tex tex)
{
    size_t i;
    for(i=0;i<submitted_count;i++)if(submitted[i].tex==tex){
        free(submitted[i].rgba);submitted[i]=submitted[--submitted_count];break;
    }
    rb_tex_delete(tex);
}
static void record_actual_sprite_draw(void);
static void counted_draw(int first,int count)
{
    record_actual_sprite_draw();rb_draw(first,count);
}
static void counted_texture(int unit,rb_tex tex)
{
    if(unit==0)bound_texture=tex;
    rb_texture(unit,tex);
}
static void counted_uniforms(int slot,const void *data,size_t bytes)
{
    if(slot==0&&bytes==sizeof draw_uniforms)memcpy(&draw_uniforms,data,bytes);
    rb_uniforms(slot,data,bytes);
}
static void counted_tex_image(rb_tex tex,int w,int h,const void *pixels,int mipmaps)
{
    submitted_texture *t=texture_record(tex,1);
    size_t bytes=(size_t)w*(size_t)h*4;
    unsigned char *copy=malloc(bytes);
    if(!t||!copy)abort();
    memcpy(copy,pixels,bytes);free(t->rgba);t->rgba=copy;t->w=w;t->h=h;
    upload_calls++;upload_bytes+=(uint64_t)bytes;
    rb_tex_image(tex,w,h,pixels,mipmaps);
    if(upload_started){upload_ns+=SDL_GetTicksNS()-upload_started;upload_started=0;}
}
#define rb_tex_new counted_tex_new
#define rb_tex_delete counted_tex_delete
#define rb_tex_image counted_tex_image
#define rb_texture counted_texture
#define rb_uniforms counted_uniforms
#define rb_draw counted_draw
#define malloc counted_malloc
#include "src/hud.c"
#undef rb_tex_new
#undef rb_tex_delete
#undef rb_tex_image
#undef rb_texture
#undef rb_uniforms
#undef rb_draw
#undef malloc
static rb_tex drawn[512];static unsigned drawn_count,draw_calls;static int track_draws;
static void record_actual_sprite_draw(void)
{
    unsigned j;int i;
    if(!track_draws||!draw_uniforms.u_textured)return;
    for(i=0;i<g_src_n;i++)if(g_src[i].tex==bound_texture){
        draw_calls++;for(j=0;j<drawn_count&&drawn[j]!=bound_texture;j++);
        if(j==drawn_count&&drawn_count<512)drawn[drawn_count++]=bound_texture;
        break;
    }
}
int shp_fixture_texture_live(unsigned tex){return texture_record(tex,0)!=NULL;}
size_t shp_fixture_texture_snapshot(unsigned tex,unsigned char **out,int *w,int *h)
{
    submitted_texture *t=texture_record(tex,0);size_t size;
    *out=NULL;if(w)*w=0;if(h)*h=0;
    if(!t||!t->rgba)return 0;
    size=(size_t)t->w*(size_t)t->h*4;*out=malloc(size);
    if(!*out)return 0;
    memcpy(*out,t->rgba,size);if(w)*w=t->w;if(h)*h=t->h;
    return size;
}
void shp_fixture_track_draws(int on){track_draws=on;}
unsigned shp_fixture_drawn_count(void){return drawn_count;}
unsigned shp_fixture_draw_calls(void){return draw_calls;}
uint64_t shp_fixture_upload_ns(void){return upload_ns;}
void shp_fixture_fail_allocation(size_t size){refused_size=size;refused_allocations=1;}
void shp_fixture_upload_reset(void){upload_calls=0;upload_bytes=0;upload_ns=0;upload_started=0;drawn_count=draw_calls=0;}
unsigned shp_fixture_upload_calls(void){return upload_calls;}
uint64_t shp_fixture_upload_bytes(void){return upload_bytes;}
int shp_fixture_cache_count(void){return g_src_n;}
int shp_fixture_index_used(int index){return index>=0&&index<256&&g_src_used[index];}
int shp_fixture_dirty(unsigned texture)
{
#ifdef MERCS_SHP_DEFERRED
    int i;for(i=0;i<g_src_n;i++)if(g_src[i].tex==texture)return g_src[i].dirty;
#else
    (void)texture;
#endif
    return 0;
}
int shp_fixture_dirty_count(void)
{
    int n=0;
#ifdef MERCS_SHP_DEFERRED
    int i;for(i=0;i<g_src_n;i++)if(g_src[i].dirty)n++;
#endif
    return n;
}
int shp_fixture_source_matches(const hud_sprite *s,const shp_frame *f)
{
    int i;
    for(i=0;i<g_src_n;i++)if(g_src[i].tex==s->tex){
        const hud_src *p=&g_src[i];size_t n=(size_t)f->w*(size_t)f->h;
        return p->w==f->w&&p->h==f->h&&p->from==-1&&p->to==-1&&
               !memcmp(p->pix,f->pix,n)&&!memcmp(p->mask,f->mask,n);
    }
    return 0;
}
size_t shp_fixture_sources_snapshot(unsigned char **out)
{
    size_t n=0,at=0;int i;
    for(i=0;i<g_src_n;i++)n+=sizeof(int)*5+sizeof g_src[i].used+(size_t)g_src[i].w*(size_t)g_src[i].h*2;
    *out=malloc(n);if(!*out)return 0;
    for(i=0;i<g_src_n;i++){
        const hud_src *p=&g_src[i];int meta[5]={(int)p->tex,p->w,p->h,p->from,p->to};size_t size=(size_t)p->w*(size_t)p->h;
        memcpy(*out+at,meta,sizeof meta);at+=sizeof meta;memcpy(*out+at,p->used,sizeof p->used);at+=sizeof p->used;
        memcpy(*out+at,p->pix,size);at+=size;memcpy(*out+at,p->mask,size);at+=size;
    }
    return n;
}
size_t shp_fixture_atlas_snapshot(hud *h,unsigned char **out)
{
    submitted_texture *t[2];int i;size_t n=0,at=0;
    if(!h||!out)return 0;
    *out=NULL;t[0]=texture_record(h->font_tex,0);t[1]=texture_record(h->font_tex3,0);
    for(i=0;i<2;i++){
        if(!t[i]||!t[i]->rgba)return 0;
        n+=(size_t)t[i]->w*(size_t)t[i]->h*4;
    }
    *out=malloc(n);if(!*out)return 0;
    for(i=0;i<2;i++){
        size_t bytes=(size_t)t[i]->w*(size_t)t[i]->h*4;
        memcpy(*out+at,t[i]->rgba,bytes);at+=bytes;
    }
    return n;
}

#endif
