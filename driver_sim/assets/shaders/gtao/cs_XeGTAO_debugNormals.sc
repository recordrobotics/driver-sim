#include <bgfx_compute.sh>

#include "common.sh"

IMAGE2D_RO(s_workingAOTerm, r32ui, 0);
IMAGE2D_WO(dest, rgba16f, 1);

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    if (any(greaterThanEqual(pixel, ivec2(u_viewRect.zw))))
        return;

    uint packedValue = imageLoad(s_workingAOTerm, pixel).x;
    float visibility;
    vec3 bentNormal;
    XeGTAO_DecodeVisibilityBentNormal( packedValue, visibility, bentNormal );

    imageStore(dest, pixel, vec4(bentNormal, 1.0));
}