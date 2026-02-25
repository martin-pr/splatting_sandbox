cmake_minimum_required(VERSION 3.10)
project(splatting_sandbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(APP_SOURCES
  src/main.cpp
)

add_executable(splatting_sandbox ${APP_SOURCES})
