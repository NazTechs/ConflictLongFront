#include "core/Application.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>
#include <string>

#include <SDL3/SDL.h>

#include <spdlog/spdlog.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "core/Camera2D.hpp"
#include "core/Camera3D.hpp"
#include "core/SettingsManager.hpp"
#include "core/ScopedTimer.hpp"
#include "core/Time.hpp"
#include "render/CameraController.hpp"
#include "render/Renderer2D.hpp"
#include "render/Renderer3D.hpp"
#include "sim/Components.hpp"
#include "sim/ScenarioManager.hpp"
#include "sim/World.hpp"
#include "sim/visibility/FogOfWarSystem.hpp"
#include "sim/visibility/TeamFogOfWarSystem.hpp"
#include "ui/BattleControlPanel.hpp"
#include "ui/CombatLogPanel.hpp"
#include "ui/SelectedUnitPanel.hpp"
#include "ui/ViewControlPanel.hpp"

namespace clf::core {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr double kFixedSimDtSeconds = 1.0 / 60.0;
constexpr int kMaxSimStepsPerFrame = 4;

} // namespace

Application::Application()
{
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
}

Application::~Application()
{
    ShutdownImGui();
    ShutdownSDL();
}

int Application::Run()
{
    const auto startupStart = std::chrono::steady_clock::now();

    if (!InitSDL()) {
        return 1;
    }

    core::AppSettings loaded{};
    {
        core::ScopedTimer timer("[Startup] Load settings");
        m_settingsManager =
            std::make_unique<core::SettingsManager>(ResolveSettingsDir());
        loaded = m_settingsManager->LoadOrDefaults();
    }

    m_viewMode = loaded.viewMode;
    m_renderMode = loaded.renderMode;
    m_paused = loaded.paused;
    m_simSpeed = std::clamp(loaded.simulationSpeed, 0.0, 4.0);
    m_debugSettings = loaded.debug;

    m_battleState.seed = loaded.lastRandomSeed;
    m_battleState.randomizeOnRestart = loaded.randomizeOnRestart;
    m_battleState.paused = m_paused;
    m_battleState.simSpeed = static_cast<float>(m_simSpeed);

    m_viewState.viewMode = m_viewMode;
    m_viewState.renderMode = m_renderMode;
    m_viewState.followSelected = loaded.cam3d_follow_selected;

    m_imguiIniPath = m_settingsManager->ImGuiIniPath().string();

    {
        core::ScopedTimer t("[Startup] Init ImGui");
        if (!InitImGui()) {
            return 1;
        }
    }

    {
        core::ScopedTimer timer("[Startup] Create world/systems");
        m_time = std::make_unique<Time>();
        m_camera = std::make_unique<Camera2D>();
        m_camera3D = std::make_unique<Camera3D>();
        m_camera3D->Reset();
        m_camera3D->SetYawPitchDistance(
            loaded.cam3d_yaw_rad,
            loaded.cam3d_pitch_rad,
            loaded.cam3d_distance_m);
        m_camera3D->SetTarget(glm::vec3(
            loaded.cam3d_target_x_m,
            0.0f,
            loaded.cam3d_target_z_m));
        m_world = std::make_unique<sim::World>();
        m_scenarioManager = std::make_unique<sim::ScenarioManager>(*m_world);
        m_renderer2D = std::make_unique<render::Renderer2D>(m_renderer);
        m_renderer3D = std::make_unique<render::Renderer3D>(m_renderer);
        m_cameraController = std::make_unique<render::CameraController>();
        m_fogSystem = std::make_unique<sim::visibility::FogOfWarSystem>();
        m_teamFogSystem = std::make_unique<sim::visibility::TeamFogOfWarSystem>();
    }

    m_battleControlPanel = std::make_unique<ui::BattleControlPanel>();
    m_viewControlPanel = std::make_unique<ui::ViewControlPanel>();
    m_selectedUnitPanel = std::make_unique<ui::SelectedUnitPanel>();
    m_combatLogPanel = std::make_unique<ui::CombatLogPanel>();

    const auto dataDir = ResolveDataDir();

    {
        core::ScopedTimer t("[Startup] Load data (JSON/config)");
        if (!m_world->LoadData(dataDir)) {
        spdlog::warn(
            "Failed to load JSON from '{}', using fallback sample data.",
            dataDir.string());

        m_world->LoadFallbackSample();
        }
    }

    // Default start: also apply seed-based terrain generation via restart
    // so UI restart is consistent.
    sim::ScenarioSettings initial{};
    initial.seed = m_battleState.seed;
    initial.randomizePositions = false;

    {
        core::ScopedTimer t("[Startup] Spawn scenario");
    m_scenarioManager->Restart(initial);

    const auto startupEnd = std::chrono::steady_clock::now();
    const auto totalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(startupEnd - startupStart).count();
    spdlog::info("[Startup] Total startup: {} ms", totalMs);
    }

    m_running = true;
    m_simAccumulatorSeconds = 0.0;
    m_simTimeSeconds = 0.0;

    while (m_running) {
        m_time->Tick();

        ProcessEvents();

        const double rawFrameDt = std::min(m_time->DeltaSeconds(), 0.25);

        UpdateCamera(rawFrameDt);

        m_simSpeed = std::clamp(m_simSpeed, 0.0, 4.0);

        const double scaledDt = m_paused ? 0.0 : rawFrameDt * m_simSpeed;

        m_simAccumulatorSeconds += scaledDt;

        int simSteps = 0;

        while (m_simAccumulatorSeconds >= kFixedSimDtSeconds &&
               simSteps < kMaxSimStepsPerFrame) {
            StepSimulationFixed(kFixedSimDtSeconds);
            m_simAccumulatorSeconds -= kFixedSimDtSeconds;
            ++simSteps;
        }

        // Prevent spiral-of-death when 4x speed or heavy simulation load
        // creates more accumulated simulation time than the CPU can process.
        if (simSteps == kMaxSimStepsPerFrame &&
            m_simAccumulatorSeconds >= kFixedSimDtSeconds) {
            m_simAccumulatorSeconds = 0.0;
        }

        RenderFrame();
    }

    return 0;
}

