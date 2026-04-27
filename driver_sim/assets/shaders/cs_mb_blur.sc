#include <bgfx_compute.sh>

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38
#define M_PI 3.1415926535897932384626433832795

SAMPLER2D(s_color, 0);
SAMPLER2D(s_velocity, 1);
SAMPLER2D(s_neighbormax, 2);
IMAGE2D_WO(s_output, rgba32f, 3);
SAMPLER2D(s_tilemax, 4);
SAMPLER2D(s_prevColor, 5);
SAMPLER2D(s_prevVelocity, 6);

uniform vec4 u_mbBlurData[2];

// McGuire's functions https://docs.google.com/document/d/1IIlAKTj-O01hcXEdGxTErQbCHO9iBmRx6oFUy_Jm0fI/edit
// ----------------------------------------------------------
float soft_depth_compare(float depth_X, float depth_Y, float sze)
{
	return clamp(1 - (depth_X - depth_Y) / sze, 0, 1);
}

float cone(float T, float v)
{
	return clamp(1 - T / v, 0, 1);
}

float cylinder(float T, float v)
{
	return 1.0 - smoothstep(0.95 * v, 1.05 * v, T);
}
// ----------------------------------------------------------

// Guertin's functions https://research.nvidia.com/sites/default/files/pubs/2013-11_A-Fast-and/Guertin2013MotionBlur-small.pdf
// ----------------------------------------------------------
float z_compare(float a, float b, float sze)
{
	return clamp(1. - sze * (a - b), 0, 1);
}
// ----------------------------------------------------------

// from https://www.shadertoy.com/view/ftKfzc
// ----------------------------------------------------------
float interleaved_gradient_noise(uint frame, vec2 uv){
	uv += float(frame)  * (vec2(47, 17) * 0.695);

    vec3 magic = vec3( 0.06711056, 0.00583715, 52.9829189 );

    return fract(magic.z * fract(dot(uv, magic.xy)));
}
// ----------------------------------------------------------

// from https://github.com/bradparks/KinoMotion__unity_motion_blur/tree/master
// ----------------------------------------------------------
vec2 safenorm(vec2 v)
{
	float l = max(length(v), 1e-6);
	return v / l * int(l >= 0.5);
}

