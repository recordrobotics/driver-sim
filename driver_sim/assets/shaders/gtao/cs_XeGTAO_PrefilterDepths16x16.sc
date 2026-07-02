// https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.hlsli

#include <bgfx_compute.sh>
#include "../common/utils.sh"

SAMPLER2D(s_depth, 0);
IMAGE2D_WO(s_depthMip0, r32f, 1);
IMAGE2D_WO(s_depthMip1, r32f, 2);
IMAGE2D_WO(s_depthMip2, r32f, 3);
IMAGE2D_WO(s_depthMip3, r32f, 4);
IMAGE2D_WO(s_depthMip4, r32f, 5);

// weighted average depth filter
float XeGTAO_DepthMIPFilter( float depth0, float depth1, float depth2, float depth3, float consts_effectRadius, float consts_radiusMultiplier, float consts_effectFalloffRange )
{
    float maxDepth = max( max( depth0, depth1 ), max( depth2, depth3 ) );

    const float depthRangeScaleFactor = 0.75; // found empirically :)

    const float effectRadius              = depthRangeScaleFactor * consts_effectRadius * consts_radiusMultiplier;
    const float falloffRange              = consts_effectFalloffRange * effectRadius;

    const float falloffFrom       = effectRadius * (1.0-consts_effectFalloffRange);
    // fadeout precompute optimisation
    const float falloffMul        = -1.0 / ( falloffRange );
    const float falloffAdd        = falloffFrom / ( falloffRange ) + 1.0;

    float weight0 = saturate( (maxDepth-depth0) * falloffMul + falloffAdd );
    float weight1 = saturate( (maxDepth-depth1) * falloffMul + falloffAdd );
    float weight2 = saturate( (maxDepth-depth2) * falloffMul + falloffAdd );
    float weight3 = saturate( (maxDepth-depth3) * falloffMul + falloffAdd );

    float weightSum = weight0 + weight1 + weight2 + weight3;
    return (weight0 * depth0 + weight1 * depth1 + weight2 * depth2 + weight3 * depth3) / weightSum;
}

// This is also a good place to do non-linear depth conversion for cases where one wants the 'radius' (effectively the threshold between near-field and far-field GI), 
// is required to be non-linear (i.e. very large outdoors environments).
float XeGTAO_ClampDepth( float depth )
{
#ifdef XE_GTAO_USE_HALF_FLOAT_PRECISION
    return clamp( depth, 0.0, 65504.0 );
#else
    return clamp( depth, 0.0, 3.402823466e+38 );
#endif
}

SHARED float g_scratchDepths[8][8];

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
*/
uniform vec4 u_XeGTAOData[3];

