/* Generated from bounded original Mercs instructions; see JSON scope. */
typedef struct { int position[3],radius,object_radius,coefficient,ticks,health,active,has_object,want_health,want_destroyed; } original_debris_area_case;
static const original_debris_area_case original_debris_areas[] = {
{{0,0,0},100,50,8192,6,1048576,1,1,1048575,0},
{{150,0,0},100,50,8192,6,1048576,1,1,1048575,0},
{{151,0,0},100,50,8192,6,1048576,1,1,1048576,0},
{{80,80,80},100,50,8192,6,1048576,1,1,1048575,0},
{{90,90,90},100,50,8192,6,1048576,1,1,1048576,0},
{{0,0,0},100,50,8192,6,1048576,0,1,1048576,0},
{{0,0,0},100,50,8192,6,1048576,1,0,1048576,0},
{{0,0,0},100,50,128,6,1048576,1,1,1048576,0},
{{0,0,0},100,50,8192,4,1048576,1,1,1048575,0},
{{0,0,0},100,50,8192,6,1,1,1,0,0},
{{0,0,0},100,50,8192,6,0,1,1,-1,1},
{{0,0,0},100,50,8192,500,1048576,1,1,1048513,0},
{{0,0,0},-100,50,8192,6,1048576,1,1,1048575,0},
{{65536,0,0},0,0,8192,6,1048576,1,1,1048575,0},
};
typedef struct { int health,damage,want_health,want_destroyed; } original_debris_direct_case;
static const original_debris_direct_case original_debris_direct[] = {
{1048576,0,1048576,0},
{1048576,65536,983040,0},
{1048576,1048576,0,0},
{1048576,1048577,-1,1},
{0,1,-1,1},
{1,1,0,0},
{1,2,-1,1},
};
typedef struct { int type,group,location,physics,full,node,model,registered; } original_debris_registration_case;
static const original_debris_registration_case original_debris_registration[] = {
{272,3,3,0,0,1,1,1},
{368,3,3,0,0,1,1,0},
{272,0,3,0,0,1,1,0},
{272,0,0,0,0,1,1,1},
{80,3,1,0,0,1,1,1},
{112,0,0,0,0,1,1,0},
{272,3,3,-1,0,1,1,0},
{272,3,3,0,1,1,1,0},
{272,3,3,0,0,0,1,0},
{272,3,3,0,0,1,0,0},
};
typedef struct { int dos,elapsed,retained,active; } original_debris_expiry_case;
static const original_debris_expiry_case original_debris_expiry[] = {
{1,3639,0,1},
{1,3639,1,1},
{1,3640,0,1},
{1,3640,1,1},
{1,3641,0,0},
{1,3641,1,1},
{0,3619,0,1},
{0,3619,1,1},
{0,3620,0,1},
{0,3620,1,1},
{0,3621,0,0},
{0,3621,1,1},
};
