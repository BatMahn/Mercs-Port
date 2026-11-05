/* Inspect the exact font RGBA submissions without backend-specific texture queries.
 * The same instrumented real HUD also backs the accelerated SHP cache gate. */
#include "hud_backend_fixture.h"

size_t accelerated_hud_atlas_snapshot(hud *h, unsigned char **out)
{
    return shp_fixture_atlas_snapshot(h,out);
}
