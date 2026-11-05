#version 450
// One-pixel palette primitives expanded into triangles by glr.c.
layout(location = 0) in vec4 a_clip;
layout(location = 1) in vec3 a_colour;
layout(location = 0) out vec3 colour;
void main() {
    gl_Position = a_clip;
    if (gl_Position.w == 0.0) gl_Position.z *= 0.5;
    else gl_Position.z = (gl_Position.z / gl_Position.w * 0.5 + 0.5) * gl_Position.w;
    colour = a_colour;
}
