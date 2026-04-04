#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Scene/Shader.hpp"

namespace mirai {
    Format get_texture_format(const std::string &inputFormat);
    AttachmentLoadOp get_attachment_load_op(const std::string &op);
    CullMode get_cull_mode(const std::string &value);
    FrontFace get_front_face(const std::string &value);
    CompareOp get_compare_op(const std::string &value);
    Topology get_topology(const std::string &value);
    PolygonMode get_polygon_mode(const std::string &value);
    DrawMode get_draw_mode(const std::string &draw_mode);
} // namespace mirai