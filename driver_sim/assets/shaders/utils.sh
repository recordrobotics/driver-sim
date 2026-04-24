#ifndef UTILS_H_HEADER_GUARD
#define UTILS_H_HEADER_GUARD

uniform vec4 u_info;

#define CAMERA_NEAR u_info.x
#define CAMERA_FAR u_info.y

float norm_depth(vec3 viewPosition) {
    return (viewPosition.z - CAMERA_NEAR) / max(CAMERA_FAR - CAMERA_NEAR, 1e-5);
}

#endif // UTILS_H_HEADER_GUARD