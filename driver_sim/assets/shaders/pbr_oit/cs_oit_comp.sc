#include <bgfx_compute.sh>
#include "../common/pbr.sh"

IMAGE2D_RO(s_accum, rgba16f, 0);
IMAGE2D_RO(s_reveal, r16f, 1);
IMAGE2D_RO(s_albedo, rgba8, 2);
IMAGE2D_RO(s_emission, rgba16f, 3);
IMAGE2D_RO(s_normal, rgba16f, 4);
IMAGE2D_RO(s_pbrData, rgba16f, 5);
IMAGE2D_RO(s_depth, r32f, 6);
IMAGE2D_WO(s_output, rgba16f, 7);

uniform vec4 u_jitter;

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 render_size = ivec2(imageSize(s_albedo));
	ivec2 uvi = ivec2(gl_GlobalInvocationID.xy);
	if ((uvi.x >= render_size.x) || (uvi.y >= render_size.y)) 
	{
		return;
	}

    vec4 accum = imageLoad(s_accum, uvi);
    float reveal = imageLoad(s_reveal, uvi).r;
    
    float depth = imageLoad(s_depth, uvi).r;

    vec4 pbr_col;

    if(depth > 0) {
        vec4 albedo = imageLoad(s_albedo, uvi);
        vec4 emission = imageLoad(s_emission, uvi);
        vec3 normal = imageLoad(s_normal, uvi).xyz;
        vec4 pbrData = imageLoad(s_pbrData, uvi);

        vec2 uvn = vec2(uvi + vec2_splat(0.5)) / render_size;
        
        vec3 current_ndc = vec3(uvn * 2.0 - 1.0, depth);
        current_ndc.y *= -1.0;
        vec3 current_uv = vec3((current_ndc.xy - u_jitter.xy) * 0.5 + 0.5, current_ndc.z);

        vec4 view_position = mul(u_invProj, vec4(current_ndc, 1.0));
        view_position.xyz /= view_position.w;

        vec4 world_local_position = mul(u_invView, vec4(view_position.xyz, 1.0));

        pbr_col = pbr(albedo.rgb, emission.rgb, world_local_position.xyz, mtxGetColumn(u_invView, 3).xyz, normal, albedo.a, pbrData.y, pbrData.z);
    } else {
        pbr_col = vec4_splat(0.0);
    }
    
    vec4 oit_col = clamp(vec4(accum.rgb / clamp(accum.a, 1e-4, 5e4), reveal), 0.0, 300.0);
    oit_col.a = saturate(oit_col.a);
    vec4 final_col = oit_col * (1.0 - oit_col.a) + pbr_col * oit_col.a;
    final_col.a = saturate(final_col.a);

    vec3 sky_col = pow(vec3(0.18, 0.18, 0.2) * 1.5, vec3_splat(2.2));

    imageStore(s_output, uvi, vec4(sky_col * (1.0 - final_col.a) + final_col.rgb * final_col.a, 1.0));
}