bool Application::InitSDL()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    SDL_WindowFlags windowFlags =
        SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_HIGH_PIXEL_DENSITY;

    m_window = SDL_CreateWindow(
        "ConflictLongFront",
        kWindowWidth,
        kWindowHeight,
        windowFlags);

    if (m_window == nullptr) {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, nullptr);

    if (m_renderer == nullptr) {
        spdlog::error("SDL_CreateRenderer failed: {}", SDL_GetError());
        return false;
    }

    SDL_SetRenderVSync(m_renderer, 1);

    return true;
}

bool Application::InitImGui()
{
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(m_window, m_renderer);
    ImGui_ImplSDLRenderer3_Init(m_renderer);

    if (!m_imguiIniPath.empty()) {
        io.IniFilename = m_imguiIniPath.c_str();

        if (std::filesystem::exists(m_imguiIniPath)) {
            ImGui::LoadIniSettingsFromDisk(m_imguiIniPath.c_str());
        }
    }

    return true;
}

void Application::ShutdownImGui()
{
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    if (m_settingsManager) {
        core::AppSettings s{};

        s.viewMode = m_viewMode;
        s.renderMode = m_renderMode;
        if (m_camera3D) {
            s.cam3d_yaw_rad = m_camera3D->YawRad();
            s.cam3d_pitch_rad = m_camera3D->PitchRad();
            s.cam3d_distance_m = m_camera3D->DistanceMeters();
            s.cam3d_target_x_m = m_camera3D->Target().x;
            s.cam3d_target_z_m = m_camera3D->Target().z;
            s.cam3d_follow_selected = m_viewState.followSelected;
        }
        s.paused = m_paused;
        s.simulationSpeed = std::clamp(m_simSpeed, 0.0, 4.0);
        s.lastRandomSeed = m_battleState.seed;
        s.randomizeOnRestart = m_battleState.randomizeOnRestart;
        s.debug = m_debugSettings;

        m_settingsManager->Save(s);
    }

    if (!m_imguiIniPath.empty()) {
        ImGui::SaveIniSettingsToDisk(m_imguiIniPath.c_str());
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();

    ImGui::DestroyContext();
}

void Application::ShutdownSDL()
{
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void Application::ProcessEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        if (event.type == SDL_EVENT_QUIT) {
            m_running = false;
            return;
        }

        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
            event.window.windowID == SDL_GetWindowID(m_window)) {
            m_running = false;
            return;
        }

        // Convert mouse coordinates to renderer pixel coordinates.
        SDL_ConvertEventToRenderCoordinates(m_renderer, &event);

        ImGuiIO& io = ImGui::GetIO();

        if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
            continue;
        }

        if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            const float y = event.wheel.y;
            if (m_renderMode == render::RenderMode::Perspective3D && m_camera3D) {
                m_camera3D->Zoom(static_cast<float>(-y) * 80.0f);
            } else {
                const double factor = std::pow(1.1, static_cast<double>(y));
                m_camera->MultiplyZoom(factor);
            }
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT) {
            SelectEntityAtScreen(event.button.x, event.button.y);
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_MIDDLE) {
            m_middleMouseDown = true;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_MIDDLE) {
            m_middleMouseDown = false;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_RIGHT) {
            m_rightMouseDown = true;
            m_rightMouseMoved = false;
            m_rightDownX = event.button.x;
            m_rightDownY = event.button.y;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_RIGHT) {
            const bool wasDrag = m_rightMouseMoved;
            m_rightMouseDown = false;
            m_rightMouseMoved = false;

            if (!wasDrag) {
                const SDL_Keymod mods = SDL_GetModState();
                const bool append = (mods & SDL_KMOD_SHIFT) != 0;
                IssueWaypointAtScreen(event.button.x, event.button.y, append);
            }
        }

        if (event.type == SDL_EVENT_MOUSE_MOTION && m_rightMouseDown) {
            if (m_renderMode == render::RenderMode::TopDown2D &&
                (m_viewMode == render::ViewMode::Spectator ||
                 m_viewMode == render::ViewMode::DebugTactical)) {
                m_camera->PanPixels(
                    static_cast<double>(-event.motion.xrel),
                    static_cast<double>(-event.motion.yrel));

                if (std::abs(event.motion.xrel) > 1.0f ||
                    std::abs(event.motion.yrel) > 1.0f) {
                    m_rightMouseMoved = true;
                }
            }
        }

        if (event.type == SDL_EVENT_MOUSE_MOTION && m_middleMouseDown) {
            if (m_renderMode == render::RenderMode::Perspective3D && m_camera3D) {
                const float dx = static_cast<float>(event.motion.xrel);
                const float dy = static_cast<float>(event.motion.yrel);
                m_camera3D->Orbit(dx * 0.006f, dy * 0.006f);
            }
        }

        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_ESCAPE) {
            m_running = false;
            return;
        }
    }
}

