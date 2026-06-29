$input v_viewPosition, v_viewNormal, v_currentPosition, v_previousPosition, v_worldPosition

#include <bgfx_shader.sh>
#include "../common/utils.sh"
#include "../common/color.sh"

SAMPLER2D(s_baseColor, 0);
SAMPLER2D(s_bump, 1);

uniform vec4 u_baseColor;
uniform vec4 u_emissionColor;
uniform vec4 u_jitter;
uniform vec4 u_pbrData;

vec3 ComputeHeightNormalTS(vec2 uv, vec2 texelSize, float heightScale, float cellSize)
{
    float hL = texture2D(s_bump, uv - vec2(texelSize.x, 0)).r;
    float hR = texture2D(s_bump, uv + vec2(texelSize.x, 0)).r;
    float hD = texture2D(s_bump, uv - vec2(0, texelSize.y)).r;
    float hU = texture2D(s_bump, uv + vec2(0, texelSize.y)).r;

    float dU = (hR - hL) * heightScale;
    float dV = (hU - hD) * heightScale;

    return normalize(vec3(
        -dU,
        -dV,
         2.0 * cellSize
    ));
}

void main()
{
	bool writeMotionVectors = u_pbrData.x > 0.5;

    vec2 uv = v_worldPosition.xy * 0.4;

    vec3 N = normalize(v_viewNormal);
    vec3 T = normalize(mtxGetColumn(u_modelView, 0)).xyz;
    T = normalize(T - N * dot(T, N));
    vec3 B = normalize(cross(N, T));

    mat3 TBN = mtxFromCols(T, B, N);

    vec3 normalTS = ComputeHeightNormalTS(uv, vec2_splat(1.0 / 1024.0), 0.7, 1.0);

    vec3 viewNormal = normalize(mul(TBN, normalTS));

	gl_FragData[0] = u_baseColor * vec4(SRGBToLinear(texture2D(s_baseColor, uv).rgb), 1.0);
	gl_FragData[1] = u_emissionColor;
	gl_FragData[2] = vec4(viewNormal, 1.0);
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