/* Generated from executed original DOS layout/cursor and DATABASE fonts. */
#ifndef MERCS_DEBRIEF_VIEW_LAYOUT_FIXTURES_H
#define MERCS_DEBRIEF_VIEW_LAYOUT_FIXTURES_H
typedef struct { int rows,glyphs; uint64_t hash; } mdv_layout_golden;
typedef struct { unsigned source; int finances; mdv_layout_golden expected; } mdv_section_golden;
static const mdv_section_golden mdv_section_goldens[] = {
{0,0,{4,46,UINT64_C(0x238d0956f09ed170)}},
{1,0,{10,213,UINT64_C(0x4b28d9ab1751a3ec)}},
{2,0,{6,101,UINT64_C(0xcd8df773de34aaa6)}},
{3,0,{2,29,UINT64_C(0x79203d32f37e26b4)}},
{3,1,{7,118,UINT64_C(0x30aadce88bc6bfb3)}},
{4,0,{9,236,UINT64_C(0x7ca81aa9761d1e6c)}},
{4,1,{7,118,UINT64_C(0xd598270861fc29b4)}},
{5,0,{5,119,UINT64_C(0x4f7945f167f488a2)}},
{5,1,{7,118,UINT64_C(0x302e962ab4d1aaa8)}},
};
typedef struct { const char *resource; int mode,outcome; unsigned source; mdv_layout_golden pages[2]; } mdv_page_golden;
static const mdv_page_golden mdv_page_goldens[] = {
{"TRA1DBFS",0,2,1,{{16,437,UINT64_C(0xcb0580285f439a8b)},{12,840,UINT64_C(0xe72f8903dc38e7f9)}}},
{"TRA1DBFF",0,3,1,{{18,473,UINT64_C(0x1f8d43ee64129028)},{12,840,UINT64_C(0xe72f8903dc38e7f9)}}},
{"BET1DBFS",1,2,4,{{32,1059,UINT64_C(0xf1cd5be9977204d1)},{14,886,UINT64_C(0xf0d90e68f153863f)}}},
{"BET1DBFF",1,3,4,{{24,764,UINT64_C(0x4661f8b410a3e928)},{25,1755,UINT64_C(0xbab3158c1d87dee1)}}},
{"OUT3DBFS",1,2,4,{{31,1145,UINT64_C(0x953760b61ed121b2)},{2,94,UINT64_C(0x8b61b90f58ca73b4)}}},
{"OUT3DBFF",1,3,4,{{26,866,UINT64_C(0x421b6a0fd40068fe)},{2,94,UINT64_C(0x8b61b90f58ca73b4)}}},
{"LUT4DBFS",1,2,4,{{26,813,UINT64_C(0x2a7ffe674b4201de)},{17,1218,UINT64_C(0xcab1a522dafab7c3)}}},
{"LUT4DBFF",1,3,4,{{24,820,UINT64_C(0x940b13745e603732)},{17,1218,UINT64_C(0xcab1a522dafab7c3)}}},
};
#endif
