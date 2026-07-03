$input v_viewPosition, v_viewNormal, v_currentPosition, v_previousPosition, v_worldPosition

#include <bgfx_shader.sh>
#include "../common/utils.sh"
#include "pbr.sh"

uniform vec4 u_baseColor;
uniform vec4 u_emissionColor;
uniform vec4 u_pbrData;

float w(float z, float a) {
    return a * clamp(0.03 / (1e-5 + pow(z / 200.0, 5.0) ), 0.01, 3000.0);
}

void main()
{
    vec4 color = pbr(u_baseColor.rgb, u_emissionColor.rgb * u_emissionColor.a, v_viewPosition, v_viewNormal, u_baseColor.a, u_pbrData.y, u_pbrData.z, 1.0, 1.0);

    color.rgb *= color.a;

    float z = ScreenSpaceToLinearDepth(v_viewPosition.z);
	gl_FragData[0] = color * w(z, color.a);
	gl_FragData[1] = vec4_splat(color.a);
}