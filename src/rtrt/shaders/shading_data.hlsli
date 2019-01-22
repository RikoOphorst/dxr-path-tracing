#ifndef SHADINGDATA_HLSL
#define SHADINGDATA_HLSL

ConstantBuffer<SceneConstantBuffer> scene_constants : register(HLSL_REGISTER_CONSTANTS);

RWTexture2D<float4> render_target : register(HLSL_REGISTER_OUTPUT);
RWStructuredBuffer<float4> normals_target : register(HLSL_REGISTER_NORMALS);
RWStructuredBuffer<float4> albedo_target : register(HLSL_REGISTER_ALBEDO);
RWStructuredBuffer<int> picking_buffer : register(HLSL_REGISTER_PICKING_BUFFER);

RaytracingAccelerationStructure scene_as : register(HLSL_REGISTER_ACCELERATION_STRUCT);
StructuredBuffer<Mesh> scene_meshes : register(HLSL_REGISTER_MESHES);
StructuredBuffer<Vertex> scene_vertices : register(HLSL_REGISTER_VERTICES);
StructuredBuffer<uint> scene_indices : register(HLSL_REGISTER_INDICES);
StructuredBuffer<Material> scene_materials : register(HLSL_REGISTER_MATERIALS);
Texture2D<float4> scene_textures[] : register(HLSL_REGISTER_TEXTURES);
StructuredBuffer<Light> scene_lights : register(HLSL_REGISTER_LIGHTS);

SamplerState scene_sampler : register(HLSL_REGISTER_SAMPLER);

typedef BuiltInTriangleIntersectionAttributes TriangleAttributes;

struct InterpolatedVertex
{
  float3 position;
  float3 normal;
  float3 tangent;
  float3 bitangent;
  float2 uv;
  float4 color;
};

struct Triangle
{
  Vertex vertices[3];
};

uint3 GetIndices()
{
  int prim_idx = PrimitiveIndex();
  int mesh_idx = InstanceID();

  return uint3(
    scene_meshes[mesh_idx].first_idx_vertices + scene_indices[scene_meshes[mesh_idx].first_idx_indices + (prim_idx * 3) + 0],
    scene_meshes[mesh_idx].first_idx_vertices + scene_indices[scene_meshes[mesh_idx].first_idx_indices + (prim_idx * 3) + 1],
    scene_meshes[mesh_idx].first_idx_vertices + scene_indices[scene_meshes[mesh_idx].first_idx_indices + (prim_idx * 3) + 2]
    );
}

Triangle GetTriangle()
{
  uint3 indices = GetIndices();

  Triangle tri;
  tri.vertices[0] = scene_vertices[indices.x];
  tri.vertices[1] = scene_vertices[indices.y];
  tri.vertices[2] = scene_vertices[indices.z];

  return tri;
}

InterpolatedVertex CalculateInterpolatedVertex(in Vertex v[3], in float2 barycentrics)
{
  float3 bary_factors = CalculateBarycentricalInterpolationFactors(barycentrics);

  InterpolatedVertex vertex;
  vertex.position = BarycentricInterpolation(v[0].position, v[1].position, v[2].position, bary_factors);
  vertex.normal = normalize(BarycentricInterpolation(v[0].normal, v[1].normal, v[2].normal, bary_factors));
  vertex.tangent = normalize(BarycentricInterpolation(v[0].tangent, v[1].tangent, v[2].tangent, bary_factors));
  vertex.bitangent = normalize(cross(vertex.normal, vertex.tangent));
  vertex.uv = BarycentricInterpolation(v[0].uv, v[1].uv, v[2].uv, bary_factors);
  vertex.color = BarycentricInterpolation(v[0].color, v[1].color, v[2].color, bary_factors);

  return vertex;
}

struct ShadingData
{
  uint shading_model;
  float3 position;
  float3 normal;
  float3 diffuse;
  float3 emissive;
  float index_of_refraction;
  float glossiness;
};

inline float4 SampleTexture(in SamplerState samplr, in Texture2D tex, in float2 uv)
{
  return tex.SampleLevel(samplr, uv, 0, 0);
}

inline ShadingData GetShadingData(TriangleAttributes attr)
{
  ShadingData data;

  Triangle tri = GetTriangle();
  InterpolatedVertex vertex = CalculateInterpolatedVertex(tri.vertices, attr.barycentrics);
  Material material = scene_materials[scene_meshes[InstanceID()].material];

  data.shading_model = material.shading_model;
  data.position = WorldRayOrigin() + (WorldRayDirection() * RayTCurrent());
  data.normal = normalize(vertex.normal);
  data.diffuse = material.diffuse_map != MATERIAL_NO_TEXTURE_INDEX ? SampleTexture(scene_sampler, scene_textures[material.diffuse_map], vertex.uv).xyz : material.color_diffuse.xyz;
  data.emissive = material.emissive_map != MATERIAL_NO_TEXTURE_INDEX ? SampleTexture(scene_sampler, scene_textures[material.emissive_map], vertex.uv).xyz : material.color_emissive.xyz;
  data.index_of_refraction = material.index_of_refraction;
  data.glossiness = material.glossiness;

  return data;
}

#endif // SHADINGDATA_HLSL