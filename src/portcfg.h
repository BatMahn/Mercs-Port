/* The port's configuration: one per-user file,
 *   Linux   $XDG_CONFIG_HOME/mercsport/mercsport.cfg  (~/.config/mercsport/mercsport.cfg)
 *   macOS   ~/Library/Application Support/mercsport/mercsport.cfg
 * plain "key=value" lines:
 *   game=<folder>          the game folder: install/ (settings, pilots, saves), cd/ (the disc's files) or an .iso,
 *                          music/ (CD tracks), ultrasnd/ (Gravis patches), 3d/ (3D-edition models, textures, sky)
 *   install= cd= music= ultrasnd= mt32= models= textures= skygnd=     optional overrides of the folder's parts
 *                          (mt32/: the MT-32 ROMs MT32_CONTROL.ROM + MT32_PCM.ROM for the menu music)
 *   textures_kind=ati | 3dfx
 *   renderer=dos | direct3d | 3dfx   (the supplied Mercenaries editions' appearances: retail DOS,
 *                          Titanium 16-bit Windows Direct3D, or the dedicated 3Dfx Interactive Edition.
 *                          Each uses its own complete archive and sky data. The 3dfx name denotes that
 *                          supplied edition's appearance; it does not claim a native Glide backend.)
 *   textures=original     (each Mercs profile uses its own textures, sky and ground; a separate Titanium overlay
 *                          is not exposed. Older inherited MW2 renderer names are still read for compatibility.)
 *                          (the old edition=<renderer> is still read; edition=titanium = renderer=enhanced textures=titanium;
 *                          textures= with any other value is the texture archive's path, as below)
 *   texture_filter=original | nearest | linear   (original: the selected edition's filtering;
 *                          nearest: sharp pixel edges; linear: smooth bilinear textures. Archive identity
 *                          stays paired with the renderer, including its sky, ground and model records.)
 *   titanium=<folder>      the Titanium edition's 16-bit files (MW2.PRJ, SKYGND.PAR from the disc's Install/16bit);
 *                          default: the game folder's titanium/
 *   resolution=desktop | window | <w>x<h>     (desktop: native fullscreen, the default)
 *   gpu=auto | d3d12 | vulkan   (Windows: auto prefers D3D12; either choice falls back to the other.
 *                          macOS always uses Metal; Linux always uses Vulkan. Separate from renderer=.)
 *   aa=0 | 2 | 4 | 8
 *   render=native | 320x200 | 640x480 | 1024x768   (the sim's picture: the display's resolution, or one of the
 *                          original's RESOLUTION choices, drawn at that size and scaled up in blocks to 4:3)
 *   videos=original | smooth | scanlines | crt | crt_curved   (full-screen movie presentation; original by default)
 *   video_fit=4:3 | fill | stretch   (movie layout: a centred 4:3 display, aspect-preserving crop to fill, or stretch)
 *   salvage=auto | contract | dynamic   (auto, the default: Titanium's dynamic salvage - the Salvage Yard after a won
 *                          contract - with the Titanium Direct3D look, the contract's salvage list alone (DOS, 3dfx) otherwise;
 *                          contract / dynamic choose one for every look)
 *   hud_layout=wide | original   (wide, the default: on a window wider than 4:3 the HUD's left-hand instruments - radar,
 *                          altitude tape, target display - sit at the window's left edge and the right-hand ones - weapon
 *                          list, damage display / viewport, throttle bar, speed - at its right edge, each as far from
 *                          the edge as from the 4:3 area's edge; the compass, reticle, markers, bars and messages stay
 *                          centred. original: the whole HUD in the 4:3 middle, as the original drew it)
 *   dos_shading=dithered | smooth   (the DOS renderer's Gouraud shading - the shaded polygons (cockpit, mechs, buildings)
 *                          and the sky's haze band: dithered, the default, as the original drew them - each palette step a
 *                          flat band and a 1-pixel checkerboard of it and the next (MW2.EXE 0x59e77; the haze band in the
 *                          sky palette's 15 colours 0xE0-0xEE, 0x3c000); smooth: the same colours blended into an even
 *                          gradient. dos_sky=banded | smooth, its older name, is still read. The other renderers are not
 *                          affected)
 *   system_shortcuts=block | allow   (macOS: block, the default, turns the system's keyboard shortcuts - Mission
 *                          Control's Ctrl+arrows, Cmd+Tab, Spotlight - off while a mission is played with the window in
 *                          front (not in the MAIN MENU or paused); allow leaves them on. Other systems: no effect)
 * The game folder is found on first run (the command line, the current folder, next to the executable - in a macOS
 * bundle: next to the .app -, one level up, ~/Mercs-game; else the shell asks with the system's folder picker) and
 * remembered. */
