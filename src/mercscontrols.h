/* Mercenaries Cockpit Controls: DOS shell 0x2c5b8..0x30000.
 * CPC records retain the original 37 slots/page, although only 33 are shown.
 * Unlike Mech2, saved device numbers must be resolved through their names. */
#ifndef MERCSCONTROLS_H
#define MERCSCONTROLS_H
#include <stddef.h>
#include <stdint.h>

#define MC_LEVELS 4
#define MC_SLOTS 37
#define MC_CONTROLS 33
#define MC_DEVICES 16
#define MC_CPC_BYTES 3872
typedef struct { int32_t type, device; uint32_t flags; int32_t pad, a, b; } mercs_control_binding;
typedef struct {
    char name[64];
    int32_t dev_index[MC_DEVICES];
    char dev_name[MC_DEVICES][12];
    mercs_control_binding b[MC_LEVELS][MC_SLOTS];
} mercs_control_config;
typedef struct { const char *map, *label; } mercs_control_input;
typedef struct {
    const char *name, *title;
    int naxis, nbutton;
    mercs_control_input axis[8], button[128];
} mercs_control_device;
typedef struct { int analog, discrete; } mercs_control_counts;
extern const char *const mercs_control_labels[MC_CONTROLS];
extern const char *const mercs_control_names[MC_CONTROLS];
const mercs_control_device *mercs_control_device_at(int index);
void mercs_controls_clear(mercs_control_config *c);
int mercs_controls_decode(mercs_control_config *c, const uint8_t *data, size_t size);
void mercs_controls_encode(const mercs_control_config *c, uint8_t data[MC_CPC_BYTES]);
int mercs_controls_remap(mercs_control_config *c);
void mercs_controls_selected(const mercs_control_config *c, int selected[MC_DEVICES]);
int mercs_controls_load(mercs_control_config *c, const char *name);
int mercs_controls_defaults(mercs_control_config *c, const int selected[MC_DEVICES]);
size_t mercs_controls_generate(const mercs_control_config *c, char *out, size_t size,
                              mercs_control_counts *counts);
int mercs_controls_save_slot(const mercs_control_config *c, const char *overlay,
                             int slot, char *error, size_t size);
int mercs_controls_apply(const mercs_control_config *c, const char *overlay,
                         char *error, size_t size);
const char *mercs_control_input_name(const mercs_control_binding *b, int half);
void mercs_control_cycle_modifier(mercs_control_binding *b);
#endif
