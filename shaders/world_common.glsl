// world_common.glsl - the world renderer's (src/glr.c) resource layout, shared by world.vert and world.frag.
//
// SDL_GPU's binding model (SDL_gpu.h, SDL_CreateGPUShader): SPIR-V vertex set 0 = storage buffers (no textures), set 1 =
// uniform buffers; fragment set 2 = the sampled textures (combined image samplers) then the storage buffer, set 3 = the
// uniform buffers. scripts/gen_shaders.py moves the storage buffers after the uniform buffers for Metal ([[buffer]]
// order) and reads the blocks' member offsets back (spirv-cross --reflect) to write the C structs in src/rb_shaders.h.
//
// The blocks are pushed to both stages with the same data: block 0 (WorldFrame) changes a few times a frame (the
// camera, lights, the DOS projection), block 1 (WorldDraw) per batch. Every member keeps the name the OpenGL program's
// uniform had; arrays of vec3 / int became vec4 / ivec4 (std140 strides).

layout(std140, set = UBO_SET, binding = 0) uniform WorldFrame {
    mat4  u_mvp;
    mat4  u_view;
    vec4  u_tifog;
    vec4  u_hz_c;
    vec3  u_dos_eye;   int   u_dos;
    vec3  u_dos_rx;    float u_far;
    vec3  u_dos_ry;    int   u_point;
    vec3  u_dos_rz;    int   u_tidn;
    ivec2 u_dos_ps;    vec2  u_vp;
    vec2  u_vpo;       float u_fbh;      // u_fbh: the target's height (GL's bottom-left gl_FragCoord from the top-left one)
    int   u_dos_smooth;
    vec3  u_lpos;      int   u_dos_dir;
    vec3  u_tiamb;     float u_ambient;
    vec3  u_tignd;     float u_fog_density;
    vec3  u_light;     float u_pfade;
    vec3  u_fog_color; float u_dos_amb;
    vec3  u_dos_lpos;  float u_dos_fogdist;
    vec3  u_hz_r;      int   u_notex;
    vec3  u_hz_u;      int   u_dsign;
    vec3  u_hz_f;      float u_hz_band;
    vec2  u_hz_x;      ivec2 u_tilt;
    int   u_hz_smooth; int   u_ati;
    vec4  u_tilv[2];   // xyz (was vec3[2])
    vec4  u_tilc[2];
    vec4  u_tidp[8];
    vec4  u_tidc[8];   // xyz (was vec3[8])
    vec4  u_depth_remap;
    vec2  u_pit_z;     vec2  u_tifog_height;
    vec2  u_map_range; int   u_lod_q16;  float u_lod_float;
    vec3  u_hz_indices; int  u_dos_ortho;
    int   u_shared_depth; int u_satellite; int u_dos_texture_linear; int u_dos_light;
    float u_dos_intensity;
    ivec4 u_pit_rank4[128];   // u_pit_rank[512] (GLR_PIT_MAX), four to an ivec4
};

layout(std140, set = UBO_SET, binding = 1) uniform WorldDraw {
    vec4  u_wire;
    vec4  u_mono;
    vec3  u_tint;      int   u_first;    // u_first: the draw's first vertex in the layer (gl_VertexID = u_first + gl_VertexIndex)
    vec2  u_texsize;   int   u_textured; int u_unlit;
    int   u_shadow;    float u_alpha;    int u_blend;  int u_tinton;
    int   u_mga_lod;   int   u_horizon;  int u_tilit; int u_sprite_indexed;
};

#define u_pit_rank(i) u_pit_rank4[(i) >> 2][(i) & 3]


// Packed Mercs vertex data: 13 legacy mesh floats, sphere4, Titanium colour4,
// cockpit id1, WTBO distance thresholds2, satellite material1. Reading one
// storage buffer lets the accelerated vertex stage reproduce the former
// near-clipping geometry stage on D3D12, Vulkan and Metal.
#if defined(WORLD_VERTEX) || DOS_LOOK
layout(std430, set = SSBO_SET, binding = SSBO_BINDING) readonly buffer WorldVertices { float world_vtx[]; };
float dos_float(int k) { return world_vtx[(k / 13) * 25 + (k % 13)]; }
#endif

#if DOS_LOOK
int dos_idiv(int n, int d) { int q = abs(n) / abs(d); return (n < 0) != (d < 0) ? -q : q; }
float dos_rnd(float v) { return floor(v + 0.5); }
int dos_pq(int c, int s, int z) { int q; if (abs(c) < (1 << (30 - s))) q = dos_idiv(c << s, z); else q = int(trunc(float(c) * exp2(float(s)) / float(z))); return (q + 2) >> 2; }
float dos_camz(vec3 w) { vec3 d = vec3(dos_rnd(w.x) - dos_rnd(u_dos_eye.x), dos_rnd(w.y) - dos_rnd(u_dos_eye.y), dos_rnd(-w.z) - dos_rnd(-u_dos_eye.z)); return dos_rnd(dot(u_dos_rz, d)); }
ivec2 dos_scr(vec3 w) {
  vec3 d = vec3(dos_rnd(w.x) - dos_rnd(u_dos_eye.x), dos_rnd(w.y) - dos_rnd(u_dos_eye.y), dos_rnd(-w.z) - dos_rnd(-u_dos_eye.z));
  int xc = int(dos_rnd(dot(u_dos_rx, d))), yc = int(dos_rnd(dot(u_dos_ry, d))), zc = max(int(dos_rnd(dot(u_dos_rz, d))), 1);
  if (u_dos_ortho > 0) zc = u_dos_ortho;
  ivec2 c = ivec2(u_vp) / 2;
  return ivec2(c.x + dos_pq(xc, u_dos_ps.x, zc), c.y - dos_pq(yc, u_dos_ps.y, zc));
}
#endif

// Interpolation is unchanged from the Mercs shader. Fog is carried as fog*w
// and divided by interpolated w in the fragment stage; it is not perspective fog.
#define V_LOCATIONS \
    VLOC(0) VARY vec3 v_normal; \
    VLOC(1) VARY vec2 v_uv; \
    VLOC(2) VARY vec3 v_color; \
    VLOC(3) VARY vec3 v_viewpos; \
    VLOC(4) VARY vec3 v_world; \
    VLOC(5) VARY vec3 v_bary; \
    VLOC(6) VARY vec3 v_tic; \
    VLOC(7) flat VARY ivec4 v_i0; \
    VLOC(8) flat VARY ivec4 v_i1; \
    VLOC(9) flat VARY vec4 v_sph; \
    VLOC(10) flat VARY float v_dkey; \
    VLOC(11) noperspective VARY vec4 v_np; \
    VLOC(12) VARY vec2 v_fogmap;
#define VLOC(n) layout(location = n)
#define v_texsel v_i0.x
#define v_emask v_i0.y
#define v_bb v_i0.z
#define v_tif v_i0.w
#define v_dnib v_i1.x
#define v_ticm v_i1.y
#define v_pit v_i1.z
#define v_mapmat v_i1.w
#define v_gs v_np.xy
#define v_uvl v_np.zw
#define v_tivf v_fogmap.x
#define v_mapshade v_fogmap.y
