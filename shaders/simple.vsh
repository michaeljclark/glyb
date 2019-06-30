#version 130

#extension GL_EXT_gpu_shader4 : enable

attribute vec3 a_pos;
attribute vec2 a_uv0;
attribute vec4 a_color;
attribute float a_gamma;
attribute float x_material;

uniform mat4 u_mvp;

varying vec4 v_color;
varying vec2 v_uv0;
varying float v_gamma;
varying float v_material;

void main() {
    v_color = a_color;
    v_gamma = a_gamma;
    v_uv0 = a_uv0;
    v_material = x_material;
    gl_Position = u_mvp * vec4(a_pos.xyz, 1.0);
}
