# Source Files
file(GLOB SrcFiles "*.h" "*.cc")
add_msvc_precompiled_header("pch.pch" "pch.cpp" SrcFiles)
source_group("src" FILES ${SrcFiles})

file(GLOB SharedFiles "shared/*.h" "shared/*.cc")
source_group("src\\shared" FILES ${SharedFiles})

file(GLOB ImguiFiles "imgui/*.h" "imgui/*.cc")
source_group("src\\imgui" FILES ${ImguiFiles})

add_executable(rtrt 
  ${SrcFiles}
  ${SharedFiles}
  ${ImguiFiles}
)

set_property(TARGET rtrt PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

target_link_libraries(rtrt PRIVATE 
  assimp
  d3d12raytracingfallback
# d3dx12                  # at the moment, d3dx12 is added as part of the d3d12raytracingfallback's include directories
  directxtk12
  glfw
  stb
  winpixeventruntime
  optix
  "d3d12.lib"
  "dxgi.lib"
  "dxguid.lib"
  "d3dcompiler.lib"
)

add_custom_command(
  TARGET rtrt POST_BUILD 
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${ASSIMP_DLLS}
  $<TARGET_FILE_DIR:rtrt>
)

add_custom_command(
  TARGET rtrt POST_BUILD 
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${WINPIXEVENTRUNTIME_DLLS}
  $<TARGET_FILE_DIR:rtrt>
)

add_custom_command(
  TARGET rtrt POST_BUILD 
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${OPTIX_DLLS}
  $<TARGET_FILE_DIR:rtrt>
)

add_custom_command(
  TARGET rtrt POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${DXR_FALLBACK_TOOLS}
  $<TARGET_FILE_DIR:rtrt>
)

# Directory for compiled shaders
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/rtrt/compiled-shaders/compiled-shaders")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/rtrt/compiled-shaders/compiled-shaders/rt")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/rtrt/compiled-shaders/compiled-shaders/cs")
target_include_directories(rtrt PRIVATE "${CMAKE_BINARY_DIR}/rtrt/compiled-shaders/")
set (GeneratedRTShadersDirectory "${CMAKE_BINARY_DIR}/rtrt/compiled-shaders/compiled-shaders/rt/")
set (GeneratedCSShadersDirectory "${CMAKE_BINARY_DIR}/rtrt/compiled-shaders/compiled-shaders/cs/")

string(REPLACE "/" "\\" CMAKE_SOURCE_DIR_REPLACED ${CMAKE_SOURCE_DIR})

file (GLOB RTShaders "shaders/*.rt.hlsl")
source_group("src\\shaders\\rt" FILES ${RTShaders})
set_source_files_properties(${RTShaders} PROPERTIES VS_SHADER_TYPE "Library")
set_source_files_properties(${RTShaders} PROPERTIES VS_SHADER_MODEL "6.3")
set_source_files_properties(${RTShaders} PROPERTIES VS_SHADER_ENTRYPOINT " ")
set_source_files_properties(${RTShaders} PROPERTIES VS_SHADER_FLAGS "-I\"${CMAKE_SOURCE_DIR_REPLACED}\\src\\rtrt\\shared\\\" -Zpr")
set_source_files_properties(${RTShaders} PROPERTIES VS_SHADER_OBJECT_FILE_NAME " ")

foreach(CurrentShader ${RTShaders})
  get_filename_component(CurrentShaderName "${CurrentShader}" NAME_WE)
  set_source_files_properties(${CurrentShader} PROPERTIES VS_SHADER_OUTPUT_HEADER_FILE "${GeneratedRTShadersDirectory}${CurrentShaderName}.cso.h")
  set_source_files_properties(${CurrentShader} PROPERTIES VS_SHADER_VARIABLE_NAME "cso_${CurrentShaderName}")
  target_sources(rtrt PUBLIC ${CurrentShader})

  set (GeneratedShader "${GeneratedRTShadersDirectory}${CurrentShaderName}.cso.h")
  source_group("src\\shaders\\compiled" FILES ${GeneratedShader})
  set_source_files_properties(${GeneratedShader} PROPERTIES GENERATED TRUE)
  target_sources(rtrt PUBLIC ${GeneratedShader})
endforeach(CurrentShader ${RTShaders})

file (GLOB CSShaders "shaders/*.cs.hlsl")
source_group("src\\shaders\\cs" FILES ${CSShaders})
set_source_files_properties(${CSShaders} PROPERTIES VS_SHADER_TYPE "Compute")
set_source_files_properties(${CSShaders} PROPERTIES VS_SHADER_MODEL "5.1")
set_source_files_properties(${CSShaders} PROPERTIES VS_SHADER_ENTRYPOINT "main")
set_source_files_properties(${CSShaders} PROPERTIES VS_SHADER_FLAGS "-I\"${CMAKE_SOURCE_DIR_REPLACED}\\src\\rtrt\\shared\\\" -Zpr")
set_source_files_properties(${CSShaders} PROPERTIES VS_SHADER_OBJECT_FILE_NAME " ")

foreach(CurrentShader ${CSShaders})
  get_filename_component(CurrentShaderName "${CurrentShader}" NAME_WE)
  set_source_files_properties(${CurrentShader} PROPERTIES VS_SHADER_OUTPUT_HEADER_FILE "${GeneratedCSShadersDirectory}${CurrentShaderName}.cso.h")
  set_source_files_properties(${CurrentShader} PROPERTIES VS_SHADER_VARIABLE_NAME "cso_${CurrentShaderName}")
  target_sources(rtrt PUBLIC ${CurrentShader})

  set (GeneratedShader "${GeneratedCSShadersDirectory}${CurrentShaderName}.cso.h")
  source_group("src\\shaders\\compiled" FILES ${GeneratedShader})
  set_source_files_properties(${GeneratedShader} PROPERTIES GENERATED TRUE)
  target_sources(rtrt PUBLIC ${GeneratedShader})
endforeach(CurrentShader ${CSShaders})

file (GLOB ShaderHeaders "shaders/*.hlsli")
source_group("src\\shaders" FILES ${ShaderHeaders})
set_source_files_properties(${ShaderHeaders} PROPERTIES VS_TOOL_OVERRIDE "None")
target_sources(rtrt PUBLIC ${ShaderHeaders})