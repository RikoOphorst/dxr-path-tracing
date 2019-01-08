#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include <raytracing_data.h>
#include "util.hlsli"
#include "shading_data.hlsli"

struct ColorPayload
{
  float3 color;
  uint depth;
  uint seed;
};

struct GeometryPayload
{
  float3 normal;
  float3 albedo;
};

//------------------------------------------------------------------------------------------------------
inline void GenerateCameraRay(uint2 index, inout uint seed, out float3 origin, out float3 direction)
{
  float lens_radius = scene_constants.lens_diameter / 2.0f;

  float2 xy = float2(index) + (float2(nextRand(seed) * 2.0f - 1.0f, nextRand(seed) * 2.0f - 1.0f) * scene_constants.aa_enabled);
  float2 screen_pos = xy / float2(DispatchRaysDimensions().xy) * 2.0f - 1.0f;

  // Invert Y for DirectX-style coordinates.
  screen_pos.y = -screen_pos.y;

  // Unproject the pixel coordinate into a ray.
  float4 world = mul(float4(screen_pos, 0, 1), scene_constants.projection_to_world);

  world.xyz /= world.w;
  origin = scene_constants.camera_position.xyz + (RandomPointInUnitDisk(seed) * lens_radius);
  direction = normalize(world.xyz - origin);
}

//------------------------------------------------------------------------------------------------------
inline float3 ShootColorRay(float3 origin, float3 direction, float tmin, float tmax, uint seed, uint depth = 0)
{
  if (depth <= scene_constants.gi_num_bounces)
  {
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = tmin;
    ray.TMax = tmax;

    ColorPayload pay;
    pay.color = float3(0.0f, 0.0f, 0.0f);
    pay.depth = depth;
    pay.seed = seed;

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
  else
  {
    return float3(0.0f, 0.0f, 0.0f);
  }
}

GeometryPayload ShootGeometryRay(float3 origin, float3 direction, float tmin, float tmax)
{
  RayDesc ray;
  ray.Origin = origin;
  ray.Direction = direction;
  ray.TMin = tmin;
  ray.TMax = tmax;

  GeometryPayload pay;
  pay.normal = float3(0.0f, 0.0f, 0.0f);
  pay.albedo = float3(0.0f, 0.0f, 0.0f);

  TraceRay(
    scene_as,
    RAY_FLAG_NONE,
    ~0,
    1,
    0,
    1,
    ray,
    pay
  );

  return pay;
}

//------------------------------------------------------------------------------------------------------
[shader("raygeneration")]
void PrimaryRaygeneration()
{
  float3 ray_direction;
  float3 ray_origin;

  uint seed = initRand(DispatchRaysIndex().x * scene_constants.frame_count, DispatchRaysIndex().y * scene_constants.frame_count, 16);

  GenerateCameraRay(DispatchRaysIndex().xy, seed, ray_origin, ray_direction);

  float3 color = ShootColorRay(ray_origin, ray_direction, 0.001f, 10000.0f, seed, 0);
  GeometryPayload geometry = ShootGeometryRay(ray_origin, ray_direction, 0.001f, 10000.0f);

  render_target[DispatchRaysIndex().xy] += float4(saturate(color), 1.0f);
  normals_target[DispatchRaysIndex().y * 1280 + DispatchRaysIndex().x] += float4(geometry.normal, 1.0f);
  albedo_target[DispatchRaysIndex().y * 1280 + DispatchRaysIndex().x] += float4(geometry.albedo, 1.0f);
}

//------------------------------------------------------------------------------------------------------
[shader("closesthit")]
void ColorHit(inout ColorPayload payload, in TriangleAttributes attr)
{
  ShadingData hit = GetShadingData(attr);

  payload.color = hit.diffuse;

  if (hit.shading_model == 7)
  {
    float3 outward_normal;
    float3 reflected = reflect(WorldRayDirection(), hit.normal);
    float ni_over_nt;
    
    float3 refracted;
    float reflect_prob;
    float cosine;
    
    if (dot(WorldRayDirection(), hit.normal) > 0)
    {
      outward_normal = -hit.normal;
      ni_over_nt = hit.index_of_refraction;
      cosine = hit.index_of_refraction * dot(WorldRayDirection(), hit.normal) / length(WorldRayDirection());
    }
    else
    {
      outward_normal = hit.normal;
      ni_over_nt = 1.0f / hit.index_of_refraction;
      cosine = -dot(WorldRayDirection(), hit.normal) / length(WorldRayDirection());
    }
    
    if (srefract(WorldRayDirection(), outward_normal, ni_over_nt, refracted))
    {
      reflect_prob = schlick(cosine, hit.index_of_refraction);
    }
    else
    {
      reflect_prob = 1;
    }
    
    if (nextRand(payload.seed) < reflect_prob)
    {
      payload.color = ShootColorRay(hit.position, normalize(reflected), 0.001f, scene_constants.gi_bounce_distance, payload.seed, payload.depth + 1);
    }
    else
    {
      payload.color = ShootColorRay(hit.position, normalize(refracted), 0.001f, scene_constants.gi_bounce_distance, payload.seed, payload.depth + 1);
    }
  }
  else if (hit.shading_model == 9) 
  {
    payload.color = hit.emissive;
  }
  else 
  {
    float3 reflection_direction = CosineWeightedHemisphereSample(payload.seed, hit.normal);

    payload.color = hit.diffuse * ShootColorRay(hit.position, reflection_direction, 0.001f, scene_constants.gi_bounce_distance, payload.seed, payload.depth + 1);
  }

  payload.color += hit.emissive;
}

//------------------------------------------------------------------------------------------------------
[shader("miss")]
void ColorMiss(inout ColorPayload payload)
{
  payload.color = scene_constants.sky_color.xyz;
}

//------------------------------------------------------------------------------------------------------
[shader("closesthit")]
void GeometryHit(inout GeometryPayload payload, in TriangleAttributes attr)
{
  ShadingData hit = GetShadingData(attr);

  payload.normal = hit.normal;
  payload.albedo = hit.diffuse;
}

//------------------------------------------------------------------------------------------------------
[shader("miss")]
void GeometryMiss(inout GeometryPayload payload)
{
  payload.normal = float3(0.0f, 0.0f, 0.0f);
  payload.albedo = scene_constants.sky_color.xyz;
}

#endif // RAYTRACING_HLSL