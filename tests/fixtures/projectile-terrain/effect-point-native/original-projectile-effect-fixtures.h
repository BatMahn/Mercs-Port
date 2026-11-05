/* Generated from captured original outputs by make_fixtures.py.
 * No native value produces an expected coordinate.
 * Profiles: 0 DOS, 1 retail Windows, 2 dedicated3dfx, 3 Titanium.
 * Segment rows supply intersection C; consumer rows supply C and E.
 * The sound leaf is a submitted vector, not original audio output. */
#ifndef ORIGINAL_PROJECTILE_EFFECT_FIXTURES_H
#define ORIGINAL_PROJECTILE_EFFECT_FIXTURES_H
typedef struct { const char *name; int profile; float from[3], to[3], contact[3], backoff[3]; } original_backoff_row;
static const original_backoff_row original_backoff_rows[] = {
  {"dos/captured-short-diagonal", 0, {0.0f, 1000.0f, 0.0f}, {100.0f, 0.0f, 200.0f}, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}},
  {"dos/reverse-short-diagonal", 0, {0.0f, 1000.0f, 0.0f}, {-100.0f, 2000.0f, -200.0f}, {-50.0f, 1500.0f, -100.0f}, {-41.0f, 1407.0f, -81.0f}},
  {"dos/approximate-length-3000-includes-short", 0, {0.0f, 1000.0f, 0.0f}, {2000.0f, 3000.0f, 2000.0f}, {1000.0f, 2000.0f, 1000.0f}, {933.0f, 1933.0f, 933.0f}},
  {"dos/approximate-length-3001-selects-long", 0, {0.0f, 1000.0f, 0.0f}, {2001.0f, 3000.0f, 2000.0f}, {1000.0f, 2000.0f, 1000.0f}, {942.0f, 1942.0f, 942.0f}},
  {"dos/positive-half-rounding", 0, {0.0f, 1000.0f, 0.0f}, {1.0f, 1008.0f, 0.0f}, {0.0f, 1004.0f, 0.0f}, {-13.0f, 904.0f, 0.0f}},
  {"dos/negative-half-rounding", 0, {0.0f, 1000.0f, 0.0f}, {-1.0f, 992.0f, 0.0f}, {0.0f, 996.0f, 0.0f}, {12.0f, 1096.0f, 0.0f}},
  {"dos/long-diagonal", 0, {0.0f, 1000.0f, 0.0f}, {12345.0f, -22456.0f, 34567.0f}, {6172.0f, -10728.0f, 17283.0f}, {6144.0f, -10674.0f, 17204.0f}},
  {"dos/reverse-long-diagonal", 0, {0.0f, 1000.0f, 0.0f}, {-12345.0f, 24456.0f, -34567.0f}, {-6172.0f, 12728.0f, -17283.0f}, {-6144.0f, 12674.0f, -17204.0f}},
  {"dos/zero-ray-helper-only", 0, {0.0f, 1000.0f, 0.0f}, {0.0f, 1000.0f, 0.0f}, {0.0f, 1000.0f, 0.0f}, {0.0f, 1000.0f, 0.0f}},
  {"dedicated/captured-short-diagonal", 2, {0.0f, 1000.0f, 0.0f}, {100.0f, 0.0f, 200.0f}, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}},
  {"dedicated/reverse-short-diagonal", 2, {0.0f, 1000.0f, 0.0f}, {-100.0f, 2000.0f, -200.0f}, {-50.0f, 1500.0f, -100.0f}, {-41.0f, 1407.0f, -81.0f}},
  {"dedicated/approximate-length-3000-includes-short", 2, {0.0f, 1000.0f, 0.0f}, {2000.0f, 3000.0f, 2000.0f}, {1000.0f, 2000.0f, 1000.0f}, {933.0f, 1933.0f, 933.0f}},
  {"dedicated/approximate-length-3001-selects-long", 2, {0.0f, 1000.0f, 0.0f}, {2001.0f, 3000.0f, 2000.0f}, {1000.0f, 2000.0f, 1000.0f}, {942.0f, 1942.0f, 942.0f}},
  {"dedicated/positive-half-rounding", 2, {0.0f, 1000.0f, 0.0f}, {1.0f, 1008.0f, 0.0f}, {0.0f, 1004.0f, 0.0f}, {-13.0f, 904.0f, 0.0f}},
  {"dedicated/negative-half-rounding", 2, {0.0f, 1000.0f, 0.0f}, {-1.0f, 992.0f, 0.0f}, {0.0f, 996.0f, 0.0f}, {12.0f, 1096.0f, 0.0f}},
  {"dedicated/long-diagonal", 2, {0.0f, 1000.0f, 0.0f}, {12345.0f, -22456.0f, 34567.0f}, {6172.0f, -10728.0f, 17283.0f}, {6144.0f, -10674.0f, 17204.0f}},
  {"dedicated/reverse-long-diagonal", 2, {0.0f, 1000.0f, 0.0f}, {-12345.0f, 24456.0f, -34567.0f}, {-6172.0f, 12728.0f, -17283.0f}, {-6144.0f, 12674.0f, -17204.0f}},
  {"dedicated/zero-ray-helper-only", 2, {0.0f, 1000.0f, 0.0f}, {0.0f, 1000.0f, 0.0f}, {0.0f, 1000.0f, 0.0f}, {0.0f, 1000.0f, 0.0f}},
  {"titanium/reused-declared-prior-segment", 3, {0.0f, 1000.0f, 0.0f}, {100.0f, 0.0f, 200.0f}, {50.0f, 500.0f, 100.0f}, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}},
};
typedef struct { const char *name; int profile, visual, flags, allowed, refused;
  float contact[3], backoff[3]; int active, type, remaining; float presentation[3];
  int light_active; float light[3]; int sound_count, sound_id, sound_half; float sound_delta[3];
  int area_executed; float area_center[3]; } original_effect_row;
