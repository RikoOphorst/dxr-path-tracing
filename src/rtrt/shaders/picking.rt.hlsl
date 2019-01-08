#ifndef PICKING_HLSL
#define PICKING_HLSL

#include <raytracing_data.h>
#include "util.hlsli"
#include "shading_data.hlsli"

struct PickingPayload
{
  int material_index;
};

//------------------------------------------------------------------------------------------------------
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
  float lens_radius = scene_constants.lens_diameter / 2.0f;

  float2 xy = float2(index);
  float2 screen_pos = xy / float2(1280.0f, 720.0f) * 2.0f - 1.0f;

  // Invert Y for DirectX-style coordinates.
  screen_pos.y = -screen_pos.y;

  // Unproject the pixel coordinate into a ray.
  float4 world = mul(float4(screen_pos, 0, 1), scene_constants.projection_to_world);

  world.xyz /= world.w;
  origin = scene_constants.camera_position.xyz;
  direction = normalize(world.xyz - origin);
}

//------------------------------------------------------------------------------------------------------
inline int ShootPickingRay(float3 origin, float3 direction, float tmin, float tmax)
{
  RayDesc ray;
  ray.Origin = origin;
  ray.Direction = direction;
  ray.TMin = tmin;
  ray.TMax = tmax;

  PickingPayload pay;
  pay.material_index = -1;

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

  return pay.material_index;
}

//------------------------------------------------------------------------------------------------------
[shader("raygeneration")]
void PickingRaygeneration()
{
  float3 ray_direction;
  float3 ray_origin;

  GenerateCameraRay(uint2(scene_constants.picking_point), ray_origin, ray_direction);
  picking_buffer[0] = ShootPickingRay(ray_origin, ray_direction, 0.1f, 10000.0f);
}

//------------------------------------------------------------------------------------------------------
[shader("closesthit")]
void PickingHit(inout PickingPayload payload, in TriangleAttributes attr)
{
  payload.material_index = scene_meshes[InstanceID()].material;
}

//------------------------------------------------------------------------------------------------------
[shader("miss")]
void PickingMiss(inout PickingPayload payload)
{
  payload.material_index = -1;
}

#endif // PICKING_HLSL