#include "acceleration_structure.h"

#include "model.h"
#include "buffer.h"
#include "device.h"
#include "descriptor_heap.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  AccelerationStructure::AccelerationStructure() :
    scratch(nullptr),
    instance_descs_buffer(nullptr)
  {

  }

  //------------------------------------------------------------------------------------------------------
  AccelerationStructure::~AccelerationStructure()
  {
    for (size_t i = 0; i < structures.size(); i++)
    {
      RELEASE(structures[i]);
    }

    if (instance_descs_buffer != nullptr)
    {
      delete instance_descs_buffer;
    }

    RELEASE(scratch);
  }

  //------------------------------------------------------------------------------------------------------
  void AccelerationStructureUtility::BuildMultipleBLASesFromModel(
    Device* device,
    DescriptorHeap* descriptor_heap,
    const Model& model, 
    std::vector<Buffer*> model_vertex_buffers, 
    std::vector<Buffer*> model_index_buffers, 
    AccelerationStructure* out_blases
  )
  {
    ThrowIfFalse(out_blases != nullptr);

    AccelerationStructure& blases = *out_blases;
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs;
    std::vector<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC> build_descs;
    std::vector<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO> prebuild_infos;

    blases.structures.resize(model.meshes.size());
    blases.structure_pointers.resize(model.meshes.size());
    geometry_descs.resize(model.meshes.size());
    build_descs.resize(model.meshes.size());
    prebuild_infos.resize(model.meshes.size());

    UINT64 max_size_scratch = 0;

    for (size_t i = 0; i < model.meshes.size(); i++)
    {
      geometry_descs[i] = {};
      geometry_descs[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
      geometry_descs[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

      geometry_descs[i].Triangles.VertexBuffer.StartAddress = model_vertex_buffers[i]->GetBuffer()->GetGPUVirtualAddress();
      geometry_descs[i].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
      geometry_descs[i].Triangles.VertexCount = static_cast<UINT>(model.meshes[i].vertices.size());
      geometry_descs[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

      geometry_descs[i].Triangles.IndexBuffer = model_index_buffers[i]->GetBuffer()->GetGPUVirtualAddress();
      geometry_descs[i].Triangles.IndexCount = static_cast<UINT>(model.meshes[i].indices.size());
      geometry_descs[i].Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

      build_descs[i] = {};
      build_descs[i].Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
      build_descs[i].Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
      build_descs[i].Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
      build_descs[i].Inputs.NumDescs = 1;
      build_descs[i].Inputs.pGeometryDescs = &geometry_descs[i];

      device->fallback_device->GetRaytracingAccelerationStructurePrebuildInfo(&build_descs[i].Inputs, &prebuild_infos[i]);

      max_size_scratch = std::max(max_size_scratch, prebuild_infos[i].ScratchDataSizeInBytes);
    }

    CD3DX12_HEAP_PROPERTIES scratch_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC scratch_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(max_size_scratch, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ThrowIfFailed(device->device->CreateCommittedResource(&scratch_heap_props, D3D12_HEAP_FLAG_NONE, &scratch_buffer_desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&blases.scratch)));

    for (size_t i = 0; i < model.meshes.size(); i++)
    {
      CD3DX12_HEAP_PROPERTIES blas_heap_props(D3D12_HEAP_TYPE_DEFAULT);
      D3D12_RESOURCE_DESC blas_desc = CD3DX12_RESOURCE_DESC::Buffer(prebuild_infos[i].ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
      ThrowIfFailed(device->device->CreateCommittedResource(&blas_heap_props, D3D12_HEAP_FLAG_NONE, &blas_desc, device->fallback_device->GetAccelerationStructureResourceState(), nullptr, IID_PPV_ARGS(&blases.structures[i])));

      build_descs[i].ScratchAccelerationStructureData = blases.scratch->GetGPUVirtualAddress();
      build_descs[i].DestAccelerationStructureData = blases.structures[i]->GetGPUVirtualAddress();
      
      blases.structure_pointers[i] = device->CreateFallbackWrappedPointer(descriptor_heap, blases.structures[i], static_cast<UINT>(prebuild_infos[i].ResultDataMaxSizeInBytes) / sizeof(UINT32));

      device->PrepareCommandLists();
      ID3D12DescriptorHeap* heaps[1] = { descriptor_heap->GetDescriptorHeap() };
      device->fallback_command_list->SetDescriptorHeaps(1, heaps);
      device->fallback_command_list->BuildRaytracingAccelerationStructure(&build_descs[i], 0, nullptr);
      device->ExecuteCommandLists();
      device->WaitForGPU();
    }
  }
  
  //------------------------------------------------------------------------------------------------------
  void AccelerationStructureUtility::BuildSingleTLASFromModel(
    Device* device,
    DescriptorHeap* descriptor_heap,
    const Model& model, 
    const AccelerationStructure& model_blases, 
    AccelerationStructure* out_tlas
  )
  {
    ThrowIfFalse(out_tlas != nullptr);

    AccelerationStructure& tlas = *out_tlas;
    std::vector<D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC> instance_descs;

    tlas.structures.resize(1);
    tlas.structure_pointers.resize(1);

    std::function<DirectX::XMMATRIX(const Model::Node*)> CalculateTransformForNode = [&](const Model::Node* node)->DirectX::XMMATRIX
    {
      if (node->parent != nullptr)
      {
        return DirectX::XMMatrixMultiply(node->transform, CalculateTransformForNode(node->parent));
      }
      else
      {
        return DirectX::XMMatrixIdentity();
      }
    };

    std::function<void(const Model::Node*)> ProcessModelNode = [&](const Model::Node* node)
    {
      for (size_t i = 0; i < node->meshes.size(); i++)
      {
        D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC instance_desc = {};
        DirectX::XMStoreFloat3x4(reinterpret_cast<DirectX::XMFLOAT3X4*>(&instance_desc.Transform), CalculateTransformForNode(node));
        instance_desc.InstanceMask = 1;
        instance_desc.InstanceID = node->meshes[i];
        instance_desc.AccelerationStructure = model_blases.structure_pointers[node->meshes[i]];
        instance_descs.push_back(instance_desc);
      }

      for (size_t i = 0; i < node->children.size(); i++)
      {
        ProcessModelNode(node->children[i]);
      }
    };

    ProcessModelNode(model.root_node);

    tlas.instance_descs_buffer = new Buffer();
    tlas.instance_descs_buffer->Create(device, D3D12_RESOURCE_STATE_GENERIC_READ, static_cast<UINT>(sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC) * instance_descs.size()), instance_descs.data());

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc = {};
    build_desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    build_desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    build_desc.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    build_desc.Inputs.NumDescs = static_cast<UINT>(instance_descs.size());
    build_desc.Inputs.pGeometryDescs = nullptr;
    build_desc.Inputs.InstanceDescs = tlas.instance_descs_buffer->GetBuffer()->GetGPUVirtualAddress();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info;
    device->fallback_device->GetRaytracingAccelerationStructurePrebuildInfo(&build_desc.Inputs, &prebuild_info);

    CD3DX12_HEAP_PROPERTIES scratch_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC scratch_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(prebuild_info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    device->device->CreateCommittedResource(&scratch_heap_props, D3D12_HEAP_FLAG_NONE, &scratch_buffer_desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&tlas.scratch));

    CD3DX12_HEAP_PROPERTIES tlas_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC tlas_desc = CD3DX12_RESOURCE_DESC::Buffer(prebuild_info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    device->device->CreateCommittedResource(&tlas_heap_props, D3D12_HEAP_FLAG_NONE, &tlas_desc, device->fallback_device->GetAccelerationStructureResourceState(), nullptr, IID_PPV_ARGS(&tlas.structures[0]));

    build_desc.ScratchAccelerationStructureData = tlas.scratch->GetGPUVirtualAddress();
    build_desc.DestAccelerationStructureData = tlas.structures[0]->GetGPUVirtualAddress();

    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.resize(model_blases.structures.size());

    for (size_t i = 0; i < model_blases.structures.size(); i++)
    {
      barriers[i] = CD3DX12_RESOURCE_BARRIER::UAV(model_blases.structures[i]);
    }

    device->PrepareCommandLists();
    ID3D12DescriptorHeap* heaps[1] = { descriptor_heap->GetDescriptorHeap() };
    device->fallback_command_list->SetDescriptorHeaps(1, heaps);
    device->command_list->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    device->fallback_command_list->BuildRaytracingAccelerationStructure(&build_desc, 0, nullptr);
    device->ExecuteCommandLists();
    device->WaitForGPU();

    tlas.structure_pointers[0] = device->CreateFallbackWrappedPointer(descriptor_heap, tlas.structures[0], static_cast<UINT>(prebuild_info.ResultDataMaxSizeInBytes) / sizeof(UINT32));
  }
}