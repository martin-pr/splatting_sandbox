cmake_minimum_required(VERSION 3.10)
project(splatting_sandbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED)
find_package(Vulkan REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(IMGUI REQUIRED IMPORTED_TARGET imgui)
set(IMGUI_SDL3_BACKEND_SRC third-party/imgui_impl_sdl3.cpp)

set(APP_SOURCES
  src/App.cpp
  src/ImGuiLayer.cpp
  src/Renderer.cpp
  src/main.cpp
  ${IMGUI_SDL3_BACKEND_SRC}
)

add_executable(splatting_sandbox ${APP_SOURCES})
target_link_libraries(splatting_sandbox PRIVATE SDL3::SDL3 Vulkan::Vulkan PkgConfig::IMGUI)
target_include_directories(splatting_sandbox PRIVATE /usr/include/imgui/backends)
