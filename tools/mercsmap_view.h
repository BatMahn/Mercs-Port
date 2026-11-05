/* DOS11970/11b18: a satellite view draws geometry, its view cone, navigation
 * markers and the selected target. Ordinary radar unit blips are omitted. */
#ifndef MERCSMAP_VIEW_H
#define MERCSMAP_VIEW_H
static void mercs_satellite_marks(const hud_win *box,int full_screen)
{
    const int width=box->x1-box->x0+1,height=box->y1-box->y0+1;
    const float range=100000.0f/(float)(1<<g_sat_i);
    float units=range/(float)width;
    const int nav_target=g_tgt_sel<0&&g_tgt_thing<0?current_nav(NULL):-1;
    int q;
    if(width<=0 || height<=0)return;
    if(full_screen)units*=((float)width*box->ux)/640.0f;
    if(g_tex_kind==3)units=fmaxf(1.0f,floorf(units));
    hud_clip(g_hud,box->x0*box->ux,box->y0*box->uy,width*box->ux,height*box->uy);
    if(!g_sim.player_unit.destroyed&&!g_sim.player_unit.shutdown) {
        int sign;
        const int32_t half=glr_dos_atan2(65536,(int32_t)lrintf(fmaxf(g_zoom,1.0f)*65536.0f));
        const int32_t bearing=(int32_t)lrintf((g_pl.heading+g_pl.twist)*65536.0f);
        for(sign=-1;sign<=1;sign+=2) {
            const int32_t angle=0x5a0000-bearing+sign*half;
            const int dx=glr_dos_cos(angle)>>13,dy=-(glr_dos_sin(angle)>>13);
            int point[2];
            mercs_map_edge(width,height,width/2+dx,height/2+dy,point);
            px_line(box,box->x0+width/2,box->y0+height/2,
                    box->x0+point[0],box->y0+point[1],HP(14));
        }
    }
    /* All visible navigation markers first, then the current unit/thing or
     * selected navigation marker. No extra radar dots obscure the models. */
    for(q=0;q<=g_sim.nav_count;q++) {
        float wx,wz;
        int target=0,side=0,visited=0,set3=0,p[2],inside;
        const hud_sprite *sprite;
        if(q<g_sim.nav_count) {
            const msim_nav *nav=&g_sim.navs[q];
            if(!nav->selectable || q==nav_target)continue;
            wx=nav->x;wz=nav->z;visited=q<256&&g_nav_seen[q];
            sprite=&g_hs_rnp[visited];
        } else {
            target=1;
            if(g_tgt_sel>=0 && g_tgt_sel<g_actor_count && !g_sim.units[g_tgt_sel].destroyed) {
                wx=(float)g_actors[g_tgt_sel].mech.origin[0];wz=(float)g_actors[g_tgt_sel].mech.origin[2];
                side=unit_side(g_tgt_sel);if(side<0||side>2)side=2;
                set3=!(g_actors[g_tgt_sel].unit_flags&0x1400) || (g_actors[g_tgt_sel].unit_flags&0x800);
                sprite=set3?&g_hs_rtgt[side]:&g_hs_rtgp[side];
            } else if(g_tgt_thing>=0 && g_tgt_thing<g_sim.bld_count && g_sim.bld[g_tgt_thing].hp>0) {
                float point[3];
                if(!msim_thing_point(&g_sim,g_tgt_thing,point))continue;
                wx=point[0];wz=point[2];side=g_sim.bld[g_tgt_thing].side;if(side<0||side>2)side=2;
                set3=!(g_sim.bld[g_tgt_thing].flags&0x1400);
                sprite=set3?&g_hs_rtgt[side]:&g_hs_rtgp[side];
            } else if(nav_target>=0 && nav_target<g_sim.nav_count) {
                wx=g_sim.navs[nav_target].x;wz=g_sim.navs[nav_target].z;
                visited=nav_target<256&&g_nav_seen[nav_target];sprite=&g_hs_rtnp[visited];
            } else continue;
        }
        if(g_tex_kind==3) {
            const int64_t dx=(int32_t)wx-g_pl.origin[0],dz=(int32_t)wz-g_pl.origin[2];
            p[0]=(int)((dx*4/(int)units+2)>>2)+width/2;
            p[1]=height-((int)((dz*4/(int)units+2)>>2)+height/2);
        } else {
            p[0]=(int)lroundf((wx-g_pl.origin[0])/units)+width/2;
            p[1]=height/2-(int)lroundf((wz-g_pl.origin[2])/units);
        }
        inside=p[0]>=0&&p[0]<width&&p[1]>=0&&p[1]<height;
        if(!inside) {
            if(!target)continue;
            mercs_map_edge(width,height,p[0],p[1],p);sprite=&g_hs_rtof[side];
        }
        if(sprite->tex)hud_draw(g_hud,sprite,(box->x0+p[0])*box->ux,(box->y0+p[1])*box->uy,NULL);
    }
    hud_clip_off(g_hud);
    if(full_screen) {
        char label[48];
        const float half_range=range/200.0f;
        if(half_range>=1000)snprintf(label,sizeof label,"Range: %3.1fkm",(double)half_range/1000.0);
        else snprintf(label,sizeof label,"Range: %3.1fm",(double)half_range);
        hud_text(g_hud,6.4f,4.8f,label,HP(14));
        snprintf(label,sizeof label,"Bearing: %3.1f",(double)fmodf(g_pl.heading+g_pl.twist+720.0f,360.0f));
        hud_text(g_hud,6.4f,24.0f,label,HP(14));
    }
}
#endif
