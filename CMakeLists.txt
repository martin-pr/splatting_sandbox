cmake_minimum_required(VERSION 3.10)
project(splatting_sandbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED)
find_package(Vulkan REQUIRED)

set(APP_SOURCES
  src/App.cpp
  src/Renderer.cpp
  src/main.cpp
)

add_executable(splatting_sandbox ${APP_SOURCES})
target_link_libraries(splatting_sandbox PRIVATE SDL3::SDL3 Vulkan::Vulkan)
