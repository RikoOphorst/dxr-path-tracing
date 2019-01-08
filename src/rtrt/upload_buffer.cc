#include "upload_buffer.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  UploadBuffer::UploadBuffer() :
    data_(nullptr),
    buffer_(nullptr),
    buffer_size_(0)
  {

  }
  
  //------------------------------------------------------------------------------------------------------
  UploadBuffer::~UploadBuffer()
  {
    Destroy();
  }

  //------------------------------------------------------------------------------------------------------
  void* UploadBuffer::Create(ID3D12Device* device, UINT buffer_size, void* initial_data)
  {
    Destroy();

    buffer_size_ = buffer_size;
    
    D3D12_HEAP_PROPERTIES heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);
    
    ThrowIfFailed(
      device->CreateCommittedResource(
        &heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer_)
      )
    );

    void* mapped = Map();

    if (initial_data != nullptr)
    {
      Write(buffer_size, initial_data);
    }

    return mapped;
  }

  //------------------------------------------------------------------------------------------------------
  void UploadBuffer::Destroy()
  {
    if (buffer_ != nullptr)
    {
      if (data_ != nullptr)
      {
        Unmap();
      }

      buffer_->Release();
    }
  }

  //------------------------------------------------------------------------------------------------------
  ID3D12Resource* UploadBuffer::GetBuffer()
  {
    return buffer_;
  }

  //------------------------------------------------------------------------------------------------------
  void* UploadBuffer::GetData()
  {
    return data_;
  }

  //------------------------------------------------------------------------------------------------------
  bool UploadBuffer::IsMapped()
  {
    return data_ != nullptr;
  }

  //------------------------------------------------------------------------------------------------------
  UINT UploadBuffer::GetBufferSize()
  {
    return buffer_size_;
  }

  //------------------------------------------------------------------------------------------------------
  void UploadBuffer::Write(UINT write_size_in_bytes, void* data, UINT offset_from_start_in_bytes)
  {
    ThrowIfFalse(data != nullptr);
    ThrowIfFalse(write_size_in_bytes > 0);
    ThrowIfFalse(IsMapped());

    memcpy((void*)((uintptr_t)data_ + offset_from_start_in_bytes), data, write_size_in_bytes);
  }

  //------------------------------------------------------------------------------------------------------
  void* UploadBuffer::Map()
  {
    ThrowIfFalse(buffer_ != nullptr);
    ThrowIfFailed(buffer_->Map(0, nullptr, &data_));
    return data_;
  }

  //------------------------------------------------------------------------------------------------------
  void UploadBuffer::Unmap()
  {
    ThrowIfFalse(buffer_ != nullptr);
    buffer_->Unmap(0, nullptr);
    data_ = nullptr;
  }
}