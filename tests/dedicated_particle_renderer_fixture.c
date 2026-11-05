/* Test-only read access to the actual renderer's copied primitive state. */
#include "src/glr.c"
int dedicated_particle_renderer_state(const glr *r,unsigned char palette[256][3],int *dos,int *pid)
{
    memcpy(palette,r->palpar_palette,sizeof r->palpar_palette);
    *dos=r->dos;*pid=r->par_n;return r->palpar_n;
}
int dedicated_particle_renderer_primitives(const glr *r,glr_palette_particle *out,int capacity)
{
    int n=r->palpar_n;if(n>capacity)n=capacity;
    if(n>0)memcpy(out,r->palpar,(size_t)n*sizeof *out);
    return n;
}
