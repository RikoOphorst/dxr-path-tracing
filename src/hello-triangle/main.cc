#include <dxgi1_6.h>
#include <d3d12_1.h>
#include <D3D12RaytracingFallback.h>
#include <D3D12RaytracingHelpers.hpp>
#include <d3dx12.h>
#include <dxgidebug.h>

#include <wrl/event.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <string>
#include <sstream>
#include <iomanip>

#include "shader_table.h"
#include "util.h"

#include <compiled-shaders/raytrace.cso.h>

namespace GlobalRootSignatureParams {
  enum Value {
    OutputViewSlot = 0,
    AccelerationStructureSlot,
    Count
  };
}

namespace LocalRootSignatureParams {
  enum Value {
    ViewportConstantSlot = 0,
    Count
  };
}

const static size_t MAX_BACK_BUFFER_COUNT = 3;
const static UINT INVALID_UINT = 696969696;

struct Viewport
{
  float left;
  float top;
  float right;
  float bottom;
};

struct RayGenConstantBuffer
{
  Viewport viewport;
  Viewport stencil;
};

UINT width = 1280;
UINT height = 720;
float aspect_ratio = (float)(width) / (float)(height);

RECT window_bounds;

GLFWwindow* glfw_window;
HWND win32_window;

ID3D12RaytracingFallbackDevice* fallback_device = nullptr;
ID3D12RaytracingFallbackCommandList* fallback_command_list = nullptr;
ID3D12RaytracingFallbackStateObject* fallback_state_object = nullptr;
WRAPPED_GPU_POINTER fallback_top_level_acceleration_structure_pointer;

ID3D12RootSignature* raytracing_global_root_signature = nullptr;
ID3D12RootSignature* raytracing_local_root_signature = nullptr;
ID3D12RootSignature* raytracing_local_root_signature_empty = nullptr;

ID3D12DescriptorHeap* descriptor_heap = nullptr;
UINT descriptors_allocated = INVALID_UINT;
UINT descriptor_size = INVALID_UINT;

RayGenConstantBuffer ray_generation_constant_buffer;

typedef uint16_t Index;
struct Vertex { float v1, v2, v3; };
ID3D12Resource* index_buffer = nullptr;
ID3D12Resource* vertex_buffer = nullptr;

ID3D12Resource* acceleration_structure = nullptr;
ID3D12Resource* bottom_level_acceleration_structure = nullptr;
ID3D12Resource* top_level_acceleration_structure = nullptr;

ID3D12Resource* raytracing_output = nullptr;
D3D12_GPU_DESCRIPTOR_HANDLE raytracing_output_resource_uav_gpu_descriptor;
UINT raytracing_output_resource_uav_descriptor_heap_index = UINT_MAX;

static const wchar_t* hit_group_name = L"MyHitGroup";
static const wchar_t* ray_gen_shader_name = L"MyRaygenShader";
static const wchar_t* closest_hit_shader_name = L"MyClosestHitShader";
static const wchar_t* miss_shader_name = L"MyMissShader";
ID3D12Resource* hit_group_shader_table = nullptr;
ID3D12Resource* ray_gen_shader_table = nullptr;
ID3D12Resource* miss_shader_table = nullptr;

UINT adapter_id_override = UINT_MAX;
UINT back_buffer_index = 0;
IDXGIAdapter1* adapter = nullptr;
UINT adapter_id = UINT_MAX;
std::wstring adapter_description;
bool dxr_supported = false;

ID3D12Device* d3d_device = nullptr;
ID3D12CommandQueue* command_queue = nullptr;
ID3D12GraphicsCommandList* command_list = nullptr;
ID3D12CommandAllocator* command_allocators[MAX_BACK_BUFFER_COUNT] = {};

IDXGIFactory4* dxgi_factory = nullptr;
IDXGISwapChain3* swap_chain = nullptr;
ID3D12Resource* render_targets[MAX_BACK_BUFFER_COUNT];
ID3D12Resource* depth_stencil = nullptr;

ID3D12Fence* fence = nullptr;
UINT64 fence_values[MAX_BACK_BUFFER_COUNT] = {};
Microsoft::WRL::Wrappers::Event fence_event;

ID3D12DescriptorHeap* rtv_descriptor_heap = nullptr;
ID3D12DescriptorHeap* dsv_descriptor_heap = nullptr;
UINT rtv_descriptor_size = 0;
D3D12_VIEWPORT screen_viewport = {};
D3D12_RECT scissor_rect = {};

DXGI_FORMAT back_buffer_format = DXGI_FORMAT_R8G8B8A8_UNORM;
DXGI_FORMAT depth_buffer_format = DXGI_FORMAT_UNKNOWN;
UINT back_buffer_count = 3;
D3D_FEATURE_LEVEL d3d_min_feature_level = D3D_FEATURE_LEVEL_11_0;
D3D_FEATURE_LEVEL d3d_feature_level = D3D_FEATURE_LEVEL_11_0;
RECT output_size = { 0, 0, 1, 1 };

void WaitForGPU();

void InitializeAllVariables();
void CreateGLFWWindow();
void EnableDebugLayer();
void InitializeDXGIAdapter();
void EnableRaytracing();
void CreateDeviceResources();
void CreateSwapChainResources();
void CreateRaytracingInterfaces();
void CreateRootSignatures();
void CreateRaytracingPipelineStateObject();
void CreateDescriptorHeap();
void BuildGeometry();
void BuildAccelerationStructures();
void BuildShaderTables();
void CreateRaytracingOutputResource();
void MainLoop();
void ReleaseAllVariables();

UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);
WRAPPED_GPU_POINTER CreateFallbackWrappedPointer(ID3D12Resource* resource, UINT bufferNumElements);
void ExecuteCommandList();

