# Source Files
file(GLOB SrcFiles "*.h" "*.cc")
source_group("src" FILES ${SrcFiles})

add_executable(hello-triangle 
  ${SrcFiles}
)

target_link_libraries(hello-triangle PRIVATE 
  assimp
  d3d12raytracingfallback
# d3dx12                  # at the moment, d3dx12 is added as part of the d3d12raytracingfallback's include directories
  glfw
  stb
  winpixeventruntime
  "d3d12.lib"
  "dxgi.lib"
  "dxguid.lib"
)

add_custom_command(
  TARGET hello-triangle POST_BUILD 
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${ASSIMP_DLLS}
  $<TARGET_FILE_DIR:hello-triangle>
)

add_custom_command(
  TARGET hello-triangle POST_BUILD 
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${WINPIXEVENTRUNTIME_DLLS}
  $<TARGET_FILE_DIR:hello-triangle>
)

add_custom_command(
  TARGET hello-triangle POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${DXR_FALLBACK_TOOLS}
  $<TARGET_FILE_DIR:hello-triangle>
)

# Directory for compiled shaders
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/hello-triangle/compiled-shaders/compiled-shaders")
target_include_directories(hello-triangle PRIVATE "${CMAKE_BINARY_DIR}/hello-triangle/compiled-shaders/")
set (GeneratedShadersDirectory "${CMAKE_BINARY_DIR}/hello-triangle/compiled-shaders/compiled-shaders/")

file (GLOB Shaders "shaders/*.hlsl")
source_group("src\\shaders" FILES ${Shaders})
set_source_files_properties(${Shaders} PROPERTIES VS_SHADER_TYPE "Library")
set_source_files_properties(${Shaders} PROPERTIES VS_SHADER_MODEL "6.3")
set_source_files_properties(${Shaders} PROPERTIES VS_SHADER_ENTRYPOINT " ")
set_source_files_properties(${Shaders} PROPERTIES VS_SHADER_FLAGS "/Zpr")
set_source_files_properties(${Shaders} PROPERTIES VS_SHADER_OBJECT_FILE_NAME " ")

foreach(CurrentShader ${Shaders})
  get_filename_component(CurrentShaderName "${CurrentShader}" NAME_WE)
  set_source_files_properties(${CurrentShader} PROPERTIES VS_SHADER_OUTPUT_HEADER_FILE "${GeneratedShadersDirectory}${CurrentShaderName}.cso.h")
  set_source_files_properties(${CurrentShader} PROPERTIES VS_SHADER_VARIABLE_NAME "cso_${CurrentShaderName}")
  target_sources(hello-triangle PUBLIC ${CurrentShader})

  set (GeneratedShader "${GeneratedShadersDirectory}${CurrentShaderName}.cso.h")
  source_group("src\\shaders" FILES ${GeneratedShader})
  set_source_files_properties(${GeneratedShader} PROPERTIES GENERATED TRUE)
  target_sources(hello-triangle PUBLIC ${GeneratedShader})
endforeach(CurrentShader ${Shaders})