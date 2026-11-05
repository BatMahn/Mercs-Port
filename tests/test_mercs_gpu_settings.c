/* No game assets required. Pass a new, private settings filename:
 *   ./test_mercs_gpu_settings /tmp/mercs-gpu-test/port.cfg */
#include "portcfg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void write_settings(const char *path, const char *gpu)
{
    FILE *f = fopen(path, "w");
    assert(f);
    assert(fprintf(f, "renderer=direct3d\ntexture_filter=linear\nvideos=crt_curved\n"
        "video_fit=fill\nhud_layout=original\ndos_shading=smooth\nrender=640x480\n%s", gpu) > 0);
    assert(!fclose(f));
}

static void check_other_settings(const portcfg *c)
{
    assert(c->renderer == PORTCFG_ED_D3D && c->texset == PORTCFG_TX_ORIGINAL);
    assert(c->texture_filter == PORTCFG_FILTER_LINEAR);
    assert(c->videos == PORTCFG_VID_CRT_CURVED && c->video_fit == PORTCFG_FIT_FILL);
    assert(c->hud_original && c->dos_sky_smooth);
    assert(c->render_w == 640 && c->render_h == 480);
}

int main(int argc, char **argv)
{
    static const char *const invalid[] = {"gpu=opengl\n", "gpu=metal\n", "gpu=-1\n", "gpu=3\n", "gpu=\n"};
    struct stat st;
    portcfg c, saved;
    char line[64];
    size_t i;
    int gpu;
    if (argc != 2) { fprintf(stderr, "usage: %s NEW_PRIVATE_SETTINGS_FILE\n", argv[0]); return 2; }
    if (!stat(argv[1], &st)) { fputs("Refusing to overwrite an existing settings file.\n", stderr); return 2; }
    assert(!setenv("MERCS_CONFIG", argv[1], 1));
    portcfg_defaults(&c);
    assert(c.gpu == PORTCFG_GPU_AUTO && c.renderer == PORTCFG_ED_DOS);
    write_settings(argv[1], "");
    assert(!portcfg_load(NULL, &c) && c.gpu == PORTCFG_GPU_AUTO);
    check_other_settings(&c);
    for (gpu = 0; gpu < PORTCFG_GPU_COUNT; gpu++) {
        snprintf(line, sizeof line, "gpu=%s\n", PORTCFG_GPU_NAME[gpu]);
        write_settings(argv[1], line);
        assert(!portcfg_load(NULL, &c) && c.gpu == gpu);
        check_other_settings(&c);
        assert(!portcfg_save(NULL, &c));
        assert(!portcfg_load(NULL, &saved) && saved.gpu == gpu);
        check_other_settings(&saved);
    }
    for (i = 0; i < sizeof invalid / sizeof *invalid; i++) {
        write_settings(argv[1], invalid[i]);
        assert(!portcfg_load(NULL, &c) && c.gpu == PORTCFG_GPU_AUTO);
        check_other_settings(&c);
    }
    c.gpu = -1;
    assert(!portcfg_save(NULL, &c));
    assert(!portcfg_load(NULL, &saved) && saved.gpu == PORTCFG_GPU_AUTO);
    check_other_settings(&saved);
    c.gpu = PORTCFG_GPU_COUNT;
    assert(!portcfg_save(NULL, &c));
    assert(!portcfg_load(NULL, &saved) && saved.gpu == PORTCFG_GPU_AUTO);
    check_other_settings(&saved);
    assert(!remove(argv[1]));
    puts("PASS: GPU default, all three preferences, legacy/invalid settings and save normalization; Mercs appearance, CRT movies, filtering, HUD and low resolution retained.");
    return 0;
}