void Application::IssueWaypointAtScreen(float xPx, float yPx, bool append)
{
    if (!m_world || !m_camera || m_selectedEntity == entt::null) {
        return;
    }

    int w = 0;
    int h = 0;

    SDL_GetWindowSizeInPixels(m_window, &w, &h);

    if (w <= 0 || h <= 0) {
        return;
    }

    auto& registry = m_world->Registry();

    if (!registry.valid(m_selectedEntity) ||
        !registry.all_of<sim::Tank, sim::Transform>(m_selectedEntity)) {
        return;
    }

    glm::dvec2 worldPos{};

    if (m_renderMode == render::RenderMode::Perspective3D && m_camera3D) {
        glm::vec2 hit{};
        const Ray3D ray = m_camera3D->ScreenPointToRay(xPx, yPx, w, h);
        if (!m_camera3D->RaycastGroundPlaneY0(ray, hit)) {
            return;
        }
        worldPos = glm::dvec2(hit.x, hit.y);
    } else {
        worldPos =
            m_camera->ScreenToWorldMeters(
                glm::vec2(xPx, yPx),
                w,
                h);
    }

    auto& mode =
        registry.get_or_emplace<sim::ControlModeComponent>(m_selectedEntity);

    mode.mode = sim::ControlMode::Manual;

    auto& path =
        registry.get_or_emplace<sim::WaypointPathComponent>(m_selectedEntity);

    if (!append) {
        path.waypoints.clear();
    }

    path.waypoints.push_back(worldPos);
}

