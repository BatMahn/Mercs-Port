/* Bounded original-backed quadtree regression. This private-implementation
 * fixture invokes the production tree builder and ground lookup; it does not
 * step the simulation or claim a completed mission. */
#include "../src/msim.c"
#include "../tests/fixtures/quadtree-integer-domain/original-quadtree.h"

static void release_tree(terrain_t *t)
{
    int i;
    if (t->ccell) for (i=0; i<t->gw*t->gh; i++) free(t->ccell[i].idx);
    free(t->ccell); free(t->co); free(t->cp); free(t->qn); free(t->qidx);
}

int main(void)
{
    size_t row, j;
    int bad_bounds=0, bad_queries=0, queries=0;
    for (row=0; row<sizeof original_quadtrees/sizeof original_quadtrees[0]; row++) {
        terrain_t t={0}; mech3d world={0}; mech3d_part part={0};
        wtb_object object={0}; wtb_vertex vertices[4]={{0}}; wtb_poly poly={0};
        const int32_t *r=original_quadtrees[row].rectangle;
        int k, a, root;
        vertices[0].x=r[0]; vertices[0].z=r[2];
        vertices[1].x=r[1]; vertices[1].z=r[2];
        vertices[2].x=r[1]; vertices[2].z=r[3];
        vertices[3].x=r[0]; vertices[3].z=r[3];
        for(k=0;k<4;k++) { vertices[k].y=1234; poly.idx[k]=(uint16_t)k; }
        poly.n=4;
        object.verts=vertices; object.vert_count=4; object.polys=&poly; object.poly_count=1;
        part.model.objects=&object; part.model.object_count=1; part.coll=5; part.has_rot=1;
        for(k=0;k<9;k++) part.rot[k]=(float)((double)original_quadtrees[row].matrix_q29[k]/536870912.0);
        world.parts=&part; world.part_count=1;
        t.gw=t.gh=1; t.cs=1000; t.x0=t.z0=-500;
        clist_build(&t,&world,NULL,0);
        if(t.nco!=1 || t.co[0].root<0) { fprintf(stderr,"%s: no root\n",original_quadtrees[row].name); return 2; }
        root=t.co[0].root;
        for(a=0;a<6;a++) if(t.qn[root].r[a]!=original_quadtrees[row].root[a]) {
            fprintf(stderr,"%s root bound%d: native %.9g original %d\n",original_quadtrees[row].name,
                    a,(double)t.qn[root].r[a],original_quadtrees[row].root[a]);
            bad_bounds++;
        }
        for(k=0;k<4;k++) {
            int child=t.qn[root].ch[k];
            if(child<0) { bad_bounds++; continue; }
            for(a=0;a<6;a++) if(t.qn[child].r[a]!=original_quadtrees[row].children[k][a]) {
                fprintf(stderr,"%s child%d bound%d: native %.9g original %d\n",
                        original_quadtrees[row].name,k,a,(double)t.qn[child].r[a],original_quadtrees[row].children[k][a]);
                bad_bounds++;
            }
        }
        for(j=0;j<16;j++) {
            float height=-98765; const float *normal=NULL;
            int result=qt_ground(&t,root,(float)original_quadtrees[row].queries[j].x,
                                 (float)original_quadtrees[row].queries[j].z,&height,&normal);
            queries++;
            if(result!=original_quadtrees[row].queries[j].result || height!=original_quadtrees[row].queries[j].height) {
                fprintf(stderr,"%s (%d,%d): native result%d height%.9g original result%d height%d\n",
                        original_quadtrees[row].name,original_quadtrees[row].queries[j].x,
                        original_quadtrees[row].queries[j].z,result,(double)height,
                        original_quadtrees[row].queries[j].result,original_quadtrees[row].queries[j].height);
                bad_queries++;
            }
        }
        release_tree(&t);
    }
    printf("%s: %zu original-backed flat trees, %d integer ground queries; %d bound mismatches, %d query mismatches\n",
           bad_bounds||bad_queries?"FAIL":"PASS",sizeof original_quadtrees/sizeof original_quadtrees[0],
           queries,bad_bounds,bad_queries);
    for(row=0;row<sizeof original_splits/sizeof original_splits[0];row++) {
        terrain_t t={0}; cobj_t object={0}; int root=qt_new(&t),child,k,a;
        if(root<0) return 2;
        for(a=0;a<6;a++) t.qn[root].r[a]=original_splits[row].parent[a];
        for(k=0;k<4;k++) {
            child=qt_child(&t,&object,root,k);
            if((child>=0)!=original_splits[row].valid[k]) {
                fprintf(stderr,"edge split%zu child%d: existence mismatch\n",row,k); bad_bounds++;
            } else if(child>=0) for(a=0;a<6;a++) if(t.qn[child].r[a]!=original_splits[row].children[k][a]) {
                fprintf(stderr,"edge split%zu child%d bound%d: native%.9g original%d\n",
                        row,k,a,(double)t.qn[child].r[a],original_splits[row].children[k][a]); bad_bounds++;
            }
        }
        release_tree(&t);
    }
    printf("%s: %zu original-backed edge child builds, including wrapped sums and strict tiny spans; total %d bound mismatches, %d query mismatches\n",
           bad_bounds||bad_queries?"FAIL":"PASS",4*sizeof original_splits/sizeof original_splits[0],bad_bounds,bad_queries);
    {
        const float rounded[6]={-1.5f,-0.5f,0.5f,1.5f,-1.75f,1.75f};
        const int32_t expected[6]={-1,0,1,2,-2,2};
        const float invalid[]={NAN,INFINITY,-INFINITY,2147483648.0f,-2147483904.0f};
        int32_t out[6]; float box[6]={0}; size_t i;
        if(!qt_bounds(rounded,out) || memcmp(out,expected,sizeof out)) {
            fprintf(stderr,"bounds quantization: nearest/ties-to-positive mismatch\n"); bad_bounds++;
        }
        for(i=0;i<sizeof invalid/sizeof invalid[0];i++) {
            box[0]=invalid[i];
            if(qt_bounds(box,out)) { fprintf(stderr,"bounds quantization: unsafe float accepted\n"); bad_bounds++; }
        }
        box[0]=-2147483648.0f; box[1]=2147483520.0f;
        if(!qt_bounds(box,out) || out[0]!=INT32_MIN || out[1]!=INT32_C(2147483520)) {
            fprintf(stderr,"bounds quantization: representable edge refused\n"); bad_bounds++;
        }
        printf("%s: native float-domain guards and half-tie rounding\n",bad_bounds?"FAIL":"PASS");
    }
    return bad_bounds||bad_queries;
}
