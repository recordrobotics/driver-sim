#include <bgfx_compute.sh>

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38
#define DBL_MAX 1.7976931348623158e+308
#define DBL_MIN 2.2250738585072014e-308

SAMPLER2D(s_color, 0);
SAMPLER2D(s_depth, 1);
SAMPLER2D(s_velocity, 2);
SAMPLER2D(s_buffer, 3);
IMAGE2D_WO(s_output, rgba8, 4);

uniform vec4 u_mbBlurData[2];
// near plane distance
float npd = 0.1;

// SOFT_Z_EXTENT
float sze = 0.1;

// Helper functions
// --------------------------------------------
// from https://www.shadertoy.com/view/ftKfzc
float interleaved_gradient_noise(vec2 uv, int FrameId){
	uv += float(FrameId)  * (vec2(47, 17) * 0.695);

    vec3 magic = vec3( 0.06711056, 0.00583715, 52.9829189 );

    return fract(magic.z * fract(dot(uv, magic.xy)));
}
float get_motion_difference(vec2 V, vec2 V2, float power)
{
	vec2 VO = V - V2;
	float difference = dot(VO, V) / max(FLT_MIN, dot(V, V));
	return pow(clamp(difference, 0, 1), power);
}
// McGuire's function https://docs.google.com/document/d/1IIlAKTj-O01hcXEdGxTErQbCHO9iBmRx6oFUy_Jm0fI/edit
float soft_depth_compare(float depth_X, float depth_Y)
{
	return clamp(1 - (depth_X - depth_Y) / sze, 0, 1);
}
// -------------------------------------------------------

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 render_size = ivec2(textureSize(s_color, 0));
	ivec2 uvi = ivec2(gl_GlobalInvocationID.xy);
	if ((uvi.x >= render_size.x) || (uvi.y >= render_size.y)) 
	{
		return;
	}

    uint motion_blur_samples = uint(u_mbBlurData[0].x);
    float motion_blur_intensity = u_mbBlurData[0].y;
    uint frame = uint(u_mbBlurData[0].w);
    float max_dilation_radius = u_mbBlurData[1].w;

	// must be on pixel center for whole values (tested)
	vec2 uvn = vec2(uvi + vec2_splat(0.5)) / render_size;

	vec4 base_color = texture2DLod(s_color, uvn, 0.0);
	// get dominant velocity data
	vec4 velocity_map_sample = texture2DLod(s_buffer, uvn, 0.0);

	vec3 dominant_velocity = -texture2DLod(s_velocity, velocity_map_sample.xy, 0.0).xyz;

	vec3 naive_velocity = -texture2DLod(s_velocity, uvn, 0.0).xyz;
	// if velocity is 0 and we dont show debug, return right away.
	if ((dot(dominant_velocity, dominant_velocity) == 0 || motion_blur_intensity == 0))
	{
		imageStore(s_output, uvi, base_color);
		return;
	}
	// offset along velocity to blend between sample steps
	float noise_offset = interleaved_gradient_noise(uvi, int(frame)) - 1;
	// scale of step
	float velocity_step_coef = min(motion_blur_intensity, max_dilation_radius / max(render_size.x, render_size.y) / (length(dominant_velocity) * motion_blur_intensity)) / max(1.0, motion_blur_samples - 1.0);

	vec3 step_sample = dominant_velocity * velocity_step_coef;

	vec3 naive_step_sample = naive_velocity * velocity_step_coef;

	vec4 velocity_map_step_sample = vec4_splat(0);

	//float d = 1.0 - min(1.0, 2.0 * distance(uvn, vec2_splat(0.5)));
	//sample_step *= 1.0 - d * fade_padding.x;

	float total_weight = 1;
	
	vec3 dominant_offset = step_sample * noise_offset;
	
	vec3 naive_offset = naive_step_sample * noise_offset;

	vec3 dominant_back_offset = -step_sample * (1. - noise_offset);

	vec4 col = base_color * total_weight;

	float naive_depth = 1.0 - texture2DLod(s_depth, uvn, 0.0).x;

	float backstepping_coef = clamp(length(dominant_velocity) / 0.05, 0, 1);

	vec2 dominant_uvo;

	vec2 naive_uvo;

	vec3 current_dominant_offset;

	float current_naive_depth;

	float foreground;

	vec3 current_dominant_velocity;

	float motion_difference;

	float sample_weight;

	float dominant_naive_mix;
	
	vec2 sample_uv;

	for (uint i = 1; i < motion_blur_samples; i++) 
	{
		dominant_offset += step_sample;

		naive_offset += naive_step_sample;

		dominant_uvo = uvn + dominant_offset.xy;

		naive_uvo = uvn + naive_offset.xy;
		
		current_dominant_offset = dominant_offset;

		current_naive_depth = 1.0 - texture2DLod(s_depth, dominant_uvo, 0.0).x;
		// is current depth closer than origin of dilation (stepped into a foreground object)
		foreground = step(naive_depth + current_dominant_offset.z, current_naive_depth - 0.0001);
		
		velocity_map_step_sample = texture2DLod(s_buffer, dominant_uvo, 0.0);

		current_dominant_velocity = -texture2DLod(s_velocity, velocity_map_step_sample.xy, 0.0).xyz;
		
		motion_difference = get_motion_difference(dominant_velocity.xy, current_dominant_velocity.xy, 0.1);
		
		sample_weight = 1;
		
		if (any(notEqual(dominant_uvo, clamp(dominant_uvo, vec2_splat(0.0), vec2_splat(1.0)))) || foreground * motion_difference > 0.5) 
		{
			dominant_uvo = uvn + dominant_back_offset.xy;
			current_dominant_offset = dominant_back_offset;
			dominant_back_offset -= step_sample; 
			sample_weight = 0.5;//backstepping_coef;
		}
		
		velocity_map_step_sample = texture2DLod(s_buffer, dominant_uvo, 0.0);

		current_dominant_velocity = -texture2DLod(s_velocity, velocity_map_step_sample.xy, 0.0).xyz;
		// is current velocity different than dilated velocity		
		
		current_naive_depth = 1.0 - texture2DLod(s_depth, dominant_uvo, 0.0).x;
		// is current depth closer than origin of dilation (stepped into a foreground object)
		foreground = step(naive_depth + current_dominant_offset.z, current_naive_depth - 0.002);
		
		motion_difference = get_motion_difference(dominant_velocity.xy, current_dominant_velocity.xy, 0.1);
		// if we are sampling a foreground object and its velocity is different, discard this sample (prevent ghosting)
		sample_weight *= 1 - (foreground * motion_difference);

		dominant_naive_mix = 1. - step(0.9, motion_difference);

		sample_uv = mix(naive_uvo, dominant_uvo, dominant_naive_mix);

		total_weight += sample_weight;

		col += texture2DLod(s_color, sample_uv, 0.0) * sample_weight;
	}

	col /= total_weight;

	imageStore(s_output, uvi, col);
}