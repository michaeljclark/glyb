#version 150

in vec4 v_color;
in vec2 v_uv0;
in float v_gamma;

uniform sampler2D u_tex0;

out vec4 outFragColor;

void main() {
    vec4 t_color = texture(u_tex0, v_uv0);
    outFragColor = v_color * vec4(pow(t_color.rgb, vec3(1.0/v_gamma)), t_color.a);
}
