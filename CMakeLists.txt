cmake_minimum_required(VERSION 3.24)

project(DirectXRayTracingSamples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "These samples target Windows and DirectX 12.")
endif()

set(DXC_NUGET_PACKAGE "Microsoft.Direct3D.DXC_1.9.2602.17")

add_library(dx12_sdk INTERFACE)
target_link_libraries(dx12_sdk INTERFACE d3d12 dxgi d3dcompiler dxcompiler)
target_compile_definitions(dx12_sdk INTERFACE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    DX12_SAMPLE_ASSET_DIR=L"${CMAKE_CURRENT_SOURCE_DIR}/Assets"
)

function(dx12_source_groups target)
    get_target_property(target_sources ${target} SOURCES)
    foreach(source_file IN LISTS target_sources)
        if(IS_ABSOLUTE "${source_file}")
            file(RELATIVE_PATH relative_file "${CMAKE_CURRENT_SOURCE_DIR}" "${source_file}")
        else()
            set(relative_file "${source_file}")
        endif()
        get_filename_component(source_path "${relative_file}" PATH)
        if(source_path)
            string(REPLACE "/" "\\" group_name "${source_path}")
            source_group("${group_name}" FILES "${source_file}")
        endif()
    endforeach()
endfunction()

set(COMMON_SOURCES
    Src/Common/Desktop/Window.cpp
    Src/Common/Desktop/Window.h
    Src/Common/Graphics/DescriptorHeap.cpp
    Src/Common/Graphics/DescriptorHeap.h
    Src/Common/Graphics/GUI.cpp
    Src/Common/Graphics/GUI.h
    Src/Common/ShaderCompiler.h
    External/imgui/imgui.cpp
    External/imgui/imgui.h
    External/imgui/imconfig.h
    External/imgui/imgui_draw.cpp
    External/imgui/imgui_internal.h
    External/imgui/imgui_tables.cpp
    External/imgui/imgui_widgets.cpp
    External/imgui/imstb_rectpack.h
    External/imgui/imstb_textedit.h
    External/imgui/imstb_truetype.h
    External/imgui/backends/imgui_impl_dx12.cpp
    External/imgui/backends/imgui_impl_dx12.h
    External/imgui/backends/imgui_impl_win32.cpp
    External/imgui/backends/imgui_impl_win32.h
    External/fastgltf/include/fastgltf/base64.hpp
    External/fastgltf/include/fastgltf/core.hpp
    External/fastgltf/include/fastgltf/dxmath_element_traits.hpp
    External/fastgltf/include/fastgltf/glm_element_traits.hpp
    External/fastgltf/include/fastgltf/math.hpp
    External/fastgltf/include/fastgltf/tools.hpp
    External/fastgltf/include/fastgltf/types.hpp
    External/fastgltf/include/fastgltf/util.hpp
)

add_library(Common STATIC ${COMMON_SOURCES})
target_include_directories(Common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/Common"
    "${CMAKE_CURRENT_SOURCE_DIR}/External/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/External/imgui/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/External/fastgltf/include"
)
target_link_libraries(Common PUBLIC dx12_sdk)
set_target_properties(Common PROPERTIES
    VS_PACKAGE_REFERENCES "${DXC_NUGET_PACKAGE}"
    VS_GLOBAL_NuGetTargetMoniker "native,Version=v0.0"
)
dx12_source_groups(Common)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/External/imgui" PREFIX "Common\\External\\imgui" FILES
    External/imgui/imgui.cpp
    External/imgui/imgui.h
    External/imgui/imconfig.h
    External/imgui/imgui_draw.cpp
    External/imgui/imgui_internal.h
    External/imgui/imgui_tables.cpp
    External/imgui/imgui_widgets.cpp
    External/imgui/imstb_rectpack.h
    External/imgui/imstb_textedit.h
    External/imgui/imstb_truetype.h
    External/imgui/backends/imgui_impl_dx12.cpp
    External/imgui/backends/imgui_impl_dx12.h
    External/imgui/backends/imgui_impl_win32.cpp
    External/imgui/backends/imgui_impl_win32.h
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/External/fastgltf/include" PREFIX "Common\\External\\fastgltf" FILES
    External/fastgltf/include/fastgltf/base64.hpp
    External/fastgltf/include/fastgltf/core.hpp
    External/fastgltf/include/fastgltf/dxmath_element_traits.hpp
    External/fastgltf/include/fastgltf/glm_element_traits.hpp
    External/fastgltf/include/fastgltf/math.hpp
    External/fastgltf/include/fastgltf/tools.hpp
    External/fastgltf/include/fastgltf/types.hpp
    External/fastgltf/include/fastgltf/util.hpp
)

function(add_dx12_sample target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Src/${target}")
    target_link_libraries(${target} PRIVATE Common)
    set_target_properties(${target} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VS_PACKAGE_REFERENCES "${DXC_NUGET_PACKAGE}"
        VS_GLOBAL_NuGetTargetMoniker "native,Version=v0.0"
    )
    dx12_source_groups(${target})
endfunction()

add_dx12_sample(ClearScreen Src/ClearScreen/ClearScreen.cpp)
add_dx12_sample(AccelerationStructures Src/AccelerationStructures/AccelerationStructures.cpp)
add_dx12_sample(VertexBuffer Src/VertexBuffer/VertexBuffer.cpp)
add_dx12_sample(IndexBuffer Src/IndexBuffer/IndexBuffer.cpp)
add_dx12_sample(ConstantBuffer Src/ConstantBuffer/ConstantBuffer.cpp)
add_dx12_sample(ImGui Src/ImGui/ImGui.cpp)
