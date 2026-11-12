#version 450
#extension GL_GOOGLE_include_directive : require
// Mercs world shader, ported from src/glr.c without replacing Mercs rendering rules.
#define UBO_SET 1
#define SSBO_SET 0
#define SSBO_BINDING 0
#define WORLD_VERTEX 1
#include "world_common.glsl"
#define VARY out
V_LOCATIONS









  
 
 

 
 
  
float ti_height(float y) {
  float Y = u_tifog_height.x, M = u_tifog_height.y;
  if (Y == 0.0) return 1.0;
  if (y <= 0.0) return Y < 0.0 ? M : 1.0;
  float q = Y > 0.0 ? Y / y : -Y * y;
  return q < M ? M : min(q, 1.0);
}
float ti_vertex_fog(vec3 vp, vec3 world) {
  float z = max(-vp.z, 1.0); if (u_tifog.z > 2.5) z = floor(z);
  float a = u_tifog.y > 0.0 ? (abs(u_tifog.z) > 1.5 ? min(1.0, u_tifog.y / z) : 1.0 - min(1.0, z / u_tifog.y)) : 1.0;
  a *= (abs(u_tifog.z) > 1.5 ? ti_height(world.y) : 1.0) * u_tifog.w;
  return 1.0 - float(int(a * 255.0) & 255) / 255.0;
}


