#include "readback_buffer.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  ReadbackBuffer::ReadbackBuffer() :
    data_(nullptr),
    buffer_(nullptr),
    buffer_size_(0)
  {

  }

  //------------------------------------------------------------------------------------------------------
  ReadbackBuffer::~ReadbackBuffer()
  {
    Destroy();
  }

  //------------------------------------------------------------------------------------------------------
  void* ReadbackBuffer::Create(ID3D12Device* device, UINT buffer_size)
  {
    Destroy();

    buffer_size_ = buffer_size;

    D3D12_HEAP_PROPERTIES heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    D3D12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

    ThrowIfFailed(
      device->CreateCommittedResource(
        &heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &buffer_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&buffer_)
      )
    );

    return nullptr;
  }

  //------------------------------------------------------------------------------------------------------
  void ReadbackBuffer::Destroy()
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
  ID3D12Resource* ReadbackBuffer::GetBuffer()
  {
    return buffer_;
  }

  //------------------------------------------------------------------------------------------------------
  void* ReadbackBuffer::GetData()
  {
    return data_;
  }

  //------------------------------------------------------------------------------------------------------
  bool ReadbackBuffer::IsMapped()
  {
    return data_ != nullptr;
  }

  //------------------------------------------------------------------------------------------------------
  UINT ReadbackBuffer::GetBufferSize()
  {
    return buffer_size_;
  }

  //------------------------------------------------------------------------------------------------------
  void* ReadbackBuffer::Map()
  {
    ThrowIfFalse(buffer_ != nullptr);
    ThrowIfFailed(buffer_->Map(0, nullptr, &data_));
    return data_;
  }

  //------------------------------------------------------------------------------------------------------
  void ReadbackBuffer::Unmap()
  {
    ThrowIfFalse(buffer_ != nullptr);
    buffer_->Unmap(0, nullptr);
    data_ = nullptr;
  }
}