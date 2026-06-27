$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_tex, 0);

void main()
{
    vec3 color = texture2D(s_tex, v_texcoord0).rgb;

    // Tonemapping
    color = color / (color + vec3_splat(1.0));

    // Gamma correction
    color = pow(abs(color), vec3_splat(1.0 / 2.2));

    gl_FragColor = vec4(color, 1.0);
}