#ifndef PORTCFG_H
#define PORTCFG_H
#include <stddef.h>
#define PORTCFG_PATH 1024
typedef struct {
    int mode;        /* 0 desktop fullscreen, 1 windowed, 2 exclusive fullscreen at w x h */
    int w, h;
    int aa;
    int gpu;                 /* PORTCFG_GPU_*: graphics API preference, independent of edition appearance */
    int render_w, render_h;   /* 0 = native */
    float speed;              /* game speed: 1.0 = real time (speed= in the file; default 0.85) */
    char game[PORTCFG_PATH], install[PORTCFG_PATH], cd[PORTCFG_PATH], music[PORTCFG_PATH], ultrasnd[PORTCFG_PATH], mt32[PORTCFG_PATH];
    char models[PORTCFG_PATH], textures[PORTCFG_PATH], skygnd[PORTCFG_PATH];
    char skygnd_fog[PORTCFG_PATH];   /* the PowerVR edition's skygnd.par (the fog densities): 3d/skygnd_pvr.par */
    char titanium[PORTCFG_PATH];     /* the Titanium edition's folder (MW2.PRJ + SKYGND.PAR of its 16-bit build) */
    int  textures_ati;
    int  no3d;                /* (portcfg_resolve) no dedicated 3dfx archive and the base disc is not Mercs3DFX;
                               * the Titanium Direct3D profile is checked independently */
    int  renderer;            /* PORTCFG_ED_*: how the simulation is drawn (Combat Variables RENDERER) */
    int  texset;              /* PORTCFG_TX_*: the texture set (Combat Variables TEXTURES) */
    int  texture_filter;      /* PORTCFG_FILTER_*: original edition filtering or an explicit native override */
    int  system_shortcuts;    /* 0 block (default): the system's shortcuts off while playing (macOS); 1 allow */
    int  hud_original;        /* hud_layout=: 0 wide (default), 1 original (the 4:3 layout centred) */
    int  dos_sky_smooth;      /* dos_shading= (dos_sky=): 0 dithered (default, the original), 1 smooth (the DOS renderer's
                               * Gouraud polygons and haze band blended) */
    int  videos;              /* PORTCFG_VID_*: optional full-screen movie filter; original by default */
    int  video_fit;           /* PORTCFG_FIT_*: movie layout; 4:3 by default */
    int  salvage;             /* PORTCFG_SALVAGE_*: auto by default */
} portcfg;
/* RENDITION: the Titanium edition's own renderer plugin for the Rendition Verite (plugin/zredline.dll, loaded by mercsw.dll):
 * the enhanced mix's archives beside the Titanium archive, whose textures, sky dome, ground and fog it always draws */
enum { PORTCFG_ED_ENHANCED, PORTCFG_ED_DOS, PORTCFG_ED_3DFX, PORTCFG_ED_ATI, PORTCFG_ED_S3, PORTCFG_ED_PVR, PORTCFG_ED_MGA,
       PORTCFG_ED_RENDITION, PORTCFG_ED_D3D, PORTCFG_ED_COUNT };
extern const char *const PORTCFG_ED_NAME[PORTCFG_ED_COUNT];   /* "enhanced", "dos", ... (the file's words) */
/* The three supported Mercenaries profiles. Names are configuration/CLI tokens;
 * labels are user-facing; overlays are independent writable directories. Each
 * helper returns NULL for an inherited MW2 renderer that is not a Mercs option.
 * parse returns -1 for an unknown name and accepts "d3d" as a direct3d alias. */
const char *portcfg_mercs_name(int renderer);
const char *portcfg_mercs_label(int renderer);
const char *portcfg_mercs_overlay(int renderer);
int portcfg_mercs_parse(const char *name);
enum { PORTCFG_TX_ORIGINAL, PORTCFG_TX_TITANIUM, PORTCFG_TX_COUNT };
extern const char *const PORTCFG_TX_NAME[PORTCFG_TX_COUNT];   /* "original", "titanium" */
enum { PORTCFG_FILTER_ORIGINAL, PORTCFG_FILTER_NEAREST, PORTCFG_FILTER_LINEAR, PORTCFG_FILTER_COUNT };
extern const char *const PORTCFG_FILTER_NAME[PORTCFG_FILTER_COUNT];
enum { PORTCFG_GPU_AUTO, PORTCFG_GPU_D3D12, PORTCFG_GPU_VULKAN, PORTCFG_GPU_COUNT };
extern const char *const PORTCFG_GPU_NAME[PORTCFG_GPU_COUNT];
enum { PORTCFG_VID_ORIGINAL, PORTCFG_VID_SMOOTH, PORTCFG_VID_SCANLINES, PORTCFG_VID_CRT,
       PORTCFG_VID_CRT_CURVED, PORTCFG_VID_COUNT };