void Application::UpdateCamera(double frameDtSeconds)
{
    ImGuiIO& io = ImGui::GetIO();

    if (m_cameraController) {
        m_cameraController->ApplyViewMode(
            *m_camera,
            *m_world,
            m_selectedEntity);
    }

    if (io.WantCaptureKeyboard) {
        return;
    }

    if (m_renderMode == render::RenderMode::Perspective3D && m_camera3D) {
        int numKeys = 0;
        const bool* keys = SDL_GetKeyboardState(&numKeys);
        if (keys == nullptr || numKeys == 0) {
            return;
        }

        float speed = 200.0f; // m/s target pan
        if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
            speed *= 3.0f;
        }

        float dx = 0.0f;
        float dz = 0.0f;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) dx -= 1.0f;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) dx += 1.0f;
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) dz -= 1.0f;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) dz += 1.0f;

        if (dx != 0.0f || dz != 0.0f) {
            const float len = std::sqrt(dx * dx + dz * dz);
            dx /= len;
            dz /= len;
            m_camera3D->PanTargetGround(dx * speed * static_cast<float>(frameDtSeconds),
                                        dz * speed * static_cast<float>(frameDtSeconds));
        }

        if (m_viewState.followSelected &&
            m_selectedEntity != entt::null &&
            m_world &&
            m_world->Registry().valid(m_selectedEntity) &&
            m_world->Registry().all_of<sim::Transform>(m_selectedEntity)) {
            const auto& xf = m_world->Registry().get<const sim::Transform>(m_selectedEntity);
            m_camera3D->SetTarget(glm::vec3(static_cast<float>(xf.position_m.x), 0.0f, static_cast<float>(xf.position_m.y)));
        }

        return;
    }

    if (!(m_viewMode == render::ViewMode::Spectator ||
          m_viewMode == render::ViewMode::DebugTactical)) {
        return;
    }

    int numKeys = 0;
    const bool* keys = SDL_GetKeyboardState(&numKeys);

    if (keys == nullptr || numKeys == 0) {
        return;
    }

    double speed = 40.0;

    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
        speed *= 3.0;
    }

    double dx = 0.0;
    double dy = 0.0;

    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
        dx -= 1.0;
    }

    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
        dx += 1.0;
    }

    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
        dy -= 1.0;
    }

    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
        dy += 1.0;
    }

    if (dx != 0.0 || dy != 0.0) {
        const double len = std::sqrt(dx * dx + dy * dy);

        dx /= len;
        dy /= len;

        m_camera->PanMeters(
            dx * speed * frameDtSeconds,
            dy * speed * frameDtSeconds);
    }
}