int lod_fixed(int t) { uint q = uint(u_lod_q16), lo = (uint(t) & 65535u) * (q & 65535u); return int(uint(t >> 16) * q + (uint(t) & 65535u) * (q >> 16) + (lo >> 16) + ((lo >> 15) & 1u)); }
struct VertexOut {
  vec4 clip;
  vec3 normal;
  vec2 uv;
  vec3 color;
  vec3 viewpos;
  vec3 world;
  vec3 bary;
  vec3 tic;
  float tivf;
  float mapshade;
  int texsel;
  int emask;
  int bb;
  int tif;
  int dnib;
  int pit;
  vec4 sph;
  int ticm;
  float dkey;
  int mapmat;
  vec2 gs;
  vec2 uvl;
  int pstart;   /* DOS look: the polygon's first vertex + 1 (its triangles follow), 0 none */
};
VertexOut world_vertex(int vertex_id) {
  VertexOut result;
  vec3 a_pos = vec3(world_vtx[vertex_id * 25 + 0], world_vtx[vertex_id * 25 + 1], world_vtx[vertex_id * 25 + 2]);
  vec3 a_normal = vec3(world_vtx[vertex_id * 25 + 3], world_vtx[vertex_id * 25 + 4], world_vtx[vertex_id * 25 + 5]);
  vec2 a_uv = vec2(world_vtx[vertex_id * 25 + 6], world_vtx[vertex_id * 25 + 7]);
  vec3 a_color = vec3(world_vtx[vertex_id * 25 + 8], world_vtx[vertex_id * 25 + 9], world_vtx[vertex_id * 25 + 10]);
  float a_texsel = world_vtx[vertex_id * 25 + 11];
  float a_edge = world_vtx[vertex_id * 25 + 12];
  vec4 a_sphere = vec4(world_vtx[vertex_id * 25 + 13], world_vtx[vertex_id * 25 + 14], world_vtx[vertex_id * 25 + 15], world_vtx[vertex_id * 25 + 16]);
  vec4 a_tic = vec4(world_vtx[vertex_id * 25 + 17], world_vtx[vertex_id * 25 + 18], world_vtx[vertex_id * 25 + 19], world_vtx[vertex_id * 25 + 20]);
  float a_pit = world_vtx[vertex_id * 25 + 21];
  vec2 a_lod = vec2(world_vtx[vertex_id * 25 + 22], world_vtx[vertex_id * 25 + 23]);
  float a_mapmat = world_vtx[vertex_id * 25 + 24];

  int e = int(a_edge + 0.5), pit = int(a_pit + 0.5);
  result.pit = pit;
  result.dnib = e / 2048; e -= result.dnib * 2048;
  result.bb = e >= 1024 ? 1 : 0;
  result.tif = 0;
  if (e < 1024 && e >= 512) { result.tif |= 2; e -= 512; }
  if (e < 1024 && e >= 256) { result.tif |= 1; e -= 256; }
  vec3 p = a_pos, nrm = a_normal;
  result.clip = u_mvp * vec4(p, 1.0);
  if (pit > 0) { result.clip.z = u_pit_z.x * result.clip.w + u_pit_z.y; result.tif |= 4; }
  if (e >= 1024) {
    vec4 cb = u_mvp * vec4(a_pos, 1.0), ca = u_mvp * vec4(a_normal, 1.0);
    int k = int(a_texsel + 0.5), tib = k >= 4 ? 1 : 0;
    e -= 1024; nrm = vec3(0.0); k -= 4 * tib;
    if (ca.w <= 1.0 || cb.w <= 1.0) result.clip = vec4(2.0, 2.0, 2.0, 1.0);
    else if (tib != 0) {
      vec2 h = 0.5 * u_vp, T = ca.xy / ca.w * h, F = cb.xy / cb.w * h;
      T.y = -T.y; F.y = -F.y;
      float dx = 0.5 * (F.y - T.y), dy = 0.5 * (F.x - T.x);
      vec2 c = k == 0 ? vec2(T.x - dx, T.y - dy) : k == 1 ? vec2(F.x - dx, F.y + dy) : k == 2 ? vec2(T.x + dx, F.y - dy) : vec2(F.x + dx, T.y + dy);
      result.clip = vec4(c.x / h.x, -c.y / h.y, k == 0 || k == 3 ? ca.z / ca.w : cb.z / cb.w, 1.0);
      p = k == 0 || k == 3 ? a_normal : a_pos;
    } else {
      vec2 h = 0.5 * u_vp, A = ca.xy / ca.w * h, B = cb.xy / cb.w * h;
      A.y = -A.y; B.y = -B.y;
      vec2 d = 0.5 * (B - A), c = k == 0 ? A + vec2(-d.y, d.x) : k == 1 ? A + vec2(d.y, -d.x) : k == 2 ? B + vec2(d.y, -d.x) : B + vec2(-d.y, d.x);
      result.clip = vec4(c.x / h.x, -c.y / h.y, k < 2 ? ca.z / ca.w : cb.z / cb.w, 1.0);
      p = k < 2 ? a_normal : a_pos;
    }
  }
#if DOS_LOOK
  if (u_dos != 0 && int(a_color.r + 0.5) == 3 && a_texsel > 0.5 && e < 256) {
    int s = int(a_texsel + 0.5) - 1, rel = vertex_id - s, t = rel / 3, cn = rel - 3 * t;
    vec2 Q[18]; int n = 0; bool ok = true; vec2 h = 0.5 * u_vp;
    for (int k = 0; k < 16; k++) {
      int b = s + 3 * k;
      for (int c = (k == 0 ? 0 : 1); c < 3; c++) {
        if (c == 2 && ((int(dos_float(b * 13 + 12) + 0.5) % 32 / 3) & 2) == 0) break;
        vec4 wp = vec4(dos_float((b + c) * 13), dos_float((b + c) * 13 + 1), dos_float((b + c) * 13 + 2), 1.0), q = u_mvp * wp;
        if (pit > 0) q.z = u_pit_z.x * q.w + u_pit_z.y;
        if (q.z + q.w < 0.0 || q.w <= 0.0) ok = false; else { ivec2 sp = dos_scr(wp.xyz); Q[n] = vec2(float(sp.x) + 0.5, u_vp.y - float(sp.y) - 0.5) - h; }
        n++;
      }
      if (((int(dos_float(b * 13 + 12) + 0.5) % 32 / 3) & 2) != 0) break;
    }
    int j = cn == 0 ? 0 : t + cn;
    if (ok && n >= 3 && j < n && result.clip.w > 0.0) {
      float area = 0.0;
      for (int i = 0; i < n; i++) { vec2 a = Q[i], b = Q[(i + 1) % n]; area += a.x * b.y - a.y * b.x; }
      vec2 A = Q[(j + n - 1) % n], B = Q[j], C = Q[(j + 1) % n], e1 = B - A, e2 = C - B;
      float sg = area > 0.0 ? 1.0 : -1.0;
      vec2 n1 = length(e1) > 1e-4 ? sg * vec2(e1.y, -e1.x) / length(e1) : vec2(0.0), n2 = length(e2) > 1e-4 ? sg * vec2(e2.y, -e2.x) / length(e2) : vec2(0.0);
      vec2 m = n1 + n2; float d = 1.0 + dot(n1, n2);
      vec2 off = d > 1e-3 ? 3.0 * m / d : 3.0 * m;
      if (length(off) > 24.0) off = off / length(off) * 24.0;
      result.clip.xy = (B + off) / h * result.clip.w;
    } else if (j < n && result.clip.w > 0.0 && result.clip.z + result.clip.w >= 0.0) {
      result.clip.xy = Q[j] / h * result.clip.w;
    }
  }
#endif
  if (pit > 0 && u_dos != 0) {
    int rk = u_pit_rank(min(pit - 1, 511));
    if (rk < 0) result.clip = vec4(2.0, 2.0, 2.0, 1.0);
    else result.clip.z = (-1.0 + 0.01 * (1.0 - float(rk + 1) / 1024.0)) * result.clip.w;
  }
  if (u_far > 0.0 && a_sphere.w > 0.0) {
    vec3 sc = (u_view * vec4(a_sphere.xyz, 1.0)).xyz;
    if (dot(sc, sc) > (u_far + a_sphere.w) * (u_far + a_sphere.w) || -sc.z - a_sphere.w > u_far) result.clip = vec4(2.0, 2.0, 2.0, 1.0);
  }
  if ((a_lod.x >= 0.0 || a_lod.y >= 0.0) && u_lod_q16 > 0) {
    float z = -(u_view * vec4(a_sphere.xyz, 1.0)).z; bool outlod;
    if (u_lod_float > 0.0) outlod = (a_lod.x >= 0.0 && z < a_lod.x * u_lod_float) || (a_lod.y >= 0.0 && z >= a_lod.y * u_lod_float);
    else { int d = int(floor(z + 0.5)); outlod = (a_lod.x >= 0.0 && d < lod_fixed(int(a_lod.x))) || (a_lod.y >= 0.0 && d >= lod_fixed(int(a_lod.y))); }
    if (outlod) result.clip = vec4(2.0, 2.0, 2.0, 1.0);
  }
  result.dkey = -1.0; result.pstart = 0;
#if DOS_LOOK
  if (u_dos != 0) {
    int vid = vertex_id, t = vid / 3, k, c;
    if (abs(dos_float(vid * 13) - a_pos.x) + abs(dos_float(vid * 13 + 2) - a_pos.z) < 0.01) {
      bool first = false, last = false;
      for (k = 0; k < 16 && t >= 0; k++) { if (((int(dos_float(3 * t * 13 + 12) + 0.5) % 32 / 3) & 4) != 0) { first = true; break; } if (t == 0) break; t--; }
      float km = -1e9;
      for (k = 0; k < 16; k++) {
        int b = 3 * (t + k);
        for (c = 0; c < 3; c++) km = max(km, dos_camz(vec3(dos_float((b + c) * 13), dos_float((b + c) * 13 + 1), dos_float((b + c) * 13 + 2))));
        if (((int(dos_float(b * 13 + 12) + 0.5) % 32 / 3) & 2) != 0) { last = true; break; }
      }
      result.dkey = km; result.pstart = first && last ? 3 * t + 1 : 0;   /* (a polygon walked whole, its first to its last triangle) */
    }
  }
#endif
  result.viewpos = (u_view * vec4(p, 1.0)).xyz; result.world = p;
  result.mapmat = int(a_mapmat); float mq = trunc((floor(p.y+0.5)-u_map_range.x)*65536.0/max(u_map_range.y,1.0));
  result.mapshade=clamp(floor(mq/4096.0+0.5),0.0,15.0);
  result.tivf = ti_vertex_fog(result.viewpos, result.world) * result.clip.w;
  result.normal = nrm; result.uv = a_uv; result.color = a_color; result.texsel = int(floor(a_texsel + 0.5)); result.sph = a_sphere;
  if (u_dos != 0 && int(a_color.r + 0.5) == 5) result.color.g *= result.clip.w;
  result.tic = a_tic.rgb; result.ticm = int(a_tic.a + 0.5); result.gs = a_uv; result.uvl = a_uv;
#if DOS_LOOK
  if (u_dos != 0 && result.ticm == 4) {   /* the vertex's own level (dos_vertex_level), interpolated over the polygon */
    float lv = dos_dnib(dos_vertex_level(p, a_tic.rgb, floor(a_color.b + 0.5)), result.dnib);
    /* interpolated in screen space, as the original's spans (v_gs: noperspective) - a huge polygon's corner made on the
     * near plane (0x4cd65) projects far off the screen, so the far corners' levels hold across what is seen (BET2's
     * TL1PLAIN black at night in DOSBox, as its far corners) */
    result.gs = int(a_color.r + 0.5) == 3 ? vec2(0.0, a_uv.y + a_uv.x * dos_gouraud_level(lv))   /* Gouraud: its corner's colour */
                                          : vec2(lv, 0.0);
  }
#endif
  int hi = e / 32; e -= hi * 32; int corner = e - 3 * (e / 3);
  result.bary = vec3(corner == 0 ? 1.0 : 0.0, corner == 1 ? 1.0 : 0.0, corner == 2 ? 1.0 : 0.0); result.emask = (e / 3) | (hi << 3);

  return result;
}
void output_vertex(VertexOut value) {
  gl_Position = value.clip;
  v_normal = value.normal;
  v_uv = value.uv;
  v_color = value.color;
  v_viewpos = value.viewpos;
  v_world = value.world;
  v_bary = value.bary;
  v_tic = value.tic;
  v_tivf = value.tivf;
  v_mapshade = value.mapshade;
  v_texsel = value.texsel;
  v_emask = value.emask;
  v_bb = value.bb;
  v_tif = value.tif;
  v_dnib = value.dnib;
  v_pit = value.pit;
  v_sph = value.sph;
  v_ticm = value.ticm;
  v_dkey = value.dkey;
  v_mapmat = value.mapmat;
  v_gs = value.gs;
  v_uvl = value.uvl;
  v_pstart = value.pstart;
  // Preserve GL's division/multiply rounding while changing the clip depth range.
  // A corner exactly on the eye plane still needs native homogeneous clipping.
  // Dividing by its zero w would turn the whole primitive into NaNs; only that
  // singular case uses the finite homogeneous limit of the depth conversion.
  if (gl_Position.w == 0.0) gl_Position.z *= 0.5;
  else gl_Position.z = (gl_Position.z / gl_Position.w * 0.5 + 0.5) * gl_Position.w;
}
VertexOut clipped_vertex(VertexOut a, VertexOut b, VertexOut c, vec3 weight, bool boundary) {
  VertexOut result;
  result.clip = weight.x * a.clip + weight.y * b.clip + weight.z * c.clip;
  if (boundary) result.clip.z = -result.clip.w;
  result.normal = weight.x * a.normal + weight.y * b.normal + weight.z * c.normal;
  result.uv = weight.x * a.uv + weight.y * b.uv + weight.z * c.uv;
  result.color = weight.x * a.color + weight.y * b.color + weight.z * c.color;
  result.viewpos = weight.x * a.viewpos + weight.y * b.viewpos + weight.z * c.viewpos;
  result.world = weight.x * a.world + weight.y * b.world + weight.z * c.world;
  result.bary = weight.x * a.bary + weight.y * b.bary + weight.z * c.bary;
  result.tic = weight.x * a.tic + weight.y * b.tic + weight.z * c.tic;
  result.tivf = weight.x * a.tivf + weight.y * b.tivf + weight.z * c.tivf;
  result.mapshade = weight.x * a.mapshade + weight.y * b.mapshade + weight.z * c.mapshade;
  result.texsel = a.texsel;
  result.emask = a.emask;
  result.bb = a.bb;
  result.tif = a.tif;
  result.dnib = a.dnib;
  result.pit = a.pit;
  result.sph = a.sph;
  result.ticm = a.ticm;
  result.dkey = a.dkey;
  result.mapmat = a.mapmat;
  vec3 screen_weight = weight * vec3(a.clip.w, b.clip.w, c.clip.w) / result.clip.w;
  result.gs = screen_weight.x * a.gs + screen_weight.y * b.gs + screen_weight.z * c.gs;
  result.uvl = screen_weight.x * a.uvl + screen_weight.y * b.uvl + screen_weight.z * c.uvl;
  result.pstart = a.pstart;
  result.tivf = ti_vertex_fog(result.viewpos, result.world) * result.clip.w;
#if DOS_LOOK
  if (u_dos != 0 && result.ticm == 4 && boundary) {
    /* a corner made on the near plane (MW2.EXE 0x4cd65: position and vertex normal interpolated along the edge) is lit
     * at its own depth and normal (0x4f6c8), not given its corners' interpolated level */
    float lv = dos_dnib(dos_vertex_level(result.world, result.tic, floor(result.color.b + 0.5)), result.dnib);
    result.gs = int(a.color.r + 0.5) == 3 ? vec2(0.0, result.uv.y + result.uv.x * dos_gouraud_level(lv)) : vec2(lv, 0.0);
  }
#endif
  return result;
}

