$input a_position, a_normal, i_data0, i_data1
$output v_viewPosition, v_worldNormal, v_currentPosition, v_previousPosition, v_worldPosition

#include <bgfx_shader.sh>

uniform vec4 u_pbrData;
uniform mat4 u_previousModelViewProj;

mat4 mtxTranslation(vec3 translation)
{
	return mtxFromCols(
		vec4(1.0, 0.0, 0.0, 0.0),
		vec4(0.0, 1.0, 0.0, 0.0),
		vec4(0.0, 0.0, 1.0, 0.0),
		vec4(translation.x, translation.y, translation.z, 1.0)
	);
}

void main()
{
	mat4 model = mtxTranslation(i_data0.xyz);
	mat4 prevModel = mtxTranslation(i_data1.xyz);
	
	bool writeMotionVectors = u_pbrData.x > 0.5;

	vec4 worldPosition = mul(model, vec4(a_position, 1.0) );
	vec4 position = mul(u_viewProj, worldPosition);
	vec4 viewPos = mul(u_view, worldPosition);
	v_viewPosition = viewPos.xyz;
	v_worldPosition = worldPosition.xyz;

	vec4 normal = a_normal * 2.0 - 1.0;
	v_worldNormal = normalize(mul(model, normal)).xyz;

	if(writeMotionVectors) {
		vec4 previousPosition = mul(u_previousModelViewProj, mul(prevModel, vec4(a_position, 1.0) ));
		v_previousPosition = previousPosition;
		v_currentPosition = position;
	}

	gl_Position = position;
}