int main(int argc, char** argv)
{
  InitializeAllVariables();
  CreateGLFWWindow();
  InitializeDXGIAdapter();
  EnableRaytracing();
  CreateDeviceResources();
  CreateSwapChainResources();
  CreateRaytracingInterfaces();
  CreateRootSignatures();
  CreateRaytracingPipelineStateObject();
  CreateDescriptorHeap();
  BuildGeometry();
  BuildAccelerationStructures();
  BuildShaderTables();
  CreateRaytracingOutputResource();

  MainLoop();

  ReleaseAllVariables();

  glfwTerminate();

  return 0;
}

void WaitForGPU()
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

void InitializeAllVariables()
{
  width = 1280;
  height = 720;
  aspect_ratio = (float)width / (float)height;

  glfw_window = nullptr;
  win32_window = NULL;

  fallback_device = nullptr;
  fallback_command_list = nullptr;
  fallback_state_object = nullptr;
  fallback_top_level_acceleration_structure_pointer = { 0 };

  raytracing_global_root_signature = nullptr;
  raytracing_local_root_signature = nullptr;
  raytracing_local_root_signature_empty = nullptr;

  descriptor_heap = nullptr;
  descriptors_allocated = 0;
  descriptor_size = 0;

  float border = 0.0f;
  ray_generation_constant_buffer.viewport = { -1.0f, -1.0f, 1.0f, 1.0f };

  if (width <= height)
  {
    ray_generation_constant_buffer.stencil =
    {
      -1 + border, -1 + border * aspect_ratio,
      1.0f - border, 1 - border * aspect_ratio
    };
  }
  else
  {
    ray_generation_constant_buffer.stencil =
    {
      -1 + border / aspect_ratio, -1 + border,
       1 - border / aspect_ratio, 1.0f - border
    };
  }

  index_buffer = nullptr;
  vertex_buffer = nullptr;

  acceleration_structure = nullptr;
  bottom_level_acceleration_structure = nullptr;
  top_level_acceleration_structure = nullptr;

  raytracing_output = nullptr;
  raytracing_output_resource_uav_gpu_descriptor.ptr = UINT64_MAX;
  raytracing_output_resource_uav_descriptor_heap_index = UINT_MAX;

  miss_shader_table = nullptr;
  hit_group_shader_table = nullptr;
  ray_gen_shader_table = nullptr;

  adapter_id_override = UINT_MAX;
  back_buffer_index = 0;
  adapter = nullptr;
  adapter_id = UINT_MAX;
  adapter_description = L"UNKNOWN";
  dxr_supported = false;

  d3d_device = nullptr;
  command_queue = nullptr;
  command_list = nullptr;

  for (size_t i = 0; i < MAX_BACK_BUFFER_COUNT; i++)
  {
    command_allocators[i] = nullptr;
    render_targets[i] = nullptr;
    fence_values[i] = 0;
  }

  dxgi_factory = nullptr;
  swap_chain = nullptr;
  depth_stencil = nullptr;

  fence = nullptr;

  rtv_descriptor_heap = nullptr;
  dsv_descriptor_heap = nullptr;
  rtv_descriptor_size = 0;

  screen_viewport.Height = 0.0f;
  screen_viewport.Width = 0.0f;
  screen_viewport.MaxDepth = 0.0f;
  screen_viewport.MinDepth = 0.0f;
  screen_viewport.TopLeftX = 0.0f;
  screen_viewport.TopLeftY = 0.0f;

  scissor_rect.bottom = 0;
  scissor_rect.left = 0;
  scissor_rect.right = 0;
  scissor_rect.top = 0;

  back_buffer_format = DXGI_FORMAT_R8G8B8A8_UNORM;
  depth_buffer_format = DXGI_FORMAT_UNKNOWN; // disables depth buffer
  back_buffer_count = 3;
  d3d_min_feature_level = D3D_FEATURE_LEVEL_11_0;
  d3d_feature_level = D3D_FEATURE_LEVEL_11_0;

  output_size.left = output_size.top = 0;
  output_size.right = width;
  output_size.bottom = height;
}

void CreateGLFWWindow()
{
  glfwInit();

  glfw_window = glfwCreateWindow(1280, 720, "Realtime Raytracing using DXR", nullptr, nullptr);
  win32_window = glfwGetWin32Window(glfw_window);
}

void EnableDebugLayer()
{
  ID3D12Debug* debug_controller = nullptr;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
  {
    debug_controller->EnableDebugLayer();
  }
  else
  {
    printf("WARNING: Direct3D Debug Device is not available\n");
  }

  IDXGIInfoQueue* dxgi_info_queue = nullptr;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_info_queue))))
  {
    CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgi_factory));

    dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
    dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
  }

  RELEASE(debug_controller);
  RELEASE(dxgi_info_queue);
}

void InitializeDXGIAdapter()
{
  RELEASE(dxgi_factory);

  bool debug_mode = false;

#ifdef DEBUG
  EnableDebugLayer();
  debug_mode = true;
#endif

  if (debug_mode == false)
  {
    CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
  }

  RELEASE(adapter);

  IDXGIAdapter1* current_adapter = nullptr;
  for (UINT current_adapter_id = 0; DXGI_ERROR_NOT_FOUND != dxgi_factory->EnumAdapters1(current_adapter_id, &current_adapter); ++current_adapter_id)
  {
    if (adapter_id_override != UINT_MAX && current_adapter_id != adapter_id_override)
    {
      RELEASE(current_adapter);
      continue;
    }

    DXGI_ADAPTER_DESC1 desc;
    ThrowIfFailed(current_adapter->GetDesc1(&desc));

    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
    {
      // Don't select the Basic Render Driver adapter.
      RELEASE(current_adapter);
      continue;
    }

    // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
    if (SUCCEEDED(D3D12CreateDevice(current_adapter, d3d_min_feature_level, _uuidof(ID3D12Device), nullptr)))
    {
      adapter_id = current_adapter_id;
      adapter_description = desc.Description;
#ifdef _DEBUG
      wchar_t buff[256] = {};
      swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", current_adapter_id, desc.VendorId, desc.DeviceId, desc.Description);
      OutputDebugStringW(buff);
#endif
      break;
    }
    else
    {
      RELEASE(current_adapter);
    }
  }

  if (current_adapter == nullptr)
  {
    if (adapter_id_override != UINT_MAX)
    {
      BREAK("Unavailable adapter requested.");
    }
    else
    {
      BREAK("Unavailable adapter.");
    }
  }

  adapter = current_adapter;
}

