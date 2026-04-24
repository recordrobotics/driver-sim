$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_tex, 0);

void main()
{
    vec4 col = texture2D(s_tex, v_texcoord0);
    gl_FragColor = vec4(col.rgb, 1.0);
}