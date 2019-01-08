#pragma once

#include <dxgi1_6.h>
#include <d3d12_1.h>
#include <D3D12RaytracingFallback.h>
#include <d3dx12.h>
#include <dxgidebug.h>
#include <D3D12RaytracingHelpers.hpp>
#include <unordered_map>
#include <sstream>

#include "util.h"

inline UINT Align(UINT size, UINT alignment)
{
  return (size + (alignment - 1)) & ~(alignment - 1);
}

class GpuUploadBuffer
{
public:
  ID3D12Resource* GetResource() { return m_resource; }

protected:
  ID3D12Resource* m_resource = nullptr;

  GpuUploadBuffer() : m_resource(nullptr) {}
  ~GpuUploadBuffer()
  {
    if (m_resource != nullptr)
    {
      m_resource->Unmap(0, nullptr);
    }
  }

  void Allocate(ID3D12Device* device, UINT bufferSize, LPCWSTR resourceName = nullptr)
  {
    auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    ThrowIfFailed(device->CreateCommittedResource(
      &uploadHeapProperties,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&m_resource)));
    m_resource->SetName(resourceName);
  }

  uint8_t* MapCpuWriteOnly()
  {
    uint8_t* mappedData;
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
    return mappedData;
  }
};

// Shader record = {{Shader ID}, {RootArguments}}
class ShaderRecord
{
public:
  ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize) :
    shaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
  {
  }

  ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize) :
    shaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
    localRootArguments(pLocalRootArguments, localRootArgumentsSize)
  {
  }

  void CopyTo(void* dest) const
  {
    uint8_t* byteDest = static_cast<uint8_t*>(dest);
    memcpy(byteDest, shaderIdentifier.ptr, shaderIdentifier.size);
    if (localRootArguments.ptr)
    {
      memcpy(byteDest + shaderIdentifier.size, localRootArguments.ptr, localRootArguments.size);
    }
  }

  struct PointerWithSize {
    void *ptr;
    UINT size;

    PointerWithSize() : ptr(nullptr), size(0) {}
    PointerWithSize(void* _ptr, UINT _size) : ptr(_ptr), size(_size) {};
  };
  PointerWithSize shaderIdentifier;
  PointerWithSize localRootArguments;
};

// Shader table = {{ ShaderRecord 1}, {ShaderRecord 2}, ...}
class ShaderTable : public GpuUploadBuffer
{
  uint8_t* m_mappedShaderRecords;
  UINT m_shaderRecordSize;

  // Debug support
  std::wstring m_name;
  std::vector<ShaderRecord> m_shaderRecords;

  ShaderTable() {}
public:
  ShaderTable(ID3D12Device* device, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName = nullptr)
    : m_name(resourceName)
  {
    m_shaderRecordSize = Align(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    m_shaderRecords.reserve(numShaderRecords);
    UINT bufferSize = numShaderRecords * m_shaderRecordSize;
    Allocate(device, bufferSize, resourceName);
    m_mappedShaderRecords = MapCpuWriteOnly();
  }

  void push_back(const ShaderRecord& shaderRecord)
  {
    ThrowIfFalse(m_shaderRecords.size() < m_shaderRecords.capacity());
    m_shaderRecords.push_back(shaderRecord);
    shaderRecord.CopyTo(m_mappedShaderRecords);
    m_mappedShaderRecords += m_shaderRecordSize;
  }

  UINT GetShaderRecordSize() { return m_shaderRecordSize; }

  // Pretty-print the shader records.
  void DebugPrint(std::unordered_map<void*, std::wstring> shaderIdToStringMap)
  {
    std::wstringstream wstr;
    wstr << L"|--------------------------------------------------------------------\n";
    wstr << L"|Shader table - " << m_name.c_str() << L": "
      << m_shaderRecordSize << L" | "
      << m_shaderRecords.size() * m_shaderRecordSize << L" bytes\n";

    for (UINT i = 0; i < m_shaderRecords.size(); i++)
    {
      wstr << L"| [" << i << L"]: ";
      wstr << shaderIdToStringMap[m_shaderRecords[i].shaderIdentifier.ptr] << L", ";
      wstr << m_shaderRecords[i].shaderIdentifier.size << L" + " << m_shaderRecords[i].localRootArguments.size << L" bytes \n";
    }
    wstr << L"|--------------------------------------------------------------------\n";
    wstr << L"\n";
    OutputDebugStringW(wstr.str().c_str());
  }
};