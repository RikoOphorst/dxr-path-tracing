#pragma once

namespace rtrt
{
  class ReadbackBuffer
  {
  public:
    ReadbackBuffer();
    ~ReadbackBuffer();

    void* Create(ID3D12Device* device, UINT buffer_size);
    void Destroy();

    ID3D12Resource* GetBuffer();
    void* GetData();
    bool IsMapped();
    UINT GetBufferSize();

    void* Map();
    void Unmap();
  private:
    ID3D12Resource* buffer_;
    void* data_;
    UINT buffer_size_;
  };
}