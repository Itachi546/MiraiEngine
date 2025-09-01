cmake_minimum_required(VERSION 3.5.0)
project(TerrainRendering)

find_package(Vulkan REQUIRED)
file(GLOB_RECURSE PROJECT_HEADER_FILES CONFIGURE_DEPENDS Source/*.hpp)
file(GLOB_RECURSE PROJECT_SOURCE_FILES CONFIGURE_DEPENDS Source/*.cpp)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_FILES} ${PROJECT_HEADER_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC 
  ../External/volk
  ../External/glfw/include
  ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} MiraiEngine_Windows)
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(Shaders/)
add_dependencies(${PROJECT_NAME} TerrainShaders)

# Copy Assets Folder
 add_custom_command(TARGET ${PROJECT_NAME} 
   POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_directory
   ${CMAKE_CURRENT_SOURCE_DIR}/Assets
   ${CMAKE_BINARY_DIR}/Assets
 )
