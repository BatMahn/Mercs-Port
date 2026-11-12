/* Original Mercs generic MFD consumer. Seven physical windows independently
 * select all fourteen modes; their authored geometry stays fixed. Included
 * after glview's renderer helpers so it reuses the original-asset painters. */
static float mfd_target_text(hud *hd,float x,float y,const char *text,const float *color)
{
    /* The original target-name window18 is separate from the generic MFD. */
    return g_mfd_active == MERCS_MFD_A ? hud_text(hd,x,y,text,color) : 0.0f;
}
static float mfd_htal_x(const hud_win *w,float x)
{ return w->x0*w->ux+(x-516.9f)/102.5f*(w->x1-w->x0)*w->ux; }
static float mfd_htal_y(const hud_win *w,float y)
{ return w->y0*w->uy+(y-349.4f)/79.4f*(w->y1-w->y0)*w->uy; }
static void mercs_mfd_draw_one(int slot, int w, int h)
{
    const int mode = g_hud_settings.mode[slot], side = mfd_side(slot);
    const int hud_dead = g_sim_ok && g_pow != 2;
    const combat_unit *pu = &g_sim.player_unit;
    const float *green=HP(14), *red=HP(11);
    static const float black[4]={0,0,0,0.9f};
    int q, tgt=g_tgt_sel;
    float head=g_pl.heading+g_pl.twist, tdist=-1.0f;
    int old_vport=g_vport;
    if (!mode || (mode==MERCS_MFD_SATELLITE && !g_combat_options.dif[9])) return;
    if (tgt >= 0 && tgt < g_actor_count)
        tdist=hypotf((float)g_actors[tgt].mech.origin[0]-g_pl.origin[0],
                     (float)g_actors[tgt].mech.origin[2]-g_pl.origin[2]);
    else tgt=-1;
    g_mfd_active=slot; g_vport=mfd_view_kind(mode);
            if (mode == MERCS_MFD_ALTITUDE && !hud_dead) {
                const hud_win aw = mercs_instrument(MERCS_HUD_ALTITUDE);
                const float fh = hud_text_height(g_hud);
                const float ax = (float)aw.rx * aw.ux, ay = (float)aw.ry * aw.uy;
                char altitude[32];
                HUD_SIDE(side);
                hud_text(g_hud, ax, ay + 2 * fh, "Alt:", green);
                /* the original's number: (object +0x54 - unit +0x400) x 1000 / 65536 (DOS 0x3fa10 "%.2f m" with 1/65536,
                 * MERCSW 0x1002eded the same), the height in centimetres - so it reads about 1.53 x the height in metres.
                 * DOSBox BUZ1SCN1 at the start: "2836.67 m" where the mech stands 1859.04 m up (185904 x 1000 / 65536 =
                 * 2836.67), so +0x400 is 0 there, standing on raised terrain: taken as 0 (INFERRED) */
                snprintf(altitude, sizeof altitude, "%.2f m", (double)pu->y * 1000.0 / 65536.0);
                hud_text(g_hud, ax, ay + 3 * fh, altitude, green);
                if (pu->jets > 0) {
                    const int AW = aw.x1 - aw.x0 + 1, AH = aw.y1 - aw.y0 + 1;
                    const int vx = fx16_round((long long)(AW - 1) * 0x1e7a);
                    const int vy = fx16_round((long long)(AH - 1) * 0x8dc9);
                    const int top = AH - (vy + vy / 2) - 3, span = AH - 1 - top - 2;
                    int n = hud_filter_steps(&g_jet_acc, &g_jet_seen), fill;
                    const int32_t fuel = (int32_t)pu->jet_fuel;
                    hud_text(g_hud, ax, ay, "Jets", green);
                    while (n-- > 0) g_jet_s += (fuel - g_jet_s) >> 1;
                    fill = (int)((long long)(span + 1) * g_jet_s / 0x712);
                    if (fill > span) fill = span;
                    if (fill < 0) fill = 0;
                    g_jet_p = fill;
                    px_band_v(&aw, 1, AH - 2, vx - 1, fill, 11);
                    px_fill(&aw, aw.x0, aw.y0 + top, aw.x0, aw.y1, HP(10));
                    px_fill(&aw, aw.x0 + vx, aw.y0 + top, aw.x0 + vx, aw.y1, HP(10));
                    px_fill(&aw, aw.x0, aw.y0 + top, aw.x0 + vx, aw.y0 + top, HP(10));
                    px_fill(&aw, aw.x0, aw.y1, aw.x0 + vx, aw.y1, HP(10));
                }
                HUD_SIDE(side);
            }
            HUD_SIDE(side);   /* right-hand: the viewport box, H T A L bars or damage outline */
            if (g_vport) {   /* the viewport box: blue frame, label sprite at the top centre (DOS) */
                const float *vblue = HP(6);   /* DOSBox YELLSCN1: 0,0,199 = YELL_DA 6 */
                /* window 2 (cpit_window): DOSBox YELLSCN1 F7 frame 827-991 x 559-686 at 1024 x 768 */
                const hud_win vw2 = cpit_window(2, 0, 0);
                float x0 = (float)vw2.x0 * vw2.ux, y0 = (float)vw2.y0 * vw2.uy, x1 = (float)vw2.x1 * vw2.ux, y1 = (float)vw2.y1 * vw2.uy, vwf = 1, vhf = 1;
                if (pow_window(1, &vwf, &vhf)) {   /* powering up / down: window 2 about its centre (0x10011aa0 / 0x10011b70) */
                    float mx = (x0 + x1) * 0.5f, my = (y0 + y1) * 0.5f, hw = (x1 - x0) * 0.5f * vwf, hh = (y1 - y0) * 0.5f * vhf;
                    int full = vwf >= 1.0f && vhf >= 1.0f;
                    x0 = mx - hw; x1 = mx + hw; y0 = my - hh; y1 = my + hh;
                    if (g_vport == 3 && !g_wcam_live) hud_rect(g_hud, x0, y0, x1 - x0, y1 - y0, black);   /* weapon view, nothing in flight: black (DOS) */
                    if (g_snow_show[0]) draw_snow(x0, y0, x1 - x0, y1 - y0);   /* display damage: SNOWCLR instead of the view (0x10011a40),
                                                                                  * in the moving box while it opens / closes */
                    if(mode==MERCS_MFD_SATELLITE && full && !g_snow_show[0])mercs_satellite_marks(&vw2,0);
                    if (full) px_frame(&vw2, vblue);
                    else {
                        hud_line(g_hud, x0, y0, x1, y0, 1.0f, vblue); hud_line(g_hud, x1, y0, x1, y1, 1.0f, vblue);
                        hud_line(g_hud, x1, y1, x0, y1, 1.0f, vblue); hud_line(g_hud, x0, y1, x0, y0, 1.0f, vblue);
                    }
                    if (!g_snow_show[0] && g_vport <= 3 && g_hs_vp[g_vport - 1].tex) {
                        /* the label (0x10011a70 -> 0x10023120: x = the window's left edge + half the instrument's full width
                         * (+0x44 / 2), y = its top + 2): while the window opens / closes it rides the moving left and top
                         * edges, clipped to the box */
                        if (!full) hud_clip(g_hud, x0, y0, x1 - x0, y1 - y0);
                        /* (DOSBox YELLSCN1 F7: REAR 7 px higher than the port's y0 + 6 units at 1024 x 768) */
                        hud_draw(g_hud, &g_hs_vp[g_vport - 1], x0 + (float)((vw2.x1 - vw2.x0 + 1) / 2) * vw2.ux, y0 + 2.0f * vw2.uy, NULL);
                        if (!full) hud_clip_off(g_hud);
                    }
                }
            } else if (mode == MERCS_MFD_HTAL && !hud_dead) {
                /* H T A L (DOS, F6; engine 0x1001b890, DOS 0x2f790): armour bars hanging from a line a third down the
                 * lower window (0x5555 of its height, y 375.6), torsos as half-width front / rear pairs, blue letters
                 * (index 6). The scale is the mech's own: at the HUD's set-up (0x1001b230 / DOS 0x2f190) every
                 * location's starting front / rear armour is copied (0x1007c388 / DOS 0xfe0a8) and the largest FRONT
                 * value kept (0x1024c620 / DOS 0x96664); a bar is armour x half the window's height (0x8000 of it,
                 * converted to pixels at the set-up: 40 rows at 640x480) / that maximum, truncated (DOS 64-bit
                 * integer divide), so the most armoured location's bar is half the window whatever the mech (the
                 * port had drawn a fixed 1.125 units per MEK point: a Mad Dog's bars ~1.6x too short, every hit
                 * stripping too little). Each part is drawn as a vertical line run, both ends inclusive (0x1001c810 /
                 * DOS 0x31520): the armour left in green (0xf bands) from the line down to it, then the armour lost
                 * from there to the starting length - yellow (3) while more than a quarter of the start is left (DOS
                 * (start >> 2) < armour), else red (0xb), over the green's last row; a destroyed location all grey
                 * (0xf3). DOSBox AQUASCN1, Mad Dog (largest front 23 x 4 = CT / legs): 1024x768 full lengths
                 * H 26 / side torsos and arms 45 / CT and legs 65 / rears 20 px; an ER PPC hit on the CT -12 px */
                /* the bars' x (0x1007c340.. / DOS 0xfdf60..: 0x2323, 0x7373, 0x5a5a, 0x4141, 0xaaab, 0x9192, 0xe1e2,
                 * 0xc8c9 of the window's width for locations 0-7): left to right head, 3, 2, 1, 5, 4, 7, 6 - each pair
                 * the higher location first (the port had drawn 1..7 left to right, every pair mirrored; DOSBox
                 * AQUASCN1: the CT's partner in the DOS spread, location 1, is the RIGHT torso pair) */
                static const struct { int loc, rear; float x, w; } B[11] = {
                    {0, 0, 531.3f, 8.1f}, {3, 0, 543.1f, 3.75f}, {3, 1, 546.9f, 3.75f}, {2, 0, 553.1f, 3.75f}, {2, 1, 556.9f, 3.75f},
                    {1, 0, 563.1f, 3.75f}, {1, 1, 566.9f, 3.75f}, {5, 0, 575.0f, 8.1f}, {4, 0, 585.0f, 8.1f}, {7, 0, 597.5f, 8.1f}, {6, 0, 607.5f, 8.1f}};
                const float *hb = HP(14), *hl = HP(15), *hd = HP(13);   /* the band colours in the mission palette (0x10260324) */
                const float *yb = HP(2), *yl = HP(3), *ydk = HP(1);
                const float *rb = HP(10), *rl = HP(11), *rd = HP(9);
                const float *gb2 = HP(0xf2), *gl2 = HP(0xf3), *gd2 = HP(0xf1);
                const float *hblue = HP(6);
                const hud_win hw=cpit_window(2,0,0);
                const int64_t half = (hw.y1-hw.y0+1)/2;   /* 0x8000 x the window's 80 rows (y 349.4-428.8, 0x10005ec0 rounds) */
                int64_t fmax = 0;
                int b2;
                for (b2 = 0; b2 < 8; b2++) if (pu->armor_max[b2] > fmax) fmax = pu->armor_max[b2];   /* front only, as both loops compare */
                hud_text(g_hud, mfd_htal_x(&hw,528.8f), mfd_htal_y(&hw,357.5f), "H", hblue); hud_text(g_hud, mfd_htal_x(&hw,549.4f), mfd_htal_y(&hw,357.5f), "T", hblue);
                hud_text(g_hud, mfd_htal_x(&hw,577.5f), mfd_htal_y(&hw,357.5f), "A", hblue); hud_text(g_hud, mfd_htal_x(&hw,601.3f), mfd_htal_y(&hw,357.5f), "L", hblue);
                for (b2 = 0; b2 < 11 && fmax > 0; b2++) {
                    int loc = B[b2].loc;
                    float cur = B[b2].rear ? pu->rear[loc] : pu->armor[loc];
                    int64_t mx = B[b2].rear ? pu->rear_max[loc] : pu->armor_max[loc];
                    int64_t len = (int64_t)(cur > 0 ? cur : 0) * half / fmax, full = mx * half / fmax;
                    float x=mfd_htal_x(&hw,B[b2].x), w3=B[b2].w/3.0f*(hw.x1-hw.x0)*hw.ux/102.5f, y0=mfd_htal_y(&hw,375.6f);
                    const float *mb = (float)mx * 0.25f < cur ? yb : rb, *ml = (float)mx * 0.25f < cur ? yl : rl,
                                *md = (float)mx * 0.25f < cur ? ydk : rd;
                    if (pu->loc_gone[loc]) { len = 0; mb = gb2; ml = gl2; md = gd2; }
                    if (len != 0) {   /* rows y0 .. y0 + len */
                        hud_rect(g_hud, x, y0, w3, ((float)len + 1.0f)*hw.uy, hb);
                        hud_rect(g_hud, x + w3, y0, w3, ((float)len + 1.0f)*hw.uy, hl);
                        hud_rect(g_hud, x + 2 * w3, y0, w3, ((float)len + 1.0f)*hw.uy, hd);
                    }
                    if (len < full) {   /* rows y0 + len .. y0 + full */
                        hud_rect(g_hud, x, y0 + (float)len*hw.uy, w3, ((float)(full - len) + 1.0f)*hw.uy, mb);
                        hud_rect(g_hud, x + w3, y0 + (float)len*hw.uy, w3, ((float)(full - len) + 1.0f)*hw.uy, ml);
                        hud_rect(g_hud, x + 2 * w3, y0 + (float)len*hw.uy, w3, ((float)(full - len) + 1.0f)*hw.uy, md);
                    }
                }
            } else if (mode == MERCS_MFD_DAMAGE && g_layout_n > 2 && !hud_dead) {
                static const int REG_LOC[15] = {1, 3, 3, 2, 2, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8};
                /* centred in window 2 (set-up 0x1001b230 / DOS 0x2f190): x0 + (W - the outline's width) / 2, y0 + (H -
                 * its height) / 2, W / H the window's +0x44 / +0x46 (x1 - x0 + 1, y1 - y0 + 1), the sprite's size its
                 * bounds (0x10060b80 / DOS 0x58908), C division. The port had drawn it at the window's corner: (16, 4)
                 * px off at 640 x 480, (47, 29) at 1024 x 768 (DOSBox YELLSCN1 Timber Wolf: 532, 352 / 873, 586) */
                const hud_win dw = cpit_window(2, 0, 0);
                const int sw = g_hs_dmg_lvl[0].w, sh = g_hs_dmg_lvl[0].h;
                float dx = (float)(dw.x0 + (dw.x1 - dw.x0 + 1 - sw) / 2) * dw.ux, dy = (float)(dw.y0 + (dw.y1 - dw.y0 + 1 - sh) / 2) * dw.uy;
                /* the whole outline first in its own colour 6 (0x1001b620: 0x10062241 at the window's 0, 0), then each region
                 * whose colour is not 6 again, remapped (DOSBox: the outline's pixels outside every region rectangle -
                 * the side torsos' lower edges - are drawn; the port had drawn the regions alone) */
                hud_draw(g_hud, &g_hs_dmg_lvl[0], dx, dy, NULL);
                for (q = 0; q < 15; q++) {
                    int loc = REG_LOC[q] - 1, lv, pick;
                    const int *rg = g_dmg_regions[q];
                    if (rg[2] <= 0 || rg[3] <= 0) continue;
                    /* DOS 0x3af08 / Windows 0x100117f2: 15 - 3 x (armour / 4 + structure) / the condition baseline -
                     * the port had graded armour lost alone, turning a location yellow on its first hit */
                    lv = combat_loc_level(pu, loc);
                    pick = pu->loc_gone[loc] ? 3 : lv <= 0 ? 0 : lv < 12 ? 1 : 2;
                    if (pick == 0) continue;
                    hud_draw_region(g_hud, &g_hs_dmg_lvl[pick], dx, dy, (float)rg[0] * 2.0f * g_hud_s, (float)rg[1] * 2.0f * g_hud_s, (float)rg[2] * 2.0f * g_hud_s, (float)rg[3] * 2.0f * g_hud_s);
                }
            }
            HUD_SIDE(side);

            if (mode == MERCS_MFD_SPEED && !hud_dead) {   /* the instruments (dark while shut down) */
            /* Mercs speed/throttle window 22, MERCSW 0x1002e822 /
             * 0x10016801. The frame fractions are independent of the text
             * reference at (0, .25); this gauge sits above the damage viewer. */
            if (g_layout_n > 1) {
                const hud_win tw = mercs_instrument(MERCS_HUD_SPEED);
                const int TW = tw.x1 - tw.x0 + 1, TH = tw.y1 - tw.y0 + 1;
                const int vx = fx16_round((long long)(TW - 1) * 0x1e7a);
                const int vy = fx16_round((long long)(TH - 1) * 0x8dc9);
                const int c0 = TW - vx, top = TH - (vy + vy / 2) - 3, zero = TH - vy / 2 - 2;
                HUD_SIDE(side);
                float th2 = fabsf(g_throttle);
                if (th2 > 1) th2 = 1;
                {   /* x = (setting << 16) x vy / 0x400 (setting 0..1024: 3Dfx ftol(+8 x 65536 + 0.5), DOS +8), reversing
                     * -(x / 2); smoothed s += (x - s) >> 1 a frame (DOS 0x96864 / 3Dfx 0x1024c828, see g_heat_s), v = s >> 16
                     * (DOS 0x313ae..0x31419, 3Dfx 0x1001c67d..0x1001c6dc); the branch by the reverse flag (+0x2f), so a
                     * fill still on the other side of the zero line after a switch is not drawn (count v + 1 <= 0) */
                    const int32_t X = (int32_t)(((long long)(int32_t)(th2 * 1024.0f + 0.5f) << 16) * vy / 0x400);
                    int n = hud_filter_steps(&g_thr_acc, &g_thr_seen);
                    while (n-- > 0) g_thr_s += ((g_reverse ? -(X / 2) : X) - g_thr_s) >> 1;
                    if (!g_reverse) {
                        int v2 = g_thr_s >> 16;
                        if (v2 > vy - 1) v2 = vy - 1;
                        g_thr_p = v2;
                        px_band_v(&tw, c0, zero, vx - 1, v2 + 1, 0xf);
                    } else {
                        int r = -(g_thr_s >> 16);
                        if (r > vy / 2) r = vy / 2;
                        g_thr_p = -r;
                        px_band_v(&tw, c0, zero + r, vx - 1, r + 1, 7);
                    }
                }
                {
                    const float *fr = HP(10);   /* frame colour 0xa (0x1001c5b0) */
                    px_fill(&tw, tw.x0 + c0 - 1, tw.y0 + top, tw.x0 + c0 - 1, tw.y0 + TH - 1, fr);
                    px_fill(&tw, tw.x0 + TW - 1, tw.y0 + top, tw.x0 + TW - 1, tw.y0 + TH - 1, fr);
                    px_fill(&tw, tw.x0 + c0 - 1, tw.y0 + top, tw.x0 + TW - 1, tw.y0 + top, fr);
                    px_fill(&tw, tw.x0 + c0 - 1, tw.y0 + TH - 1, tw.x0 + TW - 1, tw.y0 + TH - 1, fr);
                }
                {   /* DOS footage: "NN kph" under it, right-aligned left of the bar */
                    char kph[16];
                    /* engine 0x10018470: |v| in cm/tick x 6.516 x 1.5, truncated; minus when reversing */
                    {   /* DOS 0x32085 (3Dfx 0x10018470 the same 1.5): |v| estimated as (4 max + mid + min) >> 2 of the velocity's
                         * |x|, |y|, |z| (16.16 cm/tick), / 10002 (= 1 km/h) truncated, x 1.5 truncated (0x5c3c2: frndint with
                         * the control word's rounding set to chop) - the port had rounded the last step (91.5 -> 92). The
                         * vector is the controller's whole velocity +0xf4 / +0xf8 / +0xfc (c[0x3d..0x3f]), the sign c[0xb]'s
                         * (+0x2c): the walking velocity plus the directional jets' drift plus the vertical speed - DOSBox
                         * ORANSCN1 Nova, forward jets (GreyHome) from a standstill: 60 / 141 / 232 / 298 / 364 / 391 kph
                         * (the port had left the drift out and read 0) */
                        float hh = g_pl.heading * 3.14159265f / 180.0f, k16 = 65536.0f / 182.0f;
                        float vx = (sinf(hh) * g_pl_speed + g_jet_vx) * k16, vz = (cosf(hh) * g_pl_speed + g_jet_vz) * k16;
                        int32_t ax = (int32_t)fabsf(vx), az = (int32_t)fabsf(vz), ay = (int32_t)(fabsf(g_sim.player_unit.vy) * 65536.0f), t3;
                        if (ax < az) { t3 = ax; ax = az; az = t3; }
                        if (ax < ay) { t3 = ax; ax = ay; ay = t3; }
                        snprintf(kph, sizeof kph, "%d kph", (int)((double)(((4 * ax + az + ay) >> 2) / 10002) * 1.5) * (g_spd_srv < 0 ? -1 : 1));   /* the sign c[0xb]'s (+0x2c, 0x100184bf) */
                    }
                    if (getenv("MERCS_KPH_TRACE")) fprintf(stderr, "kph %.2fs %s (%.1f cm/s, heading %.2f)\n", g_sim.now / 1000.0, kph, g_pl_speed, g_pl.heading);   /* tests */
                    {
                        const float *col = g_spd_srv < 0 ? HP(6) : green;
                        hud_text(g_hud, (float)tw.rx * tw.ux, (float)tw.ry * tw.uy, "Speed:", col);
                        hud_text(g_hud, (float)tw.rx * tw.ux, (float)tw.ry * tw.uy + hud_text_height(g_hud), kph, col);
                    }
                }
                HUD_SIDE(side);
            }
            }
            float tv_wf = 1, tv_hf = 1;
            if (mode == MERCS_MFD_TARGET && g_layout_n > HUD_TARGET_TEXT && !g_tgtdisp_off && pow_window(1, &tv_wf, &tv_hf)) {
                /* CPIT supplies the original lower-left picture and text rectangles, 17 and 18. */
                const hud_win tvw = cpit_window(HUD_TARGET_VIEW, 0, 0), txw = cpit_window(HUD_TARGET_TEXT, 0x28f, 0x28f);
                HUD_SIDE(side);   /* left-hand: the box, its picture and text lines */
                float bx = (float)tvw.x0 * tvw.ux, by = (float)tvw.y0 * tvw.uy, bw = (float)(tvw.x1 - tvw.x0) * tvw.ux, bh = (float)(tvw.y1 - tvw.y0) * tvw.uy;
                int shaded = (g_tgtdisp == 2 || (g_tgtdisp == 1 && g_tex_kind != 3)) && tgt >= 0 && g_cockpit && g_pow == 2;   /* drawn in 3D before the HUD */
                int full = tv_wf >= 1.0f && tv_hf >= 1.0f;
                if (!full) { bx += bw * (1.0f - tv_wf) * 0.5f; by += bh * (1.0f - tv_hf) * 0.5f; bw *= tv_wf; bh *= tv_hf; }
                const int snow = g_snow_show[1];   /* display damage: SNOWCLR instead of the contents (0x10021710), also in the moving box */
                if (!shaded) {   /* cleared to colour 0, opaque (0x100628a1; DOSBox 0,0,0): the whole window, both edges */
                    if (full) px_fill(&tvw, tvw.x0, tvw.y0, tvw.x1, tvw.y1, HP(0));
                    else hud_rect(g_hud, bx, by, bw, bh, HP(0));
                }
                if (snow) draw_snow(bx, by, bw, bh);
                if (g_pow != 2 && tgt < 0 && g_nav_sel >= 0 && current_nav(NULL) >= 0 && g_hs_vtgt_nav.tex && !snow) {
                    /* starting up / shut down (0x10021730 / 0x100217f0 -> 0x10021300 with the window set to the moving
                     * rectangle): a selected nav point's emblem (0x109 / 0x106 at half the rectangle's width and height,
                     * 0x10023120) - centred, clipped to the box as it opens or closes; a mech or building target: black */
                    hud_clip(g_hud, bx, by, bw, bh);
                    hud_draw(g_hud, &g_hs_vtgt_nav, bx + bw * 0.5f, by + bh * 0.5f, NULL);
                    hud_clip_off(g_hud);
                }
                {   /* the frame in colour 8 (0x10006450 with edx 8 on every 0x10021300 path; DOSBox 60,0,0) */
                    const float *tfr = HP(8);
                    if (full) px_frame(&tvw, tfr);
                    else {   /* the moving box while it opens / closes */
                        hud_line(g_hud, bx, by, bx + bw, by, 1.2f, tfr); hud_line(g_hud, bx + bw, by, bx + bw, by + bh, 1.2f, tfr);
                        hud_line(g_hud, bx + bw, by + bh, bx, by + bh, 1.2f, tfr); hud_line(g_hud, bx, by + bh, bx, by, 1.2f, tfr);
                    }
                }
                if (full && g_pow == 2) {   /* (not while starting up / shut down: the box alone - DOSBox YELLSCN1 / TNJ1SCN1 start-up)
                     * DOS footage: a mech target shows its damage outline, name below in red, range in green;
                     * otherwise the current nav point: VTGT_NP6 emblem, name and range below in yellow */
                    char info[48];
                    /* The text in window 18: the name at the window's corner (its text
                     * routine +0x58 / 0x10063e49 with 0, 0); the range "\n%3ldm" / "\n%2.2fk" through the wrapping text
                     * routine (DOS 0x13ad0 / 3Dfx 0x100066c0): inset by (0.01, 0.01) of the window (DOS 0x95748 / 0x9574c
                     * = 0x28f, rounded), one font height a line, so on the second line. DOSBox YELLSCN1: name 32 / 705,
                     * range 35 / 720 at 1024 x 768 (20 / 440, 22 / 454 at 640 x 480); the port had 6 px below the box and
                     * an 18 px line */
                    const int fhp = (int)lrintf(hud_text_height(g_hud) / txw.uy);
                    const float ty = (float)txw.y0 * txw.uy, nx = (float)txw.x0 * txw.ux, rgx = (float)txw.rx * txw.ux,
                                rgy = (float)(txw.ry + fhp) * txw.uy;
                    if (tgt >= 0) {
                        dmg_set *ds = NULL;
                        if (getenv("MERCS_TARGET_OUTLINE") && g_sim.armed[tgt]) ds = target_outline(g_ma ? g_ma : NULL, g_actors[tgt].skel);   /* the damage outline instead (tests) */
                        if (!shaded && !snow) {   /* the target's wireframe, from where the player stands */
                            const world_actor *ta2 = &g_actors[tgt];
                            float ddx = (float)g_pl.origin[0] - (float)ta2->mech.origin[0], ddz = (float)g_pl.origin[2] - (float)ta2->mech.origin[2];
                            float brg = atan2f(ddx, ddz) * 180.0f / 3.14159265f;
                            static const float wblue[4] = {0.0f, 48.0f / 255.0f, 215.0f / 255.0f, 1.0f};
                            /* the posed parts are already in the world (heading applied): only the viewing direction turns */
                            {   /* the viewer's camera (wire_camera): level at the mech's centre, 3 R back along the bearing */
                                float sr0 = g_wire_r[0], sr1 = g_wire_r[1], sf0 = g_wire_f[0], sf1 = g_wire_f[1];
                                float R = g_sim.radius ? g_sim.radius[tgt] : 545.0f, ch = g_sim.centre_h ? g_sim.centre_h[tgt] : 550.0f;
                                (void)brg; (void)wblue;
                                wire_camera((float)ta2->mech.origin[0], (tgt < g_sim.actor_count ? g_sim.units[tgt].y : (float)ta2->mech.origin[1]) + ch, (float)ta2->mech.origin[2], R);
                                g_wire_unit = g_sim.armed[tgt] ? &g_sim.units[tgt] : NULL;
                                draw_target_wire(&ta2->mech, 0, ta2->mech.part_count, head, bx, by, bw, bh, HP(7));   /* colour 7 (0x28810's first argument) */
                                g_wire_unit = NULL; g_wire_persp = 0;
                                g_wire_r[0] = sr0; g_wire_r[1] = sr1; g_wire_f[0] = sf0; g_wire_f[1] = sf1;
                            }
                        }
                        if (ds && !snow) {
                            float ow = hud_px_x(g_hud, (float)ds->lvl[0].w), oh = hud_px_y(g_hud, (float)ds->lvl[0].h);
                            draw_outline((const hud_sprite *)ds->lvl, (const int (*)[4])ds->regions, &g_sim.units[tgt], bx + (bw - ow) * 0.5f, by + (bh - oh) * 0.5f);
                        }
                        {   /* the text line (0x10020e40): the inspection result for 0x16a ticks, else the name (unit +0xe8,
                             * "Mech" when empty), in the side's colour (0x1002119f: alliance 0 colour 0xe, 1 0xa, 2 6) */
                            const char *it = inspect_text(tgt, -1);
                            mfd_target_text(g_hud, nx, ty, it ? it : g_actors[tgt].name[0] ? g_actors[tgt].name : "Mech",
                                     g_actors[tgt].alliance == 2 ? HP(6) : g_actors[tgt].friendly ? HP(0xe) : HP(0xa));
                        }
                        range_text(info, sizeof info, tdist);
                        mfd_target_text(g_hud, rgx, rgy, info, green);
                    } else if (g_tgt_thing >= 0 && g_tgt_thing < g_sim.bld_count && (g_sim.bld[g_tgt_thing].hp <= 0 ||
                               (g_world && g_sim.bld[g_tgt_thing].intact >= 0 && g_sim.bld[g_tgt_thing].intact < g_mech.part_count && g_mech.parts[g_sim.bld[g_tgt_thing].intact].hidden))) {
                        g_tgt_thing = -1;   /* destroyed: the target is dropped (engine 0x10046a90 clears DAT_1025a664) */
                    } else if (g_tgt_thing >= 0 && g_tgt_thing < g_sim.bld_count) {   /* a structure: its name, then (inspected)
                                                                                         * what it holds, and the range */
                        const struct msim_building *gb = &g_sim.bld[g_tgt_thing];
                        float cx = (gb->mn[0] + gb->mx[0]) * 0.5f - (float)g_pl.origin[0], cz = (gb->mn[2] + gb->mx[2]) * 0.5f - (float)g_pl.origin[2];
                        if (!snow && g_world && gb->intact >= 0 && gb->intact < g_mech.part_count) {   /* the structure's wireframe */
                            static const float wblue[4] = {0.0f, 48.0f / 255.0f, 215.0f / 255.0f, 1.0f};
                            const mech3d_part *bp = &g_mech.parts[gb->intact];
                            float brg = atan2f(-cx, -cz) * 180.0f / 3.14159265f;
                            mech3d one;
                            memset(&one, 0, sizeof one);
                            one.parts = (mech3d_part *)bp; one.part_count = 1;
                            {   /* world placed; the camera on the structure's centre, its radius the box's half diagonal
                                 * (the engine's bounding sphere +0x34..+0x40, 0x10029890 - the port keeps a box) */
                                mech3d_part tmp = *bp;
                                float sr0 = g_wire_r[0], sr1 = g_wire_r[1], sf0 = g_wire_f[0], sf1 = g_wire_f[1];
                                float hx = (gb->mx[0] - gb->mn[0]) * 0.5f, hy = (gb->mx[1] - gb->mn[1]) * 0.5f, hz = (gb->mx[2] - gb->mn[2]) * 0.5f;
                                one.parts = &tmp;
                                (void)brg; (void)wblue;
                                wire_camera((gb->mn[0] + gb->mx[0]) * 0.5f, (gb->mn[1] + gb->mx[1]) * 0.5f, (gb->mn[2] + gb->mx[2]) * 0.5f, sqrtf(hx * hx + hy * hy + hz * hz));
                                draw_target_wire(&one, 0, 1, head, bx, by, bw, bh, HP(7));
                                g_wire_persp = 0;
                                g_wire_r[0] = sr0; g_wire_r[1] = sr1; g_wire_f[0] = sf0; g_wire_f[1] = sf1;
                            }
                        }
                        {   /* one text line (0x10020e40): the inspection result for 0x16a ticks, else the name ("Installation"
                             * when empty); its colour by the thing's alliance (0x10021167) - not kept by the port: red */
                            const char *it = inspect_text(-1, g_tgt_thing);
                            mfd_target_text(g_hud, nx, ty, it ? it : gb->name[0] ? gb->name : "Installation", red);
                        }
                        range_text(info, sizeof info, sqrtf(cx * cx + cz * cz));
                        mfd_target_text(g_hud, rgx, rgy, info, green);
                    } else {
                        const char *label = NULL;
                        /* only a nav point the player selected (NEXT / PREV_NAVPOINT): the viewer draws the player's target
                         * record +0xdc, black when it is none (0x10021300) - DOSBox YELLSCN1 / TNJ1SCN1: the box stays black
                         * from the start (60 s) with no nav selected; the port had shown the current objective's nav */
                        int nv = g_nav_sel >= 0 ? current_nav(&label) : -1;
                        if (nv >= 0) {
                            if (g_hs_vtgt_nav.tex && !snow) hud_draw(g_hud, &g_hs_vtgt_nav, (float)(tvw.x0 + (tvw.x1 - tvw.x0) / 2) * tvw.ux, (float)(tvw.y0 + (tvw.y1 - tvw.y0) / 2) * tvw.uy, NULL);   /* at ((x1 - x0) / 2, (y1 - y0) / 2) of the window (0x100213d1, C division): DOSBox (114, 622) at 1024 x 768 */
                            mfd_target_text(g_hud, nx, ty, label, HP(2));   /* DOSBox YELLSCN1 "Nav Epsilon": 190,146,0 = index 2 */
                            /* the range as for any target (the player's +0xc4, formatted by range_text) in the text
                             * routine's colour 0xe (DOS 0x30180 / 3Dfx 0x1002118f reset it after the name): DOSBox YELLSCN1
                             * "1.08k" in green for Nav Epsilon at 1075 m (the port had " 1075m" in the name's yellow) */
                            range_text(info, sizeof info, sqrtf((g_sim.navs[nv].x - (float)g_pl.origin[0]) * (g_sim.navs[nv].x - (float)g_pl.origin[0]) +
                                                                (g_sim.navs[nv].z - (float)g_pl.origin[2]) * (g_sim.navs[nv].z - (float)g_pl.origin[2])));
                            mfd_target_text(g_hud, rgx, rgy, info, green);
                        }
                    }
                }
                HUD_SIDE(side);
            }
            if(mode==MERCS_MFD_HEAT_BAR && !hud_dead) {
                /* DOS0x3de5c: paired yellow ends, blue/red middle, saturation
                 * all red. All coordinates are this selected MFD's fields. */
                const hud_win hw=mfd_window(slot);
                const int length=fx16_round((long long)(hw.x1-hw.x0)*0x8000);
                const int by=fx16_round((long long)(hw.y1-hw.y0)*0x8000);
                const int height=fx16_round((long long)(hw.y1-hw.y0)*0x2db7)-1;
                const int32_t x=(int32_t)((int64_t)(int32_t)(pu->heat*65536.0f)*length/100);
                int steps=hud_filter_steps(&g_heat_acc,&g_heat_seen),p;
                while(steps-->0)g_heat_s+=(x-g_heat_s)>>1;
                p=g_heat_s>>16;g_heat_x=x;g_heat_p=p;
                if(p<1)px_band_h(&hw,0,by,length,height,7);
                else if(p>=length)px_band_h(&hw,0,by,length,height,11);
                else {
                    int edge=p<length/2?p:length-p, middle=length-2*edge;
                    px_band_h(&hw,0,by,edge,height,3);
                    px_band_h(&hw,edge+1,by,middle,height,p<length/2?7:11);
                    px_band_h(&hw,edge+1+middle,by,edge,height,3);
                }
                hud_text(g_hud,hw.rx*hw.ux,hw.ry*hw.uy,
                    (pu->shutdown_pending||pu->shutdown)?(pu->override?"Overridden":"Shutdown..."):"Heat",
                    (pu->shutdown_pending||pu->shutdown)?red:green);
            }
            if ((mode == MERCS_MFD_HEAT_DIAL || mode == MERCS_MFD_HEAT_RATE) && !hud_dead) {   /* the instruments (dark while not up) */
            /* Mercs defaults to the circular heat meter (generic mode 12),
             * with a separate dH/dT bar (mode 13). Original window/table and
             * painters: MERCSW 0x100b2cf0, 0x10016254, 0x10016651. */
            {
                const hud_win hw = mercs_instrument(MERCS_HUD_HEAT);
                const hud_win dw = mercs_instrument(MERCS_HUD_DHDT);
                const hud_win ref = hud_window(144, 136, 177, 188, 0, 0x8000);
                const int hlen = fx16_round((long long)(hw.x1 - hw.x0) * 0xea4e);
                const int dlen = fx16_round((long long)(dw.x1 - dw.x0) * 0x8000);
                const int bh = fx16_round((long long)(dw.y1 - dw.y0) * 0x2db7) - 1;
                const int by = fx16_round((long long)(dw.y1 - dw.y0) * 0x8000);
                const int HW = hw.x1 - hw.x0 + 1, HH = hw.y1 - hw.y0 + 1;
                if (mode == MERCS_MFD_HEAT_DIAL) {
                    int p, a, angle, n = hud_filter_steps(&g_heat_acc, &g_heat_seen);
                    const int32_t hx = (int32_t)((long long)(int32_t)((double)pu->heat * 65536.0) * hlen / 100);
                    /* The original VFX lines are clipped by this instrument's
                     * inclusive window; its aspect-correct rim can extend a
                     * few pixels above/below the window at 320x200. */
                    hud_clip(g_hud, (float)hw.x0 * hw.ux, (float)hw.y0 * hw.uy,
                             (float)HW * hw.ux, (float)HH * hw.uy);
                    g_heat_x = hx;
                    while (n-- > 0) g_heat_s += (hx - g_heat_s) >> 1;
                    p = g_heat_p = g_heat_s >> 16;
                    angle = p >= hlen ? 355 : hlen > 0 ? p * 360 / hlen : 0;
                    px_line(&hw, hw.x0 + HW / 2, hw.y0, hw.x0 + HW / 2, hw.y0 + HH / 2, HP(2));
                    if (angle > 0) {
                        for (a = 0; a < angle; a += 15) mercs_heat_tick(&hw, a, w, h);
                        mercs_heat_tick(&hw, angle, w, h);
                    }
                    if (pu->shutdown_pending || pu->shutdown)
                        hud_text(g_hud, (float)hw.rx * hw.ux, (float)hw.ry * hw.uy, pu->override ? "Overridden" : "Shutdown...", red);
                    else hud_text(g_hud, (float)hw.rx * hw.ux, (float)hw.ry * hw.uy, "Heat", green);
                    hud_clip_off(g_hud);
                }
                if (mode == MERCS_MFD_HEAT_RATE) {   /* The Mercs rate consumer uses net fixed16 heat >> 6,
                     * retaining small gains and cooling below zero. Filter
                     * shift 4; the existing 30 Hz sampling is retained. */
                    static int32_t sm, acc_t;
                    static int initialized;
                    int32_t s2;
                    int f = 0, c1 = 7, c2 = 7;
                    float dnet = 0;
                    if (!initialized || g_sim.now < acc_t) {
                        initialized = 1; acc_t = g_sim.now; sm = 0;
                        ((combat_unit *)pu)->dhdt_net = 0;
                    }
                    if (g_sim.now - acc_t >= 33) {
                        dnet = pu->dhdt_net / (float)((g_sim.now - acc_t) / 33);
                        ((combat_unit *)pu)->dhdt_net = 0;
                    }
                    while (g_sim.now - acc_t >= 33) {
                        const int32_t x = (int32_t)(((int64_t)floor((double)dnet * 65536.0)) >> 6);
                        sm += (x - sm) >> 4;
                        acc_t += 33;
                        if (g_sim.now - acc_t > 1000) acc_t = g_sim.now;
                    }
                    s2 = g_dhdt_s = sm;
                    if (s2 < 1) s2 = 0;
                    else if (s2 < 0x300) { c1 = 3; c2 = 7; }
                    else { c1 = 11; c2 = 3; s2 -= 0x300; }
                    if (s2 > 0) { f = (int)((long long)dlen * s2 / 0x300); if (f < 0) f = 0; if (f > dlen) f = dlen; }
                    if (f > 0) px_band_h(&dw, 0, by, f, bh, c1);
                    if (f < dlen) px_band_h(&dw, f, by, dlen - f, bh, c2);
                }
                if (mode == MERCS_MFD_HEAT_RATE) hud_text(g_hud, (float)dw.x0 * dw.ux, (float)(dw.y0 + ref.ry - ref.y0) * dw.uy, "dH/dT", green);
            }
            }
    g_mfd_active=-1; g_vport=old_vport; HUD_SIDE(0);
}
static void mercs_mfd_draw_all(int w,int h)
{
    int slot;
    for(slot=0;slot<MERCS_MFD_COUNT;slot++) mercs_mfd_draw_one(slot,w,h);
}

