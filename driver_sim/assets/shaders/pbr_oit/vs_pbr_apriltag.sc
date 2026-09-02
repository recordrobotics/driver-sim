$input a_position, a_normal, a_texcoord0, i_data0, i_data1, i_data2, i_data3
$output v_viewNormal, v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    uint id = uint(i_data3.w);
	mat4 model = mtxFromCols(i_data0, i_data1, i_data2, vec4(i_data3.xyz, 1.0));

	vec4 worldPosition = mul(model, vec4(a_position, 1.0) );
	vec4 position = mul(u_viewProj, worldPosition);

	mat4 modelView = mul(u_view, model);

	vec3 normal = a_normal.xyz * 2.0 - 1.0;
	v_viewNormal = normalize(mul(modelView, vec4(normal, 0.0))).xyz;

    vec2 gridSize = vec2_splat(10.0 / 250.0);
    uint gridX = id % uint(25);
    uint gridY = id / uint(25);
    vec2 gridOffset = vec2(float(gridX), float(gridY)) * gridSize;
    v_texcoord0 = a_texcoord0 * gridSize + gridOffset;

	gl_Position = position;
}