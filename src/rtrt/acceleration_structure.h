#pragma once

namespace rtrt
{
  class Model;
  class Buffer;
  class Device;
  class DescriptorHeap;

  struct AccelerationStructure
  {
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE type;
    ID3D12Resource* scratch = nullptr;
    std::vector<ID3D12Resource*> structures;
    std::vector<WRAPPED_GPU_POINTER> structure_pointers;
    Buffer* instance_descs_buffer = nullptr;

    AccelerationStructure();
    ~AccelerationStructure();
  };

  class AccelerationStructureUtility
  {
  public:
    static void BuildMultipleBLASesFromModel(
      Device* device,
      DescriptorHeap* descriptor_heap,
      const Model& model,
      std::vector<Buffer*> model_vertex_buffers,
      std::vector<Buffer*> model_index_buffers,
      AccelerationStructure* out_blases
    );

    static void BuildSingleTLASFromModel(
      Device* device,
      DescriptorHeap* descriptor_heap,
      const Model& model,
      const AccelerationStructure& model_blases,
      AccelerationStructure* out_tlas
    );
  };
}