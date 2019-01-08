#include "imgui_layer.h"

#include "device.h"
#include "upload_buffer.h"
#include "root_signatures.h"

namespace rtrt
{
  std::queue<UINT> ImGuiLayer::input_characters_ = std::queue<UINT>();
  float ImGuiLayer::mouse_scroll_ = 0.0f;

  //------------------------------------------------------------------------------------------------------
  ImGuiLayer::ImGuiLayer() :
    initialized_(false),
    vertex_shader_blob_(nullptr),
    pixel_shader_blob_(nullptr),
    window_(nullptr),
    device_(nullptr),
    pipeline_state_(nullptr),
    root_signature_(nullptr),
    texture_buffer_(nullptr),
    rtv_heap_(nullptr),
    srv_heap_(nullptr)
  {

  }

  //------------------------------------------------------------------------------------------------------
  ImGuiLayer::~ImGuiLayer()
  {
    Shutdown();
  }

  //------------------------------------------------------------------------------------------------------
  void ImGuiLayer::Init(GLFWwindow* window, Device* device, DescriptorHeap* rtv_heap, DescriptorHeap* srv_heap)
  {
    device_ = device;
    window_ = window;
    rtv_heap_ = rtv_heap;
    srv_heap_ = srv_heap;

    ImGuiIO& io = ImGui::GetIO();
    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

    io.RenderDrawListsFn = ImGuiRenderDrawLists;
    io.ImeWindowHandle = glfwGetWin32Window(window);
    io.UserData = this;

    QueryPerformanceFrequency((LARGE_INTEGER*)&ticks_per_second_);
    QueryPerformanceCounter((LARGE_INTEGER*)&time_);

    glfwSetCharCallback(window, GlfwCharCallback);
    glfwSetScrollCallback(window, GlfwScrollCallback);

    CreateDeviceObjects();
  }

  //------------------------------------------------------------------------------------------------------
  void ImGuiLayer::NewFrame()
  {
    if (!initialized_)
    {
      CreateDeviceObjects();
    }

    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2(1280.0f, 720.0f);

    INT64 current_time;
    QueryPerformanceCounter((LARGE_INTEGER *)&current_time);
    io.DeltaTime = (float)(current_time - time_) / ticks_per_second_;
    time_ = current_time;

    io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    io.KeySuper = false;

    for (int i = 0; i < 512; i++)
    {
      io.KeysDown[i] = glfwGetKey(window_, i) == GLFW_PRESS;
    }

    for (int i = 0; i < 3; i++)
    {
      io.MouseDown[i] = glfwGetMouseButton(window_, i) == GLFW_PRESS;
    }

    while (!input_characters_.empty())
    {
      unsigned int current = input_characters_.front();

      if (current > 0 && current < 0x10000)
      {
        io.AddInputCharacter(static_cast<ImWchar>(input_characters_.front()));
      }

      input_characters_.pop();
    }

    double x, y;
    glfwGetCursorPos(window_, &x, &y);
    io.MousePos = ImVec2(static_cast<float>(x), static_cast<float>(y));
    io.MouseWheel = mouse_scroll_;
    mouse_scroll_ = 0.0f;

    ImGui::NewFrame();
  }

  //------------------------------------------------------------------------------------------------------
  void ImGuiLayer::Render(DescriptorHandle render_target)
  {
    render_target_ = render_target;

    ImGui::Render();
  }

  //------------------------------------------------------------------------------------------------------
  void ImGuiLayer::Shutdown()
  {
    InvalidateDeviceObjects();
    ImGui::Shutdown();
  }

