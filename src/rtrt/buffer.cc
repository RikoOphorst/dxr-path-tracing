#include "buffer.h"

#include "device.h"
#include "upload_buffer.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  Buffer::Buffer() :
    buffer_(nullptr),
    size_(0)
  {

  }

  //------------------------------------------------------------------------------------------------------
  Buffer::~Buffer()
  {
    RELEASE_SAFE(buffer_);
  }
  
  //------------------------------------------------------------------------------------------------------
  void Buffer::Create(Device* device, D3D12_RESOURCE_STATES initial_state, UINT size)
  {
    device->device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(size), initial_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE),
      initial_state,
      nullptr,
      IID_PPV_ARGS(&buffer_)
    );
  }

  //------------------------------------------------------------------------------------------------------
  void Buffer::Create(Device* device, D3D12_RESOURCE_STATES initial_state, UINT buffer_size, void* initial_data)
  {
    Create(device, initial_state, buffer_size, initial_data, buffer_size);
  }
  
  //------------------------------------------------------------------------------------------------------
  void Buffer::Create(Device* device, D3D12_RESOURCE_STATES initial_state, UINT size, void* initial_data, UINT initial_data_size)
  {
    device->device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(size), initial_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE),
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&buffer_)
    );

    UploadBuffer* upload = new UploadBuffer();
    upload->Create(device->device, initial_data_size, initial_data);

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer_, D3D12_RESOURCE_STATE_COPY_DEST, initial_state);

    device->PrepareCommandLists();
    device->command_list->CopyResource(buffer_, upload->GetBuffer());
    device->command_list->ResourceBarrier(1, &barrier);
    device->ExecuteCommandLists();
    device->WaitForGPU();

    DELETE_SAFE(upload);
  }
  
  //------------------------------------------------------------------------------------------------------
  ID3D12Resource* Buffer::GetBuffer()
  {
    return buffer_;
  }
}