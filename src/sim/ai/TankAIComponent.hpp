#pragma once

#include <cstdint>
#include <glm/vec2.hpp>

namespace clf::sim::ai {

enum class TankAIState : std::uint8_t {
    Idle = 0,
    Search,
    MoveToSearchPosition,
    Scan,
    TargetDetected,
    AimAtTarget,
    Fire,
    Reload,
    Reposition,
    Disabled,
    Destroyed,
};

struct TankAI final {
    TankAIState state = TankAIState::Search;

    double state_time_s = 0.0;

    glm::dvec2 search_waypoint_m{0.0, 0.0};
    bool has_waypoint = false;

    // Simple scanning.
    double scan_phase = 0.0;
};

} // namespace clf::sim::ai