void EnableRaytracing()
{
  // DXR is an experimental feature and needs to be enabled before creating a D3D12 device.
  ID3D12Device* test_device = nullptr;
  ID3D12DeviceRaytracingPrototype* test_raytracing_device = nullptr;
  UUID experimental_features[] = { D3D12ExperimentalShaderModels, D3D12RaytracingPrototype };

  dxr_supported = SUCCEEDED(D3D12EnableExperimentalFeatures(2, experimental_features, nullptr, nullptr))
    && SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&test_device)))
    && SUCCEEDED(test_device->QueryInterface(IID_PPV_ARGS(&test_raytracing_device)));

  RELEASE(test_device);
  RELEASE(test_raytracing_device);

  if (!dxr_supported)
  {
    // DXR is not supported on the available hardware.
    // Instead, try looking for support for the fallback layers on this machine (only experimental shader models need to be supported for this)

    LOG(
      "Could not enable DirectX Raytracing. (D3D12EnableExperimentalFeatures(D3D12ExperimentalShaderModels, D3D12RaytracingPrototype) failed).\n" \
      "Possible reasons:\n" \
      "  1) your OS is not in developer mode.\n" \
      "  2) your GPU driver doesn't match the D3D12 runtime loaded by the app (d3d12.dll and friends).\n" \
      "  3) your D3D12 runtime doesn't match the D3D12 headers used by your app (in particular, the GUID passed to D3D12EnableExperimentalFeatures).\n\n");

    LOG("Enabling compute based fallback raytracing support.\n");

    if (!(SUCCEEDED(D3D12EnableExperimentalFeatures(1, experimental_features, nullptr, nullptr))
      && SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&test_device)))))
    {
      BREAK("Could not enable compute based fallback raytracing support (D3D12EnableExperimentalFeatures(D3D12ExperimentalShaderModels) failed).\n");
    }
    else
    {
      LOG("Succesfully enabled compute based fallback raytracing layer.\n");
    }

    RELEASE(test_device);
  }
}

void CreateDeviceResources()
{
  ThrowIfFailed(D3D12CreateDevice(adapter, d3d_min_feature_level, IID_PPV_ARGS(&d3d_device)));

#ifdef DEBUG
  ID3D12InfoQueue* d3d_info_queue = nullptr;
  if (SUCCEEDED(d3d_device->QueryInterface(IID_PPV_ARGS(&d3d_info_queue))))
  {
    d3d_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    d3d_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

    D3D12_MESSAGE_ID hide[] =
    {
      D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
      D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
    };
    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumIDs = _countof(hide);
    filter.DenyList.pIDList = hide;
    d3d_info_queue->AddStorageFilterEntries(&filter);
  }

  RELEASE(d3d_info_queue);
#endif

  // Determine maximum supported feature level for this device
  static const D3D_FEATURE_LEVEL feature_levels[] =
  {
    D3D_FEATURE_LEVEL_12_1,
    D3D_FEATURE_LEVEL_12_0,
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
  };

  D3D12_FEATURE_DATA_FEATURE_LEVELS feat_levels =
  {
    _countof(feature_levels), feature_levels, D3D_FEATURE_LEVEL_11_0
  };

  HRESULT hr = d3d_device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &feat_levels, sizeof(feat_levels));

  if (SUCCEEDED(hr))
  {
    d3d_feature_level = feat_levels.MaxSupportedFeatureLevel;
  }
  else
  {
    d3d_feature_level = d3d_min_feature_level;
  }

  // Create the command queue.
  D3D12_COMMAND_QUEUE_DESC queue_desc = {};
  queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

  ThrowIfFailed(d3d_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));

  // Create descriptor heaps for render target views and depth stencil views.
  D3D12_DESCRIPTOR_HEAP_DESC rtv_descriptor_heap_desc = {};
  rtv_descriptor_heap_desc.NumDescriptors = back_buffer_count;
  rtv_descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

  ThrowIfFailed(d3d_device->CreateDescriptorHeap(&rtv_descriptor_heap_desc, IID_PPV_ARGS(&rtv_descriptor_heap)));

  rtv_descriptor_size = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  if (depth_buffer_format != DXGI_FORMAT_UNKNOWN)
  {
    D3D12_DESCRIPTOR_HEAP_DESC dsv_descriptor_heap_desc = {};
    dsv_descriptor_heap_desc.NumDescriptors = 1;
    dsv_descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

    ThrowIfFailed(d3d_device->CreateDescriptorHeap(&dsv_descriptor_heap_desc, IID_PPV_ARGS(&dsv_descriptor_heap)));
  }

  // Create a command allocator for each back buffer that will be rendered to.
  for (UINT n = 0; n < back_buffer_count; n++)
  {
    ThrowIfFailed(d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[n])));
  }

  // Create a command list for recording graphics commands.
  ThrowIfFailed(d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[0], nullptr, IID_PPV_ARGS(&command_list)));
  ThrowIfFailed(command_list->Close());

  // Create a fence for tracking GPU execution progress.
  ThrowIfFailed(d3d_device->CreateFence(fence_values[back_buffer_index], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
  fence_values[back_buffer_index]++;

  fence_event.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));
  if (!fence_event.IsValid())
  {
    ThrowIfFailed(E_FAIL);
  }
}

