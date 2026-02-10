#include "Engine/App.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"
#include "Device/Window.hpp"
#include "Scene/Scene.hpp"
#include "Scene/TextureCache.hpp"
#include "Scene/FrameGraph.hpp"
#include "Graphics/Renderer.hpp"
#include "RenderPass/RenderPass.hpp"
#include "Utils/FirstPersonController.hpp"
#include "Scene/GLTFLoader.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Math/MathUtils.hpp"
#include "Inspector.hpp"

#include "ImGuiService.hpp"
#include "ImGuiRenderPass.hpp"

#include <fstream>
#include <filesystem>

using namespace mirai;
const Color Color_Black = {0.0f, 0.0f, 0.0f, 1.0f};

class TestApplication : public App {
  public:
    TestApplication(const std::vector<std::string> &model_paths) : App("TestApplication"), model_paths(model_paths) {
        Window::get()->set_title("TestApplication");
        Window::get()->set_fullscreen(fullscreen);
        frame_graph = nullptr;
        scene = nullptr;
    }

    void initialize_frame_graph(FrameGraph *frame_graph) {
        FrameGraphNodeDescription depth_prepass = {
            .name = "depth_prepass",
            .enabled = true,
            .is_compute_pass = false,
            .outputs = {
                FrameGraphResourceOutput{
                    .name = "texture_depth",
                    .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                    .width = 1920,
                    .height = 1080,
                    .array_layers = 1,
                    .format = FORMAT_D32_SFLOAT,
                    .load_op = LOAD_OP_CLEAR,
                    .clear_color = Color{1.0f, 0.0f, 0.0f, 1.0f},
                },

            },
            .renderer = std::make_shared<DepthPrePass>(),
        };
        frame_graph->add_node(depth_prepass);

        FrameGraphNodeDescription forward_pass = {
            .name = "forward_pass",
            .enabled = true,
            .is_compute_pass = false,
            .inputs = {
                FrameGraphResourceInput{
                    .name = "texture_depth",
                    .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                    .load_op = LOAD_OP_LOAD,
                },
            },
            .outputs = {
                FrameGraphResourceOutput{
                    .name = "texture_color",
                    .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                    .width = 1920,
                    .height = 1080,
                    .array_layers = 1,
                    .format = FORMAT_B8G8R8A8_UNORM,
                    .load_op = LOAD_OP_CLEAR,
                    .clear_color = 0x333333ff,
                },
            },
            .renderer = std::make_shared<ForwardPass>(),
        };
        frame_graph->add_node(forward_pass);

        FrameGraphNodeDescription overlay_pass = {
            .name = "overlay3D",
            .enabled = true,
            .is_compute_pass = false,
            .inputs = {
                FrameGraphResourceInput{
                    .name = "texture_depth",
                    .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                    .load_op = LOAD_OP_LOAD,
                },
                FrameGraphResourceInput{
                    .name = "texture_color",
                    .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                    .load_op = LOAD_OP_LOAD,
                },
            },
            .outputs = {
                FrameGraphResourceOutput{
                    .name = "texture_color",
                    .resource_type = FRAMEGRAPH_RESOURCE_TYPE_REFERENCE,
                },
            },
            .renderer = std::make_shared<Overlay3DPass>(),
        };
        frame_graph->add_node(overlay_pass);

        FrameGraphNodeDescription swapchain_copy_pass = {
            .name = "swapchain_copy",
            .enabled = true,
            .is_compute_pass = false,
            .inputs = {
                FrameGraphResourceInput{
                    .name = "texture_color",
                    .resource_type = FRAMEGRAPH_RESOURCE_TYPE_TEXTURE,
                },
            },
            .outputs = {
                FrameGraphResourceOutput{
                    .name = "swapchain",
                    .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                    .load_op = LOAD_OP_CLEAR,
                },
            },
            .renderer = std::make_shared<SwapchainCopyPass>(),
        };
        frame_graph->add_node(swapchain_copy_pass);

        FrameGraphNodeDescription imgui_pass = {
            .name = "imgui_pass",
            .enabled = true,
            .is_compute_pass = false,
            .inputs = {
                FrameGraphResourceInput{
                    .name = "swapchain",
                    .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                },
            },
            .outputs = {
                FrameGraphResourceOutput{
                    .name = "swapchain",
                    .resource_type = FRAMEGRAPH_RESOURCE_TYPE_REFERENCE,
                    .load_op = LOAD_OP_CLEAR,
                },
            },
            .renderer = std::make_shared<ImGuiRenderPass>(),
        };
        frame_graph->add_node(imgui_pass);
    }

