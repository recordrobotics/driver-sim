#include <bgfx_compute.sh>

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38
#define M_PI 3.1415926535897932384626433832795

SAMPLER2D(s_color, 0);
SAMPLER2D(s_velocity, 1);
SAMPLER2D(s_neighbormax, 2);
SAMPLER2D(s_tilevariance, 3);
IMAGE2D_WO(s_output, rgba16f, 4);

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

vec2 jitter_tile(uint frame, vec2 uvi, vec2 tile_render_size)
{
	float rx, ry;
	float angle = interleaved_gradient_noise(frame, uvi + vec2(2, 0)) * M_PI * 2;
	rx = cos(angle);
	ry = sin(angle);
	return vec2(rx, ry) / tile_render_size / 4;
}

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 render_size = ivec2(textureSize(s_color, 0));
	vec2 tile_render_size_v = vec2(textureSize(s_neighbormax, 0));
    ivec2 tile_render_size = ivec2(tile_render_size_v);
    ivec2 uvi = ivec2(gl_GlobalInvocationID.xy);
    if ((uvi.x >= render_size.x) || (uvi.y >= render_size.y)) 
	{
		return;
	}

    // blur params
    float maximum_jitter_value = u_mbBlurData[0].z;
    float motion_blur_intensity = u_mbBlurData[0].w;

    uint tile_size = uint(round(u_mbBlurData[1].x));
    uint sample_count = uint(round(u_mbBlurData[1].y));
    uint frame = uint(round(u_mbBlurData[1].z));

    vec2 x = (vec2(uvi) + vec2_splat(0.5)) / vec2(render_size);

    vec4 vnzw =  texture2DLod(s_neighbormax, x + vec2_splat(tile_size / 2) / vec2(render_size) + jitter_tile(frame, vec2(uvi), tile_render_size_v), 0.0) * vec4(render_size / 2., 1, 1) * motion_blur_intensity;

	vec2 vn = vnzw.xy;

	float vn_length = length(vn);

	vec4 base_color = texture2DLod(s_color, x, 0.0);

	vec4 vxzw = texture2DLod(s_velocity, x, 0.0) * vec4(vec2(render_size) / 2., 1, 1) * motion_blur_intensity;

	if(vn_length < 0.5)
	{
		imageStore(s_output, uvi, base_color);
		return;
	}

	vec2 wn = safenorm(vn);

	vec2 vx = vxzw.xy;

	float vx_length = max(0.5, length(vx));

	vec2 wx = safenorm(vx);
	
	float j = interleaved_gradient_noise(frame, vec2(uvi)) * 2. - 1.;

	float zx = vxzw.w;

	float weight = 1e-6;

	vec4 sum = base_color * weight;

	float nai_weight = 1e-6;

	vec4 nai_sum = base_color * nai_weight;

	for(uint i = 0; i < sample_count; i++)
	{
		float t = mix(-1.0, 1.0, (i + j * maximum_jitter_value + 1.0) / (sample_count + 1.0));
		
		bool use_vn = ((i % 2) == 0);

		vec2 d = use_vn ? vn : vx;

		float dz = use_vn ? vnzw.z : vxzw.z;

		vec2 wd = use_vn ? wn : wx;

		float T = abs(t * vn_length);

		vec2 y = x + t * d / render_size;

		float wa = abs(dot(wx, wd));
		
		vec4 vyzw = texture2DLod(s_velocity, y, 0.0) * vec4(render_size / 2., 1, 1) * motion_blur_intensity;
		
		vec2 vy = vyzw.xy - dz * t; 
	
		float vy_length = max(0.5, length(vy));

		float zy = vyzw.w;

		float f = z_compare(-zy, -zx, 20000);
		float b = z_compare(-zx, -zy, 20000);

		float wb = abs(dot(vy / vy_length, wd));
		
		if(use_vn)
		{
			float ay = f * step(T, vy_length * wb);

			weight += ay; 

			sum += texture2DLod(s_color, y, 0.0) * ay;
		}

		float nai_ay = b * step(T, vx_length * wa) * 2;

		nai_weight += nai_ay;

		nai_sum += texture2DLod(s_color, y, 0.0) * nai_ay;
	}

	sum /= weight;

	weight /= sample_count / 2;

	nai_sum /= nai_weight;

	sum = mix(nai_sum, sum, weight);

	imageStore(s_output, uvi, sum);
}