void CreateSwapChainResources()
{
  if (!win32_window)
  {
    BREAK("Must create a window first before creating swap chain.");
  }

  // Wait until all previous GPU work is complete.
  WaitForGPU();

  // Release resources that are tied to the swap chain and update fence values.
  for (UINT n = 0; n < back_buffer_count; n++)
  {
    RELEASE(render_targets[n]);
    fence_values[n] = fence_values[back_buffer_index];
  }

  // Determine the render target size in pixels.
  UINT back_buffer_width = max(output_size.right - output_size.left, 1);
  UINT back_buffer_height = max(output_size.bottom - output_size.top, 1);

  // If the swap chain already exists, resize it, otherwise create one.
  if (swap_chain != nullptr)
  {
    // If the swap chain already exists, resize it.
    HRESULT hr = swap_chain->ResizeBuffers(
      back_buffer_count,
      back_buffer_width,
      back_buffer_height,
      back_buffer_format,
      0
    );

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
      char buff[64] = {};
      sprintf_s(buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n", (hr == DXGI_ERROR_DEVICE_REMOVED) ? d3d_device->GetDeviceRemovedReason() : hr);
      LOG(buff);
#endif
      // If the device was removed for any reason, a new device and swap chain will need to be created.
      // HandleDeviceLost();

      // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method 
      // and correctly set up the new device.
      return;
    }
    else
    {
      ThrowIfFailed(hr);
    }
  }
  else
  {
    // Create a descriptor for the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Width = back_buffer_width;
    swap_chain_desc.Height = back_buffer_height;
    swap_chain_desc.Format = back_buffer_format;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = back_buffer_count;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swap_chain_desc.Flags = 0;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fs_swap_chain_desc = { 0 };
    fs_swap_chain_desc.Windowed = TRUE;

    IDXGISwapChain1* temp_swap_chain = nullptr;

    ThrowIfFailed(dxgi_factory->CreateSwapChainForHwnd(command_queue, win32_window, &swap_chain_desc, &fs_swap_chain_desc, nullptr, &temp_swap_chain));

    ThrowIfFailed(temp_swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain)));

    RELEASE(temp_swap_chain);

    ThrowIfFailed(dxgi_factory->MakeWindowAssociation(win32_window, DXGI_MWA_NO_ALT_ENTER));
  }

  // Obtain the back buffers for this window which will be the final render targets
  // and create render target views for each of them.
  for (UINT n = 0; n < back_buffer_count; n++)
  {
    ThrowIfFailed(swap_chain->GetBuffer(n, IID_PPV_ARGS(&render_targets[n])));

    wchar_t name[25] = {};
    swprintf_s(name, L"Render target %u", n);
    render_targets[n]->SetName(name);

    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = back_buffer_format;
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), n, rtv_descriptor_size);
    d3d_device->CreateRenderTargetView(render_targets[n], &rtv_desc, rtv_handle);
  }

  // Reset the index to the current back buffer.
  back_buffer_index = swap_chain->GetCurrentBackBufferIndex();

  if (depth_buffer_format != DXGI_FORMAT_UNKNOWN)
  {
    // Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view
    // on this surface.
    CD3DX12_HEAP_PROPERTIES depth_heap_properties(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC depth_stencil_desc = CD3DX12_RESOURCE_DESC::Tex2D(
      depth_buffer_format,
      back_buffer_width,
      back_buffer_height,
      1, // This depth stencil view has only one texture.
      1  // Use a single mipmap level.
    );
    depth_stencil_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depth_optimized_clear_value = {};
    depth_optimized_clear_value.Format = depth_buffer_format;
    depth_optimized_clear_value.DepthStencil.Depth = 1.0f;
    depth_optimized_clear_value.DepthStencil.Stencil = 0;

    ThrowIfFailed(d3d_device->CreateCommittedResource(&depth_heap_properties,
      D3D12_HEAP_FLAG_NONE,
      &depth_stencil_desc,
      D3D12_RESOURCE_STATE_DEPTH_WRITE,
      &depth_optimized_clear_value,
      IID_PPV_ARGS(&depth_stencil)
    ));

    depth_stencil->SetName(L"Depth stencil");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format = depth_buffer_format;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    d3d_device->CreateDepthStencilView(depth_stencil, &dsv_desc, dsv_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
  }

  // Set the 3D rendering viewport and scissor rectangle to target the entire window.
  screen_viewport.TopLeftX = screen_viewport.TopLeftY = 0.f;
  screen_viewport.Width = static_cast<float>(back_buffer_width);
  screen_viewport.Height = static_cast<float>(back_buffer_height);
  screen_viewport.MinDepth = D3D12_MIN_DEPTH;
  screen_viewport.MaxDepth = D3D12_MAX_DEPTH;

  scissor_rect.left = scissor_rect.top = 0;
  scissor_rect.right = back_buffer_width;
  scissor_rect.bottom = back_buffer_height;
}

void CreateRaytracingInterfaces()
{
  CreateRaytracingFallbackDeviceFlags create_device_flags = CreateRaytracingFallbackDeviceFlags::None;

  ThrowIfFailed(D3D12CreateRaytracingFallbackDevice(d3d_device, create_device_flags, 0, IID_PPV_ARGS(&fallback_device)));
  fallback_device->QueryRaytracingCommandList(command_list, IID_PPV_ARGS(&fallback_command_list));
}

void CreateRootSignatures()
{
  ID3DBlob* blob = nullptr;
  ID3DBlob* error = nullptr;

  // Global Root Signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
  {
    CD3DX12_DESCRIPTOR_RANGE uav_descriptor;
    uav_descriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_ROOT_PARAMETER root_parameters[GlobalRootSignatureParams::Count];
    root_parameters[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &uav_descriptor);
    root_parameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
    CD3DX12_ROOT_SIGNATURE_DESC global_root_signature_desc(ARRAYSIZE(root_parameters), root_parameters);

    ThrowIfFailed(fallback_device->D3D12SerializeRootSignature(&global_root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
    ThrowIfFailed(fallback_device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&raytracing_global_root_signature)));
  }

  RELEASE(blob);
  RELEASE(error);

  // Local Root Signature
  // This is a root signature that enables a shader to have unique arguments that come from shader tables.
  {
    CD3DX12_ROOT_PARAMETER root_parameters[LocalRootSignatureParams::Count];
    root_parameters[LocalRootSignatureParams::ViewportConstantSlot].InitAsConstants(SizeOfInUint32(ray_generation_constant_buffer), 0, 0);
    CD3DX12_ROOT_SIGNATURE_DESC local_root_signature_desc(ARRAYSIZE(root_parameters), root_parameters);
    local_root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    ThrowIfFailed(fallback_device->D3D12SerializeRootSignature(&local_root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
    ThrowIfFailed(fallback_device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&raytracing_local_root_signature)));
  }

  RELEASE(blob);
  RELEASE(error);

  // Empty local root signature
  {
    CD3DX12_ROOT_SIGNATURE_DESC local_root_signature_desc(D3D12_DEFAULT);
    local_root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    ThrowIfFailed(fallback_device->D3D12SerializeRootSignature(&local_root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
    ThrowIfFailed(fallback_device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&raytracing_local_root_signature_empty)));
  }
}

void CreateRaytracingPipelineStateObject()
{
  // Create 7 subobjects that combine into a RTPSO:
  // Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
  // Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
  // This simple sample utilizes default shader association except for local root signature subobject
  // which has an explicit association specified purely for demonstration purposes.
  // 1 - DXIL library
  // 1 - Triangle hit group
  // 1 - Shader config
  // 2 - Local root signature and association
  // 1 - Global root signature
  // 1 - Pipeline config
  CD3D12_STATE_OBJECT_DESC raytracing_pipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };


  // DXIL library
  // This contains the shaders and their entrypoints for the state object.
  // Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
  auto lib = raytracing_pipeline.CreateSubobject<CD3D12_DXIL_LIBRARY_SUBOBJECT>();
  D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void *)cso_raytrace, ARRAYSIZE(cso_raytrace));
  lib->SetDXILLibrary(&libdxil);
  // Define which shader exports to surface from the library.
  // If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
  // In this sample, this could be omitted for convenience since the sample uses all shaders in the library. 
  {
    lib->DefineExport(ray_gen_shader_name);
    lib->DefineExport(closest_hit_shader_name);
    lib->DefineExport(miss_shader_name);
  }

  // Triangle hit group
  // A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
  // In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
  auto hit_group = raytracing_pipeline.CreateSubobject<CD3D12_HIT_GROUP_SUBOBJECT>();
  hit_group->SetClosestHitShaderImport(closest_hit_shader_name);
  hit_group->SetHitGroupExport(hit_group_name);

  // Shader config
  // Defines the maximum sizes in bytes for the ray payload and attribute structure.
  auto shader_config = raytracing_pipeline.CreateSubobject<CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
  UINT payload_size = 4 * sizeof(float);   // float4 color
  UINT attribute_size = 2 * sizeof(float); // float2 barycentrics
  shader_config->Config(payload_size, attribute_size);

  // Local root signature and shader association
  // Local root signature to be used in a ray gen shader.
  {
    auto local_root_signature = raytracing_pipeline.CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    local_root_signature->SetRootSignature(raytracing_local_root_signature);
    // Shader association
    auto root_signature_association = raytracing_pipeline.CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    root_signature_association->SetSubobjectToAssociate(*local_root_signature);
    root_signature_association->AddExport(ray_gen_shader_name);
  }

  // Empty local root signature to be used in a miss shader and a hit group.
  {
    auto local_root_signature = raytracing_pipeline.CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    local_root_signature->SetRootSignature(raytracing_local_root_signature_empty);
    // Shader association
    auto root_signature_association = raytracing_pipeline.CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    root_signature_association->SetSubobjectToAssociate(*local_root_signature);
    root_signature_association->AddExport(miss_shader_name);
    root_signature_association->AddExport(hit_group_name);
  }

  // This is a root signature that enables a shader to have unique arguments that come from shader tables.

  // Global root signature
  // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
  auto global_root_signature = raytracing_pipeline.CreateSubobject<CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
  global_root_signature->SetRootSignature(raytracing_global_root_signature);

  // Pipeline config
  // Defines the maximum TraceRay() recursion depth.
  auto pipeline_config = raytracing_pipeline.CreateSubobject<CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
  // PERFOMANCE TIP: Set max recursion depth as low as needed 
  // as drivers may apply optimization strategies for low recursion depths. 
  UINT max_recursion_depth = 1; // ~ primary rays only. 
  pipeline_config->Config(max_recursion_depth);

