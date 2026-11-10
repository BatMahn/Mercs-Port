#version 450
#extension GL_GOOGLE_include_directive : require
#define UBO_SET 3
#define SSBO_SET 2
#define SSBO_BINDING 4
#include "world_common.glsl"
layout(set = 2, binding = 0) uniform sampler2D u_tex;
layout(set = 2, binding = 1) uniform sampler2D u_dos_tab;
layout(set = 2, binding = 2) uniform sampler2D u_ptab;
layout(set = 2, binding = 3) uniform sampler2D u_map_palette;
#define VARY in
V_LOCATIONS
layout(location = 0) out vec4 o_color;
vec4 glFragCoord;
      
  
 
 
 

 
   
  
float ti_len(vec3 v) { vec3 a = abs(v); float m = max(a.x, max(a.y, a.z)); return m + (a.x + a.y + a.z - m) * 0.25; }
vec3 ti_light(vec3 nn) {
  vec3 c = u_tiamb;
  for (int i = 0; i < 2; i++) {
    if (u_tilt[i] == 1) { float d = -dot(u_tilv[i].xyz, nn); if (d > 0.0) c += u_tilc[i].xyz * d; }
    else if (u_tilt[i] == 2) { vec3 l = u_tilv[i].xyz - v_world; float d = dot(l, nn); if (d > 0.0) c += u_tilc[i].xyz * d / max(ti_len(l), 1e-3); }
    else c += u_tilc[i].xyz;
  }
  c = min(c, vec3(1.0));
  vec3 acc = c; bool hit = false;
  for (int j = 0; j < 8; j++) {
    if (j >= u_tidn) break;
    vec3 dd = abs(v_sph.xyz - u_tidp[j].xyz);
    if (v_sph.w > 0.0 && (dd.x > u_tidp[j].w + v_sph.w || dd.y > u_tidp[j].w + v_sph.w || dd.z > u_tidp[j].w + v_sph.w)) continue;
    vec3 l = u_tidp[j].xyz - v_world; float d = dot(l, nn);
    hit = true;
    if (d > 0.0) acc += u_tidc[j].xyz * d / max(ti_len(l), 1e-3);
  }
  return hit ? min(c + acc, vec3(1.0)) : c;
}








   
   
   


 
 
 
    

vec3 dos_colour(float idx) { return texture(u_dos_tab, vec2((idx + 0.5) / 256.0, 16.5 / 17.0)).rgb; }
#if DOS_LOOK
vec4 dos_texture_tap(vec2 uv, float lev, bool keyed) {
  vec4 t = textureLod(u_tex, uv, 0.0); float idx = floor(t.r * 255.0 + 0.5);
  if (lev < 15.0) idx = floor(texture(u_dos_tab, vec2((idx + 0.5) / 256.0, (lev + 0.5) / 17.0)).r * 255.0 + 0.5);
  float a = keyed ? t.a : 1.0; return vec4(dos_colour(idx) * a, a);
}
vec4 dos_texture_filtered(vec2 uv, float lev, bool keyed) {
  vec2 size = vec2(textureSize(u_tex, 0)), p = uv * size - 0.5, w = fract(p), c = (floor(p) + 0.5) / size;
  vec4 top = mix(dos_texture_tap(c, lev, keyed), dos_texture_tap(c + vec2(1.0 / size.x, 0.0), lev, keyed), w.x);
  vec4 bottom = mix(dos_texture_tap(c + vec2(0.0, 1.0 / size.y), lev, keyed), dos_texture_tap(c + 1.0 / size, lev, keyed), w.x);
  vec4 t = mix(top, bottom, w.y); if (t.a > 0.0) t.rgb /= t.a; return t;
}
#endif
#if DOS_LOOK
vec4 dos_gclip(int i) { int b = i * 13; vec4 q = u_mvp * vec4(dos_float(b), dos_float(b + 1), dos_float(b + 2), 1.0); if (v_pit > 0) q.z = u_pit_z.x * q.w + u_pit_z.y; return q; }
float dos_gshade(int i, float g) {
  int b = i * 13;
  if (world_vtx[i * 25 + 20] > 3.5)   /* each corner at its own level (0x4f6c8 -> the span's +0x2c), not the polygon's */
    g = (dos_dnib(dos_vertex_level(vec3(dos_float(b), dos_float(b + 1), dos_float(b + 2)), vec3(world_vtx[i * 25 + 17], world_vtx[i * 25 + 18], world_vtx[i * 25 + 19]), floor(dos_float(b + 10) + 0.5)), v_dnib) + 1.0) / 16.0;
  return dos_float(b + 7) + dos_float(b + 6) * dos_gouraud_level(floor(g * 16.0 - 0.5));
}
int dos_q(float p) { return (int(trunc(p * 4.0)) + 2) >> 2; }
ivec2 dos_gvtx(vec4 q) { vec2 p = q.xy / q.w * 0.5 * u_vp; ivec2 c = ivec2(u_vp) / 2; return ivec2(c.x + dos_q(p.x), c.y - dos_q(p.y)); }
  
