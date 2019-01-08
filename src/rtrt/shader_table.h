#pragma once

#include "upload_buffer.h"

namespace rtrt
{
  class ShaderRecord
  {
    friend class ShaderTable;
  public:
    ShaderRecord(void* shader_identifier, UINT shader_identifier_size, void* local_root_arguments, UINT local_root_arguments_size);

  protected:
    void* shader_identifier_;
    UINT shader_identifier_size_;
    void* local_root_arguments_;
    UINT local_root_arguments_size_;
  };

  class ShaderTable : public UploadBuffer
  {
  public:
    ShaderTable(ID3D12Device* device, UINT total_num_shader_records, UINT shader_record_size);

    void Add(const ShaderRecord& shader_record);
    UINT64 GetSizeInBytes();
    UINT64 GetStrideInBytes();
  private:
    uintptr_t record_address_;
    UINT record_size_;
    std::vector<ShaderRecord> records_;
  };
}