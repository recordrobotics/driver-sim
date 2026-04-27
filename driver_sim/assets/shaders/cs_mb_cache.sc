#include <bgfx_compute.sh>

SAMPLER2D(s_velocity, 0);
SAMPLER2D(s_color, 1);
IMAGE2D_WO(s_prevVelocity, rgba32f, 2);
IMAGE2D_WO(s_prevColor, rgba8, 3);

// Guertin's functions https://research.nvidia.com/sites/default/files/pubs/2013-11_A-Fast-and/Guertin2013MotionBlur-small.pdf
// ----------------------------------------------------------
float z_compare(float a, float b, float sze)
{
	return clamp(1. - sze * (a - b), 0, 1);
}
// ----------------------------------------------------------

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 render_size = ivec2(textureSize(s_velocity, 0));
	ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
	if ((uv.x >= render_size.x) || (uv.y >= render_size.y)) 
	{
		return;
	}
	
	vec2 x = (vec2(uv) + 0.5) / render_size;

	vec4 past_vx = texture2DLod(s_velocity, x, 0.0);

	vec4 past_vx_vx = texture2DLod(s_velocity, x + past_vx.xy, 0.0);

	vec4 past_col_vx = texture2DLod(s_color, x + past_vx.xy, 0.0);

	vec4 past_col_x = texture2DLod(s_color, x, 0.0);

	float alpha = 1 - z_compare(-past_vx.w, -past_vx_vx.w, 20000);

	vec4 final_past_col = mix(past_col_vx, past_col_x, alpha); 
	
	vec4 final_past_vx = mix(vec4(past_vx_vx.xyz, past_vx.w), past_vx, alpha);

	imageStore(s_prevColor, uv, final_past_col);
	imageStore(s_prevVelocity, uv, final_past_vx);
}