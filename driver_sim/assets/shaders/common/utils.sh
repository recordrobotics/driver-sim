#ifndef UTILS_H_HEADER_GUARD
#define UTILS_H_HEADER_GUARD

uniform vec4 u_info;

// assumes reverse z, infinite far
#define CAMERA_NEAR u_info.x

float ScreenSpaceToViewSpaceDepth(float depth, float cameraNear) {
    return cameraNear / max(depth, 1e-9);
}

// 0 - near, 1 - far
float ScreenSpaceToLinearDepth(float depth) {
    return 1.0 - depth;
}

#endif // UTILS_H_HEADER_GUARD