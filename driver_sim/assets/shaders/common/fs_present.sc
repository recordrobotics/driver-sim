$input v_texcoord0

#include <bgfx_shader.sh>
#include "color.sh"

SAMPLER2D(s_texPresent, 0);

void main()
{
    vec3 linearColor = texture2D(s_texPresent, v_texcoord0).rgb;
    vec3 srgbColor = LinearToSRGB(linearColor);

    // TODO: dither

    gl_FragColor = vec4(srgbColor, 1.0);
}