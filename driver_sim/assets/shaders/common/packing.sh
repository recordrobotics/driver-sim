#ifndef PACKING_H_HEADER_GUARD
#define PACKING_H_HEADER_GUARD

// from https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.hlsli

// R11G11B10_UNORM <-> float3
vec3 R11G11B10_UNORM_to_FLOAT3( uint packedInput )
{
    vec3 unpackedOutput;
    unpackedOutput.x = float( ( packedInput       ) & uint(0x000007ff) ) / 2047.0;
    unpackedOutput.y = float( ( packedInput >> 11 ) & uint(0x000007ff) ) / 2047.0;
    unpackedOutput.z = float( ( packedInput >> 22 ) & uint(0x000003ff) ) / 1023.0;
    return unpackedOutput;
}
// 'unpackedInput' is float3 and not float3 on purpose as half float lacks precision for below!
uint FLOAT3_to_R11G11B10_UNORM( vec3 unpackedInput )
{
    uint packedOutput;
    packedOutput =( ( uint( saturate( unpackedInput.x ) * 2047.0 + 0.5 ) ) |
        ( uint( saturate( unpackedInput.y ) * 2047.0 + 0.5 ) << 11 ) |
        ( uint( saturate( unpackedInput.z ) * 1023.0 + 0.5 ) << 22 ) );
    return packedOutput;
}
//
vec4 R8G8B8A8_UNORM_to_FLOAT4( uint packedInput )
{
    vec4 unpackedOutput;
    unpackedOutput.x = float( packedInput & uint(0x000000ff) ) / 255.0;
    unpackedOutput.y = float( ( ( packedInput >> 8 ) & uint(0x000000ff) ) ) / 255.0;
    unpackedOutput.z = float( ( ( packedInput >> 16 ) & uint(0x000000ff) ) ) / 255.0;
    unpackedOutput.w = float( packedInput >> 24 ) / 255.0;
    return unpackedOutput;
}
//
uint FLOAT4_to_R8G8B8A8_UNORM( vec4 unpackedInput )
{
    return (( uint( saturate( unpackedInput.x ) * 255.0 + 0.5 ) ) |
            ( uint( saturate( unpackedInput.y ) * 255.0 + 0.5 ) << 8 ) |
            ( uint( saturate( unpackedInput.z ) * 255.0 + 0.5 ) << 16 ) |
            ( uint( saturate( unpackedInput.w ) * 255.0 + 0.5 ) << 24 ) );
}

#endif // PACKING_H_HEADER_GUARD