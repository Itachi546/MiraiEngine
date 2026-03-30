set(DXC "$ENV{VULKAN_SDK}/Bin/dxc.exe")
set(SPIRV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/SPIRV_HLSL")
set(DEP_OUTPUT_DIR "${SPIRV_OUTPUT_DIR}/deps")

set(SHADER_STAGES vert frag comp)

file(GLOB HLSL_SOURCE_FILES CONFIGURE_DEPENDS "*.hlsl")

file(MAKE_DIRECTORY "${SPIRV_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${DEP_OUTPUT_DIR}")

foreach(SHADER_PATH ${HLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${SHADER_PATH} NAME_WLE)
    get_filename_component(SHADER_ABS_PATH ${SHADER_PATH} ABSOLUTE)

    foreach(STAGE_EXT ${SHADER_STAGES})
        string(FIND "${FILE_NAME}" ".${STAGE_EXT}" STAGE_MATCH)
        if(STAGE_MATCH EQUAL -1)
            continue()
        endif()

        if(STAGE_EXT STREQUAL "vert")
            set(STAGE_PROFILE "vs_6_6")
            set(STAGE_ENTRY   "main")
        elseif(STAGE_EXT STREQUAL "frag")
            set(STAGE_PROFILE "ps_6_6")
            set(STAGE_ENTRY   "main")
        elseif(STAGE_EXT STREQUAL "comp")
            set(STAGE_PROFILE "cs_6_6")
            set(STAGE_ENTRY   "main")
        endif()

        set(OUT_SPV  "${SPIRV_OUTPUT_DIR}/${FILE_NAME}.spv")
        set(DEP_FILE "${DEP_OUTPUT_DIR}/${FILE_NAME}.d")

        # Stage 1: generate only the dep file
        add_custom_command(
            OUTPUT "${DEP_FILE}"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${DEP_FILE}"
            COMMAND ${DXC}
                    -spirv
                    -T ${STAGE_PROFILE} -E ${STAGE_ENTRY} "${SHADER_ABS_PATH}"
                    -MF "${DEP_FILE}"
                    -Fo "${DEP_FILE}.null.spv"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${DEP_FILE}.null.spv"
            DEPENDS "${SHADER_PATH}"
            COMMENT "Generating depfile for ${FILE_NAME}"
            VERBATIM
        )

        # Stage 2: compile SPV, depends on dep file from stage 1
        add_custom_command(
            OUTPUT "${OUT_SPV}"
            COMMAND ${DXC}
                    -spirv
                    -T ${STAGE_PROFILE} -E ${STAGE_ENTRY} "${SHADER_ABS_PATH}"
                    -Fo "${OUT_SPV}"
            DEPENDS "${DEP_FILE}"
            DEPFILE "${DEP_FILE}"
            COMMENT "Compiling ${FILE_NAME} -> ${OUT_SPV}"
            VERBATIM
        )

        list(APPEND SPIRV_BINARY_FILES ${OUT_SPV})
    endforeach()
endforeach()

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)