  //------------------------------------------------------------------------------------------------------
  void ImGuiLayer::ImGuiRenderDrawLists(ImDrawData* draw_data)
  {
    ImGuiLayer* layer = reinterpret_cast<ImGuiLayer*>(ImGui::GetIO().UserData);

    BYTE* mapped_data = reinterpret_cast<BYTE*>(layer->upload_buffer_->GetData());

    BYTE* write_cursor = mapped_data;

    // Copy the projection matrix at the beginning of the buffer
    {
      float translate = -0.5f * 2.f;
      const float L = 0.f;
      const float R = ImGui::GetIO().DisplaySize.x;
      const float B = ImGui::GetIO().DisplaySize.y;
      const float T = 0.f;
      const float mvp[4][4] =
      {
          { 2.0f / (R - L),       0.0f,                   0.0f,       0.0f },
          { 0.0f,                 2.0f / (T - B),         0.0f,       0.0f },
          { 0.0f,                 0.0f,                   0.5f,       0.0f },
          { (R + L) / (L - R),    (T + B) / (B - T),      0.5f,       1.0f },
      };

      memcpy(write_cursor, &mvp[0], sizeof(mvp));
      write_cursor += sizeof(mvp);
    }

    // Copy the vertices and indices for each command list
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
      const ImDrawList* cmd_list = draw_data->CmdLists[n];
      size_t vertices_count = cmd_list->VtxBuffer.size();
      size_t indices_count = cmd_list->IdxBuffer.size();
      size_t vertices_size = vertices_count * sizeof(ImDrawVert);
      size_t indices_size = indices_count * sizeof(ImDrawIdx);

      // Copy the vertex data
      memcpy(write_cursor, &cmd_list->VtxBuffer[0], vertices_size);
      write_cursor += vertices_size;

      // Copy the index data
      memcpy(write_cursor, &cmd_list->IdxBuffer[0], indices_size);
      write_cursor += indices_size;
    }

    D3D12_VIEWPORT viewport;
    viewport.Width = ImGui::GetIO().DisplaySize.x;
    viewport.Height = ImGui::GetIO().DisplaySize.y;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;

    layer->device_->command_list->SetPipelineState(layer->pipeline_state_);
    layer->device_->command_list->SetGraphicsRootSignature(layer->root_signature_);
    layer->device_->command_list->RSSetViewports(1, &viewport);
    layer->device_->command_list->OMSetRenderTargets(1, &layer->render_target_.cpu_handle(), FALSE, nullptr);
    layer->device_->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    layer->device_->command_list->SetGraphicsRootConstantBufferView(1, layer->upload_buffer_->GetBuffer()->GetGPUVirtualAddress());
    layer->device_->command_list->SetGraphicsRootDescriptorTable(0, layer->texture_handle_);

    D3D12_GPU_VIRTUAL_ADDRESS buffer_address = layer->upload_buffer_->GetBuffer()->GetGPUVirtualAddress();
    int64_t read_cursor = 64; // start at 64 because the constant buffer is 64 bytes in size - one mat44

    for (int i = 0; i < draw_data->CmdListsCount; i++)
    {
      // Render command lists
      int vtx_offset = 0;
      int idx_offset = 0;

      const ImDrawList* cmd_list = draw_data->CmdLists[i];
      size_t vertices_count = cmd_list->VtxBuffer.size();
      size_t indices_count = cmd_list->IdxBuffer.size();
      size_t vertices_size = vertices_count * sizeof(ImDrawVert);
      size_t indices_size = indices_count * sizeof(ImDrawIdx);

      D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
      vertex_buffer_view.BufferLocation = buffer_address + read_cursor;
      vertex_buffer_view.StrideInBytes = sizeof(ImDrawVert);
      vertex_buffer_view.SizeInBytes = static_cast<UINT>(vertices_size);
      read_cursor += static_cast<int64_t>(vertices_size);

      D3D12_INDEX_BUFFER_VIEW index_buffer_view;
      index_buffer_view.BufferLocation = buffer_address + read_cursor;
      index_buffer_view.SizeInBytes = static_cast<UINT>(indices_size);
      index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
      read_cursor += static_cast<int64_t>(indices_size);

      layer->device_->command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
      layer->device_->command_list->IASetIndexBuffer(&index_buffer_view);

      for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
      {
        const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
        if (pcmd->UserCallback)
        {
          pcmd->UserCallback(cmd_list, pcmd);
        }
        else
        {
          const D3D12_RECT scissor_rect = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };

          layer->device_->command_list->RSSetScissorRects(1, &scissor_rect);
          layer->device_->command_list->DrawIndexedInstanced(pcmd->ElemCount, 1, idx_offset, vtx_offset, 0);
        }

        idx_offset += pcmd->ElemCount;
      }

