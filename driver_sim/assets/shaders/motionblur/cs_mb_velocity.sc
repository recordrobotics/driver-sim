#include <bgfx_compute.sh>

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38

SAMPLER2D(s_depth, 0);
SAMPLER2D(s_velocity, 1);
IMAGE2D_WO(s_output, rgba16f, 2);
IMAGE2D_WO(s_full_velocity_output, rgba16f, 3);

uniform vec4 u_mbVelocityData[3];

uniform mat4 u_previousView;
uniform mat4 u_previousProj;
uniform vec4 u_jitter;

float sharp_step(float lower, float upper, float x)
{
	return clamp((x - lower) / (upper - lower), 0, 1);
}

float get_view_depth(float depth)
{
	return 0.;
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

    float rotation_velocity_multiplier = u_mbVelocityData[0].x;
    float movement_velocity_multiplier = u_mbVelocityData[0].y;
    float object_velocity_multiplier = u_mbVelocityData[0].z;
    float rotation_velocity_lower_threshold = u_mbVelocityData[0].w;
    float movement_velocity_lower_threshold = u_mbVelocityData[1].x;
    float object_velocity_lower_threshold = u_mbVelocityData[1].y;
    float rotation_velocity_upper_threshold = u_mbVelocityData[1].z;
    float movement_velocity_upper_threshold = u_mbVelocityData[1].w;
    float object_velocity_upper_threshold = u_mbVelocityData[2].x;
    float is_fsr2 = 0.0f;
    float motion_blur_intensity = u_mbVelocityData[2].y;

	// must be on pixel center for whole values (tested)
	vec2 uvn = vec2(uvi + vec2_splat(0.5)) / render_size;
	
	float depth = texture2DLod(s_depth, uvn, 0.0).x;

    // if(depth == 0) {
    //     imageStore(s_output, uvi, vec4(0.0, 0.0, 0.0, depth));
    //     imageStore(s_full_velocity_output, uvi, vec4(0.0, 0.0, 0.0, depth));
    //     return;
    // }

    vec3 current_ndc = vec3(uvn * 2.0 - 1.0, depth);
    current_ndc.y *= -1.0;
    vec3 current_uv = vec3((current_ndc.xy - u_jitter.xy) * 0.5 + 0.5, current_ndc.z);

    vec4 view_past_ndc;

    if(depth == 0.0) {
        vec4 pt1 = mul(u_invViewProj, vec4(current_ndc.xy, 0.8, 1.0));
        vec4 pt2 = mul(u_invViewProj, vec4(current_ndc.xy, 0.2, 1.0));
        
        vec3 world1 = pt1.xyz / pt1.w;
        vec3 world2 = pt2.xyz / pt2.w;
        
        vec3 world_dir = normalize(world2 - world1);

        view_past_ndc = mul(u_previousProj, mul(u_previousView, vec4(world_dir, 0.0)));
    } else {
        vec4 view_position = mul(u_invProj, vec4(current_ndc, 1.0));

        view_position.xyz /= view_position.w;

        // get full change 
        vec4 world_local_position = mul(u_invView, vec4(view_position.xyz, 1.0));

        vec4 view_past_position = mul(u_previousView, vec4(world_local_position.xyz, 1.0));
        
        view_past_ndc = mul(u_previousProj, view_past_position);
    }

    view_past_ndc.xyz /= view_past_ndc.w;

    vec3 past_uv = vec3((view_past_ndc.xy - u_jitter.zw) * 0.5 + 0.5, view_past_ndc.z);

    vec4 view_past_ndc_cache = view_past_ndc;

    vec3 camera_uv_change = past_uv - current_uv;

    vec3 camera_rotation_uv_change;

    if(depth > 0) {
        // get just rotation change
#if BGFX_SHADER_LANGUAGE_GLSL
        mat3 invViewRot = mat3(u_invView);
        mat3 previousViewRot = mat3(u_previousView);
#else
        mat3 invViewRot = (mat3)u_invView;
        mat3 previousViewRot = (mat3)u_previousView;
#endif
        vec4 view_position = mul(u_invProj, vec4(current_ndc, 1.0));
        view_position.xyz /= view_position.w;

        vec4 world_local_position = mul(mtxFromRows(
            vec4(mtxGetRow(invViewRot, 0), 0),
            vec4(mtxGetRow(invViewRot, 1), 0),
            vec4(mtxGetRow(invViewRot, 2), 0),
            vec4(0, 0, 0, 1)
        ), vec4(view_position.xyz, 1.0));

        vec4 view_past_position = mul(mtxFromRows(
            vec4(mtxGetRow(previousViewRot, 0), 0),
            vec4(mtxGetRow(previousViewRot, 1), 0),
            vec4(mtxGetRow(previousViewRot, 2), 0),
            vec4(0, 0, 0, 1)
        ), vec4(world_local_position.xyz, 1.0));
        
        view_past_ndc = mul(u_previousProj, view_past_position);

        view_past_ndc.xyz /= view_past_ndc.w;

        past_uv = vec3((view_past_ndc.xy - u_jitter.zw) * 0.5 + 0.5, view_past_ndc.z);

        camera_rotation_uv_change = past_uv - current_uv;
    } else {
        camera_rotation_uv_change = camera_uv_change;
    }

	// get just movement change
	vec3 camera_movement_uv_change = camera_uv_change - camera_rotation_uv_change;
	// fill in gaps in base velocity (skybox, z velocity)
    vec4 velocity_sample = texture2DLod(s_velocity, uvn, 0.0);
	vec3 base_velocity = vec3(velocity_sample.xy + mix(vec2_splat(0), camera_uv_change.xy, 1.0 - velocity_sample.a /* blend between buffer and camera velocity based on blend factor */), camera_uv_change.z);
	// fsr just makes it so values are larger than 1, I assume its the only case when it happens
	if(is_fsr2 > 0.5 && dot(base_velocity.xy, base_velocity.xy) >= 1)
	{
		base_velocity = camera_uv_change;
	}

	// get object velocity
	vec3 object_uv_change = base_velocity - camera_uv_change.xyz;

	// construct final velocity with user defined weights
	vec3 total_velocity = camera_rotation_uv_change * rotation_velocity_multiplier * sharp_step(rotation_velocity_lower_threshold, rotation_velocity_upper_threshold, length(camera_rotation_uv_change) * rotation_velocity_multiplier * motion_blur_intensity)
	+ camera_movement_uv_change * movement_velocity_multiplier * sharp_step(movement_velocity_lower_threshold, movement_velocity_upper_threshold, length(camera_movement_uv_change) * movement_velocity_multiplier * motion_blur_intensity)
	+ object_uv_change * object_velocity_multiplier * sharp_step(object_velocity_lower_threshold, object_velocity_upper_threshold, length(object_uv_change) * object_velocity_multiplier * motion_blur_intensity);
	// if objects move, clear z direction, (z only correct for static environment)
	if(dot(object_uv_change.xy, object_uv_change.xy) > 0.000001)
	{
		total_velocity.z = 0;
		base_velocity.z = 0;
	}
	// choose the smaller option out of the two based on amgnitude, seems to work well
	if(dot(total_velocity.xy * 99, total_velocity.xy * 100) >= dot(base_velocity.xy * 100, base_velocity.xy * 100))
	{
		total_velocity = base_velocity;
	}

	float total_velocity_length = max(FLT_MIN, length(total_velocity));
	total_velocity = total_velocity * clamp(total_velocity_length, 0, 1) / total_velocity_length;

    base_velocity.y *= -1; // flip y
    total_velocity.y *= -1; // flip y

	imageStore(s_output, uvi, vec4(total_velocity * (view_past_ndc_cache.w < 0 ? -1 : 1), depth));
	imageStore(s_full_velocity_output, uvi, vec4(base_velocity * (view_past_ndc_cache.w < 0 ? -1 : 1), depth));
}