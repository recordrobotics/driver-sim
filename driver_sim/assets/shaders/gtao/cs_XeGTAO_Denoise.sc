// https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.hlsli

#include <bgfx_compute.sh>
#include "../common/utils.sh"
#include "common.sh"

USAMPLER2D(s_workingAOTerm, 0);
SAMPLER2D(s_workingEdges, 1);
UIMAGE2D_WO(s_finalAOTerm, r32ui, 2);

#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
    #define AOTermType vec4   // xyz = bent normal, w = visibility
#else
    #define AOTermType float  // visibility only
#endif

void XeGTAO_AddSample( AOTermType ssaoValue, float edgeValue, inout AOTermType sum, inout float sumWeight )
{
    sum += (edgeValue * ssaoValue);
    sumWeight += edgeValue;
}

void XeGTAO_DecodeGatherPartial( const uvec4 packedValue, out AOTermType outDecoded[4] )
{
    for( int i = 0; i < 4; i++ )
#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
        XeGTAO_DecodeVisibilityBentNormal( packedValue[i], outDecoded[i].w, outDecoded[i].xyz );
#else
        outDecoded[i] = float(packedValue[i]) / 255.0;
#endif
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
    ivec2 render_size = ivec2(imageSize(s_finalAOTerm));
    const ivec2 pixCoordBase = ivec2(gl_GlobalInvocationID.xy) * ivec2(2, 1);   // we're computing 2 horizontal pixels at a time (performance optimization)
    const bool finalApply = true;

    const float blurAmount = (finalApply)?(u_XeGTAOData[2].z):(u_XeGTAOData[2].z/5.0);
    const float diagWeight = 0.85 * 0.5;

    AOTermType aoTerm[2];   // pixel pixCoordBase and pixel pixCoordBase + int2( 1, 0 )
    vec4 edgesC_LRTB[2];
    float weightTL[2];
    float weightTR[2];
    float weightBL[2];
    float weightBR[2];

    // gather edge and visibility quads, used later
    const vec2 gatherCenter = vec2( pixCoordBase.x, pixCoordBase.y ) / vec2( render_size );
    vec4 edgesQ0        = texture2DGatherOffset( s_workingEdges, gatherCenter, ivec2( 0, 0 ), 0 );
    vec4 edgesQ1        = texture2DGatherOffset( s_workingEdges, gatherCenter, ivec2( 2, 0 ), 0 );
    vec4 edgesQ2        = texture2DGatherOffset( s_workingEdges, gatherCenter, ivec2( 1, 2 ), 0 );

    AOTermType visQ0[4];    XeGTAO_DecodeGatherPartial( texture2DGatherOffset( s_workingAOTerm, gatherCenter, ivec2( 0, 0 ), 0 ), visQ0 );
    AOTermType visQ1[4];    XeGTAO_DecodeGatherPartial( texture2DGatherOffset( s_workingAOTerm, gatherCenter, ivec2( 2, 0 ), 0 ), visQ1 );
    AOTermType visQ2[4];    XeGTAO_DecodeGatherPartial( texture2DGatherOffset( s_workingAOTerm, gatherCenter, ivec2( 0, 2 ), 0 ), visQ2 );
    AOTermType visQ3[4];    XeGTAO_DecodeGatherPartial( texture2DGatherOffset( s_workingAOTerm, gatherCenter, ivec2( 2, 2 ), 0 ), visQ3 );

    for( int side = 0; side < 2; side++ )
    {
        const ivec2 pixCoord = ivec2( pixCoordBase.x + side, pixCoordBase.y );

        vec4 edgesL_LRTB  = XeGTAO_UnpackEdges( (side==0)?(edgesQ0.x):(edgesQ0.y) );
        vec4 edgesT_LRTB  = XeGTAO_UnpackEdges( (side==0)?(edgesQ0.z):(edgesQ1.w) );
        vec4 edgesR_LRTB  = XeGTAO_UnpackEdges( (side==0)?(edgesQ1.x):(edgesQ1.y) );
        vec4 edgesB_LRTB  = XeGTAO_UnpackEdges( (side==0)?(edgesQ2.w):(edgesQ2.z) );

        edgesC_LRTB[side]     = XeGTAO_UnpackEdges( (side==0)?(edgesQ0.y):(edgesQ1.x) );

        // Edges aren't perfectly symmetrical: edge detection algorithm does not guarantee that a left edge on the right pixel will match the right edge on the left pixel (although
        // they will match in majority of cases). This line further enforces the symmetricity, creating a slightly sharper blur. Works real nice with TAA.
        edgesC_LRTB[side] *= vec4( edgesL_LRTB.y, edgesR_LRTB.x, edgesT_LRTB.w, edgesB_LRTB.z );

#if 1   // this allows some small amount of AO leaking from neighbours if there are 3 or 4 edges; this reduces both spatial and temporal aliasing
        const float leak_threshold = 2.5; const float leak_strength = 0.5;
        float edginess = (saturate(4.0 - leak_threshold - dot( edgesC_LRTB[side], vec4_splat(1.0) )) / (4-leak_threshold)) * leak_strength;
        edgesC_LRTB[side] = saturate( edgesC_LRTB[side] + edginess );
#endif

        // for diagonals; used by first and second pass
        weightTL[side] = diagWeight * (edgesC_LRTB[side].x * edgesL_LRTB.z + edgesC_LRTB[side].z * edgesT_LRTB.x);
        weightTR[side] = diagWeight * (edgesC_LRTB[side].z * edgesT_LRTB.y + edgesC_LRTB[side].y * edgesR_LRTB.z);
        weightBL[side] = diagWeight * (edgesC_LRTB[side].w * edgesB_LRTB.x + edgesC_LRTB[side].x * edgesL_LRTB.w);
        weightBR[side] = diagWeight * (edgesC_LRTB[side].y * edgesR_LRTB.w + edgesC_LRTB[side].w * edgesB_LRTB.y);

        // first pass
        AOTermType ssaoValue     = (side==0)?(visQ0[1]):(visQ1[0]);
        AOTermType ssaoValueL    = (side==0)?(visQ0[0]):(visQ0[1]);
        AOTermType ssaoValueT    = (side==0)?(visQ0[2]):(visQ1[3]);
        AOTermType ssaoValueR    = (side==0)?(visQ1[0]):(visQ1[1]);
        AOTermType ssaoValueB    = (side==0)?(visQ2[2]):(visQ3[3]);
        AOTermType ssaoValueTL   = (side==0)?(visQ0[3]):(visQ0[2]);
        AOTermType ssaoValueBR   = (side==0)?(visQ3[3]):(visQ3[2]);
        AOTermType ssaoValueTR   = (side==0)?(visQ1[3]):(visQ1[2]);
        AOTermType ssaoValueBL   = (side==0)?(visQ2[3]):(visQ2[2]);

        float sumWeight = blurAmount;
        AOTermType sum = ssaoValue * sumWeight;

        XeGTAO_AddSample( ssaoValueL, edgesC_LRTB[side].x, sum, sumWeight );
        XeGTAO_AddSample( ssaoValueR, edgesC_LRTB[side].y, sum, sumWeight );
        XeGTAO_AddSample( ssaoValueT, edgesC_LRTB[side].z, sum, sumWeight );
        XeGTAO_AddSample( ssaoValueB, edgesC_LRTB[side].w, sum, sumWeight );

        XeGTAO_AddSample( ssaoValueTL, weightTL[side], sum, sumWeight );
        XeGTAO_AddSample( ssaoValueTR, weightTR[side], sum, sumWeight );
        XeGTAO_AddSample( ssaoValueBL, weightBL[side], sum, sumWeight );
        XeGTAO_AddSample( ssaoValueBR, weightBR[side], sum, sumWeight );

        aoTerm[side] = sum / sumWeight;

    #ifdef XE_GTAO_COMPUTE_BENT_NORMALS
        float     visibility = aoTerm[side].w * ((finalApply)?(XE_GTAO_OCCLUSION_TERM_SCALE):(1));
        vec3      bentNormal = normalize(aoTerm[side].xyz);
        bentNormal.z *= -1.0; // flip Z back to view space
        imageStore(s_finalAOTerm, pixCoord.xy, uvec4_splat(XeGTAO_EncodeVisibilityBentNormal( visibility, bentNormal )));
    #else
        imageStore(s_finalAOTerm, pixCoord.xy, uvec4_splat(uint(aoTerm[side] * ((finalApply)?(XE_GTAO_OCCLUSION_TERM_SCALE):(1)) * 255.0 + 0.5)));
    #endif
    }
}