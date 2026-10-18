$input v_texcoord0

#include <bgfx_shader.sh>
#include "color.sh"

uniform vec4 u_renderedRect;

SAMPLER2D(s_texPresent, 0);

vec3 SampleTextureCatmullRom(sampler2D tex, vec2 uv, vec2 texSize)
{
    // Convert UV to pixel coordinates
    vec2 samplePos = uv * texSize;
    vec2 texPos1 = floor(samplePos - 0.5) + 0.5;
    
    vec2 f = samplePos - texPos1;
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);

    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / (w1 + w2);

    vec2 texPos0  = texPos1 - 1.0;
    vec2 texPos3  = texPos1 + 2.0;
    vec2 texPos12 = texPos1 + offset12;

    // Normalize coordinates back to [0, 1]
    vec2 invTexSize = 1.0 / texSize;
    vec2 uv0  = texPos0  * invTexSize;
    vec2 uv3  = texPos3  * invTexSize;
    vec2 uv12 = texPos12 * invTexSize;

    vec3 result = vec3_splat(0.0);

    // 5-tap Catmull-Rom filter using bilateral weights
    result += texture2DLod(tex, vec2(uv12.x, uv0.y ), 0).rgb * (w12.x * w0.y );
    result += texture2DLod(tex, vec2(uv0.x,  uv12.y), 0).rgb * (w0.x  * w12.y);
    result += texture2DLod(tex, vec2(uv12.x, uv12.y), 0).rgb * (w12.x * w12.y);
    result += texture2DLod(tex, vec2(uv3.x,  uv12.y), 0).rgb * (w3.x  * w12.y);
    result += texture2DLod(tex, vec2(uv12.x, uv3.y ), 0).rgb * (w12.x * w3.y );

    // Normalize
    float totalWeight = (w12.x * w0.y) + (w0.x * w12.y) + (w12.x * w12.y) + (w3.x * w12.y) + (w12.x * w3.y);
    return result / totalWeight;
}

void main()
{
    vec3 linearColor = SampleTextureCatmullRom(s_texPresent, v_texcoord0, u_renderedRect.zw).rgb;
    vec3 srgbColor = LinearToSRGB(linearColor);

    gl_FragColor = vec4(srgbColor, 1.0);
}