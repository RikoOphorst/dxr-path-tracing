#include "device.h"

#include "adapter_selector.h"
#include "descriptor_heap.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  Device::Device() :
    initialized(false),
    width(1280),
    height(720),
    glfw_window(nullptr),
    adapter(nullptr),
    factory(nullptr),
    device(nullptr),
    command_queue(nullptr),
    command_list(nullptr),
    fallback_device(nullptr),
    fallback_command_list(nullptr),
    swap_chain(nullptr),
    back_buffer_format(DXGI_FORMAT_R8G8B8A8_UNORM),
    back_buffer_index(0),
    fence(nullptr),
    cbv_srv_uav_heap(nullptr),
    rtv_heap(nullptr),
    dsv_heap(nullptr)
  {
    for (int i = 0; i < NUM_BACK_BUFFERS; i++)
    {
      command_allocators[i] = nullptr;
      back_buffers[i] = nullptr;
      fence_values[i] = 0;
    }
  }

  //------------------------------------------------------------------------------------------------------
  Device::~Device()
  {
    Shutdown();
  }

  //------------------------------------------------------------------------------------------------------
  void Device::Initialize(GLFWwindow* a_glfw_window)
  {
    if (initialized == false)
    {
      glfw_window = a_glfw_window;
      int width_, height_;
      glfwGetWindowSize(glfw_window, &width_, &height_);
      width = static_cast<int>(width_);
      height = static_cast<int>(height_);

      AdapterSelector::SelectAdapter(
#ifdef _DEBUG
        AdapterSelector::FLAG_ENABLE_DEBUG_LAYER |
        AdapterSelector::FLAG_ENABLE_BREAK_ON_CORRUPTION |
        AdapterSelector::FLAG_ENABLE_BREAK_ON_ERROR |
        AdapterSelector::FLAG_ENABLE_BREAK_ON_WARNING |
#endif
        AdapterSelector::FLAG_FORCE_NVIDIA_ADAPTER |
        AdapterSelector::FLAG_PRINT_ALL_AVAILABLE_ADAPTERS |
        AdapterSelector::FLAG_PRINT_SELECTED_ADAPTER |
        AdapterSelector::FLAG_MINIMUM_FEATURE_LEVEL_12_1,
        &adapter,
        &factory
      );

      EnableRaytracing();
      CreateDeviceResources();
      CreateSwapChainResources();

      initialized = true;
    }
  }

  //------------------------------------------------------------------------------------------------------
  void Device::Shutdown()
  {
    if (initialized == true)
    {
      WaitForGPU();

      RELEASE(adapter);
      RELEASE(factory);
      RELEASE(device);
      RELEASE(command_queue);
      RELEASE(command_list);
      RELEASE(fallback_device);
      RELEASE(fallback_command_list);
      RELEASE(swap_chain);
      RELEASE(fence);
      DELETE(cbv_srv_uav_heap);
      DELETE(rtv_heap);
      DELETE(dsv_heap);

      for (int i = 0; i < NUM_BACK_BUFFERS; i++)
      {
        RELEASE(back_buffers[i]);
        RELEASE(command_allocators[i]);
      }

      initialized = false;
    }
  }

  //------------------------------------------------------------------------------------------------------
  void Device::PrepareCommandLists()
  {
    ThrowIfFailed(command_allocators[back_buffer_index]->Reset());
    ThrowIfFailed(command_list->Reset(command_allocators[back_buffer_index], nullptr));

    ID3D12DescriptorHeap* heaps[1] = {
      cbv_srv_uav_heap->GetDescriptorHeap()
    };

    command_list->SetDescriptorHeaps(1, heaps);
  }

  //------------------------------------------------------------------------------------------------------
  void Device::ExecuteCommandLists()
  {
    ThrowIfFalse(initialized, "Device needs to be initialized first. Call Device::Initialize().");

    ThrowIfFailed(command_list->Close());
    ID3D12CommandList* lists[] = { command_list };
    command_queue->ExecuteCommandLists(ARRAYSIZE(lists), lists);
  }

  //------------------------------------------------------------------------------------------------------
  void Device::WaitForGPU()
  {
    if (command_queue != nullptr && fence != nullptr && fence_event.IsValid())
    {
      UINT64 fence_value = fence_values[back_buffer_index];
      if (SUCCEEDED(command_queue->Signal(fence, fence_value)))
      {
        if (SUCCEEDED(fence->SetEventOnCompletion(fence_value, fence_event.Get())))
        {
          WaitForSingleObjectEx(fence_event.Get(), INFINITE, FALSE);
          fence_values[back_buffer_index]++;
        }
      }
    }
  }

  //------------------------------------------------------------------------------------------------------
  void Device::Present()
  {
    swap_chain->Present(0, 0);

    const UINT64 current_fence_value = fence_values[back_buffer_index];
    ThrowIfFailed(command_queue->Signal(fence, current_fence_value));

    // Update the back buffer index.
    back_buffer_index = swap_chain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (fence->GetCompletedValue() < fence_values[back_buffer_index])
    {
      ThrowIfFailed(fence->SetEventOnCompletion(fence_values[back_buffer_index], fence_event.Get()));
      WaitForSingleObjectEx(fence_event.Get(), INFINITE, FALSE);
    }

    fence_values[back_buffer_index] = current_fence_value + 1;
  }

  //------------------------------------------------------------------------------------------------------
  WRAPPED_GPU_POINTER Device::CreateFallbackWrappedPointer(DescriptorHeap* uav_descriptor_heap, ID3D12Resource* resource, UINT buffer_num_elements)
  {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    uav_desc.Buffer.NumElements = buffer_num_elements;

    UINT descriptor_heap_index = 0;

    if (!fallback_device->UsingRaytracingDriver())
    {
      rtrt::DescriptorHandle handle;
      uav_descriptor_heap->CreateDescriptor(device, resource, nullptr, &uav_desc, &handle);

      descriptor_heap_index = handle.descriptor_index;
    }

    return fallback_device->GetWrappedPointerSimple(descriptor_heap_index, resource->GetGPUVirtualAddress());
  }
  
  //------------------------------------------------------------------------------------------------------
  void Device::EnableRaytracing()
  {
    bool fallback_supported = false;
    {
      ID3D12Device* test_device = nullptr;
      UUID experimental_features[] = { D3D12ExperimentalShaderModels };

      fallback_supported = 
        SUCCEEDED(D3D12EnableExperimentalFeatures(1, experimental_features, nullptr, nullptr)) && 
        SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&test_device)));

      RELEASE(test_device);
    }

    if (fallback_supported == false)
    {
      LOG("Could not enable compute-based fallback layer.\n");
      LOG("Possible reasons:\n");
      LOG("  1) Your OS is not in developer mode.\n");
      LOG("  2) Your hardware does not support DXIL (Shader Model 6.0+).\n");
      LOG("Native DXR will still be tested.\n");
    }

    bool dxr_supported = false;
    {
      ID3D12Device* test_device = nullptr;
      D3D12_FEATURE_DATA_D3D12_OPTIONS5 feature_support_data = {};

      dxr_supported = 
        SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&test_device))) &&
        SUCCEEDED(test_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &feature_support_data, sizeof(feature_support_data))) &&
        feature_support_data.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;

      RELEASE(test_device);
    }

    if (dxr_supported == false)
    {
      LOG("Could not enable DirectX Raytracing.\n");
      LOG("Possible reasons:\n");
      LOG("  1) Your OS is outdated (must be Windows 10 version 1809).\n");
      LOG("  2) You are using the incorrect version of the Windows SDK (must be 10.0.17763.0).\n");
      LOG("  3) Your driver is outdated (NV: must be on driver 415+. AMD: unknown).\n");
      LOG("  4) Your GPU does not support DXR. (NV: must be on Turing or Volta architecture. AMD: unknown).\n");
    }

    if (fallback_supported == false && dxr_supported == false)
    {
      BREAK("Your system does not support either the fallback layer or native DirectX Raytracing.\n");
    }
    else if (dxr_supported == true)
    {
      LOG("Rendering path: NATIVE DirectX Raytracing through Fallback Layer API.\n");
    }
    else if (fallback_supported == true)
    {
      LOG("Rendering path: compute based raytracing through Fallback Layer API.\n");
    }
  }

  //------------------------------------------------------------------------------------------------------
  void Device::CreateDeviceResources()
  {
    ThrowIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)));

