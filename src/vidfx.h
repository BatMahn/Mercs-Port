/* Optional CPU movie presentation filters adapted from the supplied GBL port.
 * These are port presentation choices, not reconstructed Mercenaries rendering
 * rules. Input is palette-indexed; output is RGB24 at the requested pixel size.
 * ORIGINAL uses solid source-pixel blocks; SMOOTH uses Mitchell-Netravali taps;
 * SCANLINES uses narrow source-line beams; CRT adds bloom and an aperture grille;
 * CRT_CURVED adds the GBL filter's two-pass curved-screen approximation.
 */
#ifndef MERCS_VIDFX_H
#define MERCS_VIDFX_H
#include <stdint.h>
enum { VIDFX_ORIGINAL, VIDFX_SMOOTH, VIDFX_SCANLINES, VIDFX_CRT, VIDFX_CRT_CURVED, VIDFX_COUNT };
typedef struct vidfx vidfx;
vidfx *vidfx_new(void);
void vidfx_free(vidfx *v);
/* Return 0 on success, -1 for invalid dimensions/pitch or allocation failure.
 * Output must be at least the source size; the presenter can downscale later.
 * On error the output is unspecified and must not be presented as a good frame.
 * Source dimensions are bounded to 4096; output dimensions to 8192.
 */
int vidfx_render(vidfx *v, int mode, const uint8_t *pix, int sw, int sh,
                 const uint8_t (*pal)[3], uint8_t *out, int ow, int oh, int pitch);
#endif
