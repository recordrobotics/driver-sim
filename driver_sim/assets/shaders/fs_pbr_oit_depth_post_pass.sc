$input v_viewPosition, v_worldNormal, v_currentPosition, v_previousPosition

#include <bgfx_shader.sh>

uniform vec4 u_jitter;

void main()
{
    vec3 currentPosNDC = v_currentPosition.xyz / v_currentPosition.w;
	vec3 previousPosNDC = v_previousPosition.xyz / v_previousPosition.w;
	vec2 velocity = (currentPosNDC.xy - u_jitter.xy) - (previousPosNDC.xy - u_jitter.zw);

    gl_FragData[0] = vec4(velocity, 0.0, 0.0);
}