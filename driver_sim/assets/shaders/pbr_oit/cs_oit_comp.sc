#include <bgfx_compute.sh>
#include "pbr.sh"
#include "../common/color.sh"
#include "../common/packing.sh"
#include "../gtao/common.sh"

IMAGE2D_RO(s_accum, rgba16f, 0);
IMAGE2D_RO(s_albedo, rgba8, 1);
IMAGE2D_RO(s_emission, r11f_g11f_b10f, 2);
UIMAGE2D_RO(s_normal, r32ui, 3);
IMAGE2D_RO(s_pbrData, rgba8, 4);
UIMAGE2D_RO(s_finalAOTerm, r32ui, 5);
IMAGE2D_WO(s_output, r11f_g11f_b10f, 6);
SAMPLER2D(s_depth, 7);

uniform vec4 u_jitter;

// linear space
uniform vec4 u_skyColor;

/*
    float indirectIntensity
    float directIntensity
    float bentNormalIntensity
*/
uniform vec4 u_gtaoIntensity;

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

    vec2 uvn = vec2(uvi + vec2_splat(0.5)) / render_size;

    float depth = texture2DLod(s_depth, uvn, 0).r;

    vec4 pbr_col;

    if(depth > 0) {
        vec4 albedo = imageLoad(s_albedo, uvi);
        vec3 emission = imageLoad(s_emission, uvi).rgb;

        uint packedInput = imageLoad(s_normal, uvi).x;
        vec3 unpackedOutput = R11G11B10_UNORM_to_FLOAT3( packedInput );
        vec3 normal = normalize(unpackedOutput * vec3_splat(2.0) - vec3_splat(1.0));

        vec4 pbrData = imageLoad(s_pbrData, uvi);

        float indirectAO = 1.0;
        float directAO = 1.0;

        if(u_gtaoIntensity.x > 0.0 || u_gtaoIntensity.y > 0.0) {
            // no filtering allowed as it could be packed bent normal
            uint aoPacked = imageLoad(s_finalAOTerm, uvi).x;
            float aoVisibility = 1.0;
            vec3 bentNormal = normal;
            XeGTAO_DecodeVisibilityBentNormal( aoPacked, aoVisibility, bentNormal );

            indirectAO = mix(1.0, aoVisibility, u_gtaoIntensity.x);
            directAO = mix(1.0, aoVisibility, u_gtaoIntensity.y);
            normal = mix(normal, bentNormal, u_gtaoIntensity.z);
        }
        
        vec3 current_ndc = vec3(uvn * 2.0 - 1.0, depth);
        current_ndc.y *= -1.0;
        
        vec3 current_uv = vec3((current_ndc.xy - u_jitter.xy) * 0.5 + 0.5, current_ndc.z);

        vec4 view_position = mul(u_invProj, vec4(current_ndc, 1.0));
        view_position.xyz /= view_position.w;

        pbr_col = pbr(albedo.rgb, emission.rgb, view_position.xyz, normal, albedo.a, pbrData.y, pbrData.z, indirectAO, directAO);
    } else {
        pbr_col = vec4_splat(0.0);
    }
    
    float transparentAlpha = saturate(accum.a);

    vec4 finalColor;
    finalColor.rgb = accum.rgb + pbr_col.rgb * (1.0 - transparentAlpha);
    finalColor.a = transparentAlpha + pbr_col.a * (1.0 - transparentAlpha);

    imageStore(s_output, uvi, vec4(finalColor.rgb + u_skyColor.rgb * (1.0 - finalColor.a), 1.0));
}