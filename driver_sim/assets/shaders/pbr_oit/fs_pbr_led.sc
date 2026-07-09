$input v_viewNormal, v_currentPosition, v_previousPosition, v_texcoord0

#include <bgfx_shader.sh>
#include "../common/utils.sh"
#include "../common/color.sh"
#include "../common/packing.sh"

SAMPLER2D(s_ledMask, 0);
SAMPLER2D(s_ledColors, 1);

uniform vec4 u_baseColor;
uniform vec4 u_emissionColor;
uniform vec4 u_jitter;
uniform vec4 u_pbrData;
uniform vec4 u_ledData;

vec3 fetchLedColor(int index, int ledCountI)
{
	if(index < 0 || index >= ledCountI)
	{
		return vec3_splat(0.0);
	}
	return texelFetch(s_ledColors, ivec2(index, 0), 0).rgb;
}

float smoothRemap(float value, float minInput, float maxInput, float minOutput, float maxOutput)
{
	return mix(minOutput, maxOutput, smoothstep(minInput, maxInput, value));
}

void main()
{
	bool writeMotionVectors = u_pbrData.x > 0.5;

    vec3 unpackedInput = v_viewNormal * vec3_splat(0.5) + vec3_splat(0.5);
    uint packedOutput = FLOAT3_to_R11G11B10_UNORM( unpackedInput );

	vec2 ledCoord = v_texcoord0 * vec2(u_ledData.x, 1.0);
	vec2 ledMaskUV = fract(ledCoord);
	ledMaskUV.x = ledMaskUV.x * u_ledData.y + (1.0 - u_ledData.y) * 0.5;

	float inside =
		step(0.0, v_texcoord0.x) *
		step(v_texcoord0.x, 1.0) *
		step(0.0, v_texcoord0.y) *
		step(v_texcoord0.y, 1.0);

	float mask = inside * SRGBToLinear(texture2D(s_ledMask, ledMaskUV).r);

	int ledCountI = int(floor(u_ledData.x));

	vec3 ledColorB = fetchLedColor(int(floor(ledCoord.x)), ledCountI);
	vec3 ledColorA = fetchLedColor(int(floor(ledCoord.x - 1.0)), ledCountI);
	vec3 ledColorC = fetchLedColor(int(floor(ledCoord.x + 1.0)), ledCountI);

	vec3 ledColor = mix(ledColorA, ledColorB, smoothRemap(fract(ledCoord.x), 0.0, 0.4, 0.5, 1.0));
	ledColor = mix(ledColor, ledColorC, smoothRemap(fract(ledCoord.x), 0.6, 1.0, 0.0, 0.5));

	vec3 emission = u_emissionColor.rgb * u_emissionColor.a;
	emission += SRGBToLinear(ledColor) * 550.0 * mask;

	gl_FragData[0] = u_baseColor;
	gl_FragData[1] = vec4(emission, 1.0);
	gl_FragData[2] = packedOutput;
	gl_FragData[3] = u_pbrData;

	if(writeMotionVectors) {
		vec3 currentPosNDC = v_currentPosition.xyz / v_currentPosition.w;
		vec3 previousPosNDC = v_previousPosition.xyz / v_previousPosition.w;

		vec2 currentPosUV = (currentPosNDC.xy - u_jitter.xy) * 0.5 + 0.5;
		vec2 previousPosUV = (previousPosNDC.xy - u_jitter.zw) * 0.5 + 0.5;
		vec2 velocity = previousPosUV - currentPosUV;
		
		gl_FragData[4] = vec4(velocity, 0.0, 1.0);
	} else {
		gl_FragData[4] = vec4_splat(0.0);
	}
}