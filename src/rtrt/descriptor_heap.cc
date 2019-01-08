#include "descriptor_heap.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  DescriptorHeap::DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type, UINT max_allocated_descriptors) :
    num_allocated_descriptors_(0),
    max_num_allocated_descriptors_(max_allocated_descriptors),
    descriptor_type_(descriptor_heap_type),
    descriptor_heap_(nullptr),
    descriptor_size_(0)
  {
    Initialize(device, descriptor_heap_type, max_allocated_descriptors);
  }

  //------------------------------------------------------------------------------------------------------
  DescriptorHeap::~DescriptorHeap()
  {
    Shutdown();
  }

  //------------------------------------------------------------------------------------------------------
  void DescriptorHeap::Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type, UINT max_allocated_descriptors)
  {
    if (descriptor_heap_ != nullptr)
    {
      Shutdown();
    }

    ThrowIfFalse(device != nullptr);
    ThrowIfFalse(max_allocated_descriptors > 0);

    D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc;
    descriptor_heap_desc.Flags = descriptor_heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    descriptor_heap_desc.NumDescriptors = max_allocated_descriptors;
    descriptor_heap_desc.Type = descriptor_heap_type;
    descriptor_heap_desc.NodeMask = 0;

    device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap_));
    descriptor_type_ = descriptor_heap_type;
    descriptor_size_ = device->GetDescriptorHandleIncrementSize(descriptor_heap_type);
    num_allocated_descriptors_ = 0;
    max_num_allocated_descriptors_ = max_allocated_descriptors;
  }

  //------------------------------------------------------------------------------------------------------
  void DescriptorHeap::Shutdown()
  {
    if (descriptor_heap_ != nullptr)
    {
      descriptor_heap_->Release();

      num_allocated_descriptors_ = 0;
      max_num_allocated_descriptors_ = 0;
      descriptor_size_ = 0;
    }
  }

  //------------------------------------------------------------------------------------------------------
  void DescriptorHeap::CreateDescriptor(ID3D12Device* device, D3D12_CONSTANT_BUFFER_VIEW_DESC* cbv_desc, DescriptorHandle* out_handle)
  {
    ThrowIfFalse(descriptor_type_ == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ThrowIfFalse(descriptor_heap_ != nullptr);
    ThrowIfFalse(device != nullptr);
    ThrowIfFalse(out_handle != nullptr);

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(num_allocated_descriptors_, descriptor_size_);

    device->CreateConstantBufferView(cbv_desc, handle);

    out_handle->parent_heap = this;
    out_handle->descriptor_index = num_allocated_descriptors_;

    num_allocated_descriptors_ = num_allocated_descriptors_ + 1;
  }

  //------------------------------------------------------------------------------------------------------
  void DescriptorHeap::CreateDescriptor(ID3D12Device* device, ID3D12Resource* resource, D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc, DescriptorHandle* out_handle)
  {
    ThrowIfFalse(descriptor_type_ == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ThrowIfFalse(descriptor_heap_ != nullptr);
    ThrowIfFalse(device != nullptr);
    ThrowIfFalse(out_handle != nullptr);

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(num_allocated_descriptors_, descriptor_size_);

    device->CreateShaderResourceView(resource, srv_desc, handle);

    out_handle->parent_heap = this;
    out_handle->descriptor_index = num_allocated_descriptors_;

    num_allocated_descriptors_ = num_allocated_descriptors_ + 1;
  }

  //------------------------------------------------------------------------------------------------------
  void DescriptorHeap::CreateDescriptor(ID3D12Device* device, ID3D12Resource* resource, ID3D12Resource* counter_resource, D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_desc, DescriptorHandle* out_handle)
  {
    ThrowIfFalse(descriptor_type_ == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ThrowIfFalse(descriptor_heap_ != nullptr);
    ThrowIfFalse(device != nullptr);
    ThrowIfFalse(out_handle != nullptr);

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(num_allocated_descriptors_, descriptor_size_);

    device->CreateUnorderedAccessView(resource, counter_resource, uav_desc, handle);

    out_handle->parent_heap = this;
    out_handle->descriptor_index = num_allocated_descriptors_;

    num_allocated_descriptors_ = num_allocated_descriptors_ + 1;
  }

  //------------------------------------------------------------------------------------------------------
  void DescriptorHeap::CreateDescriptor(ID3D12Device* device, ID3D12Resource* resource, D3D12_RENDER_TARGET_VIEW_DESC* rtv_desc, DescriptorHandle* out_handle)
  {
    ThrowIfFalse(descriptor_type_ == D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    ThrowIfFalse(descriptor_heap_ != nullptr);
    ThrowIfFalse(device != nullptr);
    ThrowIfFalse(out_handle != nullptr);

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(num_allocated_descriptors_, descriptor_size_);

    device->CreateRenderTargetView(resource, rtv_desc, handle);

    out_handle->parent_heap = this;
    out_handle->descriptor_index = num_allocated_descriptors_;

    num_allocated_descriptors_ = num_allocated_descriptors_ + 1;
  }

  //------------------------------------------------------------------------------------------------------
  void DescriptorHeap::CreateDescriptor(ID3D12Device* device, ID3D12Resource* resource, D3D12_DEPTH_STENCIL_VIEW_DESC* dsv_desc, DescriptorHandle* out_handle)
  {
    ThrowIfFalse(descriptor_type_ == D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    ThrowIfFalse(descriptor_heap_ != nullptr);
    ThrowIfFalse(device != nullptr);
    ThrowIfFalse(out_handle != nullptr);

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(num_allocated_descriptors_, descriptor_size_);

    device->CreateDepthStencilView(resource, dsv_desc, handle);

    out_handle->parent_heap = this;
    out_handle->descriptor_index = num_allocated_descriptors_;

    num_allocated_descriptors_ = num_allocated_descriptors_ + 1;
  }

  //------------------------------------------------------------------------------------------------------
  void DescriptorHeap::CreateDescriptor(ID3D12Device* device, D3D12_SAMPLER_DESC* sampler_desc, DescriptorHandle* out_handle)
  {
    ThrowIfFalse(descriptor_type_ == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    ThrowIfFalse(descriptor_heap_ != nullptr);
    ThrowIfFalse(device != nullptr);
    ThrowIfFalse(out_handle != nullptr);

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(descriptor_heap_->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(num_allocated_descriptors_, descriptor_size_);

    device->CreateSampler(sampler_desc, handle);

    out_handle->parent_heap = this;
    out_handle->descriptor_index = num_allocated_descriptors_;

    num_allocated_descriptors_ = num_allocated_descriptors_ + 1;
  }
}