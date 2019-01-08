#include "shader_table.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  ShaderRecord::ShaderRecord(void* shader_identifier, UINT shader_identifier_size, void* local_root_arguments, UINT local_root_arguments_size) :
    shader_identifier_(shader_identifier),
    shader_identifier_size_(shader_identifier_size),
    local_root_arguments_(local_root_arguments),
    local_root_arguments_size_(local_root_arguments_size)
  {

  }
  
  //------------------------------------------------------------------------------------------------------
  ShaderTable::ShaderTable(ID3D12Device* device, UINT total_num_shader_records, UINT shader_record_size)
  {
    std::function<UINT(UINT, UINT)> Align = [&](UINT size, UINT alignment)
    {
      return (size + (alignment - 1)) & ~(alignment - 1);
    };

    records_.reserve(total_num_shader_records);
    record_size_ = Align(shader_record_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    record_address_ = reinterpret_cast<uintptr_t>(Create(device, total_num_shader_records * record_size_, nullptr));
  }
  
  //------------------------------------------------------------------------------------------------------
  void ShaderTable::Add(const ShaderRecord& shader_record)
  {
    ThrowIfFalse(records_.size() < records_.capacity());
    Write(shader_record.shader_identifier_size_, shader_record.shader_identifier_, static_cast<UINT>(records_.size()) * record_size_);

    if (shader_record.local_root_arguments_size_ > 0)
    {
      Write(shader_record.local_root_arguments_size_, shader_record.local_root_arguments_, static_cast<UINT>(records_.size()) * record_size_ + shader_record.shader_identifier_size_);
    }

    records_.push_back(shader_record);
  }

  //------------------------------------------------------------------------------------------------------
  UINT64 ShaderTable::GetSizeInBytes()
  {
    return static_cast<UINT64>(records_.size() * record_size_);
  }
  
  //------------------------------------------------------------------------------------------------------
  UINT64 ShaderTable::GetStrideInBytes()
  {
    return static_cast<UINT64>(record_size_);
  }
}