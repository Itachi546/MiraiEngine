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
#include "Scene/ShaderManager.hpp"

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

    void start() override {
        ImGuiService::Initialize();

        uint32_t width = 1920;
        uint32_t height = 1080;

        scene = Renderer::get()->get_scene();

        std::shared_ptr<EnvironmentMap> env_map = std::make_shared<EnvironmentMap>("Assets/Envmap/daytime.hdr");

        scene->set_environment_map(env_map);

        Camera *camera = scene->get_camera();
        camera->position = glm::vec3(6.786f, 2.394f, -0.213f);
        camera->rotation = glm::vec3(18.0f, -84, 0.0f);
        camera->set_far_plane(1000.0f);
        // Create RenderPass
        frame_graph = Renderer::get()->get_frame_graph();
#if 0
        frame_graph->load_from_file("Assets/forward_pass.json");
        frame_graph->set_renderer("forward_pass", std::make_shared<ForwardPass>());
        frame_graph->set_renderer("depth_prepass", std::make_shared<DepthPrePass>());
#else
        frame_graph->load_from_file("Assets/deferred_pass.json");
        frame_graph->set_renderer("deferred_pass", std::make_shared<DeferredPass>());
        frame_graph->set_renderer("deferred_lighting_pass", std::make_shared<DeferredLightingPass>());
        frame_graph->set_renderer("deferred_transparent_pass", std::make_shared<DeferredTransparentPass>());
#endif
        frame_graph->set_renderer("ssao_pass", std::make_shared<SSAOPass>());
        frame_graph->set_renderer("cascaded_shadow_pass", std::make_shared<CascadedShadowPass>());
        frame_graph->set_renderer("swapchain_copy", std::make_shared<SwapchainCopyPass>());
        frame_graph->set_renderer("debug_pass", std::make_shared<DebugPass>());
        frame_graph->set_renderer("overlay3D", std::make_shared<Overlay3DPass>());
        frame_graph->set_renderer("imgui_pass", std::make_shared<ImGuiRenderPass>());

        if (model_paths.size() > 0) {
            for (const auto &path : model_paths)
                ImportModel_GLTF(path, scene);
        }

        controller = std::make_unique<FirstPersonController>(scene->get_camera());
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
            for (auto &batch : scene->main_render_batches) {
                total_entities += cast_u32(batch.transform_indices.size());
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
            /*
            int projection_mode = cast_int(camera->get_projection_mode());
            const char *projection_options = "PERSPECTIVE\0ORTHOGRAPHIC";
            if (ImGui::Combo("Projection Mode", &projection_mode, projection_options))
                camera->set_projection_mode(ProjectionMode(projection_mode));
            */
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

    void add_pass_ui() {
        ASSERT(frame_graph != nullptr);
        if (ImGui::CollapsingHeader("Passes")) {
            auto *cascaded_shadow_pass = (CascadedShadowPass *)frame_graph->get_renderer("cascaded_shadow_pass");
            if (cascaded_shadow_pass != nullptr) {
                if (ImGui::CollapsingHeader("Cascaded Shadow Pass")) {
                    ImGui::Text("Material: %s", cascaded_shadow_pass->shader->get_name().c_str());
                    ImGui::Text("Shadow Map Size: %d", cascaded_shadow_pass->shadow_map_size);
                    ImGui::Checkbox("Split Distance Automatic", &cascaded_shadow_pass->calculate_distance_automatic);
                    if (cascaded_shadow_pass->calculate_distance_automatic) {
                        ImGui::DragFloat("Shadow Distance", &cascaded_shadow_pass->shadow_distance, 1.0f, 0.0f, scene->get_camera()->get_far_plane());
                        ImGui::DragFloat("Split Lambda", &cascaded_shadow_pass->split_lamda, 0.01f, 0.0f, 1.0f);
                    } else {
                        for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
                            std::string cascadeName = "Cascade" + std::to_string(i);
                            ImGui::DragFloat(cascadeName.c_str(), &cascaded_shadow_pass->split_distances_constants[i]);
                        }
                    }
                }
            }

            auto *ssao_pass = (SSAOPass *)frame_graph->get_renderer("ssao_pass");
            if (ssao_pass != nullptr) {
                if (ImGui::CollapsingHeader("SSAO Pass")) {
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
                }
            }
        }
    }

    void add_debug_ui() {
        if (show_debug_ui) {
            ImGui::Begin("Debug UI", 0);
            add_profiler_ui();
            add_scene_ui();
            add_pass_ui();
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
    FrameGraph *frame_graph;
    const std::vector<std::string> &model_paths;
    std::unique_ptr<FirstPersonController> controller;
};

int main(int argc, char **argv) {
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
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
