/* Generated only from agreeing original DOS/Windows executions. */
#ifndef MERCS_ORIGINAL_ALLIANCE_FIXTURES_H
#define MERCS_ORIGINAL_ALLIANCE_FIXTURES_H
typedef struct { const char *name, *scene; int player_table;
    const unsigned char *star; size_t star_bytes;
    int unit_count; const int *unit_tables, *unit_alliances;
    int thing_count; const int *thing_affiliations;
    unsigned initial_counts[6]; int player_alliance, affl_zero;
} mercs_alliance_fixture;
static const unsigned char alliance_star_0[] = {83,84,65,82,128,0,0,0,6,0,0,0,0,0,0,0,83,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,98,117,98,98,97,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,83,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,83,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,83,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0};
static const int alliance_unit_tables_0[] = {0,1,1,2,2,3,3,4,4,4,4};
static const int alliance_unit_alliances_0[] = {0,1,1,1,1,1,1,1,1,1,1};
static const int alliance_thing_affiliations_0[] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static const unsigned char alliance_star_1[] = {83,84,65,82,152,0,0,0,5,0,0,0,0,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,2,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,1,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,1,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,1,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,2,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0};
static const int alliance_unit_tables_1[] = {0,1,1,2,2,3,3,4,4,4,5};
static const int alliance_unit_alliances_1[] = {0,2,2,1,1,1,1,1,1,1,2};
static const int alliance_thing_affiliations_1[] = {6,6};
static const unsigned char alliance_star_2[] = {83,84,65,82,80,0,0,0,5,0,0,0,0,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,2,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0};
static const int alliance_unit_tables_2[] = {0,1,2};
static const int alliance_unit_alliances_2[] = {0,1,2};
static const int alliance_thing_affiliations_2[] = {-1,0,5,6};
static const unsigned char alliance_star_3[] = {83,84,65,82,80,0,0,0,5,0,0,0,0,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,2,0,0,0,115,116,97,114,0,0,0,0,0,0,0,0,0,0,0,0};
static const int alliance_unit_tables_3[] = {2,0,1};
static const int alliance_unit_alliances_3[] = {2,0,1};
static const int alliance_thing_affiliations_3[] = {-1,0,5,6};
static const mercs_alliance_fixture mercs_alliance_fixtures[] = {
    {"IVA1SCN1","IVA1SCN1",0,alliance_star_0,sizeof alliance_star_0,11,alliance_unit_tables_0,alliance_unit_alliances_0,45,alliance_thing_affiliations_0,{1,0,10,0,45,0},0,1},
    {"OUT3SCN1","OUT3SCN1",0,alliance_star_1,sizeof alliance_star_1,11,alliance_unit_tables_1,alliance_unit_alliances_1,2,alliance_thing_affiliations_1,{1,3,7,2,0,0},0,0},
    {"synthetic-player-0","",0,alliance_star_2,sizeof alliance_star_2,3,alliance_unit_tables_2,alliance_unit_alliances_2,4,alliance_thing_affiliations_2,{1,1,1,1,2,1},0,1},
    {"synthetic-player-2","",2,alliance_star_3,sizeof alliance_star_3,3,alliance_unit_tables_3,alliance_unit_alliances_3,4,alliance_thing_affiliations_3,{1,1,1,1,2,1},2,1},
};
#define MERCS_ALLIANCE_FIXTURE_COUNT (sizeof mercs_alliance_fixtures / sizeof mercs_alliance_fixtures[0])
#endif
