#include <bgfx_compute.sh>

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

SAMPLER2D(s_velocity, 0);
SAMPLER2D(s_depth, 1);
IMAGE2D_WO(s_tilemax_x, rgba16f, 2);

/*
	float ----;
	float ----;
	float maximum_jitter_value;
	float motion_blur_intensity;
	int tile_size;
	int sample_count;
	int frame;
	int ____;
*/
uniform vec4 u_mbBlurData[2];

NUM_THREADS(16, 16, 1)
void main()
{
    uint tile_size = uint(round(u_mbBlurData[1].x));

    ivec2 render_size = ivec2(textureSize(s_velocity, 0));
	ivec2 output_size = imageSize(s_tilemax_x);
	ivec2 uvi = ivec2(gl_GlobalInvocationID.xy);
	ivec2 global_uvi = uvi * ivec2(tile_size, 1);
	if ((uvi.x >= output_size.x) || (uvi.y >= output_size.y) || (global_uvi.x >= render_size.x) || (global_uvi.y >= render_size.y))  
	{
		return;
	}

	vec2 uvn = (vec2(global_uvi) + vec2_splat(0.5)) / render_size;

	vec4 max_velocity = vec4_splat(0);

	float max_velocity_length = -1;

	for(uint i = 0; i < tile_size; i++)
	{
		vec2 current_uv = uvn + vec2(float(i) / render_size.x, 0);
		vec3 velocity_sample = texture2DLod(s_velocity, current_uv, 0.0).xyz;
		float current_velocity_length = dot(velocity_sample.xy, velocity_sample.xy);
		if(current_velocity_length > max_velocity_length)
		{
			max_velocity_length = current_velocity_length;
			max_velocity = vec4(velocity_sample, texture2DLod(s_depth, current_uv, 0.0).x);
		}
	}
	imageStore(s_tilemax_x, uvi, max_velocity);
}