#ifndef MERCS_TARGET_MATH_H
#define MERCS_TARGET_MATH_H

#include <stdint.h>
#include "glr.h"

/* DOS Mercs0x35a6c: target bearing uses the engine's interpolated angle
 * tables and integer horizontal distance. A floating sqrt/atan introduces
 * different novice-aim steps near the original15/256-degree boundaries. */
static void mercs_target_bearing(const int32_t p[3], int32_t *heading,
                                 int32_t *horizontal, int32_t *elevation)
{
    int32_t a=glr_dos_atan2(p[0],p[2]),divisor,component;
    int64_t ax=p[0]<0?-(int64_t)p[0]:p[0];
    int64_t az=p[2]<0?-(int64_t)p[2]:p[2];
    if(ax>az){divisor=glr_dos_sin(a);component=p[0];}
    else {divisor=glr_dos_cos(a);component=p[2];}
    *heading=a;
    *horizontal=divisor?(int32_t)(((int64_t)component*536870912)/divisor):0;
    *elevation=glr_dos_atan2(p[1],*horizontal);
}

#endif
