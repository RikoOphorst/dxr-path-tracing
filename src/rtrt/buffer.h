#pragma once

namespace rtrt
{
  class Device;

  class Buffer
  {
  public:
    Buffer();
    ~Buffer();

    void Create(Device* device, D3D12_RESOURCE_STATES initial_state, UINT buffer_size);
    void Create(Device* device, D3D12_RESOURCE_STATES initial_state, UINT buffer_size, void* initial_data);
    void Create(Device* device, D3D12_RESOURCE_STATES initial_state, UINT buffer_size, void* initial_data, UINT initial_data_size);

    ID3D12Resource* GetBuffer();

  private:
    ID3D12Resource* buffer_;
    UINT size_;
  };
}