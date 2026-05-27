#pragma once

#include <memory>
#include <filesystem>

struct SDL_Window;
struct SDL_Renderer;

namespace clf::render {
class Renderer2D;
}

namespace clf::sim {
class World;
}

namespace clf::ui {
class DebugUI;
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
    void StepSimulationFixed(double dtSeconds);
    void RenderFrame();

    std::filesystem::path ResolveDataDir() const;

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_running = false;

    bool m_rightMouseDown = false;

    std::unique_ptr<Time> m_time;
    std::unique_ptr<Camera2D> m_camera;
    std::unique_ptr<sim::World> m_world;
    std::unique_ptr<render::Renderer2D> m_renderer2D;
    std::unique_ptr<ui::DebugUI> m_debugUI;

    double m_simAccumulatorSeconds = 0.0;
    double m_simTimeSeconds = 0.0;
};

} // namespace clf::core

