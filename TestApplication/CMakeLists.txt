cmake_minimum_required(VERSION 3.5.0)

add_executable(TestApplication TestApplication.cpp)

target_link_libraries(TestApplication MiraiEngine_Windows)
add_subdirectory(Shaders/)

add_dependencies(TestApplication Shaders)

set_property(TARGET TestApplication PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
message(${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(TARGET TestApplication
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_BINARY_DIR}/SPIRV
  ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV
)