    void start() override {
        ImGuiService::Initialize();

        uint32_t width = 1920;
        uint32_t height = 1080;

        Renderer *renderer = Renderer::get();
        scene = renderer->get_scene();

        std::shared_ptr<EnvironmentMap> env_map = std::make_shared<EnvironmentMap>("Assets/Envmap/papermill.hdr");

        scene->set_environment_map(env_map);

        Camera *camera = scene->get_camera();
        camera->position = glm::vec3(-0.529f, 1.127f, -0.067f);
        camera->rotation = glm::vec3(-5.2f, -180.853f, 0.0f);
        camera->set_far_plane(1000.0f);
        // Create RenderPass
        frame_graph = Renderer::get()->get_frame_graph();
#if 0
        renderer->set_pipeline_description_file("Assets/forward-pipelines.json");
        initialize_frame_graph(frame_graph);
        AppSettings::render_mode = RenderMode::RENDERMODE_FORWARD;
#else
        renderer->set_pipeline_description_file("Assets/deferred-pipelines.json");
        frame_graph->load_from_file("Assets/deferred-graph.json");
        AppSettings::render_mode = RenderMode::RENDERMODE_DEFERRED;

        frame_graph->set_renderer("deferred_pass", std::make_shared<DeferredPass>());
        frame_graph->set_renderer("deferred_lighting_pass", std::make_shared<DeferredLightingPass>());
        frame_graph->set_renderer("deferred_transparent_pass", std::make_shared<DeferredTransparentPass>());

        frame_graph->set_renderer("ssao_pass", std::make_shared<SSAOPass>());
        frame_graph->set_renderer("directional_shadow_pass", std::make_shared<CascadedShadowPass>());
        frame_graph->set_renderer("rt_directional_shadow_pass", std::make_shared<DirectionalShadowPassRT>());
        frame_graph->set_renderer("debug_pass", std::make_shared<DebugPass>());
        frame_graph->set_renderer("overlay3D", std::make_shared<Overlay3DPass>());
        frame_graph->set_renderer("imgui_pass", std::make_shared<ImGuiRenderPass>());
        frame_graph->set_renderer("taa_resolve_pass", std::make_shared<TAAResolvePass>());
        frame_graph->set_renderer("swapchain_copy", std::make_shared<SwapchainCopyPass>());
        frame_graph->set_renderer("imgui_pass", std::make_shared<ImGuiRenderPass>());

#endif
        if (model_paths.size() > 0) {
            for (const auto &path : model_paths)
                ImportModel_GLTF(path, scene);
        }

        controller = std::make_unique<FirstPersonController>(camera);
        controller->set_walk_speed(10.0f);
        controller->set_run_speed(20.0f);
    }

    void update() override {
        ImGuiService::NewFrame();
        if (Input::get()->is_down(KB_ESCAPE))
            Engine::get()->request_close();

        float dt = Engine::get()->get_dt_seconds();

        controller->set_disable_input(ImGuiService::IsAcceptingEvent());
        controller->update(dt);

        if (Input::get()->was_down(KB_F)) {
            fullscreen = !fullscreen;
            Window::get()->set_fullscreen(fullscreen);
        }

        if (Input::get()->was_down(KB_O)) {
            show_debug_ui = !show_debug_ui;
        }

        add_debug_ui();
    }

    void add_profiler_ui() {
        if (!miProfiler::IsEnabled())
            return;
        std::vector<miProfiler::ProfilerOutput> cpu, gpu;
        miProfiler::GetProfilerOutput(cpu, gpu);

        if (ImGui::CollapsingHeader("Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (cpu.size() > 0) {
                ImGui::Text("CPU Time");
                ImGui::Separator();
                for (auto &[name, time] : cpu) {
                    ImGui::Text("%s: %.2fms", name.c_str(), time);
                }
            }

            if (gpu.size() > 0) {
                ImGui::Text("GPU Time");
                ImGui::Separator();
                for (auto &[name, time] : gpu) {
                    ImGui::Text("%s: %.2fms", name.c_str(), time);
                }
            }
        }
    }