NUM_THREADS(8, 8, 1)
void main()
{
  uvec2 render_size = uvec2(textureSize(s_depth, 0));

  float consts_effectRadius = u_XeGTAOData[0].x;
  float consts_radiusMultiplier = u_XeGTAOData[0].y;
  float consts_effectFalloffRange = u_XeGTAOData[0].z;

  // MIP 0
  const uvec2 baseCoord = uvec2(gl_GlobalInvocationID.xy);
  const uvec2 pixCoord = baseCoord * 2;
  vec4 depths4 = textureGatherOffset( s_depth, vec2(pixCoord) / vec2(render_size), ivec2(1,1), 0 );
  float depth0 = XeGTAO_ClampDepth( ScreenSpaceToViewSpaceDepth( depths4.w, CAMERA_NEAR ) );
  float depth1 = XeGTAO_ClampDepth( ScreenSpaceToViewSpaceDepth( depths4.z, CAMERA_NEAR ) );
  float depth2 = XeGTAO_ClampDepth( ScreenSpaceToViewSpaceDepth( depths4.x, CAMERA_NEAR ) );
  float depth3 = XeGTAO_ClampDepth( ScreenSpaceToViewSpaceDepth( depths4.y, CAMERA_NEAR ) );

  imageStore( s_depthMip0, pixCoord + uvec2(0, 0), depth0 );
  imageStore( s_depthMip0, pixCoord + uvec2(1, 0), depth1 );
  imageStore( s_depthMip0, pixCoord + uvec2(0, 1), depth2 );
  imageStore( s_depthMip0, pixCoord + uvec2(1, 1), depth3 );

  // MIP 1
  float dm1 = XeGTAO_DepthMIPFilter( depth0, depth1, depth2, depth3, consts_effectRadius, consts_radiusMultiplier, consts_effectFalloffRange );
  imageStore( s_depthMip1, baseCoord, dm1 );
  g_scratchDepths[ gl_LocalInvocationID.x ][ gl_LocalInvocationID.y ] = dm1;

  memoryBarrierShared();
  barrier();

  // MIP 2
  BRANCH
  if( all( ( uvec2(gl_LocalInvocationID.xy) % uvec2_splat(2) ) == 0 ) )
  {
      float inTL = g_scratchDepths[gl_LocalInvocationID.x+0][gl_LocalInvocationID.y+0];
      float inTR = g_scratchDepths[gl_LocalInvocationID.x+1][gl_LocalInvocationID.y+0];
      float inBL = g_scratchDepths[gl_LocalInvocationID.x+0][gl_LocalInvocationID.y+1];
      float inBR = g_scratchDepths[gl_LocalInvocationID.x+1][gl_LocalInvocationID.y+1];

      float dm2 = XeGTAO_DepthMIPFilter( inTL, inTR, inBL, inBR, consts_effectRadius, consts_radiusMultiplier, consts_effectFalloffRange );
      imageStore( s_depthMip2, baseCoord / 2, dm2 );
      g_scratchDepths[ gl_LocalInvocationID.x ][ gl_LocalInvocationID.y ] = dm2;
  }

  memoryBarrierShared();
  barrier();

  // MIP 3
  BRANCH
  if( all( ( uvec2(gl_LocalInvocationID.xy) % uvec2_splat(4) ) == 0 ) )
  {
      float inTL = g_scratchDepths[gl_LocalInvocationID.x+0][gl_LocalInvocationID.y+0];
      float inTR = g_scratchDepths[gl_LocalInvocationID.x+2][gl_LocalInvocationID.y+0];
      float inBL = g_scratchDepths[gl_LocalInvocationID.x+0][gl_LocalInvocationID.y+2];
      float inBR = g_scratchDepths[gl_LocalInvocationID.x+2][gl_LocalInvocationID.y+2];

      float dm3 = XeGTAO_DepthMIPFilter( inTL, inTR, inBL, inBR, consts_effectRadius, consts_radiusMultiplier, consts_effectFalloffRange );
      imageStore( s_depthMip3, baseCoord / 4, dm3 );
      g_scratchDepths[ gl_LocalInvocationID.x ][ gl_LocalInvocationID.y ] = dm3;
  }

  memoryBarrierShared();
  barrier();

  // MIP 4
  BRANCH
  if( all( ( uvec2(gl_LocalInvocationID.xy) % uvec2_splat(8) ) == 0 ) )
  {
      float inTL = g_scratchDepths[gl_LocalInvocationID.x+0][gl_LocalInvocationID.y+0];
      float inTR = g_scratchDepths[gl_LocalInvocationID.x+4][gl_LocalInvocationID.y+0];
      float inBL = g_scratchDepths[gl_LocalInvocationID.x+0][gl_LocalInvocationID.y+4];
      float inBR = g_scratchDepths[gl_LocalInvocationID.x+4][gl_LocalInvocationID.y+4];

      float dm4 = XeGTAO_DepthMIPFilter( inTL, inTR, inBL, inBR, consts_effectRadius, consts_radiusMultiplier, consts_effectFalloffRange );
      imageStore( s_depthMip4, baseCoord / 8, dm4 );
      //g_scratchDepths[ gl_LocalInvocationID.x ][ gl_LocalInvocationID.y ] = dm4;
  }
}