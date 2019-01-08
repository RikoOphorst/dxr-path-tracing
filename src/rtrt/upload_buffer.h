#pragma once

namespace rtrt
{
  class UploadBuffer
  {
  public:
    UploadBuffer();
    ~UploadBuffer();

    void* Create(ID3D12Device* device, UINT buffer_size, void* initial_data);
    void Destroy();

    ID3D12Resource* GetBuffer();
    void* GetData();
    bool IsMapped();
    UINT GetBufferSize();

    void Write(UINT write_size_in_bytes, void* data, UINT offset_from_start_in_bytes = 0);
    
    void* Map();
    void Unmap();
  private:
    ID3D12Resource* buffer_;
    void* data_;
    UINT buffer_size_;
  };
}