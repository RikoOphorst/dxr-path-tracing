#pragma once

#include "model.h"

namespace rtrt
{
  class Camera;

  struct AmbientOcclusion
  {
    int num_samples;
    float ao_size;
  };

  struct Shadows
  {
    float shadow_distance;
  };

  struct Lens
  {
    float lens_diameter;
    float focal_length;
  };

  struct AntiAliasing
  {
    enum Algorithm {
      Random,
      Stratified2,
      Stratified4,
      Stratified8,
      Stratified16
    };

    bool enabled;
    Algorithm algorithm;
    DirectX::XMFLOAT2 sample_point;
  };

  struct PostProcessing
  {
    float gamma;
  };

  struct GlobalIllumination
  {
    int num_bounces;
    float bounce_distance;
  };

  class Application
  {
  public:
    Application();
    ~Application();

    void Initialize();

    void Update(GLFWwindow* window, int picking_result);

  public:
    bool freeze_rendering;
    int freeze_at_sample;
    
    UINT frame_count;
    UINT sample_count;
    bool clear_samples;

    int selected_material;
    bool materials_dirty;
    
    float delta_time;
    float previous_timestamp;
    float current_timestamp;

    DirectX::XMFLOAT2 previous_cursor_position;
    DirectX::XMFLOAT2 current_cursor_position;

    Camera* camera;
    AmbientOcclusion ao;
    Shadows shadows;
    Lens lens;
    AntiAliasing aa;
    PostProcessing pp;
    GlobalIllumination gi;
    DirectX::XMFLOAT4 sky_color;
    Model model;
  };
}