/* The original hides the player's whole object for an auxiliary camera,
 * then restores it for the main view. Keep shots, debris and other actors;
 * they follow the external player in the composed layer. */
static void mfd_world_actors(glr *r,int satellite)
{
    static mech3d_part *parts;
    static int capacity;
    mech3d world=g_frame_layer;
    int first=g_aux_player_first, end=g_aux_player_end;
    if(g_aux_cockpit_first>=0 && g_aux_cockpit_first<world.part_count)
        world.part_count=g_aux_cockpit_first;
    if(satellite || (first>=0 && end>first && end<=world.part_count)) {
        int q,count=0,need=world.part_count+(satellite?g_pl.part_count:0);
        if(need>capacity) {
            mech3d_part *grown=realloc(parts,(size_t)need*sizeof *parts);
            if(!grown) { world.part_count=first>=0?first:0; goto upload; }
            parts=grown;capacity=need;
        }
        for(q=0;q<world.part_count;q++)
            if(!(first>=0&&q>=first&&q<end))parts[count++]=world.parts[q];
        if(satellite && !g_sim.player_unit.body_detached) {
            const unsigned fly=mech3d_fly_mask(g_sim.player_unit.loc_gone);
            for(q=0;q<g_pl.part_count;q++) {
                if(mech3d_fly_loc(&g_pl,q,fly))continue;
                parts[count]=g_pl.parts[q];parts[count].map_alliance=0;
                parts[count].map_flags=(g_sim.player_unit.destroyed?2u:0u)|(g_sim.player_unit.shutdown?0x10u:0u);
                count++;
            }
        }
        world.parts=parts;world.part_count=count;
    }
upload:
    glr_set_actors(r,&world,(const uint8_t (*)[3])g_scene.palette,g_scene.slot,g_camo,g_clan);
}

