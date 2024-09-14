cmake_minimum_required(VERSION 3.5.0)
project(TestApplication VERSION 0.0.1 LANGUAGES CXX C)

add_executable(${PROJECT_NAME} TestApplication.cpp)
message(CMAKE_LIB_DIRECTORY = "${CMAKE_BINARY_DIR}")
target_link_libraries(${PROJECT_NAME} MiraiEngine_Windows)

# add_custom_command(TARGET ${PROJECT_NAME} 
#   POST_BUILD
#   COMMAND ${CMAKE_COMMAND} -E copy 
#   ${CMAKE_BINARY_DIR}/glfw3.dll
#   ${CMAKE_CURRENT_BINARY_DIR}/glfw3.dll
# )