void Application::SelectEntityAtScreen(float xPx, float yPx)
{
    if (!m_world || !m_camera) {
        return;
    }

    int w = 0;
    int h = 0;

    SDL_GetWindowSizeInPixels(m_window, &w, &h);

    if (w <= 0 || h <= 0) {
        return;
    }

    glm::dvec2 worldPos{};

    if (m_renderMode == render::RenderMode::Perspective3D && m_camera3D) {
        glm::vec2 hit{};
        const Ray3D ray = m_camera3D->ScreenPointToRay(xPx, yPx, w, h);
        if (!m_camera3D->RaycastGroundPlaneY0(ray, hit)) {
            return;
        }
        worldPos = glm::dvec2(hit.x, hit.y);
    } else {
        worldPos =
            m_camera->ScreenToWorldMeters(
                glm::vec2(xPx, yPx),
                w,
                h);
    }

    entt::entity best = entt::null;

    double bestDist2 = std::numeric_limits<double>::infinity();

    auto& registry = m_world->Registry();

    const auto view = registry.view<const sim::Tank, const sim::Transform>();

    const double minPickPx = 8.0;
    const double zoom = std::max(1e-6, m_camera->ZoomPixelsPerMeter());

    for (const auto e : view) {
        const auto& tank = view.get<const sim::Tank>(e);
        const auto& xf = view.get<const sim::Transform>(e);

        const glm::dvec2 d = xf.position_m - worldPos;
        const double dist2 = d.x * d.x + d.y * d.y;

        const double pickRadius_m =
            std::max(tank.radius_m, minPickPx / zoom);

        if (dist2 <= pickRadius_m * pickRadius_m &&
            dist2 < bestDist2) {
            best = e;
            bestDist2 = dist2;
        }
    }

    m_selectedEntity = best;
}

void Application::StepSimulationFixed(double dtSeconds)
{
    m_world->Step(dtSeconds);

    m_simTimeSeconds += dtSeconds;

    if (m_fogSystem && m_debugSettings.showFogOfWar) {
        const auto& cfg = m_world->GetAiSettings().fog_of_war;

        if (cfg.enabled) {
            if (m_debugSettings.fogViewMode == core::FogViewMode::SelectedTeam) {
                if (m_teamFogSystem) {
                    sim::visibility::TeamFogParams p{};
                    p.grid_resolution_m = cfg.grid_resolution_m;
                    p.update_rate_hz = cfg.update_rate_hz;
                    p.max_teams = 2;
                    m_teamFogSystem->Configure(p);
                    m_teamFogSystem->Update(m_world->Registry(), m_world->GetTerrain(), dtSeconds);
                }
            } else if (m_debugSettings.fogViewMode == core::FogViewMode::SelectedUnit) {
                sim::visibility::FogOfWarParams p{};
                p.grid_resolution_m = cfg.grid_resolution_m;
                p.update_rate_hz = cfg.update_rate_hz;

                m_fogSystem->Configure(p);
                m_fogSystem->Update(m_world->Registry(), m_world->GetTerrain(), ResolveViewerEntity(), dtSeconds);
            }
        }
    }
}

