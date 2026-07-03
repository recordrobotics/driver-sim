// https://github.com/Unity-Technologies/Graphics/blob/master/com.unity.postprocessing/PostProcessing/Shaders/Colors.hlsl

#include <bgfx_compute.sh>
#include "../common/color.sh"

IMAGE2D_RW(s_image, rgba16f, 0);
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
        texture2DLod(s_lut, vec2(uvw.x, 1.0 - uvw.y), 0).rgb,
        texture2DLod(s_lut, vec2(uvw.x + scaleOffset.y, 1.0 - uvw.y), 0).rgb,
        uvw.z - shift
    );
    return uvw;
}

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    if (any(greaterThanEqual(pixel, ivec2(u_viewRect.zw))))
        return;

    vec3 linearColor = imageLoad(s_image, pixel).rgb;

    vec3 logc = saturate(LinearToLogC(linearColor));
    vec3 lutLinear = applyLUT2D(logc, u_lutParams.xyz);

    imageStore(s_image, pixel, vec4(lutLinear, 1.0));
}