static const original_effect_row original_effect_rows[] = {
  {"dos/laser-ground", 0, 1, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 9, 169, {50.0f, 500.0f, 100.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 260, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"dos/bullet-ground", 0, 5, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 9, 169, {50.0f, 500.0f, 100.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 260, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"dos/bullet-unit", 0, 5, 256, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 8, 97, {41.0f, 500.0f, 81.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 269, 1, {959.0f, 1500.0f, 2919.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"dos/missile-ground", 0, 3, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 12, 406, {50.0f, 500.0f, 100.0f}, 1, {41.0f, 593.0f, 81.0f}, 1, 227, 1, {950.0f, 1500.0f, 2900.0f}, 1, {50.0f, 500.0f, 100.0f}},
  {"dos/ppc-ground", 0, 6, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 21, 588, {50.0f, 500.0f, 100.0f}, 1, {41.0f, 593.0f, 81.0f}, 1, 256, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"dos/missile-light-disallowed", 0, 3, 1024, 0, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 12, 406, {50.0f, 500.0f, 100.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 227, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"dos/missile-allocation-refused", 0, 3, 1024, 1, 1, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 0, -1, 0, {0.0f, 0.0f, 0.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 0, 0, 0, {0.0f, 0.0f, 0.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"dos/fallback-equal-points", 0, 3, 1024, 1, 0, {60.0f, 0.0f, 120.0f}, {60.0f, 0.0f, 120.0f}, 1, 12, 406, {60.0f, 0.0f, 120.0f}, 1, {60.0f, 0.0f, 120.0f}, 1, 227, 1, {940.0f, 2000.0f, 2880.0f}, 1, {60.0f, 0.0f, 120.0f}},
  {"retail-windows/laser-ground", 1, 1, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 9, 168, {50.0f, 500.0f, 100.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 260, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"retail-windows/bullet-ground", 1, 5, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 9, 168, {50.0f, 500.0f, 100.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 260, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"retail-windows/bullet-unit", 1, 5, 256, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 8, 96, {41.0f, 500.0f, 81.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 269, 1, {959.0f, 1500.0f, 2919.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"retail-windows/missile-ground", 1, 3, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 12, 404, {50.0f, 500.0f, 100.0f}, 1, {41.0f, 593.0f, 81.0f}, 1, 227, 1, {950.0f, 1500.0f, 2900.0f}, 1, {50.0f, 500.0f, 100.0f}},
  {"retail-windows/ppc-ground", 1, 6, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 21, 585, {50.0f, 500.0f, 100.0f}, 1, {41.0f, 593.0f, 81.0f}, 1, 256, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"retail-windows/missile-light-disallowed", 1, 3, 1024, 0, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 12, 404, {50.0f, 500.0f, 100.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 227, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"retail-windows/missile-allocation-refused", 1, 3, 1024, 1, 1, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 0, -1, 0, {0.0f, 0.0f, 0.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 0, 0, 0, {0.0f, 0.0f, 0.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"retail-windows/fallback-equal-points", 1, 3, 1024, 1, 0, {60.0f, 0.0f, 120.0f}, {60.0f, 0.0f, 120.0f}, 1, 12, 404, {60.0f, 0.0f, 120.0f}, 1, {60.0f, 0.0f, 120.0f}, 1, 227, 1, {940.0f, 2000.0f, 2880.0f}, 1, {60.0f, 0.0f, 120.0f}},
  {"dedicated/bullet-ground-control", 2, 5, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 9, 168, {50.0f, 500.0f, 100.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 260, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"dedicated/bullet-unit-xz-discriminator", 2, 5, 256, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 8, 96, {41.0f, 500.0f, 81.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 269, 1, {959.0f, 1500.0f, 2919.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"dedicated/missile-light-and-area-discriminator", 2, 3, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 12, 404, {50.0f, 500.0f, 100.0f}, 1, {41.0f, 593.0f, 81.0f}, 1, 227, 1, {950.0f, 1500.0f, 2900.0f}, 1, {50.0f, 500.0f, 100.0f}},
  {"dedicated/missile-light-prohibited-control", 2, 3, 1024, 0, 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, 1, 12, 404, {50.0f, 500.0f, 100.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 227, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"titanium/bullet-ground-control", 3, 5, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}, 1, 9, 168, {50.0f, 500.0f, 100.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 260, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
  {"titanium/bullet-unit-xz-discriminator", 3, 5, 256, 1, 0, {50.0f, 500.0f, 100.0f}, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}, 1, 8, 96, {40.241001129150391f, 500.0f, 80.482002258300781f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 269, 1, {959.75897216796875f, 1500.0f, 2919.51806640625f}, 0, {0.0f, 0.0f, 0.0f}},
  {"titanium/missile-light-and-area-discriminator", 3, 3, 1024, 1, 0, {50.0f, 500.0f, 100.0f}, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}, 1, 12, 404, {50.0f, 500.0f, 100.0f}, 1, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}, 1, 227, 1, {950.0f, 1500.0f, 2900.0f}, 1, {50.0f, 500.0f, 100.0f}},
  {"titanium/missile-light-prohibited-control", 3, 3, 1024, 0, 0, {50.0f, 500.0f, 100.0f}, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}, 1, 12, 404, {50.0f, 500.0f, 100.0f}, 0, {4444.0f, 5555.0f, 6666.0f}, 1, 227, 1, {950.0f, 1500.0f, 2900.0f}, 0, {0.0f, 0.0f, 0.0f}},
};
typedef struct { const char *name; int profile; float contact1[3], backoff1[3], contact2[3], backoff2[3];
  int replaced, final_type; float final_light[3]; } original_light_row;
static const original_light_row original_light_rows[] = {
  {"dos/new-contact-nearer-than-old-backoff", 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, {40.0f, 500.0f, 90.0f}, {-10.0f, 700.0f, 0.0f}, 1, 21, {-10.0f, 700.0f, 0.0f}},
  {"dos/new-contact-farther-despite-new-backoff-nearer", 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, {0.0f, 500.0f, 80.0f}, {100.0f, 700.0f, 150.0f}, 0, 12, {41.0f, 593.0f, 81.0f}},
  {"dos/equal-xz-distance-refuses-replacement", 0, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, {41.0f, 500.0f, 81.0f}, {1000.0f, 900.0f, 3000.0f}, 0, 12, {41.0f, 593.0f, 81.0f}},
  {"retail-windows/new-contact-nearer-than-old-backoff", 1, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, {40.0f, 500.0f, 90.0f}, {-10.0f, 700.0f, 0.0f}, 1, 21, {-10.0f, 700.0f, 0.0f}},
  {"retail-windows/new-contact-farther-despite-new-backoff-nearer", 1, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, {0.0f, 500.0f, 80.0f}, {100.0f, 700.0f, 150.0f}, 0, 12, {41.0f, 593.0f, 81.0f}},
  {"retail-windows/equal-xz-distance-refuses-replacement", 1, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, {41.0f, 500.0f, 81.0f}, {1000.0f, 900.0f, 3000.0f}, 0, 12, {41.0f, 593.0f, 81.0f}},
  {"dedicated/new-contact-nearer-than-old-backoff", 2, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, {40.0f, 500.0f, 90.0f}, {-10.0f, 700.0f, 0.0f}, 1, 21, {-10.0f, 700.0f, 0.0f}},
  {"dedicated/new-contact-farther-despite-new-backoff-nearer", 2, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, {0.0f, 500.0f, 80.0f}, {100.0f, 700.0f, 150.0f}, 0, 12, {41.0f, 593.0f, 81.0f}},
  {"dedicated/equal-xz-distance-refuses-replacement", 2, {50.0f, 500.0f, 100.0f}, {41.0f, 593.0f, 81.0f}, {41.0f, 500.0f, 81.0f}, {1000.0f, 900.0f, 3000.0f}, 0, 12, {41.0f, 593.0f, 81.0f}},
  {"titanium/new-contact-nearer-than-old-backoff", 3, {50.0f, 500.0f, 100.0f}, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}, {40.0f, 500.0f, 90.0f}, {-10.0f, 700.0f, 0.0f}, 1, 21, {-10.0f, 700.0f, 0.0f}},
  {"titanium/new-contact-farther-despite-new-backoff-nearer", 3, {50.0f, 500.0f, 100.0f}, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}, {0.0f, 500.0f, 80.0f}, {100.0f, 700.0f, 150.0f}, 0, 12, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}},
  {"titanium/equal-xz-distance-refuses-replacement", 3, {50.0f, 500.0f, 100.0f}, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}, {40.241001129150391f, 500.0f, 80.482002258300781f}, {1000.0f, 900.0f, 3000.0f}, 0, 12, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}},
  {"titanium/nearer-fractional-contact-in-same-integer-bucket", 3, {50.0f, 500.0f, 100.0f}, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}, {40.241001129150391f, 500.0f, 80.607002258300781f}, {-10.0f, 700.0f, 0.0f}, 1, 21, {-10.0f, 700.0f, 0.0f}},
  {"titanium/farther-fractional-contact-in-same-integer-bucket", 3, {50.0f, 500.0f, 100.0f}, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}, {40.241001129150391f, 500.0f, 80.357002258300781f}, {100.0f, 700.0f, 150.0f}, 0, 12, {40.241001129150391f, 597.59002685546875f, 80.482002258300781f}},
};
#endif