#if _DEBUG
  PrintStateObjectDesc(raytracing_pipeline);
#endif

  ThrowIfFailed(fallback_device->CreateStateObject(raytracing_pipeline, IID_PPV_ARGS(&fallback_state_object)));
}

void CreateDescriptorHeap()
{
  D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
  // Allocate a heap for 3 descriptors:
  // 2 - bottom and top level acceleration structure fallback wrapped pointers
  // 1 - raytracing output texture SRV
  descriptor_heap_desc.NumDescriptors = 3;
  descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  descriptor_heap_desc.NodeMask = 0;
  d3d_device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap));
  descriptor_heap->SetName(L"Descriptor Heap");

  descriptor_size = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void BuildGeometry()
{
  Index indices[] =
  {
    0, 1, 2
  };

  float depthValue = 1.0f;
  float offset = 0.7f;
  Vertex vertices[] =
  {
    // The sample raytraces in screen space coordinates.
    // Since DirectX screen space coordinates are right handed (i.e. Y axis points down).
    // Define the vertices in counter clockwise order ~ clockwise in left handed.
    { 0, -offset, depthValue },
    { -offset, offset, depthValue },
    { offset, offset, depthValue }
  };

  AllocateUploadBuffer(d3d_device, vertices, sizeof(vertices), &vertex_buffer, L"Vertex Buffer");
  AllocateUploadBuffer(d3d_device, indices, sizeof(indices), &index_buffer, L"Index Buffer");
}

