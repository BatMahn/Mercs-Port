#include "mercsmap.h"
#include <assert.h>
#include <stdio.h>
#include "../tests/fixtures/controls-completion/original-map.h"
#define COUNT(x) (sizeof(x)/sizeof((x)[0]))
int main(void)
{
    size_t i;
    for(i=0;i<COUNT(original_map_materials);i++) {
        assert(mercs_map_material((unsigned)original_map_materials[i].obj,
                                 (unsigned)original_map_materials[i].material,
                                 (unsigned)original_map_materials[i].alliance)==
               (unsigned)original_map_materials[i].result);
    }
    for(i=0;i<COUNT(original_map_visibility);i++)
        assert(mercs_map_visible((unsigned)original_map_visibility[i].obj,
                                (unsigned)original_map_visibility[i].flags)==original_map_visibility[i].visible);
    for(i=0;i<COUNT(original_map_heights);i++)
        assert(mercs_map_height(original_map_heights[i].height,original_map_heights[i].low,
                               original_map_heights[i].high)==original_map_heights[i].result);
    for(i=0;i<COUNT(original_map_zoom);i++)
        assert(mercs_map_zoom(original_map_zoom[i].before,original_map_zoom[i].action,
                             original_map_zoom[i].allowed)==original_map_zoom[i].after);
    for(i=0;i<COUNT(original_map_edges);i++) {
        int p[2];
        mercs_map_edge(original_map_edges[i].width,original_map_edges[i].height,
                       original_map_edges[i].x,original_map_edges[i].y,p);
        assert(p[0]==original_map_edges[i].ex&&p[1]==original_map_edges[i].ey);
    }
    puts("PASS294 original map materials,84 object filters,117 height shades,24 zoom/availability and56 off-screen target cases");
    return 0;
}
