#include <bgfx_compute.sh>

IMAGE2D_RO(s_input, r11f_g11f_b10f, 0);
IMAGE2D_WO(s_output, r11f_g11f_b10f, 1);

// x = 1 / lut_width
// y = 1 / lut_height
// z = lutHeight - 1
// w = exposureScale
uniform vec4 u_lutParams;

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    if (any(greaterThanEqual(pixel, ivec2(u_viewRect.zw))))
        return;

    vec4 color = imageLoad(s_input, pixel);
    color.rgb *= u_lutParams.w;
    imageStore(s_output, pixel, color);
}