cmake_minimum_required(VERSION 3.5.0)
set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
set(SPIRV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/SPIRV")
set(DEP_OUTPUT_DIR "${SPIRV_OUTPUT_DIR}/deps")

file(GLOB_RECURSE GLSL_SOURCE_FILES CONFIGURE_DEPENDS
    "*.frag.glsl"
    "*.vert.glsl"
    "*.comp.glsl"
    "*.geom.glsl"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    # 1. Get the filename (e.g., "main.vert.glsl")
    get_filename_component(FULL_NAME ${GLSL} NAME)
    
    # 2. Strip ".glsl" and append ".spv" (e.g., "main.vert.spv")
    string(REPLACE ".glsl" ".spv" SPV_NAME "${FULL_NAME}")
    
    set(SPIRV "${SPIRV_OUTPUT_DIR}/${SPV_NAME}")
    set(DEP_FILE "${DEP_OUTPUT_DIR}/${SPV_NAME}.d")

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUTPUT_DIR}" "${DEP_OUTPUT_DIR}"
        COMMAND ${GLSL_VALIDATOR} -V --target-env vulkan1.4 ${GLSL} 
                -o ${SPIRV} 
                -g0 
                --depfile ${DEP_FILE}
        DEPENDS ${GLSL}
        DEPFILE ${DEP_FILE}
        COMMENT "Compiling ${FULL_NAME} -> ${SPV_NAME}"
        VERBATIM
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)