#include <bgfx_compute.sh>

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

SAMPLER2D(s_tilemax, 0);
IMAGE2D_WO(s_neighbormax, rgba16f, 1);

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

	vec2 max_neighbor_velocity = vec2_splat(0);

	float max_neighbor_velocity_length = 0;

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

			bool is_diagonal = (abs(i) + abs(j) == 2);

			vec2 current_neighbor_velocity = texture2DLod(s_tilemax, current_uv, 0.0).xy;

			bool facing_center = dot(current_neighbor_velocity, current_offset) > 0;

			if(is_diagonal && !facing_center)
			{
				continue;
			}

			float current_neighbor_velocity_length = dot(current_neighbor_velocity, current_neighbor_velocity);
			if(current_neighbor_velocity_length > max_neighbor_velocity_length)
			{
				max_neighbor_velocity_length = current_neighbor_velocity_length;
				max_neighbor_velocity = current_neighbor_velocity;
			}
		}
	}

	imageStore(s_neighbormax, uvi, vec4(max_neighbor_velocity, 0, 1));
}