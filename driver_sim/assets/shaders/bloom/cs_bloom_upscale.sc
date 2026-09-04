// https://github.com/tgalaj/RapidGL/blob/master/src/demos/26_bloom/upscale.comp

#include <bgfx_compute.sh>


SAMPLER2D(s_bloomInput, 0);
IMAGE2D_RW(u_output_image, r11f_g11f_b10f, 1);
SAMPLER2D(s_bloomDirt, 2);

uniform vec4  u_texel_size;
uniform vec4 u_bloom_intensity;

#define DIRT_TEXTURE_ASPECT_RATIO  (1280.0 / 720.0)

#define GROUP_SIZE         8
#define GROUP_THREAD_COUNT (GROUP_SIZE * GROUP_SIZE)
#define FILTER_SIZE        3
#define FILTER_RADIUS      (FILTER_SIZE / 2)
#define TILE_SIZE          (GROUP_SIZE + 2 * FILTER_RADIUS)
#define TILE_PIXEL_COUNT   (TILE_SIZE * TILE_SIZE)

SHARED float sm_r[TILE_PIXEL_COUNT];
SHARED float sm_g[TILE_PIXEL_COUNT];
SHARED float sm_b[TILE_PIXEL_COUNT];

void store_lds(uint idx, vec4 c)
{
    sm_r[idx] = c.r;
    sm_g[idx] = c.g;
    sm_b[idx] = c.b;
}

vec4 load_lds(uint idx)
{
    return vec4(sm_r[idx], sm_g[idx], sm_b[idx], 1.0);
}

NUM_THREADS(GROUP_SIZE, GROUP_SIZE, 1)
void main()
{
    int u_mip_level = int(round(u_texel_size.z));

	ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
    vec2  base_index   = ivec2(gl_WorkGroupID.xy) * GROUP_SIZE - FILTER_RADIUS;

    // The first (TILE_PIXEL_COUNT - GROUP_THREAD_COUNT) threads load at most 2 texel values
    for (uint i = gl_LocalInvocationIndex; i < TILE_PIXEL_COUNT; i += GROUP_THREAD_COUNT)
    {
        vec2 uv        = (base_index + 0.5) * u_texel_size.xy;
        vec2 uv_offset = vec2(i % TILE_SIZE, i / TILE_SIZE) * u_texel_size.xy;
        
        vec4 color = texture2DLod(s_bloomInput, (uv + uv_offset), u_mip_level);
        store_lds(i, color);
    }

    memoryBarrierShared();
    barrier();

    // center texel
    uint sm_idx = (gl_LocalInvocationID.x + FILTER_RADIUS) + (gl_LocalInvocationID.y + FILTER_RADIUS) * TILE_SIZE;

    // Based on [Jimenez14] http://goo.gl/eomGso
    vec4 s;
    s =  load_lds(sm_idx - TILE_SIZE - 1);
    s += load_lds(sm_idx - TILE_SIZE    ) * 2.0;
    s += load_lds(sm_idx - TILE_SIZE + 1);
	
    s += load_lds(sm_idx - 1) * 2.0;
    s += load_lds(sm_idx    ) * 4.0;
    s += load_lds(sm_idx + 1) * 2.0;
	
    s += load_lds(sm_idx + TILE_SIZE - 1);
    s += load_lds(sm_idx + TILE_SIZE    ) * 2.0;
    s += load_lds(sm_idx + TILE_SIZE + 1);

    vec4 bloom = s * (1.0 / 16.0);

	vec4 out_pixel = imageLoad(u_output_image, pixel_coords);
	     out_pixel += bloom * u_bloom_intensity.x;

    if (u_mip_level == 1)
    {
        vec2  uv  = (vec2(pixel_coords) + vec2(0.5, 0.5)) * u_texel_size.xy;
        float screenAspect = u_viewRect.z / u_viewRect.w;
        if(screenAspect > DIRT_TEXTURE_ASPECT_RATIO)
        {
            uv.y = uv.y * (DIRT_TEXTURE_ASPECT_RATIO / screenAspect) + 0.5 * (1.0 - DIRT_TEXTURE_ASPECT_RATIO / screenAspect);
        }
        else
        {
            uv.x = uv.x * (screenAspect / DIRT_TEXTURE_ASPECT_RATIO) + 0.5 * (1.0 - screenAspect / DIRT_TEXTURE_ASPECT_RATIO);
        }

        out_pixel += texture2DLod(s_bloomDirt, uv, 0.0) * u_bloom_intensity.y * bloom * u_bloom_intensity.x;
    }

	imageStore(u_output_image, pixel_coords, out_pixel);
}