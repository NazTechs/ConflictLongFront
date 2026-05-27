#pragma once

#include <memory>
#include <filesystem>
#include <string>

#include <entt/entity/entity.hpp>

#include "core/SimulationDebugSettings.hpp"
#include "render/ViewMode.hpp"
#include "ui/BattleControlPanel.hpp"
#include "ui/ViewControlPanel.hpp"

struct SDL_Window;
struct SDL_Renderer;

namespace clf::render {
class Renderer2D;
class CameraController;
}

namespace clf::sim {
class World;
class ScenarioManager;
namespace visibility {
class FogOfWarSystem;
class TeamFogOfWarSystem;
}
}

namespace clf::ui {
class BattleControlPanel;
class ViewControlPanel;
class SelectedUnitPanel;
class CombatLogPanel;
}

namespace clf::core {

class Camera2D;
class Time;
class SettingsManager;
struct AppSettings;

class Application final {
public:
    Application();
    ~Application();

    int Run();

private:
    bool InitSDL();
    bool InitImGui();
    void ShutdownImGui();
    void ShutdownSDL();

    void ProcessEvents();
    void UpdateCamera(double frameDtSeconds);
    void SelectEntityAtScreen(float xPx, float yPx);
    void IssueWaypointAtScreen(float xPx, float yPx, bool append);
    void StepSimulationFixed(double dtSeconds);
    void RenderFrame();
    entt::entity ResolveViewerEntity() const;

    std::filesystem::path ResolveDataDir() const;
    std::filesystem::path ResolveSettingsDir() const;

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_running = false;

    bool m_rightMouseDown = false;
    bool m_rightMouseMoved = false;
    float m_rightDownX = 0.0f;
    float m_rightDownY = 0.0f;
    entt::entity m_selectedEntity = entt::null;
    SimulationDebugSettings m_debugSettings{};
    render::ViewMode m_viewMode = render::ViewMode::Spectator;
    ui::BattleControlState m_battleState{};
    ui::ViewControlState m_viewState{};
    std::string m_imguiIniPath;
    std::unique_ptr<SettingsManager> m_settingsManager;

    std::unique_ptr<Time> m_time;
    std::unique_ptr<Camera2D> m_camera;
    std::unique_ptr<sim::World> m_world;
    std::unique_ptr<sim::ScenarioManager> m_scenarioManager;
    std::unique_ptr<render::Renderer2D> m_renderer2D;
    std::unique_ptr<render::CameraController> m_cameraController;
    std::unique_ptr<sim::visibility::FogOfWarSystem> m_fogSystem;
    std::unique_ptr<sim::visibility::TeamFogOfWarSystem> m_teamFogSystem;

    std::unique_ptr<ui::BattleControlPanel> m_battleControlPanel;
    std::unique_ptr<ui::ViewControlPanel> m_viewControlPanel;
    std::unique_ptr<ui::SelectedUnitPanel> m_selectedUnitPanel;
    std::unique_ptr<ui::CombatLogPanel> m_combatLogPanel;

    bool m_paused = false;
    double m_simSpeed = 1.0;

    double m_simAccumulatorSeconds = 0.0;
    double m_simTimeSeconds = 0.0;
};

} // namespace clf::core
