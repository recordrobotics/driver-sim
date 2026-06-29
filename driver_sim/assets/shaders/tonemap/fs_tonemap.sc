// https://github.com/Unity-Technologies/Graphics/blob/master/com.unity.postprocessing/PostProcessing/Shaders/Colors.hlsl

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_tex, 0);
SAMPLER2D(s_lut, 1);

#ifndef USE_PRECISE_LOGC
    // Set to 1 to use more precise but more expensive log/linear conversions. I haven't found a proper
    // use case for the high precision version yet so I'm leaving this to 0.
    #define USE_PRECISE_LOGC 0
#endif

#ifndef USE_VERY_FAST_SRGB
    #if defined(SHADER_API_MOBILE)
        #define USE_VERY_FAST_SRGB 1
    #else
        #define USE_VERY_FAST_SRGB 0
    #endif
#endif

#ifndef USE_FAST_SRGB
    #if defined(SHADER_API_CONSOLE)
        #define USE_FAST_SRGB 1
    #else
        #define USE_FAST_SRGB 0
    #endif
#endif

//
// Alexa LogC converters (El 1000)
// See http://www.vocas.nl/webfm_send/964
// Max range is ~58.85666
//

#define CUT 0.011361
#define A   5.555556
#define B   0.047996
#define C   0.244161
#define D   0.386036
#define E   5.301883
#define F   0.092819

#if BGFX_SHADER_LANGUAGE_HLSL
#else

#define INVLOG10 0.434294481903

float log10(float x) {
    return log(x) * INVLOG10;
}

vec3 log10(vec3 x) {
    return log(x) * INVLOG10;
}

#endif

float LinearToLogC_Precise(float x)
{
    float o;
    if (x > CUT)
        o = C * log10(A * x + B) + D;
    else
        o = E * x + F;
    return o;
}

vec3 LinearToLogC(vec3 x)
{
#if USE_PRECISE_LOGC
    return vec3(
        LinearToLogC_Precise(x.x),
        LinearToLogC_Precise(x.y),
        LinearToLogC_Precise(x.z)
    );
#else
    return C * log10(A * x + B) + D;
#endif
}

float LogCToLinear_Precise(float x)
{
    float o;
    if (x > E * CUT + F)
        o = (pow(10.0, (x - D) / C) - B) / A;
    else
        o = (x - F) / E;
    return o;
}

vec3 LogCToLinear(vec3 x)
{
#if USE_PRECISE_LOGC
    return vec3(
        LogCToLinear_Precise(x.x),
        LogCToLinear_Precise(x.y),
        LogCToLinear_Precise(x.z)
    );
#else
    return (pow(vec3_splat(10.0), (x - D) / C) - B) / A;
#endif
}

#define FLT_EPSILON     1.192092896e-07 // Smallest positive number, such that 1.0 + FLT_EPSILON != 1.0

vec3 PositivePow(vec3 base, vec3 power)
{
    return pow(max(abs(base), vec3(FLT_EPSILON, FLT_EPSILON, FLT_EPSILON)), power);
}

vec3 LinearToSRGB(vec3 c)
{
#if USE_VERY_FAST_SRGB
    return sqrt(c);
#elif USE_FAST_SRGB
    return max(1.055 * PositivePow(c, 0.416666667) - 0.055, 0.0);
#else
    vec3 sRGBLo = c * 12.92;
    vec3 sRGBHi = (PositivePow(c, vec3(1.0 / 2.4, 1.0 / 2.4, 1.0 / 2.4)) * 1.055) - 0.055;
    bvec3 cCheck = lessThanEqual(c, vec3_splat(0.0031308));
    vec3 sRGB = mix(sRGBHi, sRGBLo, vec3(cCheck));
    return sRGB;
#endif
}

// x = 1 / lut_width
// y = 1 / lut_height
// z = lutHeight - 1
// w = exposureScale
uniform vec4 u_lutParams;

//
// 2D LUT grading
// scaleOffset = (1 / lut_width, 1 / lut_height, lut_height - 1)
//
vec3 applyLUT2D(vec3 uvw, vec3 scaleOffset)
{
    // Strip format where `height = sqrt(width)`
    uvw.z *= scaleOffset.z;
    float shift = floor(uvw.z);
    uvw.xy = uvw.xy * scaleOffset.z * scaleOffset.xy + scaleOffset.xy * 0.5;
    uvw.x += shift * scaleOffset.y;
    uvw.xyz = mix(
        texture2D(s_lut, vec2(uvw.x, 1.0 - uvw.y)).rgb,
        texture2D(s_lut, vec2(uvw.x + scaleOffset.y, 1.0 - uvw.y)).rgb,
        uvw.z - shift
    );
    return uvw;
}

void main()
{
    vec3 linearColor = texture2D(s_tex, v_texcoord0).rgb;

    vec3 logc = saturate(LinearToLogC(linearColor));
    vec3 lutOutput = applyLUT2D(logc, u_lutParams.xyz);

    gl_FragColor = vec4(LinearToSRGB(lutOutput), 1.0);
}