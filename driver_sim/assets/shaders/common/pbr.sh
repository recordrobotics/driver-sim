#ifndef PBR_H_HEADER_GUARD
#define PBR_H_HEADER_GUARD

#define PI 3.14159265359
#define LIGHT_COUNT 6

uniform vec4 u_lightPos[LIGHT_COUNT];
uniform vec4 u_lightColor[LIGHT_COUNT];

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;

    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = max(PI * denom * denom, 1e-4);

    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 computeLight(vec3 light_position, vec3 light_color, float light_intensity, vec3 world_position, vec3 N, vec3 V, vec3 albedo,
                  float metallic, float roughness, vec3 F0)
{
    vec3 L = normalize(light_position - world_position);
    vec3 H = normalize(V + L);

    float distance = length(light_position - world_position);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = light_color * light_intensity * attenuation;

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    vec3 specular = numerator / denom;

    vec3 kS = F;
    vec3 kD = (vec3_splat(1.0) - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec4 pbr(vec3 albedo, vec3 emission, vec3 world_position, vec3 camera_position, vec3 normal, float alpha, float metallic, float roughness) {
    if(alpha == 0.0) {
        return vec4_splat(0.0);
    }

    albedo = pow(abs(albedo), vec3_splat(2.2));
    float ao = 1.0;

    vec3 N = normal;
    vec3 V = normalize(camera_position - world_position);

    vec3 F0 = vec3_splat(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3_splat(0.0);

    for(int i = 0; i < LIGHT_COUNT; i++)
        Lo += computeLight(u_lightPos[i].xyz, u_lightColor[i].rgb, u_lightColor[i].a, world_position, N, V, albedo, metallic, roughness, F0);

    vec3 ambient = vec3_splat(0.03) * albedo * ao;

    vec3 color = ambient + Lo + emission;

    return vec4(color, alpha);
}

#endif // PBR_H_HEADER_GUARD