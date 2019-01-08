#include <iostream>
#include <optix_world.h>

#include "application.h"
#include "imgui_layer.h"
#include "acceleration_structure.h"
#include "device.h"
#include "root_signatures.h"
#include "buffer.h"
#include "upload_buffer.h"
#include "readback_buffer.h"
#include "descriptor_heap.h"
#include "model.h"
#include "camera.h"
#include "shader_table.h"
#include "texture_loader.h"
#include "shared/raytracing_data.h"

#include "compiled-shaders/rt/raytrace.cso.h"
#include "compiled-shaders/rt/pathtrace.cso.h"
#include "compiled-shaders/rt/picking.cso.h"
#include "compiled-shaders/cs/averager.cso.h"

using namespace rtrt;

namespace GlobalRootSignatureParams
{
  enum Enum
  {
    AccelerationStructure = 0,
    OutputTexture,
    OutputNormals,
    OutputAlbedo,
    SceneConstants,
    Materials,
    Textures,
    Meshes,
    Vertices,
    Indices,
    Lights,
    PickingBuffer,
    Count
  };
}

namespace AveragerRootSignatureParams
{
  enum Enum
  {
    InputTexture = 0,
    InputNormals,
    InputAlbedo,
    OutputTexture,
    OutputBuffer,
    OutputNormals,
    OutputAlbedo,
    Constants,
    Count
  };
}

union AlignedSceneConstantBuffer
{
  SceneConstantBuffer buffer;
  uint8_t alignment_padding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
};

union AlignedAveragerConstantBuffer
{
  AveragerConstantBuffer buffer;
  uint8_t alignment_padding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
};

