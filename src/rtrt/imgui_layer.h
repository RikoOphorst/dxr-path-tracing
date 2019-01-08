#pragma once

#include "imgui/imgui.h"
#include "descriptor_heap.h"

namespace rtrt
{
  class UploadBuffer;
  class Device;

  class ImGuiLayer
  {
  public:
    ImGuiLayer();
    ~ImGuiLayer();

    void Init(GLFWwindow* window, Device* device, DescriptorHeap* rtv_heap, DescriptorHeap* srv_heap);
    void NewFrame();
    void Render(DescriptorHandle render_target);
    void Shutdown();

    static void ImGuiRenderDrawLists(ImDrawData* draw_data);
    static void GlfwCharCallback(GLFWwindow* window, UINT unicode_character);
    static void GlfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

  protected:
    void CreateDeviceObjects();
    void InvalidateDeviceObjects();

  private:
    static std::queue<UINT> input_characters_;
    static float mouse_scroll_;

    Device* device_;
    GLFWwindow* window_;
    DescriptorHandle render_target_;
    DescriptorHeap* rtv_heap_;
    DescriptorHeap* srv_heap_;

    bool initialized_;
    INT64 ticks_per_second_;
    INT64 time_;

    ID3DBlob* vertex_shader_blob_;
    D3D12_SHADER_BYTECODE vertex_shader_;

    ID3DBlob* pixel_shader_blob_;
    D3D12_SHADER_BYTECODE pixel_shader_;

    ID3D12PipelineState* pipeline_state_;
    ID3D12RootSignature* root_signature_;
    ID3D12Resource* texture_buffer_;
    DescriptorHandle texture_handle_;

    UploadBuffer* upload_buffer_;
  };
}