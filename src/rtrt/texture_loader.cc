#include "texture_loader.h"

#include <DDSTextureLoader.h>
#include <ResourceUploadBatch.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace filesystem = std::experimental::filesystem;

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  void TextureLoader::LoadTexture(ID3D12Device* device, ID3D12CommandQueue* queue, const std::string& texture_path, ID3D12Resource** out_texture)
  {
    filesystem::path texture_path_fs = texture_path;
    std::wstring extension = texture_path_fs.extension().c_str();

    if (extension == L".dds" || extension == L".DDS")
    {
      LoadUsingDDS(device, queue, texture_path, out_texture);
    }
    else
    {
      LoadUsingSTB(device, queue, texture_path, out_texture);
    }
  }
  //------------------------------------------------------------------------------------------------------
  void TextureLoader::UploadTexture(ID3D12Device* device, ID3D12CommandQueue* queue, unsigned char* pixels, UINT width, UINT height, ID3D12Resource** out_texture)
  {
    D3D12_SUBRESOURCE_DATA subresource_data;
    subresource_data.pData = pixels;
    subresource_data.RowPitch = width * 4;
    subresource_data.SlicePitch = subresource_data.RowPitch * height;

    D3D12_RESOURCE_DESC texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<UINT64>(width), static_cast<UINT>(height));

    UINT64 texture_upload_buffer_size;
    device->GetCopyableFootprints(&texture_desc, 0, 1, 0, nullptr, nullptr, nullptr, &texture_upload_buffer_size);

    ID3D12Resource* upload_buffer = nullptr;

    ThrowIfFailed(
      device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(texture_upload_buffer_size),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&upload_buffer)
      )
    );

    ID3D12CommandAllocator* allocator = nullptr;
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));

    ID3D12GraphicsCommandList* list = nullptr;
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&list)));

    UpdateSubresources(list, *out_texture, upload_buffer, 0, 0, 1, &subresource_data);

    list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(*out_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

    ThrowIfFailed(list->Close());
    ID3D12CommandList* lists[] = { list };
    queue->ExecuteCommandLists(1, lists);

    ID3D12Fence* fence = nullptr;
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    Microsoft::WRL::Wrappers::Event fence_event;

    if (SUCCEEDED(queue->Signal(fence, 1)))
    {
      if (SUCCEEDED(fence->SetEventOnCompletion(1, fence_event.Get())))
      {
        WaitForSingleObjectEx(fence_event.Get(), INFINITE, FALSE);
      }
    }

    RELEASE(upload_buffer);
    RELEASE(list);
    RELEASE(allocator);
    RELEASE(fence);
  }
  
  //------------------------------------------------------------------------------------------------------
  void TextureLoader::LoadUsingDDS(ID3D12Device* device, ID3D12CommandQueue* queue, const std::string& texture_path, ID3D12Resource** out_texture)
  {
    DirectX::ResourceUploadBatch upload_batch(device);
    upload_batch.Begin();

    std::wstring wide_texture_path(texture_path.begin(), texture_path.end());

    // Just use the DDSTextureLoader from the DirectX Toolkit for D3D12
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile(device, upload_batch, wide_texture_path.c_str(), out_texture, true, 0, nullptr, nullptr));

    auto finisher = upload_batch.End(queue);

    finisher.wait();
  }
  
  //------------------------------------------------------------------------------------------------------
  void TextureLoader::LoadUsingSTB(ID3D12Device* device, ID3D12CommandQueue* queue, const std::string& texture_path, ID3D12Resource** out_texture)
  {
    int width, height, comp;
    unsigned char* pixel_data = stbi_load(texture_path.c_str(), &width, &height, &comp, 4);
    ThrowIfFalse(pixel_data != nullptr);

    D3D12_RESOURCE_DESC texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<UINT64>(width), static_cast<UINT>(height));

    ThrowIfFailed(
      device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texture_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(out_texture)
      )
    );

    UploadTexture(device, queue, pixel_data, width, height, out_texture);
    STBI_FREE(pixel_data);
  }
}