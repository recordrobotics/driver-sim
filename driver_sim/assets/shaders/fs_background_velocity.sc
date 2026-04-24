$input v_texcoord0

#include <bgfx_shader.sh>

uniform mat4 u_previousModelViewProj;
uniform vec4 u_jitter;

void main()
{
    vec2 ndc = v_texcoord0.xy * 2.0 - 1.0;
    ndc.y *= -1.0;

    vec4 pt1 = mul(u_invViewProj, vec4(ndc, 0.8, 1.0));
    vec4 pt2 = mul(u_invViewProj, vec4(ndc, 0.2, 1.0));
    
    vec3 world1 = pt1.xyz / pt1.w;
    vec3 world2 = pt2.xyz / pt2.w;
    
    vec3 world_dir = normalize(world2 - world1);

    vec4 prev_clip = mul(u_previousModelViewProj, vec4(world_dir, 0.0));
    vec2 prev_ndc = prev_clip.xy / prev_clip.w;

    vec2 velocity = (ndc - u_jitter.xy) - (prev_ndc - u_jitter.zw);

    gl_FragData[0] = vec4(velocity, 0.0, 1.0);
}