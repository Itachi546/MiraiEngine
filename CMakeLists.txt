cmake_minimum_required(VERSION 3.5.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(MiraiEngine VERSION 0.0.1 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MIRAI_DYNAMIC_LIBRARY "Build MiraiEngine as dynamic library" OFF)

add_subdirectory(MiraiEngine)
add_subdirectory(TestApplication)
add_subdirectory(TerrainRendering)