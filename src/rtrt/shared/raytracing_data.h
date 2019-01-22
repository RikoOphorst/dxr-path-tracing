#ifndef RAYTRACING_DATA
#define RAYTRACING_DATA

// CBV slots
#define CPP_REGISTER_CONSTANTS 0
#define HLSL_REGISTER_CONSTANTS b0

// UAV slots
#define CPP_REGISTER_OUTPUT 0
#define HLSL_REGISTER_OUTPUT u0

#define CPP_REGISTER_PICKING_BUFFER 1
#define HLSL_REGISTER_PICKING_BUFFER u1

#define CPP_REGISTER_NORMALS 2
#define HLSL_REGISTER_NORMALS u2

#define CPP_REGISTER_ALBEDO 3
#define HLSL_REGISTER_ALBEDO u3

// SRV slots
#define CPP_REGISTER_ACCELERATION_STRUCT 0
#define HLSL_REGISTER_ACCELERATION_STRUCT t0

#define CPP_REGISTER_MESHES 1
#define HLSL_REGISTER_MESHES t1

#define CPP_REGISTER_VERTICES 2
#define HLSL_REGISTER_VERTICES t2

#define CPP_REGISTER_INDICES 3
#define HLSL_REGISTER_INDICES t3

#define CPP_REGISTER_MATERIALS 4
#define HLSL_REGISTER_MATERIALS t4

#define CPP_REGISTER_LIGHTS 5
#define HLSL_REGISTER_LIGHTS t5

#define CPP_REGISTER_TEXTURES 6
#define HLSL_REGISTER_TEXTURES t6

// Sampler slots
#define CPP_REGISTER_SAMPLER 0
#define HLSL_REGISTER_SAMPLER s0

#ifdef __cplusplus
using namespace DirectX;
#endif

#ifndef __cplusplus
#define XMFLOAT4 float4
#define XMFLOAT3 float3
#define XMFLOAT2 float2
#define XMMATRIX float4x4
#define UINT uint
#define XMINT2 int2
#endif

#define MATERIAL_NO_TEXTURE_INDEX (0xFFFFFFFF)

struct Vertex
{
  XMFLOAT3 position;
  XMFLOAT3 normal;
  XMFLOAT3 tangent;
  XMFLOAT2 uv;
  XMFLOAT4 color;
};

struct Material
{
  XMFLOAT4 color_emissive;
  XMFLOAT4 color_ambient;
  XMFLOAT4 color_diffuse;
  XMFLOAT4 color_specular;
  float opacity;
  float specular_scale;
  float specular_power;
  float bump_intensity;
  UINT emissive_map;
  UINT ambient_map;
  UINT diffuse_map;
  UINT specular_map;
  UINT specular_power_map;
  UINT bump_map;
  UINT normal_map;
  float index_of_refraction;
  UINT shading_model;
  float glossiness;
};

struct SceneConstantBuffer
{
  XMMATRIX projection_to_world;
  // boundary
  XMFLOAT4 camera_position;
  // boundary
  UINT gi_num_bounces;
  float gi_bounce_distance;
  float lens_diameter;
  UINT aa_enabled;
  // boundary
  XMFLOAT2 aa_sampling_point;
  UINT aa_algorithm;
  UINT ao_samples;
  // boundary
  UINT ao_size;
  UINT frame_count;
  XMINT2 picking_point;
  // boundary
  XMFLOAT4 sky_color;
};

struct AveragerConstantBuffer
{
  UINT clear_samples;
  float gamma;
  XMFLOAT2 padding;
};

struct Mesh
{
  UINT first_idx_vertices;
  UINT first_idx_indices;
  UINT material;
};

struct Light
{
  XMFLOAT3 intensity;
  XMFLOAT3 position;
};

#endif