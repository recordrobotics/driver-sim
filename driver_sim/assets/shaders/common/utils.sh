#ifndef UTILS_H_HEADER_GUARD
#define UTILS_H_HEADER_GUARD

/*
    float CAMERA_NEAR
    float NDC_TO_VIEW_MUL.x
    float NDC_TO_VIEW_MUL.y
    float NDC_TO_VIEW_ADD.x

    float NDC_TO_VIEW_ADD.y
*/
uniform vec4 u_info[2];

// assumes reverse z, infinite far
#define CAMERA_NEAR u_info[0].x
#define NDC_TO_VIEW_MUL u_info[0].yz
#define NDC_TO_VIEW_ADD vec2(u_info[0].w, u_info[1].x)

// Inputs are screen XY and viewspace depth, output is viewspace position
vec3 ComputeViewspacePosition( const vec2 screenPos, const float viewspaceDepth, const vec2 NDCToViewMul, const vec2 NDCToViewAdd )
{
    vec3 ret;
    ret.xy = (NDCToViewMul * screenPos.xy + NDCToViewAdd) * viewspaceDepth;
    ret.z = viewspaceDepth;
    return ret;
}

float ScreenSpaceToViewSpaceDepth(float depth, float cameraNear) {
    return cameraNear / max(depth, 1e-9);
}

// 0 - near, 1 - far
float ScreenSpaceToLinearDepth(float depth) {
    return 1.0 - depth;
}

#endif // UTILS_H_HEADER_GUARD