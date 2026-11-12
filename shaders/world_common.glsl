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
    int   u_dos_fogpal;   // the LUMA table is a fog table (its [0][0] not 0, Mercs 0x49d06 -> 0xb3f6c)
    int   u_dos_mlit_n;   // the mission's MLIT point lights (0 with Multiple Lights off): xyz GL position, w the box half size
    int   u_dos_near;     // the DOS world polygons' near plane, camera z in quarter cm: the view's +0x3c x 4 (0x504c1 -> 0x49fbc)
    vec4  u_dos_mlit[4];
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
/* MW2.EXE 0x4f6c8: a vertex's light level from its normal tn (glr's DOS vertex normal, the stored sign) - the ambient level,
 * the directional term x intensity / 128 when positive, less the fog by its depth (0x4f7e4), then each point light whose
 * box holds it while under 0x80 (capped there, 0x4f98e) - and (C >> 1) x light / 0x440, 0..15 (0x4fa07) */
/* Mercenaries' Gouraud corner (0x495d0 .. 0x49610, colour type 0x4000): a corner whose colour is a ramp shade (index 48 or
 * more) becomes the polygon's ramp + its level, or + 15 - its level with a fog table (the fog palettes' ramps run from
 * the colour to the fog); a lower index is kept */
/* a damaged mech location (its type word's 0x100 objects' nibble n, 1-15: 0x4689b): the level x (16 - n) / 16, rounded
 * (0x46645), or with PLNT +52 set + n / 15 of the way to 15 (0x466a4) - the polygon's and each corner's */
float dos_dnib(float lev, int n) {
  if (n <= 0) return lev;
  int l = int(lev);
  if (u_dsign == 0) return float((((16 - n) * 4096 * l) + 32768) >> 16 & 15);
  int t = ((15 - l) << 16) / 15;
  return float(l + ((n * t + 32768) >> 16));
}
float dos_gouraud_level(float lev) { return u_dos_fogpal != 0 ? 15.0 - lev : lev; }
/* 64-bit helpers for dos_cos (no int64 in every target): (hi, lo) two's complement */
uvec2 dos_mul64(int a, int b) {
  uint ua = uint(abs(a)), ub = uint(abs(b));
  uint a0 = ua & 0xffffu, a1 = ua >> 16, b0 = ub & 0xffffu, b1 = ub >> 16;
  uint p0 = a0 * b0, p1 = a0 * b1, p2 = a1 * b0, p3 = a1 * b1;
  uint mid = (p0 >> 16) + (p1 & 0xffffu) + (p2 & 0xffffu);
  uint lo = (p0 & 0xffffu) | (mid << 16), hi = p3 + (p1 >> 16) + (p2 >> 16) + (mid >> 16);
  if ((a < 0) != (b < 0)) { lo = ~lo + 1u; hi = ~hi + (lo == 0u ? 1u : 0u); }
  return uvec2(hi, lo);
}
uvec2 dos_add64(uvec2 x, uvec2 y) { uint lo = x.y + y.y; return uvec2(x.x + y.x + (lo < x.y ? 1u : 0u), lo); }
int dos_sar64(uvec2 x, int r) {   /* the low 32 bits of x >> r, arithmetic, 0 <= r < 64 */
  if (r == 0) return int(x.y);
  if (r >= 32) return int(x.x) >> min(r - 32, 31);
  return int((x.y >> uint(r)) | (x.x << uint(32 - r)));
}
/* MW2.EXE 0x4d246: the light's cosine as the original works it out - the light vector's components scaled so the
 * largest has 8 bits, its length from a table of trunc(sqrt(i) * 1024) at (x^2 + y^2 + z^2) >> 8 (0x486d4), the dot
 * product with the Q29 normal >> 16 shifted the same way, divided (truncating). About 128 x the cosine, but not
 * exactly: one more than the port's trunc(128 cos) for many directions. L: the light minus the vertex, n: unit. */
float dos_cos(vec3 Lf, vec3 n) {
  ivec3 L = ivec3(floor(Lf + 0.5));
  ivec3 a = abs(L);
  int o = a.x | a.y | a.z;
  if (o == 0) return 127.0;
  int hb = findMSB(o), sh = hb - 7;
  ivec3 nq = ivec3(trunc(n * 536870912.0));
  uvec2 d = dos_add64(dos_add64(dos_mul64(L.x, nq.x), dos_mul64(L.y, nq.y)), dos_mul64(L.z, nq.z));
  int v; ivec3 b;
  if (sh >= 0) { v = dos_sar64(d, 16 + sh); b = a >> sh; }
  else { v = dos_sar64(d, 16) << (-sh); b = a << (-sh); }
  b &= 0xff;
  int S = b.x * b.x + b.y * b.y + b.z * b.z, idx = S >> 8;
  int t = int(floor(sqrt(float(idx)) * 1024.0)), q = idx << 20;
  if ((t + 1) * (t + 1) <= q) t++;
  if (t * t > q) t--;
  if (t <= 0) return 127.0;
  int r = abs(v) / t; r = v < 0 ? -r : r;
  return float((r << 16) >> 16);   /* (a short) */
}
float dos_vertex_level(vec3 p, vec3 tn, float C) {
  /* 0x4f79f pushes the vertex normal +0x28, +0x24, +0x20 to 0x4d246 (the polygon's 0x4d20e: +0x14, +0x18, +0x1c), so a
   * vertex sees the light's x against its normal's z and z against x: in game axes nv = (nz, ny, nx), here (GL z = -game
   * z) (-nz, ny, -nx) */
  float nl = length(tn); vec3 nv = nl > 0.0 ? -tn / nl : vec3(0.0);
  nv = vec3(-nv.z, nv.y, -nv.x);
  vec3 Lv = u_dos_dir != 0 ? u_dos_lpos : u_dos_lpos - p; float ll = length(Lv);
  float f = nl > 0.0 && ll > 0.0 ? dos_cos(Lv, nv) : 127.0;
  float light = u_dos_amb + (u_dos_light != 0 ? max(0.0, trunc(f * u_dos_intensity / 128.0)) : 0.0);
  if (u_dos_fogdist > 0.0) { int key = int(max(dos_camz(p), 0.0)); light = max(0.0, light - float((int(uint(key) << 11) / int(u_dos_fogdist)) >> 4)); }
  for (int i = 0; i < 4; i++) {
    if (i >= u_dos_mlit_n || light >= 128.0 || nl <= 0.0 || u_dos_fogpal != 0) break;   /* 0x4f84a: not with a fog table */
    vec3 d = u_dos_mlit[i].xyz - p;
    if (any(greaterThan(abs(d), vec3(u_dos_mlit[i].w)))) continue;
    float lp = length(d), fi = lp > 0.0 ? trunc(128.0 * dot(nv, d / lp)) : 128.0;
    if (fi > 0.0) light = min(128.0, light + fi);
  }
  return clamp(floor(floor(C / 2.0) * light / 1088.0), 0.0, 15.0);
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
    VLOC(12) VARY vec2 v_fogmap; \
    VLOC(13) flat VARY int v_pstart;
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
