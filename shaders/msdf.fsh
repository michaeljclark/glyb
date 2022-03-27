#version 150

in vec4 v_color;
in vec2 v_uv0;
in float v_gamma;

uniform sampler2D u_tex0;

out vec4 outFragColor;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

#define SQRT2_2 0.70710678118654757 /* 1 / sqrt(2.) */

void main()
{
    vec3 sample = texture(u_tex0, v_uv0).rgb;

    ivec2 sz = textureSize( u_tex0, 0 );
    //vec2 sz = vec2(1024,1024);

    float dx = dFdx( v_uv0.x ) * sz.x;
    float dy = dFdy( v_uv0.y ) * sz.y;
    float toPixels = 16.0 * inversesqrt( dx * dx + dy * dy );
    float sigDist = median( sample.r, sample.g, sample.b ) - 0.5;
    float alpha = clamp( sigDist * toPixels + 0.5, 0.0, 1.0 );

    outFragColor = vec4(pow(v_color.rgb, vec3(1.0/v_gamma)), alpha);
}
