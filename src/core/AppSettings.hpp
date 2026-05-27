#pragma once

#include <cstdint>
#include <string>

#include "core/SimulationDebugSettings.hpp"
#include "render/ViewMode.hpp"

namespace clf::core {

struct AppSettings final {
    render::ViewMode viewMode = render::ViewMode::Spectator;

    bool paused = false;
    double simulationSpeed = 1.0;
    std::uint32_t lastRandomSeed = 1337;
    bool randomizeOnRestart = false;

    SimulationDebugSettings debug{};
};

std::string ToString(render::ViewMode mode);
render::ViewMode ViewModeFromString(const std::string& s, render::ViewMode fallback);

} // namespace clf::core