ivec2 dos_gchain(ivec2 V[20], int C[20], int m, int top, int dir, int y, int ybot) {
  int a = top, b = (a + dir + m) % m, r0 = V[top].y;
  for (int g = 0; g < 20 && V[b].y == V[a].y && b != top; g++) { a = b; b = (a + dir + m) % m; }
  int cnt = V[b].y - V[a].y, yq = y == ybot && y > r0 ? y - 1 : y;
  for (int g = 0; g < 20 && yq >= r0 + cnt && b != top; g++) { r0 += cnt; a = b; b = (a + dir + m) % m; cnt = V[b].y - V[a].y; if (cnt == 0) cnt = 1; }
  if (cnt == 0) cnt = 1;
  uint t = uint(y - r0);
  return ivec2(int(uint(V[a].x * 65536 + 0x8000) + t * uint(dos_idiv((V[b].x - V[a].x) * 65536, cnt))),
               int(uint(C[a] + 0x8000) + t * uint(dos_idiv(C[b] - C[a], cnt))));
}
vec3 dos_gouraud(float lev) {
  float g = (lev + 1.0) / 16.0, px = floor(glFragCoord.x), py = floor(glFragCoord.y);
  float cc = v_gs.y + v_gs.x * dos_gouraud_level(lev) + 0.5, cmin = -1e9, cmax = 1e9;
  int k = int(px), r = int(py);
  uint val = 0u; bool exact = false;
  if (v_texsel > 0) {
    vec4 P[18]; float C[18]; int I[18]; ivec2 V[20]; int CF[20]; int n = 0, m = 0, s = v_texsel - 1, L1 = int(lev) + 1;
    for (int t = 0; t < 16; t++) {
      int b = s + 3 * t;
      if (t == 0) { P[0] = dos_gclip(b); C[0] = dos_gshade(b, g); I[0] = b; n = 1; }
      P[n] = dos_gclip(b + 1); C[n] = dos_gshade(b + 1, g); I[n] = b + 1; n++;
      if (((int(dos_float(b * 13 + 12) + 0.5) % 32 / 3) & 2) != 0) { P[n] = dos_gclip(b + 2); C[n] = dos_gshade(b + 2, g); I[n] = b + 2; n++; break; }
    }
    cmin = 1e9; cmax = -1e9; bool near_clipped = false;
    for (int i = 0; i < n; i++) {
      vec4 a = P[i], b = P[(i + 1) % n]; float da = a.z + a.w, db = b.z + b.w;
      if (da < 0.0) near_clipped = true;
      cmin = min(cmin, C[i]); cmax = max(cmax, C[i]);
      if (da >= 0.0) { int b2 = I[i] * 13; V[m] = dos_scr(vec3(dos_float(b2), dos_float(b2 + 1), dos_float(b2 + 2))); CF[m] = int(floor(C[i] * 65536.0 + 0.5)); m++; }
      if ((da >= 0.0) != (db >= 0.0)) { float t = da / (da - db); V[m] = dos_gvtx(mix(a, b, t)); CF[m] = int(floor(mix(C[i], C[(i + 1) % n], t) * 65536.0 + 0.5)); m++; }
    }
    if (m >= 3) {
      int top = 0, ybot = V[0].y;
      for (int i = 0; i < m; i++) { if (V[i].y <= V[top].y) top = i; ybot = max(ybot, V[i].y); }
      int y0 = int(u_vp.y) - 1 - (int(py) - int(u_vpo.y)), y = clamp(y0, V[top].y, ybot);
      if (!near_clipped && y0 != y) discard;
      ivec2 e1 = dos_gchain(V, CF, m, top, -1, y, ybot), e2 = dos_gchain(V, CF, m, top, 1, y, ybot);
      if (e2.x <= e1.x) { ivec2 t2 = e1; e1 = e2; e2 = t2; }
      int x0 = e1.x >> 16, w = (e2.x >> 16) - x0;
      int st = w != 0 ? dos_idiv(e2.y - e1.y, max(w >> 1, 1)) : 0;
      k = int(px) - int(u_vpo.x) - x0;
      if (!near_clipped && (k < 0 || k > w)) discard;
      k = clamp(k, 0, w); r = y - V[top].y;
      val = uint(e1.y) + uint(k >> 1) * uint(st); exact = true;
      cc = float(int(val)) / 65536.0;
    }
  }
  if (u_dos_smooth != 0) {
    float e = clamp(cc - 0.25, cmin, cmax), e0 = floor(e);
    return mix(dos_colour(e0), dos_colour(e0 + 1.0), e - e0);
  }
  if (exact) return dos_colour(float(((val + (((k + r) & 1) == 0 ? 0x7fffu : 0u)) >> 16) & 0xffu));
  return dos_colour(floor(cc + (((k + r) & 1) == 0 ? 32767.0 / 65536.0 : 0.0)));
}
#endif
void main() {
  glFragCoord = vec4(gl_FragCoord.x, u_fbh - gl_FragCoord.y, gl_FragCoord.zw);
  if (u_dos != 0 && v_pit > 0 && -v_viewpos.z < 8.0) discard;
  if(u_satellite != 0) {
    int kind=v_mapmat&61440; if(v_mapmat<0 || kind==32768 || kind==36864 || v_pit>0)discard;
    if(kind==0 || kind==16384) { int index=kind==0?(v_mapmat&255):((v_mapmat&240)+int(floor(v_mapshade+0.5)));
      o_color=vec4(texelFetch(u_map_palette,ivec2(clamp(index,0,255),0),0).rgb,1.0);return; }
  }
  if (u_wire.a > 0.0) {
    vec3 w = fwidth(v_bary); float d = 1.0e9;
    if ((v_emask & 1) != 0) d = min(d, v_bary.x / max(w.x, 1e-6));
    if ((v_emask & 2) != 0) d = min(d, v_bary.y / max(w.y, 1e-6));
    if ((v_emask & 4) != 0) d = min(d, v_bary.z / max(w.z, 1e-6));
    if (d > 1.0) { o_color = vec4(0.0, 0.0, 0.0, 1.0); return; }
    int wc = (v_emask >> 3) & 7;
    o_color = wc == 1 ? vec4(0.0, 48.0 / 255.0, 212.0 / 255.0, 1.0) : wc == 2 ? vec4(252.0 / 255.0, 24.0 / 255.0, 24.0 / 255.0, 1.0)
            : wc == 3 ? vec4(252.0 / 255.0, 204.0 / 255.0, 0.0, 1.0) : wc == 4 ? vec4(60.0 / 255.0, 0.0, 0.0, 1.0) : vec4(u_wire.rgb, 1.0); return; }
  if (u_shadow != 0) { o_color = vec4(0.0, 0.0, 0.0, 0.45); return; }
  if (u_horizon != 0) {
    float sx = glFragCoord.x, sy = glFragCoord.y;
    vec3 ray = u_hz_r * (sx - u_hz_c.x) / u_hz_c.z + u_hz_u * (sy - u_hz_c.y) / u_hz_c.w + u_hz_f;
    if (ray.y < 0.0) { o_color = vec4(dos_colour(u_hz_indices.z), 1.0); return; }
    float idx = u_hz_indices.x;
    if (u_hz_band > 0.0 && abs(u_hz_u.y) > 1e-6) {
      float sh = u_hz_c.y - u_hz_c.w * ((sx - u_hz_c.x) * u_hz_r.y / u_hz_c.z + u_hz_f.y) / u_hz_u.y;
      float d = sy - sh;
      if (d >= 0.0 && d < u_hz_band) {
        float c = mix(u_hz_indices.y, u_hz_indices.x, d / u_hz_band);
        if (u_hz_smooth != 0) {
          float e = clamp(c + 0.25, min(u_hz_indices.x, u_hz_indices.y), max(u_hz_indices.x, u_hz_indices.y)), e0 = min(floor(e), 254.0);
          o_color = vec4(mix(dos_colour(e0), dos_colour(e0 + 1.0), e - e0), 1.0); return;
        }
        float shl = u_hz_c.y - u_hz_c.w * ((u_hz_x.x - u_hz_c.x) * u_hz_r.y / u_hz_c.z + u_hz_f.y) / u_hz_u.y;
        float shr = u_hz_c.y - u_hz_c.w * ((u_hz_x.y - u_hz_c.x) * u_hz_r.y / u_hz_c.z + u_hz_f.y) / u_hz_u.y;
        int row = int(floor(max(shl, shr) + u_hz_band - sy)), col = int(floor(sx - u_hz_x.x));
        idx = clamp(floor(c + (((row + col) & 1) != 0 ? 1.0 : 0.5)), min(u_hz_indices.x, u_hz_indices.y), max(u_hz_indices.x, u_hz_indices.y));
      }
    }
    o_color = vec4(dos_colour(idx), 1.0); return;
  }
#if DOS_LOOK
  if (u_dos != 0) {
    int kind = int(v_color.r + 0.5); float base = floor(v_color.g + 0.5), C = floor(v_color.b + 0.5);
    if (kind == 0) { o_color = vec4(dos_colour(base), 1.0); return; }
    if (kind == 4) {
      if (u_dos_texture_linear != 0) { vec4 t = dos_texture_filtered(v_uv / u_texsize, 15.0, true); if (t.a < 0.5) discard; o_color = vec4(t.rgb, 1.0); return; }
      vec4 tb = texture(u_tex, v_uv / u_texsize); if (tb.a < 0.5) discard; o_color = vec4(dos_colour(floor(tb.r * 255.0 + 0.5)), 1.0); return; }
    if (kind == 5) { float sh = clamp(floor(v_color.g * glFragCoord.w + 0.00001), 0.0, 15.0);
      if (u_dos_texture_linear != 0) { o_color = vec4(dos_texture_filtered(v_uv, sh, false).rgb, 1.0); return; }
      float tx = floor(texture(u_tex, v_uv).r * 255.0 + 0.5);
      float idx = sh >= 15.0 ? tx : floor(texture(u_dos_tab, vec2((tx + 0.5) / 256.0, (sh + 0.5) / 17.0)).r * 255.0 + 0.5); o_color = vec4(dos_colour(idx), 1.0); return; }
    float nl = length(v_normal); vec3 Lv = u_dos_dir != 0 ? u_dos_lpos : u_dos_lpos - v_world; float ll = length(Lv);
    vec3 nf = nl > 0.0 ? -v_normal / nl : vec3(0.0);
    if (nl > 0.0 && dot(v_viewpos, mat3(u_view) * nf) >= 0.0) discard;
    float f = nl > 0.0 && ll > 0.0 ? trunc(128.0 * dot(nf, Lv / ll)) : 127.0;
    float light = u_dos_amb + (u_dos_light != 0 ? max(0.0, trunc(f * u_dos_intensity / 128.0)) : 0.0);
    if (u_dos_fogdist > 0.0) { int key = v_dkey >= 0.0 ? int(v_dkey) : int(max(-v_viewpos.z, 0.0) * 4.0); light = max(0.0, light - float((int(uint(key) << 11) / int(u_dos_fogdist)) >> 4)); }
    /* the point lights (MW2.EXE 0x4fb3b..0x4fbaf: the list 0xb411c, Multiple Lights on): while the light is under 0x80,
     * each one whose box (+-radius) holds the polygon (0x4f5f8) adds 128 x the cosine towards it, unscaled (0x4d20e) */
    for (int i = 0; i < 4; i++) {
      if (i >= u_dos_mlit_n || light >= 128.0 || nl <= 0.0) break;
      vec3 d = u_dos_mlit[i].xyz - v_world;
      if (any(greaterThan(abs(d), vec3(u_dos_mlit[i].w)))) continue;
      float lp = length(d), fi = lp > 0.0 ? trunc(128.0 * dot(nf, d / lp)) : 127.0;
      if (fi > 0.0) light += fi;
    }
    float lev = dos_dnib(clamp(floor(floor(C / 2.0) * light / 1088.0), 0.0, 15.0), v_dnib);
    if (kind == 1 || (kind == 2 && v_texsel < 0 && ((-v_texsel) & u_notex) != 0)) { o_color = vec4(dos_colour(base + lev), 1.0); return; }
    if (kind == 3) { o_color = vec4(dos_gouraud(lev), 1.0); return; }
    if (v_ticm == 4) lev = clamp(floor(v_gs.x + 0.5), 0.0, 15.0);   /* the vertices' levels across the polygon (0x4f6c8), in screen space */
    if (u_dos_texture_linear != 0) { vec4 t = dos_texture_filtered(v_uv / u_texsize, lev, true); if (t.a < 0.5) discard; o_color = vec4(t.rgb, 1.0); return; }
    vec4 tx = texture(u_tex, v_uv / u_texsize); if (tx.a < 0.5) discard;
    float ti = floor(tx.r * 255.0 + 0.5);
    float si = lev >= 15.0 ? ti : floor(texture(u_dos_tab, vec2((ti + 0.5) / 256.0, (lev + 0.5) / 17.0)).r * 255.0 + 0.5);
    o_color = vec4(dos_colour(si), 1.0); return;
  }
#endif
  if (v_ticm == 3 && ((-v_texsel) & u_notex) == 0) discard;
  vec3 base = u_mono.a > 1.5 ? v_color : u_mono.a > 0.0 ? u_mono.rgb : v_ticm >= 2 ? v_tic : v_color; float ta = 1.0; bool tinted = false;
  if (u_textured != 0 && u_mono.a <= 0.0 && (v_texsel >= 0 || ((-v_texsel) & u_notex) == 0)) {
    vec4 t; int af = u_ati != 0 && v_texsel < 0 ? -v_texsel : 0;
    vec2 uv = ((u_ati & 1) != 0 && (af & 4) != 0) || ((u_ati & 2) != 0 && (af & 16) != 0) ? v_uvl : v_uv;
#if DOS_LOOK
    if (u_sprite_indexed != 0) {
      if (u_dos_texture_linear != 0) t = dos_texture_filtered(uv / u_texsize, 15.0, true);
      else { t = textureLod(u_tex, uv / u_texsize, 0.0); t.rgb = dos_colour(floor(t.r * 255.0 + 0.5)); }
    } else
#endif
    if (u_mga_lod != 0) { float z = -v_viewpos.z; t = textureLod(u_tex, uv / u_texsize, z < 2500.0 ? 0.0 : z < 5000.0 ? 1.0 : z < 15000.0 ? 2.0 : 3.0); }
    else t = texture(u_tex, uv / u_texsize);
    if (u_blend == 3) {} else if (u_blend != 0) { if (t.a < 0.004) discard; } else if (t.a < 0.5 && ((af & 8) == 0 || (u_ati & 4) == 0)) discard; base = t.rgb * u_tint; ta = u_blend == 2 ? t.a * t.a : u_blend == 3 ? 1.0 : t.a; tinted = u_tinton != 0; }
  else if (u_pfade > 0.0 && v_texsel > 0 && v_bb == 0) base = mix(base, texture(u_ptab, vec2((float(v_texsel - 1) + 0.5) / 256.0, 0.5)).rgb, u_pfade);
  float n = length(v_normal);
  vec3 L = u_point != 0 ? normalize(u_lpos - v_world) : u_light;
  float diff = n > 0.0 ? abs(dot(v_normal / n, L)) : 0.5;
  float ds = v_dnib > 0 ? (u_dsign != 0 ? 1.0 + float(v_dnib) / 15.0 : 1.0 - float(v_dnib) / 15.0) : 1.0;
  vec3 lit = u_unlit != 0 || v_bb != 0 || tinted || (v_tif & 2) != 0 || v_ticm == 3 ? base : u_tilit == 1 ? base * (v_ticm == 1 ? v_tic : ti_light((v_tif & 1) != 0 ? v_normal : n > 0.0 ? v_normal / n : vec3(0.0)))
           : u_tilit == 2 ? base * u_tignd : base * min(1.0, (u_ambient + diff) * ds);
  float fz = max(-v_viewpos.z, 1.0);
  float fi = u_fog_density > 0.0 ? floor(u_fog_density / fz) : 1.0e9;
  float fk = mod(fi, 128.0), fs = floor(fi / 128.0);
  float ft = fk == 0.0 ? 255.0 : floor(256.0 * exp2(-fk / 128.0));
  float ff = fs >= 9.0 ? 0.0 : floor(ft / exp2(fs)) / 256.0;
  if (u_tifog.x > 0.5) ff = u_tifog.z > 0.5 ? v_tivf * glFragCoord.w : 1.0 - (u_tifog.y > 0.0 ? (u_tifog.z < -1.5 ? min(1.0, u_tifog.y / fz) : 1.0 - min(1.0, fz / u_tifog.y)) : 1.0) * u_tifog.w;
  if ((v_tif & 4) != 0) ff = 0.0;
  o_color = vec4(lit + (u_fog_color - lit) * ff, u_blend != 0 ? ta * u_alpha : u_alpha);
}
