cmake_minimum_required(VERSION 3.5.0)
set(TARGET_NAME TestApplication)

add_executable(${TARGET_NAME} TestApplication.cpp)

target_link_libraries(${TARGET_NAME} MiraiEngine_Windows)
add_subdirectory(Shaders/)

add_dependencies(${TARGET_NAME} Shaders)

set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_command(TARGET ${TARGET_NAME} 
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_BINARY_DIR}/SPIRV
  ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV
)
