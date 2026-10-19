#include <bgfx_compute.sh>

#define PI 3.14159265358979323846

SAMPLER2D(s_texFisheye, 0);
IMAGE2D_WO(dest, r11f_g11f_b10f, 1);

uniform vec4 u_fisheyeParams; // x = fovRadians, y = sourceXFovRadians, z = sourceYFovRadians, w = unused

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 resolution = ivec2(u_viewRect.zw);

    if (any(greaterThanEqual(pixel, resolution)))
        return;

    vec2 uv = (vec2(pixel) + 0.5) / vec2(resolution);

    vec2 p = (uv - 0.5) * vec2(
        float(resolution.x) / float(resolution.y),
        1.0
    );

    float r = length(p);
    float thetaMax = 0.5 * u_fisheyeParams.x;

    float maxRadius = 0.5 * length(vec2(resolution)) / float(resolution.y);
    float theta = r * thetaMax / maxRadius;

    if (r > maxRadius)
    {
        imageStore(dest, pixel, vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }

    float sinTheta = sin(theta);
    vec3 ray = vec3(0.0, 0.0, cos(theta));

    if (r > 1e-6)
        ray.xy = (p / r) * sinTheta;


    vec2 sourceUV = vec2(
        0.5 + ray.x / (2.0 * ray.z * tan(u_fisheyeParams.y * 0.5)),
        0.5 + ray.y / (2.0 * ray.z * tan(u_fisheyeParams.z * 0.5))
    );

    if (ray.z <= 0.0 ||
        any(lessThan(sourceUV, vec2_splat(0.0))) ||
        any(greaterThan(sourceUV, vec2_splat(1.0))))
    {
        imageStore(dest, pixel, vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }

    vec4 color = texture2DLod(s_texFisheye, sourceUV, 0.0);
    imageStore(dest, pixel, color);
}