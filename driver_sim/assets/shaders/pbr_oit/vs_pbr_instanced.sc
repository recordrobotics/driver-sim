$input a_position, a_normal, i_data0, i_data1, i_data2, i_data3, i_data4, i_data5
$output v_viewPosition, v_viewNormal, v_currentPosition, v_previousPosition, v_worldPosition

#include <bgfx_shader.sh>

uniform vec4 u_pbrData;
uniform mat4 u_previousViewProj;

void main()
{
	mat4 model = mtxFromRows(i_data0, i_data1, i_data2, vec4(0,0,0,1));
	mat4 prevModel = mtxFromRows(i_data3, i_data4, i_data5, vec4(0,0,0,1));
	
	bool writeMotionVectors = u_pbrData.x > 0.5;

	vec4 worldPosition = mul(model, vec4(a_position, 1.0) );
	vec4 position = mul(u_viewProj, worldPosition);
	vec4 viewPos = mul(u_view, worldPosition);
	v_viewPosition = viewPos.xyz;
	v_worldPosition = worldPosition.xyz;

	mat4 modelView = mul(u_view, model);

	vec3 normal = a_normal.xyz * 2.0 - 1.0;
	v_viewNormal = normalize(mul(modelView, vec4(normal, 0.0))).xyz;

	v_currentPosition = position;

	if(writeMotionVectors) {
		vec4 previousPosition = mul(u_previousViewProj, mul(prevModel, vec4(a_position, 1.0) ));
		v_previousPosition = previousPosition;
	}

	gl_Position = position;
}