    void add_scene_ui() {
        if (ImGui::CollapsingHeader("Scene Stats")) {
            auto *final_pass = (SwapchainCopyPass *)frame_graph->get_renderer("swapchain_copy");
            if (final_pass) {
                static bool enable_aa = final_pass->is_antialiasing_enabled();
                if (ImGui::Checkbox("FXAA", &enable_aa)) {
                    final_pass->set_antialiasing(enable_aa);
                }
            }

            uint64_t memory_usage = RenderingDevice::get()->get_memory_usage();
            ImGui::Text("GPU Memory Usage: %.2f MB", utils::bytes_to_mb(memory_usage));

            uint32_t total_entities = 0;
            for (auto &batch : Renderer::get()->main_render_batches) {
                for (auto &mesh_batch : batch.meshes) {
                    total_entities += cast_u32(mesh_batch.mesh_draw_infos.size());
                }
            }
            ImGui::Text("Total Visible Entities: %u", total_entities);

            uint32_t total_materials = cast_u32(scene->materials.size());
            ImGui::Text("Total Materials: %u", total_materials);

            uint32_t total_mesh_buffer = cast_u32(scene->gpu_meshes.size());
            ImGui::Text("Total Mesh Buffer: %u", total_mesh_buffer);

            uint32_t total_textures = TextureCache::get()->get_texture_count();
            ImGui::Text("Total Textures: %u", total_textures);
        }

        if (ImGui::CollapsingHeader("Directional Light")) {
            Light *light = scene->get_sun();
            ImGui::Checkbox("Enable Shadow", &light->cast_shadow);
            ImGui::DragFloat3("Direction", &light->rotation[0], 1.0f, -360.0f, 360.0f);
            ImGui::DragFloat("Intensity", &light->intensity, 0.2f, 0.0f, 200.0f);
            ImGui::ColorPicker3("Color", &light->color[0]);
        }

        if (ImGui::CollapsingHeader("Camera")) {
            Camera *camera = scene->get_camera();
            static float fov = camera->get_fov();
            if (ImGui::DragFloat("FOV", &fov, 1.0f, 0.0f, 90.0f))
                camera->set_fov(fov);

            static float near_plane = camera->get_near_plane();
            if (ImGui::DragFloat("Near Plane", &near_plane, 0.1f, 0.01f, 10.0f))
                camera->set_near_plane(near_plane);

            static float far_plane = camera->get_far_plane();
            if (ImGui::DragFloat("Far Plane", &far_plane, 1.0f, 50.0f, 5000.0f))
                camera->set_far_plane(far_plane);

            int projection_mode = cast_int(camera->get_projection_mode());
            const char *projection_options = "PERSPECTIVE\0ORTHOGRAPHIC";
            if (ImGui::Combo("Projection Mode", &projection_mode, projection_options))
                camera->set_projection_mode(ProjectionMode(projection_mode));
        }

        if (ImGui::CollapsingHeader("Camera Controller")) {
            ImGui::DragFloat3("Target Position", &controller->target_position[0]);
            ImGui::DragFloat3("Target Rotation", &controller->target_rotation[0], 1.0f, -360.0f, 360.0f);
            ImGui::DragFloat("Walk Speed", &controller->walk_speed, 1.0f, 0.0f, 100.0f);
            ImGui::DragFloat("Run Speed", &controller->run_speed, 1.0f, 0.0f, 100.0f);
            ImGui::DragFloat("Sensitivity", &controller->sensitivity, 1.0f, 0.0f, 100.0f);
            ImGui::Checkbox("Enable Damping", &controller->enable_smoothing);
            ImGui::DragFloat("Damping(T)", &controller->smoothing_factor, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Damping(R)", &controller->rotation_smoothing_factor, 0.001f, 0.0f, 1.0f);
        }
    }

    bool show_render_pass_debug_popup = false;
    bool add_rendertarget_texture_debug_ui(const char *id, FrameGraphResource *resource, const ImVec4 &tint_color = {1.0f, 1.0f, 1.0f, 1.0f}) {
        ImGuiService::AddImage(resource->handle.id, ImVec2{128, 64}, tint_color);
        ImGui::SameLine();
        std::string formatted_id = std::string("Maximize##") + id;
        ImGui::Text(id);
        if (ImGui::Button(formatted_id.c_str())) {
            selected_renderpass_debug_resource = resource;
            ImGui::OpenPopup("render_pass_debug_popup");
            show_render_pass_debug_popup = true;
            return true;
        }
        return false;
    }

    void add_pass_ui() {
        ASSERT(frame_graph != nullptr);
        if (ImGui::CollapsingHeader("Passes")) {
            DeferredLightingPass *deferred_pass = (DeferredLightingPass *)frame_graph->get_renderer("deferred_lighting_pass");
            if (deferred_pass != nullptr && ImGui::TreeNodeEx("Deferred Pass")) {
                ImGui::SliderFloat("Split Percentage", &deferred_pass->split_percentage, 0.0f, 1.0f);
                static const char *options = "Albedo\0Normal\0Metallic\0Roughness\0AO\0Shadow\0\0";
                ImGui::Combo("Target", &deferred_pass->debug_texture, options);

                FrameGraphResource *color_texture = frame_graph->get_resource("gbuffer_color");
                add_rendertarget_texture_debug_ui("gbuffer-color", color_texture);

                FrameGraphResource *normal_texture = frame_graph->get_resource("gbuffer_normal");
                add_rendertarget_texture_debug_ui("normal_metallic_roughness", normal_texture);

                FrameGraphResource *emissive_texture = frame_graph->get_resource("gbuffer_emissive");
                add_rendertarget_texture_debug_ui("gbuffer-emissive", emissive_texture);

                FrameGraphResource *velocity_texture = frame_graph->get_resource("gbuffer_velocity");

                add_rendertarget_texture_debug_ui("velocity texture", velocity_texture);

                FrameGraphResource *taa_output = frame_graph->get_resource("taa_output");
                add_rendertarget_texture_debug_ui("taa_output", taa_output);
                ImGui::TreePop();
            }

            ForwardPass *forward_pass = (ForwardPass *)frame_graph->get_renderer("forward_pass");
            if (forward_pass && ImGui::TreeNodeEx("Forward Pass")) {
                ImGui::SliderFloat("Split Percentage", &forward_pass->split_percentage, 0.0f, 1.0f);
                static const char *options = "Albedo\0Normal\0Metallic\0Roughness\0AO\0Shadow\0\0";
                ImGui::Combo("Target", &forward_pass->debug_texture, options);
                ImGui::TreePop();
            }

            bool supports_raytracing = RenderingDevice::get()->supports_raytracing();
            if (ImGui::TreeNodeEx("Shadow Pass")) {
                if (supports_raytracing) {
                    ImGui::Checkbox("Ray Traced Shadow", &AppSettings::enable_rt_shadow);
                    if (AppSettings::enable_rt_shadow) {
                        FrameGraphResource *resource = frame_graph->get_resource("rt_directional_shadow_map");
                        add_rendertarget_texture_debug_ui("rt_shadow_pass", resource);
                    }
                }
                if (!AppSettings::enable_rt_shadow) {
                    auto *cascaded_shadow_pass = (CascadedShadowPass *)frame_graph->get_renderer("directional_shadow_pass");
                    ImGui::Text("Shadow Map Size: %d", cascaded_shadow_pass->shadow_map_size);
                    ImGui::Checkbox("Split Distance Automatic", &cascaded_shadow_pass->calculate_distance_automatic);
                    if (cascaded_shadow_pass->calculate_distance_automatic) {
                        ImGui::DragFloat("Shadow Distance", &cascaded_shadow_pass->shadow_distance, 1.0f, 0.0f, scene->get_camera()->get_far_plane());
                        ImGui::DragFloat("Split Lambda", &cascaded_shadow_pass->split_lamda, 0.001f, 0.0f, 1.0f);
                    } else {
                        for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
                            std::string cascadeName = "Cascade" + std::to_string(i);
                            ImGui::DragFloat(cascadeName.c_str(), &cascaded_shadow_pass->split_distances_constants[i], 1.0f, 0.0f);
                        }
                    }
                    FrameGraphResource *resource = frame_graph->get_resource("cascaded_shadow_map");
                    add_rendertarget_texture_debug_ui("csm_shadow", resource);
                }
                ImGui::TreePop();
            }
            SSAOPass *ssao_pass = (SSAOPass *)frame_graph->get_renderer("ssao_pass");
            if (ssao_pass != nullptr) {
                if (ImGui::TreeNodeEx("SSAO Pass")) {
                    ImGui::Text("SSAO Generation");
                    ImGui::DragFloat("Num Step", &ssao_pass->constant_data.num_step, 1.0f, 4.0f, 32.0f);
                    ImGui::DragFloat("Num Direction Step", &ssao_pass->constant_data.direction_step, 1.0f, 2.0f, 16.0f);
                    ImGui::DragFloat("March Step Size", &ssao_pass->constant_data.step_size, 0.0001f, 0.0f, 0.1f);
                    ImGui::DragFloat("SSAO Intensity", &ssao_pass->constant_data.intensity, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("Tangent Bias", &ssao_pass->constant_data.tangent_bias, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("SSAO Radius", &ssao_pass->constant_data.radius, 0.01f, 0.0f, 5.0f);

                    ImGui::Separator();
                    ImGui::Text("SSAO Blur");
                    ImGui::DragFloat("Blur radius", &ssao_pass->blur_radius, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("Blur Sharpness", &ssao_pass->blur_sharpness, 0.01f, 0.0f, 100.0f);

                    FrameGraphResource *resource = frame_graph->get_resource("ssao_texture");
                    add_rendertarget_texture_debug_ui("ssao_texture", resource);
                    ImGui::TreePop();
                }
            }

            TAAResolvePass *taa = (TAAResolvePass *)frame_graph->get_renderer("taa_resolve_pass");
            if (ImGui::TreeNodeEx("TAA") && taa != nullptr) {
                bool should_reset_history_texture = false;
                should_reset_history_texture |= ImGui::Checkbox("Enable TAA", &taa->enable_taa);
                ImGui::Checkbox("Reset History Texture", &taa->should_reset_history_texture);
                should_reset_history_texture |= ImGui::Checkbox("TAA Simple", &taa->enable_taa_simple);
                should_reset_history_texture |= ImGui::Checkbox("Temporal filtering", &taa->enable_temporal_filtering);
                should_reset_history_texture |= ImGui::Checkbox("Sample motion vector", &taa->should_sample_motion_vector);
                if (taa->should_sample_motion_vector)
                    should_reset_history_texture |= ImGui::Checkbox("Enable min depth", &taa->should_enable_min_depth);
                should_reset_history_texture |= ImGui::Checkbox("Enable History Sampling", &taa->should_enable_history_sampling);
                should_reset_history_texture |= ImGui::SliderInt("Jitter period", &Renderer::get()->jitter_period, 2, 16);
                ImGui::TreePop();
            }
        }

        if (show_render_pass_debug_popup) {
            if (ImGui::BeginPopupModal("render_pass_debug_popup", &show_render_pass_debug_popup)) {
                ImGui::SetNextWindowSize(ImVec2{800, 600});
                ImVec2 available_size = ImGui::GetContentRegionAvail();
                ImGuiService::AddImage(selected_renderpass_debug_resource->handle.id, available_size);
                ImGui::EndPopup();
            }
        }
    }

    void add_debug_ui() {
        if (show_debug_ui) {
            ImGui::Begin("Debug UI", 0);
            add_profiler_ui();
            add_scene_ui();
            add_pass_ui();
            add_entity_inspector_ui(scene);
            ImGui::End();
        }
    }

    ~TestApplication() {
        ImGuiService::Shutdown();
        Log::Info("Destroying Test Application...");
    }

  private:
    bool show_debug_ui = false;
    bool fullscreen = false;
    Scene *scene;
    FrameGraph *frame_graph = nullptr;
    FrameGraphResource *selected_renderpass_debug_resource = nullptr;
    const std::vector<std::string> &model_paths;
    std::unique_ptr<FirstPersonController> controller;
};

int main(int argc, char **argv) {
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
        .render_mode = RenderMode::RENDERMODE_DEFERRED,
    };

    std::vector<std::string> model_paths;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            model_paths.push_back(argv[i]);
        }
    }
    std::unique_ptr<Engine> engine = std::make_unique<Engine>(options);
    Log::Info("Creating Test Application ...");
    engine->set_app(std::make_unique<TestApplication>(model_paths));
    engine->run();
    engine = nullptr;
}
