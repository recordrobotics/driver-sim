#include <bgfx_compute.sh>

SAMPLER2D(s_tilemax_x, 0);
IMAGE2D_WO(s_tilemax, rgba16f, 1);

uniform vec4 u_mbSampleStepMultiplier;

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 render_size = ivec2(textureSize(s_tilemax_x, 0));
	ivec2 output_size = imageSize(s_tilemax);
	ivec2 uvi = ivec2(gl_GlobalInvocationID.xy);
	uint tile_size = (uint)render_size.y / (uint)output_size.y;
	ivec2 global_uvi = uvi * ivec2(1, tile_size);
	if ((uvi.x >= output_size.x) || (uvi.y >= output_size.y) || (global_uvi.x >= render_size.x) || (global_uvi.y >= render_size.y)) 
	{
		return;
	}

	vec2 uvn = (vec2(global_uvi) + vec2_splat(0.5)) / render_size;

	vec4 max_velocity = vec4_splat(0);

	float max_velocity_length = -1;

	//bool foreground = false;

	for(uint i = 0; i < tile_size; i++)
	{
		vec2 current_uv = uvn + vec2(0, float(i) / render_size.y);
		vec4 velocity_sample = texture2DLod(s_tilemax_x, current_uv, 0.0);
		float current_velocity_length = dot(velocity_sample.xy, velocity_sample.xy);
		if(current_velocity_length > max_velocity_length)// || (velocity_sample.w > 0 && !foreground))
		{
			max_velocity_length = current_velocity_length;
			max_velocity = vec4(velocity_sample);
//			if(velocity_sample.w > 0)
//			{
//				foreground = true;
//			}
		}
	}
	imageStore(s_tilemax, uvi, max_velocity);
}