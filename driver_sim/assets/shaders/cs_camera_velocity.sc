#include <bgfx_compute.sh>

IMAGE2D_WO(s_velocity, rg16f, 0);
IMAGE2D_RO(s_depth, r32f, 1);

uniform mat4 u_previousModelViewProj;
uniform vec4 u_jitter;

NUM_THREADS(8, 8, 1)
void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    if (any(greaterThanEqual(pixel, ivec2(u_viewRect.zw))))
        return;

    float depth = imageLoad(s_depth, pixel).r;

    if(depth >= 1e-6)
        return;

    vec2 uv = (vec2(pixel) + 0.5) / u_viewRect.zw;

    vec2 ndc = uv * 2.0 - 1.0;
    ndc.y *= -1.0;

    vec4 pt1 = mul(u_invViewProj, vec4(ndc, 0.8, 1.0));
    vec4 pt2 = mul(u_invViewProj, vec4(ndc, 0.2, 1.0));
    
    vec3 world1 = pt1.xyz / pt1.w;
    vec3 world2 = pt2.xyz / pt2.w;
    
    vec3 world_dir = normalize(world2 - world1);

    vec4 prev_clip = mul(u_previousModelViewProj, vec4(world_dir, 0.0));
    vec2 prev_ndc = prev_clip.xy / prev_clip.w;

    vec2 velocity = (ndc - u_jitter.xy) - (prev_ndc - u_jitter.zw);

    imageStore(s_velocity, pixel, vec4(velocity, 0.0, 1.0));
}