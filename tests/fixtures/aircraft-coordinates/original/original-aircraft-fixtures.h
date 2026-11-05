/* Generated from original-aircraft.json SHA256 afc2d270150429d2705eee41866512c9ec346891e8ed5194d9a1530d0941e3e2.
 * Explicit declared-record/slice scope; no full original scene or ground integration.
 * Failed target selection has no published cache: range fields are -1 sentinels. */
#ifndef MERCS_ORIGINAL_AIRCRAFT_FIXTURES_H
#define MERCS_ORIGINAL_AIRCRAFT_FIXTURES_H
#include <stdint.h>
typedef struct { const char *model; int32_t unit_class,mgeo[7],feet[3],body[3]; } mercs_original_aircraft_model;
static const mercs_original_aircraft_model original_aircraft_models[] = {
 {"STALKER",1,{771,0,-771,0,0,5898240,760},{1000,0,2000},{1000,771,2000}},
 {"SHILONEA",2,{213000,0,-213000,0,0,2949120,1550},{1000,0,2000},{1000,213000,2000}},
 {"CHIPPEWB",2,{213000,0,-213000,0,0,2949120,1550},{1000,0,2000},{1000,213000,2000}},
 {"STINGRAB",2,{213000,0,-213000,0,0,2949120,1550},{1000,0,2000},{1000,213000,2000}},
};
typedef struct { const char *name; int32_t unit_class,override_global,initial_feet_y,ground_y,candidate_body_y,center,published_y,feet_y; } mercs_original_aircraft_altitude;
static const mercs_original_aircraft_altitude original_aircraft_altitudes[] = {
 {"actual-class2-at-ground-zero",2,0,0,0,213000,213000,213000,0},
 {"class2-deck-ground-discriminator",2,0,0,199776,412776,213000,213000,0},
 {"class2-nonzero-authored-root-discriminator",2,0,12345,0,225345,213000,213000,0},
 {"declared-class1-keeps-candidate",1,0,0,199776,412776,213000,412776,199776},
 {"class2-override-global-keeps-candidate",2,1,0,199776,412776,213000,412776,199776},
};
typedef struct { const char *name,*model; int32_t source_feet[3],target_feet[3],source_center,target_center,target_flags,override_y_enabled,override_y,source_body[3],target_body[3],status,cache[3],horizontal,distance,lock_on,lock_ticks_dos,lock_ticks_windows; } mercs_original_aircraft_target;
static const mercs_original_aircraft_target original_aircraft_targets[] = {
 {"authored-nav-SHILONEA","SHILONEA",{-45000,199776,35000},{0,0,100000},771,213000,1024,0,0,{-45000,200547,35000},{0,213000,100000},1,{0,213000,100000},79057,80032,0,0,0},
 {"authored-nav-CHIPPEWB","CHIPPEWB",{-45000,199776,35000},{0,0,-100000},771,213000,1024,0,0,{-45000,200547,35000},{0,213000,-100000},1,{0,213000,-100000},142303,142847,0,0,0},
 {"authored-nav-STINGRAB","STINGRAB",{-45000,199776,35000},{200000,0,0},771,213000,1024,0,0,{-45000,200547,35000},{200000,213000,0},-7,{0,0,0},-1,-1,-1,-1,-1},
 {"declared-near-visible-aircraft","SHILONEA",{0,199776,0},{0,0,50000},771,213000,1024,0,0,{0,200547,0},{0,213000,50000},1,{0,213000,50000},50000,51527,1,364,362},
 {"declared-native-old-centre-550-negative","SHILONEA",{0,199776,0},{0,0,50000},771,213000,1024,1,550,{0,200547,0},{0,550,50000},-7,{0,0,0},-1,-1,-1,-1,-1},
 {"selector-maximum-equality","SHILONEA",{0,212229,0},{0,0,175000},771,213000,1024,0,0,{0,213000,0},{0,213000,175000},1,{0,213000,175000},175000,175000,0,0,0},
 {"selector-past-maximum","SHILONEA",{0,212229,0},{0,0,175001},771,213000,1024,0,0,{0,213000,0},{0,213000,175001},-7,{0,0,0},-1,-1,-1,-1,-1},
};
#endif