extern const char *const PORTCFG_VID_NAME[PORTCFG_VID_COUNT];
enum { PORTCFG_FIT_43, PORTCFG_FIT_FILL, PORTCFG_FIT_STRETCH, PORTCFG_FIT_COUNT };
extern const char *const PORTCFG_FIT_NAME[PORTCFG_FIT_COUNT];
enum { PORTCFG_SALVAGE_AUTO, PORTCFG_SALVAGE_CONTRACT, PORTCFG_SALVAGE_DYNAMIC, PORTCFG_SALVAGE_COUNT };
extern const char *const PORTCFG_SALVAGE_NAME[PORTCFG_SALVAGE_COUNT];
/* Whether a mission played with this look offers Titanium's Salvage Yard. */
int portcfg_dynamic_salvage(const portcfg *c, int renderer);
/* An edition's files: the archive for models and data, the archive for textures and its kind ("ati", "3dfx", "pvr",
 * "dos", "mercs16"), the sky / ground file and the fog file ("" = no fog). Mercs direct3d and 3dfx both use
 * their own 1.2x palette CELs, never the base MW2 packed-3dfx format. Availability requires a valid 1.2x CEL
 * and a sky file; models and textures identify the same selected archive. Returns 0 if the edition's files are present (else the
 * outputs describe the enhanced mix, or the DOS archive without the 3D files). */
int  portcfg_edition(const portcfg *c, int ed, char *models, char *textures, char *kind, char *sky, char *fog);
/* The Titanium edition's files: its archive and its SKYGND.PAR (either may be NULL). Returns 0 if the archive is there.
 * (TEXTURES TITANIUM keeps the renderer's archives - its models and data, its textures for the names the Titanium
 * archive lacks - which portcfg_edition reports; the Titanium archive is opened beside them.) */
int  portcfg_titanium(const portcfg *c, char *prj, char *sky);
/* The three supported Mercs profiles accept only ORIGINAL (their own data).
 * Inherited MW2 renderers retain the older separate-texture compatibility rules. */
int  portcfg_texset_ok(const portcfg *c, int renderer, int texset);
/* Mercs profiles always return ORIGINAL; inherited RENDITION returns TITANIUM. */
int  portcfg_texset_for(const portcfg *c, int renderer, int texset);
/* A renderer + texture set's files (portcfg_edition, then with TITANIUM the Titanium SKYGND.PAR as the sky file and no
 * fog file - the Titanium fog comes from its own SKYGND.PAR). Returns 0 if both are available. */
int  portcfg_look(const portcfg *c, int renderer, int texset, char *models, char *textures, char *kind, char *sky, char *fog);
void portcfg_defaults(portcfg *c);
const char *portcfg_file(void);                       /* the per-user file's path */
int  portcfg_load(const char *unused, portcfg *c);    /* 0 if read; defaults otherwise (the argument is ignored) */
int  portcfg_save(const char *unused, const portcfg *c);
/* Find the game folder (if not configured) and fill in every part left empty. exe_dir: the executable's folder;
 * hint: a folder from the command line, or NULL. Returns 0 if an install folder was found. */
int  portcfg_resolve(portcfg *c, const char *exe_dir, const char *hint);
/* Is this a game folder (or a plain DOS install)? */
int  portcfg_is_game_dir(const char *dir);
/* A chosen folder: the game folder itself, or a folder holding Mercs-game/. Returns 1 with the game folder's absolute
 * path in out, 0 if neither. */
int  portcfg_game_in(const char *dir, char *out, size_t n);
/* The writable edition directory beneath the canonical game root. Only "retail",
 * "direct3d" and "3dfx" are accepted. Reject existing links/reparse points at install/ or
 * its edition directory; with create=0, missing directories are allowed and no
 * files are changed. With create=1, create and recheck both directories. */
int  portcfg_overlay_dir(const portcfg *c, const char *edition, char *out, size_t n, int create);
/* exe_dir inside a macOS bundle (".../X.app/Contents/..."): 1 with the folder holding X.app in out, else 0 */
int  portcfg_app_folder(const char *exe_dir, char *out, size_t n);
/* exe_dir is a macOS App Translocation copy (a quarantined download run in place: a random read-only path) */
int  portcfg_translocated(const char *exe_dir);
#endif
