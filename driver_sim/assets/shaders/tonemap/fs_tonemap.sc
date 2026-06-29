// https://github.com/Unity-Technologies/Graphics/blob/master/com.unity.postprocessing/PostProcessing/Shaders/Colors.hlsl

$input v_texcoord0

#include <bgfx_shader.sh>
#include "../common/color.sh"

SAMPLER2D(s_tex, 0);
SAMPLER2D(s_lut, 1);

// x = 1 / lut_width
// y = 1 / lut_height
// z = lutHeight - 1
// w = exposureScale
uniform vec4 u_lutParams;

//
// 2D LUT grading
// scaleOffset = (1 / lut_width, 1 / lut_height, lut_height - 1)
//
vec3 applyLUT2D(vec3 uvw, vec3 scaleOffset)
{
    // Strip format where `height = sqrt(width)`
    uvw.z *= scaleOffset.z;
    float shift = floor(uvw.z);
    uvw.xy = uvw.xy * scaleOffset.z * scaleOffset.xy + scaleOffset.xy * 0.5;
    uvw.x += shift * scaleOffset.y;
    uvw.xyz = mix(
        texture2D(s_lut, vec2(uvw.x, 1.0 - uvw.y)).rgb,
        texture2D(s_lut, vec2(uvw.x + scaleOffset.y, 1.0 - uvw.y)).rgb,
        uvw.z - shift
    );
    return uvw;
}

void main()
{
    vec3 linearColor = texture2D(s_tex, v_texcoord0).rgb;

    vec3 logc = saturate(LinearToLogC(linearColor));
    vec3 lutOutput = applyLUT2D(logc, u_lutParams.xyz);

    gl_FragColor = vec4(LinearToSRGB(lutOutput), 1.0);
}