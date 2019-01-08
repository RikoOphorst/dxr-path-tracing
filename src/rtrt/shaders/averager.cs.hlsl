#include "raytracing_data.h"

ConstantBuffer<AveragerConstantBuffer> constants : register(b0);

RWTexture2D<float4> input_texture   : register(u0);
RWBuffer<float4>    input_normals   : register(u1);
RWBuffer<float4>    input_albedo    : register(u2);
RWTexture2D<float4> output_texture  : register(u3);
RWBuffer<float4>    output_buffer   : register(u4);
RWBuffer<float4>    output_normals  : register(u5);
RWBuffer<float4>    output_albedo   : register(u6);

[numthreads(1, 1, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
  int idx = thread_id.y * 1280 + thread_id.x;
  float exp = 1.0f / constants.gamma;

  output_texture[thread_id.xy]  = float4(pow(abs(input_texture[thread_id.xy].xyz / input_texture[thread_id.xy].w), float3(exp, exp, exp)), 1.0f);
  output_buffer[idx]            = output_texture[thread_id.xy];
  output_normals[idx]           = float4(pow(abs(input_normals[idx].xyz / input_normals[idx].w), float3(exp, exp, exp)), 0.0f);
  output_albedo[idx]            = float4(pow(abs(input_albedo[idx].xyz / input_albedo[idx].w), float3(exp, exp, exp)), 0.0f);

  input_texture[thread_id.xy] *= constants.clear_samples;
  input_normals[idx]          *= constants.clear_samples;
  input_albedo[idx]           *= constants.clear_samples;
}