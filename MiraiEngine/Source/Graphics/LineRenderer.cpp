#include "LineRenderer.hpp"

#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/ShaderManager.hpp"
#include <vector>

namespace mirai {
    LineRenderer *LineRenderer::Instance = nullptr;

    LineRenderer::LineRenderer() {
        Instance = this;
        BufferDescription buffer_desc = {
            .size = K_MAX_LINE_COUNT * sizeof(Line),
            .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };

        RenderingDevice *device = RenderingDevice::get();
        buffer = device->create_buffer(&buffer_desc, "line_vertex_buffer");
        line_array = (Line *)device->map_buffer(buffer);

        shader_material = ShaderManager::get()->get_shader("line_3d");

        UniformLayout vertex_layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_STORAGE_BUFFER,
            .shader_stage = SHADER_STAGE_VERTEX,
        };
        uniform_set = device->create_uniform_set(&vertex_layout, 1, 0, "line_uniform_set");
        UniformBinding binding = {
            .resource_id = buffer,
        };
        device->update_uniform_set(uniform_set, &binding, 1);
        shader_material->set_uniform_sets(&uniform_set, 1);
    }

    void LineRenderer::NewFrame() {
        line_count = 0;
    }

    void LineRenderer::AddLine(glm::vec3 s, glm::vec3 e, uint32_t color) {
        ASSERT(line_count < K_MAX_LINE_COUNT);
        Line *line = &line_array[line_count++];
        line->start.x = s.x, line->start.y = s.y, line->start.z = s.z;
        line->start.color = color;

        line->end.x = e.x, line->end.y = e.y, line->end.z = e.z;
        line->end.color = color;
    }

    void LineRenderer::AddAABB(const AABB &aabb, uint32_t color) {
        const glm::vec3 &min = aabb.min;
        const glm::vec3 &max = aabb.max;

        // Bottom
        AddLine(min, glm::vec3(max.x, min.y, min.z), color);
        AddLine(min, glm::vec3(min.x, min.y, max.z), color);
        AddLine(glm::vec3(max.x, min.y, max.z), glm::vec3(min.x, min.y, max.z), color);
        AddLine(glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, min.y, min.z), color);

        // Top
        AddLine(glm::vec3(min.x, max.y, min.z), glm::vec3(max.x, max.y, min.z), color);
        AddLine(glm::vec3(min.x, max.y, min.z), glm::vec3(min.x, max.y, max.z), color);
        AddLine(glm::vec3(max.x, max.y, max.z), glm::vec3(min.x, max.y, max.z), color);
        AddLine(glm::vec3(max.x, max.y, max.z), glm::vec3(max.x, max.y, min.z), color);

        // Joint
        AddLine(min, glm::vec3(min.x, max.y, min.z), color);
        AddLine(glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, max.y, max.z), color);
        AddLine(glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z), color);
        AddLine(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z), color);
    }

    void LineRenderer::AddFrustum(const std::array<glm::vec3, 8> &points, uint32_t color) {
        // Near Plane
        AddLine(points[0], points[1], color);
        AddLine(points[0], points[3], color);
        AddLine(points[1], points[2], color);
        AddLine(points[3], points[2], color);

        // Far Plane
        AddLine(points[4], points[5], color);
        AddLine(points[4], points[7], color);
        AddLine(points[5], points[6], color);
        AddLine(points[7], points[6], color);

        // Joint
        AddLine(points[0], points[4], color);
        AddLine(points[1], points[5], color);
        AddLine(points[3], points[7], color);
        AddLine(points[2], points[6], color);
    }

    LineRenderer::~LineRenderer() {
        shader_material = nullptr;
        RenderingDevice::get()->destroy_buffers(&buffer, 1);
    }
} // namespace mirai
