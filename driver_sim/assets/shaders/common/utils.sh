#ifndef UTILS_H_HEADER_GUARD
#define UTILS_H_HEADER_GUARD

/*
    float CAMERA_NEAR
    float CAMERA_NEAR_LOG
    float CAMERA_LINEAR_RANGE
    float CAMERA_LINEAR_RANGE_LOG

    float NDC_TO_VIEW_MUL.x
    float NDC_TO_VIEW_MUL.y
    float NDC_TO_VIEW_ADD.x
    float NDC_TO_VIEW_ADD.y
*/
uniform vec4 u_info[2];

// assumes reverse z, infinite far
#define CAMERA_NEAR u_info[0].x
#define CAMERA_NEAR_LOG u_info[0].y
#define CAMERA_LINEAR_RANGE u_info[0].z
#define CAMERA_LINEAR_RANGE_LOG u_info[0].w

#define NDC_TO_VIEW_MUL u_info[1].xy
#define NDC_TO_VIEW_ADD u_info[1].zw

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
float ScreenSpaceToLinearDepth(float depth, float cameraNear, float cameraLinearRange) {
    return cameraNear * (1.0 / max(depth, 1e-9) - 1.0) / cameraLinearRange;
}

// -1 - near, 1 - far, log distribution
float ScreenSpaceToLogDepth(float depth, float cameraNear, float cameraNearLog, float cameraLinearRangeLog) {
    float viewSpaceDepth = ScreenSpaceToViewSpaceDepth(depth, cameraNear);
    return ((log(viewSpaceDepth) - cameraNearLog) / cameraLinearRangeLog) * 2.0 - 1.0;
}

#endif // UTILS_H_HEADER_GUARD