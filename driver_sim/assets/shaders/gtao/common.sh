#ifndef XeGTAO_COMMON_H_HEADER_GUARD
#define XeGTAO_COMMON_H_HEADER_GUARD

#include "../common/packing.sh"

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

#endif // XeGTAO_COMMON_H_HEADER_GUARD