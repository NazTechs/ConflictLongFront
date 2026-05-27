#include "core/Application.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include <SDL3/SDL.h>

#include <spdlog/spdlog.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "core/Camera2D.hpp"
#include "core/SettingsManager.hpp"
#include "core/Time.hpp"
#include "render/CameraController.hpp"
#include "render/Renderer2D.hpp"
#include "sim/Components.hpp"
#include "sim/ScenarioManager.hpp"
#include "sim/World.hpp"
#include "sim/visibility/FogOfWarSystem.hpp"
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
    if (!InitSDL()) {
        return 1;
    }

    m_settingsManager = std::make_unique<core::SettingsManager>(ResolveSettingsDir());

    const core::AppSettings loaded = m_settingsManager->LoadOrDefaults();

    m_viewMode = loaded.viewMode;
    m_paused = loaded.paused;
    m_simSpeed = std::clamp(loaded.simulationSpeed, 0.0, 4.0);
    m_debugSettings = loaded.debug;

    m_battleState.seed = loaded.lastRandomSeed;
    m_battleState.randomizeOnRestart = loaded.randomizeOnRestart;
    m_battleState.paused = m_paused;
    m_battleState.simSpeed = static_cast<float>(m_simSpeed);

    m_viewState.viewMode = m_viewMode;

    m_imguiIniPath = m_settingsManager->ImGuiIniPath().string();

    if (!InitImGui()) {
        return 1;
    }

    m_time = std::make_unique<Time>();
    m_camera = std::make_unique<Camera2D>();
    m_world = std::make_unique<sim::World>();
    m_scenarioManager = std::make_unique<sim::ScenarioManager>(*m_world);
    m_renderer2D = std::make_unique<render::Renderer2D>(m_renderer);
    m_cameraController = std::make_unique<render::CameraController>();
    m_fogSystem = std::make_unique<sim::visibility::FogOfWarSystem>();

    m_battleControlPanel = std::make_unique<ui::BattleControlPanel>();
    m_viewControlPanel = std::make_unique<ui::ViewControlPanel>();
    m_selectedUnitPanel = std::make_unique<ui::SelectedUnitPanel>();
    m_combatLogPanel = std::make_unique<ui::CombatLogPanel>();

    const auto dataDir = ResolveDataDir();

    if (!m_world->LoadData(dataDir)) {
        spdlog::warn(
            "Failed to load JSON from '{}', using fallback sample data.",
            dataDir.string());

        m_world->LoadFallbackSample();
    }

    // Default start: also apply seed-based terrain generation via restart
    // so UI restart is consistent.
    sim::ScenarioSettings initial{};
    initial.seed = m_battleState.seed;
    initial.randomizePositions = false;

    m_scenarioManager->Restart(initial);

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
            const double factor = std::pow(1.1, static_cast<double>(y));
            m_camera->MultiplyZoom(factor);
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT) {
            SelectEntityAtScreen(event.button.x, event.button.y);
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_RIGHT) {
            m_rightMouseDown = true;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_RIGHT) {
            m_rightMouseDown = false;
        }

        if (event.type == SDL_EVENT_MOUSE_MOTION && m_rightMouseDown) {
            if (m_viewMode == render::ViewMode::Spectator ||
                m_viewMode == render::ViewMode::DebugTactical) {
                m_camera->PanPixels(
                    static_cast<double>(-event.motion.xrel),
                    static_cast<double>(-event.motion.yrel));
            }
        }

        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_ESCAPE) {
            m_running = false;
            return;
        }
    }
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

    const glm::dvec2 worldPos =
        m_camera->ScreenToWorldMeters(
            glm::vec2(xPx, yPx),
            w,
            h);

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
            sim::visibility::FogOfWarParams p{};

            p.grid_resolution_m = cfg.grid_resolution_m;
            p.update_rate_hz = cfg.update_rate_hz;

            m_fogSystem->Configure(p);

            m_fogSystem->Update(
                m_world->Registry(),
                m_world->GetTerrain(),
                ResolveViewerEntity(),
                dtSeconds);
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
        m_viewControlPanel->Render(m_viewState, m_debugSettings);

        m_viewMode = m_viewState.viewMode;

        m_cameraController->SetViewMode(m_viewState.viewMode);
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
        (m_viewMode == render::ViewMode::SelectedTank ||
         m_viewMode == render::ViewMode::BlueTank ||
         m_viewMode == render::ViewMode::RedTank ||
         m_viewMode == render::ViewMode::DebugTactical);

    renderOptions.fogMask =
        (m_fogSystem && renderOptions.fog.enabled)
            ? &m_fogSystem->Mask()
            : nullptr;

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
            static_cast<float>(clamp01(1.0 - cfg.known_brightness));
    }

    m_renderer2D->Render(
        *m_world,
        *m_camera,
        w,
        h,
        renderOptions);

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