void BuildAccelerationStructures()
{
  // Reset the command list for the acceleration structure construction.
  command_list->Reset(command_allocators[back_buffer_index], nullptr);

  D3D12_RAYTRACING_GEOMETRY_DESC geometry_desc = {};
  geometry_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
  geometry_desc.Triangles.IndexBuffer = index_buffer->GetGPUVirtualAddress();
  geometry_desc.Triangles.IndexCount = static_cast<UINT>(index_buffer->GetDesc().Width) / sizeof(Index);
  geometry_desc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
  geometry_desc.Triangles.Transform3x4 = 0;
  geometry_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
  geometry_desc.Triangles.VertexCount = static_cast<UINT>(vertex_buffer->GetDesc().Width) / sizeof(Vertex);
  geometry_desc.Triangles.VertexBuffer.StartAddress = vertex_buffer->GetGPUVirtualAddress();
  geometry_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

  // Mark the geometry as opaque. 
  // PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
  // Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
  geometry_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

  // Get required sizes for an acceleration structure.
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top_level_inputs = {};
  top_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  top_level_inputs.Flags = build_flags;
  top_level_inputs.NumDescs = 1;

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO top_level_prebuild_info = {};

  top_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  top_level_inputs.Flags = build_flags;
  top_level_inputs.NumDescs = 1;
  top_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
  fallback_device->GetRaytracingAccelerationStructurePrebuildInfo(&top_level_inputs, &top_level_prebuild_info);
  ThrowIfFalse(top_level_prebuild_info.ResultDataMaxSizeInBytes > 0);

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottom_level_prebuild_info = {};
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottom_level_inputs = top_level_inputs;
  bottom_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  bottom_level_inputs.pGeometryDescs = &geometry_desc;
  fallback_device->GetRaytracingAccelerationStructurePrebuildInfo(&bottom_level_inputs, &bottom_level_prebuild_info);
  ThrowIfFalse(bottom_level_prebuild_info.ResultDataMaxSizeInBytes > 0);

  ID3D12Resource* scratch_resource = nullptr;
  AllocateUAVBuffer(d3d_device, max(top_level_prebuild_info.ScratchDataSizeInBytes, bottom_level_prebuild_info.ScratchDataSizeInBytes), &scratch_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

  // Allocate resources for acceleration structures.
  // Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
  // Default heap is OK since the application doesn’t need CPU read/write access to them. 
  // The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
  // and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
  //  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
  //  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
  {
    D3D12_RESOURCE_STATES initial_resource_state;
    initial_resource_state = fallback_device->GetAccelerationStructureResourceState();

    AllocateUAVBuffer(d3d_device, bottom_level_prebuild_info.ResultDataMaxSizeInBytes, &bottom_level_acceleration_structure, initial_resource_state, L"BottomLevelAccelerationStructure");
    AllocateUAVBuffer(d3d_device, top_level_prebuild_info.ResultDataMaxSizeInBytes, &top_level_acceleration_structure, initial_resource_state, L"TopLevelAccelerationStructure");
  }

  // Note on Emulated GPU pointers (AKA Wrapped pointers) requirement in Fallback Layer:
  // The primary point of divergence between the DXR API and the compute-based Fallback layer is the handling of GPU pointers. 
  // DXR fundamentally requires that GPUs be able to dynamically read from arbitrary addresses in GPU memory. 
  // The existing Direct Compute API today is more rigid than DXR and requires apps to explicitly inform the GPU what blocks of memory it will access with SRVs/UAVs.
  // In order to handle the requirements of DXR, the Fallback Layer uses the concept of Emulated GPU pointers, 
  // which requires apps to create views around all memory they will access for raytracing, 
  // but retains the DXR-like flexibility of only needing to bind the top level acceleration structure at DispatchRays.
  //
  // The Fallback Layer interface uses WRAPPED_GPU_POINTER to encapsulate the underlying pointer
  // which will either be an emulated GPU pointer for the compute - based path or a GPU_VIRTUAL_ADDRESS for the DXR path.

  // Create an instance desc for the bottom-level acceleration structure.
  ID3D12Resource* instance_descs = nullptr;
  {
    D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC instance_desc = {};
    instance_desc.Transform[0][0] = instance_desc.Transform[1][1] = instance_desc.Transform[2][2] = 1;
    instance_desc.InstanceMask = 1;
    UINT num_buffer_elements = static_cast<UINT>(bottom_level_prebuild_info.ResultDataMaxSizeInBytes) / sizeof(UINT32);
    instance_desc.AccelerationStructure = CreateFallbackWrappedPointer(bottom_level_acceleration_structure, num_buffer_elements);
    AllocateUploadBuffer(d3d_device, &instance_desc, sizeof(instance_desc), &instance_descs, L"InstanceDescs");
  }

  // Create a wrapped pointer to the acceleration structure.
  {
    UINT num_buffer_elements = static_cast<UINT>(top_level_prebuild_info.ResultDataMaxSizeInBytes) / sizeof(UINT32);
    fallback_top_level_acceleration_structure_pointer = CreateFallbackWrappedPointer(top_level_acceleration_structure, num_buffer_elements);
  }

  // Bottom Level Acceleration Structure desc
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
  {
    bottomLevelBuildDesc.Inputs = bottom_level_inputs;
    bottomLevelBuildDesc.ScratchAccelerationStructureData = scratch_resource->GetGPUVirtualAddress();
    bottomLevelBuildDesc.DestAccelerationStructureData = bottom_level_acceleration_structure->GetGPUVirtualAddress();
  }

  // Top Level Acceleration Structure desc
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = bottomLevelBuildDesc;
  {
    top_level_inputs.InstanceDescs = instance_descs->GetGPUVirtualAddress();
    topLevelBuildDesc.Inputs = top_level_inputs;
    topLevelBuildDesc.DestAccelerationStructureData = top_level_acceleration_structure->GetGPUVirtualAddress();
    topLevelBuildDesc.ScratchAccelerationStructureData = scratch_resource->GetGPUVirtualAddress();
  }

  auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
  {
    raytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
    command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(bottom_level_acceleration_structure));
    raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
  };

  // Build acceleration structure.

  // Set the descriptor heaps to be used during acceleration structure build for the Fallback Layer.
  ID3D12DescriptorHeap *pDescriptorHeaps[] = { descriptor_heap };
  fallback_command_list->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
  BuildAccelerationStructure(fallback_command_list);

  // Kick off acceleration structure construction.
  ExecuteCommandList();

  // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
  WaitForGPU();

  RELEASE(scratch_resource);
  RELEASE(instance_descs);
}

