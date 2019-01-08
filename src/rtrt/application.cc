#include "application.h"

#include "camera.h"
#include "imgui_layer.h"

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  Application::Application() :
    camera(nullptr)
  {

  }

  //------------------------------------------------------------------------------------------------------
  Application::~Application()
  {
    DELETE(camera);
  }

  //------------------------------------------------------------------------------------------------------
  void Application::Initialize()
  {
    current_cursor_position = DirectX::XMFLOAT2(0.0f, 0.0f);
    previous_cursor_position = DirectX::XMFLOAT2(0.0f, 0.0f);

    freeze_rendering = false;
    freeze_at_sample = -1;
    
    frame_count = 0;
    sample_count = 0;
    clear_samples = false;

    selected_material = -1;
    materials_dirty = false;

    delta_time = 0.0f;
    previous_timestamp = 0.0f;
    current_timestamp = 0.0f;

    camera = new Camera();
    camera->SetNearPlane(100.0f);
    camera->SetFarPlane(1000.0f);
    camera->SetAperture(0.0f);
    camera->SetFovDegrees(70.0f);
    camera->SetPosition(DirectX::XMFLOAT3(0.0f, 0.75f, 2.0f));
    camera->SetRotation(DirectX::XMFLOAT3(0.0f, DirectX::XMConvertToRadians(180.0f), 0.0f));

    ao.ao_size = 50.0f;
    ao.num_samples = 2;
    
    shadows.shadow_distance = 10000.0f;

    lens.focal_length = 1.0f;
    lens.lens_diameter = 0.0f;

    aa.enabled = true;
    aa.algorithm = AntiAliasing::Algorithm::Random;
    aa.sample_point = DirectX::XMFLOAT2(0.0f, 0.0f);

    pp.gamma = 2.2f;

    gi.bounce_distance = 10000.0f;
    gi.num_bounces = 2;

    sky_color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    model.LoadFromFile("./models/Sponza/glTF/Sponza.gltf");
    //model.LoadFromFile("./models/CornellBox/CornellBox-Sphere.obj");
  }

  //------------------------------------------------------------------------------------------------------
  void Application::Update(GLFWwindow* window, int picking_result)
  {
    clear_samples = false;
    materials_dirty = false;

    current_timestamp = static_cast<float>(glfwGetTime());
    delta_time = current_timestamp - previous_timestamp;
    previous_timestamp = current_timestamp;

    if (!freeze_rendering)
    {
      if (!ImGui::GetIO().WantCaptureKeyboard)
      {
        float speed = 1.0f * delta_time;

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
          clear_samples = true;
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        {
          speed *= 5.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS)
        {
          speed *= 1000.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
          camera->Translate(DirectX::XMFLOAT3(0.0f, 0.0f, speed));
          clear_samples = true;
        }

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
          camera->Translate(DirectX::XMFLOAT3(0.0f, 0.0f, -speed));
          clear_samples = true;
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
          camera->Translate(DirectX::XMFLOAT3(-speed, 0.0f, 0.0f));
          clear_samples = true;
        }

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
          camera->Translate(DirectX::XMFLOAT3(speed, 0.0f, 0.0f));
          clear_samples = true;
        }
      }

      double x, y;
      glfwGetCursorPos(window, &x, &y);
      current_cursor_position = { static_cast<float>(x), static_cast<float>(y) };

      if (!ImGui::GetIO().WantCaptureMouse)
      {
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
        {
          camera->Rotate(
            DirectX::XMFLOAT3(
              DirectX::XMConvertToRadians(current_cursor_position.y - previous_cursor_position.y) * 0.2f,
              DirectX::XMConvertToRadians(current_cursor_position.x - previous_cursor_position.x) * 0.2f,
              0
            )
          );

          clear_samples = true;
        }

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
          selected_material = picking_result;
        }
      }

      if (sample_count == freeze_at_sample)
      {
        freeze_rendering = true;
      }
    }

    previous_cursor_position = current_cursor_position;
    sample_count = freeze_rendering ? sample_count : sample_count + 1;
    frame_count = frame_count + 1;

    ImGui::SetNextWindowSize(ImVec2(400.0f, 720.0f), ImGuiSetCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiSetCond_Always);
    ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    // Stats
    {
      ImGui::BeginChild("Stats", ImVec2(380, 120), true);
      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.0f, 1.0f), "Lens");

      ImGui::LabelText("Frame time", "%.2f ms", delta_time * 1000);
      ImGui::LabelText("Sample count", "%d", sample_count);
      ImGui::InputInt("Freeze at sample #", &freeze_at_sample, 1, 100);
      ImGui::Checkbox("Freeze rendering", &freeze_rendering);

      ImGui::EndChild();
    }

    // Lens
    {
      ImGui::BeginChild("Lens", ImVec2(380, 80), true);

      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.0f, 1.0f), "Lens");

      clear_samples = ImGui::InputFloat("Lens Diameter", &lens.lens_diameter, 0.1f, 5.0f, 1) ? true : clear_samples;
      lens.lens_diameter = std::max(lens.lens_diameter, 0.0f);

      clear_samples = ImGui::InputFloat("Focal Length", &lens.focal_length, 0.25f, 50.0f, 2) ? true : clear_samples;
      lens.focal_length = std::max(lens.focal_length, 0.01f);
      camera->SetNearPlane(lens.focal_length);

      ImGui::EndChild();
    }

    // Global illumination
    {
      ImGui::BeginChild("Global Illumination", ImVec2(380, 80), true);
      
      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.0f, 1.0f), "Global Illumination");

      clear_samples = ImGui::InputInt("GI Bounces", &gi.num_bounces, 1, 1) ? true : clear_samples;
      gi.num_bounces = std::max(std::min(gi.num_bounces, 15), 0);

      clear_samples = ImGui::InputFloat("Bounce Distance", &gi.bounce_distance, 0.1f, 50.0f, 2) ? true : clear_samples;
      gi.bounce_distance = std::max(gi.bounce_distance, 0.01f);

      ImGui::EndChild();
    }

    // Anti aliasing
    {
      ImGui::BeginChild("Anti Aliasing", ImVec2(380, 80), true);
      
      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.0f, 1.0f), "Anti Aliasing");

      clear_samples = ImGui::Checkbox("Anti Aliasing", &aa.enabled) ? true : clear_samples;

      const char* items[] = { "Random", "Stratified 2x", "Stratified 4x", "Stratified 8x", "Stratified 16x" };
      clear_samples = ImGui::Combo("AA Algorithm", reinterpret_cast<int*>(&aa.algorithm), items, 5) ? true : clear_samples;

      ImGui::EndChild();
    }

    // Post processing
    {
      ImGui::BeginChild("Post Processing", ImVec2(380, 55), true);

      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.0f, 1.0f), "Post Processing");

      ImGui::InputFloat("Gamma Correction", &pp.gamma, 0.1f, 1.0f, 1);
      pp.gamma = std::max(pp.gamma, 0.1f);

      ImGui::EndChild();
    }

    // Sky
    {
      ImGui::BeginChild("Sky", ImVec2(380, 55), true);

      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.0f, 1.0f), "Sky");

      clear_samples = ImGui::ColorEdit4("Sky Color", &sky_color.x) ? true : clear_samples;

      ImGui::EndChild();
    }

    // Material editing
    {
      ImGui::BeginChild("Material", ImVec2(380, 125), true);

      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.0f, 1.0f), "Material Editing");

      if (selected_material == -1)
      {
        ImGui::Text("No material selected.");
      }
      else
      {
        int shading_model = static_cast<int>(model.materials[selected_material].shading_model);
        materials_dirty = ImGui::InputInt("Shading Model", &shading_model) ? true : materials_dirty;
        model.materials[selected_material].shading_model = std::max(shading_model, 0);

        materials_dirty = ImGui::ColorEdit4("Emissive", &model.materials[selected_material].color_emissive.x) ? true : materials_dirty;
        materials_dirty = ImGui::ColorEdit4("Diffuse", &model.materials[selected_material].color_diffuse.x) ? true : materials_dirty;
        materials_dirty = ImGui::InputFloat("Index of Refraction", &model.materials[selected_material].index_of_refraction, 0.025f, 0.1f, 3) ? true : materials_dirty;
      }

      ImGui::EndChild();
    }

    ImGui::End();

    if (materials_dirty)
    {
      clear_samples = true;
    }

    if (clear_samples)
    {
      sample_count = 0;
    }

    DirectX::XMFLOAT2 sampling_points[31] = {
      // Random
      { (static_cast<float>(rand() % 100) / 100.0f) - 0.5f, (static_cast<float>(rand() % 100) / 100.0f) - 0.5f },
      // Strat2x
      { 0.25f, 0.25f },
      { -0.25f, -0.25f },
      // Strat4x
      { -0.125f,  -0.375f },
      {  0.375f,  -0.125f },
      { -0.375f,   0.125f },
      {  0.125f,   0.375f },
      // Strat8x
      {  0.0625f, -0.1875f },
      { -0.0625f,  0.1875f },
      {  0.3125f,  0.0625f },
      { -0.1875f, -0.3125f },
      { -0.3125f,  0.3125f },
      { -0.4375f, -0.0625f },
      {  0.1875f,  0.4375f },
      {  0.4375f, -0.4375f },
      // Strat16x
      {  0.0625f,  0.0625f },
      { -0.0625f, -0.1875f },
      { -0.1875f,  0.125f  },
      {  0.25f,   -0.0625f },
      { -0.3125f, -0.125f  },
      {  0.125f,   0.3125f },
      {  0.3125f,  0.1875f },
      {  0.1875f, -0.3125f },
      { -0.125f,   0.375f  },
      {  0.0f,    -0.4375f },
      { -0.25f,   -0.375f  },
      { -0.375f,   0.25f   },
      { -0.5f,     0.0f    },
      {  0.4375f, -0.25f   },
      {  0.375f,   0.4375f },
      { -0.4375f, -0.5f    }
    };

    for (int i = 1; i < 31; i++)
    {
      sampling_points[i].x = (sampling_points[i].x + 8.0f / 16.0f) - 0.5f;
      sampling_points[i].y = (sampling_points[i].y + 8.0f / 16.0f) - 0.5f;
    }

    int num_samples_in_algo;

    switch (aa.algorithm)
    {
    case AntiAliasing::Random:
      num_samples_in_algo = 1;
      aa.sample_point = sampling_points[0];
      break;
    case AntiAliasing::Stratified2:
      aa.sample_point = sampling_points[1 + sample_count % 2];
      break;
    case AntiAliasing::Stratified4:
      aa.sample_point = sampling_points[3 + sample_count % 4];
      break;
    case AntiAliasing::Stratified8:
      aa.sample_point = sampling_points[7 + sample_count % 8];
      break;
    case AntiAliasing::Stratified16:
      aa.sample_point = sampling_points[15 + sample_count % 16];
      break;
    }
  }
}
