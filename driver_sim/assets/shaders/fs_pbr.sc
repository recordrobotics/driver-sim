$input v_viewPosition, v_worldNormal, v_currentPosition, v_previousPosition

#include <bgfx_shader.sh>
#include "utils.sh"

uniform vec4 u_baseColor;
uniform vec4 u_jitter;

void main()
{
	gl_FragData[0] = u_baseColor;
	gl_FragData[1] = vec4(v_worldNormal, 1.0);

	vec3 currentPosNDC = v_currentPosition.xyz / v_currentPosition.w;
	vec3 previousPosNDC = v_previousPosition.xyz / v_previousPosition.w;
	vec2 velocity = (currentPosNDC.xy - u_jitter.xy) - (previousPosNDC.xy - u_jitter.zw);

	gl_FragData[2] = vec4(velocity, 0.0, 0.0);
}