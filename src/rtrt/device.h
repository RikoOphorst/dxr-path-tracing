#pragma once

#include "descriptor_heap.h"

namespace rtrt
{
  class Device
  {
  public:
    static const UINT MAX_BACK_BUFFER_COUNT = 3;
    static const UINT NUM_BACK_BUFFERS = 3;
    static const UINT NUM_CBV_SRV_UAV_DESCRIPTORS = 4096;
    static const UINT NUM_RTV_DESCRIPTORS = 128;
    static const UINT NUM_DSV_DESCRIPTORS = 128;

    bool initialized;

    UINT width;
    UINT height;

    GLFWwindow* glfw_window;

    IDXGIAdapter1* adapter;
    IDXGIFactory4* factory;

    union 
    {
      DescriptorHeap* cbv_srv_uav_heap;
      DescriptorHeap* cbv_heap;
      DescriptorHeap* srv_heap;
      DescriptorHeap* uav_heap;
    };

    DescriptorHeap* rtv_heap;
    DescriptorHeap* dsv_heap;

    ID3D12Device* device;
    ID3D12CommandQueue* command_queue;
    ID3D12CommandAllocator* command_allocators[NUM_BACK_BUFFERS];
    ID3D12GraphicsCommandList* command_list;

    ID3D12RaytracingFallbackDevice* fallback_device;
    ID3D12RaytracingFallbackCommandList* fallback_command_list;

    IDXGISwapChain3* swap_chain;
    DXGI_FORMAT back_buffer_format;
    ID3D12Resource* back_buffers[MAX_BACK_BUFFER_COUNT];
    DescriptorHandle back_buffer_rtvs[MAX_BACK_BUFFER_COUNT];
    UINT back_buffer_index;

    ID3D12Fence* fence;
    UINT64 fence_values[MAX_BACK_BUFFER_COUNT];
    Microsoft::WRL::Wrappers::Event fence_event;

    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor_rect;

  public:
    Device();
    ~Device();

    void Initialize(GLFWwindow* glfw_window);
    void Shutdown();

    void PrepareCommandLists();
    void ExecuteCommandLists();
    void WaitForGPU();

    void Present();

    WRAPPED_GPU_POINTER CreateFallbackWrappedPointer(DescriptorHeap* uav_descriptor_heap, ID3D12Resource* resource, UINT buffer_num_elements);

  private:
    void EnableRaytracing();
    void CreateDeviceResources();
    void CreateSwapChainResources();
  };
}