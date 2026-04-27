#include <bgfx_compute.sh>

SAMPLER2D(s_tilemax, 0);
SAMPLER2D(s_buffer, 1);
IMAGE2D_WO(s_neighbormax, rgba16f, 2);

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 render_size = ivec2(textureSize(s_tilemax, 0));
	ivec2 uvi = ivec2(gl_GlobalInvocationID.xy);
	if ((uvi.x >= render_size.x) || (uvi.y >= render_size.y)) 
	{
		return;
	}

	vec2 uvn = (vec2(uvi) + vec2_splat(0.5)) / render_size;

	vec2 best_sample_uv = vec2(uvn);

	float max_neighbor_velocity_length = -1;

	for(int i = -1; i < 2; i++)
	{
		for(int j = -1; j < 2; j++)
		{
			vec2 current_offset = vec2_splat(1) / vec2(render_size) * vec2(i, j);
			vec2 current_uv = uvn + current_offset;
			if(current_uv.x < 0 || current_uv.x > 1 || current_uv.y < 0 || current_uv.y > 1)
			{
				continue;
			}

			vec2 velocity_map_sample = texture2DLod(s_buffer, current_uv, 0.0).xy;

			vec4 velocity_sample = texture2DLod(s_tilemax, velocity_map_sample, 0.0);

			float current_velocity_length = dot(velocity_sample.xy, velocity_sample.xy);
			
			if(current_velocity_length > max_neighbor_velocity_length)
			{
				max_neighbor_velocity_length = current_velocity_length;
				best_sample_uv = velocity_map_sample;
			}
		}
	}

	imageStore(s_neighbormax, uvi, vec4(best_sample_uv, 0, 1));
}