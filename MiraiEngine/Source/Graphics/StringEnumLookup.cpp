#include "StringEnumLookup.hpp"

namespace mirai {
    Format get_texture_format(const std::string &inputFormat) {
        if (inputFormat == "B8G8R8A8_UNORM") {
            return FORMAT_B8G8R8A8_UNORM;
        } else if (inputFormat == "R16G16B16A16_SFLOAT") {
            return FORMAT_R16G16B16A16_SFLOAT;
        } else if (inputFormat == "R16G16B16_SFLOAT") {
            return FORMAT_R16G16B16_SFLOAT;
        } else if (inputFormat == "R32G32B32_SFLOAT") {
            return FORMAT_R32G32B32_SFLOAT;
        } else if (inputFormat == "R32G32B32A32_SFLOAT") {
            return FORMAT_R32G32B32A32_SFLOAT;
        } else if (inputFormat == "D32_SFLOAT") {
            return FORMAT_D32_SFLOAT;
        } else if (inputFormat == "D32_SFLOAT_S8_UINT") {
            return FORMAT_D32_SFLOAT_S8_UINT;
        } else if (inputFormat == "R16_SFLOAT") {
            return FORMAT_R16_SFLOAT;
        } else if (inputFormat == "R32_SFLOAT") {
            return FORMAT_R32_SFLOAT;
        } else if (inputFormat == "D24_UNORM_S8_UINT")
            return FORMAT_D24_UNORM_S8_UINT;

        ASSERT(!"Undefined input format");
        return FORMAT_UNDEFINED;
    }

    AttachmentLoadOp get_attachment_load_op(const std::string &op) {
        if (op == "LOAD_OP_CLEAR")
            return LOAD_OP_CLEAR;
        else if (op == "LOAD_OP_LOAD")
            return LOAD_OP_LOAD;

        return LOAD_OP_DONT_CARE;
    }

    CullMode get_cull_mode(const std::string &value) {
        if (value == "CULL_MODE_BACK")
            return CULL_MODE_BACK;
        else if (value == "CULL_MODE_FRONT")
            return CULL_MODE_FRONT;
        else if (value == "CULL_MODE_FRONT_AND_BACK")
            return CULL_MODE_FRONT_AND_BACK;
        else if (value == "CULL_MODE_NONE")
            return CULL_MODE_NONE;
        ASSERT(0);
        return CULL_MODE_BACK;
    }

    FrontFace get_front_face(const std::string &value) {
        if (value == "FRONT_FACE_CLOCKWISE")
            return FRONT_FACE_CLOCKWISE;
        else
            return FRONT_FACE_COUNTER_CLOCKWISE;
    }

    CompareOp get_compare_op(const std::string &value) {
        if (value == "COMPARE_OP_NEVER")
            return COMPARE_OP_NEVER;
        else if (value == "COMPARE_OP_LESS")
            return COMPARE_OP_LESS;
        else if (value == "COMPARE_OP_EQUAL")
            return COMPARE_OP_EQUAL;
        else if (value == "COMPARE_OP_LESS_OR_EQUAL")
            return COMPARE_OP_LESS_OR_EQUAL;
        else if (value == "COMPARE_OP_GREATER")
            return COMPARE_OP_GREATER;
        else if (value == "COMPARE_OP_NOT_EQUAL")
            return COMPARE_OP_NOT_EQUAL;
        else if (value == "COMPARE_OP_GREATER_OR_EQUAL")
            return COMPARE_OP_GREATER_OR_EQUAL;
        else if (value == "COMPARE_OP_ALWAYS")
            return COMPARE_OP_ALWAYS;
        ASSERT(0);
        return COMPARE_OP_LESS_OR_EQUAL;
    }

    Topology get_topology(const std::string &value) {
        if (value == "TOPOLOGY_TRIANGLE_LIST")
            return TOPOLOGY_TRIANGLE_LIST;
        else if (value == "TOPOLOGY_LINE_LIST")
            return TOPOLOGY_LINE_LIST;
        ASSERT(0);
        return TOPOLOGY_TRIANGLE_LIST;
    }

    PolygonMode get_polygon_mode(const std::string &value) {
        if (value == "POLYGON_MODE_FILL")
            return POLYGON_MODE_FILL;
        else if (value == "POLYGON_MODE_LINE")
            return POLYGON_MODE_LINE;
        else if (value == "POLYGON_MODE_POINT")
            return POLYGON_MODE_POINT;
        ASSERT(0);
        return POLYGON_MODE_FILL;
    }

    DrawMode get_draw_mode(const std::string &draw_mode) {
        if (draw_mode == "DRAWMODE_INDEXED")
            return DRAWMODE_INDEXED;
        else if (draw_mode == "DRAWMODE_INDEXED_INDIRECT")
            return DRAWMODE_INDEXED_INDIRECT;
        else if (draw_mode == "DRAWMODE_INSTANCED")
            return DRAWMODE_INSTANCED;
        ASSERT(0);
        return DRAWMODE_INDEXED;
    }

    ShaderPass get_pass_mode(const std::string &pass_mode) {
        if (pass_mode == "SHADER_PASS_DEPTH_PREPASS")
            return SHADER_PASS_DEPTH_PREPASS;
        else if (pass_mode == "SHADER_PASS_PBR_FORWARD")
            return SHADER_PASS_PBR_FORWARD;
        else if (pass_mode == "SHADER_PASS_PBR_FORWARD_TRANSPARENT")
            return SHADER_PASS_PBR_FORWARD_TRANSPARENT;
        else if (pass_mode == "SHADER_PASS_FORWARD_UNLIT")
            return SHADER_PASS_FORWARD_UNLIT;
        else if (pass_mode == "SHADER_PASS_FORWARD_UNLIT_TRANSPARENT")
            return SHADER_PASS_FORWARD_UNLIT_TRANSPARENT;
        else if (pass_mode == "SHADER_PASS_SKYBOX")
            return SHADER_PASS_SKYBOX;
        else if (pass_mode == "SHADER_PASS_PBR_DEFERRED")
            return SHADER_PASS_PBR_DEFERRED;
        else if (pass_mode == "SHADER_PASS_PBR_DEFERRED_TRANSPARENT")
            return SHADER_PASS_PBR_DEFERRED_TRANSPARENT;
        else if (pass_mode == "SHADER_PASS_DEFERRED_UNLIT")
            return SHADER_PASS_DEFERRED_UNLIT;
        else if (pass_mode == "SHADER_PASS_DEFERRED_UNLIT_TRANSPARENT")
            return SHADER_PASS_DEFERRED_UNLIT_TRANSPARENT;
        else if (pass_mode == "SHADER_PASS_CASCADED_ SHADOW")
            return SHADER_PASS_CASCADED_SHADOW;
        else if (pass_mode == "SHADER_PASS_TEXT2D")
            return SHADER_PASS_TEXT2D;
        else if (pass_mode == "SHADER_PASS_DEBUG_DRAW")
            return SHADER_PASS_DEBUG_DRAW;

        ASSERT(0);
        return SHADER_PASS_COUNT;
    }

} // namespace mirai