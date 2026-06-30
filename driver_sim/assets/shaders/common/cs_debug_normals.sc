#include <bgfx_compute.sh>

#include "packing.sh"

IMAGE2D_RO(s_normal, r32ui, 0);
IMAGE2D_WO(dest, rgba16f, 1);

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    if (any(greaterThanEqual(pixel, ivec2(u_viewRect.zw))))
        return;

    uint packedInput = imageLoad(s_normal, pixel).x;
    vec3 unpackedOutput = R11G11B10_UNORM_to_FLOAT3( packedInput );
    vec3 normal = normalize(unpackedOutput * vec3_splat(2.0) - vec3_splat(1.0));

    imageStore(dest, pixel, vec4(normal, 1.0));
}