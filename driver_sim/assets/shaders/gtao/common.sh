#ifndef XeGTAO_COMMON_H_HEADER_GUARD
#define XeGTAO_COMMON_H_HEADER_GUARD

#include "../common/packing.sh"

#define XE_GTAO_PI               	(3.1415926535897932384626433832795)
#define XE_GTAO_PI_HALF             (1.5707963267948966192313216916398)

#define XE_GTAO_COMPUTE_BENT_NORMALS 1
#define XE_GTAO_OCCLUSION_TERM_SCALE                    (1.5f)      // for packing in UNORM (because raw, pre-denoised occlusion term can overshoot 1 but will later average out to 1)
#define XE_GTAO_DEPTH_MIP_LEVELS 5

uint XeGTAO_EncodeVisibilityBentNormal( float visibility, vec3 bentNormal )
{
    return FLOAT4_to_R8G8B8A8_UNORM( vec4( bentNormal * 0.5 + 0.5, visibility ) );
}

void XeGTAO_DecodeVisibilityBentNormal( const uint packedValue, out float visibility, out vec3 bentNormal )
{
    vec4 decoded = R8G8B8A8_UNORM_to_FLOAT4( packedValue );
    bentNormal = decoded.xyz * vec3_splat(2.0) - vec3_splat(1.0);   // could normalize - don't want to since it's done so many times, better to do it at the final step only
    visibility = decoded.w;
}

// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float XeGTAO_PackEdges( vec4 edgesLRTB )
{
    // integer version:
    // edgesLRTB = saturate(edgesLRTB) * 2.9.xxxx + 0.5.xxxx;
    // return (((uint)edgesLRTB.x) << 6) + (((uint)edgesLRTB.y) << 4) + (((uint)edgesLRTB.z) << 2) + (((uint)edgesLRTB.w));
    // 
    // optimized, should be same as above
    edgesLRTB = round( saturate( edgesLRTB ) * 2.9 );
    return dot( edgesLRTB, vec4( 64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0 ) ) ;
}

vec4 XeGTAO_UnpackEdges( float _packedVal )
{
    uint packedVal = uint(_packedVal * 255.5);
    vec4 edgesLRTB;
    edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.0;          // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
    edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.0;
    edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.0;
    edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.0;

    return saturate( edgesLRTB );
}

#endif // XeGTAO_COMMON_H_HEADER_GUARD