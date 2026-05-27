#pragma once

#include <memory>
#include <filesystem>

#include <entt/entity/entity.hpp>

#include "core/SimulationDebugSettings.hpp"

struct SDL_Window;
struct SDL_Renderer;

namespace clf::render {
class Renderer2D;
}

namespace clf::sim {
class World;
}

namespace clf::ui {
class SimulationDebugPanel;
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

    std::unique_ptr<Time> m_time;
    std::unique_ptr<Camera2D> m_camera;
    std::unique_ptr<sim::World> m_world;
    std::unique_ptr<render::Renderer2D> m_renderer2D;
    std::unique_ptr<ui::SimulationDebugPanel> m_debugUI;

    double m_simAccumulatorSeconds = 0.0;
    double m_simTimeSeconds = 0.0;
};

} // namespace clf::core