vec2 jitter_tile(uint frame, vec2 uvi)
{
	float rx, ry;
	float angle = interleaved_gradient_noise(frame, uvi + vec2(2, 0)) * M_PI * 2;
	rx = cos(angle);
	ry = sin(angle);
	return vec2(rx, ry) / textureSize(s_tilemax, 0) / 4;
}
// ----------------------------------------------------------

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

	vec2 x = (vec2(uvi) + vec2_splat(0.5)) / vec2(render_size);

	vec2 velocity_map_sample = texture2DLod(s_neighbormax, x + jitter_tile(frame, uvi), 0.0).xy;

	vec3 vnz = texture2DLod(s_tilemax, velocity_map_sample, 0.0).xyz * vec3(render_size, 1);
	
	float vn_length = max(0.5, length(vnz.xy));

	float multiplier = clamp(vn_length, 0, min(max_dilation_radius, vn_length * motion_blur_intensity)) / max(FLT_MIN, vn_length);

	vnz.xyz *= multiplier;

	vn_length *= multiplier;

	vec2 vn = vnz.xy;
	
	vec4 col_x = texture2DLod(s_color, x, 0.0);

	vec4 vxz = texture2DLod(s_velocity, x, 0.0) * vec4(render_size, 1, 1);
	
	float vx_length = max(0.5, length(vxz.xy));
	
	//multiplier = clamp(vx_length, 0, min(max_dilation_radius, vn_length * motion_blur_intensity)) / max(FLT_MIN, vx_length);
	
	vxz.xyz *= multiplier;

	vx_length *= multiplier;
	
	vec2 vx = vxz.xy;
	
	vec2 wx = safenorm(vx);

	if(vn_length <= 0.5)
	{
		imageStore(s_output, uvi, col_x);
		return;
	}

	vec3 wvnz = normalize(vnz.xyz);

	float velocity_match = pow(clamp(dot(vx, vn) / dot(vn, vn), 0, 1), 1 / (10000 * pow(abs(vnz.z), 2)));

	vn = mix(vn, vx, velocity_match);

	vnz = mix(vnz, vxz.xyz, velocity_match);
	
	vec2 wn = safenorm(vn);

	float zx = vxz.w;
	
	float j = interleaved_gradient_noise(frame, uvi) - 0.5;

	vec4 past_vxz = texture2DLod(s_prevVelocity, x, 0.0) * vec4(render_size * multiplier, 1 * multiplier, 1);

	vec2 past_vx = past_vxz.xy;

	vec4 past_col_x = texture2DLod(s_prevColor, x, 0.0);

	float t;
	float back_t;
	float T;
	vec2 y;
	float y_inside;
	vec4 nai_vy;
	vec2 nai_y;
	vec2 nai_back_y;
	float nai_zy;
	float nai_b;
	float nai_ay;
	float nai_y_inside;
	vec4 vy;
	float vy_length;
	float zy;
	float f;
	float wa;
	float ay_trail;
	float past_t;
	vec2 past_y;
	vec2 past_back_y;
	float past_ay;
	float alpha;
	vec4 past_vy;
	float past_zy;
	float past_b;
	float past_y_inside;
	float nai_T;
	float nai_vy_length;
	float nai_wa;

	float weight = 1e-5;

	vec4 sum = col_x * weight;

	float nai_weight = 1e-5;

	float nai_sub_weight = 1e-5;

	vec4 nai_sum = col_x * nai_sub_weight;

	float past_weight = 1e-5;

	float past_sub_weight = 1e-5;

	vec4 past_sum = past_col_x * past_sub_weight;

	float final_sample_count = motion_blur_samples + 1e-5;

	for(uint i = 0; i < motion_blur_samples; i++)
	{
		t = mix(0., -1., (i + j + 1.0) / (motion_blur_samples + 1.0));

		back_t = mix(1, 0, (i + j + 1.0) / (motion_blur_samples + 1.0));

		T = abs(t * vn_length);

		y = x + (vn / render_size) * t;
		
		nai_T = abs(t * vx_length);

		nai_y = x + (vx / render_size) * t;

		nai_back_y = x + (vx / render_size) * back_t;

		nai_vy = texture2DLod(s_velocity, nai_y, 0.0) * vec4(render_size * multiplier, 1 * multiplier, 1);
		
		nai_vy_length = max(0.5, length(nai_vy.xy));

		nai_zy = nai_vy.w - vxz.z * t;
		
		nai_b = z_compare(-zx, -nai_zy, 20000);

		float nai_f = z_compare(-nai_zy, -zx, 20000);
		
		nai_wa = abs(max(0, dot(nai_vy.xy / vy_length, wx)));
		
		nai_ay = max(nai_b, 0);//step(nai_T, nai_vy_length * nai_wa));

		nai_weight += 1;

		nai_sub_weight += 1;

		nai_y_inside = step(0, nai_y.x) * step(nai_y.x, 1) * step(0, nai_y.y) * step(nai_y.y, 1);

		nai_sum += mix(texture2DLod(s_color, nai_back_y, 0.0), texture2DLod(s_color, nai_y, 0.0), nai_ay * nai_y_inside);
		
		past_y = x + (past_vx / render_size) * t;

		past_back_y = x + (past_vx / render_size) * back_t;

		alpha = z_compare(-past_vxz.w, -vxz.w, 20000);

		past_vy = texture2DLod(s_prevVelocity, past_y, 0.0) * vec4(render_size * multiplier, 1 * multiplier, 1);

		past_zy = past_vy.w - past_vxz.z * t;

		past_b = z_compare(-past_vxz.w, -past_zy, 20000);

		past_ay = (1 - step(nai_T, nai_vy_length * nai_wa)) * (1 - alpha);

		past_weight += past_ay;

		past_sub_weight += 1;

		past_y_inside = step(0, past_y.x) * step(past_y.x, 1) * step(0, past_y.y) * step(past_y.y, 1);

		past_sum += mix(texture2DLod(s_prevColor, past_back_y, 0.0), texture2DLod(s_prevColor, past_y, 0.0), past_b * past_y_inside);

		vy = texture2DLod(s_velocity, y, 0.0) * vec4(render_size * multiplier, 1 * multiplier, 1);

		vy_length = max(0.5, length(vy.xy));

		zy = vy.w - vnz.z * t;

		f = z_compare(-zy, -zx, 20000);

		wa = abs(max(0, dot(vy.xy / vy_length, wn)));

		ay_trail = f * step(T, vy_length * wa);

		y_inside = step(0, y.x) * step(y.x, 1) * step(0, y.y) * step(y.y, 1);

		weight += ay_trail * y_inside;

		sum += texture2DLod(s_color, y, 0.0) * ay_trail * y_inside;
	}

	sum /= weight;

	weight /= final_sample_count;

	nai_sum /= nai_sub_weight;

	nai_weight /= final_sample_count;

	past_sum /= past_sub_weight;

	past_weight /= final_sample_count;
	
	sum = mix(mix(nai_sum, past_sum, past_weight), sum, weight);
	
	imageStore(s_output, uvi, sum);
}