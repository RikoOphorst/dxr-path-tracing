#pragma once

namespace rtrt
{
  class AdapterSelector
  {
  public:
    enum SelectionFlags
    {
      FLAG_NONE                         = 0 << 0,                                             // No flags
      FLAG_FORCE_WARP                   = 1 << 0,                                             // Selected adapter must be WARP adapter or nullptr
      FLAG_FORCE_INTEL_ADAPTER          = 1 << 1,                                             // Selected adapter must be Intel adapter or nullptr
      FLAG_FORCE_NVIDIA_ADAPTER         = 1 << 2,                                             // Selected adapter must be NVIDIA adapter or nullptr
      FLAG_FORCE_AMD_ADAPTER            = 1 << 3,                                             // Selected adapter must be AMD adapter or nullptr
    //FLAG_FORCE_DEDICATED_ADAPTER      = FLAG_FORCE_NVIDIA_ADAPTER | FLAG_FORCE_AMD_ADAPTER, // Selected adapter must be NVIDIA or AMD adapter or nullptr
      FLAG_ENABLE_DEBUG_LAYER           = 1 << 4,                                             // Debug layer will be enabled for this process
      FLAG_ENABLE_BREAK_ON_WARNING      = 1 << 5,                                             // (ENABLE_DEBUG_LAYER REQUIRED) Debug layer will break on warnings
      FLAG_ENABLE_BREAK_ON_ERROR        = 1 << 6,                                             // (ENABLE_DEBUG_LAYER REQUIRED) Debug layer will break on errors
      FLAG_ENABLE_BREAK_ON_CORRUPTION   = 1 << 7,                                             // (ENABLE_DEBUG_LAYER REQUIRED) Debug layer will break on corruptions
      FLAG_PRINT_SELECTED_ADAPTER       = 1 << 8,                                             // Pretty-print info about selected adapter if available
      FLAG_PRINT_ALL_AVAILABLE_ADAPTERS = 1 << 9,                                             // Pretty-print & enumerate all available adapters in the system
      FLAG_MINIMUM_FEATURE_LEVEL_11_0   = 1 << 10,                                            // Adapter must support feature level 11_0
      FLAG_MINIMUM_FEATURE_LEVEL_11_1   = 1 << 11,                                            // Adapter must support feature level 11_1
      FLAG_MINIMUM_FEATURE_LEVEL_12_0   = 1 << 12,                                            // Adapter must support feature level 12_0
      FLAG_MINIMUM_FEATURE_LEVEL_12_1   = 1 << 13                                             // Adapter must support feature level 12_1
    };

    static void SelectAdapter(int flags, IDXGIAdapter1** out_adapter, IDXGIFactory4** out_factory);

    static void PrintAdapter(IDXGIAdapter1* adapter);

  private:
    static SelectionFlags TestAdapterFeatureLevels(IDXGIAdapter1* adapter);
  };
}