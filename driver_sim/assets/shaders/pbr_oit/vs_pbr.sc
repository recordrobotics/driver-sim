$input a_position, a_normal
$output v_viewPosition, v_viewNormal, v_currentPosition, v_previousPosition, v_worldPosition

// using bones for current/previous model matrices
#define BGFX_CONFIG_MAX_BONES 2

#include <bgfx_shader.sh>

uniform vec4 u_pbrData;
uniform mat4 u_previousViewProj;

void main()
{
	bool writeMotionVectors = u_pbrData.x > 0.5;

	vec4 worldPosition = mul(u_model[0], vec4(a_position, 1.0) );
	vec4 position = mul(u_viewProj, worldPosition);
	vec4 viewPos = mul(u_modelView, vec4(a_position, 1.0) );
	v_viewPosition = viewPos.xyz;

	vec3 normal = a_normal.xyz * 2.0 - 1.0;
	v_viewNormal = normalize(mul(u_modelView, vec4(normal, 0.0))).xyz;

	if(writeMotionVectors) {
		vec4 previousPosition = mul(u_previousViewProj, mul(u_model[1], vec4(a_position, 1.0) ));
		v_previousPosition = previousPosition;
	}

	v_currentPosition = position;

	v_worldPosition = worldPosition.xyz;

	gl_Position = position;
}