#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include <raytracing_data.h>
#include "util.hlsli"
#include "shading_data.hlsli"

typedef BuiltInTriangleIntersectionAttributes TriangleAttributes;

struct PrimaryRayPayload
{
  float3 color;
};

struct ShadowRayPayload
{
  float hit;
};

struct IndirectRayPayload
{
  float3 color;
  uint depth;
  uint seed;
};

//------------------------------------------------------------------------------------------------------
inline void GenerateCameraRay(uint2 index, out float3 origin[4], out float3 direction[4])
{
  int count = 0;

  [unroll]
  for (int y = 0; y < 2; y++)
  {
    [unroll]
    for (int x = 0; x < 2; x++)
    {
      float2 xy = float2(index) + float2(0.25f + 0.5f * x, 0.25f + 0.5f * y);
      float2 screen_pos = xy / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;

      // Invert Y for DirectX-style coordinates.
      screen_pos.y = -screen_pos.y;

      // Unproject the pixel coordinate into a ray.
      float4 world = mul(float4(screen_pos, 0, 1), scene_constants.projection_to_world);

      world.xyz /= world.w;
      origin[count] = scene_constants.camera_position.xyz;
      direction[count] = normalize(world.xyz - origin[count]);

      count++;
    }
  }
}

//------------------------------------------------------------------------------------------------------
inline float3 ShootPrimaryRay(float3 origin, float3 direction, float tmin, float tmax)
{
  RayDesc ray;
  ray.Origin = origin;
  ray.Direction = direction;
  ray.TMin = tmin;
  ray.TMax = tmax;

  PrimaryRayPayload pay;
  pay.color = float3(0.0f, 0.0f, 0.0f);

  TraceRay(
    scene_as, 
    RAY_FLAG_NONE, 
    ~0, 
    0, 
    0, 
    0, 
    ray, 
    pay
  );

  return pay.color;
}

//------------------------------------------------------------------------------------------------------
inline float ShootShadowRay(float3 origin, float3 direction, float tmin, float tmax)
{
  RayDesc ray;
  ray.Origin = origin;
  ray.Direction = direction;
  ray.TMin = tmin;
  ray.TMax = tmax;

  ShadowRayPayload pay;
  pay.hit = 0.0f;

  TraceRay(
    scene_as,
    RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
    ~0, // instance mask
    1, // hitgroup index
    0, // geom multiplier
    1, // miss index
    ray,
    pay
  );

  return pay.hit;
}

//------------------------------------------------------------------------------------------------------
inline float3 ShootIndirectRay(float3 origin, float3 direction, float tmin, float tmax, uint seed, uint depth = 0)
{
  RayDesc ray;
  ray.Origin = origin;
  ray.Direction = direction;
  ray.TMin = tmin;
  ray.TMax = tmax;

  IndirectRayPayload pay;
  pay.color = float3(0.0f, 0.0f, 0.0f);
  pay.depth = depth + 1;
  pay.seed = seed;

  TraceRay(
    scene_as,
    RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
    ~0, // instance mask
    2, // hitgroup index
    0, // geom multiplier
    2, // miss index
    ray,
    pay
  );

  return pay.color;
}

//------------------------------------------------------------------------------------------------------
float3 DiffuseShade(float3 position, float3 normal, float3 diffuse, inout uint seed)
{
  uint random_light = uint(nextRand(seed) * 1);

  float sample_probability = 1.0f / float(1);

  float3 light_position = scene_lights[random_light].position;
  float3 light_intensity = scene_lights[random_light].intensity;
  float dist_to_light = length(light_position - position);
  float3 dir_to_light = normalize(light_position - position);

  float NdotL = saturate(dot(normal, dir_to_light));
  float is_lit = ShootShadowRay(position, dir_to_light, 0.001f, dist_to_light);
  float3 ray_color = is_lit * light_intensity;

  float ao = 1.0f;
  if (scene_constants.ao_samples > 0)
  {
    float ambient_occlusion = 0.0f;

    for (int i = 0; i < scene_constants.ao_samples; i++)
    {
      float3 ao_dir = CosineWeightedHemisphereSample(seed, normal);
      ambient_occlusion += ShootShadowRay(position, ao_dir, 0.001f, 100.0f);
    }

    ao = ambient_occlusion / float(scene_constants.ao_samples);
  }

  return ((NdotL * ray_color * (diffuse / 3.14f)) / sample_probability) * ao;
}

//------------------------------------------------------------------------------------------------------
[shader("raygeneration")]
void PrimaryRaygeneration()
{
  float3 ray_directions[4];
  float3 ray_origins[4];

  GenerateCameraRay(DispatchRaysIndex().xy, ray_origins, ray_directions);

  float3 pixel_color = float3(0.0f, 0.0f, 0.0f);
  
  [unroll]
  for (int i = 0; i < 4; i++)
  {
    pixel_color += ShootPrimaryRay(ray_origins[i], ray_directions[i], 0.001f, 10000.0f);
  }

  render_target[DispatchRaysIndex().xy] += float4(pixel_color / 4.0f, 1.0f);
}

//------------------------------------------------------------------------------------------------------
[shader("closesthit")]
void PrimaryHit(inout PrimaryRayPayload payload, in TriangleAttributes attr)
{
  uint random_seed = initRand(DispatchRaysIndex().x * scene_constants.frame_count, DispatchRaysIndex().y * scene_constants.frame_count, 16);

  ShadingData hit = GetShadingData(attr);
  payload.color = DiffuseShade(hit.position, hit.normal, hit.diffuse, random_seed);
  
  if (scene_constants.gi_num_bounces > 0)
  {
    payload.color += hit.diffuse * ShootIndirectRay(hit.position, CosineWeightedHemisphereSample(random_seed, hit.normal), 0.001f, 100000.0f, random_seed, 0);
  }
}

//------------------------------------------------------------------------------------------------------
[shader("miss")]
void PrimaryMiss(inout PrimaryRayPayload payload)
{
  payload.color = float3(0.0f, 0.0f, 1.0f);
}

//------------------------------------------------------------------------------------------------------
[shader("closesthit")]
void ShadowHit(inout ShadowRayPayload payload, in TriangleAttributes attr)
{
  payload.hit = 0.0f;
}

//------------------------------------------------------------------------------------------------------
[shader("miss")]
void ShadowMiss(inout ShadowRayPayload payload)
{
  payload.hit = 1.0f;
}

//------------------------------------------------------------------------------------------------------
[shader("closesthit")]
void IndirectHit(inout IndirectRayPayload payload, in TriangleAttributes attr)
{
  ShadingData hit = GetShadingData(attr);
  payload.color = DiffuseShade(hit.position, hit.normal, hit.diffuse, payload.seed);

  if (payload.depth < scene_constants.gi_num_bounces)
  {
    payload.color += hit.diffuse * ShootIndirectRay(hit.position, CosineWeightedHemisphereSample(payload.seed, hit.normal), 0.001f, 100000.0f, payload.seed, payload.depth);
  }
}

//------------------------------------------------------------------------------------------------------
[shader("miss")]
void IndirectMiss(inout IndirectRayPayload payload)
{
  payload.color = float3(0.0f, 0.0f, 0.0f);
}

#endif // RAYTRACING_HLSL