void remap_depth(inout VertexOut value) {
#if DOS_LOOK
  if (u_dos != 0) return;   /* (the DOS look keeps its own depths) */
#endif
  if (u_shared_depth != 0) {
    vec2 ab = value.pit > 0 ? u_depth_remap.zw : u_depth_remap.xy;
    value.clip.z = ab.x * value.clip.z + ab.y * value.clip.w;
  }
}
void main() {
  // SDL_GPU deliberately has no geometry shader. Six output vertices per
  // source triangle reproduce Mercs' former GS: one triangle when all corners
  // survive (the second is degenerate), two when clipping makes a quad.
  // This increases vertex work, but leaves DOS span walking, clipping, fog
  // quantization, billboards and shared cockpit/world depth in one shader.
  int source = u_first + (gl_VertexIndex / 6) * 3;
  int corner = gl_VertexIndex % 6;
  VertexOut p[3];
  p[0] = world_vertex(source); p[1] = world_vertex(source + 1); p[2] = world_vertex(source + 2);
  float d[3]; int inside = 0;
  for (int i = 0; i < 3; i++) { d[i] = p[i].clip.z + p[i].clip.w; if (d[i] >= 0.0) inside++; }
#if DOS_LOOK
  /* DOS: a triangle crossing the near plane is cut here (as 0x4d379 cuts the polygon), so the new corners can be lit
   * as the original lights them (clipped_vertex); the cockpit set and billboards as they are */
  if (inside == 3 || u_dos == 0 || p[0].pit > 0 || p[0].bb != 0) {
#else
  if (inside == 3 || (u_shared_depth == 0 && (u_tifog.x <= 0.5 || u_tifog.z <= 0.5 || p[0].pit > 0 || p[0].bb != 0))) {
#endif
    VertexOut value = p[corner < 3 ? corner : 0]; remap_depth(value); output_vertex(value); return;
  }
  if (inside == 0) {
    VertexOut value = p[0]; value.clip = vec4(2.0, 2.0, 2.0, 1.0); output_vertex(value); return;
  }
  vec3 unit[3] = vec3[3](vec3(1,0,0), vec3(0,1,0), vec3(0,0,1));
  vec3 weights[4]; bool boundary[4]; int count = 0;
  if (d[0] >= 0.0) { weights[count] = unit[0]; boundary[count++] = false; }
  for (int i = 0; i < 3; i++) {
    int j = (i + 1) % 3;
    if ((d[i] >= 0.0) != (d[j] >= 0.0)) { weights[count] = mix(unit[i], unit[j], d[i] / (d[i] - d[j])); boundary[count++] = true; }
    if (j != 0 && d[j] >= 0.0) { weights[count] = unit[j]; boundary[count++] = false; }
  }
  int index = corner < 3 ? corner : count == 4 ? (corner == 3 ? 0 : corner - 2) : 0;
  VertexOut value = clipped_vertex(p[0], p[1], p[2], weights[index], boundary[index]);
  remap_depth(value); output_vertex(value);
}
