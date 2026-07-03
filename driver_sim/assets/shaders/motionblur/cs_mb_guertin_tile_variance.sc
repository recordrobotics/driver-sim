#include <bgfx_compute.sh>

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

SAMPLER2D(s_tilemax, 0);
IMAGE2D_WO(s_tilevariance, r16f, 1);

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

	float variance = 0;

	vec2 current_velocity = abs(normalize(texture2DLod(s_tilemax, uvn, 0.0).xy));

	float tile_count = 0;

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
			if(i == j && i == 0)
			{
				continue;
			}
			
			tile_count += 1;

			vec2 current_neighbor_velocity = abs(normalize(texture2DLod(s_tilemax, current_uv, 0.0).xy));

			variance += dot(current_velocity, current_neighbor_velocity);
		}
	}

	variance /= tile_count;

	imageStore(s_tilevariance, uvi, vec4_splat(1 - variance));
}