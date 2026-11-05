/* Original condition palette and relative row layout goldens. The font
 * metrics below are read from the supplied DATABASE by the oracle. */
#include "../tools/mercsrepair_view.h"
#include "../tests/fixtures/career-return/repair_view_original_fixtures.h"
#include <assert.h>

int mercs_art_height(mercs_art *unused, int font)
{ (void)unused; return font == 5 ? 15 : font == 6 ? 16 : font == 10 ? 10 : 0; }

static void put32(uint8_t *p, int32_t value)
{
    uint32_t v = (uint32_t)value;
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v>>8); p[2] = (uint8_t)(v>>16); p[3] = (uint8_t)(v>>24);
}

int main(void)
{
    mercs_career_mech m;
    size_t i;
    int j, y[25], h[25];
    memset(&m,0,sizeof m);
    for (i = 0; i < sizeof repair_condition_fixtures/sizeof repair_condition_fixtures[0]; i++) {
        put32(m.bytes+0x760,repair_condition_fixtures[i].internal);
        put32(m.bytes+0x768,repair_condition_fixtures[i].front);
        put32(m.bytes+0x76c,repair_condition_fixtures[i].rear);
        m.bytes[0x770] = (uint8_t)repair_condition_fixtures[i].flags;
        assert(mercs_repair_view_condition(&m,0) == repair_condition_fixtures[i].result);
    }
    for (i = 0; i < sizeof repair_layout_fixtures/sizeof repair_layout_fixtures[0]; i++) {
        for (j = 0; j < 12; j++) put32(m.bytes+0x368+4*j,
            repair_layout_fixtures[i].present & (1u<<(unsigned)j) ? 1 : -1);
        mercs_repair_view_rows(NULL,&m,0,y,h);
        for (j = 0; j < 25; j++) {
            assert(y[j] == repair_layout_fixtures[i].y[j]);
            assert(h[j] == repair_layout_fixtures[i].height[j]);
        }
    }
    puts("PASS: 72 original condition states and6 original font-based row layouts");
    return 0;
}
