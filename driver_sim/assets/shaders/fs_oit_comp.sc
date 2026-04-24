$input v_texcoord0

#include <bgfx_shader.sh>
#include "pbr.sh"

SAMPLER2D(s_accum, 0);
SAMPLER2D(s_reveal, 1);
SAMPLER2D(s_albedo, 2);
SAMPLER2D(s_normal, 3);

void main()
{
    vec4 accum = texture2D(s_accum, v_texcoord0);
    float reveal = texture2D(s_reveal, v_texcoord0).r;
    vec4 albedo = texture2D(s_albedo, v_texcoord0);
    vec3 normal = texture2D(s_normal, v_texcoord0).xyz;

    vec4 pbr_col = pbr(albedo.rgb, normal, albedo.a);
    vec4 oit_col = clamp(vec4(accum.rgb / clamp(accum.a, 1e-4, 5e4), reveal), 0.0, 300.0);
    oit_col.a = saturate(oit_col.a);
    vec4 final_col = oit_col * (1.0 - oit_col.a) + pbr_col * oit_col.a;
    final_col.a = saturate(final_col.a);

    vec4 sky_col = vec4(0.18, 0.18, 0.2, 1.0);

    gl_FragColor = vec4(sky_col.rgb * (1.0 - final_col.a) + final_col.rgb * final_col.a, 1.0);
}