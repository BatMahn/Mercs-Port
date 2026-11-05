/* Common candidate/baseline control through the actual fx_texture archive
 * decoder/cache and actual card renderer. Expected frames independently expand
 * the same original CEL indices using each submitted palette. This is a native
 * GPU equivalence check, not an original framebuffer capture. */
static void actual_effect_cache(const char *archive)
{
    char err[256];prj_record record={0};texture decoded={0},reference[3]={{0}};
    texture *cached,rear={0};glr_sprite card={0};glr_view view;
    uint32_t rear_pixel=0xff33cc22u;const unsigned char rear_rgb[3]={34,204,51};
    uint8_t actual[128*128*4],expected[128*128*4];
    int slot,cel,pass,i;size_t n;uint32_t *saved;
    test_case="actual original CEL decode/cache after palette updates";
    g_fx_ta=prj_open(archive,err,sizeof err);
    pcheck(g_fx_ta!=NULL,"original resource archive opens read-only");
    if(!g_fx_ta)return;
    pcheck(fx_load(g_fx_ta,&g_fxd)==0,"actual MW2_XPL1/MW2_SHT1 definitions load");
    slot=g_fxd.render[22][0];
    pcheck(slot>=0 && slot<512 && g_fxd.tex[slot].frames>0,"actual type22 card has a first CEL");
    if(slot<0 || slot>=512 || g_fxd.tex[slot].frames<=0)goto done;
    cel=g_fxd.tex[slot].cel[0];
    pcheck(prj_read(g_fx_ta,prj_find_type(g_fx_ta,"CEL"),cel,&record)==PRJ_OK,"actual type22 CEL checksum validates");
    if(!record.data)goto done;
    pcheck(tex_decode(record.data,record.size,TEX_DOS,&decoded)==0,"independent index-bearing CEL decode");
    fprintf(stderr,"Actual type22 cache control: CEL%d %s %dx%d, payload%zu bytes\n",cel,record.name,decoded.w,decoded.h,record.size);
    prj_record_free(&record);
    if(!decoded.rgba)goto done;
    n=(size_t)decoded.w*(size_t)decoded.h;
    dos_apply_palette(test_renderer,g_dos_slotpal[0]);
    g_fx_ati=3;cached=fx_texture(cel);
    pcheck(cached!=NULL && cached->w==decoded.w && cached->h==decoded.h,"actual frontend CEL decoder returns expected dimensions");
    if(!cached)goto done;
    saved=malloc(n*sizeof *saved);if(!saved)exit(2);
    memcpy(saved,cached->rgba,n*sizeof *saved);
    glr_default_view(&view);view.free_cam=1;view.vfov=90;view.fog_density=0;view.ambient=1;view.bilinear=0;
    card.pos[2]=256;card.half_w=card.half_h=128;rear.w=rear.h=1;rear.rgba=&rear_pixel;
    rb_frame(128,128);rb_target(0);
    for(pass=0;pass<3;pass++) {
        int visible=0;
        dos_apply_palette(test_renderer,g_dos_slotpal[pass==1?18:0]);
        pcheck(fx_texture(cel)==cached && !memcmp(saved,cached->rgba,n*sizeof *saved),"palette update preserves actual frontend cache identity and storage");
        reference[pass].w=decoded.w;reference[pass].h=decoded.h;
        reference[pass].rgba=malloc(n*sizeof *reference[pass].rgba);if(!reference[pass].rgba)exit(2);
        for(i=0;i<(int)n;i++) {
            unsigned ix=decoded.rgba[i]&255,alpha=decoded.rgba[i]>>24;
            reference[pass].rgba[i]=(uint32_t)g_scene.palette[ix][0]|(uint32_t)g_scene.palette[ix][1]<<8|(uint32_t)g_scene.palette[ix][2]<<16|alpha<<24;
        }
        card.tex=cached;
#ifndef BASELINE_CACHED_RGB
        card.indexed=1;
#endif
        glr_set_sprites(test_renderer,&card,1);glr_draw(test_renderer,&view,128,128);
        pcheck(rb_read_rgba(0,0,128,128,actual),"actual cache RGBA readback succeeds");
        card.tex=&reference[pass];
#ifndef BASELINE_CACHED_RGB
        card.indexed=0;
#endif
        glr_set_sprites(test_renderer,&card,1);glr_draw(test_renderer,&view,128,128);
        pcheck(rb_read_rgba(0,0,128,128,expected),"independent RGB expansion readback succeeds");
        pcheck(!memcmp(actual,expected,sizeof actual),"actual cached CEL follows changed palette like independent RGB expansion");
        {glr_sprite pair[2];int x,y;
         pair[0]=card;pair[0].tex=cached;
#ifndef BASELINE_CACHED_RGB
         pair[0].indexed=1;
#endif
         memset(&pair[1],0,sizeof pair[1]);pair[1].tex=&rear;pair[1].pos[2]=512;pair[1].half_w=pair[1].half_h=256;
         glr_set_sprites(test_renderer,pair,2);glr_draw(test_renderer,&view,128,128);
         pcheck(rb_read_rgba(0,0,128,128,actual),"cached CEL with rear card readback succeeds");
         pair[0]=card;glr_set_sprites(test_renderer,pair,2);glr_draw(test_renderer,&view,128,128);
         pcheck(rb_read_rgba(0,0,128,128,expected),"reference CEL with rear card readback succeeds");
         pcheck(!memcmp(actual,expected,sizeof actual),"actual cached CEL preserves independent alpha and rear-card depth occlusion");
         for(y=32;y<96;y++)for(x=32;x<96;x++)if(memcmp(actual+(y*128+x)*4,rear_rgb,3))visible++;
         pcheck(visible>100,"actual archived CEL visibly occludes rear card, including ALLWHT palette");}
    }
    free(saved);
done:
    glr_set_sprites(test_renderer,NULL,0);
    for(i=0;i<3;i++)tex_free(&reference[i]);
    tex_free(&decoded);
    for(i=0;i<4096;i++)if(g_fxtex[i]){tex_free(g_fxtex[i]);free(g_fxtex[i]);g_fxtex[i]=NULL;}
    prj_close(g_fx_ta);g_fx_ta=NULL;
}
