#include "adapter_selector.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  void AdapterSelector::SelectAdapter(int flags, IDXGIAdapter1** out_adapter, IDXGIFactory4** out_factory)
  {
    ThrowIfFalse(out_adapter != nullptr);
    ThrowIfFalse(out_factory != nullptr);

    IDXGIAdapter1* adapter = nullptr;
    IDXGIFactory4* factory = nullptr;

    if ((flags & FLAG_ENABLE_DEBUG_LAYER) != 0)
    {
      ID3D12Debug* debug_controller = nullptr;
      if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
      {
        debug_controller->EnableDebugLayer();
      }
      else
      {
        LOG("D3D12 Debug Layer is unavailable.");
      }

      IDXGIInfoQueue* dxgi_info_queue = nullptr;
      if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_info_queue))))
      {
        ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

        if ((flags & FLAG_ENABLE_BREAK_ON_WARNING) != 0)
        {
          dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);
        }

        if ((flags & FLAG_ENABLE_BREAK_ON_ERROR) != 0)
        {
          dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        }

        if ((flags & FLAG_ENABLE_BREAK_ON_CORRUPTION) != 0)
        {
          dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
        }
      }
      else
      {
        LOG("DXGI breaking on messages couldn't be enabled.");
      }

      RELEASE(debug_controller);
      RELEASE(dxgi_info_queue);
    }

    if (factory == nullptr)
    {
      ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
    }

    if ((flags & FLAG_FORCE_WARP) != 0)
    {
      if (SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
      {
        if ((flags & FLAG_MINIMUM_FEATURE_LEVEL_11_0) != 0 ||
            (flags & FLAG_MINIMUM_FEATURE_LEVEL_11_1) != 0 ||
            (flags & FLAG_MINIMUM_FEATURE_LEVEL_12_0) != 0 ||
            (flags & FLAG_MINIMUM_FEATURE_LEVEL_12_1) != 0)
        {
          SelectionFlags supported_level = TestAdapterFeatureLevels(adapter);

          if ((flags & FLAG_MINIMUM_FEATURE_LEVEL_12_1) != 0 && (supported_level & FLAG_MINIMUM_FEATURE_LEVEL_12_1) == 0)
          {
            LOG("Requested WARP adapter with support for 12_1, but WARP adapter does not support 12_1. No adapter selected.");
            RELEASE(adapter);
          }

          if ((flags & FLAG_MINIMUM_FEATURE_LEVEL_12_0) != 0 && (supported_level & FLAG_MINIMUM_FEATURE_LEVEL_12_0) == 0)
          {
            LOG("Requested WARP adapter with support for 12_0, but WARP adapter does not support 12_0. No adapter selected.");
            RELEASE(adapter);
          }

          if ((flags & FLAG_MINIMUM_FEATURE_LEVEL_11_1) != 0 && (supported_level & FLAG_MINIMUM_FEATURE_LEVEL_11_1) == 0)
          {
            LOG("Requested WARP adapter with support for 11_1, but WARP adapter does not support 11_1. No adapter selected.");
            RELEASE(adapter);
          }

          if ((flags & FLAG_MINIMUM_FEATURE_LEVEL_11_0) != 0 && (supported_level & FLAG_MINIMUM_FEATURE_LEVEL_11_0) == 0)
          {
            LOG("Requested WARP adapter with support for 11_0, but WARP adapter does not support 11_0. No adapter selected.");
            RELEASE(adapter);
          }
        }
        else
        {
          // requested warp adapter with no minimum feature level, so any warp adapter is fine
        }
      }
      else
      {
        LOG("Requested WARP adapter, but this system doesn't support WARP adapters. No adapter selected.");
        RELEASE(adapter);
      }
    }
    else
    {
      IDXGIAdapter1* iterated_adapter = nullptr;
      for (UINT current_adapter_id = 0; factory->EnumAdapters1(current_adapter_id, &iterated_adapter) != DXGI_ERROR_NOT_FOUND; ++current_adapter_id)
      {
        DXGI_ADAPTER_DESC1 desc;
        ThrowIfFailed(iterated_adapter->GetDesc1(&desc));

        if (flags & FLAG_PRINT_ALL_AVAILABLE_ADAPTERS)
        {
          PrintAdapter(iterated_adapter);
        }

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
          // Don't select Microsoft's Basic Render Driver adapter. This thing is useless.
          RELEASE(iterated_adapter);
          continue;
        }

        if ((flags & FLAG_FORCE_INTEL_ADAPTER) != 0)
        {
          std::wstring haystack = desc.Description;
          std::wstring needle = L"Intel";

          if (haystack.find(needle) == std::wstring::npos)
          {
            RELEASE(iterated_adapter);
            continue;
          }
        }

        if ((flags & FLAG_FORCE_AMD_ADAPTER) != 0)
        {
          std::wstring haystack = desc.Description;
          std::wstring needle = L"AMD";

          if (haystack.find(needle) == std::wstring::npos)
          {
            RELEASE(iterated_adapter);
            continue;
          }
        }

        if ((flags & FLAG_FORCE_NVIDIA_ADAPTER) != 0)
        {
          std::wstring haystack = desc.Description;
          std::wstring needle = L"NVIDIA";

          if (haystack.find(needle) == std::wstring::npos)
          {
            RELEASE(iterated_adapter);
            continue;
          }
        }

        SelectionFlags supported_level = TestAdapterFeatureLevels(iterated_adapter);

        if ((flags & FLAG_MINIMUM_FEATURE_LEVEL_12_1) != 0 && (supported_level & FLAG_MINIMUM_FEATURE_LEVEL_12_1) == 0)
        {
          RELEASE(iterated_adapter);
          continue;
        }

        if ((flags & FLAG_MINIMUM_FEATURE_LEVEL_12_0) != 0 && (supported_level & FLAG_MINIMUM_FEATURE_LEVEL_12_0) == 0)
        {
          RELEASE(iterated_adapter);
          continue;
        }

        if ((flags & FLAG_MINIMUM_FEATURE_LEVEL_11_1) != 0 && (supported_level & FLAG_MINIMUM_FEATURE_LEVEL_11_1) == 0)
        {
          RELEASE(iterated_adapter);
          continue;
        }

        if ((flags & FLAG_MINIMUM_FEATURE_LEVEL_11_0) != 0 && (supported_level & FLAG_MINIMUM_FEATURE_LEVEL_11_0) == 0)
        {
          RELEASE(iterated_adapter);
          continue;
        }

        if (adapter == nullptr)
        {
          adapter = iterated_adapter;
        }

        if ((flags & FLAG_PRINT_ALL_AVAILABLE_ADAPTERS) == 0)
        {
          break;
        }
      }
    }
  
    if (adapter == nullptr)
    {
      LOG("Failed to select adapter based on selection flags.");
    }
    else
    {
      if ((flags & FLAG_PRINT_SELECTED_ADAPTER) != 0)
      {
        LOG("Successfully selected adapter:\n");
        PrintAdapter(adapter);
      }
      else
      {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        wchar_t message[200];
        swprintf_s(message, 200, L"Successfully selected adapter: %ls\n", desc.Description);
        
        LOGW(message);
      }
    }

    *out_adapter = adapter;
    *out_factory = factory;
  }
  
  //------------------------------------------------------------------------------------------------------
  void AdapterSelector::PrintAdapter(IDXGIAdapter1* adapter)
  {
    DXGI_ADAPTER_DESC1 desc;
    adapter->GetDesc1(&desc);

    wchar_t message[256];

    swprintf_s(message, 256, L"Adapter:                 %ls\n", desc.Description);
    LOGW(message);

    swprintf_s(message, 256, L"Dedicated Video Memory:  %u MiB\n", static_cast<unsigned int>(desc.DedicatedVideoMemory / (1 << 20)));
    LOGW(message);

    swprintf_s(message, 256, L"Dedicated System Memory: %u MiB\n", static_cast<unsigned int>(desc.DedicatedSystemMemory / (1 << 20)));
    LOGW(message);

    swprintf_s(message, 256, L"Shared System Memory:    %u MiB\n\n", static_cast<unsigned int>(desc.SharedSystemMemory / (1 << 20)));
    LOGW(message); 
  }
  
  //------------------------------------------------------------------------------------------------------
  AdapterSelector::SelectionFlags AdapterSelector::TestAdapterFeatureLevels(IDXGIAdapter1* adapter)
  {
    SelectionFlags flags = FLAG_NONE;

    if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
    {
      flags = static_cast<SelectionFlags>(flags | FLAG_MINIMUM_FEATURE_LEVEL_12_1);
    }

    if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
    {
      flags = static_cast<SelectionFlags>(flags | FLAG_MINIMUM_FEATURE_LEVEL_12_0);
    }

    if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_1, _uuidof(ID3D12Device), nullptr)))
    {
      flags = static_cast<SelectionFlags>(flags | FLAG_MINIMUM_FEATURE_LEVEL_11_1);
    }

    if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
    {
      flags = static_cast<SelectionFlags>(flags | FLAG_MINIMUM_FEATURE_LEVEL_11_0);
    }

    return flags;
  }
}