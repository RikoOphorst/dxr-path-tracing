#pragma once

namespace rtrt
{
  class TextureLoader
  {
  public:
    static void LoadTexture(ID3D12Device* device, ID3D12CommandQueue* queue, const std::string& texture_path, ID3D12Resource** out_texture);
    static void UploadTexture(ID3D12Device* device, ID3D12CommandQueue* queue, unsigned char* pixels, UINT width, UINT height, ID3D12Resource** out_texture);
  private:
    static void LoadUsingDDS(ID3D12Device* device, ID3D12CommandQueue* queue, const std::string& texture_path, ID3D12Resource** out_texture);
    static void LoadUsingSTB(ID3D12Device* device, ID3D12CommandQueue* queue, const std::string& texture_path, ID3D12Resource** out_texture);
  };
}