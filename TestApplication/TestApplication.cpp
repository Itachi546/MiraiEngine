#include "Engine/App.hpp"
#include "Engine/Engine.hpp"
#include "Common/Profiler.hpp"
#include "Device/Window.hpp"
#include "Scene/Scene.hpp"
#include "Scene/TextureCache.hpp"
#include "Graphics/Renderer.hpp"
#include "Utils/FirstPersonController.hpp"
#include "Scene/GLTFLoader.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Scene/AsyncLoader.hpp"
#include "Math/MathUtils.hpp"
#include "Inspector.hpp"
#include "FrameGraphForwardPass.hpp"
#include "Scene/ShadowSystem.hpp"
#include "Common/Random.hpp"
#include "Graphics/LineRenderer.hpp"

#include <fstream>
#include <filesystem>

using namespace mirai;
const Color Color_Black = {0.0f, 0.0f, 0.0f, 1.0f};

class TestApplication : public App {
  public:
    TestApplication(const std::vector<std::string> &model_paths) : App("TestApplication"), model_paths(model_paths) {
        Window::get()->set_title("TestApplication");
        Window::get()->set_fullscreen(fullscreen);
        scene = nullptr;
    }

    void start() override {
        ImGuiService::Initialize();

        uint32_t width = 1920;
        uint32_t height = 1080;

        Renderer *renderer = Renderer::get();

        scene = renderer->get_scene();

        std::shared_ptr<EnvironmentMap> env_map = std::make_shared<EnvironmentMap>("Assets/Envmap/daytime.hdr");
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

#if 1
        auto& component_manager = scene->ecs->component_manager;
        const uint32_t light_count = 256;
        Entity root_light = scene->create_entity("Lights");
        for (uint32_t i = 0; i < light_count; ++i) {
            bool is_spot_light = randomFloat01() < 0.1f;
            Entity entity = scene->create_entity((is_spot_light ? "SpotLight" : "PointLight") + std::to_string(i), root_light);
            TransformComponent *transform = component_manager->get_component<TransformComponent>(entity);
            transform->position = glm::vec3(
                -10.0f + randomFloat01() * 20.0f,
                randomFloat01() * 10.0f,
                -5.0f + randomFloat01() * 10.0f);
            transform->rotate(glm::vec3(-glm::pi<float>() * 0.5f, 0.0f, 0.0f));

            if (is_spot_light) {
                component_manager->add_component<LightComponent>(entity, LightComponent{
                                                                             .light_type = LIGHT_TYPE_SPOT,
                                                                             .color = glm::vec3(randomFloat01(), randomFloat01(), randomFloat01()),
                                                                             .intensity = randomFloat01() * 10.0f,
                                                                             .height = randomFloat01() * 2.0f + 2.0f,
                                                                             .cast_shadow = false,
                                                                             .inner_cone_angle = 0.1f,
                                                                             .outer_cone_angle = 0.4f,
                                                                         });
            } else {
                component_manager->add_component<LightComponent>(entity, LightComponent{
                                                                             .light_type = LIGHT_TYPE_POINT,
                                                                             .color = glm::vec3(randomFloat01(), randomFloat01(), randomFloat01()),
                                                                             .intensity = randomFloat01() * 10.0f,
                                                                             .radius = randomFloat01() * 2.0f + 2.0f,
                                                                             .cast_shadow = false,
                                                                         });
            }
        }
#endif
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

        if (ImGui::CollapsingHeader("Profiler")) {
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

    void show_render_target_textures(const std::vector<std::pair<std::string, uint32_t>> &debug_textures, bool *popup_state) {
        std::stringstream ss;
        for (auto &[name, texture] : debug_textures) {
            ss << name << '\0';
        }

        ss << '\0';
        if (ImGui::BeginPopupModal("RenderPassDebugPopup", popup_state)) {
            static int selected = 0;
            ImGui::Combo("Debug RenderPass", &selected, ss.str().c_str());

            const auto &[name, texture] = debug_textures[selected];
            ImVec2 available_size = ImGui::GetContentRegionAvail();
            ImGuiService::AddImage(texture, available_size);
            ImGui::EndPopup();
        }
    }

    void add_scene_ui() {
        if (ImGui::CollapsingHeader("Scene")) {
            uint64_t memory_usage = RenderingDevice::get()->get_memory_usage();
            ImGui::Text("GPU Memory Usage: %.2f MB", utils::bytes_to_mb(memory_usage));

            ImGui::Checkbox("Vsync", &AppSettings::enable_vsync);
            ImGui::Checkbox("Pause Animation", &scene->pause_animation);
            ImGui::DragFloat("Animation Speed", &scene->animation_speed, 0.1f, 0.1f, 10.0f);
            ImGui::Checkbox("Freeze frustum", &Renderer::get()->freeze_frustum);
            ImGui::Checkbox("Show AABB", &Renderer::get()->show_aabbs);
            ImGui::Checkbox("Disable Punctual Lights", &Renderer::get()->disable_punctual_lights);
            if (ImGui::DragFloat("Global Scene Scale", &global_scene_scale, 0.01f)) {
                TransformComponent *transform = scene->ecs->component_manager->get_component<TransformComponent>(scene->entities[0]);
                transform->scale = glm::vec3(global_scene_scale);
                transform->dirty = true;
            }

            ImGui::Text("Total Visible Entities: %u", Renderer::get()->total_visible_entities);
            ImGui::Text("Total Visible Lights: %u", Renderer::get()->total_lights);

            uint32_t total_materials = cast_u32(scene->materials.size());
            ImGui::Text("Total Materials: %u", total_materials);

            uint32_t total_mesh_buffer = cast_u32(scene->mesh_allocations.size());
            ImGui::Text("Total Mesh Allocation: %u", total_mesh_buffer);

            uint32_t total_textures = TextureCache::get()->get_texture_count();
            ImGui::Text("Total Textures: %u", total_textures);

            if (ImGui::Button("Add Plane")) {
                scene->create_plane("Plane");
            }
            if (ImGui::Button("Add Cube")) {
                scene->create_cube("Cube");
            }
            if (ImGui::Button("Add Sphere")) {
                scene->create_sphere("Sphere");
            }
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
        std::vector<std::pair<std::string, uint32_t>> render_pass_textures;
        render_pass_textures.reserve(32);
        static bool show_render_pass_debug_popup = false;

        if (ImGui::CollapsingHeader("Render Debug Options") && board->has<RenderDebugData>()) {
            RenderDebugData &debug_data = board->get<RenderDebugData>();
            ImGui::SliderFloat("Split Percentage", &debug_data.split_percentage, 0.0f, 1.0f);
            static const char *options = "Albedo\0Normal\0Metallic\0Roughness\0AO\0Shadow\0CSMSplit\0LightTile\0Velocity\0\0";
            ImGui::Combo("Target", &debug_data.debug_param_index, options);
            ImGui::Checkbox("Gamma Correction", &debug_data.enable_gamma_correction);
            ImGui::DragFloat("Exposure", &debug_data.exposure, 0.1f, 0.0f, 8.0f);
            ImGui::SliderFloat("IBL Contribution", &AppSettings::ibl_contribution, 0.0f, 4.0f);
            ImGui::Checkbox("Ground Truth", &debug_data.show_rt_ground_truth);
            if (debug_data.show_rt_ground_truth) {
                ImGui::SameLine();
                ImGui::Checkbox("Reset", &debug_data.reset_rt_texture);
            }
            ImGui::Spacing();
            if (ImGui::Button("Show RenderPass Textures")) {
                ImGui::OpenPopup("RenderPassDebugPopup");
                show_render_pass_debug_popup = true;
            }
        }

        if (show_render_pass_debug_popup) {
            FrameGraph *frame_graph = Renderer::get()->get_frame_graph();
            for (auto &resource : frame_graph->resources) {
                if (resource.resource_type == ResourceType::Buffer || resource.ref_count == 0)
                    continue;

                const FrameGraphTexture &texture = resource.get<FrameGraphTexture>();
                if (texture.id.id == K_SWAPCHAIN_TEXTURE_HANDLE.id)
                    continue;

                render_pass_textures.push_back(std::make_pair(resource.name, texture.id.id));
            }
            show_render_target_textures(render_pass_textures, &show_render_pass_debug_popup);
        }
    }
    /*
    bool show_render_pass_debug_popup = false;
    bool add_rendertarget_texture_debug_ui(const char *id, FrameGraphResourceHandle resource, const ImVec4 &tint_color = {1.0f, 1.0f, 1.0f, 1.0f}) {
        FrameGraphTexture texture = frame_graph->get<FrameGraphTexture>(resource);
        ImGuiService::AddImage(texture.id.id, ImVec2{128, 64}, tint_color);
        ImGui::SameLine();
        std::string formatted_id = std::string("Maximize##") + id;
        ImGui::Text(id);
        if (ImGui::Button(formatted_id.c_str())) {
            selected_renderpass_debug_resource = texture;
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
                    ImGui::TreePop();
                }
            }

            RenderDebugData &debug_data = board->get<RenderDebugData>();
            if (ImGui::TreeNodeEx("Bloom") && board->has<BloomPassData>()) {
                ImGui::SliderFloat("Bloom Strength", &debug_data.bloom_strength, 0.0f, 0.2f);
                ImGui::SliderFloat("Bloom Radius", &debug_data.bloom_radius, 0.1f, 40.0f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("TAA") && board->has<TAAOptions>()) {
                TAAOptions &taa_options = board->get<TAAOptions>();
                taa_options.should_reset |= ImGui::Checkbox("Enable TAA", &AppSettings::enable_taa);
                ImGui::Checkbox("Reset History Texture", &taa_options.should_reset);
                taa_options.should_reset |= ImGui::Checkbox("Sample motion vector", &taa_options.should_sample_motion_vector);
                ImGui::DragFloat("Mip LOD Bias", &debug_data.mip_lod_bias, 0.1f, -5.0f, 5.0f);
                ImGui::TreePop();
            }
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
            add_skeleton_debug_ui(scene);
        }
    }

    ~TestApplication() {
        ImGuiService::Shutdown();
        Log::Info("Destroying Test Application...");
    }

  private:
    bool show_debug_ui = false;
    bool fullscreen = false;

    float global_scene_scale = 1.0f;
    Scene *scene;
    const std::vector<std::string> &model_paths;
    std::unique_ptr<FirstPersonController> controller;
    Entity point_light;
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
