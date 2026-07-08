$input a_position, a_normal, a_texcoord0
$output v_viewNormal, v_currentPosition, v_previousPosition, v_texcoord0

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

	vec3 normal = a_normal.xyz * 2.0 - 1.0;
	v_viewNormal = normalize(mul(u_modelView, vec4(normal, 0.0))).xyz;

	v_texcoord0 = a_texcoord0;

	if(writeMotionVectors) {
		vec4 previousWorldPosition = mul(u_model[1], vec4(a_position, 1.0) );
		vec4 previousPosition = mul(u_previousViewProj, previousWorldPosition );
		v_previousPosition = previousPosition;
		v_currentPosition = position;
	}

	gl_Position = position;
}