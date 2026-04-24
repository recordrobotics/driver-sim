$input a_position, a_normal
$output v_viewPosition, v_worldNormal, v_currentPosition, v_previousPosition

#include <bgfx_shader.sh>

uniform mat4 u_previousModelViewProj;

void main()
{
	vec4 position = mul(u_modelViewProj, vec4(a_position, 1.0) );
	vec4 viewPos = mul(u_modelView, vec4(a_position, 1.0) );
	v_viewPosition = viewPos.xyz;

	vec4 normal = a_normal * 2.0 - 1.0;
	v_worldNormal = normalize(mul(u_model[0], vec4(normal.xyz, 0.0)).xyz);

	vec4 previousPosition = mul(u_previousModelViewProj, vec4(a_position, 1.0) );
	v_previousPosition = previousPosition;
	v_currentPosition = position;

	gl_Position = position;
}