#pragma once

#include <memory>
#include <filesystem>

#include <entt/entity/entity.hpp>

#include "core/SimulationDebugSettings.hpp"
#include "render/ViewMode.hpp"

struct SDL_Window;
struct SDL_Renderer;

namespace clf::render {
class Renderer2D;
class CameraController;
}

namespace clf::sim {
class World;
class ScenarioManager;
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
    void StepSimulationFixed(double dtSeconds);
    void RenderFrame();

    std::filesystem::path ResolveDataDir() const;

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_running = false;

    bool m_rightMouseDown = false;
    entt::entity m_selectedEntity = entt::null;
    SimulationDebugSettings m_debugSettings{};
    render::ViewMode m_viewMode = render::ViewMode::Spectator;

    std::unique_ptr<Time> m_time;
    std::unique_ptr<Camera2D> m_camera;
    std::unique_ptr<sim::World> m_world;
    std::unique_ptr<sim::ScenarioManager> m_scenarioManager;
    std::unique_ptr<render::Renderer2D> m_renderer2D;
    std::unique_ptr<render::CameraController> m_cameraController;

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
