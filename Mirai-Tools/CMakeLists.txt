cmake_minimum_required(VERSION 3.5.0)

project(MiraiTools)

set(PROJECT_SOURCE_FILES
    main.cpp
    ImGuiService.cpp
    External/imgui/imgui.cpp
    External/imgui/imgui_draw.cpp
    External/imgui/imgui_tables.cpp
    External/imgui/imgui_widgets.cpp
    External/imgui/imgui_impl_vulkan.cpp
    External/imgui/imgui_impl_glfw.cpp
)

set(PROJECT_HEADER_FILES
    ImGuiService.hpp
    MainPass.hpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_FILES} ${PROJECT_HEADER_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ./External/imgui)
target_compile_definitions(${PROJECT_NAME} PRIVATE 
IMGUI_IMPL_VULKAN_USE_VOLK
)

target_link_libraries(${PROJECT_NAME} MiraiEngine_Windows)

 add_custom_command(TARGET ${PROJECT_NAME} 
   POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_directory
   ${CMAKE_CURRENT_SOURCE_DIR}/Assets
   ${CMAKE_BINARY_DIR}/Assets
 )