      vtx_offset += static_cast<int>(vertices_count);
    }
  }

  //------------------------------------------------------------------------------------------------------
  void ImGuiLayer::GlfwCharCallback(GLFWwindow* window, UINT unicode_character)
  {
    ImGuiLayer::input_characters_.push(unicode_character);
  }

  //------------------------------------------------------------------------------------------------------
  void ImGuiLayer::GlfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
  {
    ImGuiLayer::mouse_scroll_ += static_cast<float>(yoffset);
  }

  //------------------------------------------------------------------------------------------------------
  void ImGuiLayer::CreateDeviceObjects()
  {
    // Create the Vertex Shader
    {
      static const char* vertex_shader =
        "cbuffer vertexBuffer : register(b0) \
                {\
                    float4x4 ProjectionMatrix; \
                };\
                \
                struct VS_INPUT\
                {\
                    float2 pos : POSITION;\
                    float4 col : COLOR0;\
                    float2 uv  : TEXCOORD0;\
                };\
                \
                struct PS_INPUT\
                {\
                    float4 pos : SV_POSITION;\
                    float4 col : COLOR0;\
                    float2 uv  : TEXCOORD0;\
                };\
                \
                PS_INPUT main(VS_INPUT input)\
                {\
                    PS_INPUT output;\
                    output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
                    output.col = input.col;\
                    output.uv  = input.uv;\
                    return output;\
                }";

      D3DCompile(vertex_shader, strlen(vertex_shader), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, &vertex_shader_blob_, NULL);
      ThrowIfFalse(vertex_shader_blob_ != nullptr);

      vertex_shader_.pShaderBytecode = vertex_shader_blob_->GetBufferPointer();
      vertex_shader_.BytecodeLength = vertex_shader_blob_->GetBufferSize();
    }

    // Create the Pixel Shader
    {
      static const char* pixel_shader =
        "struct PS_INPUT\
                {\
                    float4 pos : SV_POSITION;\
                    float4 col : COLOR0;\
                    float2 uv  : TEXCOORD0;\
                };\
                \
                SamplerState sampler0 : register(s0);\
                Texture2D texture0 : register(t0);\
                \
                float4 main(PS_INPUT input) : SV_Target\
                {\
                    float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
                    return out_col; \
                }";

      D3DCompile(pixel_shader, strlen(pixel_shader), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &pixel_shader_blob_, NULL);
      ThrowIfFalse(pixel_shader_blob_ != nullptr);

      pixel_shader_.pShaderBytecode = pixel_shader_blob_->GetBufferPointer();
      pixel_shader_.BytecodeLength = pixel_shader_blob_->GetBufferSize();
    }

    D3D12_INPUT_ELEMENT_DESC input_element_descs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (size_t)(&((ImDrawVert*)0)->pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, (size_t)(&((ImDrawVert*)0)->uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, (size_t)(&((ImDrawVert*)0)->col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_RASTERIZER_DESC rasterizer_desc;
    rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer_desc.FrontCounterClockwise = TRUE;
    rasterizer_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer_desc.DepthClipEnable = TRUE;
    rasterizer_desc.MultisampleEnable = FALSE;
    rasterizer_desc.AntialiasedLineEnable = TRUE;
    rasterizer_desc.ForcedSampleCount = 1;
    rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blend_desc;
    blend_desc.AlphaToCoverageEnable = FALSE;
    blend_desc.IndependentBlendEnable = FALSE;
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].LogicOpEnable = FALSE;
    blend_desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC depth_stencil_desc;
    depth_stencil_desc.DepthEnable = FALSE;
    depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depth_stencil_desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depth_stencil_desc.StencilEnable = FALSE;
    depth_stencil_desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depth_stencil_desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    depth_stencil_desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depth_stencil_desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depth_stencil_desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depth_stencil_desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    depth_stencil_desc.BackFace = depth_stencil_desc.FrontFace;

    D3D12_STATIC_SAMPLER_DESC sampler_desc;
    sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc.MipLODBias = 0.0f;
    sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler_desc.MinLOD = 0.0f;
    sampler_desc.MaxLOD = 0.0f;
    sampler_desc.MaxAnisotropy = 8;
    sampler_desc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler_desc.RegisterSpace = 0;
    sampler_desc.ShaderRegister = 0;
    sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    
    CD3DX12_DESCRIPTOR_RANGE range;
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    
    CD3DX12_ROOT_PARAMETER root_parameters[2];
    root_parameters[0].InitAsDescriptorTable(1, &range);
    root_parameters[1].InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc(ARRAYSIZE(root_parameters), root_parameters, 1, &sampler_desc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    root_signature_ = RootSignatureFactory::BuildRootSignature(device_->device, &root_signature_desc);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.VS = vertex_shader_;
    pso_desc.PS = pixel_shader_;
    pso_desc.BlendState = blend_desc;
    pso_desc.DepthStencilState = depth_stencil_desc;
    pso_desc.RasterizerState = rasterizer_desc;
    pso_desc.InputLayout.NumElements = 3;
    pso_desc.InputLayout.pInputElementDescs = input_element_descs;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.SampleDesc.Count = 1;
    pso_desc.SampleDesc.Quality = 0;
    pso_desc.pRootSignature = root_signature_;
    pso_desc.NumRenderTargets = 1;
    pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    ThrowIfFailed(device_->device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state_)));

    unsigned char* pixels = 0;
    int width, height;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    device_->device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<UINT>(width), static_cast<UINT>(height)),
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&texture_buffer_)
    );

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.PlaneSlice = 0;
    srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
    srv_heap_->CreateDescriptor(device_->device, texture_buffer_, &srv_desc, &texture_handle_);

    D3D12_SUBRESOURCE_DATA resource_data = {};
    resource_data.pData = pixels;
    resource_data.RowPitch = width * 4;
    resource_data.SlicePitch = width * 4 * height;
    
    {
      UINT64 texture_upload_buffer_size;
      device_->device->GetCopyableFootprints(&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<UINT>(width), static_cast<UINT>(height)), 0, 1, 0, nullptr, nullptr, nullptr, &texture_upload_buffer_size);

      ID3D12Resource* upload_buffer = nullptr;

      ThrowIfFailed(
        device_->device->CreateCommittedResource(
          &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
          D3D12_HEAP_FLAG_NONE,
          &CD3DX12_RESOURCE_DESC::Buffer(texture_upload_buffer_size),
          D3D12_RESOURCE_STATE_GENERIC_READ,
          nullptr,
          IID_PPV_ARGS(&upload_buffer)
        )
      );

      ID3D12CommandAllocator* allocator = nullptr;
      ThrowIfFailed(device_->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));

      ID3D12GraphicsCommandList* list = nullptr;
      device_->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&list));

      UpdateSubresources(list, texture_buffer_, upload_buffer, 0, 0, 1, &resource_data);

      list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture_buffer_, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

      list->Close();
      ID3D12CommandList* lists[] = { list };
      device_->command_queue->ExecuteCommandLists(1, lists);

      ID3D12Fence* fence = nullptr;
      device_->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
      Microsoft::WRL::Wrappers::Event fence_event;

      if (SUCCEEDED(device_->command_queue->Signal(fence, 1)))
      {
        if (SUCCEEDED(fence->SetEventOnCompletion(1, fence_event.Get())))
        {
          WaitForSingleObjectEx(fence_event.Get(), INFINITE, FALSE);
        }
      }

      RELEASE(upload_buffer);
      RELEASE(list);
      RELEASE(allocator);
      RELEASE(fence);
    }

    io.Fonts->TexID = texture_buffer_;
    upload_buffer_ = new UploadBuffer();
    upload_buffer_->Create(device_->device, 1024 * 1024 * 8 * 16, nullptr);

    initialized_ = true;
  }

  //------------------------------------------------------------------------------------------------------
  void ImGuiLayer::InvalidateDeviceObjects()
  {
    RELEASE(vertex_shader_blob_);
    RELEASE(pixel_shader_blob_);
    RELEASE(pipeline_state_);
    RELEASE(root_signature_);
    RELEASE(texture_buffer_);
    DELETE(upload_buffer_);
  }
}