void BuildShaderTables()
{
  void* ray_gen_shader_identifier = nullptr;
  void* miss_shader_identifier = nullptr;
  void* hit_group_shader_identifier = nullptr;

  auto GetShaderIdentifiers = [&](auto* state_object_properties)
  {
    ray_gen_shader_identifier = state_object_properties->GetShaderIdentifier(ray_gen_shader_name);
    miss_shader_identifier = state_object_properties->GetShaderIdentifier(miss_shader_name);
    hit_group_shader_identifier = state_object_properties->GetShaderIdentifier(hit_group_name);
  };

  // Get shader identifiers.
  GetShaderIdentifiers(fallback_state_object);
  UINT shader_identifier_size = fallback_device->GetShaderIdentifierSize();

  // Ray gen shader table
  {
    struct RootArguments {
      RayGenConstantBuffer cb;
    } root_arguments;
    root_arguments.cb = ray_generation_constant_buffer;

    UINT num_shader_records = 1;
    UINT shader_record_size = shader_identifier_size + sizeof(root_arguments);
    ShaderTable ray_gen_shader_table_helper(d3d_device, num_shader_records, shader_record_size, L"RayGenShaderTable");
    ray_gen_shader_table_helper.push_back(ShaderRecord(ray_gen_shader_identifier, shader_identifier_size, &root_arguments, sizeof(root_arguments)));
    ray_gen_shader_table = ray_gen_shader_table_helper.GetResource();
  }

  // Miss shader table
  {
    UINT num_shader_records = 1;
    UINT shader_record_size = shader_identifier_size;
    ShaderTable miss_shader_table_helper(d3d_device, num_shader_records, shader_record_size, L"MissShaderTable");
    miss_shader_table_helper.push_back(ShaderRecord(miss_shader_identifier, shader_identifier_size));
    miss_shader_table = miss_shader_table_helper.GetResource();
  }

  // Hit group shader table
  {
    UINT numShaderRecords = 1;
    UINT shaderRecordSize = shader_identifier_size;
    ShaderTable hit_group_shader_table_helper(d3d_device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
    hit_group_shader_table_helper.push_back(ShaderRecord(hit_group_shader_identifier, shader_identifier_size));
    hit_group_shader_table = hit_group_shader_table_helper.GetResource();
  }
}

void CreateRaytracingOutputResource()
{
  // Create the output resource. The dimensions and format should match the swap-chain.
  auto uav_desc = CD3DX12_RESOURCE_DESC::Tex2D(back_buffer_format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

  auto default_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  ThrowIfFailed(d3d_device->CreateCommittedResource(
    &default_heap_properties, D3D12_HEAP_FLAG_NONE, &uav_desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&raytracing_output)));
  raytracing_output->SetName(L"Raytracing Output");

  D3D12_CPU_DESCRIPTOR_HANDLE uav_descriptor_handle;
  raytracing_output_resource_uav_descriptor_heap_index = AllocateDescriptor(&uav_descriptor_handle, raytracing_output_resource_uav_descriptor_heap_index);
  D3D12_UNORDERED_ACCESS_VIEW_DESC UAV_desc = {};
  UAV_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
  d3d_device->CreateUnorderedAccessView(raytracing_output, nullptr, &UAV_desc, uav_descriptor_handle);
  raytracing_output_resource_uav_gpu_descriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptor_heap->GetGPUDescriptorHandleForHeapStart(), raytracing_output_resource_uav_descriptor_heap_index, descriptor_size);
}

