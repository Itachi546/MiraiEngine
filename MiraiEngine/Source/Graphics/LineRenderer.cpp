#include "LineRenderer.hpp"

#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/Shader.hpp"
#include "Scene/Material.hpp"
#include "Engine/AppSettings.hpp"
#include <vector>

namespace mirai {
    LineRenderer *LineRenderer::Instance = nullptr;

    LineRenderer::LineRenderer() {
        ASSERT(Instance == nullptr);
        Instance = this;

        BufferDescription buffer_desc = {
            .size = cast_u32(K_MAX_LINE_COUNT * sizeof(Line)) * AppSettings::K_MAX_FRAME_IN_FLIGHTS,
            .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };

        RenderingDevice *device = RenderingDevice::get();
        buffer = device->create_buffer(&buffer_desc, "line_vertex_buffer");
        line_array = (Line *)device->map_buffer(buffer);

        shader = std::make_shared<EffectMaterial>("DebugDrawLine",
                                                  std::vector<std::string>{"SPIRV/line.vert.spv", "SPIRV/line.frag.spv"},
                                                  PipelineState{
                                                      .topology = TOPOLOGY_LINE_LIST,
                                                      .blend_mode = BLEND_MODE_MIX,
                                                      .depth_test = true,
                                                      .depth_write = true,
                                                  },
                                                  PipelineAttachmentInfo{
                                                      .color_attachments_format = {FORMAT_R16G16B16A16_SFLOAT},
                                                      .has_depth_attachment = true,
                                                      .depth_attachment_format = FORMAT_D32_SFLOAT,
                                                  });
    }

    void LineRenderer::new_frame(uint32_t current_frame_index) {
        line_count = 0;
        per_frame_offset = current_frame_index * cast_u32(K_MAX_LINE_COUNT);
    }

    void LineRenderer::render(CommandBuffer *command_buffer, const glm::mat4 &VP) {
        if (line_count == 0 || !AppSettings::enable_debug_draw)
            return;
        shader->bind(command_buffer);
        command_buffer->set_push_data(0, &VP[0][0], sizeof(glm::mat4));

        DescriptorInfo descriptor_info = {.type = DescriptorType::StorageBuffer,
                                          .resource = buffer,
                                          .buffer_info = {
                                              .offset = cast_u32(per_frame_offset * sizeof(Line)),
                                              .size = cast_u32(line_count * sizeof(Line)),
                                          }};
        DescriptorOffset descriptor = Renderer::get()->resource_heap.push_descriptors_per_frame(RenderingDevice::get(), &descriptor_info, 1);
        command_buffer->set_push_data(cast_u32(sizeof(glm::mat4)), &descriptor, cast_u32(sizeof(uint32_t)));
        command_buffer->draw(line_count * 2, 1, 0, 0);
    }

    void LineRenderer::add_line(glm::vec3 s, glm::vec3 e, uint32_t color) {
        if (!AppSettings::enable_debug_draw)
            return;
        ASSERT(line_count < K_MAX_LINE_COUNT);
        Line *line = &line_array[per_frame_offset + line_count++];
        line->start.x = s.x, line->start.y = s.y, line->start.z = s.z;
        line->start.color = color;

        line->end.x = e.x, line->end.y = e.y, line->end.z = e.z;
        line->end.color = color;
    }

    void LineRenderer::add_aabb(const AABB &aabb, uint32_t color) {
        if (!AppSettings::enable_debug_draw)
            return;
        const glm::vec3 &min = aabb.min;
        const glm::vec3 &max = aabb.max;

        // Bottom
        add_line(min, glm::vec3(max.x, min.y, min.z), color);
        add_line(min, glm::vec3(min.x, min.y, max.z), color);
        add_line(glm::vec3(max.x, min.y, max.z), glm::vec3(min.x, min.y, max.z), color);
        add_line(glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, min.y, min.z), color);

        // Top
        add_line(glm::vec3(min.x, max.y, min.z), glm::vec3(max.x, max.y, min.z), color);
        add_line(glm::vec3(min.x, max.y, min.z), glm::vec3(min.x, max.y, max.z), color);
        add_line(glm::vec3(max.x, max.y, max.z), glm::vec3(min.x, max.y, max.z), color);
        add_line(glm::vec3(max.x, max.y, max.z), glm::vec3(max.x, max.y, min.z), color);

        // Joint
        add_line(min, glm::vec3(min.x, max.y, min.z), color);
        add_line(glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, max.y, max.z), color);
        add_line(glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z), color);
        add_line(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z), color);
    }

    void LineRenderer::add_frustum(const std::array<glm::vec3, 8> &points, uint32_t color) {
        if (!AppSettings::enable_debug_draw)
            return;
        // Near Plane
        add_line(points[0], points[1], color);
        add_line(points[0], points[3], color);
        add_line(points[1], points[2], color);
        add_line(points[3], points[2], color);

        // Far Plane
        add_line(points[4], points[5], color);
        add_line(points[4], points[7], color);
        add_line(points[5], points[6], color);
        add_line(points[7], points[6], color);

        // Joint
        add_line(points[0], points[4], color);
        add_line(points[1], points[5], color);
        add_line(points[3], points[7], color);
        add_line(points[2], points[6], color);
    }

    LineRenderer::~LineRenderer() {
        shader = nullptr;
        RenderingDevice::get()->destroy_buffers(&buffer, 1);
    }
} // namespace mirai