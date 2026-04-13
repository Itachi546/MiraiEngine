#include "Engine/App.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"
#include "Device/Window.hpp"
#include "Scene/Scene.hpp"
#include "Scene/TextureCache.hpp"
#include "Graphics/Renderer.hpp"
#include "Utils/FirstPersonController.hpp"
#include "Scene/GLTFLoader.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Math/MathUtils.hpp"
#include "Inspector.hpp"
#include "FrameGraphForwardPass.hpp"
#include "Scene/ShadowSystem.hpp"

// #include "ImGuiService.hpp"

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

    void start() override {
        ImGuiService::Initialize();

        uint32_t width = 1920;
        uint32_t height = 1080;

        Renderer *renderer = Renderer::get();

        scene = renderer->get_scene();

        std::shared_ptr<EnvironmentMap> env_map = std::make_shared<EnvironmentMap>("Assets/Envmap/the_sky_is_on_fire_2k.hdr");
        // std::shared_ptr<EnvironmentMap> env_map = std::make_shared<EnvironmentMap>();
        scene->set_environment_map(env_map);

        Camera *camera = scene->get_camera();
        camera->position = glm::vec3(-6.24f, 1.49f, -0.46f);
        camera->rotation = glm::vec3(-14.21f, 1.65f, 0.0f);
        camera->set_far_plane(1000.0f);

        // Create RenderPass
        FrameGraph *frame_graph = Renderer::get()->get_frame_graph();
        FrameGraphBlackBoard *board = Renderer::get()->get_frame_graph_blackboard();

        initialize_forward_pass(frame_graph, board);

        if (global_scene_scale != 1.0f) {
            TransformComponent *transform = scene->ecs->component_manager->get_component<TransformComponent>(scene->entities[0]);
            transform->scale = glm::vec3(global_scene_scale);
            transform->dirty = true;
        }

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
                for (auto &[name, time, _index] : cpu) {
                    ImGui::Text("%s: %.2fms", name.c_str(), time);
                }
            }

            if (gpu.size() > 0) {
                ImGui::Text("GPU Time");
                ImGui::Separator();
                for (auto &[name, time, _index] : gpu) {
                    ImGui::Text("%s: %.2fms", name.c_str(), time);
                }
            }
        }
    }

    void add_scene_ui() {
        if (ImGui::CollapsingHeader("Scene")) {
            /*
            auto *final_pass = (SwapchainCopyPass *)frame_graph->get_renderer("swapchain_copy");
            if (final_pass) {
                ImGui::Checkbox("FXAA", &final_pass->enable_aa);
                ImGui::Checkbox("Gamma Correction", &final_pass->enable_gamma_correction);
            }
            */
            uint64_t memory_usage = RenderingDevice::get()->get_memory_usage();
            ImGui::Text("GPU Memory Usage: %.2f MB", utils::bytes_to_mb(memory_usage));

            ImGui::Checkbox("Vsync", &AppSettings::enable_vsync);
            ImGui::Checkbox("Pause Animation", &scene->pause_animation);
            ImGui::Checkbox("Freeze frustum", &Renderer::get()->freeze_frustum);
            ImGui::Checkbox("Show AABB", &Renderer::get()->show_aabbs);
            if (ImGui::DragFloat("Global Scene Scale", &global_scene_scale, 0.01f)) {
                TransformComponent *transform = scene->ecs->component_manager->get_component<TransformComponent>(scene->entities[0]);
                transform->scale = glm::vec3(global_scene_scale);
                transform->dirty = true;
            }

            ImGui::Text("Total Visible Entities: %u", Renderer::get()->total_visible_entities);

            uint32_t total_materials = cast_u32(scene->materials.size());
            ImGui::Text("Total Materials: %u", total_materials);

            uint32_t total_mesh_buffer = cast_u32(scene->gpu_meshes.size());
            ImGui::Text("Total Mesh Buffer: %u", total_mesh_buffer);

            uint32_t total_textures = TextureCache::get()->get_texture_count();
            ImGui::Text("Total Textures: %u", total_textures);
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

        FrameGraphBlackBoard *board = Renderer::get()->get_frame_graph_blackboard();
        if (ImGui::CollapsingHeader("Render Debug Options") && board->has<RenderDebugData>()) {
            RenderDebugData &debug_data = board->get<RenderDebugData>();
            ImGui::SliderFloat("Split Percentage", &debug_data.split_percentage, 0.0f, 1.0f);
            static const char *options = "Albedo\0Normal\0Metallic\0Roughness\0AO\0Shadow\0CSMSplit\0\0";
            ImGui::Combo("Target", &debug_data.debug_param_index, options);
            ImGui::Checkbox("Gamma Correction", &debug_data.enable_gamma_correction);
        }
    }
    /*
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

    void show_popup() {
        if (show_render_pass_debug_popup) {
            if (ImGui::BeginPopupModal("render_pass_debug_popup", &show_render_pass_debug_popup)) {
                ImGui::SetNextWindowSize(ImVec2{800, 600});
                ImVec2 available_size = ImGui::GetContentRegionAvail();
                ImGuiService::AddImage(selected_renderpass_debug_resource->handle.id, available_size);
                ImGui::EndPopup();
            }
        }
    }
    */
    void add_pass_ui() {
        if (ImGui::CollapsingHeader("Passes")) {
            FrameGraphBlackBoard *board = Renderer::get()->get_frame_graph_blackboard();
            /*
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

                    show_popup();
                    ImGui::TreePop();
                }

                ForwardPass *forward_pass = (ForwardPass *)frame_graph->get_renderer("forward_pass");
                if (forward_pass && ImGui::TreeNodeEx("Forward Pass")) {
                    ImGui::SliderFloat("Split Percentage", &forward_pass->split_percentage, 0.0f, 1.0f);
                    static const char *options = "Albedo\0Normal\0Metallic\0Roughness\0AO\0Shadow\0\0";
                    ImGui::Combo("Target", &forward_pass->debug_texture, options);
                    ImGui::TreePop();
                }
            */
            bool supports_raytracing = RenderingDevice::get()->supports_raytracing();
            if (ImGui::TreeNodeEx("Shadow Pass")) {
                ShadowSystem *shadow_system = ShadowSystem::get();
                if (supports_raytracing) {
                    /*
                    ImGui::Checkbox("Ray Traced Shadow", &AppSettings::enable_rt_shadow);
                    if (AppSettings::enable_rt_shadow) {
                        FrameGraphResource *resource = frame_graph->get_resource("rt_directional_shadow_map");
                        add_rendertarget_texture_debug_ui("rt_shadow_pass", resource);
                        show_popup();
                    }
                    */
                }
                if (!AppSettings::enable_rt_shadow) {
                    DirectionLightShadowParams &shadow_params = shadow_system->dir_light_params;
                    ImGui::Text("Shadow Atlas Size: %d", shadow_params.atlas_size);
                    ImGui::Text("Shadow Map Size: %d", shadow_params.split_size);

                    ImGui::Checkbox("Split Distance Automatic", &shadow_params.calculate_distance_automatic);
                    if (shadow_params.calculate_distance_automatic) {
                        ImGui::DragFloat("Shadow Distance", &shadow_params.shadow_distance, 1.0f, 0.0f, scene->get_camera()->get_far_plane());
                        ImGui::DragFloat("Split Lambda", &shadow_params.split_lambda, 0.001f, 0.0f, 1.0f);
                    } else {
                        for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
                            std::string cascadeName = "Cascade" + std::to_string(i);
                            ImGui::DragFloat(cascadeName.c_str(), &shadow_params.split_distances[i], 1.0f, 0.0f);
                        }
                    }

                    ImGui::DragFloat("PCF Radius", &shadow_params.pcf_radius, 0.1f, 0.0f, 20.0f);
                    ImGui::DragFloat("PCF Sample Count", &shadow_params.pcf_sample_count, 1.0f, 0.0f, 64.0f);
                    // FrameGraphResource *resource = frame_graph->get_resource("cascaded_shadow_map");
                    //  add_rendertarget_texture_debug_ui("csm_shadow", resource);
                    // show_popup();
                }
                ImGui::TreePop();
            }

            if (board->has<SSAOPassData>()) {
                if (ImGui::TreeNodeEx("SSAO Pass")) {
                    HBAOParams &ssao_pass = board->get<HBAOParams>();

                    ImGui::Text("SSAO Generation");
                    ImGui::DragInt("Num Step", &ssao_pass.num_step, 1.0f, 4, 32);
                    ImGui::DragInt("Num Direction Step", &ssao_pass.num_directional_step, 1.0f, 2, 16);
                    ImGui::DragFloat("SSAO Intensity", &ssao_pass.intensity, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("Tangent Bias", &ssao_pass.tangent_bias, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("SSAO Radius", &ssao_pass.radius, 0.01f, 0.0f, 5.0f);

                    ImGui::Separator();
                    ImGui::Text("SSAO Blur");
                    ImGui::DragFloat("Blur radius", &ssao_pass.blur_radius, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat("Blur Sharpness", &ssao_pass.blur_sharpness, 0.01f, 0.0f, 100.0f);
                    /*
                    FrameGraphResource *resource = frame_graph->get_resource("ssao_texture");
                    add_rendertarget_texture_debug_ui("ssao_texture", resource);

                    show_popup();
                    */
                    ImGui::TreePop();
                }
            }
            /*
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

        */
        }
    }
    void add_debug_ui() {
        if (show_debug_ui) {
            ImGui::Begin("Debug UI", 0);
            add_scene_ui();
            add_profiler_ui();
            add_pass_ui();
            add_entity_inspector_ui(scene);
            ImGui::End();
            // add_skeleton_debug_ui(scene);
        }
    }

    ~TestApplication() {
        ImGuiService::Shutdown();
        Log::Info("Destroying Test Application...");
    }

  private:
    bool show_debug_ui = true;
    bool fullscreen = false;

    float global_scene_scale = 1.0f;
    Scene *scene;
    FrameGraph *frame_graph = nullptr;
    const std::vector<std::string> &model_paths;
    std::unique_ptr<FirstPersonController> controller;
};

int main(int argc, char **argv) {
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
        .render_mode = RenderMode::RENDERMODE_FORWARD,
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
