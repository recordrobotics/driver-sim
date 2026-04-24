#ifndef PBR_H_HEADER_GUARD
#define PBR_H_HEADER_GUARD

vec4 pbr(vec3 albedo, vec3 normal, float alpha) {
    if(alpha < 0.01) {
        return vec4_splat(0.0);
    }

    vec3 diffuse = albedo * max(dot(normal, vec3(0.0, 1.0, 0.0)), 0.0);

    vec3 finalColor = diffuse;

    return vec4(finalColor, alpha);
}

#endif // PBR_H_HEADER_GUARD