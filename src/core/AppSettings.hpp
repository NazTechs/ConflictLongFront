#pragma once

#include <cstdint>
#include <string>

#include "core/SimulationDebugSettings.hpp"
#include "render/RenderMode.hpp"
#include "render/ViewMode.hpp"

namespace clf::core {

struct AppSettings final {
    render::ViewMode viewMode = render::ViewMode::Spectator;
    render::RenderMode renderMode = render::RenderMode::TopDown2D;

    // 3D camera persistence (rendering only; sim remains 2D).
    float cam3d_yaw_rad = 0.7853982f;
    float cam3d_pitch_rad = 0.7853982f;
    float cam3d_distance_m = 1500.0f;
    float cam3d_target_x_m = 0.0f;
    float cam3d_target_z_m = 0.0f;
    bool cam3d_follow_selected = false;

    bool paused = false;
    double simulationSpeed = 1.0;
    std::uint32_t lastRandomSeed = 1337;
    bool randomizeOnRestart = false;

    SimulationDebugSettings debug{};
};

std::string ToString(render::ViewMode mode);
render::ViewMode ViewModeFromString(const std::string& s, render::ViewMode fallback);

} // namespace clf::core
