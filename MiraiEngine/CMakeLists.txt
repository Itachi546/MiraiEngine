cmake_minimum_required(VERSION 3.5.0)

if(WIN32)
set(TARGET_NAME MiraiEngine_Windows)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

if (MIRAI_DYNAMIC_LIBRARY)
set(MIRAI_LIBRARY_TYPE SHARED)
message(STATUS "Building MiraiEngine as shared library")
else()
set(MIRAI_LIBRARY_TYPE STATIC)
message(STATUS "Building MiraiEngine as static library")
endif()

find_package(glfw3 QUIET)
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

file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS *.hpp)
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS *.cpp)

add_library(${TARGET_NAME} ${MIRAI_LIBRARY_TYPE} ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(${TARGET_NAME} 
    PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME} PRIVATE ${MIRAI_LIBRARIES})
target_link_libraries(${TARGET_NAME} INTERFACE glm::glm)

target_compile_definitions(${TARGET_NAME} PRIVATE 
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_XYZW_ONLY
)

if(WIN32) 
    target_compile_definitions(${TARGET_NAME} PRIVATE 
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    )
    target_compile_options(${TARGET_NAME} PRIVATE /W3)
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()
