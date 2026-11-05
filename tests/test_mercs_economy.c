#include "../tools/mercseconomy_view.h"
#include "../tests/fixtures/economy_view_original_fixtures.h"
#include <assert.h>

static size_t drawing;
static void put(uint8_t *p,uint32_t v)
{ p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
int mercs_art_width(mercs_art *a,int font,const char *text)
{
    size_t i;(void)a;
    for(i=0;i<sizeof economy_width_original/sizeof economy_width_original[0];i++)
        if(economy_width_original[i].font==font&&!strcmp(economy_width_original[i].text,text))return economy_width_original[i].width;
    return (int)strlen(text)*6;
}
int mercs_art_height(mercs_art *a,int font){(void)a;return font==6?16:10;}
int mercs_art_background(mercs_art *a,int entry){(void)a;assert(entry==31);return 0;}
int mercs_art_button(mercs_art *a,const char *name,int x,int y,int active)
{(void)a;(void)name;(void)x;(void)y;(void)active;return 0;}
void mercs_art_center(mercs_art *a,int font,int x,int y,const char *text,int ink)
{(void)a;(void)font;(void)x;(void)y;(void)text;(void)ink;}
void mercs_art_text(mercs_art *a,int font,int x,int y,const char *text,int ink)
{
    (void)a;if(y==15)return;
    assert(drawing<sizeof economy_personnel_original/sizeof economy_personnel_original[0]);
    assert(font==economy_personnel_original[drawing].font);
    assert(x==economy_personnel_original[drawing].x&&y==economy_personnel_original[drawing].y);
    assert(ink==economy_personnel_original[drawing].ink&&!strcmp(text,economy_personnel_original[drawing].text));drawing++;
}
int mercs_text_prepare(mercs_text_layout *out,mercs_art *a,const char *text,const char *pilot,int font,int width)
{
    (void)a;(void)pilot;assert(font==10&&width==407);
    out->expanded=malloc(strlen(text)+1);assert(out->expanded);strcpy(out->expanded,text);return 0;
}
void mercs_text_draw(const mercs_text_layout *t,mercs_art *a,int x,int y,int height,int scroll,int ink)
{(void)t;(void)a;(void)x;(void)y;(void)height;(void)scroll;(void)ink;}
void mercs_text_free(mercs_text_layout *t){free(t->expanded);memset(t,0,sizeof *t);}

static mercs_save_state *fixture(void)
{
    mercs_save_state *s=mercs_save_create();uint8_t *pilots=calloc(3,MERCS_SAVE_PILOT_BYTES),*techs=calloc(2,MERCS_SAVE_TECH_BYTES);
    uint8_t mechs[2*MERCS_CAREER_MECH_BYTES]={0},*h=mercs_save_header_edit(s);int i;
    assert(s&&pilots&&techs);put(h+0x12a,1000000);put(h+0x23bb,1);put(h+0x15e,100);
    put(h+0x23a3,10000);put(h+0x23a7,2000);put(h+0x23ab,1000);put(h+0x23af,500);put(h+0x23b3,720);
    assert(!mercs_save_set_list(s,MERCS_SAVE_OWNED_MECHS,mechs,2));
    assert(!mercs_save_set_list(s,MERCS_SAVE_OWNED_REFERENCES,mechs,2));
    for(i=0;i<3;i++){uint8_t *p=pilots+i*MERCS_SAVE_PILOT_BYTES;sprintf((char *)p,"PILOT%d",i);put(p+0x20,UINT32_MAX);put(p+0x24,70u+(unsigned)i);put(p+0x3c,3000u+(unsigned)i*100u);}
    assert(!mercs_save_set_list(s,MERCS_SAVE_SHOP_PILOTS,pilots,3));
    strcpy((char *)pilots,"PLAYER");pilots[0x2064]=1;
    assert(!mercs_save_set_list(s,MERCS_SAVE_OWNED_PILOTS,pilots,3));
    for(i=0;i<2;i++){uint8_t *p=techs+i*MERCS_SAVE_TECH_BYTES;sprintf((char *)p,"AERO%d",i);put(p+0x20,80u+(unsigned)i);put(p+0x24,7000u+(unsigned)i);}
    assert(!mercs_save_set_list(s,MERCS_SAVE_SHOP_TECHS,techs,2));
    assert(!mercs_save_set_list(s,MERCS_SAVE_OWNED_TECHS,techs,1));free(pilots);free(techs);return s;
}
static void same(mercs_save_state *s,const uint8_t *expected,size_t n)
{uint8_t *b;size_t m;assert(!mercs_save_encode(s,0,&b,&m));assert(m==n&&!memcmp(b,expected,n));free(b);}

int main(void)
{
    mercs_save_state *s=fixture();mercs_finance_view f={0};mercs_personnel_view v;mercs_art *a=(mercs_art *)(uintptr_t)1;
    uint8_t *before,*many;size_t length,index;int list,i;
    assert(!mercs_finance_view_prepare(&f,a,s,0));assert(!strcmp(f.text.expanded,economy_finance_original));mercs_finance_view_free(&f);
    mercs_personnel_view_init(&v);mercs_personnel_view_draw(&v,a,s);
    assert(drawing==sizeof economy_personnel_original/sizeof economy_personnel_original[0]);
    assert(mercs_personnel_view_hit(&v,a,s,230,115,&list,&index)==MERCS_PERSONNEL_VIEW_SELECT&&list==0&&index==1);
    assert(mercs_personnel_view_hit(&v,a,s,445,225,&list,&index)==MERCS_PERSONNEL_VIEW_SELECT&&list==3&&index==1);
    assert(mercs_personnel_view_hit(&v,a,s,230,105,&list,&index)==0);
    assert(mercs_finance_view_exit(500,432)&&!mercs_finance_view_exit(499,432));
    assert(!mercs_save_encode(s,0,&before,&length));
    assert(mercs_economy_dismiss(s,MERCS_PERSONNEL_MECHWARRIOR,0)==MERCS_ECONOMY_PLAYER);same(s,before,length);
    assert(mercs_economy_hire(s,MERCS_PERSONNEL_AEROTECH,0)==MERCS_ECONOMY_CAPACITY);same(s,before,length);
    assert(mercs_economy_hire(s,MERCS_PERSONNEL_MECHWARRIOR,99)==MERCS_ECONOMY_INVALID);same(s,before,length);free(before);
    many=calloc(8,MERCS_SAVE_PILOT_BYTES);assert(many);
    for(i=0;i<8;i++)memcpy(many+i*MERCS_SAVE_PILOT_BYTES,mercs_save_list_record(s,MERCS_SAVE_OWNED_PILOTS,i%3),MERCS_SAVE_PILOT_BYTES);
    assert(!mercs_save_set_list(s,MERCS_SAVE_OWNED_PILOTS,many,8));free(many);
    assert(!mercs_save_encode(s,0,&before,&length));
    assert(mercs_economy_hire(s,MERCS_PERSONNEL_MECHWARRIOR,0)==MERCS_ECONOMY_CAPACITY);same(s,before,length);free(before);
    mercs_save_free(s);puts("PASS: original finance text/personnel rows and atomic roster guards");return 0;
}