int main(int argc, char** argv)
{
  ID3D12RootSignature* global_root_signature = nullptr;
  ID3D12RaytracingFallbackStateObject* pso = nullptr;

  ID3D12Resource* target_render = nullptr;
  ID3D12Resource* target_normals = nullptr;
  ID3D12Resource* target_albedo_target = nullptr;
  DescriptorHandle target_render_descriptor;
  DescriptorHandle target_normals_descriptor;
  DescriptorHandle target_albedo_descriptor;

  std::vector<Mesh> meshes;

  Buffer* meshes_buffer = nullptr;
  Buffer* all_vertices_buffer = nullptr;
  Buffer* all_indices_buffer = nullptr;

  std::vector<Material> materials;
  Buffer* materials_buffer = nullptr;
  DescriptorHandle materials_descriptor = {};

  std::vector<ID3D12Resource*> textures;
  std::vector<DescriptorHandle> texture_descriptors;

  std::vector<Buffer*> vertex_buffers;
  std::vector<Buffer*> index_buffers;

  AccelerationStructure bottom_level_acceleration_structures = {};
  AccelerationStructure top_level_acceleration_structure = {};

  ShaderTable* shader_table_ray_generation = nullptr;
  ShaderTable* shader_table_hit = nullptr;
  ShaderTable* shader_table_miss = nullptr;

  UploadBuffer* scene_constants_buffer = nullptr;
  SceneConstantBuffer constant_buffer_data[Device::NUM_BACK_BUFFERS] = {};

  UploadBuffer* lights_buffer = nullptr;
  std::vector<Light> lights;

  ID3D12RootSignature* averager_root_signature = nullptr;
  ID3D12PipelineState* averager_pso = nullptr;
  ID3D12Resource* averager_texture = nullptr;
  ID3D12Resource* averager_buffer = nullptr;
  ID3D12Resource* averager_normals = nullptr;
  ID3D12Resource* averager_albedo = nullptr;
  DescriptorHandle averager_texture_descriptor;
  DescriptorHandle averager_buffer_descriptor;
  DescriptorHandle averager_normals_descriptor;
  DescriptorHandle averager_albedo_descriptor;
  ReadbackBuffer* averager_buffer_readback = nullptr;
  ReadbackBuffer* averager_normals_readback = nullptr;
  ReadbackBuffer* averager_albedo_readback = nullptr;
  UploadBuffer* averager_constants_buffer = nullptr;

  AveragerConstantBuffer averager_constants[Device::NUM_BACK_BUFFERS] = {};

  ID3D12RaytracingFallbackStateObject* picking_pso = nullptr;
  ShaderTable* picking_shader_table_ray_generation = nullptr;
  ShaderTable* picking_shader_table_hit = nullptr;
  ShaderTable* picking_shader_table_miss = nullptr;
  Buffer* picking_buffer = nullptr;
  ReadbackBuffer* picking_buffer_readback = nullptr;
  DescriptorHandle picking_buffer_descriptor;

  Application app;
  GLFWwindow* window = nullptr;
  Device device;
  ImGuiLayer imgui_layer;

  optix::Context optix;
  optix::Buffer optix_input;
  optix::Buffer optix_normals;
  optix::Buffer optix_albedo;
  optix::Buffer optix_output;
  optix::CommandList optix_list;
  optix::PostprocessingStage optix_denoiser_stage;

  // App
  {
    app.Initialize();
  }

  // GLFW 
  {
    glfwInit();
    window = glfwCreateWindow(1280, 720, "RTRT", nullptr, nullptr);
  }

  // Device
  {
    device.Initialize(window);
  }

  // Imgui
  {
    imgui_layer.Init(window, &device, device.rtv_heap, device.srv_heap);
  }

  // Pathtracing global root signature
  {
    CD3DX12_DESCRIPTOR_RANGE ranges[5];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, CPP_REGISTER_OUTPUT);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1024, CPP_REGISTER_TEXTURES);
    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, CPP_REGISTER_PICKING_BUFFER);
    ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, CPP_REGISTER_NORMALS);
    ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, CPP_REGISTER_ALBEDO);

    CD3DX12_ROOT_PARAMETER root_parameters[GlobalRootSignatureParams::Count];
    root_parameters[GlobalRootSignatureParams::SceneConstants].InitAsConstantBufferView(0);

    root_parameters[GlobalRootSignatureParams::OutputTexture].InitAsDescriptorTable(1, &ranges[0]);
    root_parameters[GlobalRootSignatureParams::OutputNormals].InitAsDescriptorTable(1, &ranges[3]);
    root_parameters[GlobalRootSignatureParams::OutputAlbedo].InitAsDescriptorTable(1, &ranges[4]);

    root_parameters[GlobalRootSignatureParams::AccelerationStructure].InitAsShaderResourceView(CPP_REGISTER_ACCELERATION_STRUCT);
    root_parameters[GlobalRootSignatureParams::Meshes].InitAsShaderResourceView(CPP_REGISTER_MESHES);
    root_parameters[GlobalRootSignatureParams::Vertices].InitAsShaderResourceView(CPP_REGISTER_VERTICES);
    root_parameters[GlobalRootSignatureParams::Indices].InitAsShaderResourceView(CPP_REGISTER_INDICES);
    root_parameters[GlobalRootSignatureParams::Materials].InitAsShaderResourceView(CPP_REGISTER_MATERIALS);
    root_parameters[GlobalRootSignatureParams::Textures].InitAsDescriptorTable(1, &ranges[1]);
    root_parameters[GlobalRootSignatureParams::Lights].InitAsShaderResourceView(CPP_REGISTER_LIGHTS);
    root_parameters[GlobalRootSignatureParams::PickingBuffer].InitAsDescriptorTable(1, &ranges[2]);

    D3D12_STATIC_SAMPLER_DESC sampler;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    sampler.Filter = D3D12_FILTER_ANISOTROPIC;
    sampler.MaxAnisotropy = 16;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.MinLOD = 0.0f;
    sampler.MipLODBias = 0.0f;
    sampler.RegisterSpace = 0;
    sampler.ShaderRegister = CPP_REGISTER_SAMPLER;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc(ARRAYSIZE(root_parameters), root_parameters, 1, &sampler);
    global_root_signature = RootSignatureFactory::BuildRootSignature(device.fallback_device, &root_signature_desc);
  }

  // Pathtracing PSO
  {
    CD3D12_STATE_OBJECT_DESC pso_desc;
    pso_desc.SetStateObjectType(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    auto dxil_lib_subobject = pso_desc.CreateSubobject<CD3D12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE dxil_lib = CD3DX12_SHADER_BYTECODE(cso_pathtrace, ARRAYSIZE(cso_pathtrace));
    dxil_lib_subobject->SetDXILLibrary(&dxil_lib);

    auto color_hit_group_subobject = pso_desc.CreateSubobject<CD3D12_HIT_GROUP_SUBOBJECT>();
    color_hit_group_subobject->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    color_hit_group_subobject->SetClosestHitShaderImport(L"ColorHit");
    color_hit_group_subobject->SetHitGroupExport(L"ColorHitGroup");

    auto geometry_hit_group_subobject = pso_desc.CreateSubobject<CD3D12_HIT_GROUP_SUBOBJECT>();
    geometry_hit_group_subobject->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    geometry_hit_group_subobject->SetClosestHitShaderImport(L"GeometryHit");
    geometry_hit_group_subobject->SetHitGroupExport(L"GeometryHitGroup");

    auto global_root_signature_subobject = pso_desc.CreateSubobject<CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    global_root_signature_subobject->SetRootSignature(global_root_signature);

    auto shader_config_subobject = pso_desc.CreateSubobject<CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shader_config_subobject->Config(6 * sizeof(float), 2 * sizeof(float));

    auto pipeline_config_subobject = pso_desc.CreateSubobject<CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipeline_config_subobject->Config(31);

    ThrowIfFailed(device.fallback_device->CreateStateObject(pso_desc, IID_PPV_ARGS(&pso)));
  }

  // Pathtracing shader tables
  {
    UINT shader_identifier_size = device.fallback_device->GetShaderIdentifierSize();

    shader_table_ray_generation = new ShaderTable(device.device, 1, shader_identifier_size);
    shader_table_ray_generation->Add(ShaderRecord(pso->GetShaderIdentifier(L"PrimaryRaygeneration"), shader_identifier_size, nullptr, 0));

    shader_table_hit = new ShaderTable(device.device, 2, shader_identifier_size);
    shader_table_hit->Add(ShaderRecord(pso->GetShaderIdentifier(L"ColorHitGroup"), shader_identifier_size, nullptr, 0));
    shader_table_hit->Add(ShaderRecord(pso->GetShaderIdentifier(L"GeometryHitGroup"), shader_identifier_size, nullptr, 0));

    shader_table_miss = new ShaderTable(device.device, 2, shader_identifier_size);
    shader_table_miss->Add(ShaderRecord(pso->GetShaderIdentifier(L"ColorMiss"), shader_identifier_size, nullptr, 0));
    shader_table_miss->Add(ShaderRecord(pso->GetShaderIdentifier(L"GeometryMiss"), shader_identifier_size, nullptr, 0));
  }

  // Pathtracing render targets & readbacks
  {
    // Render target
    {
      device.device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 1280, 720, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&target_render)
      );

      D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
      uav_desc.Texture2D.MipSlice = 0;
      uav_desc.Texture2D.PlaneSlice = 0;
      uav_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

      device.uav_heap->CreateDescriptor(device.device, target_render, nullptr, &uav_desc, &target_render_descriptor);
    }

    // Normals target
    {
      device.device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(1280 * 720 * 16, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&target_normals)
      );

      D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
      uav_desc.Buffer.CounterOffsetInBytes = 0;
      uav_desc.Buffer.FirstElement = 0;
      uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
      uav_desc.Buffer.NumElements = 1280 * 720;
      uav_desc.Buffer.StructureByteStride = 16;
      uav_desc.Format = DXGI_FORMAT_UNKNOWN;

      device.uav_heap->CreateDescriptor(device.device, target_normals, nullptr, &uav_desc, &target_normals_descriptor);
    }

    // Albedo target
    {
      device.device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE, 
        &CD3DX12_RESOURCE_DESC::Buffer(1280 * 720 * 16, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
        nullptr, 
        IID_PPV_ARGS(&target_albedo_target)
      );

      D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
      uav_desc.Buffer.CounterOffsetInBytes = 0;
      uav_desc.Buffer.FirstElement = 0;
      uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
      uav_desc.Buffer.NumElements = 1280 * 720;
      uav_desc.Buffer.StructureByteStride = 16;
      uav_desc.Format = DXGI_FORMAT_UNKNOWN;

      device.uav_heap->CreateDescriptor(device.device, target_albedo_target, nullptr, &uav_desc, &target_albedo_descriptor);
    }
  }

  // Model loading
  {
    for (size_t i = 0; i < app.model.meshes.size(); i++)
    {
      Model::Mesh& mesh = app.model.meshes[i];

      vertex_buffers.push_back(new Buffer());
      vertex_buffers[i]->Create(&device, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, static_cast<UINT>(mesh.vertices.size() * sizeof(Vertex)), mesh.vertices.data());

      index_buffers.push_back(new Buffer());
      index_buffers[i]->Create(&device, D3D12_RESOURCE_STATE_INDEX_BUFFER, static_cast<UINT>(mesh.indices.size() * sizeof(Index)), mesh.indices.data());
    }

    textures.resize(app.model.textures.size());
    texture_descriptors.resize(app.model.textures.size());
    for (size_t i = 0; i < app.model.textures.size(); i++)
    {
      TextureLoader::LoadTexture(device.device, device.command_queue, app.model.textures[i].path, &textures[i]);

      D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
      srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srv_desc.Format = DXGI_FORMAT_UNKNOWN;
      srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srv_desc.Texture2D.MipLevels = 1;
      srv_desc.Texture2D.MostDetailedMip = 0;
      srv_desc.Texture2D.PlaneSlice = 0;
      srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

      device.srv_heap->CreateDescriptor(device.device, textures[i], &srv_desc, &texture_descriptors[i]);
    }

    materials.resize(app.model.materials.size());
    for (size_t i = 0; i < app.model.materials.size(); i++)
    {
      materials[i].color_emissive = app.model.materials[i].color_emissive;
      materials[i].color_ambient = app.model.materials[i].color_ambient;
      materials[i].color_diffuse = app.model.materials[i].color_diffuse;
      materials[i].color_specular = app.model.materials[i].color_specular;
      materials[i].opacity = app.model.materials[i].opacity;
      materials[i].specular_scale = app.model.materials[i].specular_scale;
      materials[i].specular_power = app.model.materials[i].specular_power;
      materials[i].bump_intensity = app.model.materials[i].bump_intensity;
      materials[i].emissive_map = app.model.materials[i].emissive_map;
      materials[i].ambient_map = app.model.materials[i].ambient_map;
      materials[i].diffuse_map = app.model.materials[i].diffuse_map;
      materials[i].specular_map = app.model.materials[i].specular_map;
      materials[i].specular_power_map = app.model.materials[i].specular_power_map;
      materials[i].bump_map = app.model.materials[i].bump_map;
      materials[i].normal_map = app.model.materials[i].normal_map;
      materials[i].index_of_refraction = app.model.materials[i].index_of_refraction;
      materials[i].shading_model = app.model.materials[i].shading_model;
    }

    materials_buffer = new Buffer();
    materials_buffer->Create(&device, D3D12_RESOURCE_STATE_GENERIC_READ, static_cast<UINT>(app.model.materials.size() * sizeof(Material)), materials.data());

    meshes_buffer = new Buffer();
    all_vertices_buffer = new Buffer();
    all_indices_buffer = new Buffer();

    meshes.resize(app.model.meshes.size());
    std::vector<Vertex> all_vertices;
    std::vector<Index> all_indices;

    for (int i = 0; i < app.model.meshes.size(); i++)
    {
      meshes[i].first_idx_vertices = static_cast<UINT>(all_vertices.size());
      meshes[i].first_idx_indices = static_cast<UINT>(all_indices.size());
      meshes[i].material = app.model.meshes[i].material;

      all_vertices.insert(all_vertices.end(), app.model.meshes[i].vertices.begin(), app.model.meshes[i].vertices.end());
      all_indices.insert(all_indices.end(), app.model.meshes[i].indices.begin(), app.model.meshes[i].indices.end());
    }

    meshes_buffer->Create(&device, D3D12_RESOURCE_STATE_GENERIC_READ, static_cast<UINT>(meshes.size() * sizeof(Mesh)), meshes.data());
    all_vertices_buffer->Create(&device, D3D12_RESOURCE_STATE_GENERIC_READ, static_cast<UINT>(all_vertices.size() * sizeof(Vertex)), all_vertices.data());
    all_indices_buffer->Create(&device, D3D12_RESOURCE_STATE_GENERIC_READ, static_cast<UINT>(all_indices.size() * sizeof(Index)), all_indices.data());
  }

  // Acceleration structures
  {
    AccelerationStructureUtility::BuildMultipleBLASesFromModel(&device, device.cbv_srv_uav_heap, app.model, vertex_buffers, index_buffers, &bottom_level_acceleration_structures);
    AccelerationStructureUtility::BuildSingleTLASFromModel(&device, device.cbv_srv_uav_heap, app.model, bottom_level_acceleration_structures, &top_level_acceleration_structure);
  }

  // Constant buffers 
  {
    scene_constants_buffer = new UploadBuffer();
    scene_constants_buffer->Create(device.device, sizeof(AlignedSceneConstantBuffer) * Device::NUM_BACK_BUFFERS, nullptr);
  }

  // Lights
  {
    Light light01;
    light01.intensity = DirectX::XMFLOAT3(2.0f, 2.0f, 2.0f);
    light01.position = DirectX::XMFLOAT3(0.0f, 255.0f, 0.0f);

    Light light02;
    light02.intensity = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    light02.position = DirectX::XMFLOAT3(5000.0f, 10000.0f, 5000.0f);

    lights.push_back(light01);
    lights.push_back(light02);

    lights_buffer = new UploadBuffer();
    lights_buffer->Create(device.device, static_cast<UINT>(sizeof(Light) * lights.size()), lights.data());
  }

  // Averager root signature
  {
    CD3DX12_DESCRIPTOR_RANGE ranges[7];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
    ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);
    ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4);
    ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);
    ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 6);

    CD3DX12_ROOT_PARAMETER root_parameters[AveragerRootSignatureParams::Count];
    root_parameters[AveragerRootSignatureParams::InputTexture].InitAsDescriptorTable(1, &ranges[0]);
    root_parameters[AveragerRootSignatureParams::InputNormals].InitAsDescriptorTable(1, &ranges[1]);
    root_parameters[AveragerRootSignatureParams::InputAlbedo].InitAsDescriptorTable(1, &ranges[2]);
    root_parameters[AveragerRootSignatureParams::OutputTexture].InitAsDescriptorTable(1, &ranges[3]);
    root_parameters[AveragerRootSignatureParams::OutputBuffer].InitAsDescriptorTable(1, &ranges[4]);
    root_parameters[AveragerRootSignatureParams::OutputNormals].InitAsDescriptorTable(1, &ranges[5]);
    root_parameters[AveragerRootSignatureParams::OutputAlbedo].InitAsDescriptorTable(1, &ranges[6]);
    root_parameters[AveragerRootSignatureParams::Constants].InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc(ARRAYSIZE(root_parameters), root_parameters);
    averager_root_signature = RootSignatureFactory::BuildRootSignature(device.device, &root_signature_desc);
  }

  // Averager pso
  {
    D3D12_COMPUTE_PIPELINE_STATE_DESC accum_pso_desc = {};

    accum_pso_desc.CS = CD3DX12_SHADER_BYTECODE(cso_averager, ARRAYSIZE(cso_averager));
    accum_pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    accum_pso_desc.NodeMask = 0;
    accum_pso_desc.pRootSignature = averager_root_signature;

    device.device->CreateComputePipelineState(&accum_pso_desc, IID_PPV_ARGS(&averager_pso));
  }

  // Averager output
  {
    {
      D3D12_RESOURCE_DESC averager_output_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1280, 720, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
      CD3DX12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE_DEFAULT);
      device.device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE, 
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1280, 720, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
        nullptr, 
        IID_PPV_ARGS(&averager_texture)
      );

      D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
      uav_desc.Texture2D.MipSlice = 0;
      uav_desc.Texture2D.PlaneSlice = 0;
      uav_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

      device.uav_heap->CreateDescriptor(device.device, averager_texture, nullptr, &uav_desc, &averager_texture_descriptor);
    }

    {
      device.device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(1280 * 720 * 16, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&averager_buffer)
      );

      D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
      uav_desc.Buffer.CounterOffsetInBytes = 0;
      uav_desc.Buffer.FirstElement = 0;
      uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
      uav_desc.Buffer.NumElements = 1280 * 720;
      uav_desc.Buffer.StructureByteStride = 16;
      uav_desc.Format = DXGI_FORMAT_UNKNOWN;

      device.uav_heap->CreateDescriptor(device.device, averager_buffer, nullptr, &uav_desc, &averager_buffer_descriptor);
    }

    {
      device.device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(1280 * 720 * 16, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&averager_normals)
      );

      D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
      uav_desc.Buffer.CounterOffsetInBytes = 0;
      uav_desc.Buffer.FirstElement = 0;
      uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
      uav_desc.Buffer.NumElements = 1280 * 720;
      uav_desc.Buffer.StructureByteStride = 16;
      uav_desc.Format = DXGI_FORMAT_UNKNOWN;

      device.uav_heap->CreateDescriptor(device.device, averager_normals, nullptr, &uav_desc, &averager_normals_descriptor);
    }

    {
      device.device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(1280 * 720 * 16, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&averager_albedo)
      );

      D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
      uav_desc.Buffer.CounterOffsetInBytes = 0;
      uav_desc.Buffer.FirstElement = 0;
      uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
      uav_desc.Buffer.NumElements = 1280 * 720;
      uav_desc.Buffer.StructureByteStride = 16;
      uav_desc.Format = DXGI_FORMAT_UNKNOWN;

      device.uav_heap->CreateDescriptor(device.device, averager_albedo, nullptr, &uav_desc, &averager_albedo_descriptor);
    }
  }

  // Averager constants
  {
    averager_constants_buffer = new UploadBuffer();
    averager_constants_buffer->Create(device.device, sizeof(AlignedAveragerConstantBuffer) * Device::NUM_BACK_BUFFERS, nullptr);
  }

  // Picking pso
  {
    CD3D12_STATE_OBJECT_DESC pso_desc;
    pso_desc.SetStateObjectType(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    auto dxil_lib_subobject = pso_desc.CreateSubobject<CD3D12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE dxil_lib = CD3DX12_SHADER_BYTECODE(cso_picking, ARRAYSIZE(cso_picking));
    dxil_lib_subobject->SetDXILLibrary(&dxil_lib);

    auto primary_hit_group_subobject = pso_desc.CreateSubobject<CD3D12_HIT_GROUP_SUBOBJECT>();
    primary_hit_group_subobject->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    primary_hit_group_subobject->SetClosestHitShaderImport(L"PickingHit");
    primary_hit_group_subobject->SetHitGroupExport(L"PickingHitGroup");

    auto global_root_signature_subobject = pso_desc.CreateSubobject<CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    global_root_signature_subobject->SetRootSignature(global_root_signature);

    auto shader_config_subobject = pso_desc.CreateSubobject<CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shader_config_subobject->Config(1 * sizeof(UINT), 2 * sizeof(float));

    auto pipeline_config_subobject = pso_desc.CreateSubobject<CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipeline_config_subobject->Config(1);

    ThrowIfFailed(device.fallback_device->CreateStateObject(pso_desc, IID_PPV_ARGS(&picking_pso)));
  }

  // Picking shader tables
  {
    UINT shader_identifier_size = device.fallback_device->GetShaderIdentifierSize();

    picking_shader_table_ray_generation = new ShaderTable(device.device, 1, shader_identifier_size);
    picking_shader_table_ray_generation->Add(ShaderRecord(picking_pso->GetShaderIdentifier(L"PickingRaygeneration"), shader_identifier_size, nullptr, 0));

    picking_shader_table_hit = new ShaderTable(device.device, 1, shader_identifier_size);
    picking_shader_table_hit->Add(ShaderRecord(picking_pso->GetShaderIdentifier(L"PickingHitGroup"), shader_identifier_size, nullptr, 0));

    picking_shader_table_miss = new ShaderTable(device.device, 1, shader_identifier_size);
    picking_shader_table_miss->Add(ShaderRecord(picking_pso->GetShaderIdentifier(L"PickingMiss"), shader_identifier_size, nullptr, 0));
  }

  // Picking buffer
  {
    picking_buffer = new Buffer();
    picking_buffer->Create(&device, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 4);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.CounterOffsetInBytes = 0;
    uav_desc.Buffer.FirstElement = 0;
    uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    uav_desc.Buffer.NumElements = 1;
    uav_desc.Buffer.StructureByteStride = 4;
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;

    device.uav_heap->CreateDescriptor(device.device, picking_buffer->GetBuffer(), nullptr, &uav_desc, &picking_buffer_descriptor);

    picking_buffer_readback = new ReadbackBuffer();
    picking_buffer_readback->Create(device.device, 4);
  }

  // OptiX
  {
    try
    {
      optix = optix::Context::create();
      optix_input = optix->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, 1280, 720);
      optix_normals = optix->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, 1280, 720);
      optix_albedo = optix->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, 1280, 720);
      optix_output = optix->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, 1280, 720);

      optix_denoiser_stage = optix->createBuiltinPostProcessingStage("DLDenoiser");
      optix_denoiser_stage->declareVariable("input_buffer")->set(optix_input);
      optix_denoiser_stage->declareVariable("input_normal_buffer")->set(optix_normals);
      optix_denoiser_stage->declareVariable("input_albedo_buffer")->set(optix_albedo);
      optix_denoiser_stage->declareVariable("output_buffer")->set(optix_output);

      optix_list = optix->createCommandList();
      optix_list->appendPostprocessingStage(optix_denoiser_stage, 1280, 720);
      optix_list->finalize();

      optix->validate();
      optix->compile();

      // Immediately execute denoising with empty inputs...
      // the first execute of optix takes ages (not sure why)
      // so it's better to execute during init time
      optix_list->execute();
    }
    catch (optix::Exception e)
    {
      std::cout << e.getErrorString() << std::endl;
    }

    averager_buffer_readback = new ReadbackBuffer();
    averager_buffer_readback->Create(device.device, 1280 * 720 * 16);
    averager_normals_readback = new ReadbackBuffer();
    averager_normals_readback->Create(device.device, 1280 * 720 * 16);
    averager_albedo_readback = new ReadbackBuffer();
    averager_albedo_readback->Create(device.device, 1280 * 720 * 16);
  }

  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    device.PrepareCommandLists();
    imgui_layer.NewFrame();

    // Picking rays
    {
      device.command_list->SetComputeRootSignature(global_root_signature);
      device.command_list->SetComputeRootConstantBufferView(GlobalRootSignatureParams::SceneConstants, scene_constants_buffer->GetBuffer()->GetGPUVirtualAddress() + device.back_buffer_index * sizeof(AlignedSceneConstantBuffer));
      device.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputTexture, target_render_descriptor);
      device.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputNormals, target_normals_descriptor);
      device.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputAlbedo, target_albedo_descriptor);
      device.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::PickingBuffer, picking_buffer_descriptor);
      device.command_list->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Materials, materials_buffer->GetBuffer()->GetGPUVirtualAddress());
      device.command_list->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Meshes, meshes_buffer->GetBuffer()->GetGPUVirtualAddress());
      device.command_list->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Vertices, all_vertices_buffer->GetBuffer()->GetGPUVirtualAddress());
      device.command_list->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Indices, all_indices_buffer->GetBuffer()->GetGPUVirtualAddress());
      device.command_list->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Lights, lights_buffer->GetBuffer()->GetGPUVirtualAddress());
      if (texture_descriptors.size() > 0)
      {
        device.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::Textures, texture_descriptors[0]);
      }
      device.fallback_command_list->SetTopLevelAccelerationStructure(GlobalRootSignatureParams::AccelerationStructure, top_level_acceleration_structure.structure_pointers[0]);
      device.fallback_command_list->SetPipelineState1(picking_pso);

      D3D12_DISPATCH_RAYS_DESC raytracing_dispatch = {};
      raytracing_dispatch.Width = 1;
      raytracing_dispatch.Height = 1;
      raytracing_dispatch.Depth = 1;

      raytracing_dispatch.HitGroupTable.StartAddress = picking_shader_table_hit->GetBuffer()->GetGPUVirtualAddress();
      raytracing_dispatch.HitGroupTable.SizeInBytes = picking_shader_table_hit->GetSizeInBytes();
      raytracing_dispatch.HitGroupTable.StrideInBytes = picking_shader_table_hit->GetStrideInBytes();

      raytracing_dispatch.MissShaderTable.StartAddress = picking_shader_table_miss->GetBuffer()->GetGPUVirtualAddress();
      raytracing_dispatch.MissShaderTable.SizeInBytes = picking_shader_table_miss->GetSizeInBytes();
      raytracing_dispatch.MissShaderTable.StrideInBytes = picking_shader_table_miss->GetStrideInBytes();

      raytracing_dispatch.RayGenerationShaderRecord.StartAddress = picking_shader_table_ray_generation->GetBuffer()->GetGPUVirtualAddress();
      raytracing_dispatch.RayGenerationShaderRecord.SizeInBytes = picking_shader_table_ray_generation->GetSizeInBytes();

      device.fallback_command_list->DispatchRays(&raytracing_dispatch);
    }

    device.ExecuteCommandLists();
    device.WaitForGPU();
    device.PrepareCommandLists();

    // Picking buffer copy
    {
      D3D12_RESOURCE_BARRIER pre_copy_barriers[1];
      pre_copy_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(picking_buffer->GetBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
      device.command_list->ResourceBarrier(ARRAYSIZE(pre_copy_barriers), pre_copy_barriers);

      device.command_list->CopyResource(picking_buffer_readback->GetBuffer(), picking_buffer->GetBuffer());

      D3D12_RESOURCE_BARRIER post_copy_barriers[1];
      post_copy_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(picking_buffer->GetBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      device.command_list->ResourceBarrier(ARRAYSIZE(post_copy_barriers), post_copy_barriers);

      device.ExecuteCommandLists();
      device.WaitForGPU();
    }

    picking_buffer_readback->Map();
    app.Update(window, *static_cast<int*>(picking_buffer_readback->GetData()));
    picking_buffer_readback->Unmap();

    if (app.materials_dirty)
    {
      materials.resize(app.model.materials.size());
      for (size_t i = 0; i < app.model.materials.size(); i++)
      {
        materials[i].color_emissive = app.model.materials[i].color_emissive;
        materials[i].color_ambient = app.model.materials[i].color_ambient;
        materials[i].color_diffuse = app.model.materials[i].color_diffuse;
        materials[i].color_specular = app.model.materials[i].color_specular;
        materials[i].opacity = app.model.materials[i].opacity;
        materials[i].specular_scale = app.model.materials[i].specular_scale;
        materials[i].specular_power = app.model.materials[i].specular_power;
        materials[i].bump_intensity = app.model.materials[i].bump_intensity;
        materials[i].emissive_map = app.model.materials[i].emissive_map;
        materials[i].ambient_map = app.model.materials[i].ambient_map;
        materials[i].diffuse_map = app.model.materials[i].diffuse_map;
        materials[i].specular_map = app.model.materials[i].specular_map;
        materials[i].specular_power_map = app.model.materials[i].specular_power_map;
        materials[i].bump_map = app.model.materials[i].bump_map;
        materials[i].normal_map = app.model.materials[i].normal_map;
        materials[i].index_of_refraction = app.model.materials[i].index_of_refraction;
        materials[i].shading_model = app.model.materials[i].shading_model;
      }


      DELETE(materials_buffer);
      materials_buffer = new Buffer();
      materials_buffer->Create(&device, D3D12_RESOURCE_STATE_GENERIC_READ, static_cast<UINT>(app.model.materials.size() * sizeof(Material)), materials.data());
    }

    device.PrepareCommandLists();

    if (app.sample_count < 100)
    {
      if (!app.freeze_rendering)
      {
        DirectX::XMMATRIX view_projection = app.camera->GetViewMatrix() * app.camera->GetProjectionMatrix();
        constant_buffer_data[device.back_buffer_index].projection_to_world = DirectX::XMMatrixInverse(nullptr, view_projection);
        constant_buffer_data[device.back_buffer_index].camera_position = DirectX::XMFLOAT4(app.camera->GetPosition().x, app.camera->GetPosition().y, app.camera->GetPosition().z, 1.0f);
        constant_buffer_data[device.back_buffer_index].frame_count = app.frame_count;
        constant_buffer_data[device.back_buffer_index].lens_diameter = app.lens.lens_diameter;
        constant_buffer_data[device.back_buffer_index].gi_num_bounces = app.gi.num_bounces;
        constant_buffer_data[device.back_buffer_index].gi_bounce_distance = app.gi.bounce_distance;
        constant_buffer_data[device.back_buffer_index].aa_enabled = app.aa.enabled ? 1 : 0;
        constant_buffer_data[device.back_buffer_index].aa_algorithm = static_cast<UINT>(app.aa.algorithm);
        constant_buffer_data[device.back_buffer_index].aa_sampling_point = app.aa.sample_point;
        constant_buffer_data[device.back_buffer_index].sky_color = app.sky_color;
        constant_buffer_data[device.back_buffer_index].picking_point = DirectX::XMINT2(static_cast<int>(std::min(std::max(app.current_cursor_position.x, 0.0f), 1280.0f)), static_cast<int>(std::min(std::max(app.current_cursor_position.y, 0.0f), 720.0f)));

        scene_constants_buffer->Write(sizeof(SceneConstantBuffer), &(constant_buffer_data[device.back_buffer_index]), sizeof(AlignedSceneConstantBuffer) * device.back_buffer_index);

        // Resource binding for pathtracing
        {
          device.command_list->SetComputeRootSignature(global_root_signature);
          device.command_list->SetComputeRootConstantBufferView(GlobalRootSignatureParams::SceneConstants, scene_constants_buffer->GetBuffer()->GetGPUVirtualAddress() + device.back_buffer_index * sizeof(AlignedSceneConstantBuffer));
          device.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputTexture, target_render_descriptor);
          device.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputNormals, target_normals_descriptor);
          device.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputAlbedo, target_albedo_descriptor);
          device.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::PickingBuffer, picking_buffer_descriptor);
          device.command_list->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Materials, materials_buffer->GetBuffer()->GetGPUVirtualAddress());
          device.command_list->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Meshes, meshes_buffer->GetBuffer()->GetGPUVirtualAddress());
          device.command_list->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Vertices, all_vertices_buffer->GetBuffer()->GetGPUVirtualAddress());
          device.command_list->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Indices, all_indices_buffer->GetBuffer()->GetGPUVirtualAddress());
          device.command_list->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Lights, lights_buffer->GetBuffer()->GetGPUVirtualAddress());
          if (texture_descriptors.size() > 0)
          {
            device.command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::Textures, texture_descriptors[0]);
          }
          device.fallback_command_list->SetTopLevelAccelerationStructure(GlobalRootSignatureParams::AccelerationStructure, top_level_acceleration_structure.structure_pointers[0]);
          device.fallback_command_list->SetPipelineState1(pso);
        }

        // Dispatch rays for pathtracing
        {
          D3D12_DISPATCH_RAYS_DESC raytracing_dispatch = {};
          raytracing_dispatch.Width = 1280;
          raytracing_dispatch.Height = 720;
          raytracing_dispatch.Depth = 1;

          raytracing_dispatch.HitGroupTable.StartAddress = shader_table_hit->GetBuffer()->GetGPUVirtualAddress();
          raytracing_dispatch.HitGroupTable.SizeInBytes = shader_table_hit->GetSizeInBytes();
          raytracing_dispatch.HitGroupTable.StrideInBytes = shader_table_hit->GetStrideInBytes();

          raytracing_dispatch.MissShaderTable.StartAddress = shader_table_miss->GetBuffer()->GetGPUVirtualAddress();
          raytracing_dispatch.MissShaderTable.SizeInBytes = shader_table_miss->GetSizeInBytes();
          raytracing_dispatch.MissShaderTable.StrideInBytes = shader_table_miss->GetStrideInBytes();

          raytracing_dispatch.RayGenerationShaderRecord.StartAddress = shader_table_ray_generation->GetBuffer()->GetGPUVirtualAddress();
          raytracing_dispatch.RayGenerationShaderRecord.SizeInBytes = shader_table_ray_generation->GetSizeInBytes();

          device.fallback_command_list->DispatchRays(&raytracing_dispatch);
        }

        // Perform an averaging pass in compute (averages out all samples)
        {
          averager_constants[device.back_buffer_index].clear_samples = app.clear_samples ? 0 : 1;
          averager_constants[device.back_buffer_index].gamma = app.pp.gamma;
          averager_constants_buffer->Write(sizeof(AveragerConstantBuffer), &(averager_constants[device.back_buffer_index]), sizeof(AlignedAveragerConstantBuffer) * device.back_buffer_index);

          device.command_list->SetPipelineState(averager_pso);
          device.command_list->SetComputeRootSignature(averager_root_signature);
          device.command_list->SetComputeRootDescriptorTable(AveragerRootSignatureParams::InputTexture, target_render_descriptor);
          device.command_list->SetComputeRootDescriptorTable(AveragerRootSignatureParams::InputNormals, target_normals_descriptor);
          device.command_list->SetComputeRootDescriptorTable(AveragerRootSignatureParams::InputAlbedo, target_albedo_descriptor);
          device.command_list->SetComputeRootDescriptorTable(AveragerRootSignatureParams::OutputTexture, averager_texture_descriptor);
          device.command_list->SetComputeRootDescriptorTable(AveragerRootSignatureParams::OutputBuffer, averager_buffer_descriptor);
          device.command_list->SetComputeRootDescriptorTable(AveragerRootSignatureParams::OutputNormals, averager_normals_descriptor);
          device.command_list->SetComputeRootDescriptorTable(AveragerRootSignatureParams::OutputAlbedo, averager_albedo_descriptor);
          device.command_list->SetComputeRootConstantBufferView(AveragerRootSignatureParams::Constants, averager_constants_buffer->GetBuffer()->GetGPUVirtualAddress() + device.back_buffer_index * sizeof(AlignedAveragerConstantBuffer));
          device.command_list->Dispatch(1280, 720, 1);
        }
      }

      // Copy averaged result from previous pass into the backbuffer
      {
        D3D12_RESOURCE_BARRIER pre_copy_barriers[2];
        pre_copy_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(device.back_buffers[device.back_buffer_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
        pre_copy_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(averager_texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        device.command_list->ResourceBarrier(ARRAYSIZE(pre_copy_barriers), pre_copy_barriers);

        device.command_list->CopyResource(device.back_buffers[device.back_buffer_index], averager_texture);

        D3D12_RESOURCE_BARRIER post_copy_barriers[2];
        post_copy_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(device.back_buffers[device.back_buffer_index], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
        post_copy_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(averager_texture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        device.command_list->ResourceBarrier(ARRAYSIZE(post_copy_barriers), post_copy_barriers);
      }
    }
    else
    {
      device.command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(device.back_buffers[device.back_buffer_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST));

      device.command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(averager_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));
      device.command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(averager_normals, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));
      device.command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(averager_albedo, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));
      device.command_list->CopyResource(averager_buffer_readback->GetBuffer(), averager_buffer);
      device.command_list->CopyResource(averager_normals_readback->GetBuffer(), averager_normals);
      device.command_list->CopyResource(averager_albedo_readback->GetBuffer(), averager_albedo);
      device.command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(averager_buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
      device.command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(averager_normals, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
      device.command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(averager_albedo, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

      device.ExecuteCommandLists();
      device.WaitForGPU();
      device.PrepareCommandLists();

      float* noisy_pixels = reinterpret_cast<float*>(averager_buffer_readback->Map());
      float* noisy_normals = reinterpret_cast<float*>(averager_normals_readback->Map());
      float* noisy_albedo = reinterpret_cast<float*>(averager_albedo_readback->Map());
      unsigned char* denoised_pixels = new unsigned char[1280 * 720 * 4];

      try
      {
        float* input = static_cast<float*>(optix_input->map());
        memcpy(input, noisy_pixels, sizeof(float) * 1280 * 720 * 4);
        optix_input->unmap();

        float* normals = static_cast<float*>(optix_normals->map());
        memcpy(normals, noisy_normals, sizeof(float) * 1280 * 720 * 4);
        optix_normals->unmap();

        float* albedo = static_cast<float*>(optix_albedo->map());
        memcpy(albedo, noisy_albedo, sizeof(float) * 1280 * 720 * 4);
        optix_albedo->unmap();

        optix_list->execute();

        float* denoised_output = static_cast<float*>(optix_output->map());

        for (int i = 0; i < 1280 * 720 * 4; i++)
        {
          denoised_pixels[i] = static_cast<unsigned char>(denoised_output[i] * 255);
        }

        optix_output->unmap();
      }
      catch (optix::Exception e)
      {
        std::cout << e.getErrorString() << std::endl;
      }

      averager_buffer_readback->Unmap();
      averager_normals_readback->Unmap();
      averager_albedo_readback->Unmap();

      TextureLoader::UploadTexture(device.device, device.command_queue, denoised_pixels, 1280, 720, &device.back_buffers[device.back_buffer_index]);
      device.command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(device.back_buffers[device.back_buffer_index], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
    }

    // Render imgui
    imgui_layer.Render(device.back_buffer_rtvs[device.back_buffer_index]);

    // Transition backbuffer to PRESENT
    device.command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(device.back_buffers[device.back_buffer_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    device.ExecuteCommandLists();
    device.Present();
    device.WaitForGPU();
  }

  device.WaitForGPU();

  RELEASE(global_root_signature);
  RELEASE(pso);

  RELEASE(target_render);
  RELEASE(averager_pso);
  RELEASE(averager_root_signature);
  RELEASE(averager_texture);
  DELETE(averager_constants_buffer);

  DELETE(meshes_buffer);
  DELETE(all_vertices_buffer);
  DELETE(all_indices_buffer);
  DELETE(shader_table_ray_generation);
  DELETE(shader_table_hit);
  DELETE(shader_table_miss);
  DELETE(scene_constants_buffer);
  DELETE(lights_buffer);
  DELETE(picking_buffer);
  DELETE(picking_buffer_readback);

  for (size_t i = 0; i < vertex_buffers.size(); i++)
  {
    DELETE(vertex_buffers[i]);
    DELETE(index_buffers[i]);
  }

  for (size_t i = 0; i < textures.size(); i++)
  {
    RELEASE(textures[i]);
  }

  DELETE(materials_buffer);

  imgui_layer.Shutdown();

  device.Shutdown();

  glfwTerminate();

  return 0;
}