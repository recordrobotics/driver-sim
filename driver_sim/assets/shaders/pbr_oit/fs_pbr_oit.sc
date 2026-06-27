$input v_viewPosition, v_worldNormal, v_currentPosition, v_previousPosition, v_worldPosition

#include <bgfx_shader.sh>
#include "../common/utils.sh"
#include "../common/pbr.sh"

uniform vec4 u_baseColor;
uniform vec4 u_emissionColor;
uniform vec4 u_pbrData;

float w(float z, float a) {
    return a * clamp(0.03 / (1e-5 + pow(z / 200.0, 5.0) ), 0.01, 3000.0);
}

void main()
{
    vec4 color = pbr(u_baseColor.rgb, u_emissionColor.rgb, v_worldPosition, mtxGetColumn(u_invView, 3).xyz, v_worldNormal, u_baseColor.a);

    color.rgb *= color.a;

    float z = norm_depth(v_viewPosition);
	gl_FragData[0] = color * w(z, color.a);
	gl_FragData[1] = vec4_splat(color.a);
}