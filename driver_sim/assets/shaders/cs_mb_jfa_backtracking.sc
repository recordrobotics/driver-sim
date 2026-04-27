#include <bgfx_compute.sh>

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

SAMPLER2D(s_depth, 0);
SAMPLER2D(s_velocity, 1);
SAMPLER2D(s_buffer, 2);
IMAGE2D_WO(s_bufferOut, rgba16f, 3);

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

// Motion similarity 
// ----------------------------------------------------------
float get_motion_difference(vec2 V, vec2 V2)
{
	return clamp(dot(V - V2, V) / dot(V, V), 0, 1);
//	vec2 VO = V - V2;
//	float parallel = dot(VO, V) / dot(V, V);
//	return clamp(parallel, 0, 1);
}
// ----------------------------------------------------------


void sample_fitness(vec2 uv_offset, vec4 uv_sample, vec2 render_size, float motion_blur_intensity, float perpen_error_thresh, inout vec4 current_sample_fitness)
{
	vec2 sample_velocity = -uv_sample.xy;
	
	// if velocity is 0, we never reach it (steps never smaller than 1)
	if (dot(sample_velocity, sample_velocity) <= FLT_MIN || uv_sample.w == 0)
	{
		current_sample_fitness = vec4(FLT_MAX, FLT_MAX, FLT_MAX, 0);
		return;
	}

	// velocity space distance (projected pixel offset onto velocity vector)
	float velocity_space_distance = dot(sample_velocity, uv_offset) / dot(sample_velocity, sample_velocity);
	// the velcity space distance to gravitate the JFA to (found more relieable than doing a 0 - 1 range)
	float mid_point = motion_blur_intensity / 2;
	// centralize the velocity space distance around that mid point
	float absolute_velocity_space_distance = abs(velocity_space_distance - mid_point);
	// if that distance is half the original, its within range (we centered around a mid point)
	float within_velocity_range = step(absolute_velocity_space_distance, mid_point);
	// perpendicular offset
	float side_offset = abs(dot(vec2(uv_offset.y, -uv_offset.x), sample_velocity)) / dot(sample_velocity, sample_velocity);
	// arbitrary perpendicular limit (lower means tighter dilation, but less reliable)
	float within_perpen_error_range = step(side_offset, perpen_error_thresh * motion_blur_intensity);
	// store relevant data for use in conditions
	current_sample_fitness = vec4(absolute_velocity_space_distance, velocity_space_distance, uv_sample.w + uv_sample.z * velocity_space_distance, within_velocity_range * within_perpen_error_range);
}


float is_sample_better(vec4 a, vec4 b)
{
	// see explanation at end of code
	return mix(1. - step(b.x * a.w, a.x * b.w), step(b.z, a.z), step(0.5, b.w) * step(0.5, a.w));
}

// dilation validation and better sample selection
vec4 get_backtracked_sample(vec2 uvn, vec2 chosen_uv, vec3 chosen_velocity, vec4 best_sample_fitness, vec2 render_size, float max_dilation_radius, float motion_blur_intensity, int backtracking_sample_count)
{
	//return vec4(chosen_uv, best_sample_fitness.z, best_sample_fitness.w);// comment this to enable backtracking

	float smallest_step = 1 / max(render_size.x, render_size.y);
	// choose maximum range to check along (matches with implementation in blur stage)
	float general_velocity_multiplier = min(best_sample_fitness.y, max_dilation_radius * smallest_step / (length(chosen_velocity) * motion_blur_intensity));

	vec2 best_uv = chosen_uv;

	float best_multiplier = best_sample_fitness.y;

	float best_depth = best_sample_fitness.z;

	// set temp variable to keet track of better matches
	float smallest_velocity_difference = 0.99;//velocity_match_threshold;
	// minimum amount of valid velocities to compare before decision
	int initial_steps_to_compare = 2;

	int steps_to_compare = initial_steps_to_compare;

	float velocity_multiplier;

	vec2 check_uv;

	vec3 velocity_test;

	float depth_test;

	float velocity_difference;

	float current_depth;

	for(int i = -backtracking_sample_count; i < backtracking_sample_count + 1; i++)
	{
		velocity_multiplier = general_velocity_multiplier * (1 + float(i) /  float(backtracking_sample_count));

		if(velocity_multiplier > motion_blur_intensity || velocity_multiplier < 0)
		{
			continue;
		}

		check_uv = uvn - chosen_velocity.xy * velocity_multiplier;

		if(any(notEqual(check_uv, clamp(check_uv, vec2_splat(0.0), vec2_splat(1.0)))))
		{
			continue;
		}
		// get potential velocity and depth matches
		velocity_test = texture2DLod(s_velocity, check_uv, 0.0).xyz;
		
		depth_test = 1.0-texture2DLod(s_depth, check_uv, 0.0).x;

		velocity_difference = get_motion_difference(chosen_velocity.xy, velocity_test.xy);
		
		current_depth = depth_test + chosen_velocity.z * velocity_multiplier;
		
		// if checked sample matches depth and velocity, it is valid for backtracking
		if((abs(current_depth - best_sample_fitness.z) < 0.002) && (velocity_difference <= smallest_velocity_difference))
		{
			best_uv = check_uv;
			best_multiplier = velocity_multiplier;
			best_depth = current_depth;
			if(steps_to_compare == 0)
			{
				return vec4(best_uv, best_depth, 0);
			}
			steps_to_compare--;
		}
		// if a sample was found and we lost footing after, go with that found sample right away
		else if(initial_steps_to_compare > steps_to_compare)
		{
			return vec4(best_uv, best_depth, 0);
		}
	}

	return vec4(uvn, best_sample_fitness.z, 1);
}


NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 render_size = ivec2(textureSize(s_velocity, 0));
	ivec2 uvi = ivec2(gl_GlobalInvocationID.xy);
	if ((uvi.x >= render_size.x) || (uvi.y >= render_size.y)) 
	{
		return;
	}

    uint iteration_index = uint(u_mbJFAData[0].x);
    uint last_iteration_index = uint(u_mbJFAData[0].y);
    float perpen_error_thresh = u_mbJFAData[0].z;
    float sample_step_multiplier = u_mbJFAData[0].w;
    float motion_blur_intensity = u_mbJFAData[1].x;
    float step_exponent_modifier = u_mbJFAData[1].y;
    float max_dilation_radius = u_mbJFAData[1].z;
    int backtracking_sample_count = int(u_mbJFAData[1].w);

	vec2 uvn = (vec2(uvi) + vec2_splat(0.5)) / render_size;

	vec2 step_size = vec2_splat(round(pow(abs(2 + step_exponent_modifier), last_iteration_index - iteration_index)) * sample_step_multiplier);

	vec2 uv_step = vec2(round(step_size)) / render_size;

	vec4 best_sample_fitness = vec4(FLT_MAX, FLT_MAX, FLT_MAX, 0.);
	
	vec2 chosen_uv = uvn;

    vec3 chosen_velocity = vec3_splat(0.);

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

        if(iteration_index > 0)
		{
            check_uv = texture2DLod(s_buffer, check_uv, 0.0).xy;
            step_offset = check_uv - uvn;
		}

		uv_sample = vec4(texture2DLod(s_velocity, check_uv, 0.0).xyz, 1.0-texture2DLod(s_depth, check_uv, 0.0).x);
		
		sample_fitness(step_offset, uv_sample, render_size, motion_blur_intensity, perpen_error_thresh, current_sample_fitness);

        if (mix(1. - step(best_sample_fitness.x * current_sample_fitness.w, current_sample_fitness.x * best_sample_fitness.w), step(best_sample_fitness.z, current_sample_fitness.z), step(0.5, best_sample_fitness.w) * step(0.5, current_sample_fitness.w)) > 0.5)//is_sample_better(current_sample_fitness, best_sample_fitness) > 0.5)
		{
			best_sample_fitness = current_sample_fitness;
			chosen_uv = check_uv;
			chosen_velocity = uv_sample.xyz;
		}
	}

    if(iteration_index < last_iteration_index)
	{
		imageStore(s_bufferOut, uvi, vec4(chosen_uv, 0, 0));
		return;
	}

    float depth = 1.0-texture2DLod(s_depth, uvn, 0.0).x;

	// best_sample_fitness.z contains the depth of the texture + offset of velocity z
	vec4 backtracked_sample = get_backtracked_sample(uvn, chosen_uv, chosen_velocity, best_sample_fitness, render_size, max_dilation_radius, motion_blur_intensity, backtracking_sample_count);
	
	if(best_sample_fitness.w == 0 || depth > backtracked_sample.z)
	{
		imageStore(s_bufferOut, uvi, vec4(uvn, 0, 0));
		return;
	}

	imageStore(s_bufferOut, uvi, backtracked_sample);
}