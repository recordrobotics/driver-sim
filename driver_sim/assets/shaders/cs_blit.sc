#include <bgfx_compute.sh>

IMAGE2D_RO(src, rgba32f, 0);
IMAGE2D_WO(dest, rgba32f, 1);

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    if (any(greaterThanEqual(pixel, ivec2(u_viewRect.zw))))
        return;

    imageStore(dest, pixel, imageLoad(src, pixel));
}