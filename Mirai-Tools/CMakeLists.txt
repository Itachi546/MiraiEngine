cmake_minimum_required(VERSION 3.5.0)

project(MiraiTools)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} MiraiEngine_Windows)