#ifdef _DEBUG
    ID3D12InfoQueue* info_queue = nullptr;
    
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&info_queue))))
    {
      info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
      info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

      D3D12_MESSAGE_ID hide[] =
      {
        D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
        D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
      };
      D3D12_INFO_QUEUE_FILTER filter = {};
      filter.DenyList.NumIDs = _countof(hide);
      filter.DenyList.pIDList = hide;
      info_queue->AddStorageFilterEntries(&filter);
    }

    RELEASE(info_queue);
#endif

    // Create a graphics command queue.
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));

    // Create a graphics & compute command allocator for each back buffer that will be rendered to.
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
      ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i])));
    }

    // Create a command list for recording graphics commands.
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[0], nullptr, IID_PPV_ARGS(&command_list)));
    ThrowIfFailed(command_list->Close());

    // Create a fence for tracking GPU execution progress.
    ThrowIfFailed(device->CreateFence(fence_values[back_buffer_index], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    fence_values[back_buffer_index]++;
    
    fence_event.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));
    if (!fence_event.IsValid())
    {
      ThrowIfFailed(E_FAIL);
    }

    // Create Fallback device & command list
    CreateRaytracingFallbackDeviceFlags create_device_flags =
      CreateRaytracingFallbackDeviceFlags::None;
    
    ThrowIfFailed(D3D12CreateRaytracingFallbackDevice(device, create_device_flags, 0, IID_PPV_ARGS(&fallback_device)));
    fallback_device->QueryRaytracingCommandList(command_list, IID_PPV_ARGS(&fallback_command_list));

    // Create descriptor heaps
    cbv_srv_uav_heap = new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Device::NUM_CBV_SRV_UAV_DESCRIPTORS);
    rtv_heap = new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, Device::NUM_RTV_DESCRIPTORS);
    dsv_heap = new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, Device::NUM_DSV_DESCRIPTORS);
  }

  //------------------------------------------------------------------------------------------------------
  void Device::CreateSwapChainResources()
  {
    WaitForGPU();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
      fence_values[i] = fence_values[back_buffer_index];
    }

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Width = width;
    swap_chain_desc.Height = height;
    swap_chain_desc.Format = back_buffer_format;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = NUM_BACK_BUFFERS;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swap_chain_desc.Flags = 0;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fs_swap_chain_desc = { 0 };
    fs_swap_chain_desc.Windowed = TRUE;

    IDXGISwapChain1* temp_swap_chain = nullptr;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(command_queue, glfwGetWin32Window(glfw_window), &swap_chain_desc, &fs_swap_chain_desc, nullptr, &temp_swap_chain));
    ThrowIfFailed(temp_swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain)));
    RELEASE(temp_swap_chain);

    ThrowIfFailed(factory->MakeWindowAssociation(glfwGetWin32Window(glfw_window), DXGI_MWA_NO_ALT_ENTER));

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
      ThrowIfFailed(swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffers[i])));

      wchar_t name[25] = {};
      swprintf_s(name, L"Back buffer #%u", i);
      back_buffers[i]->SetName(name);

      D3D12_RENDER_TARGET_VIEW_DESC rtv;
      rtv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
      rtv.Texture2D.MipSlice = 0;
      rtv.Texture2D.PlaneSlice = 0;

      rtv_heap->CreateDescriptor(device, back_buffers[i], &rtv, &back_buffer_rtvs[i]);
    }

    back_buffer_index = swap_chain->GetCurrentBackBufferIndex();

    viewport.TopLeftX = viewport.TopLeftY = 0.f;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = D3D12_MIN_DEPTH;
    viewport.MaxDepth = D3D12_MAX_DEPTH;

    scissor_rect.left = scissor_rect.top = 0;
    scissor_rect.right = width;
    scissor_rect.bottom = height;
  }
}