$input v_viewPosition, v_viewNormal, v_currentPosition, v_previousPosition, v_worldPosition

#include <bgfx_shader.sh>
#include "../common/utils.sh"

uniform vec4 u_baseColor;
uniform vec4 u_emissionColor;
uniform vec4 u_jitter;
uniform vec4 u_pbrData;

void main()
{
	bool writeMotionVectors = u_pbrData.x > 0.5;

	gl_FragData[0] = u_baseColor;
	gl_FragData[1] = u_emissionColor;
	gl_FragData[2] = vec4(v_viewNormal, 1.0);
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