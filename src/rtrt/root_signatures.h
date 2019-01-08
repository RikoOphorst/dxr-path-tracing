#pragma once

namespace rtrt
{
  class RootSignatureFactory
  {
  public:
    static ID3D12RootSignature* BuildRootSignature(ID3D12RaytracingFallbackDevice* fallback_device, D3D12_ROOT_SIGNATURE_DESC* root_signature_desc);
    static ID3D12RootSignature* BuildRootSignature(ID3D12Device* device, D3D12_ROOT_SIGNATURE_DESC* root_signature_desc);
  };
}