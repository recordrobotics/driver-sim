#include <bgfx_compute.sh>

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

SAMPLER2D(s_tilemax, 0);
SAMPLER2D(s_buffer, 1);
IMAGE2D_WO(s_bufferOut, rgba16f, 2);

uniform vec4 u_mbJFAData[2];

#define KERNEL_SIZE 8

const vec2 check_step_kernel[KERNEL_SIZE] = {
	vec2(-1, 0),
	vec2(1, 0),
	vec2(0, -1),
	vec2(0, 1),
	vec2(-1, 1),
	vec2(1, -1),
	vec2(1, 1),
	vec2(-1, -1),
};

void sample_fitness(vec2 uv_offset, vec4 uv_sample, vec2 render_size, float motion_blur_intensity, float perpen_error_thresh, inout vec4 current_sample_fitness)
{
	vec2 sample_velocity = -uv_sample.xy;
	
	if (dot(sample_velocity, sample_velocity) <= FLT_MIN || uv_sample.w == 0)
	{
		current_sample_fitness = vec4(10, 10, 0, 0);
		return;
	}

	float velocity_space_distance = dot(sample_velocity, uv_offset) / dot(sample_velocity, sample_velocity);

	float mid_point = motion_blur_intensity / 2;

	float absolute_velocity_space_distance = abs(velocity_space_distance - mid_point);

	float within_velocity_range = step(absolute_velocity_space_distance, mid_point);

	float side_offset = abs(dot(vec2(uv_offset.y, -uv_offset.x), sample_velocity)) / dot(sample_velocity, sample_velocity);

	float within_perpen_error_range = step(side_offset, perpen_error_thresh * motion_blur_intensity);

	current_sample_fitness = vec4(absolute_velocity_space_distance, velocity_space_distance, uv_sample.w + uv_sample.z * velocity_space_distance, within_velocity_range * within_perpen_error_range);
}

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 render_size = ivec2(textureSize(s_tilemax, 0));
	ivec2 uvi = ivec2(gl_GlobalInvocationID.xy);
	if ((uvi.x >= render_size.x) || (uvi.y >= render_size.y)) 
	{
		return;
	}

    uint iteration_index = uint(u_mbJFAData[0].x);
    uint last_iteration_index = uint(u_mbJFAData[0].y);
    float perpen_error_thresh = u_mbJFAData[0].z;
    float motion_blur_intensity = u_mbJFAData[1].x;
    float step_exponent_modifier = u_mbJFAData[1].y;

	vec2 uvn = (vec2(uvi) + vec2_splat(0.5)) / render_size;

	vec2 step_size = vec2_splat(round(pow(abs(2 + step_exponent_modifier), last_iteration_index - iteration_index)));

	vec2 uv_step = vec2(round(step_size)) / render_size;

	vec4 best_sample_fitness = vec4(10, 10, 0, 0);
	
	vec2 chosen_uv = uvn;

	vec2 step_offset;

	vec2 check_uv;

	vec4 uv_sample;

	vec4 current_sample_fitness;

	for(int i = 0; i < KERNEL_SIZE; i++)
	{
		step_offset = check_step_kernel[i] * uv_step;
		check_uv = uvn + step_offset;
			
		if(any(notEqual(check_uv, clamp(check_uv, vec2_splat(0.0), vec2_splat(1.0)))))
		{
			continue;
		}

		check_uv = texture2DLod(s_buffer, check_uv, 0.0).xy;

		step_offset = check_uv - uvn;

		uv_sample = texture2DLod(s_tilemax, check_uv, 0.0);
		
		sample_fitness(step_offset, uv_sample, render_size, motion_blur_intensity, perpen_error_thresh, current_sample_fitness);

		float sample_better = 1. - step(current_sample_fitness.z * current_sample_fitness.w, best_sample_fitness.z * best_sample_fitness.w);
		best_sample_fitness = mix(best_sample_fitness, current_sample_fitness, sample_better);
		chosen_uv = mix(chosen_uv, check_uv, sample_better);
	}

	imageStore(s_bufferOut, uvi, vec4(chosen_uv, 0, 0));
}