void MainLoop()
{
  while (!glfwWindowShouldClose(glfw_window))
  {
    glfwPollEvents();

    ThrowIfFailed(command_allocators[back_buffer_index]->Reset());
    ThrowIfFailed(command_list->Reset(command_allocators[back_buffer_index], nullptr));

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_targets[back_buffer_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    command_list->ResourceBarrier(1, &barrier);

    auto DispatchRays = [&](auto* cmd_list, auto* state_obj, auto* dispatch_desc)
    {
      dispatch_desc->HitGroupTable.StartAddress = hit_group_shader_table->GetGPUVirtualAddress();
      dispatch_desc->HitGroupTable.SizeInBytes = hit_group_shader_table->GetDesc().Width;
      dispatch_desc->HitGroupTable.StrideInBytes = dispatch_desc->HitGroupTable.SizeInBytes;
      dispatch_desc->MissShaderTable.StartAddress = miss_shader_table->GetGPUVirtualAddress();
      dispatch_desc->MissShaderTable.SizeInBytes = miss_shader_table->GetDesc().Width;
      dispatch_desc->MissShaderTable.StrideInBytes = dispatch_desc->MissShaderTable.SizeInBytes;
      dispatch_desc->RayGenerationShaderRecord.StartAddress = ray_gen_shader_table->GetGPUVirtualAddress();
      dispatch_desc->RayGenerationShaderRecord.SizeInBytes = ray_gen_shader_table->GetDesc().Width;
      dispatch_desc->Width = width;
      dispatch_desc->Height = height;
      dispatch_desc->Depth = 1;
      cmd_list->SetPipelineState1(state_obj);
      cmd_list->DispatchRays(dispatch_desc);
    };

    command_list->SetComputeRootSignature(raytracing_global_root_signature);

    D3D12_DISPATCH_RAYS_DESC dispatch_desc = {};
    fallback_command_list->SetDescriptorHeaps(1, &descriptor_heap);
    command_list->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, raytracing_output_resource_uav_gpu_descriptor);
    fallback_command_list->SetTopLevelAccelerationStructure(GlobalRootSignatureParams::AccelerationStructureSlot, fallback_top_level_acceleration_structure_pointer);
    DispatchRays(fallback_command_list, fallback_state_object, &dispatch_desc);

    D3D12_RESOURCE_BARRIER pre_copy_barriers[2];
    pre_copy_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(render_targets[back_buffer_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    pre_copy_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(raytracing_output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    command_list->ResourceBarrier(ARRAYSIZE(pre_copy_barriers), pre_copy_barriers);

    command_list->CopyResource(render_targets[back_buffer_index], raytracing_output);

    D3D12_RESOURCE_BARRIER post_copy_barriers[2];
    post_copy_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(render_targets[back_buffer_index], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    post_copy_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(raytracing_output, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    command_list->ResourceBarrier(ARRAYSIZE(post_copy_barriers), post_copy_barriers);

    ExecuteCommandList();

    ThrowIfFailed(swap_chain->Present(1, 0));

    UINT64 current_fence_value = fence_values[back_buffer_index];
    ThrowIfFailed(command_queue->Signal(fence, current_fence_value));

    back_buffer_index = swap_chain->GetCurrentBackBufferIndex();

    if (fence->GetCompletedValue() < fence_values[back_buffer_index])
    {
      ThrowIfFailed(fence->SetEventOnCompletion(fence_values[back_buffer_index], fence_event.Get()));
      WaitForSingleObjectEx(fence_event.Get(), INFINITE, FALSE);
    }

    fence_values[back_buffer_index] = current_fence_value + 1;
  }

  WaitForGPU();
}

void ReleaseAllVariables()
{
  WaitForGPU();

  RELEASE(fallback_device);
  RELEASE(fallback_command_list);
  RELEASE(fallback_state_object);

  RELEASE(raytracing_global_root_signature);
  RELEASE(raytracing_local_root_signature);
  RELEASE(raytracing_local_root_signature_empty);

  RELEASE(descriptor_heap);

  RELEASE(index_buffer);
  RELEASE(vertex_buffer);

  RELEASE(acceleration_structure);
  RELEASE(bottom_level_acceleration_structure);
  RELEASE(top_level_acceleration_structure);

  RELEASE(raytracing_output);

  RELEASE(miss_shader_table);
  RELEASE(hit_group_shader_table);
  RELEASE(ray_gen_shader_table);

  RELEASE(adapter);

  RELEASE(d3d_device);
  RELEASE(command_queue);
  RELEASE(command_list);

  for (UINT n = 0; n < back_buffer_count; n++)
  {
    RELEASE(command_allocators[n]);
    RELEASE(render_targets[n]);
  }

  RELEASE(dxgi_factory);
  RELEASE(swap_chain);
  RELEASE(depth_stencil);

  RELEASE(fence);
  RELEASE(rtv_descriptor_heap);
  RELEASE(dsv_descriptor_heap);
}

WRAPPED_GPU_POINTER CreateFallbackWrappedPointer(ID3D12Resource* resource, UINT bufferNumElements)
{
  D3D12_UNORDERED_ACCESS_VIEW_DESC rawBufferUavDesc = {};
  rawBufferUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  rawBufferUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
  rawBufferUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
  rawBufferUavDesc.Buffer.NumElements = bufferNumElements;

  D3D12_CPU_DESCRIPTOR_HANDLE bottomLevelDescriptor;

  // Only compute fallback requires a valid descriptor index when creating a wrapped pointer.
  UINT descriptorHeapIndex = 0;
  if (!fallback_device->UsingRaytracingDriver())
  {
    descriptorHeapIndex = AllocateDescriptor(&bottomLevelDescriptor);
    d3d_device->CreateUnorderedAccessView(resource, nullptr, &rawBufferUavDesc, bottomLevelDescriptor);
  }

  return fallback_device->GetWrappedPointerSimple(descriptorHeapIndex, resource->GetGPUVirtualAddress());
}

UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
  auto descriptorHeapCpuBase = descriptor_heap->GetCPUDescriptorHandleForHeapStart();
  if (descriptorIndexToUse >= descriptor_heap->GetDesc().NumDescriptors)
  {
    descriptorIndexToUse = descriptors_allocated++;
  }
  *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, descriptor_size);
  return descriptorIndexToUse;
}

void ExecuteCommandList()
{
  ThrowIfFailed(command_list->Close());
  ID3D12CommandList* command_lists[] = { command_list };
  command_queue->ExecuteCommandLists(ARRAYSIZE(command_lists), command_lists);
}