void Application::RenderFrame()
{
    int w = 0;
    int h = 0;

    SDL_GetWindowSizeInPixels(m_window, &w, &h);

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    ImGui::NewFrame();

    if (m_battleControlPanel && m_scenarioManager) {
        m_battleState.paused = m_paused;
        m_battleState.simSpeed = static_cast<float>(m_simSpeed);

        if (m_battleControlPanel->Render(m_battleState, *m_scenarioManager)) {
            m_selectedEntity = entt::null;
        }

        m_paused = m_battleState.paused;

        m_simSpeed = std::clamp(
            static_cast<double>(m_battleState.simSpeed),
            0.0,
            4.0);

        m_battleState.simSpeed = static_cast<float>(m_simSpeed);
    }

    if (m_viewControlPanel && m_cameraController) {
        m_viewControlPanel->Render(
            m_viewState,
            m_debugSettings,
            *m_camera3D,
            (m_selectedEntity != entt::null));

        m_viewMode = m_viewState.viewMode;
        m_renderMode = m_viewState.renderMode;

        m_cameraController->SetViewMode(m_viewState.viewMode);
    }

    if (m_camera3D && m_viewState.requestFocusSelected) {
        m_viewState.requestFocusSelected = false;
        if (m_world &&
            m_selectedEntity != entt::null &&
            m_world->Registry().valid(m_selectedEntity) &&
            m_world->Registry().all_of<sim::Transform>(m_selectedEntity)) {
            const auto& xf =
                m_world->Registry().get<const sim::Transform>(m_selectedEntity);
            m_camera3D->SetTarget(glm::vec3(
                static_cast<float>(xf.position_m.x),
                0.0f,
                static_cast<float>(xf.position_m.y)));
        }
    }

    if (m_selectedUnitPanel) {
        m_selectedUnitPanel->Render(m_selectedEntity, *m_world);
    }

    if (m_combatLogPanel) {
        m_combatLogPanel->Render(*m_world);
    }

    ImGui::Render();

    SDL_SetRenderDrawColorFloat(
        m_renderer,
        0.08f,
        0.08f,
        0.10f,
        1.0f);

    SDL_RenderClear(m_renderer);

    if (m_camera3D) {
        const float aspect =
            (h > 0) ? (static_cast<float>(w) / static_cast<float>(h))
                    : (16.0f / 9.0f);
        m_camera3D->SetAspect(aspect);
    }

    render::Renderer2D::Options renderOptions{};

    renderOptions.selectedEntity = m_selectedEntity;
    renderOptions.viewMode = m_viewMode;
    renderOptions.showOnlyDetectedUnits =
        m_debugSettings.showOnlyDetectedUnitsInUnitView;
    renderOptions.showAllUnitsInDebugView =
        m_debugSettings.showAllUnitsInDebugView;

    renderOptions.viewerEntity = ResolveViewerEntity();

    renderOptions.terrain.showHeightOverlay =
        m_debugSettings.showTerrainHeightOverlay;
    renderOptions.terrain.contourBands =
        m_debugSettings.terrainContourBands;

    renderOptions.overlay.showWeaponRange =
        m_debugSettings.showWeaponRange;
    renderOptions.overlay.showVisualRange =
        m_debugSettings.showVisualRange;
    renderOptions.overlay.showLosRay =
        m_debugSettings.showLosRay;
    renderOptions.overlay.showSensorCone =
        m_debugSettings.showSensorCone;
    renderOptions.overlay.showBlockedPoint =
        m_debugSettings.showBlockedLosPoint;
    renderOptions.overlay.showShotTraces =
        m_debugSettings.showShotTraces;
    renderOptions.overlay.highlightSelection =
        m_debugSettings.highlightSelection;
    renderOptions.overlay.showDamageZones =
        m_debugSettings.showDamageZones;
    renderOptions.overlay.showSearchWaypoints =
        m_debugSettings.showSearchWaypoints;
    renderOptions.overlay.showMovementVectors =
        m_debugSettings.showMovementVectors;
    renderOptions.overlay.showAiState =
        m_debugSettings.showAiState;
    renderOptions.overlay.showDetectionDebug =
        m_debugSettings.showDetectionDebug;
    renderOptions.overlay.showCollisionBounds =
        m_debugSettings.showCollisionBounds;

    renderOptions.fog.enabled =
        m_debugSettings.showFogOfWar &&
        (m_debugSettings.fogViewMode != core::FogViewMode::Spectator);

    renderOptions.fogMask = nullptr;

    if (renderOptions.fog.enabled) {
        if (m_debugSettings.fogViewMode == core::FogViewMode::SelectedTeam) {
            if (m_teamFogSystem) {
                renderOptions.fogMask =
                    &m_teamFogSystem->MaskForTeam(m_debugSettings.fogTeamId);
            }
        } else if (m_debugSettings.fogViewMode == core::FogViewMode::SelectedUnit) {
            if (m_fogSystem) {
                renderOptions.fogMask = &m_fogSystem->Mask();
            }
        }
    }

    {
        const auto& cfg = m_world->GetAiSettings().fog_of_war;

        renderOptions.fog.enabled =
            renderOptions.fog.enabled && cfg.enabled;

        const auto clamp01 = [](double v) {
            return std::clamp(v, 0.0, 1.0);
        };

        renderOptions.fog.unknown_alpha =
            static_cast<float>(clamp01(1.0 - cfg.unknown_brightness));

        renderOptions.fog.known_alpha =
            m_debugSettings.fogShowExploredMemory
                ? static_cast<float>(clamp01(1.0 - cfg.known_brightness))
                : 0.0f;
    }

    if (m_renderMode == render::RenderMode::Perspective3D && m_renderer3D && m_camera3D) {
        static double logAccum_s = 0.0;
        logAccum_s += m_time ? m_time->DeltaSeconds() : 0.0;
        if (logAccum_s > 2.0) {
            logAccum_s = 0.0;
            const auto pos = m_camera3D->Position();
            const auto tgt = m_camera3D->Target();
            spdlog::info(
                "[3D] cam pos (%.1f, %.1f, %.1f) target (%.1f, %.1f, %.1f) yaw %.2f pitch %.2f dist %.1f",
                pos.x, pos.y, pos.z,
                tgt.x, tgt.y, tgt.z,
                m_camera3D->YawRad(),
                m_camera3D->PitchRad(),
                m_camera3D->DistanceMeters());
        }

        render::Renderer3DOptions opt3d{};
        opt3d.showGrid = true;
        opt3d.showWaypoints = true;
        // Disable 3D fog while bringing up the renderer (can be re-enabled once visuals are confirmed).
        opt3d.showFog = false;
        opt3d.fogStrideCells = 4;
        opt3d.fogMask = renderOptions.fogMask;
        opt3d.fogUnknownAlpha = renderOptions.fog.unknown_alpha;
        opt3d.fogKnownAlpha = renderOptions.fog.known_alpha;

        m_renderer3D->Render(*m_world, *m_camera3D, w, h, m_selectedEntity, opt3d);
    } else {
        m_renderer2D->Render(*m_world, *m_camera, w, h, renderOptions);
    }

    ImGui_ImplSDLRenderer3_RenderDrawData(
        ImGui::GetDrawData(),
        m_renderer);

    SDL_RenderPresent(m_renderer);
}

