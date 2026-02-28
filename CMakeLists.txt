cmake_minimum_required(VERSION 3.10)
project(splatting_sandbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED)
find_package(Vulkan REQUIRED)
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(IMGUI REQUIRED IMPORTED_TARGET imgui)
set(IMGUI_SDL3_BACKEND_SRC third-party/imgui_impl_sdl3.cpp)

set(APP_SOURCES
  src/App.cpp
  src/ImGuiLayer.cpp
  src/Renderer.cpp
  src/TriangleLayer.cpp
  src/main.cpp
  src/VulkanErrors.cpp
  ${IMGUI_SDL3_BACKEND_SRC}
)

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(TRIANGLE_VERT_SPV "${SHADER_OUTPUT_DIR}/triangle.vert.spv")
set(TRIANGLE_FRAG_SPV "${SHADER_OUTPUT_DIR}/triangle.frag.spv")

function(compile_shader source output)
  add_custom_command(
    OUTPUT ${output}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
    COMMAND ${GLSLC_EXECUTABLE} ${source} -o ${output}
    DEPENDS ${source}
    VERBATIM
  )
endfunction()

compile_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/triangle.vert ${TRIANGLE_VERT_SPV})
compile_shader(${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/triangle.frag ${TRIANGLE_FRAG_SPV})

add_custom_target(triangle_shaders ALL
  DEPENDS ${TRIANGLE_VERT_SPV} ${TRIANGLE_FRAG_SPV}
)

add_executable(splatting_sandbox ${APP_SOURCES})
add_dependencies(splatting_sandbox triangle_shaders)
target_link_libraries(splatting_sandbox PRIVATE SDL3::SDL3 Vulkan::Vulkan PkgConfig::IMGUI)
target_include_directories(splatting_sandbox PRIVATE /usr/include/imgui/backends)
target_compile_definitions(splatting_sandbox PRIVATE SHADER_DIR="${SHADER_OUTPUT_DIR}")
