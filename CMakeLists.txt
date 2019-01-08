cmake_minimum_required (VERSION 3.12)

project("rtrt" CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "deps/cmake")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Define _CRT_SECURE_NO_WARNINGS for all C++ files disabling warnings thrown by compilers when using functions like scanf()
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

# This function set_solution_folder is courtesy of Daniel Konings http://www.danielkonings.com/
function (set_solution_folder arg1)
  if (MSVC)
    set(SolutionFolder "${arg1}")
    foreach (Target ${ARGV})
      if (NOT "${Target}" STREQUAL "${SolutionFolder}")
        set_target_properties("${Target}" PROPERTIES FOLDER "${SolutionFolder}")
      endif ()
    endforeach ()
  endif (MSVC)
endfunction (set_solution_folder)

file (GLOB DXR_FALLBACK_TOOLS "./tools/*")

# Little macro functionality to allow for precompiled headers on MSVC targets
# Source: https://stackoverflow.com/a/1380048
macro(add_msvc_precompiled_header PrecompiledHeader PrecompiledSource SourcesVar)
  if(MSVC)
    get_filename_component(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    set(PrecompiledBinary "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledBasename}.pch")
    set(Sources ${${SourcesVar}})

    set_source_files_properties(${PrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
                                           OBJECT_OUTPUTS "${PrecompiledBinary}")
    set_source_files_properties(${Sources}
                                PROPERTIES COMPILE_FLAGS "/Yu\"${PrecompiledHeader}\" /FI\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
                                           OBJECT_DEPENDS "${PrecompiledBinary}")  
    # Add precompiled header to SourcesVar
    list(APPEND ${SourcesVar} ${PrecompiledHeader} ${PrecompiledSource})
  endif(MSVC)
endmacro(add_msvc_precompiled_header)

# add all dependencies
add_subdirectory("deps")

# treat warnings as errors
# enable this after adding deps, because in-source built deps might contain warnings..
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")

add_subdirectory("src")