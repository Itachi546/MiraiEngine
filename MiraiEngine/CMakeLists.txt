cmake_minimum_required(VERSION 3.5.0)

if(WIN32)
set(TARGET_NAME MiraiEngine_Windows)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

if (MIRAI_DYNAMIC_LIBRARY)
set(MIRAI_LIBRARY_TYPE SHARED)
message(STATUS "Building MiraiEngine as shared library")
else()
set(MIRAI_LIBRARY_TYPE STATIC)
message(STATUS "Building MiraiEngine as static library")
endif()

find_package(Vulkan REQUIRED)

find_package(glfw QUIET)
if(NOT TARGET glfw)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
    set(GLFW_INSTALL OFF CACHE BOOL "")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
    add_subdirectory(../External/glfw glfw)
    set_target_properties(glfw PROPERTIES FOLDER "External")
endif()

find_package(glm QUIET)
if(NOT TARGET glm)
    set(GLM_ENABLE_CXX_17 ON CACHE BOOL "")
    set(GLM_BUILD_TESTS OFF CACHE BOOL "")
    set(GLM_BUILD_INSTALL OFF CACHE BOOL "")
    
    add_subdirectory(../External/glm glm)
    set_target_properties(glm PROPERTIES FOLDER "External")
endif()

find_package(volk QUIET)
if(NOT TARGET volk)
    add_subdirectory(../External/volk volk)
    set_target_properties(volk PROPERTIES FOLDER "External")
endif()

set(MIRAI_LIBRARIES
    glfw
    volk::volk
)

file(GLOB_RECURSE PROJECT_HEADER_FILES CONFIGURE_DEPENDS Source/*.hpp)
file(GLOB_RECURSE PROJECT_SOURCE_FILES CONFIGURE_DEPENDS Source/*.cpp)

set( HEADER_FILES ${PROJECT_HEADER_FILES})
set( SOURCE_FILES ${PROJECT_SOURCE_FILES} "${CMAKE_SOURCE_DIR}/External/SPIRV-Reflect/spirv_reflect.c")

add_library(${TARGET_NAME} ${MIRAI_LIBRARY_TYPE} ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(${TARGET_NAME} 
    PUBLIC 
    ${Vulkan_INCLUDE_DIRS}
    ../External/volk
    ../External/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ../External/SPIRV-Reflect
    ../External/tinygltf
    ../External/stb

)

target_link_libraries(${TARGET_NAME} PRIVATE ${MIRAI_LIBRARIES})
target_link_libraries(${TARGET_NAME} PUBLIC glm::glm)

target_compile_definitions(${TARGET_NAME} PUBLIC 
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
    _CRT_SECURE_NO_WARNINGS
    MIRAI_BACKEND_VULKAN
)

if(WIN32) 
    target_compile_definitions(${TARGET_NAME} PRIVATE 
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    )
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

target_compile_options(${TARGET_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W3>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

add_subdirectory(Shaders/)
add_dependencies(${TARGET_NAME} Shaders)

add_custom_command(TARGET ${TARGET_NAME} 
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/Assets
  ${CMAKE_BINARY_DIR}/Assets
)

