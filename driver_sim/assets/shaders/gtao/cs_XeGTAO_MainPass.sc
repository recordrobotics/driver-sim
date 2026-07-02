// https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.hlsli

#include <bgfx_compute.sh>
#include "../common/utils.sh"
#include "common.sh"

SAMPLER2D(s_depth, 0);
UIMAGE2D_RO(s_normal, r32ui, 1);
UIMAGE2D_RO(s_hilbertLut, r16ui, 2);
UIMAGE2D_WO(s_workingAOTerm, r32ui, 3);
IMAGE2D_WO(s_workingEdges, r32f, 4);

vec4 XeGTAO_CalculateEdges( const float centerZ, const float leftZ, const float rightZ, const float topZ, const float bottomZ )
{
    vec4 edgesLRTB = vec4( leftZ, rightZ, topZ, bottomZ ) - vec4_splat(centerZ);

    float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
    float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
    vec4 edgesLRTBSlopeAdjusted = edgesLRTB + vec4( slopeLR, -slopeLR, slopeTB, -slopeTB );
    edgesLRTB = min( abs( edgesLRTB ), abs( edgesLRTBSlopeAdjusted ) );
    return vec4(saturate( ( vec4_splat(1.25) - edgesLRTB / vec4_splat(centerZ * 0.011) ) ));
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float XeGTAO_FastSqrt( float x )
{
    return (intBitsToFloat( int(0x1fbd1df5 + ( floatBitsToInt( x ) >> 1 ) )));
}
// input [-1, 1] and output [0, PI], from https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
float XeGTAO_FastACos( float inX )
{ 
    const float PI = 3.141593;
    const float HALF_PI = 1.570796;
    float x = abs(inX); 
    float res = -0.156583 * x + HALF_PI; 
    res *= XeGTAO_FastSqrt(1.0 - x); 
    return (inX >= 0) ? res : PI - res; 
}

// "Efficiently building a matrix to rotate one vector to another"
// http://cs.brown.edu/research/pubs/pdfs/1999/Moller-1999-EBA.pdf / https://dl.acm.org/doi/10.1080/10867651.1999.10487509
// (using https://github.com/assimp/assimp/blob/master/include/assimp/matrix3x3.inl#L275 as a code reference as it seems to be best)
mat3 XeGTAO_RotFromToMatrix( vec3 from, vec3 to )
{
    const float e       = dot(from, to);
    const float f       = abs(e); //(e < 0)? -e:e;

    // WARNING: This has not been tested/worked through, especially not for 16bit floats; seems to work in our special use case (from is always {0, 0, -1}) but wouldn't use it in general
    if( f > 1.0 - 0.0003 )
        return mat3( 1, 0, 0, 0, 1, 0, 0, 0, 1 );

    const vec3 v      = cross( from, to );
    /* ... use this hand optimized version (9 mults less) */
    const float h       = (1.0)/(1.0 + e);      /* optimization by Gottfried Chen */
    const float hvx     = h * v.x;
    const float hvz     = h * v.z;
    const float hvxy    = hvx * v.y;
    const float hvxz    = hvx * v.z;
    const float hvyz    = hvz * v.y;

#if BGFX_SHADER_MATRIX_COLUMN_MAJOR
    mat3 mtx = mat3(
        e + hvx * v.x, hvxy + v.z, hvxz - v.y,
        hvxy - v.z, e + h * v.y * v.y, hvyz + v.x,
        hvxz + v.y, hvyz - v.x, e + hvz * v.z
    );
#else
    mat3 mtx = mat3(
        e + hvx * v.x, hvxy - v.z, hvxz + v.y,
        hvxy + v.z, e + h * v.y * v.y, hvyz - v.x,
        hvxz - v.y, hvyz + v.x, e + hvz * v.z
    );
#endif

    return mtx;
}

/*
  float effectRadius
  float radiusMultiplier
  float effectFalloffRange
  uint noiseIndex

  float SampleDistributionPower
  float ThinOccluderCompensation
  float NDCToViewMul_x_PixelSize.x
  float NDCToViewMul_x_PixelSize.y

  float DepthMIPSamplingOffset
  float FinalValuePower
  float DenoiseBlurBeta
*/
uniform vec4 u_XeGTAOData[3];

NUM_THREADS(16, 16, 1)
void main()
{
    ivec2 render_size = ivec2(imageSize(s_workingAOTerm));

    const ivec2 pixCoord = ivec2(gl_GlobalInvocationID.xy);
    const uint sliceCount = 3;
    const uint stepsPerSlice = 3;

    uint index = imageLoad(s_hilbertLut, ivec2(uvec2(pixCoord) % 64) ).x;
    index += 288*(uint(u_XeGTAOData[0].w)%64); // why 288? tried out a few and that's the best so far (with XE_HILBERT_LEVEL 6U) - but there's probably better :)
    // R2 sequence - see http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
    const vec2 localNoise = vec2( fract( 0.5 + index * vec2(0.75487766624669276005, 0.5698402909980532659114) ) );

    uint packedInput = imageLoad(s_normal, pixCoord).x;
    vec3 unpackedOutput = R11G11B10_UNORM_to_FLOAT3( packedInput );
    vec3 viewspaceNormal = normalize(unpackedOutput * vec3_splat(2.0) - vec3_splat(1.0));

    // XeGTAO_MainPass

    vec2 normalizedScreenPos = (vec2(pixCoord) + vec2_splat(0.5)) / vec2(render_size);

    vec4 valuesUL   = textureGather(s_depth, vec2(pixCoord) / vec2(render_size), 0);
    vec4 valuesBR   = textureGatherOffset(s_depth, vec2(pixCoord) / vec2(render_size), ivec2( 1, 1 ), 0);

    // viewspace Z at the center
    float viewspaceZ  = valuesUL.y; // texture2DLod( s_depth, normalizedScreenPos, 0 ).x; 

    // viewspace Zs left top right bottom
    const float pixLZ = valuesUL.x;
    const float pixTZ = valuesUL.z;
    const float pixRZ = valuesBR.z;
    const float pixBZ = valuesBR.x;

    vec4 edgesLRTB  = XeGTAO_CalculateEdges( viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ );
    imageStore( s_workingEdges, pixCoord, vec4_splat(XeGTAO_PackEdges(edgesLRTB)) );

    // Move center pixel slightly towards camera to avoid imprecision artifacts due to depth buffer imprecision; offset depends on depth texture format used
    viewspaceZ *= 0.99999;     // this is good for FP32 depth buffer

    const vec3 pixCenterPos   = ComputeViewspacePosition( normalizedScreenPos, viewspaceZ, NDC_TO_VIEW_MUL, NDC_TO_VIEW_ADD );
    const vec3 viewVec      = normalize(-pixCenterPos);
    
    // prevents normals that are facing away from the view vector - xeGTAO struggles with extreme cases, but in Vanilla it seems rare so it's disabled by default
    // viewspaceNormal = normalize( viewspaceNormal + max( 0, -dot( viewspaceNormal, viewVec ) ) * viewVec );


    const float effectRadius              = u_XeGTAOData[0].x * u_XeGTAOData[0].y;
    const float sampleDistributionPower   = u_XeGTAOData[1].x;
    const float thinOccluderCompensation  = u_XeGTAOData[1].y;
    const float falloffRange              = u_XeGTAOData[0].z * effectRadius;

    const float falloffFrom       = effectRadius * (1.0-u_XeGTAOData[0].z);

    // fadeout precompute optimisation
    const float falloffMul        = -1.0 / ( falloffRange );
    const float falloffAdd        = falloffFrom / ( falloffRange ) + 1.0;

    float visibility = 0;
#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
    vec3 bentNormal = vec3_splat(0);
#else
    vec3 bentNormal = viewspaceNormal;
#endif

    // see "Algorithm 1" in https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
    {
        const float noiseSlice  = localNoise.x;
        const float noiseSample = localNoise.y;

        // quality settings / tweaks / hacks
        const float pixelTooCloseThreshold  = 1.3;      // if the offset is under approx pixel size (pixelTooCloseThreshold), push it out to the minimum distance

        // approx viewspace pixel size at pixCoord; approximation of NDCToViewspace( normalizedScreenPos.xy + consts.ViewportPixelSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
        const vec2 pixelDirRBViewspaceSizeAtCenterZ = viewspaceZ.xx * vec2(u_XeGTAOData[1].z, u_XeGTAOData[1].w);

        float screenspaceRadius   = effectRadius / pixelDirRBViewspaceSizeAtCenterZ.x;

        // fade out for small screen radii 
        visibility += saturate((10 - screenspaceRadius)/100)*0.5;

#if 0   // sensible early-out for even more performance; disabled because not yet tested
        BRANCH
        if( screenspaceRadius < pixelTooCloseThreshold )
        {
            visibility = 1.0;
            visibility = saturate( visibility / XE_GTAO_OCCLUSION_TERM_SCALE );
        #ifdef XE_GTAO_COMPUTE_BENT_NORMALS
            imageStore( s_workingAOTerm, pixCoord, uvec4_splat(XeGTAO_EncodeVisibilityBentNormal( visibility, viewspaceNormal )) );
        #else
            imageStore( s_workingAOTerm, pixCoord, uvec4_splat(uint(visibility * 255.0 + 0.5)) );
        #endif
            return;
        }
#endif

        // this is the min distance to start sampling from to avoid sampling from the center pixel (no useful data obtained from sampling center pixel)
        const float minS = pixelTooCloseThreshold / screenspaceRadius;

        //UNROLL
        for( uint slice = 0; slice < sliceCount; slice++ )
        {
            float sliceK = (float(slice)+noiseSlice) / float(sliceCount);
            // lines 5, 6 from the paper
            float phi = sliceK * XE_GTAO_PI;
            float cosPhi = cos(phi);
            float sinPhi = sin(phi);
            vec2 omega = vec2(cosPhi, -sinPhi);       //lpfloat2 on omega causes issues with big radii

            // convert to screen units (pixels) for later use
            omega *= screenspaceRadius;

            // line 8 from the paper
            const vec3 directionVec = vec3(cosPhi, sinPhi, 0);

            // line 9 from the paper
            const vec3 orthoDirectionVec = directionVec - (dot(directionVec, viewVec) * viewVec);

            // line 10 from the paper
            //axisVec is orthogonal to directionVec and viewVec, used to define projectedNormal
            const vec3 axisVec = normalize( cross(orthoDirectionVec, viewVec) );

            // alternative line 9 from the paper
            // float3 orthoDirectionVec = cross( viewVec, axisVec );

            // line 11 from the paper
            vec3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);

            // line 13 from the paper
            float signNorm = float(sign( dot( orthoDirectionVec, projectedNormalVec ) ));

            // line 14 from the paper
            float projectedNormalVecLength = length(projectedNormalVec);
            float cosNorm = saturate(dot(projectedNormalVec, viewVec) / projectedNormalVecLength);

            // line 15 from the paper
            float n = signNorm * XeGTAO_FastACos(cosNorm);

            // this is a lower weight target; not using -1 as in the original paper because it is under horizon, so a 'weight' has different meaning based on the normal
            const float lowHorizonCos0  = cos(n+XE_GTAO_PI_HALF);
            const float lowHorizonCos1  = cos(n-XE_GTAO_PI_HALF);

            // lines 17, 18 from the paper, manually unrolled the 'side' loop
            float horizonCos0           = lowHorizonCos0; //-1;
            float horizonCos1           = lowHorizonCos1; //-1;

            UNROLL
            for( uint step = 0; step < stepsPerSlice; step++ )
            {
                // R1 sequence (http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/)
                const float stepBaseNoise = float(slice + step * stepsPerSlice) * 0.6180339887498948482; // <- this should unroll
                float stepNoise = fract(noiseSample + stepBaseNoise);

                // approx line 20 from the paper, with added noise
                float s = float(step+stepNoise) / float(stepsPerSlice); // + 1e-6f);

                // additional distribution modifier
                s       = pow( s, sampleDistributionPower );

                // avoid sampling center pixel
                s       += minS;

                // approx lines 21-22 from the paper, unrolled
                vec2 sampleOffset = s * omega;

                float sampleOffsetLength = length( sampleOffset );

                // note: when sampling, using point_point_point or point_point_linear sampler works, but linear_linear_linear will cause unwanted interpolation between neighbouring depth values on the same MIP level!
                const float mipLevel    = clamp( log2( sampleOffsetLength ) - u_XeGTAOData[2].x, 0, XE_GTAO_DEPTH_MIP_LEVELS );

                // Snap to pixel center (more correct direction math, avoids artifacts due to sampling pos not matching depth texel center - messes up slope - but adds other 
                // artifacts due to them being pushed off the slice). Also use full precision for high res cases.
                sampleOffset = round(sampleOffset)  / vec2(render_size);

                vec2 sampleScreenPos0 = normalizedScreenPos + sampleOffset;
                float  SZ0 = texture2DLod( s_depth, sampleScreenPos0, mipLevel ).x;
                vec3 samplePos0 = ComputeViewspacePosition( sampleScreenPos0, SZ0, NDC_TO_VIEW_MUL, NDC_TO_VIEW_ADD );

                vec2 sampleScreenPos1 = normalizedScreenPos - sampleOffset;
                float  SZ1 = texture2DLod( s_depth, sampleScreenPos1, mipLevel ).x;
                vec3 samplePos1 = ComputeViewspacePosition( sampleScreenPos1, SZ1, NDC_TO_VIEW_MUL, NDC_TO_VIEW_ADD );

                vec3 sampleDelta0     = (samplePos0 - vec3(pixCenterPos)); // using lpfloat for sampleDelta causes precision issues
                vec3 sampleDelta1     = (samplePos1 - vec3(pixCenterPos)); // using lpfloat for sampleDelta causes precision issues
                float sampleDist0     = length( sampleDelta0 );
                float sampleDist1     = length( sampleDelta1 );

                // approx lines 23, 24 from the paper, unrolled
                vec3 sampleHorizonVec0 = (sampleDelta0 / sampleDist0);
                vec3 sampleHorizonVec1 = (sampleDelta1 / sampleDist1);

                // any sample out of radius should be discarded - also use fallof range for smooth transitions; this is a modified idea from "4.3 Implementation details, Bounding the sampling area"
#if XE_GTAO_USE_DEFAULT_CONSTANTS != 0 && XE_GTAO_DEFAULT_THIN_OBJECT_HEURISTIC == 0
                float weight0         = saturate( sampleDist0 * falloffMul + falloffAdd );
                float weight1         = saturate( sampleDist1 * falloffMul + falloffAdd );
#else
                // this is our own thickness heuristic that relies on sooner discarding samples behind the center
                float falloffBase0    = length( vec3(sampleDelta0.x, sampleDelta0.y, sampleDelta0.z * (1.0+thinOccluderCompensation) ) );
                float falloffBase1    = length( vec3(sampleDelta1.x, sampleDelta1.y, sampleDelta1.z * (1.0+thinOccluderCompensation) ) );
                float weight0         = saturate( falloffBase0 * falloffMul + falloffAdd );
                float weight1         = saturate( falloffBase1 * falloffMul + falloffAdd );
#endif

                // sample horizon cos
                float shc0 = dot(sampleHorizonVec0, viewVec);
                float shc1 = dot(sampleHorizonVec1, viewVec);

                // discard unwanted samples
                shc0 = mix( lowHorizonCos0, shc0, weight0 ); // this would be more correct but too expensive: cos(mix( acos(lowHorizonCos0), acos(shc0), weight0 ));
                shc1 = mix( lowHorizonCos1, shc1, weight1 ); // this would be more correct but too expensive: cos(mix( acos(lowHorizonCos1), acos(shc1), weight1 ));

                // thickness heuristic - see "4.3 Implementation details, Height-field assumption considerations"
#if 0   // (disabled, not used) this should match the paper
                float newhorizonCos0 = max( horizonCos0, shc0 );
                float newhorizonCos1 = max( horizonCos1, shc1 );
                horizonCos0 = (horizonCos0 > shc0)?( mix( newhorizonCos0, shc0, thinOccluderCompensation ) ):( newhorizonCos0 );
                horizonCos1 = (horizonCos1 > shc1)?( mix( newhorizonCos1, shc1, thinOccluderCompensation ) ):( newhorizonCos1 );
#elif 0 // (disabled, not used) this is slightly different from the paper but cheaper and provides very similar results
                horizonCos0 = mix( max( horizonCos0, shc0 ), shc0, thinOccluderCompensation );
                horizonCos1 = mix( max( horizonCos1, shc1 ), shc1, thinOccluderCompensation );
#else   // this is a version where thicknessHeuristic is completely disabled
                horizonCos0 = max( horizonCos0, shc0 );
                horizonCos1 = max( horizonCos1, shc1 );
#endif

            }

#if 1       // I can't figure out the slight overdarkening on high slopes, so I'm adding this fudge - in the training set, 0.05 is close (PSNR 21.34) to disabled (PSNR 21.45)
            projectedNormalVecLength = mix( projectedNormalVecLength, 1.0, 0.05 );
#endif

            // line ~27, unrolled
            float h0 = -XeGTAO_FastACos(horizonCos1);
            float h1 = XeGTAO_FastACos(horizonCos0);
#if 0       // we can skip clamping for a tiny little bit more performance
            h0 = n + clamp( h0-n, -XE_GTAO_PI_HALF, XE_GTAO_PI_HALF );
            h1 = n + clamp( h1-n, -XE_GTAO_PI_HALF, XE_GTAO_PI_HALF );
#endif
            float iarc0 = (cosNorm + 2 * h0 * sin(n)-cos(2.0 * h0-n))/4.0;
            float iarc1 = (cosNorm + 2 * h1 * sin(n)-cos(2.0 * h1-n))/4.0;
            float localVisibility = projectedNormalVecLength * (iarc0+iarc1);
            visibility += localVisibility;

#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
            // see "Algorithm 2 Extension that computes bent normals b."
            float t0 = (6*sin(h0-n)-sin(3*h0-n)+6*sin(h1-n)-sin(3*h1-n)+16*sin(n)-3*(sin(h0+n)+sin(h1+n)))/12;
            float t1 = (-cos(3 * h0-n)-cos(3 * h1-n) +8 * cos(n)-3 * (cos(h0+n) +cos(h1+n)))/12;
            vec3 localBentNormal = vec3( directionVec.x * t0, directionVec.y * t0, -t1 );
            localBentNormal = mul( XeGTAO_RotFromToMatrix( vec3(0,0,-1), viewVec ), localBentNormal ) * projectedNormalVecLength;
            bentNormal += localBentNormal;
#endif
        }
        visibility /= float(sliceCount);
        visibility = pow( abs(visibility), u_XeGTAOData[2].y );
        visibility = max( 0.03, visibility ); // disallow total occlusion (which wouldn't make any sense anyhow since pixel is visible but also helps with packing bent normals)

#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
        bentNormal = normalize(bentNormal) ;
#endif
    }

    visibility = saturate( visibility / XE_GTAO_OCCLUSION_TERM_SCALE );
#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
    imageStore( s_workingAOTerm, pixCoord, uvec4_splat(XeGTAO_EncodeVisibilityBentNormal( visibility, bentNormal )) );
#else
    imageStore( s_workingAOTerm, pixCoord, uvec4_splat(uint(visibility * 255.0 + 0.5)) );
#endif
}