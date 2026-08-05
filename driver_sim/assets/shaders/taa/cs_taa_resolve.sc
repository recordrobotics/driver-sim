#include <bgfx_compute.sh>

IMAGE2D_RO(s_velocity, rgba16f, 0);
SAMPLER2D(s_depth, 1);
IMAGE2D_RO(s_taaCurrent, r11f_g11f_b10f, 2);
SAMPLER2D(s_taaHistory, 3);
IMAGE2D_WO(s_taaOutput, r11f_g11f_b10f, 4);

float Mitchell(float x) {
    float ax = abs(x);
    float x2 = ax * ax;
    float x3 = ax * ax * ax;

    if (ax < 1.0) {
        // [(7 * x^3) - (12 * x^2) + (16/3)] / 6
        return (7.0 * x3 - 12.0 * x2 + 5.333333) / 6.0;
    } else if (ax < 2.0) {
        // [(-7/3 * x^3) + (12 * x^2) - (20 * x) + (32/3)] / 6
        return (-2.333333 * x3 + 12.0 * x2 - 20.0 * ax + 10.666667) / 6.0;
    }
    return 0.0;
}

float Luminance(vec3 _color)
{
    return dot(_color, vec3(0.2127, 0.7152, 0.0722));
}

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

vec3 ClipAABB(vec3 aabbMin, vec3 aabbMax, vec3 prevSample, vec3 avg)
{
    #if 0
        // note: only clips towards aabb center (but fast!)
        vec3 p_clip = 0.5 * (aabbMax + aabbMin);
        vec3 e_clip = 0.5 * (aabbMax - aabbMin);

        vec3 v_clip = prevSample - p_clip;
        vec3 v_unit = v_clip.xyz / e_clip;
        vec3 a_unit = abs(v_unit);
        float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

        if (ma_unit > 1.0)
            return p_clip + v_clip / ma_unit;
        else
            return prevSample;// point inside aabb
    #else
        vec3 r = prevSample - avg;
        vec3 rmax = aabbMax - avg.xyz;
        vec3 rmin = aabbMin - avg.xyz;

        const float eps = 0.000001;

        if (r.x > rmax.x + eps)
            r *= (rmax.x / r.x);
        if (r.y > rmax.y + eps)
            r *= (rmax.y / r.y);
        if (r.z > rmax.z + eps)
            r *= (rmax.z / r.z);

        if (r.x < rmin.x - eps)
            r *= (rmin.x / r.x);
        if (r.y < rmin.y - eps)
            r *= (rmin.y / r.y);
        if (r.z < rmin.z - eps)
            r *= (rmin.z / r.z);

        return avg + r;
    #endif
}

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    
    if (any(greaterThanEqual(pixel, ivec2(u_viewRect.zw))))
        return;

    vec3 sourceSampleTotal = vec3(0, 0, 0);
    float sourceSampleWeight = 0.0;
    vec3 neighborhoodMin = vec3_splat(10000);
    vec3 neighborhoodMax = vec3_splat(-10000);
    vec3 m1 = vec3(0, 0, 0);
    vec3 m2 = vec3(0, 0, 0);
    float closestDepth = 0.0;
    ivec2 closestDepthPixelPosition = ivec2(0, 0);
    
    UNROLL
    for (int x = -1; x <= 1; x++)
    {   
        UNROLL
        for (int y = -1; y <= 1; y++)
        {
            ivec2 pixelPosition = pixel + ivec2(x, y);
            pixelPosition = clamp(pixelPosition, ivec2(0, 0), ivec2(u_viewRect.zw) - 1);  
    
            vec3 neighbor = max(vec3_splat(0.0), imageLoad(s_taaCurrent, pixelPosition).rgb);
            float subSampleDistance = length(vec2(x, y));
            float subSampleWeight = Mitchell(subSampleDistance);
    
            sourceSampleTotal += neighbor * subSampleWeight;
            sourceSampleWeight += subSampleWeight;
    
            neighborhoodMin = min(neighborhoodMin, neighbor);
            neighborhoodMax = max(neighborhoodMax, neighbor);
    
            m1 += neighbor;
            m2 += neighbor * neighbor;
    
            float currentDepth = texture2DLod(s_depth, vec2(pixelPosition) / u_viewRect.zw, 0).r;
            if (currentDepth > closestDepth)
            {
                closestDepth = currentDepth;
                closestDepthPixelPosition = pixelPosition;
            }
        }
    }

    vec2 motionVector = imageLoad(s_velocity, closestDepthPixelPosition).xy;
    vec2 uv = (vec2(pixel) + 0.5) / u_viewRect.zw;
    vec2 historyTexCoord = uv + motionVector;
    vec3 sourceSample = max(vec3_splat(0.0), sourceSampleTotal / sourceSampleWeight);

    if(any(historyTexCoord != saturate(historyTexCoord)))
    {
        imageStore(s_taaOutput, pixel, vec4(sourceSample, 1.0));
        return;
    }
    
    vec3 historySample = SampleTextureCatmullRom(s_taaHistory, historyTexCoord, u_viewRect.zw).rgb;

    float oneDividedBySampleCount = 1.0 / 9.0;
    float gamma = 1.0;
    vec3 mu = m1 * oneDividedBySampleCount;
    vec3 sigma = sqrt(abs((m2 * oneDividedBySampleCount) - (mu * mu)));
    vec3 minc = mu - gamma * sigma;
    vec3 maxc = mu + gamma * sigma;
    
    historySample = ClipAABB(minc, maxc, clamp(historySample, neighborhoodMin, neighborhoodMax), mu);

    float sourceWeight = 0.05;
    float historyWeight = 1.0 - sourceWeight;
    vec3 compressedSource = sourceSample * rcp(max(max(sourceSample.r, sourceSample.g), sourceSample.b) + 1.0);
    vec3 compressedHistory = historySample * rcp(max(max(historySample.r, historySample.g), historySample.b) + 1.0);
    float luminanceSource = Luminance(compressedSource);
    float luminanceHistory = Luminance(compressedHistory);
    
    sourceWeight *= 1.0 / (1.0 + luminanceSource);
    historyWeight *= 1.0 / (1.0 + luminanceHistory);
    
    vec3 result = (sourceSample * sourceWeight + historySample * historyWeight) / max(sourceWeight + historyWeight, 0.00001);
    
    imageStore(s_taaOutput, pixel, vec4(result, 1.0));
}