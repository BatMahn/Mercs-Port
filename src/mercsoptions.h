/* Original Mercenaries MW2SND.CFG (84 bytes) / MW2DIF.CFG (23 bytes).
 * The larger records, especially the driver at +0x38, are not the MW2 layout.
 * Unknown bytes and any trailing data are retained when a control is edited. */
#ifndef MERCS_OPTIONS_H
#define MERCS_OPTIONS_H
#include <stddef.h>
#include <stdint.h>

enum { MERCS_SND_BYTES = 84, MERCS_DIF_BYTES = 23, MERCS_OPTIONS_MAX = 4096 };
enum {
    MO_EFFECTS = 0x04, MO_VOICE = 0x08, MO_MUSIC = 0x0c,
    MO_OBJECT_TEXTURES = 0x14, MO_TERRAIN_TEXTURES = 0x18,
    MO_DISPLAY_DETAIL = 0x1c, MO_OBJECT_DENSITY = 0x20,
    MO_CHUNKY_EXPLOSIONS = 0x24, MO_PARTICLES = 0x28,
    MO_MULTIPLE_LIGHTS = 0x2c, MO_SHADOWS = 0x30,
    MO_DRIVER = 0x38, MO_TRANSITIONS = 0x48, MO_SUBTITLES = 0x4c
};
typedef struct {
    uint8_t snd[MERCS_OPTIONS_MAX], dif[MERCS_OPTIONS_MAX];
    size_t snd_size, dif_size;
} mercs_options;
void mercs_options_defaults(mercs_options *options);
/* Explicit overlay first, then the original CD's MERCS/ directory (or its
 * root). No writes. Missing records use the original defaults. Oversized or
 * unreadable records fail rather than losing their unrecognized bytes. */
int mercs_options_load(mercs_options *options, const char *overlay, const char *cd,
                       char *error, size_t error_size);
/* Write only the selected real writable overlay. Each replacement is atomic;
 * errors are reported to the caller and the previous file is retained. */
int mercs_options_save(const mercs_options *options, const char *overlay,
                       char *error, size_t error_size);
int mercs_options_save_sound(const mercs_options *options, const char *overlay,
                             char *error, size_t error_size);
uint32_t mercs_options_get(const mercs_options *options, int offset);
void mercs_options_set(mercs_options *options, int offset, uint32_t value);
int mercs_options_volume(const mercs_options *options, int offset);
/* Original resolution driver: 0 VGA, 1 VESA480, 2 VESA768. Native rendering is
 * an existing port preference in mercsport.cfg and is never written as a DLL. */
int mercs_options_resolution(const mercs_options *options);
void mercs_options_set_resolution(mercs_options *options, int resolution);
int mercs_options_transition(const mercs_options *options, const char *movie);
#endif
