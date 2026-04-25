#include <bgfx_compute.sh>
#include "pbr.sh"

IMAGE2D_RO(s_accum, rgba16f, 0);
IMAGE2D_RO(s_reveal, r16f, 1);
IMAGE2D_RO(s_albedo, rgba8, 2);
IMAGE2D_RO(s_normal, rgba16f, 3);
IMAGE2D_WO(s_output, rgba8, 4);

NUM_THREADS(8, 8, 1)
void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    
    if (any(greaterThanEqual(pixel, ivec2(u_viewRect.zw))))
        return;

    vec4 accum = imageLoad(s_accum, pixel);
    float reveal = imageLoad(s_reveal, pixel).r;
    vec4 albedo = imageLoad(s_albedo, pixel);
    vec3 normal = imageLoad(s_normal, pixel).xyz;

    vec4 pbr_col = pbr(albedo.rgb, normal, albedo.a);
    vec4 oit_col = clamp(vec4(accum.rgb / clamp(accum.a, 1e-4, 5e4), reveal), 0.0, 300.0);
    oit_col.a = saturate(oit_col.a);
    vec4 final_col = oit_col * (1.0 - oit_col.a) + pbr_col * oit_col.a;
    final_col.a = saturate(final_col.a);

    vec4 sky_col = vec4(0.18, 0.18, 0.2, 1.0);

    imageStore(s_output, pixel, vec4(sky_col.rgb * (1.0 - final_col.a) + final_col.rgb * final_col.a, 1.0));
}