#ifndef UTILS_H_HEADER_GUARD
#define UTILS_H_HEADER_GUARD

uniform vec4 u_info;

// assumes reverse z, infinite far
#define CAMERA_NEAR u_info.x

float Linear01Depth(float depth) {
    return CAMERA_NEAR / depth;
}

#endif // UTILS_H_HEADER_GUARD