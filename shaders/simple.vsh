#version 110

attribute vec3 a_pos;
attribute vec2 a_uv0;
attribute vec4 a_color;
attribute vec4 a_stroke;
attribute float a_gamma;
attribute float a_width;
attribute float a_material;

uniform mat4 u_mvp;

varying vec4 v_color;
varying vec4 v_stroke;
varying vec2 v_uv0;
varying float v_gamma;
varying float v_width;
varying float v_material;

void main() {
    v_stroke = a_stroke;
    v_color = a_color;
    v_gamma = a_gamma;
    v_width = a_width;
    v_uv0 = a_uv0;
    v_material = a_material;
    gl_Position = u_mvp * vec4(a_pos.xyz, 1.0);
}
