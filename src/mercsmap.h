#ifndef MERCS_MAP_H
#define MERCS_MAP_H
#include <stdint.h>

/* Executed-original Mercs DOS126a4/12720/129bc/12608. See
 * tests/fixtures/controls-completion/original-map.json. */
static inline int mercs_map_visible(unsigned type,unsigned unit_flags)
{
    if(((type>>8)&15)==1)return !(unit_flags&0x16);
    return (type&0xf0)!=0x30 && (type&0xf0)!=0x70;
}
static inline unsigned mercs_map_material(unsigned type,unsigned material,unsigned alliance)
{
    static const unsigned color[11]={14,10,6,15,11,245,2,3,249,255,240};
    unsigned cls=(type>>8)&15,kind=material&0xf000;
    if(kind==0x8000||kind==0x9000)return kind;
    if(alliance>2)alliance=2; /* bounded native team state */
    if(cls==1)return color[alliance];
    if(cls==2)return color[3+alliance];
    if(cls==4)return color[6];
    if(cls!=8) {
        switch(type&0xf0) {
        case 0x10:case 0x20:case 0x60:return material;
        case 0x40:return color[7];
        case 0x50:return color[8];
        case 0x80:return color[9];
        }
    }
    return 0x4000u|(kind==0x3000?color[10]:(material&0xf00)>>4);
}
static inline int mercs_map_height(int32_t height,int32_t low,int32_t high)
{
    int64_t q,scaled;
    if(high<=low)return 0;
    q=((int64_t)height-low)*65536/((int64_t)high-low);
    scaled=q*16;
    /* SAR/ADC round-to-nearest, including negative values. */
    q=scaled>=0?(scaled+32768)/65536:-((-scaled+32767)/65536);
    return q<0?0:q>15?15:(int)q;
}
static inline int32_t mercs_map_zoom(int32_t range,int action,int allowed)
{
    if(!allowed)return range;
    if(action==0)return 100000;
    if(action==1){range/=2;if(range<12500)range=100000;}
    else if(action==2){range*=2;if(range>100000)range=12500;}
    return range;
}
/* DOS14240/14450: rectangular clipping for an off-screen map target.
 * Points and output are relative to the map window. The original can return
 * 2*floor(width/2), which is then clipped by the destination window. */
static inline int mercs_map_round16(int64_t v)
{return (int)(v>=0?(v+32768)/65536:-((-v+32767)/65536));}
static inline void mercs_map_edge_flags(int cx,int cy,int flags,int slope,int out[2])
{
    int aspect=cx?(int)((int64_t)cy*65536/cx):0;
    switch(flags) {
    case 0:if(slope>aspect){out[0]=cx+(int)((int64_t)cy*65536/slope);out[1]=cy*2;return;}break;
    case 1:
        if(slope< -aspect){out[0]=cx+(int)((int64_t)cy*65536/slope);out[1]=cy*2;return;}
        out[0]=0;out[1]=cy-mercs_map_round16((int64_t)cx*slope);return;
    case 2:if(slope< -aspect){out[0]=cx-(int)((int64_t)cy*65536/slope);out[1]=0;return;}break;
    case 3:
        if(slope<=aspect){out[0]=0;out[1]=cy-mercs_map_round16((int64_t)cx*slope);return;}
        out[0]=cx-(int)((int64_t)cy*65536/slope);out[1]=0;return;
    case 5:out[0]=cx;out[1]=cy*2;return;
    case 7:out[0]=cx;out[1]=0;return;
    default:out[0]=out[1]=0;return;
    }
    out[0]=cx*2;out[1]=cy+mercs_map_round16((int64_t)cx*slope);
}
static inline void mercs_map_edge(int width,int height,int x,int y,int out[2])
{
    int cx=width/2,cy=height/2,dx=cx-x,dy=cy-y;
    int flags=(dx>=0)|((dy>=0)<<1),slope=0;
    if(!dx || (int64_t)dy/dx>32767 || (int64_t)dy/dx< -32768)flags|=4;
    else slope=(int)((int64_t)dy*65536/dx);
    mercs_map_edge_flags(cx,cy,flags,slope,out);
}
#endif
