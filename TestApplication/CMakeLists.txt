cmake_minimum_required(VERSION 3.5.0)

set(PROJECT_SOURCE_FILES 
    TestApplication.cpp
    ImGuiService.cpp
    External/imgui/imgui.cpp
    External/imgui/imgui_draw.cpp
    External/imgui/imgui_tables.cpp
    External/imgui/imgui_widgets.cpp
    External/imgui/imgui_impl_vulkan.cpp
    External/imgui/imgui_impl_glfw.cpp
)

set(PROJECT_INCLUDE_FILES
    ImGuiService.hpp
    ImGuiRenderPass.hpp
)

add_executable(TestApplication ${PROJECT_SOURCE_FILES} ${PROJECT_INCLUDE_FILES})

target_link_libraries(TestApplication MiraiEngine_Windows)
target_include_directories(TestApplication PUBLIC 
  ./External/imgui
)
target_compile_definitions(TestApplication PRIVATE 
IMGUI_IMPL_VULKAN_USE_VOLK
)

set_property(TARGET TestApplication PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

 add_custom_command(TARGET TestApplication
   POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_directory
   ${CMAKE_CURRENT_SOURCE_DIR}/Assets
   ${CMAKE_BINARY_DIR}/Assets
 )