static void mercs_mfd_draw_views(glr *r,const glr_view *base,int w,int h)
{
    int slot, old_vport=g_vport;
    if (!g_world || !HUD_VIEW || !g_sim_ok || g_hud_off) return;
    g_wcam_live=0;
    for(slot=0;slot<MERCS_MFD_COUNT;slot++) {
        const int mode=g_hud_settings.mode[slot];
        glr_view v=*base;
        /* Auxiliary cameras have independent orientation, including when the
         * main cockpit is attached to a fallen mech. */
        v.look_rot_ok=0;
        hud_win mw;
        int left,bottom,width,height;
        if (mode < MERCS_MFD_REAR || mode > MERCS_MFD_TARGET) continue;
        if (mode==MERCS_MFD_SATELLITE && !g_combat_options.dif[9])continue;
        v.satellite=0;v.ortho_w=0;
        g_vport=mfd_view_kind(mode);
        HUD_BEGIN(g_hud,w,h);HUD_SIDE(mfd_side(slot));mw=mfd_window(slot);
        left=(int)lroundf(hud_screen_x(g_hud,(float)mw.x0*mw.ux));
        bottom=h-(int)lroundf(hud_screen_y(g_hud,(float)(mw.y1+1)*mw.uy));
        width=(int)lroundf(hud_screen_x(g_hud,(float)(mw.x1+1)*mw.ux))-left;
        height=(int)lroundf(hud_screen_y(g_hud,(float)(mw.y1+1)*mw.uy))-
               (int)lroundf(hud_screen_y(g_hud,(float)mw.y0*mw.uy));
        hud_end(g_hud);
        if (width<=0 || height<=0) continue;
        mfd_world_actors(r,mode==MERCS_MFD_SATELLITE);
        int wcam = -1;   /* F9 weapon camera: the followed missile in flight */
        if (mode == MERCS_MFD_WEAPON) {
            /* engine 0x100119xx: the followed projectile (0x100469b0); none: the player's newest projectile if it is a
             * missile (0x10046a00) - one projectile until it dies (the same one the ordnance camera follows) */
            wcam = followed();
            if (wcam < 0 && follow_newest()) wcam = followed();
        }
        float vp_wf = 1, vp_hf = 1, td_wf = 1, td_hf = 1;
        int vp_on = pow_window(1, &vp_wf, &vp_hf), td_on = pow_window(1, &td_wf, &td_hf);   /* windows 2 and 17 (see pow_update) */
        if (g_vport && (g_vport != 3 || wcam >= 0) && vp_on && !g_snow_show[0] && vp_wf * vp_hf > 0) {
            /* DOS viewport box x 827-991, y 559-686 at 1024x768 = 516.9..619.4 x 349.4..428.8 in 640x480 units */
            glr_view vv = v;
            int px=left, py=bottom, pw=width, ph=height;
            if (vp_wf < 1.0f || vp_hf < 1.0f) {
                /* powering up / down (0x10011aa0 / 0x10011b70): the view is drawn into the window as it stands - the camera's
                 * window is the moving rectangle (0x1002ab50), its centre the projection centre and its half width the focal
                 * scale (0x1002f740: focal x = zoom x hw, y = aspect x zoom x hw) - so the picture keeps the full window's
                 * shape scaled by the width, clipped to the rectangle's height */
                int npw = (int)((float)pw * vp_wf), nph = (int)((float)ph * vp_hf), sph = (int)((float)ph * vp_wf);
                if (npw < 1) npw = 1;
                if (nph < 1) nph = 1;
                if (sph < 1) sph = 1;
                vv.clip[0] = px + (pw - npw) / 2; vv.clip[1] = py + (ph - nph) / 2; vv.clip[2] = npw; vv.clip[3] = nph;
                px += (pw - npw) / 2; py += (ph - sph) / 2; pw = npw; ph = sph;
            }
            if (wcam >= 0) {   /* 0x10044f60: AT the projectile, yaw along its horizontal velocity, level (pitch and roll 0) */
                const msim_shot *ms = &g_sim.shots[wcam];
                vv.look_yaw = atan2f(ms->vx, ms->vz) * 180.0f / 3.14159265f;
                vv.look_pitch = 0.0f;
                vv.eye[0] = ms->x; vv.eye[1] = ms->y; vv.eye[2] = ms->z;
            } else if (g_vport == 1) { vv.look_yaw = g_pl.heading + 180.0f; vv.look_pitch=0; }
            else if (g_vport == 2) vv.look_pitch = -90.0f;
            else if (g_vport == 5) {
                vv.free_cam=1; vv.ortho_w=100000.0f/(float)(1<<g_sat_i);
                vv.satellite=1;
                vv.eye[0]=(float)g_pl.origin[0];vv.eye[2]=(float)g_pl.origin[2];
                vv.eye[1]=g_sim.player_unit.y+100000.0f;
                vv.look_yaw=0;vv.look_pitch=-90.0f;
            }   /* rear / straight down */
            vv.vfov = 2.0f * atanf(tanf(73.74f * 3.14159265f / 360.0f) / 2.0f) * 180.0f / 3.14159265f;   /* all three at zoom 2.0 (0x1001bc50) */
            glr_draw_rect(r, &vv, px, py, pw, ph);
            rb_viewport(0, 0, w, h);
        }
        if (wcam >= 0) g_wcam_live = 1;
        if (mode == MERCS_MFD_TARGET && (g_tgtdisp == 2 || (g_tgtdisp == 1 && g_tex_kind != 3)) && g_layout_n > HUD_TARGET_VIEW && g_tgt_sel >= 0 && g_tgt_sel < g_actor_count && !g_hud_off && td_on && g_pow == 2 && !g_snow_show[1] &&
            !(g_tgt_sel < g_sim.actor_count && g_sim.units[g_tgt_sel].body_detached)) {
            /* (the model only with the mech up: 0x10021300 renders it when DAT_101de868 == 2 - starting up or shut down
             * the box stays black, also while it opens / closes) */
            /* the shaded target view (mode 2): the camera at the target's centre less 3 R along the player -> target
             * bearing (0x102475d4 = 3.0), level, zoom 2.0 */
            const world_actor *ta = &g_actors[g_tgt_sel];
            glr_view vv = v;
            float R = g_sim.radius ? g_sim.radius[g_tgt_sel] : 545.0f, ch = g_sim.centre_h ? g_sim.centre_h[g_tgt_sel] : 550.0f;
            float lx = (float)ta->mech.origin[0] - (float)g_pl.origin[0], lz = (float)ta->mech.origin[2] - (float)g_pl.origin[2], ll = sqrtf(lx * lx + lz * lz);
            int px=left, py=bottom, pw=width, ph=height;
            {   /* powering up / down: the window about its centre (0x10021730 / 0x100217f0 draw it there) */
                int npw = (int)((float)pw * td_wf), nph = (int)((float)ph * td_hf);
                px += (pw - npw) / 2; py += (ph - nph) / 2; pw = npw > 1 ? npw : 1; ph = nph > 1 ? nph : 1;
            }
            if (ll < 1.0f) { lx = 0; lz = 1; ll = 1; }
            vv.free_cam = 1;
            vv.eye[0] = (float)ta->mech.origin[0] - lx / ll * 3.0f * R; vv.eye[2] = (float)ta->mech.origin[2] - lz / ll * 3.0f * R;
            vv.eye[1] = (g_tgt_sel < g_sim.actor_count ? g_sim.units[g_tgt_sel].y : (float)ta->mech.origin[1]) + ch;
            vv.look_yaw = atan2f(lx, lz) * 180.0f / 3.14159265f; vv.look_pitch = 0.0f;
            vv.vfov = 2.0f * atanf(tanf(73.74f * 3.14159265f / 360.0f) / 2.0f) * 180.0f / 3.14159265f;
            if (g_tgtdisp == 1) {
                /* mode 1 in the 3D editions: the target alone, solid in the HUD's dark blue (0, 48, 215), lit, on black
                 * (the engine renders the object with its wire flag, 0x10021300 / 0x1002a4b0 class colour 7; the
                 * user's 3D-edition memory: solid). Its blown-off locations left out */
                static mech3d one;
                static int cap;
                const mech3d *src = &ta->mech;
                int q2;
                if (cap < src->part_count) { mech3d_part *np = realloc(one.parts, (size_t)src->part_count * sizeof *np); if (np) { one.parts = np; cap = src->part_count; } }
                one.part_count = 0;
                for (q2 = 0; q2 < src->part_count && q2 < cap; q2++) {
                    int gq = src->parts[q2].group;
                    if (g_sim.armed[g_tgt_sel] && gq >= 1 && gq <= 8 && gq != 3 && g_sim.units[g_tgt_sel].loc_gone[gq - 1]) continue;
                    one.parts[one.part_count] = src->parts[q2];
                    {   /* each location in its damage colour, as the damage outline (0x1001b620: levels 6 / 3 / 11 -
                         * here the HUD blue, yellow, red): armour lost front or rear in fifteenths, 0 intact, 1-11 yellow,
                         * 12+ red; blown off: gone */
                        unsigned tint = 0x1000000u | LOC_RGB[loc_level(g_sim.armed[g_tgt_sel] ? &g_sim.units[g_tgt_sel] : NULL, gq)];
                        one.parts[one.part_count].tint = tint;
                    }
                    one.part_count++;
                }
                glr_set_actors(r, &one, (const uint8_t (*)[3])g_scene.palette, g_scene.slot, g_camo, g_clan);
                vv.solo = 1; vv.fog_density = 0; vv.wire = 0;
                vv.mono[0] = vv.mono[1] = vv.mono[2] = 0.0f; vv.mono[3] = 2.0f;   /* each part's tint */
                vv.ambient = 0.35f; vv.point_light = 0;
            }
            glr_draw_rect(r, &vv, px, py, pw, ph);
            rb_viewport(0, 0, w, h);
        }

    }
    g_vport=old_vport;
    rb_viewport(0,0,w,h);
}
