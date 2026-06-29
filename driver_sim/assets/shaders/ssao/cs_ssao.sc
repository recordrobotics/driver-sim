#include <bgfx_compute.sh>

#include "../common/utils.sh"

IMAGE2D_RO(s_depth, r32f, 0);
IMAGE2D_RO(s_normal, rgba16f, 1);
IMAGE2D_RO(s_ssaoNoise, rgba16f, 2);
IMAGE2D_WO(s_output, r16f, 3);

#define KERNEL_SIZE 128

/*
float total_strength
float base
float area
float falloff

float radius
float bias
----
----
*/
uniform vec4 u_SSAOData[2];

uniform vec4 u_SSAOSamples[KERNEL_SIZE];

vec3 VSPositionFromDepth(uvec2 xy, uvec2 size)
{
  float z = 1-Linear01Depth(imageLoad(s_depth, xy).r);

  float x = xy.x / float(size.x);
  float y = xy.y / float(size.y);
  x = x * 2.0 - 1.0;
  y = y * 2.0 - 1.0;

  vec4 vProjectedPos = vec4(x, y, z, 1.0);
  
  // Transform by the inverse projection matrix
  vec4 vPositionVS = mul(u_invProj, vProjectedPos);
  
  // Divide by w to get the view-space position
  vPositionVS.xyz /= vPositionVS.w;

  return  vPositionVS.xyz;
}

uvec2 transformedTexcoords(vec2 texcoords, uvec2 size)
{
  uvec2 result = uvec2(
    uint(texcoords.x*size.x), uint(texcoords.y*size.y)
    );
  return result;
}


float ssao(uvec2 uvi, uvec2 size, float total_strength, float base, float area, float falloff, float radius, float bias)
{
  vec3 fragPos = VSPositionFromDepth(uvi, size);
  vec3 normal = imageLoad(s_normal, uvi).xyz;
  uint  noiseX = uint(uvi.x) % 4;
  uint  noiseY = uint(uvi.y) % 4;
  vec3 randomVec = imageLoad(s_ssaoNoise, uvec2(noiseX, noiseY)).xyz;

  vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
  vec3 bitangent = cross(normal, tangent);
  
  mat3 TBN = mtxFromCols(tangent, bitangent, normal); //from tangent to view

  float occlusion = 0.0;
  for(int i = 0; i < KERNEL_SIZE; i++)
  {
    // get sample position
    vec3 samplePos = mul(TBN, u_SSAOSamples[i].xyz); // from tangent to view-space
    samplePos = fragPos + samplePos * radius;

    vec4 offset = vec4(samplePos, 1.0);
    offset      = mul(u_proj, offset);    // from view to clip-space
    offset.xyz /= offset.w;               // perspective divide
    offset.xyz  = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0  

    float sampleDepth = VSPositionFromDepth(transformedTexcoords(offset.xy, size), size).z; 

    float difference = sampleDepth - fragPos.z + bias;
    occlusion += step(falloff, difference) * (1.0-smoothstep(falloff, area, difference));
    
  }
  float occ = 1.0 - (occlusion / KERNEL_SIZE);
  float result = pow(abs(occ), total_strength);
  return result;
}

NUM_THREADS(16, 16, 1)
void main()
{
    uvec2 render_size = uvec2(imageSize(s_output));
	uvec2 uvi = uvec2(gl_GlobalInvocationID.xy);
	if ((uvi.x >= render_size.x) || (uvi.y >= render_size.y)) 
	{
		return;
	}

    float result = ssao(uvi, render_size, u_SSAOData[0].x, u_SSAOData[0].y, u_SSAOData[0].z, u_SSAOData[0].w, u_SSAOData[1].x, u_SSAOData[1].y);
    imageStore(s_output, uvi, result);
}