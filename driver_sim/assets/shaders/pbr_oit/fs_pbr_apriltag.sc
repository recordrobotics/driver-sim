$input v_viewNormal, v_texcoord0

#include <bgfx_shader.sh>
#include "../common/utils.sh"
#include "../common/color.sh"
#include "../common/packing.sh"

SAMPLER2D(s_apriltags, 0);

uniform vec4 u_baseColor;
uniform vec4 u_emissionColor;
uniform vec4 u_jitter;
uniform vec4 u_pbrData;

void main()
{
    vec3 unpackedInput = v_viewNormal * vec3_splat(0.5) + vec3_splat(0.5);
    uint packedOutput = FLOAT3_to_R11G11B10_UNORM( unpackedInput );

	gl_FragData[0] = u_baseColor * vec4(SRGBToLinear(vec3_splat(texture2D(s_apriltags, v_texcoord0).r)), 1.0);
	gl_FragData[1] = vec4(u_emissionColor.rgb * u_emissionColor.a, 1.0);
	gl_FragData[2] = packedOutput;
	gl_FragData[3] = u_pbrData;
	gl_FragData[4] = vec4_splat(0.0);
}