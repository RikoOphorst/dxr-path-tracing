#pragma once

namespace rtrt
{
  class DescriptorHeap;
  struct DescriptorHandle;

  class DescriptorHeap
  {
    friend struct DescriptorHandle;
  public:
    DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type, UINT max_allocated_descriptors);
    ~DescriptorHeap();

    void Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type, UINT max_allocated_descriptors);
    void Shutdown();

    void CreateDescriptor(ID3D12Device* device, D3D12_CONSTANT_BUFFER_VIEW_DESC* cbv_desc, DescriptorHandle* out_handle);
    void CreateDescriptor(ID3D12Device* device, ID3D12Resource* resource, D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc, DescriptorHandle* out_handle);
    void CreateDescriptor(ID3D12Device* device, ID3D12Resource* resource, ID3D12Resource* counter_resource, D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc, DescriptorHandle* out_handle);
    void CreateDescriptor(ID3D12Device* device, ID3D12Resource* resource, D3D12_RENDER_TARGET_VIEW_DESC* rtv_desc, DescriptorHandle* out_handle);
    void CreateDescriptor(ID3D12Device* device, ID3D12Resource* resource, D3D12_DEPTH_STENCIL_VIEW_DESC* dsv_desc, DescriptorHandle* out_handle);
    void CreateDescriptor(ID3D12Device* device, D3D12_SAMPLER_DESC* sampler_desc, DescriptorHandle* out_handle);

    inline ID3D12DescriptorHeap* GetDescriptorHeap() { return descriptor_heap_; }

  private:
    ID3D12DescriptorHeap* descriptor_heap_;
    D3D12_DESCRIPTOR_HEAP_TYPE descriptor_type_;
    UINT descriptor_size_;
    UINT num_allocated_descriptors_;
    UINT max_num_allocated_descriptors_;
  };

  struct DescriptorHandle
  {
    DescriptorHeap* parent_heap;
    UINT descriptor_index;

    inline D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle()
    {
      CD3DX12_CPU_DESCRIPTOR_HANDLE handle(parent_heap->descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
      handle.Offset(descriptor_index, parent_heap->descriptor_size_);
      return handle;
    }

    inline D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle()
    {
      CD3DX12_GPU_DESCRIPTOR_HANDLE handle(parent_heap->descriptor_heap_->GetGPUDescriptorHandleForHeapStart());
      handle.Offset(descriptor_index, parent_heap->descriptor_size_);
      return handle;
    }

    inline operator D3D12_CPU_DESCRIPTOR_HANDLE() { return cpu_handle(); }
    inline operator D3D12_GPU_DESCRIPTOR_HANDLE() { return gpu_handle(); }
  };
}