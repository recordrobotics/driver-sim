// https://github.com/Unity-Technologies/Graphics/blob/master/com.unity.postprocessing/PostProcessing/Shaders/Colors.hlsl

#ifndef COLOR_H_HEADER_GUARD
#define COLOR_H_HEADER_GUARD

#ifndef USE_PRECISE_LOGC
    // Set to 1 to use more precise but more expensive log/linear conversions. I haven't found a proper
    // use case for the high precision version yet so I'm leaving this to 0.
    #define USE_PRECISE_LOGC 0
#endif

#ifndef USE_VERY_FAST_SRGB
    #if 0
        #define USE_VERY_FAST_SRGB 1
    #else
        #define USE_VERY_FAST_SRGB 0
    #endif
#endif

#ifndef USE_FAST_SRGB
    #if 0
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

#define LOGC_CUT 0.011361
#define LOGC_A   5.555556
#define LOGC_B   0.047996
#define LOGC_C   0.244161
#define LOGC_D   0.386036
#define LOGC_E   5.301883
#define LOGC_F   0.092819

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
    if (x > LOGC_CUT)
        o = LOGC_C * log10(LOGC_A * x + LOGC_B) + LOGC_D;
    else
        o = LOGC_E * x + LOGC_F;
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
    return LOGC_C * log10(LOGC_A * x + LOGC_B) + LOGC_D;
#endif
}

float LogCToLinear_Precise(float x)
{
    float o;
    if (x > LOGC_E * LOGC_CUT + LOGC_F)
        o = (pow(10.0, (x - LOGC_D) / LOGC_C) - LOGC_B) / LOGC_A;
    else
        o = (x - LOGC_F) / LOGC_E;
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
    return (pow(vec3_splat(10.0), (x - LOGC_D) / LOGC_C) - LOGC_B) / LOGC_A;
#endif
}

#define FLT_EPSILON     1.192092896e-07 // Smallest positive number, such that 1.0 + FLT_EPSILON != 1.0

float PositivePow(float base, float power)
{
    return pow(max(abs(base), FLT_EPSILON), power);
}

vec3 PositivePow(vec3 base, vec3 power)
{
    return pow(max(abs(base), vec3(FLT_EPSILON, FLT_EPSILON, FLT_EPSILON)), power);
}

float SRGBToLinear(float c)
{
#if USE_VERY_FAST_SRGB
    return c * c;
#elif USE_FAST_SRGB
    return c * (c * (c * 0.305306011 + 0.682171111) + 0.012522878);
#else
    float linearRGBLo = c / 12.92;
    float linearRGBHi = PositivePow((c + 0.055) / 1.055, 2.4);
    float linearRGB = (c <= 0.04045) ? linearRGBLo : linearRGBHi;
    return linearRGB;
#endif
}

vec3 SRGBToLinear(vec3 c)
{
#if USE_VERY_FAST_SRGB
    return c * c;
#elif USE_FAST_SRGB
    return c * (c * (c * 0.305306011 + 0.682171111) + 0.012522878);
#else
    vec3 linearRGBLo = c / 12.92;
    vec3 linearRGBHi = PositivePow((c + 0.055) / 1.055, vec3_splat(2.4));
    vec3 linearRGB = vec3((c.x <= 0.04045) ? linearRGBLo.x : linearRGBHi.x, (c.y <= 0.04045) ? linearRGBLo.y : linearRGBHi.y, (c.z <= 0.04045) ? linearRGBLo.z : linearRGBHi.z);
    return linearRGB;
#endif
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

#endif // COLOR_H_HEADER_GUARD