cmake_minimum_required(VERSION 3.5.0)

add_executable(TestApplication TestApplication.cpp)

target_link_libraries(TestApplication MiraiEngine_Windows)

set_property(TARGET TestApplication PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

message(${CMAKE_BUILD_TYPE})
add_custom_command(TARGET TestApplication
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_BINARY_DIR}/SPIRV
  ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/SPIRV
)
