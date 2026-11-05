/* Expected values from original loader code, not native formulas. */
#ifndef MERCS_RETURN_INIT_ORIGINAL_FIXTURES_H
#define MERCS_RETURN_INIT_ORIGINAL_FIXTURES_H
#include <stdint.h>
typedef struct { const char *name; int32_t armor, rear, internal; uint16_t input_flags, expected_flags; } mercs_return_flag_init_fixture;
static const mercs_return_flag_init_fixture mercs_return_flag_init_fixtures[] = {
  {"zero", 0, 0, 0, 0, 2048},
  {"below-five", 4, 4, 0, 255, 2048},
  {"exact-one", 5, 10, 0, 28687, 30753},
  {"division-fractions", 9, 14, 0, 0, 2081},
  {"below-clamp", 74, 74, 0, 0, 2286},
  {"exact-clamp", 75, 75, 0, 0, 2303},
  {"over-clamp", 99, 150, 0, 63487, 65535},
  {"negative-division", -6, -4, 0, 28687, 30720},
  {"negative-internal", 20, 2, -10, 291, 2306},
  {"negative-front-sum", -100, 10, 3, 0, 2080},
  {"marked-zero", 1000, -1000, -100, 2048, 2048},
  {"marked-bits", 35, 7, 10, 63829, 63829},
  {"replace-low-byte", 40, 8, 10, 61731, 63802},
  {"PLY00STD-location-0", 6, 0, 3, 256, 2305},
  {"PLY00STD-location-1", 6, 3, 6, 256, 2322},
  {"PLY00STD-location-2", 8, 4, 8, 256, 2339},
  {"PLY00STD-location-3", 6, 3, 6, 256, 2322},
  {"PLY00STD-location-4", 6, 0, 4, 256, 2306},
  {"PLY00STD-location-5", 6, 0, 4, 256, 2306},
  {"PLY00STD-location-6", 8, 0, 6, 256, 2322},
  {"PLY00STD-location-7", 8, 0, 6, 256, 2322},
  {"JEN00STD-location-0", 7, 0, 3, 256, 2306},
  {"JEN00STD-location-1", 8, 4, 8, 256, 2339},
  {"JEN00STD-location-2", 10, 3, 11, 256, 2340},
  {"JEN00STD-location-3", 8, 4, 8, 256, 2339},
  {"JEN00STD-location-4", 4, 0, 6, 256, 2322},
  {"JEN00STD-location-5", 4, 0, 6, 256, 2322},
  {"JEN00STD-location-6", 6, 0, 8, 256, 2322},
  {"JEN00STD-location-7", 6, 0, 8, 256, 2322},
  {"PNT00STD-location-0", 9, 0, 3, 0, 2050},
  {"PNT00STD-location-1", 10, 5, 8, 0, 2083},
  {"PNT00STD-location-2", 14, 7, 11, 0, 2101},
  {"PNT00STD-location-3", 10, 5, 8, 0, 2083},
  {"PNT00STD-location-4", 10, 0, 6, 0, 2067},
  {"PNT00STD-location-5", 10, 0, 6, 0, 2067},
  {"PNT00STD-location-6", 12, 0, 8, 0, 2068},
  {"PNT00STD-location-7", 12, 0, 8, 0, 2068},
  {"KDK00STD-location-0", 9, 0, 3, 256, 2306},
  {"KDK00STD-location-1", 32, 10, 21, 256, 2410},
  {"KDK00STD-location-2", 40, 15, 31, 256, 2462},
  {"KDK00STD-location-3", 32, 10, 21, 256, 2410},
  {"KDK00STD-location-4", 32, 0, 17, 256, 2361},
  {"KDK00STD-location-5", 32, 0, 17, 256, 2361},
  {"KDK00STD-location-6", 34, 0, 21, 256, 2379},
  {"KDK00STD-location-7", 34, 0, 21, 256, 2379},
};
#define MERCS_RETURN_FLAG_INIT_FIXTURE_COUNT (sizeof mercs_return_flag_init_fixtures / sizeof mercs_return_flag_init_fixtures[0])
typedef struct { const char *name; int32_t walk_mp, expected_raw; } mercs_return_walk_init_fixture;
static const mercs_return_walk_init_fixture mercs_return_walk_init_fixtures[] = {
  {"walk--5", -5, -527},
  {"walk--1", -1, -105},
  {"walk-0", 0, 0},
  {"walk-1", 1, 105},
  {"walk-2", 2, 211},
  {"walk-3", 3, 316},
  {"walk-4", 4, 422},
  {"walk-5", 5, 527},
  {"walk-6", 6, 633},
  {"walk-7", 7, 738},
  {"walk-8", 8, 844},
  {"walk-10", 10, 1055},
  {"walk-12", 12, 1266},
  {"PLY00STD", 6, 633},
  {"JEN00STD", 7, 738},
  {"PNT00STD", 4, 422},
  {"KDK00STD", 4, 422},
};
#define MERCS_RETURN_WALK_INIT_FIXTURE_COUNT (sizeof mercs_return_walk_init_fixtures / sizeof mercs_return_walk_init_fixtures[0])
typedef struct { const char *name; int32_t walk_mp, expected_raw; uint16_t expected_flags[8]; uint8_t expected_config[50]; } mercs_return_stock_init_fixture;
static const mercs_return_stock_init_fixture mercs_return_stock_init_fixtures[] = {
  {"PLY00STD", 6, 633, {2305, 2322, 2339, 2322, 2306, 2306, 2322, 2322}, {67, 79, 77, 45, 55, 88, 32, 67, 111, 109, 109, 97, 110, 100, 111, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
  {"JEN00STD", 7, 738, {2306, 2339, 2340, 2339, 2322, 2322, 2322, 2322}, {74, 82, 55, 45, 68, 32, 74, 101, 110, 110, 101, 114, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
  {"PNT00STD", 4, 422, {2050, 2083, 2101, 2083, 2067, 2067, 2068, 2068}, {80, 78, 84, 45, 57, 82, 32, 80, 97, 110, 116, 104, 101, 114, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 131, 255, 255, 117, 1, 0, 0, 0, 0, 0, 133, 255, 126, 41, 83, 255, 19, 138, 107, 16}},
  {"KDK00STD", 4, 422, {2306, 2410, 2462, 2410, 2361, 2361, 2379, 2379}, {75, 111, 100, 105, 97, 107, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 184, 2, 1, 0, 0, 0, 222, 255, 255, 255, 194, 8, 0, 184, 3, 0, 0, 0}},
};
#define MERCS_RETURN_STOCK_INIT_FIXTURE_COUNT (sizeof mercs_return_stock_init_fixtures / sizeof mercs_return_stock_init_fixtures[0])
#endif
