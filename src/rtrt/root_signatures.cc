#include "root_signatures.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  ID3D12RootSignature* RootSignatureFactory::BuildRootSignature(ID3D12RaytracingFallbackDevice* fallback_device, D3D12_ROOT_SIGNATURE_DESC* root_signature_desc)
  {
    ID3DBlob* blob = nullptr;
    ID3DBlob* error = nullptr;

    HRESULT hr = S_OK;

    hr = fallback_device->D3D12SerializeRootSignature(root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);

    if (FAILED(hr))
    {
      std::stringstream stream;

      stream << "Failed serialization of root signature. Error:" << std::endl;
      std::string err = std::string(reinterpret_cast<char*>(error->GetBufferPointer()), error->GetBufferSize());
      stream << err << std::endl;

      BREAK(stream.str().c_str());
    }

    ID3D12RootSignature* root_signature = nullptr;
    hr = fallback_device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));
    if (FAILED(hr))
    {
      std::stringstream stream;

      stream << "Failed creation of root signature. Error:" << std::endl;
      std::string err = std::string(reinterpret_cast<char*>(error->GetBufferPointer()), error->GetBufferSize());
      stream << err << std::endl;

      BREAK(stream.str().c_str());
    }

    RELEASE(blob);
    RELEASE(error);

    return root_signature;
  }
  
  //------------------------------------------------------------------------------------------------------
  ID3D12RootSignature* RootSignatureFactory::BuildRootSignature(ID3D12Device* device, D3D12_ROOT_SIGNATURE_DESC* root_signature_desc)
  {
    ID3DBlob* blob = nullptr;
    ID3DBlob* error = nullptr;

    HRESULT hr = S_OK;

    hr = D3D12SerializeRootSignature(root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);

    if (FAILED(hr))
    {
      std::stringstream stream;

      stream << "Failed serialization of root signature. Error:" << std::endl;
      std::string err = std::string(reinterpret_cast<char*>(error->GetBufferPointer()), error->GetBufferSize());
      stream << err << std::endl;

      BREAK(stream.str().c_str());
    }

    ID3D12RootSignature* root_signature = nullptr;
    hr = device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));
    if (FAILED(hr))
    {
      std::stringstream stream;

      stream << "Failed creation of root signature. Error:" << std::endl;
      std::string err = std::string(reinterpret_cast<char*>(error->GetBufferPointer()), error->GetBufferSize());
      stream << err << std::endl;

      BREAK(stream.str().c_str());
    }

    RELEASE(blob);
    RELEASE(error);

    return root_signature;
  }
}