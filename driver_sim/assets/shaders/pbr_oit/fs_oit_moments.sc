$input v_viewPosition, v_viewNormal, v_currentPosition, v_previousPosition, v_worldPosition

#include <bgfx_shader.sh>
#include "../common/utils.sh"

uniform vec4 u_baseColor;

void main()
{
    float d = -log(1.0 - u_baseColor.a);
    float z = ScreenSpaceToLogDepth(v_currentPosition.z / v_currentPosition.w, CAMERA_NEAR, CAMERA_NEAR_LOG, CAMERA_LINEAR_RANGE_LOG);

    float m1 = d * z;
    float z2 = z * z;
    float m2 = d * z2;
    float m3 = d * z2 * z;
    float m4 = d * z2 * z2;

	gl_FragData[0] = vec4(m1, m2, m3, m4);
	gl_FragData[1] = vec4_splat(d);
}