entt::entity Application::ResolveViewerEntity() const
{
    if (!m_world) {
        return entt::null;
    }

    entt::entity viewer = entt::null;

    if (m_viewMode == render::ViewMode::SelectedTank) {
        viewer = m_selectedEntity;
    } else if (m_viewMode == render::ViewMode::BlueTank ||
               m_viewMode == render::ViewMode::RedTank) {
        const int team =
            (m_viewMode == render::ViewMode::BlueTank) ? 0 : 1;

        auto view = m_world->Registry().view<const sim::Tank>();

        for (const auto e : view) {
            if (view.get<const sim::Tank>(e).team_id == team) {
                viewer = e;
                break;
            }
        }
    }

    if (m_viewMode == render::ViewMode::DebugTactical &&
        !m_debugSettings.showAllUnitsInDebugView) {
        if (m_selectedEntity != entt::null) {
            viewer = m_selectedEntity;
        }
    }

    return viewer;
}

std::filesystem::path Application::ResolveDataDir() const
{
    const char* basePathUtf8 = SDL_GetBasePath();

    if (basePathUtf8 && basePathUtf8[0] != '\0') {
        return std::filesystem::path(basePathUtf8) / "data";
    }

    return std::filesystem::current_path() / "data";
}

std::filesystem::path Application::ResolveSettingsDir() const
{
    return ResolveDataDir() / "settings";
}

} // namespace clf::core
