#version 150

in vec3 a_pos;
in vec2 a_uv0;
in vec4 a_color;
in vec4 a_stroke;
in float a_gamma;
in float a_width;
in float a_shape;

uniform mat4 u_mvp;

out vec4 v_color;
out vec2 v_uv0;
out float v_gamma;
out float v_shape;

void main() {
    v_color = a_color;
    v_gamma = a_gamma;
    v_uv0 = a_uv0;
    v_shape = a_shape;
    gl_Position = u_mvp * vec4(a_pos.xyz, 1.0);
}
